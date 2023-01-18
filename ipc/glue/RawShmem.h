/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IPC_RAWSHMEM_H_
#define MOZILLA_IPC_RAWSHMEM_H_

#include "mozilla/ipc/SharedMemoryBasic.h"
#include "mozilla/Span.h"
#include <utility>

namespace mozilla::ipc {

class WritableSharedMemoryMapping;

/// A handle to shared memory.
///
/// See the doc comment for `WritableSharedMemoryMapping` below.
class UnsafeSharedMemoryHandle {
  friend class WritableSharedMemoryMapping;
  friend struct IPC::ParamTraits<UnsafeSharedMemoryHandle>;

 public:
  UnsafeSharedMemoryHandle();
  UnsafeSharedMemoryHandle(UnsafeSharedMemoryHandle&& aOther) noexcept;
  UnsafeSharedMemoryHandle& operator=(
      UnsafeSharedMemoryHandle&& aOther) noexcept;

  /// Attempts to allocate a shmem.
  ///
  /// Returns `Nothing()` if allocation fails.
  /// If `aSize` is zero, a valid empty WritableSharedMemoryMapping is returned.
  static Maybe<std::pair<UnsafeSharedMemoryHandle, WritableSharedMemoryMapping>>
  CreateAndMap(size_t aSize);

 private:
  UnsafeSharedMemoryHandle(SharedMemoryBasic::Handle&& aHandle, uint64_t aSize)
      : mHandle(std::move(aHandle)), mSize(aSize) {}

  SharedMemoryBasic::Handle mHandle;
  uint64_t mSize;
};

/// A Shared memory buffer mapping.
///
/// Unlike `ipc::Shmem`, the underlying shared memory buffer on each side of
/// the process boundary is only deallocated with there respective
/// `WritableSharedMemoryMapping`.
///
/// ## Usage
///
/// Typical usage goes as follows:
/// - Allocate the memory using `UnsafeSharedMemoryHandle::Create`, returning a
///   handle and a mapping.
/// - Send the handle to the other process using an IPDL message.
/// - On the other process, map the shared memory by creating
///   WritableSharedMemoryMapping via `WritableSharedMemoryMapping::Open` and
///   the received handle.
///
/// Do not send the shared memory handle again, it is only intended to establish
/// the mapping on each side during initialization. The user of this class is
/// responsible for managing the lifetime of the buffers on each side, as well
/// as their identity, by for example storing them in hash map and referring to
/// them via IDs in IPDL message if need be.
///
/// ## Empty shmems
///
/// An empty WritableSharedMemoryMapping is one that was created with size zero.
/// It is analogous to a null RefPtr. It can be used like a non-empty shmem,
/// including sending the handle and openning it on another process (resulting
/// in an empty mapping on the other side).
class WritableSharedMemoryMapping {
  friend class UnsafeSharedMemoryHandle;

 public:
  WritableSharedMemoryMapping() = default;

  WritableSharedMemoryMapping(WritableSharedMemoryMapping&& aMoved) = default;

  WritableSharedMemoryMapping& operator=(WritableSharedMemoryMapping&& aMoved) =
      default;

  /// Open the shmem and immediately close the handle.
  static Maybe<WritableSharedMemoryMapping> Open(
      UnsafeSharedMemoryHandle aHandle);

  // Returns the size in bytes.
  size_t Size() const;

  // Returns the shared memory as byte range.
  Span<uint8_t> Bytes();

 private:
  explicit WritableSharedMemoryMapping(
      RefPtr<mozilla::ipc::SharedMemoryBasic>&& aRef);

  RefPtr<mozilla::ipc::SharedMemoryBasic> mRef;
};

}  // namespace mozilla::ipc

namespace IPC {
template <>
struct ParamTraits<mozilla::ipc::UnsafeSharedMemoryHandle> {
  typedef mozilla::ipc::UnsafeSharedMemoryHandle paramType;
  static void Write(IPC::MessageWriter* aWriter, paramType&& aVar);
  static bool Read(IPC::MessageReader* aReader, paramType* aVar);
};

}  // namespace IPC

#endif
