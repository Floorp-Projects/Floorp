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
      : IPC::Message(routingId, SHMEM_CREATED_MESSAGE_TYPE, 0,
                     HeaderFlags(NESTED_INSIDE_CPOW)) {
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
      : IPC::Message(routingId, SHMEM_DESTROYED_MESSAGE_TYPE) {
    IPC::MessageWriter writer(*this);
    IPC::WriteParam(&writer, aIPDLId);
  }
};

static SharedMemory* NewSegment() { return new SharedMemoryBasic; }

static already_AddRefed<SharedMemory> CreateSegment(size_t aNBytes,
                                                    size_t aExtraSize) {
  RefPtr<SharedMemory> segment = NewSegment();
  if (!segment) {
    return nullptr;
  }
  size_t size = SharedMemory::PageAlignedSize(aNBytes + aExtraSize);
  if (!segment->Create(size) || !segment->Map(size)) {
    return nullptr;
  }
  return segment.forget();
}

static already_AddRefed<SharedMemory> ReadSegment(
    const IPC::Message& aDescriptor, Shmem::id_t* aId, size_t* aNBytes,
    size_t aExtraSize) {
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
  size_t size = SharedMemory::PageAlignedSize(*aNBytes + aExtraSize);
  if (!segment->Map(size)) {
    return nullptr;
  }
  // close the handle to the segment after it is mapped
  segment->CloseHandle();
  return segment.forget();
}

static void DestroySegment(SharedMemory* aSegment) {
  // the SharedMemory dtor closes and unmaps the actual OS shmem segment
  if (aSegment) {
    aSegment->Release();
  }
}

#if defined(DEBUG)

static const char sMagic[] =
    "This little piggy went to market.\n"
    "This little piggy stayed at home.\n"
    "This little piggy has roast beef,\n"
    "This little piggy had none.\n"
    "And this little piggy cried \"Wee! Wee! Wee!\" all the way home";

struct Header {
  // Don't use size_t or bool here because their size depends on the
  // architecture.
  uint32_t mSize;
  uint32_t mUnsafe;
  char mMagic[sizeof(sMagic)];
};

static void GetSections(Shmem::SharedMemory* aSegment, Header** aHeader,
                        char** aFrontSentinel, char** aData,
                        char** aBackSentinel) {
  MOZ_ASSERT(aSegment && aFrontSentinel && aData && aBackSentinel,
             "null param(s)");

  *aFrontSentinel = reinterpret_cast<char*>(aSegment->memory());
  MOZ_ASSERT(*aFrontSentinel, "null memory()");

  *aHeader = reinterpret_cast<Header*>(*aFrontSentinel);

  size_t pageSize = Shmem::SharedMemory::SystemPageSize();
  *aData = *aFrontSentinel + pageSize;

  *aBackSentinel = *aFrontSentinel + aSegment->Size() - pageSize;
}

static Header* GetHeader(Shmem::SharedMemory* aSegment) {
  Header* header;
  char* dontcare;
  GetSections(aSegment, &header, &dontcare, &dontcare, &dontcare);
  return header;
}

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

