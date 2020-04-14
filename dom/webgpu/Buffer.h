/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_BUFFER_H_
#define GPU_BUFFER_H_

#include "js/RootingAPI.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "ObjectModel.h"

namespace mozilla {
namespace ipc {
class Shmem;
}  // namespace ipc
namespace webgpu {

class Device;

class Buffer final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Buffer)
  GPU_DECL_JS_WRAP(Buffer)

  struct Mapping final {
    UniquePtr<ipc::Shmem> mShmem;
    JS::Heap<JSObject*> mArrayBuffer;
    const bool mWrite;

    Mapping(ipc::Shmem&& aShmem, JSObject* aArrayBuffer, bool aWrite);
  };

  Buffer(Device* const aParent, RawId aId, BufferAddress aSize);
  void InitMapping(ipc::Shmem&& aShmem, JSObject* aArrayBuffer, bool aWrite);

  const RawId mId;

 private:
  virtual ~Buffer();
  void Cleanup();

  // Note: we can't map a buffer with the size that don't fit into `size_t`
  // (which may be smaller than `BufferAddress`), but general not all buffers
  // are mapped.
  const BufferAddress mSize;
  nsString mLabel;
  Maybe<Mapping> mMapping;

 public:
  already_AddRefed<dom::Promise> MapReadAsync(ErrorResult& aRv);
  void Unmap(JSContext* aCx, ErrorResult& aRv);
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_BUFFER_H_
