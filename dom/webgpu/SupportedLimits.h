/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_SupportedLimits_H_
#define GPU_SupportedLimits_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

#include <memory>

namespace mozilla::webgpu {
namespace ffi {
struct WGPULimits;
}
class Adapter;

enum class Limit : uint8_t {
  MaxTextureDimension1D,
  MaxTextureDimension2D,
  MaxTextureDimension3D,
  MaxTextureArrayLayers,
  MaxBindGroups,
  MaxBindGroupsPlusVertexBuffers,
  MaxBindingsPerBindGroup,
  MaxDynamicUniformBuffersPerPipelineLayout,
  MaxDynamicStorageBuffersPerPipelineLayout,
  MaxSampledTexturesPerShaderStage,
  MaxSamplersPerShaderStage,
  MaxStorageBuffersPerShaderStage,
  MaxStorageTexturesPerShaderStage,
  MaxUniformBuffersPerShaderStage,
  MaxUniformBufferBindingSize,
  MaxStorageBufferBindingSize,
  MinUniformBufferOffsetAlignment,
  MinStorageBufferOffsetAlignment,
  MaxVertexBuffers,
  MaxBufferSize,
  MaxVertexAttributes,
  MaxVertexBufferArrayStride,
  MaxInterStageShaderComponents,
  MaxInterStageShaderVariables,
  MaxColorAttachments,
  MaxColorAttachmentBytesPerSample,
  MaxComputeWorkgroupStorageSize,
  MaxComputeInvocationsPerWorkgroup,
  MaxComputeWorkgroupSizeX,
  MaxComputeWorkgroupSizeY,
  MaxComputeWorkgroupSizeZ,
  MaxComputeWorkgroupsPerDimension,
  _LAST = MaxComputeWorkgroupsPerDimension,
};

uint64_t GetLimit(const ffi::WGPULimits&, Limit);
void SetLimit(ffi::WGPULimits*, Limit, double);

class SupportedLimits final : public nsWrapperCache, public ChildOf<Adapter> {
 public:
  const std::unique_ptr<ffi::WGPULimits> mFfi;

  GPU_DECL_CYCLE_COLLECTION(SupportedLimits)
  GPU_DECL_JS_WRAP(SupportedLimits)

#define _(X) \
  auto X() const { return GetLimit(*mFfi, Limit::X); }

  _(MaxTextureDimension1D)
  _(MaxTextureDimension2D)
  _(MaxTextureDimension3D)
  _(MaxTextureArrayLayers)
  _(MaxBindGroups)
  _(MaxBindGroupsPlusVertexBuffers)
  _(MaxBindingsPerBindGroup)
  _(MaxDynamicUniformBuffersPerPipelineLayout)
  _(MaxDynamicStorageBuffersPerPipelineLayout)
  _(MaxSampledTexturesPerShaderStage)
  _(MaxSamplersPerShaderStage)
  _(MaxStorageBuffersPerShaderStage)
  _(MaxStorageTexturesPerShaderStage)
  _(MaxUniformBuffersPerShaderStage)
  _(MaxUniformBufferBindingSize)
  _(MaxStorageBufferBindingSize)
  _(MinUniformBufferOffsetAlignment)
  _(MinStorageBufferOffsetAlignment)
  _(MaxVertexBuffers)
  _(MaxBufferSize)
  _(MaxVertexAttributes)
  _(MaxVertexBufferArrayStride)
  _(MaxInterStageShaderComponents)
  _(MaxInterStageShaderVariables)
  _(MaxColorAttachments)
  _(MaxColorAttachmentBytesPerSample)
  _(MaxComputeWorkgroupStorageSize)
  _(MaxComputeInvocationsPerWorkgroup)
  _(MaxComputeWorkgroupSizeX)
  _(MaxComputeWorkgroupSizeY)
  _(MaxComputeWorkgroupSizeZ)
  _(MaxComputeWorkgroupsPerDimension)

#undef _

  SupportedLimits(Adapter* const aParent, const ffi::WGPULimits&);

 private:
  ~SupportedLimits();
  void Cleanup() {}
};

}  // namespace mozilla::webgpu

#endif  // GPU_SupportedLimits_H_
