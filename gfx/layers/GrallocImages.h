/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GRALLOCIMAGES_H
#define GRALLOCIMAGES_H

#ifdef MOZ_WIDGET_GONK

#include "mozilla/layers/LayersSurfaces.h"
#include "ImageLayers.h"

#include <ui/GraphicBuffer.h>

namespace mozilla {
namespace layers {

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
class GrallocPlanarYCbCrImage : public PlanarYCbCrImage {
  typedef PlanarYCbCrImage::Data Data;

public:
  GrallocPlanarYCbCrImage();

  virtual ~GrallocPlanarYCbCrImage();

  /**
   * This makes a copy of the data buffers, in order to support functioning
   * in all different layer managers.
   */
  virtual void SetData(const Data& aData);

  virtual uint32_t GetDataSize() { return 0; }

  virtual bool IsValid() { return mSurfaceDescriptor.type() != SurfaceDescriptor::T__None; }

  SurfaceDescriptor GetSurfaceDescriptor() {
    return mSurfaceDescriptor;
  }

private:
  SurfaceDescriptor mSurfaceDescriptor;
};

} // namespace layers
} // namespace mozilla
#endif

#endif /* GRALLOCIMAGES_H */
