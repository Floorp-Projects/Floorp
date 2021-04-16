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
template <typename T>
class Optional;
}

namespace ipc {
class Shmem;
}  // namespace ipc
namespace webgpu {

class Device;

struct MappedInfo {
  ipc::Shmem mShmem;
  // True if mapping is requested for writing.
  bool mWritable = false;
  // Populated by `GetMappedRange`.
  nsTArray<JS::Heap<JSObject*>> mArrayBuffers;

  MappedInfo() = default;
  MappedInfo(const MappedInfo&) = delete;
  bool IsReady() const { return mShmem.IsReadable(); }
};

class Buffer final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Buffer)
  GPU_DECL_JS_WRAP(Buffer)

  Buffer(Device* const aParent, RawId aId, BufferAddress aSize, bool aMappable);
  void SetMapped(ipc::Shmem&& aShmem, bool aWritable);

  const RawId mId;

 private:
  virtual ~Buffer();
  void Cleanup();

  // Note: we can't map a buffer with the size that don't fit into `size_t`
  // (which may be smaller than `BufferAddress`), but general not all buffers
  // are mapped.
  const BufferAddress mSize;
  const bool mMappable;
  nsString mLabel;
  // Information about the currently active mapping.
  Maybe<MappedInfo> mMapped;

 public:
  already_AddRefed<dom::Promise> MapAsync(uint32_t aMode, uint64_t aOffset,
                                          const dom::Optional<uint64_t>& aSize,
                                          ErrorResult& aRv);
  void GetMappedRange(JSContext* aCx, uint64_t aOffset,
                      const dom::Optional<uint64_t>& aSize,
                      JS::Rooted<JSObject*>* aObject, ErrorResult& aRv);
  void Unmap(JSContext* aCx, ErrorResult& aRv);
  void Destroy();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_BUFFER_H_
