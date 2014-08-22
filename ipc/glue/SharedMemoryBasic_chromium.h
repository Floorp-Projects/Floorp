/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
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

class SharedMemoryBasic : public SharedMemory
{
public:
  typedef base::SharedMemoryHandle Handle;

  SharedMemoryBasic()
  {
  }

  explicit SharedMemoryBasic(const Handle& aHandle)
    : mSharedMemory(aHandle, false)
  {
  }

  virtual bool Create(size_t aNbytes) MOZ_OVERRIDE
  {
    bool ok = mSharedMemory.Create("", false, false, aNbytes);
    if (ok) {
      Created(aNbytes);
    }
    return ok;
  }

  virtual bool Map(size_t nBytes) MOZ_OVERRIDE
  {
    bool ok = mSharedMemory.Map(nBytes);
    if (ok) {
      Mapped(nBytes);
    }
    return ok;
  }

  virtual void* memory() const MOZ_OVERRIDE
  {
    return mSharedMemory.memory();
  }

  virtual SharedMemoryType Type() const MOZ_OVERRIDE
  {
    return TYPE_BASIC;
  }

  static Handle NULLHandle()
  {
    return base::SharedMemory::NULLHandle();
  }

  static bool IsHandleValid(const Handle &aHandle)
  {
    return base::SharedMemory::IsHandleValid(aHandle);
  }

  bool ShareToProcess(base::ProcessHandle process,
                      Handle* new_handle)
  {
    base::SharedMemoryHandle handle;
    bool ret = mSharedMemory.ShareToProcess(process, &handle);
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
