/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implements a piece of cross-process shared memory that can be shared with
 * many processes simultaniously, and is synchronized by a cross-process mutex.
 *
 * In almost all cases, IPDL and its machinery should be preferred to this.
 * This class should only be used if you really know you need it, and
 * precautions need to be taken to avoid introducing security bugs.
 *
 * Specifically:
 * - Don't assume that any process other than the main process is trustworthy,
 *   since exploits may exist in graphics drivers, video codecs, etc.
 * - Don't assume the other side can't modify the shared memory even while
 *   you hold the lock.
 * - Don't assume that if you read the same piece of data twice, that it will
 *   return the same value.
 * - Don't assume the other side will ever actually release the lock.
 *   IE using RunWithLock() on the main thread is probably a bad idea.
 *
 * If possible, design things so that the trusted side is on a separate
 * thread and is write-only and you avoid those problems altogether. If you,
 * must read, consider taking a "snapshot" of the shared memory by copying it
 * to a local variable, operating on that, and then copying it back to shared
 * memory when done.
 *
 */

#ifndef DOM_GAMEPAD_SYNCHRONIZEDSHAREDMEMORY_H_
#define DOM_GAMEPAD_SYNCHRONIZEDSHAREDMEMORY_H_

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include <cinttypes>
#include <type_traits>

namespace mozilla::ipc {
class IProtocol;
}  // namespace mozilla::ipc

namespace mozilla::dom {

// IPDL structure that knows how to serialize this over IPC
class SynchronizedSharedMemoryRemoteInfo;

// Details that you don't need to know to use this class
//
// This mostly represents an interface/implementation firewall (Pimpl Idiom)
// because the implementation is per-platform and we want to keep this file
// free of platform-specific headers.
//
// It is implemented in terms of "void*" because template classes with template
// methods can't hide their implementations in .cpp files. The main class is a
// template wrapper for this that exposes it as a typesafe API for type `T`.
//
// See SynchronizedSharedMemory below for usage of the public API

class SynchronizedSharedMemoryDetail {
 public:
  static Maybe<SynchronizedSharedMemoryDetail> CreateNew(uint32_t aSize);
  ~SynchronizedSharedMemoryDetail();

  void LockMutex();
  void UnlockMutex();

  void* GetPtr() const;

  // Allows this to be transferred across IPDL
  static Maybe<SynchronizedSharedMemoryDetail> CreateFromRemoteInfo(
      const SynchronizedSharedMemoryRemoteInfo& aIPCInfo);
  bool GenerateRemoteInfo(const mozilla::ipc::IProtocol* aActor,
                          SynchronizedSharedMemoryRemoteInfo* aOut);

  // Allow move
  SynchronizedSharedMemoryDetail(SynchronizedSharedMemoryDetail&&);
  SynchronizedSharedMemoryDetail& operator=(SynchronizedSharedMemoryDetail&&);

  // Disallow copy
  SynchronizedSharedMemoryDetail(const SynchronizedSharedMemoryDetail&) =
      delete;
  SynchronizedSharedMemoryDetail& operator=(
      const SynchronizedSharedMemoryDetail&) = delete;

 private:
  class Impl;

  SynchronizedSharedMemoryDetail();
  explicit SynchronizedSharedMemoryDetail(UniquePtr<Impl> aImpl);

  UniquePtr<Impl> mImpl;

  // Testing-related helper
  template <typename T>
  friend class SynchronizedSharedMemory;
  bool GenerateTestRemoteInfo(SynchronizedSharedMemoryRemoteInfo* aOut);
};

// Represents a piece of cross-process synchronized shared memory
//
// Can create a piece of shared memory to hold an object of type T,
// pass it over IPDL to another actor, and can perform synchronized
// cross-process access of the object
//
// See top of file for warnings about security.
//
// The type, T, must be TriviallyCopyable since sharing a piece of memory
// between 2 processes is akin to memcopy-ing its bytes from one program to
// another. StandardLayout may not be strictly necessary, but conforming to the
// platform C ABI is probably a good idea when sharing across processes.
//
// Although not statically enforcable, generally the object should not contain
// pointers, since they are usually meaningless across processes.
template <typename T>
class SynchronizedSharedMemory {
  static_assert(std::is_trivially_copyable<T>::value,
                "SynchronizedSharedMemory can only be used with types that are "
                "trivially copyable");
  static_assert(std::is_standard_layout<T>::value,
                "SynchronizedSharedMemory can only be used with types that are "
                "standard layout");

 public:
  // Construct a new object of type T in cross-process shareable memory
  //
  // Returns `Nothing` if an error occurs
  //
  // # Example
  // ```
  // struct MyObject {
  //   int i;
  // };
  // auto maybeSharedMemory = SynchronizedSharedMemory<MyObject>::CreateNew();
  // MOZ_ASSERT(maybeSharedMemory);
  // ```
  template <typename... Args>
  static Maybe<SynchronizedSharedMemory> CreateNew(Args&&... args) {
    auto maybeDetail = SynchronizedSharedMemoryDetail::CreateNew(sizeof(T));
    if (!maybeDetail) {
      return Nothing{};
    }
    new ((*maybeDetail).GetPtr()) T(std::forward<Args>(args)...);
    return Some(SynchronizedSharedMemory(std::move(*maybeDetail)));
  }

