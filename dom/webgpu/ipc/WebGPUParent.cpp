/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUParent.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla {
namespace webgpu {

WebGPUParent::WebGPUParent() : mContext(ffi::wgpu_server_new()) {}

WebGPUParent::~WebGPUParent() = default;

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
    RawId aSelfId, const dom::GPUDeviceDescriptor& aOptions, RawId aNewId) {
  ffi::WGPUDeviceDescriptor desc = {};
  // TODO: fill up the descriptor

  ffi::wgpu_server_adapter_request_device(mContext, aSelfId, &desc, aNewId);
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

ipc::IPCResult WebGPUParent::RecvDeviceMapBufferRead(
    RawId aSelfId, RawId aBufferId, Shmem&& shmem,
    DeviceMapBufferReadResolver&& resolver) {
  ffi::wgpu_server_device_get_buffer_sub_data(mContext, aSelfId, aBufferId, 0,
                                              shmem.get<uint8_t>(),
                                              shmem.Size<uint8_t>());
  resolver(std::move(shmem));
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

ipc::IPCResult WebGPUParent::RecvBufferDestroy(RawId aSelfId) {
  ffi::wgpu_server_buffer_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvShutdown() {
  ffi::wgpu_server_delete(const_cast<ffi::WGPUGlobal*>(mContext));
  return IPC_OK();
}

}  // namespace webgpu
}  // namespace mozilla
