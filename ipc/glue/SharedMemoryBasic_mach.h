/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryBasic_mach_h
#define mozilla_ipc_SharedMemoryBasic_mach_h

#include "base/process.h"

#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/ipc/SharedMemory.h"
#include <mach/port.h>

#ifdef FUZZING
#  include "mozilla/ipc/SharedMemoryFuzzer.h"
#endif

//
// This is a low-level wrapper around platform shared memory.  Don't
// use it directly; use Shmem allocated through IPDL interfaces.
//

class MachPortSender;
class ReceivePort;

namespace mozilla {
namespace ipc {

class SharedMemoryBasic final
    : public SharedMemoryCommon<mozilla::UniqueMachSendRight> {
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

  virtual bool IsHandleValid(const Handle& aHandle) const override;

  virtual Handle CloneHandle() override;

 private:
  ~SharedMemoryBasic();

  mozilla::UniqueMachSendRight mPort;
  // Pointer to mapped region, null if unmapped.
  void* mMemory;
  // Access rights to map an existing region with.
  OpenRights mOpenRights;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef mozilla_ipc_SharedMemoryBasic_mach_h
