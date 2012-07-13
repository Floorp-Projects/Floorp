/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICBUFFERS_H
#define GFX_BASICBUFFERS_H

#include "BasicLayersImpl.h"

namespace mozilla {
namespace layers {

class BasicThebesLayer;
class BasicThebesLayerBuffer : public ThebesLayerBuffer {
  typedef ThebesLayerBuffer Base;

public:
  BasicThebesLayerBuffer(BasicThebesLayer* aLayer)
    : Base(ContainsVisibleBounds)
    , mLayer(aLayer)
  {
  }

  virtual ~BasicThebesLayerBuffer()
  {}

  using Base::BufferRect;
  using Base::BufferRotation;

  /**
   * Complete the drawing operation. The region to draw must have been
   * drawn before this is called. The contents of the buffer are drawn
   * to aTarget.
   */
  void DrawTo(ThebesLayer* aLayer, gfxContext* aTarget, float aOpacity,
              Layer* aMaskLayer);

  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType aType, const nsIntSize& aSize, PRUint32 aFlags);

  /**
   * Swap out the old backing buffer for |aBuffer| and attributes.
   */
  void SetBackingBuffer(gfxASurface* aBuffer,
                        const nsIntRect& aRect, const nsIntPoint& aRotation)
  {
    gfxIntSize prevSize = gfxIntSize(BufferRect().width, BufferRect().height);
    gfxIntSize newSize = aBuffer->GetSize();
    NS_ABORT_IF_FALSE(newSize == prevSize,
                      "Swapped-in buffer size doesn't match old buffer's!");
    nsRefPtr<gfxASurface> oldBuffer;
    oldBuffer = SetBuffer(aBuffer, aRect, aRotation);
  }

  void SetBackingBufferAndUpdateFrom(
    gfxASurface* aBuffer,
    gfxASurface* aSource, const nsIntRect& aRect, const nsIntPoint& aRotation,
    const nsIntRegion& aUpdateRegion);

  /**
   * When BasicThebesLayerBuffer is used with layers that hold
   * SurfaceDescriptor, this buffer only has a valid gfxASurface in
   * the scope of an AutoOpenSurface for that SurfaceDescriptor.  That
   * is, it's sort of a "virtual buffer" that's only mapped an
   * unmapped within the scope of AutoOpenSurface.  None of the
   * underlying buffer attributes (rect, rotation) are affected by
   * mapping/unmapping.
   *
   * These helpers just exist to provide more descriptive names of the
   * map/unmap process.
   */
  void MapBuffer(gfxASurface* aBuffer)
  {
    SetBuffer(aBuffer);
  }
  void UnmapBuffer()
  {
    SetBuffer(nsnull);
  }

private:
  BasicThebesLayerBuffer(gfxASurface* aBuffer,
                         const nsIntRect& aRect, const nsIntPoint& aRotation)
    // The size policy doesn't really matter here; this constructor is
    // intended to be used for creating temporaries
    : ThebesLayerBuffer(ContainsVisibleBounds)
  {
    SetBuffer(aBuffer, aRect, aRotation);
  }

  BasicThebesLayer* mLayer;
};

class ShadowThebesLayerBuffer : public BasicThebesLayerBuffer
{
  typedef BasicThebesLayerBuffer Base;

public:
  ShadowThebesLayerBuffer()
    : Base(NULL)
  {
    MOZ_COUNT_CTOR(ShadowThebesLayerBuffer);
  }

  ~ShadowThebesLayerBuffer()
  {
    MOZ_COUNT_DTOR(ShadowThebesLayerBuffer);
  }

  /**
   * Swap in the old "virtual buffer" (see above) attributes in aNew*
   * and return the old ones in aOld*.
   *
   * Swap() must only be called when the buffer is in its "unmapped"
   * state, that is the underlying gfxASurface is not available.  It
   * is expected that the owner of this buffer holds an unmapped
   * SurfaceDescriptor as the backing storage for this buffer.  That's
   * why no gfxASurface or SurfaceDescriptor parameters appear here.
   */
  void Swap(const nsIntRect& aNewRect, const nsIntPoint& aNewRotation,
            nsIntRect* aOldRect, nsIntPoint* aOldRotation)
  {
    *aOldRect = BufferRect();
    *aOldRotation = BufferRotation();

    nsRefPtr<gfxASurface> oldBuffer;
    oldBuffer = SetBuffer(nsnull, aNewRect, aNewRotation);
    MOZ_ASSERT(!oldBuffer);
  }

protected:
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType, const nsIntSize&, PRUint32)
  {
    NS_RUNTIMEABORT("ShadowThebesLayer can't paint content");
    return nsnull;
  }
};

}
}

#endif
