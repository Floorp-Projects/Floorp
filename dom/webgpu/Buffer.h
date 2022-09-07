/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_BUFFER_H_
#define GPU_BUFFER_H_

#include "js/RootingAPI.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsTArray.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;

namespace dom {
struct GPUBufferDescriptor;
template <typename T>
class Optional;
}  // namespace dom

namespace ipc {
class Shmem;
}  // namespace ipc
namespace webgpu {

class Device;

struct MappedInfo {
  // True if mapping is requested for writing.
  bool mWritable = false;
  // Populated by `GetMappedRange`.
  nsTArray<JS::Heap<JSObject*>> mArrayBuffers;
  BufferAddress mOffset;
  BufferAddress mSize;
  MappedInfo() = default;
  MappedInfo(const MappedInfo&) = delete;
};

class Buffer final : public ObjectBase, public ChildOf<Device> {
 public:
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(Buffer)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Buffer)
  GPU_DECL_JS_WRAP(Buffer)

  static already_AddRefed<Buffer> Create(Device* aDevice, RawId aDeviceId,
                                         const dom::GPUBufferDescriptor& aDesc,
                                         ErrorResult& aRv);

  already_AddRefed<dom::Promise> MapAsync(uint32_t aMode, uint64_t aOffset,
                                          const dom::Optional<uint64_t>& aSize,
                                          ErrorResult& aRv);
  void GetMappedRange(JSContext* aCx, uint64_t aOffset,
                      const dom::Optional<uint64_t>& aSize,
                      JS::Rooted<JSObject*>* aObject, ErrorResult& aRv);
  void Unmap(JSContext* aCx, ErrorResult& aRv);
  void Destroy(JSContext* aCx, ErrorResult& aRv);

  const RawId mId;

 private:
  Buffer(Device* const aParent, RawId aId, BufferAddress aSize, uint32_t aUsage,
         ipc::Shmem&& aShmem);
  virtual ~Buffer();
  Device& GetDevice() { return *mParent; }
  void Drop();
  void UnmapArrayBuffers(JSContext* aCx, ErrorResult& aRv);
  void RejectMapRequest(dom::Promise* aPromise, nsACString& message);
  void AbortMapRequest();
  void SetMapped(BufferAddress aOffset, BufferAddress aSize, bool aWritable);

  // Note: we can't map a buffer with the size that don't fit into `size_t`
  // (which may be smaller than `BufferAddress`), but general not all buffers
  // are mapped.
  const BufferAddress mSize;
  const uint32_t mUsage;
  nsString mLabel;
  // Information about the currently active mapping.
  Maybe<MappedInfo> mMapped;
  RefPtr<dom::Promise> mMapRequest;
  // mShmem does not point to a shared memory segment if the buffer is not
  // mappable.
  ipc::Shmem mShmem;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_BUFFER_H_
