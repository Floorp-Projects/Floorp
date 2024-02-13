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
template <typename T>
class Sequence;
struct GPUCommandBufferDescriptor;
class GPUComputePipelineOrGPURenderPipeline;
class RangeEnforcedUnsignedLongSequenceOrGPUExtent3DDict;
struct GPUImageCopyBuffer;
struct GPUImageCopyTexture;
struct GPUImageBitmapCopyView;
struct GPUImageDataLayout;
struct GPURenderPassDescriptor;
using GPUExtent3D = RangeEnforcedUnsignedLongSequenceOrGPUExtent3DDict;
}  // namespace dom
namespace webgpu {
namespace ffi {
struct WGPUComputePass;
struct WGPURenderPass;
struct WGPUImageDataLayout;
struct WGPUImageCopyTexture_TextureId;
struct WGPUExtent3d;
}  // namespace ffi

class BindGroup;
class Buffer;
class CanvasContext;
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
      const dom::GPUImageDataLayout& aLayout,
      ffi::WGPUImageDataLayout* aLayoutFFI);
  static void ConvertTextureCopyViewToFFI(
      const dom::GPUImageCopyTexture& aCopy,
      ffi::WGPUImageCopyTexture_TextureId* aViewFFI);

 private:
  ~CommandEncoder();
  void Cleanup();

  RefPtr<WebGPUChild> mBridge;
  nsTArray<WeakPtr<CanvasContext>> mTargetContexts;

 public:
  const auto& GetDevice() const { return mParent; };

  void EndComputePass(ffi::WGPUComputePass& aPass);
  void EndRenderPass(ffi::WGPURenderPass& aPass);

  void CopyBufferToBuffer(const Buffer& aSource, BufferAddress aSourceOffset,
                          const Buffer& aDestination,
                          BufferAddress aDestinationOffset,
                          BufferAddress aSize);
  void CopyBufferToTexture(const dom::GPUImageCopyBuffer& aSource,
                           const dom::GPUImageCopyTexture& aDestination,
                           const dom::GPUExtent3D& aCopySize);
  void CopyTextureToBuffer(const dom::GPUImageCopyTexture& aSource,
                           const dom::GPUImageCopyBuffer& aDestination,
                           const dom::GPUExtent3D& aCopySize);
  void CopyTextureToTexture(const dom::GPUImageCopyTexture& aSource,
                            const dom::GPUImageCopyTexture& aDestination,
                            const dom::GPUExtent3D& aCopySize);
  void ClearBuffer(const Buffer& aBuffer, const uint64_t aOffset,
                   const dom::Optional<uint64_t>& aSize);

  void PushDebugGroup(const nsAString& aString);
  void PopDebugGroup();
  void InsertDebugMarker(const nsAString& aString);

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
