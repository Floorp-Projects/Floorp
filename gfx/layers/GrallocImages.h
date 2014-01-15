/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GRALLOCIMAGES_H
#define GRALLOCIMAGES_H

#ifdef MOZ_WIDGET_GONK

#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/gfx/Point.h"
#include "ImageLayers.h"
#include "ImageContainer.h"

#include <ui/GraphicBuffer.h>

namespace mozilla {
namespace layers {

class GrallocTextureClientOGL;

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

/**
 * The YUV format supported by Android HAL
 *
 * 4:2:0 - CbCr width and height is half that of Y.
 *
 * This format assumes
 * - an even width
 * - an even height
 * - a horizontal stride multiple of 16 pixels
 * - a vertical stride equal to the height
 *
 * y_size = stride * height
 * c_size = ALIGN(stride/2, 16) * height/2
 * size = y_size + c_size * 2
 * cr_offset = y_size
 * cb_offset = y_size + c_size
 *
 * The Image that is rendered is the picture region defined by
 * mPicX, mPicY and mPicSize. The size of the rendered image is
 * mPicSize, not mYSize or mCbCrSize.
 */
class GrallocImage : public PlanarYCbCrImage
                   , public ISharedImage
{
  typedef PlanarYCbCrData Data;
  static uint32_t sColorIdMap[];
public:
  struct GrallocData {
    nsRefPtr<GraphicBufferLocked> mGraphicBuffer;
    gfx::IntSize mPicSize;
  };

  GrallocImage();

  virtual ~GrallocImage();

  /**
   * This makes a copy of the data buffers, in order to support functioning
   * in all different layer managers.
   */
  virtual void SetData(const Data& aData);

  /**
   *  Share the SurfaceDescriptor without making the copy, in order
   *  to support functioning in all different layer managers.
   */
  virtual void SetData(const GrallocData& aData);

  // From [android 4.0.4]/hardware/msm7k/libgralloc-qsd8k/gralloc_priv.h
  enum {
    /* OEM specific HAL formats */
    HAL_PIXEL_FORMAT_YCbCr_422_P            = 0x102,
    HAL_PIXEL_FORMAT_YCbCr_420_P            = 0x103,
    HAL_PIXEL_FORMAT_YCbCr_420_SP           = 0x109,
    HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO    = 0x10A,
    HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED     = 0x7FA30C03,
    HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS     = 0x7FA30C04,
  };

  virtual already_AddRefed<gfxASurface> DeprecatedGetAsSurface();

  void* GetNativeBuffer()
  {
    if (IsValid()) {
      return GrallocBufferActor::GetFrom(GetSurfaceDescriptor())->getNativeBuffer();
    } else {
      return nullptr;
    }
  }

  virtual bool IsValid() { return GetSurfaceDescriptor().type() != SurfaceDescriptor::T__None; }

  SurfaceDescriptor GetSurfaceDescriptor() {
    if (mGraphicBuffer.get()) {
      return mGraphicBuffer->GetSurfaceDescriptor();
    }
    return SurfaceDescriptor();
  }

  virtual ISharedImage* AsSharedImage() MOZ_OVERRIDE { return this; }

  virtual TextureClient* GetTextureClient() MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer()
  {
    return static_cast<uint8_t*>(GetNativeBuffer());
  }

private:
  bool mBufferAllocated;
  nsRefPtr<GraphicBufferLocked> mGraphicBuffer;
  RefPtr<GrallocTextureClientOGL> mTextureClient;
};

} // namespace layers
} // namespace mozilla
#endif

#endif /* GRALLOCIMAGES_H */
