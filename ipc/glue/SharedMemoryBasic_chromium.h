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

  SharedMemoryBasic() :
    mSize(0)
  {
  }

  SharedMemoryBasic(const Handle& aHandle) :
    mSharedMemory(aHandle, false),
    mSize(0)
  {
  }

  NS_OVERRIDE
  virtual bool Create(size_t aNbytes)
  {
    return mSharedMemory.Create("", false, false, aNbytes);
  }

  NS_OVERRIDE
  virtual bool Map(size_t nBytes)
  {
    bool ok = mSharedMemory.Map(nBytes);
    if (ok)
      mSize = nBytes;
    return ok;
  }

  NS_OVERRIDE
  virtual size_t Size() const
  {
    return mSize;
  }

  NS_OVERRIDE
  virtual void* memory() const
  {
    return mSharedMemory.memory();
  }

  NS_OVERRIDE
  virtual SharedMemoryType Type() const
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
  base::SharedMemory mSharedMemory;
  // NB: we have to track this because shared_memory_win.cc doesn't
  size_t mSize;
};

} // namespace ipc
} // namespace mozilla


#endif // ifndef mozilla_ipc_SharedMemoryBasic_chromium_h
