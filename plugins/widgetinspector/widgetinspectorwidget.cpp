/*
  widgetinspectorwidget.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2010-2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <config-gammaray.h>

#include "widgetinspectorwidget.h"
#include "widgetinspectorinterface.h"
#include "widgetinspectorclient.h"
#include "ui_widgetinspectorwidget.h"

#include "common/objectbroker.h"
#include "common/objectmodel.h"

#include <ui/deferredresizemodesetter.h>
#include <ui/paintbufferviewer.h>
#include <ui/remoteviewwidget.h>
#include <ui/searchlinecontroller.h>

#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QLabel>
#include <QtPlugin>
#include <QToolBar>

using namespace GammaRay;

static QObject* createWidgetInspectorClient(const QString &/*name*/, QObject *parent)
{
  return new WidgetInspectorClient(parent);
}

WidgetInspectorWidget::WidgetInspectorWidget(QWidget *parent)
  : QWidget(parent)
  , ui(new Ui::WidgetInspectorWidget)
  , m_inspector(0)
  , m_remoteView(new RemoteViewWidget(this))
{
  ObjectBroker::registerClientObjectFactoryCallback<WidgetInspectorInterface*>(createWidgetInspectorClient);
  m_inspector = ObjectBroker::object<WidgetInspectorInterface*>();

  ui->setupUi(this);
  ui->widgetPropertyWidget->setObjectBaseName(m_inspector->objectName());

  auto widgetModel = ObjectBroker::model(QStringLiteral("com.kdab.GammaRay.WidgetTree"));
  ui->widgetTreeView->setModel(widgetModel);
  ui->widgetTreeView->setSelectionModel(ObjectBroker::selectionModel(widgetModel));
  new DeferredResizeModeSetter(ui->widgetTreeView->header(), 0, QHeaderView::Stretch);
  new DeferredResizeModeSetter(ui->widgetTreeView->header(), 1, QHeaderView::Interactive);
  new SearchLineController(ui->widgetSearchLine, widgetModel);
  connect(ui->widgetTreeView->selectionModel(),
          SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
          SLOT(widgetSelected(QItemSelection)));

  m_remoteView->setName(QStringLiteral("com.kdab.GammaRay.WidgetRemoteView"));

  auto layout = new QVBoxLayout;
  layout->setMargin(0);
  auto toolbar = new QToolBar(this);
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  layout->addWidget(toolbar);
  ui->widgetPreviewContainer->setLayout(layout);
  layout->addWidget(m_remoteView);

  foreach (auto action, m_remoteView->interactionModeActions()->actions())
    toolbar->addAction(action);
  toolbar->addSeparator();

  toolbar->addWidget(new QLabel(tr("Zoom:")));
  auto zoom = new QComboBox;
  zoom->setModel(m_remoteView->zoomLevelModel());
  toolbar->addWidget(zoom);
  connect(zoom, SIGNAL(currentIndexChanged(int)), m_remoteView, SLOT(setZoomLevel(int)));
  connect(m_remoteView, SIGNAL(zoomLevelChanged(int)), zoom, SLOT(setCurrentIndex(int)));

  connect(ui->actionSaveAsImage, SIGNAL(triggered()), SLOT(saveAsImage()));
  connect(ui->actionSaveAsSvg, SIGNAL(triggered()), SLOT(saveAsSvg()));
  connect(ui->actionSaveAsPdf, SIGNAL(triggered()), SLOT(saveAsPdf()));
  connect(ui->actionSaveAsUiFile, SIGNAL(triggered()), SLOT(saveAsUiFile()));
  connect(ui->actionAnalyzePainting, SIGNAL(triggered()), SLOT(analyzePainting()));
  connect(m_inspector, SIGNAL(widgetPreviewAvailable(QImage)), m_remoteView, SLOT(setImage(QImage)));

  connect(m_inspector, SIGNAL(features(bool,bool,bool,bool)),
          this, SLOT(setFeatures(bool,bool,bool,bool)));

  // NOTE: we must add actions in the ctor...
  addAction(ui->actionSaveAsImage);
  addAction(ui->actionSaveAsSvg);
  addAction(ui->actionSaveAsPdf);
  addAction(ui->actionSaveAsUiFile);
  addAction(ui->actionAnalyzePainting);
  setActionsEnabled(false);

  m_inspector->checkFeatures();
}

WidgetInspectorWidget::~WidgetInspectorWidget()
{
}

void WidgetInspectorWidget::setFeatures(bool svg, bool print, bool designer, bool privateHeaders)
{
  if (!svg) {
    delete ui->actionSaveAsSvg;
    ui->actionSaveAsSvg = 0;
  }
  if (!print) {
    delete ui->actionSaveAsPdf;
    ui->actionSaveAsPdf = 0;
  }
  if (!designer) {
    delete ui->actionSaveAsUiFile;
    ui->actionSaveAsUiFile = 0;
  }
  if (!privateHeaders) {
    delete ui->actionAnalyzePainting;
    ui->actionAnalyzePainting = 0;
  }

  setActionsEnabled(ui->widgetTreeView->selectionModel()->hasSelection());
}

void WidgetInspectorWidget::widgetSelected(const QItemSelection& selection)
{
  QModelIndex index;
  if (selection.size() > 0)
    index = selection.first().topLeft();

  if (index.isValid()) {
    setActionsEnabled(true);

    // in case selection was triggered remotely
    ui->widgetTreeView->scrollTo(index);
  } else {
    setActionsEnabled(false);
  }
}

void WidgetInspectorWidget::setActionsEnabled(bool enabled)
{
  foreach (QAction *action, actions()) {
    action->setEnabled(enabled);
  }
}

void WidgetInspectorWidget::saveAsImage()
{
  const QString fileName =
    QFileDialog::getSaveFileName(
      this,
      tr("Save As Image"),
      QString(),
      tr("Image Files (*.png *.jpg)"));

  if (fileName.isEmpty())
    return;

  m_inspector->saveAsImage(fileName);
}

void WidgetInspectorWidget::saveAsSvg()
{
  const QString fileName =
    QFileDialog::getSaveFileName(
      this,
      tr("Save As SVG"),
      QString(),
      tr("Scalable Vector Graphics (*.svg)"));

  if (fileName.isEmpty())
    return;

  m_inspector->saveAsSvg(fileName);
}

void WidgetInspectorWidget::saveAsPdf()
{
  const QString fileName =
    QFileDialog::getSaveFileName(
      this,
      tr("Save As PDF"),
      QString(),
      tr("PDF (*.pdf)"));

  if (fileName.isEmpty())
    return;

  m_inspector->saveAsPdf(fileName);
}

void WidgetInspectorWidget::saveAsUiFile()
{
  const QString fileName =
    QFileDialog::getSaveFileName(
      this,
      tr("Save As Qt Designer UI File"),
      QString(),
      tr("Qt Designer UI File (*.ui)"));

  if (fileName.isEmpty())
    return;

  m_inspector->saveAsUiFile(fileName);
}

void WidgetInspectorWidget::analyzePainting()
{
  m_inspector->analyzePainting();

  PaintBufferViewer *viewer = new PaintBufferViewer(QStringLiteral("com.kdab.GammaRay.WidgetPaintAnalyzer"), this);
  viewer->show();
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN(WidgetInspectorUiFactory)
#endif
