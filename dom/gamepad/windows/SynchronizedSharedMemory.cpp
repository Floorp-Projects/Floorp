/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the implementation of SynchronizedSharedMemory for Windows.

#include "mozilla/dom/SynchronizedSharedMemory.h"
#include "mozilla/dom/SynchronizedSharedMemoryRemoteInfo.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "GamepadWindowsUtil.h"
#include <utility>
#include <windows.h>

namespace mozilla::dom {

// This Impl class is where most of the actual logic is for creating/using
// the shared memory is located. The public member functions of
// SynchronizedSharedMemoryDetail mostly just pass the call through to this.
class SynchronizedSharedMemoryDetail::Impl {
 public:
  static UniquePtr<Impl> CreateNew(uint32_t aSize) {
    UniqueHandle<NTMutexHandleTraits> mutex(::CreateMutex(
        nullptr /*no ACL*/, FALSE /*not owned*/, nullptr /*no name*/));
    if (!mutex) {
      return nullptr;
    }

    UniqueHandle<NTFileMappingHandleTraits> fileMapping(
        ::CreateFileMapping(INVALID_HANDLE_VALUE /*memory-only*/,
                            nullptr /*no ACL*/, PAGE_READWRITE, 0 /*sizeHigh*/,
                            aSize /*sizeLow*/, nullptr /*no name*/));
    if (!fileMapping) {
      return nullptr;
    }

    UniqueHandle<NTFileViewHandleTraits> sharedPtr(::MapViewOfFile(
        fileMapping.Get(), FILE_MAP_ALL_ACCESS, 0 /*offsetHigh*/,
        0 /*offsetLow*/, 0 /*Map entire region*/));
    if (!sharedPtr) {
      return nullptr;
    }

    return UniquePtr<Impl>(new Impl(std::move(mutex), std::move(fileMapping),
                                    std::move(sharedPtr)));
  }
  static UniquePtr<Impl> CreateFromRemote(
      const SynchronizedSharedMemoryRemoteInfo& aRemoteCreationInfo) {
    UniqueHandle<NTMutexHandleTraits> mutex(
        reinterpret_cast<HANDLE>(aRemoteCreationInfo.mutexHandle()));
    UniqueHandle<NTFileMappingHandleTraits> fileMapping(
        reinterpret_cast<HANDLE>(aRemoteCreationInfo.sharedFileHandle()));

    if (!mutex || !fileMapping) {
      return nullptr;
    }

    UniqueHandle<NTFileViewHandleTraits> sharedPtr(::MapViewOfFile(
        fileMapping.Get(), FILE_MAP_ALL_ACCESS, 0 /*offsetHigh*/,
        0 /*offsetLow*/, 0 /*Map entire region*/));
    if (!sharedPtr) {
      return nullptr;
    }

    return UniquePtr<Impl>(new Impl(std::move(mutex), std::move(fileMapping),
                                    std::move(sharedPtr)));
  }

  ~Impl() {
    MOZ_ASSERT(mMutex);
    MOZ_ASSERT(mFileMapping);
    MOZ_ASSERT(mSharedPtr);
  }

  bool GenerateRemoteInfo(const mozilla::ipc::IProtocol* aActor,
                          SynchronizedSharedMemoryRemoteInfo* aOut) {
    MOZ_ASSERT(mMutex);
    MOZ_ASSERT(mFileMapping);

    DWORD targetPID = aActor ? aActor->OtherPid() : ::GetCurrentProcessId();

    HANDLE remoteMutexHandle = nullptr;
    if (!mozilla::ipc::DuplicateHandle(mMutex.Get(), targetPID,
                                       &remoteMutexHandle, 0,
                                       DUPLICATE_SAME_ACCESS)) {
      return false;
    }

    HANDLE remoteSharedFileHandle = nullptr;
    if (!mozilla::ipc::DuplicateHandle(mFileMapping.Get(), targetPID,
                                       &remoteSharedFileHandle, 0,
                                       DUPLICATE_SAME_ACCESS)) {
      // There's no reasonable way to prevent leaking remoteMutexHandle here,
      // since we don't own it
      return false;
    }

    MOZ_ASSERT(remoteMutexHandle);
    MOZ_ASSERT(remoteSharedFileHandle);

    (*aOut) = SynchronizedSharedMemoryRemoteInfo{
        reinterpret_cast<WindowsHandle>(remoteMutexHandle),
        reinterpret_cast<WindowsHandle>(remoteSharedFileHandle),
    };

    return true;
  }

