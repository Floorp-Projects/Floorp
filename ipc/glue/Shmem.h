/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Shmem_h
#define mozilla_ipc_Shmem_h

#include "mozilla/Attributes.h"

#include "base/basictypes.h"
#include "base/process.h"

#include "nscore.h"
#include "nsDebug.h"
#include "nsAutoPtr.h"

#include "ipc/IPCMessageUtils.h"
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
 *  |SharedMemory{SysV,Basic}::Handle| it was passed, creates a
 *  |mozilla::ipc::SharedMemory| in the child
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
namespace layers {
class ShadowLayerForwarder;
}

namespace ipc {

class Shmem MOZ_FINAL
{
  friend struct IPC::ParamTraits<mozilla::ipc::Shmem>;
#ifdef DEBUG
  // For ShadowLayerForwarder::CheckSurfaceDescriptor
  friend class mozilla::layers::ShadowLayerForwarder;
#endif

public:
  typedef int32_t id_t;
  // Low-level wrapper around platform shmem primitives.
  typedef mozilla::ipc::SharedMemory SharedMemory;
  typedef SharedMemory::SharedMemoryType SharedMemoryType;
  struct IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead {};

  Shmem() :
    mSegment(nullptr),
    mData(nullptr),
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
    mSize = static_cast<size_t>(*PtrToSize(mSegment));
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

  bool operator==(const Shmem& aRhs) const
  {
    // need to compare IDs because of AdoptShmem(); two Shmems might
    // refer to the same segment but with different IDs for different
    // protocol trees.  (NB: it's possible for this method to
    // spuriously return true if AdoptShmem() gives the same ID for
    // two protocol trees, but I don't think that can cause any
    // problems since the Shmems really would be indistinguishable.)
    return mSegment == aRhs.mSegment && mId == aRhs.mId;
  }

  // Returns whether this Shmem is writable by you, and thus whether you can
  // transfer writability to another actor.
  bool
  IsWritable() const
  {
    return mSegment != nullptr;
  }

  // Returns whether this Shmem is readable by you, and thus whether you can
  // transfer readability to another actor.
  bool
  IsReadable() const
  {
    return mSegment != nullptr;
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

  int GetSysVID() const;

  // These shouldn't be used directly, use the IPDL interface instead.
  id_t Id(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead) const {
    return mId;
  }

  SharedMemory* Segment(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead) const {
    return mSegment;
  }

#ifndef DEBUG
  void RevokeRights(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead)
  {
  }
#else
  void RevokeRights(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead);
#endif

  void forget(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead)
  {
    mSegment = nullptr;
    mData = nullptr;
    mSize = 0;
    mId = 0;
  }

  static already_AddRefed<Shmem::SharedMemory>
  Alloc(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
        size_t aNBytes,
        SharedMemoryType aType,
        bool aUnsafe,
        bool aProtect=false);

  // Prepare this to be shared with |aProcess|.  Return an IPC message
  // that contains enough information for the other process to map
  // this segment in OpenExisting() below.  Return a new message if
  // successful (owned by the caller), nullptr if not.
  IPC::Message*
  ShareTo(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
          base::ProcessHandle aProcess,
          int32_t routingId);

  // Stop sharing this with |aProcess|.  Return an IPC message that
  // contains enough information for the other process to unmap this
  // segment.  Return a new message if successful (owned by the
  // caller), nullptr if not.
  IPC::Message*
  UnshareFrom(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
              base::ProcessHandle aProcess,
              int32_t routingId);

  // Return a SharedMemory instance in this process using the
  // descriptor shared to us by the process that created the
  // underlying OS shmem resource.  The contents of the descriptor
  // depend on the type of SharedMemory that was passed to us.
  static already_AddRefed<SharedMemory>
  OpenExisting(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
               const IPC::Message& aDescriptor,
               id_t* aId,
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

  static uint32_t*
  PtrToSize(SharedMemory* aSegment)
  {
    char* endOfSegment =
      reinterpret_cast<char*>(aSegment->memory()) + aSegment->Size();
    return reinterpret_cast<uint32_t*>(endOfSegment - sizeof(uint32_t));
  }

#else
  void AssertInvariants() const;
#endif

  SharedMemory* mSegment;
  void* mData;
  size_t mSize;
  id_t mId;
};


} // namespace ipc
} // namespace mozilla


namespace IPC {

template<>
struct ParamTraits<mozilla::ipc::Shmem>
{
  typedef mozilla::ipc::Shmem paramType;

  // NB: Read()/Write() look creepy in that Shmems have a pointer
  // member, but IPDL internally uses mId to properly initialize a
  // "real" Shmem

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mId);
  }

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
