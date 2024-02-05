/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Shmem.h"

#include "ProtocolUtils.h"
#include "SharedMemoryBasic.h"
#include "ShmemMessageUtils.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace ipc {

class ShmemCreated : public IPC::Message {
 private:
  typedef Shmem::id_t id_t;

 public:
  ShmemCreated(int32_t routingId, id_t aIPDLId, size_t aSize)
      : IPC::Message(
            routingId, SHMEM_CREATED_MESSAGE_TYPE, 0,
            HeaderFlags(NESTED_INSIDE_CPOW, CONTROL_PRIORITY, COMPRESSION_NONE,
                        LAZY_SEND, NOT_CONSTRUCTOR, ASYNC, NOT_REPLY)) {
    MOZ_RELEASE_ASSERT(aSize < std::numeric_limits<uint32_t>::max(),
                       "Tried to create Shmem with size larger than 4GB");
    IPC::MessageWriter writer(*this);
    IPC::WriteParam(&writer, aIPDLId);
    IPC::WriteParam(&writer, uint32_t(aSize));
  }

  static bool ReadInfo(IPC::MessageReader* aReader, id_t* aIPDLId,
                       size_t* aSize) {
    uint32_t size = 0;
    if (!IPC::ReadParam(aReader, aIPDLId) || !IPC::ReadParam(aReader, &size)) {
      return false;
    }
    *aSize = size;
    return true;
  }

  void Log(const std::string& aPrefix, FILE* aOutf) const {
    fputs("(special ShmemCreated msg)", aOutf);
  }
};

class ShmemDestroyed : public IPC::Message {
 private:
  typedef Shmem::id_t id_t;

 public:
  ShmemDestroyed(int32_t routingId, id_t aIPDLId)
      : IPC::Message(
            routingId, SHMEM_DESTROYED_MESSAGE_TYPE, 0,
            HeaderFlags(NOT_NESTED, NORMAL_PRIORITY, COMPRESSION_NONE,
                        LAZY_SEND, NOT_CONSTRUCTOR, ASYNC, NOT_REPLY)) {
    IPC::MessageWriter writer(*this);
    IPC::WriteParam(&writer, aIPDLId);
  }
};

static already_AddRefed<SharedMemory> NewSegment() {
  return MakeAndAddRef<SharedMemoryBasic>();
}

static already_AddRefed<SharedMemory> CreateSegment(size_t aNBytes) {
  if (!aNBytes) {
    return nullptr;
  }
  RefPtr<SharedMemory> segment = NewSegment();
  if (!segment) {
    return nullptr;
  }
  size_t size = SharedMemory::PageAlignedSize(aNBytes);
  if (!segment->Create(size) || !segment->Map(size)) {
    return nullptr;
  }
  return segment.forget();
}

static already_AddRefed<SharedMemory> ReadSegment(
    const IPC::Message& aDescriptor, Shmem::id_t* aId, size_t* aNBytes) {
  if (SHMEM_CREATED_MESSAGE_TYPE != aDescriptor.type()) {
    NS_ERROR("expected 'shmem created' message");
    return nullptr;
  }
  IPC::MessageReader reader(aDescriptor);
  if (!ShmemCreated::ReadInfo(&reader, aId, aNBytes)) {
    return nullptr;
  }
  RefPtr<SharedMemory> segment = NewSegment();
  if (!segment) {
    return nullptr;
  }
  if (!segment->ReadHandle(&reader)) {
    NS_ERROR("trying to open invalid handle");
    return nullptr;
  }
  reader.EndRead();
  if (!*aNBytes) {
    return nullptr;
  }
  size_t size = SharedMemory::PageAlignedSize(*aNBytes);
  if (!segment->Map(size)) {
    return nullptr;
  }
  // close the handle to the segment after it is mapped
  segment->CloseHandle();
  return segment.forget();
}

#if defined(DEBUG)

static void Protect(SharedMemory* aSegment) {
  MOZ_ASSERT(aSegment, "null segment");
  aSegment->Protect(reinterpret_cast<char*>(aSegment->memory()),
                    aSegment->Size(), RightsNone);
}

