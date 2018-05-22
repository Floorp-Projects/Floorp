/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_BLOBSURFACE_H
#define GFX_LAYERS_BLOBSURFACE_H

#include <stdint.h>                     // for uint8_t, uint32_t
#include "mozilla/Attributes.h"         // for MOZ_STACK_CLASS
#include "mozilla/RefPtr.h"             // for already_AddRefed
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor

namespace mozilla {
namespace gfx {
class DataSourceSurface;
class DrawTarget;
} // namespace gfx
} // namespace mozilla

namespace mozilla {
namespace layers {

namespace ImageDataSerializer {

// RGB

int32_t ComputeRGBStride(gfx::SurfaceFormat aFormat, int32_t aWidth);

int32_t GetRGBStride(const RGBDescriptor& aDescriptor);

uint32_t ComputeRGBBufferSize(gfx::IntSize aSize, gfx::SurfaceFormat aFormat);


// YCbCr

///This function is meant as a helper to know how much shared memory we need
///to allocate in a shmem in order to place a shared YCbCr image blob of
///given dimensions.
uint32_t ComputeYCbCrBufferSize(const gfx::IntSize& aYSize,
                                int32_t aYStride,
                                const gfx::IntSize& aCbCrSize,
                                int32_t aCbCrStride);
uint32_t ComputeYCbCrBufferSize(const gfx::IntSize& aYSize,
                                int32_t aYStride,
                                const gfx::IntSize& aCbCrSize,
                                int32_t aCbCrStride,
                                uint32_t aYOffset,
                                uint32_t aCbOffset,
                                uint32_t aCrOffset);
uint32_t ComputeYCbCrBufferSize(uint32_t aBufferSize);

void ComputeYCbCrOffsets(int32_t yStride, int32_t yHeight,
                         int32_t cbCrStride, int32_t cbCrHeight,
                         uint32_t& outYOffset, uint32_t& outCbOffset, uint32_t& outCrOffset);

gfx::SurfaceFormat FormatFromBufferDescriptor(const BufferDescriptor& aDescriptor);

gfx::IntSize SizeFromBufferDescriptor(const BufferDescriptor& aDescriptor);

Maybe<gfx::IntSize> CbCrSizeFromBufferDescriptor(const BufferDescriptor& aDescriptor);

Maybe<YUVColorSpace> YUVColorSpaceFromBufferDescriptor(const BufferDescriptor& aDescriptor);

Maybe<uint32_t> BitDepthFromBufferDescriptor(const BufferDescriptor& aDescriptor);

Maybe<StereoMode> StereoModeFromBufferDescriptor(const BufferDescriptor& aDescriptor);

uint8_t* GetYChannel(uint8_t* aBuffer, const YCbCrDescriptor& aDescriptor);

uint8_t* GetCbChannel(uint8_t* aBuffer, const YCbCrDescriptor& aDescriptor);

uint8_t* GetCrChannel(uint8_t* aBuffer, const YCbCrDescriptor& aDescriptor);

already_AddRefed<gfx::DataSourceSurface>
DataSourceSurfaceFromYCbCrDescriptor(uint8_t* aBuffer, const YCbCrDescriptor& aDescriptor, gfx::DataSourceSurface* aSurface = nullptr);

void ConvertAndScaleFromYCbCrDescriptor(uint8_t* aBuffer,
                                        const YCbCrDescriptor& aDescriptor,
                                        const gfx::SurfaceFormat& aDestFormat,
                                        const gfx::IntSize& aDestSize,
                                        unsigned char* aDestBuffer,
                                        int32_t aStride);

} // ImageDataSerializer

} // namespace layers
} // namespace mozilla

#endif
