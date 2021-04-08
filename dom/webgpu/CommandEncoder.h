/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CommandEncoder_H_
#define GPU_CommandEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;

namespace dom {
struct GPUComputePassDescriptor;
struct GPUTextureDataLayout;
class HTMLCanvasElement;
template <typename T>
class Sequence;
class GPUComputePipelineOrGPURenderPipeline;
class RangeEnforcedUnsignedLongSequenceOrGPUExtent3DDict;
struct GPUBufferCopyView;
struct GPUCommandBufferDescriptor;
struct GPUImageBitmapCopyView;
struct GPURenderPassDescriptor;
struct GPUTextureCopyView;
typedef RangeEnforcedUnsignedLongSequenceOrGPUExtent3DDict GPUExtent3D;
}  // namespace dom
namespace webgpu {
namespace ffi {
struct WGPUComputePass;
struct WGPURenderPass;
struct WGPUTextureDataLayout;
struct WGPUTextureCopyView_TextureId;
struct WGPUExtent3d;
}  // namespace ffi

class BindGroup;
class Buffer;
class CommandBuffer;
class ComputePassEncoder;
class Device;
class RenderPassEncoder;

class CommandEncoder final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(CommandEncoder)
  GPU_DECL_JS_WRAP(CommandEncoder)

  CommandEncoder(Device* const aParent, WebGPUChild* const aBridge, RawId aId);

  const RawId mId;

  static void ConvertTextureDataLayoutToFFI(
      const dom::GPUTextureDataLayout& aLayout,
      ffi::WGPUTextureDataLayout* aLayoutFFI);
  static void ConvertTextureCopyViewToFFI(
      const dom::GPUTextureCopyView& aView,
      ffi::WGPUTextureCopyView_TextureId* aViewFFI);
  static void ConvertExtent3DToFFI(const dom::GPUExtent3D& aExtent,
                                   ffi::WGPUExtent3d* aExtentFFI);

 private:
  ~CommandEncoder();
  void Cleanup();

  RefPtr<WebGPUChild> mBridge;
  // TODO: support multiple target canvases per command encoder
  WeakPtr<dom::HTMLCanvasElement> mTargetCanvasElement;

 public:
  void EndComputePass(ffi::WGPUComputePass& aPass, ErrorResult& aRv);
  void EndRenderPass(ffi::WGPURenderPass& aPass, ErrorResult& aRv);

  void CopyBufferToBuffer(const Buffer& aSource, BufferAddress aSourceOffset,
                          const Buffer& aDestination,
                          BufferAddress aDestinationOffset,
                          BufferAddress aSize);
  void CopyBufferToTexture(const dom::GPUBufferCopyView& aSource,
                           const dom::GPUTextureCopyView& aDestination,
                           const dom::GPUExtent3D& aCopySize);
  void CopyTextureToBuffer(const dom::GPUTextureCopyView& aSource,
                           const dom::GPUBufferCopyView& aDestination,
                           const dom::GPUExtent3D& aCopySize);
  void CopyTextureToTexture(const dom::GPUTextureCopyView& aSource,
                            const dom::GPUTextureCopyView& aDestination,
                            const dom::GPUExtent3D& aCopySize);

  already_AddRefed<ComputePassEncoder> BeginComputePass(
      const dom::GPUComputePassDescriptor& aDesc);
  already_AddRefed<RenderPassEncoder> BeginRenderPass(
      const dom::GPURenderPassDescriptor& aDesc);
  already_AddRefed<CommandBuffer> Finish(
      const dom::GPUCommandBufferDescriptor& aDesc);
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_CommandEncoder_H_
