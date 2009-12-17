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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

#ifndef mozilla_ipc_Shmem_h
#define mozilla_ipc_Shmem_h

#include "base/basictypes.h"

#include "nscore.h"
#include "nsDebug.h"

#include "IPC/IPCMessageUtils.h"
#include "mozilla/ipc/SharedMemory.h"

/**
 * |Shmem| is one agent in the IPDL shared memory scheme.  The way it
    works is essentially
 *
 *  (1) C++ code calls, say, |parentActor->AllocShmem(size)|

 *  (2) IPDL-generated code creates a |mozilla::ipc::SharedMemory|
 *  wrapping the bare OS shmem primitives.  The code then adds the new
 *  SharedMemory to the set of shmem segments being managed by IPDL.
 *
 *  (3) IPDL-generated code "shares" the new SharedMemory to the child
 *  process, and then sends a special asynchronous IPC message to the
 *  child notifying it of the creation of the segment.  (What this
 *  means is OS specific.)
 *
 *  (4a) The child receives the special IPC message, and using the
 *  |mozilla::ipc::SharedMemoryHandle| (which is an fd or HANDLE) it
 *  was passed, creates a |mozilla::ipc::SharedMemory| in the child
 *  process.
 *
 *  (4b) After sending the "shmem-created" IPC message, IPDL-generated
 *  code in the parent returns a |mozilla::ipc::Shmem| back to the C++
 *  caller of |parentActor->AllocShmem()|.  The |Shmem| is a "weak
 *  reference" to the underlying |SharedMemory|, which is managed by
 *  IPDL-generated code.  C++ consumers of |Shmem| can't get at the
 *  underlying |SharedMemory|.
 *
 * If parent code wants to give access rights to the Shmem to the
 * child, it does so by sending its |Shmem| to the child, in an IPDL
 * message.  The parent's |Shmem| then "dies", i.e. becomes
 * inaccessible.  This process could be compared to passing a
 * "shmem-access baton" between parent and child.
 */

namespace mozilla {
namespace ipc {


class NS_FINAL_CLASS Shmem
{
  friend struct IPC::ParamTraits<mozilla::ipc::Shmem>;

public:
  typedef int32 id_t;
  // Low-level wrapper around platform shmem primitives.
  typedef mozilla::ipc::SharedMemory SharedMemory;
  typedef SharedMemory::SharedMemoryHandle SharedMemoryHandle;
  struct IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead {};

  Shmem() :
    mSegment(0),
    mData(0),
    mSize(0),
    mId(0)
  {
  }

  Shmem(const Shmem& aOther) :
    mSegment(aOther.mSegment),
    mData(aOther.mData),
    mSize(aOther.mSize),
    mId(aOther.mId)
  {
  }

#if !defined(DEBUG)
  Shmem(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
        SharedMemory* aSegment, id_t aId) :
    mSegment(aSegment),
    mData(aSegment->memory()),
    mSize(0),
    mId(aId)
  {
    mSize = *PtrToSize(mSegment);
  }
#else
  Shmem(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
        SharedMemory* aSegment, id_t aId);
#endif

  ~Shmem()
  {
    // Shmem only holds a "weak ref" to the actual segment, which is
    // owned by IPDL. So there's nothing interesting to be done here
    forget(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead());
  }

  Shmem& operator=(const Shmem& aRhs)
  {
    mSegment = aRhs.mSegment;
    mData = aRhs.mData;
    mSize = aRhs.mSize;
    mId = aRhs.mId;
    return *this;
  }

  // Return a pointer to the user-visible data segment.
  template<typename T>
  T*
  get() const
  {
    AssertInvariants();
    AssertAligned<T>();

    return reinterpret_cast<T*>(mData);
  }

  // Return the size of the segment as requested when this shmem
  // segment was allocated, in units of T.  The underlying mapping may
  // actually be larger because of page alignment and private data,
  // but this isn't exposed to clients.
  template<typename T>
  size_t
  Size() const
  {
    AssertInvariants();
    AssertAligned<T>();

    return mSize / sizeof(T);
  }

  // These shouldn't be used directly, use the IPDL interface instead.
  id_t Id(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead) {
    return mId;
  }

  void RevokeRights(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead)
  {
    Protect(mSegment);
  }

  void forget(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead)
  {
    mSegment = 0;
    mData = 0;
    mSize = 0;
    mId = 0;
  }

  static SharedMemory*
  Alloc(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
        size_t aNBytes,
        bool aProtect=false);

  // Return a SharedMemory instance in this process using the
  // SharedMemoryHandle (fd/HANDLE) shared to us by the process that
  // created the underlying OS shmem resource
  static SharedMemory*
  OpenExisting(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
               SharedMemoryHandle aHandle,
               size_t aNBytes,
               bool aProtect=false);

  static void
  Dealloc(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
          SharedMemory* aSegment);

private:
  template<typename T>
  void AssertAligned() const
  {
    if (0 != (mSize % sizeof(T)))
      NS_RUNTIMEABORT("shmem is not T-aligned");
  }

#if !defined(DEBUG)
  void AssertInvariants() const
  { }
  static void Unprotect(SharedMemory* aSegment)
  { }
  static void Protect(SharedMemory* aSegment)
  { }

  static size_t*
  PtrToSize(SharedMemory* aSegment)
  {
    char* endOfSegment =
      reinterpret_cast<char*>(aSegment->memory()) + aSegment->Size();
    return reinterpret_cast<size_t*>(endOfSegment - sizeof(size_t));
  }

#else
  void AssertInvariants() const;

  static void Unprotect(SharedMemory* aSegment);
  static void Protect(SharedMemory* aSegment);
#endif

  static SharedMemory*
  CreateSegment(size_t aNBytes,
                SharedMemoryHandle aHandle=SharedMemory::NULLHandle());

  static void
  DestroySegment(SharedMemory* aSegment);

  static size_t
  PageAlignedSize(size_t aSize);

  SharedMemory* mSegment;
  void* mData;
  size_t mSize;
  id_t mId;

  // disable these
  static void* operator new(size_t) CPP_THROW_NEW;
  static void operator delete(void*);  
};


} // namespace ipc
} // namespace mozilla


namespace IPC {

template<>
struct ParamTraits<mozilla::ipc::Shmem>
{
  typedef mozilla::ipc::Shmem paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mId);
  }

  // NB: Read()/Write() look creepy, but IPDL internally uses mId to
  // properly initialize a "real" Shmem

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    paramType::id_t id;
    if (!ReadParam(aMsg, aIter, &id))
      return false;
    aResult->mId = id;
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(L"(shmem segment)");
  }
};


} // namespace IPC


#endif // ifndef mozilla_ipc_Shmem_h
