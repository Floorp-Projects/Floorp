/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_THEBESLAYEROGL_H
#define GFX_THEBESLAYEROGL_H

#include "Layers.h"
#include "LayerManagerOGL.h"
#include "gfxImageSurface.h"
#include "GLContext.h"
#include "base/task.h"


namespace mozilla {
namespace layers {

class ThebesLayerBufferOGL;
class BasicBufferOGL;

class ThebesLayerOGL : public ThebesLayer, 
                       public LayerOGL
{
  typedef ThebesLayerBufferOGL Buffer;

public:
  ThebesLayerOGL(LayerManagerOGL *aManager);
  virtual ~ThebesLayerOGL();

  /** Layer implementation */
  void SetVisibleRegion(const nsIntRegion& aRegion);

  /** ThebesLayer implementation */
  void InvalidateRegion(const nsIntRegion& aRegion);

  /** LayerOGL implementation */
  void Destroy();
  Layer* GetLayer();
  virtual bool IsEmpty();
  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);
  virtual void CleanupResources();

private:
  friend class BasicBufferOGL;

  bool CreateSurface();

  nsRefPtr<Buffer> mBuffer;
};


} /* layers */
} /* mozilla */
#endif /* GFX_THEBESLAYEROGL_H */
