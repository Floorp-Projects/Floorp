/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_PARENT_H_
#define WEBGPU_PARENT_H_

#include "mozilla/webgpu/PWebGPUParent.h"
#include "WebGPUTypes.h"
#include "base/timer.h"

namespace mozilla {
namespace webgpu {
namespace ffi {
struct WGPUGlobal;
}  // namespace ffi

class WebGPUParent final : public PWebGPUParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebGPUParent)

 public:
  explicit WebGPUParent();

  ipc::IPCResult RecvInstanceRequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions,
      const nsTArray<RawId>& aTargetIds,
      InstanceRequestAdapterResolver&& resolver);
  ipc::IPCResult RecvAdapterRequestDevice(RawId aSelfId,
                                          const dom::GPUDeviceDescriptor& aDesc,
                                          RawId aNewId);
  ipc::IPCResult RecvAdapterDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateBuffer(RawId aSelfId,
                                        const dom::GPUBufferDescriptor& aDesc,
                                        RawId aNewId);
  ipc::IPCResult RecvDeviceUnmapBuffer(RawId aSelfId, RawId aBufferId,
                                       Shmem&& shmem);
  ipc::IPCResult RecvBufferMapRead(RawId aSelfId, Shmem&& shmem,
                                   BufferMapReadResolver&& resolver);
  ipc::IPCResult RecvBufferDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateCommandEncoder(
      RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc,
      RawId aNewId);
  ipc::IPCResult RecvCommandEncoderRunComputePass(RawId aSelfId, Shmem&& shmem);
  ipc::IPCResult RecvCommandEncoderFinish(
      RawId aSelfId, const dom::GPUCommandBufferDescriptor& aDesc);
  ipc::IPCResult RecvCommandEncoderDestroy(RawId aSelfId);
  ipc::IPCResult RecvCommandBufferDestroy(RawId aSelfId);
  ipc::IPCResult RecvQueueSubmit(RawId aSelfId,
                                 const nsTArray<RawId>& aCommandBuffers);
  ipc::IPCResult RecvShutdown();

 private:
  virtual ~WebGPUParent();
  void MaintainDevices();

  const ffi::WGPUGlobal* const mContext;
  base::RepeatingTimer<WebGPUParent> mTimer;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_PARENT_H_
