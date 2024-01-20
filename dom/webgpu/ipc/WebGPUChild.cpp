/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUChild.h"

#include "js/RootingAPI.h"
#include "js/String.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/Warnings.h"  // JS::WarnUTF8
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/GPUUncapturedErrorEvent.h"
#include "mozilla/webgpu/ValidationError.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "Adapter.h"
#include "DeviceLostInfo.h"
#include "PipelineLayout.h"
#include "Sampler.h"
#include "CompilationInfo.h"
#include "mozilla/ipc/RawShmem.h"
#include "Utility.h"

#include <utility>

namespace mozilla::webgpu {

NS_IMPL_CYCLE_COLLECTION(WebGPUChild)

void WebGPUChild::JsWarning(nsIGlobalObject* aGlobal,
                            const nsACString& aMessage) {
  const auto& flatString = PromiseFlatCString(aMessage);
  if (aGlobal) {
    dom::AutoJSAPI api;
    if (api.Init(aGlobal)) {
      JS::WarnUTF8(api.cx(), "%s", flatString.get());
    }
  } else {
    printf_stderr("Validation error without device target: %s\n",
                  flatString.get());
  }
}

static UniquePtr<ffi::WGPUClient> initialize() {
  ffi::WGPUInfrastructure infra = ffi::wgpu_client_new();
  return UniquePtr<ffi::WGPUClient>{infra.client};
}

WebGPUChild::WebGPUChild() : mClient(initialize()) {}

WebGPUChild::~WebGPUChild() = default;

RefPtr<AdapterPromise> WebGPUChild::InstanceRequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions) {
  const int max_ids = 10;
  RawId ids[max_ids] = {0};
  unsigned long count =
      ffi::wgpu_client_make_adapter_ids(mClient.get(), ids, max_ids);

  nsTArray<RawId> sharedIds(count);
  for (unsigned long i = 0; i != count; ++i) {
    sharedIds.AppendElement(ids[i]);
  }

  return SendInstanceRequestAdapter(aOptions, sharedIds)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [](ipc::ByteBuf&& aInfoBuf) {
            // Ideally, we'd just send an empty ByteBuf, but the IPC code
            // complains if the capacity is zero...
            // So for the case where an adapter wasn't found, we just
            // transfer a single 0u64 in this buffer.
            return aInfoBuf.mLen > sizeof(uint64_t)
                       ? AdapterPromise::CreateAndResolve(std::move(aInfoBuf),
                                                          __func__)
                       : AdapterPromise::CreateAndReject(Nothing(), __func__);
          },
          [](const ipc::ResponseRejectReason& aReason) {
            return AdapterPromise::CreateAndReject(Some(aReason), __func__);
          });
}

Maybe<DeviceRequest> WebGPUChild::AdapterRequestDevice(
    RawId aSelfId, const ffi::WGPUDeviceDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_device_id(mClient.get(), aSelfId);

  ByteBuf bb;
  ffi::wgpu_client_serialize_device_descriptor(&aDesc, ToFFI(&bb));

  DeviceRequest request;
  request.mId = id;
  request.mPromise = SendAdapterRequestDevice(aSelfId, std::move(bb), id);

  return Some(std::move(request));
}

RawId WebGPUChild::CommandEncoderFinish(
    RawId aSelfId, RawId aDeviceId,
    const dom::GPUCommandBufferDescriptor& aDesc) {
  // We rely on knowledge that `CommandEncoderId` == `CommandBufferId`
  // TODO: refactor this to truly behave as if the encoder is being finished,
  // and a new command buffer ID is being created from it. Resolve the ID
  // type aliasing at the place that introduces it: `wgpu-core`.

  if (!IsOpen()) {
    // The GPU process went down. This will be handled elsewhere so pretend
    // the message was sent successfully. There is nothing we can do with
    // the produced handle so all we need is to make sure it isn't invalid
    // (it must not be zero).
    return aSelfId;
  }

  if (!SendCommandEncoderFinish(aSelfId, aDeviceId, aDesc)) {
    MOZ_CRASH("IPC failure");
  }
  return aSelfId;
}

