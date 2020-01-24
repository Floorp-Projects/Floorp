/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUParent.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla {
namespace webgpu {

const uint64_t POLL_TIME_MS = 100;

WebGPUParent::WebGPUParent() : mContext(ffi::wgpu_server_new()) {
  mTimer.Start(base::TimeDelta::FromMilliseconds(POLL_TIME_MS), this,
               &WebGPUParent::MaintainDevices);
}

WebGPUParent::~WebGPUParent() = default;

void WebGPUParent::MaintainDevices() {
  ffi::wgpu_server_poll_all_devices(mContext, false);
}

ipc::IPCResult WebGPUParent::RecvInstanceRequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions,
    const nsTArray<RawId>& aTargetIds,
    InstanceRequestAdapterResolver&& resolver) {
  ffi::WGPURequestAdapterOptions options = {};
  if (aOptions.mPowerPreference.WasPassed()) {
    options.power_preference = static_cast<ffi::WGPUPowerPreference>(
        aOptions.mPowerPreference.Value());
  }
  // TODO: make available backends configurable by prefs

  int8_t index = ffi::wgpu_server_instance_request_adapter(
      mContext, &options, aTargetIds.Elements(), aTargetIds.Length());
  if (index >= 0) {
    resolver(aTargetIds[index]);
  } else {
    resolver(0);
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvAdapterRequestDevice(
    RawId aSelfId, const dom::GPUDeviceDescriptor& aDesc, RawId aNewId) {
  ffi::WGPUDeviceDescriptor desc = {};
  Unused << aDesc;  // no useful fields
  // TODO: fill up the descriptor
  ffi::wgpu_server_adapter_request_device(mContext, aSelfId, &desc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvAdapterDestroy(RawId aSelfId) {
  ffi::wgpu_server_adapter_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceDestroy(RawId aSelfId) {
  ffi::wgpu_server_device_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateBuffer(
    RawId aSelfId, const dom::GPUBufferDescriptor& aDesc, RawId aNewId) {
  ffi::WGPUBufferDescriptor desc = {};
  desc.usage = aDesc.mUsage;
  desc.size = aDesc.mSize;
  ffi::wgpu_server_device_create_buffer(mContext, aSelfId, &desc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceUnmapBuffer(RawId aSelfId,
                                                   RawId aBufferId,
                                                   Shmem&& shmem) {
  ffi::wgpu_server_device_set_buffer_sub_data(mContext, aSelfId, aBufferId, 0,
                                              shmem.get<uint8_t>(),
                                              shmem.Size<uint8_t>());
  return IPC_OK();
}

struct MapReadRequest {
  ipc::Shmem mShmem;
  WebGPUParent::BufferMapReadResolver mResolver;
  MapReadRequest(ipc::Shmem&& shmem,
                 WebGPUParent::BufferMapReadResolver&& resolver)
      : mShmem(shmem), mResolver(resolver) {}
};

void MapReadCallback(ffi::WGPUBufferMapAsyncStatus status, const uint8_t* ptr,
                     uint8_t* userdata) {
  auto req = reinterpret_cast<MapReadRequest*>(userdata);
  // TODO: better handle errors
  MOZ_ASSERT(status == ffi::WGPUBufferMapAsyncStatus_Success);
  memcpy(req->mShmem.get<uint8_t>(), ptr, req->mShmem.Size<uint8_t>());
  req->mResolver(std::move(req->mShmem));
  delete req;
}

ipc::IPCResult WebGPUParent::RecvBufferMapRead(
    RawId aSelfId, Shmem&& shmem, BufferMapReadResolver&& resolver) {
  auto size = shmem.Size<uint8_t>();
  auto request = new MapReadRequest(std::move(shmem), std::move(resolver));
  ffi::wgpu_server_buffer_map_read(mContext, aSelfId, 0, size, &MapReadCallback,
                                   reinterpret_cast<uint8_t*>(request));
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBufferDestroy(RawId aSelfId) {
  ffi::wgpu_server_buffer_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateCommandEncoder(
    RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc,
    RawId aNewId) {
  Unused << aDesc;  // no useful fields
  ffi::WGPUCommandEncoderDescriptor desc = {};
  ffi::wgpu_server_device_create_encoder(mContext, aSelfId, &desc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderRunComputePass(RawId aSelfId,
                                                              Shmem&& shmem) {
  ffi::wgpu_server_encode_compute_pass(mContext, aSelfId, shmem.get<uint8_t>(),
                                       shmem.Size<uint8_t>());
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderFinish(
    RawId aSelfId, const dom::GPUCommandBufferDescriptor& aDesc) {
  Unused << aDesc;
  ffi::WGPUCommandBufferDescriptor desc = {};
  ffi::wgpu_server_encoder_finish(mContext, aSelfId, &desc);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderDestroy(RawId aSelfId) {
  ffi::wgpu_server_encoder_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandBufferDestroy(RawId aSelfId) {
  ffi::wgpu_server_command_buffer_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvQueueSubmit(
    RawId aSelfId, const nsTArray<RawId>& aCommandBuffers) {
  ffi::wgpu_server_queue_submit(mContext, aSelfId, aCommandBuffers.Elements(),
                                aCommandBuffers.Length());
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvShutdown() {
  mTimer.Stop();
  ffi::wgpu_server_poll_all_devices(mContext, true);
  ffi::wgpu_server_delete(const_cast<ffi::WGPUGlobal*>(mContext));
  return IPC_OK();
}

}  // namespace webgpu
}  // namespace mozilla