  void LockMutex() {
    MOZ_ALWAYS_TRUE(::WaitForSingleObject(mMutex.Get(), INFINITE) !=
                    WAIT_FAILED);
  }

  void UnlockMutex() { MOZ_ALWAYS_TRUE(::ReleaseMutex(mMutex.Get())); }

  void* GetPtr() const { return mSharedPtr.Get(); }

  // Disallow copying/moving
  Impl(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl& operator=(Impl&&) = delete;

 private:
  Impl(UniqueHandle<NTMutexHandleTraits> aMutex,
       UniqueHandle<NTFileMappingHandleTraits> aFileMapping,
       UniqueHandle<NTFileViewHandleTraits> aSharedPtr)
      : mMutex(std::move(aMutex)),
        mFileMapping(std::move(aFileMapping)),
        mSharedPtr(std::move(aSharedPtr)) {
    MOZ_ASSERT(mMutex);
    MOZ_ASSERT(mFileMapping);
    MOZ_ASSERT(mSharedPtr);
  }

  UniqueHandle<NTMutexHandleTraits> mMutex;
  UniqueHandle<NTFileMappingHandleTraits> mFileMapping;
  UniqueHandle<NTFileViewHandleTraits> mSharedPtr;
};

//////////// Everything below this line is Pimpl boilerplate ///////////////////

// static
Maybe<SynchronizedSharedMemoryDetail> SynchronizedSharedMemoryDetail::CreateNew(
    uint32_t aSize) {
  MOZ_RELEASE_ASSERT(aSize);

  UniquePtr<Impl> impl = Impl::CreateNew(aSize);
  if (!impl) {
    return Nothing{};
  }

  return Some(SynchronizedSharedMemoryDetail(std::move(impl)));
}

void SynchronizedSharedMemoryDetail::LockMutex() {
  MOZ_RELEASE_ASSERT(mImpl);
  mImpl->LockMutex();
}

void SynchronizedSharedMemoryDetail::UnlockMutex() {
  MOZ_RELEASE_ASSERT(mImpl);
  mImpl->UnlockMutex();
}

void* SynchronizedSharedMemoryDetail::GetPtr() const {
  MOZ_RELEASE_ASSERT(mImpl);
  return mImpl->GetPtr();
}

// static
Maybe<SynchronizedSharedMemoryDetail>
SynchronizedSharedMemoryDetail::CreateFromRemoteInfo(
    const SynchronizedSharedMemoryRemoteInfo& aIPCInfo) {
  UniquePtr<Impl> impl = Impl::CreateFromRemote(aIPCInfo);
  if (!impl) {
    return Nothing{};
  }

  return Some(SynchronizedSharedMemoryDetail(std::move(impl)));
}

bool SynchronizedSharedMemoryDetail::GenerateRemoteInfo(
    const mozilla::ipc::IProtocol* aActor,
    SynchronizedSharedMemoryRemoteInfo* aOut) {
  MOZ_RELEASE_ASSERT(mImpl);

  return mImpl->GenerateRemoteInfo(aActor, aOut);
}

bool SynchronizedSharedMemoryDetail::GenerateTestRemoteInfo(
    SynchronizedSharedMemoryRemoteInfo* aOut) {
  MOZ_RELEASE_ASSERT(mImpl);

  return mImpl->GenerateRemoteInfo(nullptr, aOut);
}

SynchronizedSharedMemoryDetail::SynchronizedSharedMemoryDetail() = default;
SynchronizedSharedMemoryDetail::~SynchronizedSharedMemoryDetail() = default;

SynchronizedSharedMemoryDetail::SynchronizedSharedMemoryDetail(
    UniquePtr<Impl> aImpl)
    : mImpl(std::move(aImpl)) {
  MOZ_ASSERT(mImpl);
}

SynchronizedSharedMemoryDetail::SynchronizedSharedMemoryDetail(
    SynchronizedSharedMemoryDetail&&) = default;
SynchronizedSharedMemoryDetail& SynchronizedSharedMemoryDetail::operator=(
    SynchronizedSharedMemoryDetail&&) = default;

}  // namespace mozilla::dom
