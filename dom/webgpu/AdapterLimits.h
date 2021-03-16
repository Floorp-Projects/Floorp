/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_AdapterLimitss_H_
#define GPU_AdapterLimitss_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {
namespace ffi {
struct WGPULimits;
}
class Adapter;

class AdapterLimits final : public nsWrapperCache, public ChildOf<Adapter> {
  const UniquePtr<ffi::WGPULimits> mLimits;

 public:
  GPU_DECL_CYCLE_COLLECTION(AdapterLimits)
  GPU_DECL_JS_WRAP(AdapterLimits)

  uint32_t MaxTextureDimension1D() const;
  uint32_t MaxTextureDimension2D() const;
  uint32_t MaxTextureDimension3D() const;
  uint32_t MaxTextureArrayLayers() const;
  uint32_t MaxBindGroups() const;
  uint32_t MaxDynamicUniformBuffersPerPipelineLayout() const;
  uint32_t MaxDynamicStorageBuffersPerPipelineLayout() const;
  uint32_t MaxSampledTexturesPerShaderStage() const;
  uint32_t MaxSamplersPerShaderStage() const;
  uint32_t MaxStorageBuffersPerShaderStage() const;
  uint32_t MaxStorageTexturesPerShaderStage() const;
  uint32_t MaxUniformBuffersPerShaderStage() const;
  uint32_t MaxUniformBufferBindingSize() const;
  uint32_t MaxStorageBufferBindingSize() const;
  uint32_t MaxVertexBuffers() const;
  uint32_t MaxVertexAttributes() const;
  uint32_t MaxVertexBufferArrayStride() const;

  AdapterLimits(Adapter* const aParent, const ffi::WGPULimits& aLimits);

 private:
  ~AdapterLimits();
  void Cleanup() {}
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_AdapterLimitss_H_
