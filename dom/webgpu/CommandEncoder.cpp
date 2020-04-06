/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CommandEncoder.h"

#include "CommandBuffer.h"
#include "Buffer.h"
#include "ComputePassEncoder.h"
#include "Device.h"
#include "RenderPassEncoder.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(CommandEncoder, mParent, mBridge)
GPU_IMPL_JS_WRAP(CommandEncoder)

CommandEncoder::CommandEncoder(Device* const aParent,
                               WebGPUChild* const aBridge, RawId aId)
    : ChildOf(aParent), mId(aId), mBridge(aBridge) {}

CommandEncoder::~CommandEncoder() { Cleanup(); }

void CommandEncoder::Cleanup() {
  if (mValid && mParent) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendCommandEncoderDestroy(mId);
    }
  }
}

void CommandEncoder::CopyBufferToBuffer(const Buffer& aSource,
                                        BufferAddress aSourceOffset,
                                        const Buffer& aDestination,
                                        BufferAddress aDestinationOffset,
                                        BufferAddress aSize) {
  if (mValid) {
    mBridge->SendCommandEncoderCopyBufferToBuffer(
        mId, aSource.mId, aSourceOffset, aDestination.mId, aDestinationOffset,
        aSize);
  }
}

already_AddRefed<ComputePassEncoder> CommandEncoder::BeginComputePass(
    const dom::GPUComputePassDescriptor& aDesc) {
  RefPtr<ComputePassEncoder> pass = new ComputePassEncoder(this, aDesc);
  return pass.forget();
}

already_AddRefed<RenderPassEncoder> CommandEncoder::BeginRenderPass(
    const dom::GPURenderPassDescriptor& aDesc) {
  RefPtr<RenderPassEncoder> pass = new RenderPassEncoder(this, aDesc);
  return pass.forget();
}

void CommandEncoder::EndComputePass(Span<const uint8_t> aData,
                                    ErrorResult& aRv) {
  if (!mValid) {
    return aRv.ThrowInvalidStateError("Command encoder is not valid");
  }
  ipc::Shmem shmem;
  if (!mBridge->AllocShmem(aData.Length(), ipc::Shmem::SharedMemory::TYPE_BASIC,
                           &shmem)) {
    return aRv.ThrowAbortError(nsPrintfCString(
        "Unable to allocate shmem of size %zu", aData.Length()));
  }

  memcpy(shmem.get<uint8_t>(), aData.data(), aData.Length());
  mBridge->SendCommandEncoderRunComputePass(mId, std::move(shmem));
}

void CommandEncoder::EndRenderPass(Span<const uint8_t> aData,
                                   ErrorResult& aRv) {
  if (!mValid) {
    return aRv.ThrowInvalidStateError("Command encoder is not valid");
  }
  ipc::Shmem shmem;
  if (!mBridge->AllocShmem(aData.Length(), ipc::Shmem::SharedMemory::TYPE_BASIC,
                           &shmem)) {
    return aRv.ThrowAbortError(nsPrintfCString(
        "Unable to allocate shmem of size %zu", aData.Length()));
  }

  memcpy(shmem.get<uint8_t>(), aData.data(), aData.Length());
  mBridge->SendCommandEncoderRunRenderPass(mId, std::move(shmem));
}

already_AddRefed<CommandBuffer> CommandEncoder::Finish(
    const dom::GPUCommandBufferDescriptor& aDesc) {
  RawId id = 0;
  if (mValid) {
    mValid = false;
    id = mBridge->CommandEncoderFinish(mId, aDesc);
  }
  RefPtr<CommandBuffer> comb = new CommandBuffer(mParent, id);
  return comb.forget();
}

}  // namespace webgpu
}  // namespace mozilla
