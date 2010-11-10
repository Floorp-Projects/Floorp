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
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
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

#ifndef mozilla_ipc_SharedMemory_h
#define mozilla_ipc_SharedMemory_h

#include "nsDebug.h"
#include "nsISupportsImpl.h"    // NS_INLINE_DECL_REFCOUNTING

//
// This is a low-level wrapper around platform shared memory.  Don't
// use it directly; use Shmem allocated through IPDL interfaces.
//
namespace {
enum Rights {
  RightsNone = 0,
  RightsRead = 1 << 0,
  RightsWrite = 1 << 1
};
}

namespace mozilla {
namespace ipc {

class SharedMemory
{
public:
  enum SharedMemoryType {
    TYPE_BASIC,
    TYPE_SYSV,
    TYPE_UNKNOWN
  };

  virtual ~SharedMemory() { Unmapped(); Destroyed(); }

  size_t Size() const { return mMappedSize; }

  virtual void* memory() const = 0;

  virtual bool Create(size_t size) = 0;
  virtual bool Map(size_t nBytes) = 0;

  virtual SharedMemoryType Type() const = 0;

  void
  Protect(char* aAddr, size_t aSize, int aRights)
  {
    char* memStart = reinterpret_cast<char*>(memory());
    if (!memStart)
      NS_RUNTIMEABORT("SharedMemory region points at NULL!");
    char* memEnd = memStart + Size();

    char* protStart = aAddr;
    if (!protStart)
      NS_RUNTIMEABORT("trying to Protect() a NULL region!");
    char* protEnd = protStart + aSize;

    if (!(memStart <= protStart
          && protEnd <= memEnd))
      NS_RUNTIMEABORT("attempt to Protect() a region outside this SharedMemory");

    // checks alignment etc.
    SystemProtect(aAddr, aSize, aRights);
  }

  NS_INLINE_DECL_REFCOUNTING(SharedMemory)

  static void SystemProtect(char* aAddr, size_t aSize, int aRights);
  static size_t SystemPageSize();
  static size_t PageAlignedSize(size_t aSize);

protected:
  SharedMemory();

  // Implementations should call these methods on shmem usage changes,
  // but *only if* the OS-specific calls are known to have succeeded.
  // The methods are expected to be called in the pattern
  //
  //   Created (Mapped Unmapped)* Destroy
  //
  // but this isn't checked.
  void Created(size_t aNBytes);
  void Mapped(size_t aNBytes);
  void Unmapped();
  void Destroyed();

  // The size of the shmem region requested in Create(), if
  // successful.  SharedMemory instances that are opened from a
  // foreign handle have an alloc size of 0, even though they have
  // access to the alloc-size information.
  size_t mAllocSize;
  // The size of the region mapped in Map(), if successful.  All
  // SharedMemorys that are mapped have a non-zero mapped size.
  size_t mMappedSize;
};

} // namespace ipc
} // namespace mozilla


#endif // ifndef mozilla_ipc_SharedMemory_h
