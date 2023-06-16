/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Utility.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/webgpu/WebGPUTypes.h"

namespace mozilla::webgpu {

template <typename E>
void ConvertToExtent3D(const E& aExtent, ffi::WGPUExtent3d* aExtentFFI) {
  *aExtentFFI = {};
  if (aExtent.IsRangeEnforcedUnsignedLongSequence()) {
    const auto& seq = aExtent.GetAsRangeEnforcedUnsignedLongSequence();
    aExtentFFI->width = seq.Length() > 0 ? seq[0] : 0;
    aExtentFFI->height = seq.Length() > 1 ? seq[1] : 1;
    aExtentFFI->depth_or_array_layers = seq.Length() > 2 ? seq[2] : 1;
  } else if (aExtent.IsGPUExtent3DDict()) {
    const auto& dict = aExtent.GetAsGPUExtent3DDict();
    aExtentFFI->width = dict.mWidth;
    aExtentFFI->height = dict.mHeight;
    aExtentFFI->depth_or_array_layers = dict.mDepthOrArrayLayers;
  } else {
    MOZ_CRASH("Unexpected extent type");
  }
}

void ConvertExtent3DToFFI(const dom::GPUExtent3D& aExtent,
                          ffi::WGPUExtent3d* aExtentFFI) {
  ConvertToExtent3D(aExtent, aExtentFFI);
}

void ConvertExtent3DToFFI(const dom::OwningGPUExtent3D& aExtent,
                          ffi::WGPUExtent3d* aExtentFFI) {
  ConvertToExtent3D(aExtent, aExtentFFI);
}

ffi::WGPUExtent3d ConvertExtent(const dom::GPUExtent3D& aExtent) {
  ffi::WGPUExtent3d extent = {};
  ConvertToExtent3D(aExtent, &extent);
  return extent;
}

ffi::WGPUExtent3d ConvertExtent(const dom::OwningGPUExtent3D& aExtent) {
  ffi::WGPUExtent3d extent = {};
  ConvertToExtent3D(aExtent, &extent);
  return extent;
}

}  // namespace mozilla::webgpu