  // Run the given function on the current thread while holding a lock on the
  // cross-process mutex
  //
  // Guarantees that no well-behaved users** will race or forget to unlock when
  // accessing this object, even from other processes.
  //
  // ** Of course, proper locking/unlocking can never be enforced in a
  //    compromised process. See the security warning at the top of this file
  //
  // # Example
  // ```
  // ===== In one process =====
  //
  // auto maybeSharedMemory = SynchronizedSharedMemory<int>::CreateNew();
  // maybeSharedMemory.RunWithLock([] (int* pInt) {
  //   *pInt = 5;
  // });
  //
  // // ===== In another process =====
  //
  // int curValue = 0;
  // maybeSharedMemory.RunWithLock([&] (int* pInt) {
  //   curValue = *pInt;
  // });
  //
  // MOZ_ASSERT(curValue == 5);
  // ```
  template <typename Fn, typename... Args>
  void RunWithLock(Fn&& aFn, Args&&... args) {
    mDetail.LockMutex();

    // Justification for volatile: We need to make a local copy of structure T
    // so a compromised remote process can't exploit us by ignoring the lock
    // and writing after we've performed validation and trust the data, but
    // before we actually use it to do something privileged
    //
    // Without volatile, the optimizer may be allowed to evade the copy
    // and continue to act on the original remote object (among other dangerous
    // things)
    //
    T localCopy;
    volatile_memcpy(&localCopy, mDetail.GetPtr(), sizeof(T));

    (std::forward<Fn>(aFn))(&localCopy, std::forward<Args>(args)...);

    volatile_memcpy(mDetail.GetPtr(), &localCopy, sizeof(T));

    mDetail.UnlockMutex();
  }

  // Create from info generated using GenerateRemoteInfo()
  //
  // This is the other end to GenerateRemoteInfo(). It receives the
  // serialized `SynchronizedSharedMemoryRemoteInfo` structure sent over IPDL
  // and uses it to attach to the shared object.
  //
  // Returns `Nothing` if an error occurs.
  //
  // # Example
  //  ```
  //  // ===== One side of IPDL =====
  //
  //  bool SetupSharedMemory(mozilla::ipc::PMyProtocol& aProtocol) {
  //    auto maybeRemoteInfo = mSharedMemory.GenerateRemoteInfo(&aProtocol);
  //    if (!maybeRemoteInfo) {
  //      return false;
  //    }
  //    return aProtocol.SendSharedMemoryInfo(*maybeRemoteInfo);
  //  }
  //
  //  // ===== Other side of IPDL =====
  //
  //  mozilla::ipc::IPCResult MyProtocol::RecvSharedMemoryInfo(
  //      const SynchronizedSharedMemoryRemoteInfo& aRemoteInfo) {
  //    auto maybeSharedMemory = SynchronizedSharedMemory::CreateFromRemoteInfo(
  //      aRemoteInfo);
  //    if (!maybeSharedMemory) {
  //      return IPC_FAIL_NO_REASON(this);
  //    }
  //    mSharedMemory = std::move(*maybeSharedMemory);
  //    return IPC_OK();
  //  }
  //  ```
  static Maybe<SynchronizedSharedMemory> CreateFromRemoteInfo(
      const SynchronizedSharedMemoryRemoteInfo& aIPCInfo) {
    auto maybeDetail =
        SynchronizedSharedMemoryDetail::CreateFromRemoteInfo(aIPCInfo);
    if (!maybeDetail) {
      return Nothing{};
    }
    return Some(SynchronizedSharedMemory(std::move(*maybeDetail)));
  }

  // Generate remote info needed to share object with specified actor
  //
  // Note that the returned information is likely specific to the actor. This
  // should be called individually for each actor that will use the shared
  // object.
  //
  // See `CreateFromRemoteInfo` for usage example
  bool GenerateRemoteInfo(const mozilla::ipc::IProtocol* aActor,
                          SynchronizedSharedMemoryRemoteInfo* aOut) {
    return mDetail.GenerateRemoteInfo(aActor, aOut);
  }

  // Allow move
  SynchronizedSharedMemory(SynchronizedSharedMemory&&) = default;
  SynchronizedSharedMemory& operator=(SynchronizedSharedMemory&&) = default;

  // Disallow copy
  SynchronizedSharedMemory(const SynchronizedSharedMemory&) = delete;
  SynchronizedSharedMemory& operator=(const SynchronizedSharedMemory&) = delete;

  ~SynchronizedSharedMemory() = default;

 private:
  SynchronizedSharedMemory() = default;
  explicit SynchronizedSharedMemory(SynchronizedSharedMemoryDetail aDetail)
      : mDetail(std::move(aDetail)) {}

  void volatile_memcpy(void* aDst, const void* aSrc, size_t aSize) {
    const volatile unsigned char* src =
        static_cast<const volatile unsigned char*>(aSrc);
    volatile unsigned char* dst = static_cast<volatile unsigned char*>(aDst);
    size_t remaining = aSize;

    while (remaining) {
      *dst = *src;
      ++src;
      ++dst;
      --remaining;
    }
  }

  SynchronizedSharedMemoryDetail mDetail;

  // The following is for testing of this API. These should be accessed
  // from `SynchronizedSharedMemoryTestFriend`. This is used because generating
  // a mock IProtocol actor is a fair amount of work.
  friend class SynchronizedSharedMemoryTestFriend;
  bool GenerateTestRemoteInfo(SynchronizedSharedMemoryRemoteInfo* aOut) {
    return mDetail.GenerateTestRemoteInfo(aOut);
  }
};

}  // namespace mozilla::dom

#endif  // DOM_GAMEPAD_SYNCHRONIZEDSHAREDMEMORY_H_
