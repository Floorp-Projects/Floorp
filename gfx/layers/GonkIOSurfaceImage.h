/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONKIOSURFACEIMAGE_H
#define GONKIOSURFACEIMAGE_H

#ifdef MOZ_WIDGET_GONK

#include "mozilla/layers/LayersSurfaces.h"
#include "ImageLayers.h"

#include <ui/GraphicBuffer.h>

namespace mozilla {
namespace layers {

/**
 * The gralloc buffer maintained by android GraphicBuffer can be
 * shared between the compositor thread and the producer thread. The
 * mGraphicBuffer is owned by the producer thread, but when it is
 * wrapped by GraphicBufferLocked and passed to the compositor, the
 * buffer content is guaranteed to not change until Unlock() is
 * called. Each producer must maintain their own buffer queue and
 * implement the GraphicBufferLocked::Unlock() interface.
 */
class GraphicBufferLocked {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GraphicBufferLocked)

public:
  GraphicBufferLocked(SurfaceDescriptor aGraphicBuffer)
    : mSurfaceDescriptor(aGraphicBuffer)
  {}

  virtual ~GraphicBufferLocked() {}

  virtual void Unlock() {}

  SurfaceDescriptor GetSurfaceDescriptor()
  {
    return mSurfaceDescriptor;
  }

protected:
  SurfaceDescriptor mSurfaceDescriptor;
};

class THEBES_API GonkIOSurfaceImage : public Image {
public:
  struct Data {
    nsRefPtr<GraphicBufferLocked> mGraphicBuffer;
    gfxIntSize mPicSize;
  };
  GonkIOSurfaceImage()
    : Image(NULL, GONK_IO_SURFACE)
    , mSize(0, 0)
    {}

  virtual ~GonkIOSurfaceImage()
  {
    mGraphicBuffer->Unlock();
  }

  virtual void SetData(const Data& aData)
  {
    mGraphicBuffer = aData.mGraphicBuffer;
    mSize = aData.mPicSize;
  }

  virtual gfxIntSize GetSize()
  {
    return mSize;
  }

  virtual already_AddRefed<gfxASurface> GetAsSurface()
  {
    // We need to fix this and return a ASurface at some point.
    return nullptr;
  }

  void* GetNativeBuffer()
  {
    return GrallocBufferActor::GetFrom(GetSurfaceDescriptor())->getNativeBuffer();
  }

  SurfaceDescriptor GetSurfaceDescriptor()
  {
    return mGraphicBuffer->GetSurfaceDescriptor();
  }

private:
  nsRefPtr<GraphicBufferLocked> mGraphicBuffer;
  gfxIntSize mSize;
};

} // namespace layers
} // namespace mozilla
#endif

#endif /* GONKIOSURFACEIMAGE_H */
