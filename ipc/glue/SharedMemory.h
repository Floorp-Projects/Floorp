/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemory_h
#define mozilla_ipc_SharedMemory_h

#include "nsDebug.h"
#include "nsISupportsImpl.h"    // NS_INLINE_DECL_REFCOUNTING
#include "mozilla/Attributes.h"

#include "base/process.h"
#include "chrome/common/ipc_message_utils.h"

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
} // namespace

namespace mozilla {

namespace ipc {
class SharedMemory;
} // namespace ipc

namespace ipc {

class SharedMemory
{
protected:
  virtual ~SharedMemory()
  {
    Unmapped();
    Destroyed();
  }

public:
  enum SharedMemoryType {
    TYPE_BASIC,
    TYPE_UNKNOWN
  };

  size_t Size() const { return mMappedSize; }

  virtual void* memory() const = 0;

  virtual bool Create(size_t size) = 0;
  virtual bool Map(size_t nBytes) = 0;

  virtual void CloseHandle() = 0;

  virtual SharedMemoryType Type() const = 0;

  virtual bool ShareHandle(base::ProcessId aProcessId, IPC::Message* aMessage) = 0;
  virtual bool ReadHandle(const IPC::Message* aMessage, PickleIterator* aIter) = 0;

  void
  Protect(char* aAddr, size_t aSize, int aRights)
  {
    char* memStart = reinterpret_cast<char*>(memory());
    if (!memStart)
      MOZ_CRASH("SharedMemory region points at NULL!");
    char* memEnd = memStart + Size();

    char* protStart = aAddr;
    if (!protStart)
      MOZ_CRASH("trying to Protect() a NULL region!");
    char* protEnd = protStart + aSize;

    if (!(memStart <= protStart
          && protEnd <= memEnd))
      MOZ_CRASH("attempt to Protect() a region outside this SharedMemory");

    // checks alignment etc.
    SystemProtect(aAddr, aSize, aRights);
  }

  // bug 1168843, compositor thread may create shared memory instances that are destroyed by main thread on shutdown, so this must use thread-safe RC to avoid hitting assertion
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedMemory)

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

template<typename HandleImpl>
class SharedMemoryCommon : public SharedMemory
{
public:
  typedef HandleImpl Handle;

  virtual bool ShareToProcess(base::ProcessId aProcessId, Handle* aHandle) = 0;
  virtual bool IsHandleValid(const Handle& aHandle) const = 0;
  virtual bool SetHandle(const Handle& aHandle) = 0;

  virtual bool ShareHandle(base::ProcessId aProcessId, IPC::Message* aMessage) override
  {
    Handle handle;
    if (!ShareToProcess(aProcessId, &handle)) {
      return false;
    }
    IPC::WriteParam(aMessage, handle);
    return true;
  }

  virtual bool ReadHandle(const IPC::Message* aMessage, PickleIterator* aIter) override
  {
    Handle handle;
    return IPC::ReadParam(aMessage, aIter, &handle) &&
           IsHandleValid(handle) &&
           SetHandle(handle);
  }
};

} // namespace ipc
} // namespace mozilla


#endif // ifndef mozilla_ipc_SharedMemory_h
