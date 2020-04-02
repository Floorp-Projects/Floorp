/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ArrayBuffer.h"
#include "js/Value.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "Device.h"

#include "Adapter.h"
#include "Buffer.h"
#include "ComputePipeline.h"
#include "Queue.h"
#include "RenderPipeline.h"
#include "Sampler.h"
#include "Texture.h"
#include "TextureView.h"
#include "ipc/WebGPUChild.h"

namespace mozilla {
namespace webgpu {

mozilla::LazyLogModule gWebGPULog("WebGPU");

NS_IMPL_CYCLE_COLLECTION_INHERITED(Device, DOMEventTargetHelper, mBridge,
                                   mQueue)
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(Device, DOMEventTargetHelper)
GPU_IMPL_JS_WRAP(Device)

static void mapFreeCallback(void* aContents, void* aUserData) {
  Unused << aContents;
  Unused << aUserData;
}

RefPtr<WebGPUChild> Device::GetBridge() { return mBridge; }

JSObject* Device::CreateExternalArrayBuffer(JSContext* aCx, size_t aSize,
                                            ipc::Shmem& aShmem) {
  MOZ_ASSERT(aShmem.Size<uint8_t>() == aSize);
  return JS::NewExternalArrayBuffer(aCx, aSize, aShmem.get<uint8_t>(),
                                    &mapFreeCallback, nullptr);
}

Device::Device(Adapter* const aParent, RawId aId)
    : DOMEventTargetHelper(aParent->GetParentObject()),
      mBridge(aParent->mBridge),
      mId(aId),
      mQueue(new Queue(this, aParent->mBridge, aId)) {}

Device::~Device() { Cleanup(); }

void Device::Cleanup() {
  if (mValid && mBridge && mBridge->IsOpen()) {
    mValid = false;
    mBridge->SendDeviceDestroy(mId);
  }
}

void Device::GetLabel(nsAString& aValue) const { aValue = mLabel; }
void Device::SetLabel(const nsAString& aLabel) { mLabel = aLabel; }

Queue* Device::DefaultQueue() const { return mQueue; }

already_AddRefed<Buffer> Device::CreateBuffer(
    const dom::GPUBufferDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateBuffer(mId, aDesc);
  RefPtr<Buffer> buffer = new Buffer(this, id, aDesc.mSize);
  return buffer.forget();
}

void Device::CreateBufferMapped(JSContext* aCx,
                                const dom::GPUBufferDescriptor& aDesc,
                                nsTArray<JS::Value>& aSequence,
                                ErrorResult& aRv) {
  const auto checked = CheckedInt<size_t>(aDesc.mSize);
  if (!checked.isValid()) {
    aRv.ThrowRangeError("Mapped size is too large");
    return;
  }
  const auto& size = checked.value();

  // TODO: use `ShmemPool`
  ipc::Shmem shmem;
  if (!mBridge->AllocShmem(size, ipc::Shmem::SharedMemory::TYPE_BASIC,
                           &shmem)) {
    aRv.ThrowAbortError(
        nsPrintfCString("Unable to allocate shmem of size %" PRIuPTR, size));
    return;
  }

  // zero out memory
  memset(shmem.get<uint8_t>(), 0, size);

  JS::Rooted<JSObject*> arrayBuffer(
      aCx, CreateExternalArrayBuffer(aCx, size, shmem));
  if (!arrayBuffer) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  dom::GPUBufferDescriptor modifiedDesc(aDesc);
  modifiedDesc.mUsage |= dom::GPUBufferUsage_Binding::MAP_WRITE;
  RawId id = mBridge->DeviceCreateBuffer(mId, modifiedDesc);
  RefPtr<Buffer> buffer = new Buffer(this, id, aDesc.mSize);

  JS::Rooted<JS::Value> bufferValue(aCx);
  if (!dom::ToJSValue(aCx, buffer, &bufferValue)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  aSequence.AppendElement(bufferValue);
  aSequence.AppendElement(JS::ObjectValue(*arrayBuffer));

  buffer->InitMapping(std::move(shmem), arrayBuffer);
}

RefPtr<MappingPromise> Device::MapBufferForReadAsync(RawId aId, size_t aSize,
                                                     ErrorResult& aRv) {
  ipc::Shmem shmem;
  if (!mBridge->AllocShmem(aSize, ipc::Shmem::SharedMemory::TYPE_BASIC,
                           &shmem)) {
    aRv.ThrowAbortError(
        nsPrintfCString("Unable to allocate shmem of size %" PRIuPTR, aSize));
    return nullptr;
  }

  return mBridge->SendBufferMapRead(aId, std::move(shmem));
}

void Device::UnmapBuffer(RawId aId, UniquePtr<ipc::Shmem> aShmem) {
  mBridge->SendDeviceUnmapBuffer(mId, aId, std::move(*aShmem));
}

already_AddRefed<Texture> Device::CreateTexture(
    const dom::GPUTextureDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateTexture(mId, aDesc);
  RefPtr<Texture> texture = new Texture(this, id, aDesc);
  return texture.forget();
}

already_AddRefed<Sampler> Device::CreateSampler(
    const dom::GPUSamplerDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateSampler(mId, aDesc);
  RefPtr<Sampler> sampler = new Sampler(this, id);
  return sampler.forget();
}

already_AddRefed<CommandEncoder> Device::CreateCommandEncoder(
    const dom::GPUCommandEncoderDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateCommandEncoder(mId, aDesc);
  RefPtr<CommandEncoder> encoder = new CommandEncoder(this, mBridge, id);
  return encoder.forget();
}

already_AddRefed<BindGroupLayout> Device::CreateBindGroupLayout(
    const dom::GPUBindGroupLayoutDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateBindGroupLayout(mId, aDesc);
  RefPtr<BindGroupLayout> object = new BindGroupLayout(this, id);
  return object.forget();
}
already_AddRefed<PipelineLayout> Device::CreatePipelineLayout(
    const dom::GPUPipelineLayoutDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreatePipelineLayout(mId, aDesc);
  RefPtr<PipelineLayout> object = new PipelineLayout(this, id);
  return object.forget();
}
already_AddRefed<BindGroup> Device::CreateBindGroup(
    const dom::GPUBindGroupDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateBindGroup(mId, aDesc);
  RefPtr<BindGroup> object = new BindGroup(this, id);
  return object.forget();
}

already_AddRefed<ShaderModule> Device::CreateShaderModule(
    const dom::GPUShaderModuleDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateShaderModule(mId, aDesc);
  RefPtr<ShaderModule> object = new ShaderModule(this, id);
  return object.forget();
}

already_AddRefed<ComputePipeline> Device::CreateComputePipeline(
    const dom::GPUComputePipelineDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateComputePipeline(mId, aDesc);
  RefPtr<ComputePipeline> object = new ComputePipeline(this, id);
  return object.forget();
}

already_AddRefed<RenderPipeline> Device::CreateRenderPipeline(
    const dom::GPURenderPipelineDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateRenderPipeline(mId, aDesc);
  RefPtr<RenderPipeline> object = new RenderPipeline(this, id);
  return object.forget();
}

already_AddRefed<Texture> Device::InitSwapChain(
    const dom::GPUSwapChainDescriptor& aDesc,
    const dom::GPUExtent3DDict& aExtent3D, wr::ExternalImageId aExternalImageId,
    gfx::SurfaceFormat aFormat) {
  const layers::RGBDescriptor rgbDesc(
      gfx::IntSize(AssertedCast<int>(aExtent3D.mWidth),
                   AssertedCast<int>(aExtent3D.mHeight)),
      aFormat, false);
  // buffer count doesn't matter much, will be created on demand
  const size_t maxBufferCount = 10;
  mBridge->DeviceCreateSwapChain(mId, rgbDesc, maxBufferCount,
                                 aExternalImageId);

  dom::GPUTextureDescriptor desc;
  desc.mDimension = dom::GPUTextureDimension::_2d;
  desc.mSize.SetAsGPUExtent3DDict() = aExtent3D;
  desc.mFormat = aDesc.mFormat;
  desc.mArrayLayerCount = 1;
  desc.mMipLevelCount = 1;
  desc.mSampleCount = 1;
  desc.mUsage = aDesc.mUsage | dom::GPUTextureUsage_Binding::COPY_SRC;
  return CreateTexture(desc);
}

}  // namespace webgpu
}  // namespace mozilla