//
// In debug builds, we specially allocate shmem segments.  The layout
// is as follows
//
//   Page 0: "front sentinel"
//     size of mapping
//     magic bytes
//   Page 1 through n-1:
//     user data
//   Page n: "back sentinel"
//     [nothing]
//
// The mapping can be in one of the following states, wrt to the
// current process.
//
//   State "unmapped": all pages are mapped with no access rights.
//
//   State "mapping": all pages are mapped with read/write access.
//
//   State "mapped": the front and back sentinels are mapped with no
//     access rights, and all the other pages are mapped with
//     read/write access.
//
// When a SharedMemory segment is first allocated, it starts out in
// the "mapping" state for the process that allocates the segment, and
// in the "unmapped" state for the other process.  The allocating
// process will then create a Shmem, which takes the segment into the
// "mapped" state, where it can be accessed by clients.
//
// When a Shmem is sent to another process in an IPDL message, the
// segment transitions into the "unmapped" state for the sending
// process, and into the "mapping" state for the receiving process.
// The receiving process will then create a Shmem from the underlying
// segment, and take the segment into the "mapped" state.
//
// In the "mapping" state, we use the front sentinel to verify the
// integrity of the shmem segment.  If valid, it has a size_t
// containing the number of bytes the user allocated followed by the
// magic bytes above.
//
// In the "mapped" state, the front and back sentinels have no access
// rights.  They act as guards against buffer overflows and underflows
// in client code; if clients touch a sentinel, they die with SIGSEGV.
//
// The "unmapped" state is used to enforce single-owner semantics of
// the shmem segment.  If a process other than the current owner tries
// to touch the segment, it dies with SIGSEGV.
//

Shmem::Shmem(SharedMemory* aSegment, id_t aId)
    : mSegment(aSegment), mData(nullptr), mSize(0) {
  MOZ_ASSERT(mSegment, "null segment");
  MOZ_ASSERT(aId != 0, "invalid ID");

  Unprotect(mSegment);

  Header* header;
  char* frontSentinel;
  char* data;
  char* backSentinel;
  GetSections(aSegment, &header, &frontSentinel, &data, &backSentinel);

  // do a quick validity check to avoid weird-looking crashes in libc
  char check = *frontSentinel;
  (void)check;

  MOZ_ASSERT(!strncmp(header->mMagic, sMagic, sizeof(sMagic)),
             "invalid segment");
  mSize = static_cast<size_t>(header->mSize);

  size_t pageSize = SharedMemory::SystemPageSize();
  MOZ_ASSERT(mSegment->Size() - (2 * pageSize) >= mSize,
             "illegal size in shared memory segment");

  // transition into the "mapped" state by protecting the front and
  // back sentinels (which guard against buffer under/overflows)
  mSegment->Protect(frontSentinel, pageSize, RightsNone);
  mSegment->Protect(backSentinel, pageSize, RightsNone);

  // don't set these until we know they're valid
  mData = data;
  mId = aId;
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

  size_t pageSize = SharedMemory::SystemPageSize();
  Header* header = GetHeader(mSegment);

  // Open this up for reading temporarily
  mSegment->Protect(reinterpret_cast<char*>(header), pageSize, RightsRead);

  if (!header->mUnsafe) {
    Protect(mSegment);
  } else {
    mSegment->Protect(reinterpret_cast<char*>(header), pageSize, RightsNone);
  }
}

// static
already_AddRefed<Shmem::SharedMemory> Shmem::Alloc(size_t aNBytes, bool aUnsafe,
                                                   bool aProtect) {
  NS_ASSERTION(aNBytes <= UINT32_MAX, "Will truncate shmem segment size!");
  MOZ_ASSERT(!aProtect || !aUnsafe, "protect => !unsafe");

  size_t pageSize = SharedMemory::SystemPageSize();
  // |2*pageSize| is for the front and back sentinel
  RefPtr<SharedMemory> segment = CreateSegment(aNBytes, 2 * pageSize);
  if (!segment) {
    return nullptr;
  }

  Header* header;
  char* frontSentinel;
  char* data;
  char* backSentinel;
  GetSections(segment, &header, &frontSentinel, &data, &backSentinel);

  // initialize the segment with Shmem-internal information

  // NB: this can't be a static assert because technically pageSize
  // isn't known at compile time, event though in practice it's always
  // going to be 4KiB
  MOZ_ASSERT(sizeof(Header) <= pageSize, "Shmem::Header has gotten too big");
  memcpy(header->mMagic, sMagic, sizeof(sMagic));
  header->mSize = static_cast<uint32_t>(aNBytes);
  header->mUnsafe = aUnsafe;

  if (aProtect) Protect(segment);

  return segment.forget();
}