static void Unprotect(SharedMemory* aSegment) {
  MOZ_ASSERT(aSegment, "null segment");
  aSegment->Protect(reinterpret_cast<char*>(aSegment->memory()),
                    aSegment->Size(), RightsRead | RightsWrite);
}

void Shmem::AssertInvariants() const {
  MOZ_ASSERT(mSegment, "null segment");
  MOZ_ASSERT(mData, "null data pointer");
  MOZ_ASSERT(mSize > 0, "invalid size");
  // if the segment isn't owned by the current process, these will
  // trigger SIGSEGV
  char checkMappingFront = *reinterpret_cast<char*>(mData);
  char checkMappingBack = *(reinterpret_cast<char*>(mData) + mSize - 1);

  // avoid "unused" warnings for these variables:
  Unused << checkMappingFront;
  Unused << checkMappingBack;
}

void Shmem::RevokeRights() {
  AssertInvariants();

  // When sending a non-unsafe shmem, remove read/write rights from the local
  // mapping of the segment.
  if (!mUnsafe) {
    Protect(mSegment);
  }
}

#endif  // if defined(DEBUG)

Shmem::Shmem(SharedMemory* aSegment, id_t aId, size_t aSize, bool aUnsafe)
    : mSegment(aSegment), mData(aSegment->memory()), mSize(aSize), mId(aId) {
#ifdef DEBUG
  mUnsafe = aUnsafe;
  Unprotect(mSegment);
#endif

  MOZ_RELEASE_ASSERT(mSegment->Size() >= mSize,
                     "illegal size in shared memory segment");
}

// static
already_AddRefed<Shmem::SharedMemory> Shmem::Alloc(size_t aNBytes) {
  RefPtr<SharedMemory> segment = CreateSegment(aNBytes);
  if (!segment) {
    return nullptr;
  }

  return segment.forget();
}

// static
already_AddRefed<Shmem::SharedMemory> Shmem::OpenExisting(
    const IPC::Message& aDescriptor, id_t* aId, bool /*unused*/) {
  size_t size;
  RefPtr<SharedMemory> segment = ReadSegment(aDescriptor, aId, &size);
  if (!segment) {
    return nullptr;
  }

  return segment.forget();
}

UniquePtr<IPC::Message> Shmem::MkCreatedMessage(int32_t routingId) {
  AssertInvariants();

  auto msg = MakeUnique<ShmemCreated>(routingId, mId, mSize);
  IPC::MessageWriter writer(*msg);
  if (!mSegment->WriteHandle(&writer)) {
    return nullptr;
  }
  // close the handle to the segment after it is shared
  mSegment->CloseHandle();
  return msg;
}

UniquePtr<IPC::Message> Shmem::MkDestroyedMessage(int32_t routingId) {
  AssertInvariants();
  return MakeUnique<ShmemDestroyed>(routingId, mId);
}

void IPDLParamTraits<Shmem>::Write(IPC::MessageWriter* aWriter,
                                   IProtocol* aActor, Shmem&& aParam) {
  WriteIPDLParam(aWriter, aActor, aParam.mId);
  WriteIPDLParam(aWriter, aActor, uint32_t(aParam.mSize));
#ifdef DEBUG
  WriteIPDLParam(aWriter, aActor, aParam.mUnsafe);
#endif

  aParam.RevokeRights();
  aParam.forget();
}

bool IPDLParamTraits<Shmem>::Read(IPC::MessageReader* aReader,
                                  IProtocol* aActor, paramType* aResult) {
  paramType::id_t id;
  uint32_t size;
  if (!ReadIPDLParam(aReader, aActor, &id) ||
      !ReadIPDLParam(aReader, aActor, &size)) {
    return false;
  }

  bool unsafe = false;
#ifdef DEBUG
  if (!ReadIPDLParam(aReader, aActor, &unsafe)) {
    return false;
  }
#endif

  Shmem::SharedMemory* rawmem = aActor->LookupSharedMemory(id);
  if (rawmem) {
    if (size > rawmem->Size()) {
      return false;
    }

    *aResult = Shmem(rawmem, id, size, unsafe);
    return true;
  }
  *aResult = Shmem();
  return true;
}

}  // namespace ipc
}  // namespace mozilla
