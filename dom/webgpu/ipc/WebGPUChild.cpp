/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUChild.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTION(WebGPUChild)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGPUChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGPUChild, Release)

static ffi::WGPUClient* initialize() {
  ffi::WGPUInfrastructure infra = ffi::wgpu_client_new();
  return infra.client;
}

WebGPUChild::WebGPUChild() : mClient(initialize()), mIPCOpen(false) {}

WebGPUChild::~WebGPUChild() {
  if (mClient) {
    ffi::wgpu_client_delete(mClient);
  }
}

RefPtr<RawIdPromise> WebGPUChild::InstanceRequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions) {
  const int max_ids = 10;
  RawId ids[max_ids] = {0};
  unsigned long count =
      ffi::wgpu_client_make_adapter_ids(mClient, ids, max_ids);

  auto client = mClient;
  nsTArray<RawId> sharedIds;
  for (unsigned long i = 0; i != count; ++i) {
    sharedIds.AppendElement(ids[i]);
  }

  return SendInstanceRequestAdapter(aOptions, sharedIds)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [client, ids, count](const RawId& aId) {
            if (aId == 0) {
              ffi::wgpu_client_kill_adapter_ids(client, ids, count);
              return RawIdPromise::CreateAndReject(Nothing(), __func__);
            } else {
              // find the position in the list
              unsigned int i = 0;
              while (ids[i] != aId) {
                i++;
              }
              if (i > 0) {
                ffi::wgpu_client_kill_adapter_ids(client, ids, i);
              }
              if (i + 1 < count) {
                ffi::wgpu_client_kill_adapter_ids(client, ids + i + 1,
                                                  count - i - 1);
              }
              return RawIdPromise::CreateAndResolve(aId, __func__);
            }
          },
          [client, ids, count](const ipc::ResponseRejectReason& aReason) {
            ffi::wgpu_client_kill_adapter_ids(client, ids, count);
            return RawIdPromise::CreateAndReject(Some(aReason), __func__);
          });
}

Maybe<RawId> WebGPUChild::AdapterRequestDevice(
    RawId aSelfId, const dom::GPUDeviceDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_device_id(mClient, aSelfId);
  if (SendAdapterRequestDevice(aSelfId, aDesc, id)) {
    return Some(id);
  } else {
    ffi::wgpu_client_kill_device_id(mClient, id);
    return Nothing();
  }
}

RawId WebGPUChild::DeviceCreateBuffer(RawId aSelfId,
                                      const dom::GPUBufferDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_device_id(mClient, aSelfId);
  if (!SendDeviceCreateBuffer(aSelfId, aDesc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

}  // namespace webgpu
}  // namespace mozilla