// static
already_AddRefed<Shmem::SharedMemory> Shmem::OpenExisting(
    const IPC::Message& aDescriptor, id_t* aId, bool aProtect) {
  size_t size;
  size_t pageSize = SharedMemory::SystemPageSize();
  // |2*pageSize| is for the front and back sentinels
  RefPtr<SharedMemory> segment =
      ReadSegment(aDescriptor, aId, &size, 2 * pageSize);
  if (!segment) {
    return nullptr;
  }

  Header* header = GetHeader(segment);

  if (size != header->mSize) {
    // Deallocation should zero out the header, so check for that.
    if (header->mSize || header->mUnsafe || header->mMagic[0] ||
        memcmp(header->mMagic, &header->mMagic[1],
               sizeof(header->mMagic) - 1)) {
      NS_ERROR("Wrong size for this Shmem!");
    } else {
      NS_WARNING("Shmem was deallocated");
    }
    return nullptr;
  }

  // The caller of this function may not know whether the segment is
  // unsafe or not
  if (!header->mUnsafe && aProtect) Protect(segment);

  return segment.forget();
}

// static
void Shmem::Dealloc(SharedMemory* aSegment) {
  if (!aSegment) return;

  size_t pageSize = SharedMemory::SystemPageSize();
  Header* header;
  char* frontSentinel;
  char* data;
  char* backSentinel;
  GetSections(aSegment, &header, &frontSentinel, &data, &backSentinel);

  aSegment->Protect(frontSentinel, pageSize, RightsWrite | RightsRead);
  memset(header->mMagic, 0, sizeof(sMagic));
  header->mSize = 0;
  header->mUnsafe = false;  // make it "safe" so as to catch errors

  DestroySegment(aSegment);
}

#else  // !defined(DEBUG)

Shmem::Shmem(SharedMemory* aSegment, id_t aId)
    : mSegment(aSegment), mData(aSegment->memory()), mSize(0), mId(aId) {
  mSize = static_cast<size_t>(*PtrToSize(mSegment));
  MOZ_RELEASE_ASSERT(mSegment->Size() - sizeof(uint32_t) >= mSize,
                     "illegal size in shared memory segment");
}

// static
already_AddRefed<Shmem::SharedMemory> Shmem::Alloc(size_t aNBytes,
                                                   bool /*unused*/,
                                                   bool /*unused*/) {
  RefPtr<SharedMemory> segment = CreateSegment(aNBytes, sizeof(uint32_t));
  if (!segment) {
    return nullptr;
  }

  *PtrToSize(segment) = static_cast<uint32_t>(aNBytes);

  return segment.forget();
}

// static
already_AddRefed<Shmem::SharedMemory> Shmem::OpenExisting(
    const IPC::Message& aDescriptor, id_t* aId, bool /*unused*/) {
  size_t size;
  RefPtr<SharedMemory> segment =
      ReadSegment(aDescriptor, aId, &size, sizeof(uint32_t));
  if (!segment) {
    return nullptr;
  }

  // this is the only validity check done in non-DEBUG builds
  if (size != static_cast<size_t>(*PtrToSize(segment))) {
    return nullptr;
  }

  return segment.forget();
}

// static
void Shmem::Dealloc(SharedMemory* aSegment) { DestroySegment(aSegment); }

#endif  // if defined(DEBUG)

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

  aParam.RevokeRights();
  aParam.forget();
}

bool IPDLParamTraits<Shmem>::Read(IPC::MessageReader* aReader,
                                  IProtocol* aActor, paramType* aResult) {
  paramType::id_t id;
  if (!ReadIPDLParam(aReader, aActor, &id)) {
    return false;
  }

  Shmem::SharedMemory* rawmem = aActor->LookupSharedMemory(id);
  if (rawmem) {
    *aResult = Shmem(rawmem, id);
    return true;
  }
  *aResult = Shmem();
  return true;
}

}  // namespace ipc
}  // namespace mozilla