RawId WebGPUChild::RenderBundleEncoderFinish(
    ffi::WGPURenderBundleEncoder& aEncoder, RawId aDeviceId,
    const dom::GPURenderBundleDescriptor& aDesc) {
  ffi::WGPURenderBundleDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_render_bundle(
      mClient.get(), &aEncoder, aDeviceId, &desc, ToFFI(&bb));

  if (!SendDeviceAction(aDeviceId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }

  return id;
}

RawId WebGPUChild::RenderBundleEncoderFinishError(RawId aDeviceId,
                                                  const nsString& aLabel) {
  webgpu::StringHelper label(aLabel);

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_render_bundle_error(
      mClient.get(), aDeviceId, label.Get(), ToFFI(&bb));

  if (!SendDeviceAction(aDeviceId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }

  return id;
}

RawId WebGPUChild::DeviceCreateComputePipelineImpl(
    PipelineCreationContext* const aContext,
    const dom::GPUComputePipelineDescriptor& aDesc, ByteBuf* const aByteBuf) {
  ffi::WGPUComputePipelineDescriptor desc = {};
  nsCString entryPoint;

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  if (aDesc.mLayout.IsGPUAutoLayoutMode()) {
    desc.layout = 0;
  } else if (aDesc.mLayout.IsGPUPipelineLayout()) {
    desc.layout = aDesc.mLayout.GetAsGPUPipelineLayout()->mId;
  } else {
    MOZ_ASSERT_UNREACHABLE();
  }
  desc.stage.module = aDesc.mCompute.mModule->mId;
  CopyUTF16toUTF8(aDesc.mCompute.mEntryPoint, entryPoint);
  desc.stage.entry_point = entryPoint.get();

  RawId implicit_bgl_ids[WGPUMAX_BIND_GROUPS] = {};
  RawId id = ffi::wgpu_client_create_compute_pipeline(
      mClient.get(), aContext->mParentId, &desc, ToFFI(aByteBuf),
      &aContext->mImplicitPipelineLayoutId, implicit_bgl_ids);

  for (const auto& cur : implicit_bgl_ids) {
    if (!cur) break;
    aContext->mImplicitBindGroupLayoutIds.AppendElement(cur);
  }

  return id;
}

RawId WebGPUChild::DeviceCreateComputePipeline(
    PipelineCreationContext* const aContext,
    const dom::GPUComputePipelineDescriptor& aDesc) {
  ByteBuf bb;
  const RawId id = DeviceCreateComputePipelineImpl(aContext, aDesc, &bb);

  if (!SendDeviceAction(aContext->mParentId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RefPtr<PipelinePromise> WebGPUChild::DeviceCreateComputePipelineAsync(
    PipelineCreationContext* const aContext,
    const dom::GPUComputePipelineDescriptor& aDesc) {
  ByteBuf bb;
  const RawId id = DeviceCreateComputePipelineImpl(aContext, aDesc, &bb);

  return SendDeviceActionWithAck(aContext->mParentId, std::move(bb))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [id](bool aDummy) {
            Unused << aDummy;
            return PipelinePromise::CreateAndResolve(id, __func__);
          },
          [](const ipc::ResponseRejectReason& aReason) {
            return PipelinePromise::CreateAndReject(aReason, __func__);
          });
}

static ffi::WGPUMultisampleState ConvertMultisampleState(
    const dom::GPUMultisampleState& aDesc) {
  ffi::WGPUMultisampleState desc = {};
  desc.count = aDesc.mCount;
  desc.mask = aDesc.mMask;
  desc.alpha_to_coverage_enabled = aDesc.mAlphaToCoverageEnabled;
  return desc;
}

static ffi::WGPUBlendComponent ConvertBlendComponent(
    const dom::GPUBlendComponent& aDesc) {
  ffi::WGPUBlendComponent desc = {};
  desc.src_factor = ffi::WGPUBlendFactor(aDesc.mSrcFactor);
  desc.dst_factor = ffi::WGPUBlendFactor(aDesc.mDstFactor);
  desc.operation = ffi::WGPUBlendOperation(aDesc.mOperation);
  return desc;
}

static ffi::WGPUStencilFaceState ConvertStencilFaceState(
    const dom::GPUStencilFaceState& aDesc) {
  ffi::WGPUStencilFaceState desc = {};
  desc.compare = ConvertCompareFunction(aDesc.mCompare);
  desc.fail_op = ffi::WGPUStencilOperation(aDesc.mFailOp);
  desc.depth_fail_op = ffi::WGPUStencilOperation(aDesc.mDepthFailOp);
  desc.pass_op = ffi::WGPUStencilOperation(aDesc.mPassOp);
  return desc;
}

static ffi::WGPUDepthStencilState ConvertDepthStencilState(
    const dom::GPUDepthStencilState& aDesc) {
  ffi::WGPUDepthStencilState desc = {};
  desc.format = ConvertTextureFormat(aDesc.mFormat);
  desc.depth_write_enabled = aDesc.mDepthWriteEnabled;
  desc.depth_compare = ConvertCompareFunction(aDesc.mDepthCompare);
  desc.stencil.front = ConvertStencilFaceState(aDesc.mStencilFront);
  desc.stencil.back = ConvertStencilFaceState(aDesc.mStencilBack);
  desc.stencil.read_mask = aDesc.mStencilReadMask;
  desc.stencil.write_mask = aDesc.mStencilWriteMask;
  desc.bias.constant = aDesc.mDepthBias;
  desc.bias.slope_scale = aDesc.mDepthBiasSlopeScale;
  desc.bias.clamp = aDesc.mDepthBiasClamp;
  return desc;
}

RawId WebGPUChild::DeviceCreateRenderPipelineImpl(
    PipelineCreationContext* const aContext,
    const dom::GPURenderPipelineDescriptor& aDesc, ByteBuf* const aByteBuf) {
  // A bunch of stack locals that we can have pointers into
  nsTArray<ffi::WGPUVertexBufferLayout> vertexBuffers;
  nsTArray<ffi::WGPUVertexAttribute> vertexAttributes;
  ffi::WGPURenderPipelineDescriptor desc = {};
  nsCString vsEntry, fsEntry;
  ffi::WGPUIndexFormat stripIndexFormat = ffi::WGPUIndexFormat_Uint16;
  ffi::WGPUFace cullFace = ffi::WGPUFace_Front;
  ffi::WGPUVertexState vertexState = {};
  ffi::WGPUFragmentState fragmentState = {};
  nsTArray<ffi::WGPUColorTargetState> colorStates;
  nsTArray<ffi::WGPUBlendState> blendStates;

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  if (aDesc.mLayout.IsGPUAutoLayoutMode()) {
    desc.layout = 0;
  } else if (aDesc.mLayout.IsGPUPipelineLayout()) {
    desc.layout = aDesc.mLayout.GetAsGPUPipelineLayout()->mId;
  } else {
    MOZ_ASSERT_UNREACHABLE();
  }

  {
    const auto& stage = aDesc.mVertex;
    vertexState.stage.module = stage.mModule->mId;
    CopyUTF16toUTF8(stage.mEntryPoint, vsEntry);
    vertexState.stage.entry_point = vsEntry.get();

    for (const auto& vertex_desc : stage.mBuffers) {
      ffi::WGPUVertexBufferLayout vb_desc = {};
      if (!vertex_desc.IsNull()) {
        const auto& vd = vertex_desc.Value();
        vb_desc.array_stride = vd.mArrayStride;
        vb_desc.step_mode = ffi::WGPUVertexStepMode(vd.mStepMode);
        // Note: we are setting the length but not the pointer
        vb_desc.attributes_length = vd.mAttributes.Length();
        for (const auto& vat : vd.mAttributes) {
          ffi::WGPUVertexAttribute ad = {};
          ad.offset = vat.mOffset;
          ad.format = ffi::WGPUVertexFormat(vat.mFormat);
          ad.shader_location = vat.mShaderLocation;
          vertexAttributes.AppendElement(ad);
        }
      }
      vertexBuffers.AppendElement(vb_desc);
    }
    // Now patch up all the pointers to attribute lists.
    size_t numAttributes = 0;
    for (auto& vb_desc : vertexBuffers) {
      vb_desc.attributes = vertexAttributes.Elements() + numAttributes;
      numAttributes += vb_desc.attributes_length;
    }

    vertexState.buffers = vertexBuffers.Elements();
    vertexState.buffers_length = vertexBuffers.Length();
    desc.vertex = &vertexState;
  }

  if (aDesc.mFragment.WasPassed()) {
    const auto& stage = aDesc.mFragment.Value();
    fragmentState.stage.module = stage.mModule->mId;
    CopyUTF16toUTF8(stage.mEntryPoint, fsEntry);
    fragmentState.stage.entry_point = fsEntry.get();

    // Note: we pre-collect the blend states into a different array
    // so that we can have non-stale pointers into it.
    for (const auto& colorState : stage.mTargets) {
      ffi::WGPUColorTargetState desc = {};
      desc.format = ConvertTextureFormat(colorState.mFormat);
      desc.write_mask = colorState.mWriteMask;
      colorStates.AppendElement(desc);
      ffi::WGPUBlendState bs = {};
      if (colorState.mBlend.WasPassed()) {
        const auto& blend = colorState.mBlend.Value();
        bs.alpha = ConvertBlendComponent(blend.mAlpha);
        bs.color = ConvertBlendComponent(blend.mColor);
      }
      blendStates.AppendElement(bs);
    }
    for (size_t i = 0; i < colorStates.Length(); ++i) {
      if (stage.mTargets[i].mBlend.WasPassed()) {
        colorStates[i].blend = &blendStates[i];
      }
    }

    fragmentState.targets = colorStates.Elements();
    fragmentState.targets_length = colorStates.Length();
    desc.fragment = &fragmentState;
  }

  {
    const auto& prim = aDesc.mPrimitive;
    desc.primitive.topology = ffi::WGPUPrimitiveTopology(prim.mTopology);
    if (prim.mStripIndexFormat.WasPassed()) {
      stripIndexFormat = ffi::WGPUIndexFormat(prim.mStripIndexFormat.Value());
      desc.primitive.strip_index_format = &stripIndexFormat;
    }
    desc.primitive.front_face = ffi::WGPUFrontFace(prim.mFrontFace);
    if (prim.mCullMode != dom::GPUCullMode::None) {
      cullFace = prim.mCullMode == dom::GPUCullMode::Front ? ffi::WGPUFace_Front
                                                           : ffi::WGPUFace_Back;
      desc.primitive.cull_mode = &cullFace;
    }
    desc.primitive.unclipped_depth = prim.mUnclippedDepth;
  }
  desc.multisample = ConvertMultisampleState(aDesc.mMultisample);

  ffi::WGPUDepthStencilState depthStencilState = {};
  if (aDesc.mDepthStencil.WasPassed()) {
    depthStencilState = ConvertDepthStencilState(aDesc.mDepthStencil.Value());
    desc.depth_stencil = &depthStencilState;
  }

  RawId implicit_bgl_ids[WGPUMAX_BIND_GROUPS] = {};
  RawId id = ffi::wgpu_client_create_render_pipeline(
      mClient.get(), aContext->mParentId, &desc, ToFFI(aByteBuf),
      &aContext->mImplicitPipelineLayoutId, implicit_bgl_ids);

  for (const auto& cur : implicit_bgl_ids) {
    if (!cur) break;
    aContext->mImplicitBindGroupLayoutIds.AppendElement(cur);
  }

  return id;
}

RawId WebGPUChild::DeviceCreateRenderPipeline(
    PipelineCreationContext* const aContext,
    const dom::GPURenderPipelineDescriptor& aDesc) {
  ByteBuf bb;
  const RawId id = DeviceCreateRenderPipelineImpl(aContext, aDesc, &bb);

  if (!SendDeviceAction(aContext->mParentId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RefPtr<PipelinePromise> WebGPUChild::DeviceCreateRenderPipelineAsync(
    PipelineCreationContext* const aContext,
    const dom::GPURenderPipelineDescriptor& aDesc) {
  ByteBuf bb;
  const RawId id = DeviceCreateRenderPipelineImpl(aContext, aDesc, &bb);

  return SendDeviceActionWithAck(aContext->mParentId, std::move(bb))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [id](bool aDummy) {
            Unused << aDummy;
            return PipelinePromise::CreateAndResolve(id, __func__);
          },
          [](const ipc::ResponseRejectReason& aReason) {
            return PipelinePromise::CreateAndReject(aReason, __func__);
          });
}

ipc::IPCResult WebGPUChild::RecvUncapturedError(const Maybe<RawId> aDeviceId,
                                                const nsACString& aMessage) {
  RefPtr<Device> device;
  if (aDeviceId) {
    const auto itr = mDeviceMap.find(*aDeviceId);
    if (itr != mDeviceMap.end()) {
      device = itr->second.get();
      MOZ_ASSERT(device);
    }
  }
  if (!device) {
    JsWarning(nullptr, aMessage);
  } else {
    // We don't want to spam the errors to the console indefinitely
    if (device->CheckNewWarning(aMessage)) {
      JsWarning(device->GetOwnerGlobal(), aMessage);

      dom::GPUUncapturedErrorEventInit init;
      init.mError = new ValidationError(device->GetParentObject(), aMessage);
      RefPtr<mozilla::dom::GPUUncapturedErrorEvent> event =
          dom::GPUUncapturedErrorEvent::Constructor(
              device, u"uncapturederror"_ns, init);
      device->DispatchEvent(*event);
    }
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUChild::RecvDropAction(const ipc::ByteBuf& aByteBuf) {
  const auto* byteBuf = ToFFI(&aByteBuf);
  ffi::wgpu_client_drop_action(mClient.get(), byteBuf);
  return IPC_OK();
}

ipc::IPCResult WebGPUChild::RecvDeviceLost(RawId aDeviceId,
                                           Maybe<uint8_t> aReason,
                                           const nsACString& aMessage) {
  RefPtr<Device> device;
  const auto itr = mDeviceMap.find(aDeviceId);
  if (itr != mDeviceMap.end()) {
    device = itr->second.get();
    MOZ_ASSERT(device);
  }

  if (device) {
    auto message = NS_ConvertUTF8toUTF16(aMessage);
    if (aReason.isSome()) {
      dom::GPUDeviceLostReason reason =
          static_cast<dom::GPUDeviceLostReason>(*aReason);
      device->ResolveLost(Some(reason), message);
    } else {
      device->ResolveLost(Nothing(), message);
    }
  }
  return IPC_OK();
}

void WebGPUChild::DeviceCreateSwapChain(
    RawId aSelfId, const RGBDescriptor& aRgbDesc, size_t maxBufferCount,
    const layers::RemoteTextureOwnerId& aOwnerId,
    bool aUseExternalTextureInSwapChain) {
  RawId queueId = aSelfId;  // TODO: multiple queues
  nsTArray<RawId> bufferIds(maxBufferCount);
  for (size_t i = 0; i < maxBufferCount; ++i) {
    bufferIds.AppendElement(
        ffi::wgpu_client_make_buffer_id(mClient.get(), aSelfId));
  }
  SendDeviceCreateSwapChain(aSelfId, queueId, aRgbDesc, bufferIds, aOwnerId,
                            aUseExternalTextureInSwapChain);
}

void WebGPUChild::QueueOnSubmittedWorkDone(
    const RawId aSelfId, const RefPtr<dom::Promise>& aPromise) {
  SendQueueOnSubmittedWorkDone(aSelfId)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aPromise]() { aPromise->MaybeResolveWithUndefined(); },
      [aPromise](const ipc::ResponseRejectReason& aReason) {
        aPromise->MaybeRejectWithNotSupportedError("IPC error");
      });
}

void WebGPUChild::SwapChainPresent(RawId aTextureId,
                                   const RemoteTextureId& aRemoteTextureId,
                                   const RemoteTextureOwnerId& aOwnerId) {
  // Hack: the function expects `DeviceId`, but it only uses it for `backend()`
  // selection.
  RawId encoderId = ffi::wgpu_client_make_encoder_id(mClient.get(), aTextureId);
  SendSwapChainPresent(aTextureId, encoderId, aRemoteTextureId, aOwnerId);
}

void WebGPUChild::RegisterDevice(Device* const aDevice) {
  mDeviceMap.insert({aDevice->mId, aDevice});
}

void WebGPUChild::UnregisterDevice(RawId aId) {
  mDeviceMap.erase(aId);
  if (IsOpen()) {
    SendDeviceDrop(aId);
  }
}

void WebGPUChild::FreeUnregisteredInParentDevice(RawId aId) {
  ffi::wgpu_client_kill_device_id(mClient.get(), aId);
  mDeviceMap.erase(aId);
}

void WebGPUChild::ActorDestroy(ActorDestroyReason) {
  // Resolving the promise could cause us to update the original map if the
  // callee frees the Device objects immediately. Since any remaining entries
  // in the map are no longer valid, we can just move the map onto the stack.
  const auto deviceMap = std::move(mDeviceMap);
  mDeviceMap.clear();

  for (const auto& targetIter : deviceMap) {
    RefPtr<Device> device = targetIter.second.get();
    if (!device) {
      // The Device may have gotten freed when we resolved the Promise for
      // another Device in the map.
      continue;
    }

    device->ResolveLost(Nothing(), u"WebGPUChild destroyed"_ns);
  }
}

}  // namespace mozilla::webgpu
