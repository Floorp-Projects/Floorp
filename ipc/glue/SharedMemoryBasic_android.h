/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryBasic_android_h
#define mozilla_ipc_SharedMemoryBasic_android_h

#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/UniquePtrExtensions.h"

#ifdef FUZZING
#  include "mozilla/ipc/SharedMemoryFuzzer.h"
#endif

//
// This is a low-level wrapper around platform shared memory.  Don't
// use it directly; use Shmem allocated through IPDL interfaces.
//

namespace mozilla {
namespace ipc {

class SharedMemoryBasic final
    : public SharedMemoryCommon<mozilla::UniqueFileHandle> {
 public:
  SharedMemoryBasic();

  virtual bool SetHandle(Handle aHandle, OpenRights aRights) override;

  virtual bool Create(size_t aNbytes) override;

  virtual bool Map(size_t nBytes, void* fixed_address = nullptr) override;

  virtual void Unmap() override;

  virtual void CloseHandle() override;

  virtual void* memory() const override {
#ifdef FUZZING
    return SharedMemoryFuzzer::MutateSharedMemory(mMemory, mAllocSize);
#else
    return mMemory;
#endif
  }

  static Handle NULLHandle() { return Handle(); }

  static void* FindFreeAddressSpace(size_t aSize);

  virtual bool IsHandleValid(const Handle& aHandle) const override {
    return aHandle != nullptr;
  }

  virtual Handle CloneHandle() override;

 private:
  ~SharedMemoryBasic();

  // The /dev/ashmem fd we allocate.
  int mShmFd;
  // Pointer to mapped region, null if unmapped.
  void* mMemory;
  // Access rights to map an existing region with.
  OpenRights mOpenRights;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef mozilla_ipc_SharedMemoryBasic_android_h
