/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Shmem_h
#define mozilla_ipc_Shmem_h

#include "mozilla/Attributes.h"

#include "base/basictypes.h"
#include "base/process.h"

#include "nscore.h"
#include "nsDebug.h"

#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/Range.h"
#include "mozilla/UniquePtr.h"

namespace IPC {
template <typename T>
struct ParamTraits;
}

namespace mozilla::ipc {

// A `Shmem` is a wrapper around OS-level shared memory. It comes in two major
// modes: safe and unsafe.
//
// Access to the memory within a "safe" shmem is conceptually transferred to the
// remote process when the shmem is transferred by marking the mapped memory in
// the sending process as inaccessable (debug-mode only). Code should not
// attempt to access the shmem after it has been transferred, though it will
// remain mapped until all other Shmem references have been dropped.
//
// Access to the memory within an "unsafe" shmem is not protected in the same
// way, and can be shared between multiple processes.
//
// For now, the primary way to create a `Shmem` is to call the
// `IProtocol::AllocShmem` or `IProtocol::AllocUnsafeShmem` methods on an IPDL
// actor, however this requirement may be relaxed in the future.
class Shmem final {
  friend struct IPC::ParamTraits<mozilla::ipc::Shmem>;

 public:
  // Low-level wrapper around platform shmem primitives.
  typedef mozilla::ipc::SharedMemory SharedMemory;
  typedef SharedMemory::SharedMemoryType SharedMemoryType;

  Shmem() = default;

  // Allocates a brand new shared memory region with sufficient size for
  // `aNBytes` bytes. This region may be transferred to other processes by being
  // sent over an IPDL actor.
  Shmem(size_t aNBytes, SharedMemoryType aType, bool aUnsafe);

  // Create a reference to an existing shared memory region. The segment must be
  // large enough for `aNBytes`.
  Shmem(RefPtr<SharedMemory> aSegment, size_t aNBytes, bool aUnsafe);

  // NOTE: Some callers are broken if a move constructor is provided here.
  Shmem(const Shmem& aOther) = default;
  Shmem& operator=(const Shmem& aRhs) = default;

  bool operator==(const Shmem& aRhs) const { return mSegment == aRhs.mSegment; }
  bool operator!=(const Shmem& aRhs) const { return mSegment != aRhs.mSegment; }

  // Returns whether this Shmem is valid, and contains a reference to internal
  // state.
  bool IsValid() const { return mSegment != nullptr; }

  // Legacy names for `IsValid()` - do _NOT_ actually confirm whether or not you
  // should be able to write to or read from this type.
  bool IsWritable() const { return IsValid(); }
  bool IsReadable() const { return IsValid(); }

  // Return a pointer to the user-visible data segment.
  template <typename T>
  T* get() const {
    AssertInvariants();
    AssertAligned<T>();

    return reinterpret_cast<T*>(mData);
  }

  // Return the size of the segment as requested when this shmem
  // segment was allocated, in units of T.  The underlying mapping may
  // actually be larger because of page alignment and private data,
  // but this isn't exposed to clients.
  template <typename T>
  size_t Size() const {
    AssertInvariants();
    AssertAligned<T>();

    return mSize / sizeof(T);
  }

  template <typename T>
  Range<T> Range() const {
    return {get<T>(), Size<T>()};
  }

  // For safe shmems in debug mode, immediately revoke all access rights to the
  // memory when deallocating it.
  // Also resets this particular `Shmem` instance to an invalid state.
#ifndef DEBUG
  void RevokeRights() { *this = Shmem(); }
#else
  void RevokeRights();
#endif

 private:
  template <typename T>
  void AssertAligned() const {
    MOZ_RELEASE_ASSERT(0 == (mSize % sizeof(T)),
                       "shmem size is not a multiple of sizeof(T)");
  }

#ifndef DEBUG
  void AssertInvariants() const {}
#else
  void AssertInvariants() const;
#endif

  RefPtr<SharedMemory> mSegment;
  void* mData = nullptr;
  size_t mSize = 0;
#ifdef DEBUG
  bool mUnsafe = false;
#endif
};

}  // namespace mozilla::ipc

#endif  // ifndef mozilla_ipc_Shmem_h
