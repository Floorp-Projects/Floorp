/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_BLOBSURFACE_H
#define GFX_LAYERS_BLOBSURFACE_H

#include <stdint.h>                     // for uint8_t, uint32_t
#include "mozilla/Attributes.h"         // for MOZ_STACK_CLASS
#include "mozilla/RefPtr.h"             // for TemporaryRef
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Types.h"          // for SurfaceFormat

class gfxImageSurface;

namespace mozilla {
namespace gfx {
class DataSourceSurface;
class DrawTarget;
} // namespace gfx
} // namespace mozilla

namespace mozilla {
namespace layers {

class ImageDataSerializerBase
{
public:
  bool IsValid() const;

  uint8_t* GetData();
  uint32_t GetStride() const;
  gfx::IntSize GetSize() const;
  gfx::SurfaceFormat GetFormat() const;
  TemporaryRef<gfx::DataSourceSurface> GetAsSurface();
  TemporaryRef<gfxImageSurface> GetAsThebesSurface();
  TemporaryRef<gfx::DrawTarget> GetAsDrawTarget();

protected:

  ImageDataSerializerBase(uint8_t* aData)
  : mData(aData) {}
  uint8_t* mData;
};

/**
 * A facility to serialize an image into a buffer of memory.
 * This is intended for use with the IPC code, in order to copy image data
 * into shared memory.
 * Note that there is a separate serializer class for YCbCr images
 * (see YCbCrImageDataSerializer.h).
 */
class MOZ_STACK_CLASS ImageDataSerializer : public ImageDataSerializerBase
{
public:
  ImageDataSerializer(uint8_t* aData) : ImageDataSerializerBase(aData) {}
  void InitializeBufferInfo(gfx::IntSize aSize,
                            gfx::SurfaceFormat aFormat);
  static uint32_t ComputeMinBufferSize(gfx::IntSize aSize,
                                       gfx::SurfaceFormat aFormat);
};

/**
 * A facility to deserialize image data that has been serialized by an
 * ImageDataSerializer.
 */
class MOZ_STACK_CLASS ImageDataDeserializer : public ImageDataSerializerBase
{
public:
  ImageDataDeserializer(uint8_t* aData) : ImageDataSerializerBase(aData) {}
};

} // namespace layers
} // namespace mozilla

#endif
