/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_THEBESLAYEROGL_H
#define GFX_THEBESLAYEROGL_H

#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"

#include "Layers.h"
#include "LayerManagerOGL.h"
#include "gfxImageSurface.h"
#include "GLContext.h"
#include "base/task.h"


namespace mozilla {
namespace layers {

class ThebesLayerBufferOGL;
class BasicBufferOGL;
class ShadowBufferOGL;

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

class ShadowThebesLayerBufferOGL
{
public:
  ShadowThebesLayerBufferOGL()
  {
    MOZ_COUNT_CTOR(ShadowThebesLayerBufferOGL);
  }

  ~ShadowThebesLayerBufferOGL()
  {
    MOZ_COUNT_DTOR(ShadowThebesLayerBufferOGL);
  }

  void Swap(const SurfaceDescriptor& aDescriptor,
            const nsIntRect& aNewRect, const nsIntPoint& aNewRotation,
            SurfaceDescriptor* aOldDescriptor,
            nsIntRect* aOldRect, nsIntPoint* aOldRotation)
  {
    *aOldDescriptor = mBuffer;
    *aOldRect = mBufferRect;
    *aOldRotation = mBufferRotation;

    mBuffer = aDescriptor;
    mBufferRect = aNewRect;
    mBufferRotation = aNewRotation;
  }

  nsIntRect Rect() {
    return mBufferRect;
  }

  nsIntPoint Rotation() {
    return mBufferRotation;
  }

  SurfaceDescriptor Buffer() {
    return mBuffer;
  }

  /**
   * Wipe out all retained contents. Call this when the entire
   * buffer becomes invalid.
   */
  void Clear()
  {
    mBufferRect.SetEmpty();
  }

protected:
  SurfaceDescriptor mBuffer;
  nsIntRect mBufferRect;
  nsIntPoint mBufferRotation;
};

class ShadowThebesLayerOGL : public ShadowThebesLayer,
                             public LayerOGL
{
public:
  ShadowThebesLayerOGL(LayerManagerOGL *aManager);
  virtual ~ShadowThebesLayerOGL();

  virtual void
  Swap(const ThebesBuffer& aNewFront, const nsIntRegion& aUpdatedRegion,
       OptionalThebesBuffer* aNewBack, nsIntRegion* aNewBackValidRegion,
       OptionalThebesBuffer* aReadOnlyFront, nsIntRegion* aFrontUpdatedRegion);
  virtual void DestroyFrontBuffer();

  virtual void Disconnect();

  virtual void SetValidRegion(const nsIntRegion& aRegion)
  {
    ShadowThebesLayer::SetValidRegion(aRegion);
  }

  // LayerOGL impl
  void Destroy();
  Layer* GetLayer();
  virtual bool IsEmpty();
  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);
  virtual void CleanupResources();

private:
  nsRefPtr<ShadowBufferOGL> mBuffer;
  SurfaceDescriptor mBufferDescriptor;
  nsIntRegion mValidRegionForNextBackBuffer;
};

} /* layers */
} /* mozilla */
#endif /* GFX_THEBESLAYEROGL_H */
