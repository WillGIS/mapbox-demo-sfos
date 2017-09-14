/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2017 Mapbox, Inc.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgmapboxglnode.h"
#include "qsgtextureplain.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>

#include <QDebug>

static const QSize minTextureSize = QSize(64, 64);


QSGMapboxGLTextureNode::QSGMapboxGLTextureNode(const QMapboxGLSettings &settings, const QSize &size, qreal pixelRatio, QQuickItem *item)
  : QSGSimpleTextureNode()
{
  qDebug() << "QSGMapboxGLTextureNode";

  setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);
  setFiltering(QSGTexture::Linear);

  m_map.reset(new QMapboxGL(nullptr, settings, size.expandedTo(minTextureSize), pixelRatio));

  m_map->setZoom(12);
  m_map->setCoordinate(QMapbox::Coordinate(59.437, 24.7536));

  auto& styles = QMapbox::defaultStyles();
  m_map->setStyleUrl(styles[0].first);

  QObject::connect(m_map.data(), &QMapboxGL::needsRendering, item, &QQuickItem::update);
  QObject::connect(m_map.data(), &QMapboxGL::copyrightsChanged, item, &QQuickItem::update);

  resize(size, 1); // to fill and attach fbo
}

void QSGMapboxGLTextureNode::resize(const QSize &size, qreal pixelRatio)
{

  qDebug() << "QSGMapboxGLTextureNode resize";

  const QSize& minSize = size.expandedTo(minTextureSize);
  const QSize fbSize = minSize * pixelRatio;
  m_map->resize(minSize, fbSize);

  m_fbo.reset(new QOpenGLFramebufferObject(fbSize, QOpenGLFramebufferObject::CombinedDepthStencil));
  m_map->setFramebufferObject(m_fbo->handle());

  QSGTexturePlain *fboTexture = static_cast<QSGTexturePlain *>(texture());
  if (!fboTexture)
    fboTexture = new QSGTexturePlain;

  fboTexture->setTextureId(m_fbo->texture());
  fboTexture->setTextureSize(fbSize);

  if (!texture()) {
      setTexture(fboTexture);
      setOwnsTexture(true);
    }

  setRect(QRectF(QPointF(), minSize));
  markDirty(QSGNode::DirtyGeometry);
}

bool QSGMapboxGLTextureNode::render(QQuickWindow *window)
{
  qDebug() << "QSGMapboxGLTextureNode render: " << m_fbo->width() << " " << m_fbo->height() << " " << m_map->isFullyLoaded();

  bool loaded = m_map->isFullyLoaded();
  QOpenGLFunctions *f = window->openglContext()->functions();
  f->glViewport(0, 0, m_fbo->width(), m_fbo->height());

  GLint alignment;
  f->glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);

  m_fbo->bind();
  m_map->render();
  //    m_logo.render();
  m_fbo->release();

  // QTBUG-62861
  f->glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

  window->resetOpenGLState();
  markDirty(QSGNode::DirtyMaterial);

  return loaded;
}
