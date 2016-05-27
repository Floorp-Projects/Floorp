/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryBasic_chromium_h
#define mozilla_ipc_SharedMemoryBasic_chromium_h

#include "base/shared_memory.h"
#include "SharedMemory.h"

#include "nsDebug.h"

//
// This is a low-level wrapper around platform shared memory.  Don't
// use it directly; use Shmem allocated through IPDL interfaces.
//

namespace mozilla {
namespace ipc {

class SharedMemoryBasic final : public SharedMemoryCommon<base::SharedMemoryHandle>
{
public:
  SharedMemoryBasic()
  {
  }

  virtual bool SetHandle(const Handle& aHandle) override {
    return mSharedMemory.SetHandle(aHandle, false);
  }

  virtual bool Create(size_t aNbytes) override
  {
    bool ok = mSharedMemory.Create("", false, false, aNbytes);
    if (ok) {
      Created(aNbytes);
    }
    return ok;
  }

  virtual bool Map(size_t nBytes) override
  {
    bool ok = mSharedMemory.Map(nBytes);
    if (ok) {
      Mapped(nBytes);
    }
    return ok;
  }

  virtual void CloseHandle() override
  {
    mSharedMemory.Close(false);
  }

  virtual void* memory() const override
  {
    return mSharedMemory.memory();
  }

  virtual SharedMemoryType Type() const override
  {
    return TYPE_BASIC;
  }

  static Handle NULLHandle()
  {
    return base::SharedMemory::NULLHandle();
  }

  virtual bool IsHandleValid(const Handle &aHandle) const override
  {
    return base::SharedMemory::IsHandleValid(aHandle);
  }

  virtual bool ShareToProcess(base::ProcessId aProcessId,
                              Handle* new_handle) override
  {
    base::SharedMemoryHandle handle;
    bool ret = mSharedMemory.ShareToProcess(aProcessId, &handle);
    if (ret)
      *new_handle = handle;
    return ret;
  }

private:
  ~SharedMemoryBasic()
  {
  }

  base::SharedMemory mSharedMemory;
};

} // namespace ipc
} // namespace mozilla


#endif // ifndef mozilla_ipc_SharedMemoryBasic_chromium_h
