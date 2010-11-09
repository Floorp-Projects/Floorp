/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  NS_OVERRIDE
  virtual bool Create(size_t aNbytes)
  {
    int id = shmget(IPC_PRIVATE, aNbytes, IPC_CREAT | 0600);
    if (id == -1)
      return false;

    mHandle = id;
    mAllocSize = aNbytes;
    Created(aNbytes);

    return Map(aNbytes);
  }

  NS_OVERRIDE
  virtual bool Map(size_t nBytes)
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

#ifdef NS_DEBUG
    struct shmid_ds info;
    if (shmctl(mHandle, IPC_STAT, &info) < 0)
      return false;

    NS_ABORT_IF_FALSE(nBytes <= info.shm_segsz,
                      "Segment doesn't have enough space!");
#endif

    Mapped(nBytes);
    return true;
  }

  NS_OVERRIDE
  virtual void* memory() const
  {
    return mData;
  }

  NS_OVERRIDE
  virtual SharedMemoryType Type() const
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
