/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_UTIL_H_
#define GPU_UTIL_H_

#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
class ErrorResult;

namespace dom {
struct GPUComputePassDescriptor;
template <typename T>
class Sequence;
using GPUExtent3D = RangeEnforcedUnsignedLongSequenceOrGPUExtent3DDict;
using OwningGPUExtent3D =
    OwningRangeEnforcedUnsignedLongSequenceOrGPUExtent3DDict;
}  // namespace dom
namespace webgpu {
namespace ffi {
struct WGPUExtent3d;
}  // namespace ffi

void ConvertExtent3DToFFI(const dom::GPUExtent3D& aExtent,
                          ffi::WGPUExtent3d* aExtentFFI);

void ConvertExtent3DToFFI(const dom::OwningGPUExtent3D& aExtent,
                          ffi::WGPUExtent3d* aExtentFFI);

ffi::WGPUExtent3d ConvertExtent(const dom::GPUExtent3D& aExtent);

ffi::WGPUExtent3d ConvertExtent(const dom::OwningGPUExtent3D& aExtent);

ffi::WGPUCompareFunction ConvertCompareFunction(
    const dom::GPUCompareFunction& aCompare);

ffi::WGPUTextureFormat ConvertTextureFormat(
    const dom::GPUTextureFormat& aFormat);

ffi::WGPUMultisampleState ConvertMultisampleState(
    const dom::GPUMultisampleState& aDesc);

ffi::WGPUBlendComponent ConvertBlendComponent(
    const dom::GPUBlendComponent& aDesc);

ffi::WGPUStencilFaceState ConvertStencilFaceState(
    const dom::GPUStencilFaceState& aDesc);

ffi::WGPUDepthStencilState ConvertDepthStencilState(
    const dom::GPUDepthStencilState& aDesc);

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_UTIL_H_
