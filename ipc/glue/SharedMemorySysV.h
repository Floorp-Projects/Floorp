/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemorySysV_h
#define mozilla_ipc_SharedMemorySysV_h

#if defined(OS_LINUX) && !defined(ANDROID)

// SysV shared memory isn't available on Windows, but we define the
// following macro so that #ifdefs are clearer (compared to #ifdef
// OS_LINUX).
#define MOZ_HAVE_SHAREDMEMORYSYSV

#include "SharedMemory.h"

#include "nsDebug.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

//
// This is a low-level wrapper around platform shared memory.  Don't
// use it directly; use Shmem allocated through IPDL interfaces.
//

namespace mozilla {
namespace ipc {


class SharedMemorySysV : public SharedMemory
{
public:
  typedef int Handle;

  SharedMemorySysV() :
    mHandle(-1),
    mData(nsnull)
  {
  }

  SharedMemorySysV(Handle aHandle) :
    mHandle(aHandle),
    mData(nsnull)
  {
  }

  virtual ~SharedMemorySysV()
  {
    shmdt(mData);
    mHandle = -1;
    mData = nsnull;
  }

  virtual bool Create(size_t aNbytes) MOZ_OVERRIDE
  {
    int id = shmget(IPC_PRIVATE, aNbytes, IPC_CREAT | 0600);
    if (id == -1)
      return false;

    mHandle = id;
    mAllocSize = aNbytes;
    Created(aNbytes);

    return Map(aNbytes);
  }

  virtual bool Map(size_t nBytes) MOZ_OVERRIDE
  {
    // already mapped
    if (mData)
      return true;

    if (!IsHandleValid(mHandle))
      return false;

    void* mem = shmat(mHandle, nsnull, 0);
    if (mem == (void*) -1) {
      char warning[256];
      snprintf(warning, sizeof(warning)-1,
               "shmat(): %s (%d)\n", strerror(errno), errno);

      NS_WARNING(warning);

      return false;
    }

    // Mark the handle as deleted so that, should this process go away, the
    // segment is cleaned up.
    shmctl(mHandle, IPC_RMID, 0);

    mData = mem;

#ifdef DEBUG
    struct shmid_ds info;
    if (shmctl(mHandle, IPC_STAT, &info) < 0)
      return false;

    NS_ABORT_IF_FALSE(nBytes <= info.shm_segsz,
                      "Segment doesn't have enough space!");
#endif

    Mapped(nBytes);
    return true;
  }

  virtual void* memory() const MOZ_OVERRIDE
  {
    return mData;
  }

  virtual SharedMemoryType Type() const MOZ_OVERRIDE
  {
    return TYPE_SYSV;
  }

  Handle GetHandle() const
  {
    NS_ABORT_IF_FALSE(IsHandleValid(mHandle), "invalid handle");
    return mHandle;
  }

  static Handle NULLHandle()
  {
    return -1;
  }

  static bool IsHandleValid(Handle aHandle)
  {
    return aHandle != -1;
  }

private:
  Handle mHandle;
  void* mData;
};

} // namespace ipc
} // namespace mozilla

#endif // OS_LINUX

#endif // ifndef mozilla_ipc_SharedMemorySysV_h
