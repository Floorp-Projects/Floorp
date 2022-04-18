/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Shmem.h"

#include "ProtocolUtils.h"
#include "SharedMemoryBasic.h"
#include "ShmemMessageUtils.h"
#include "mozilla/Unused.h"

namespace mozilla::ipc {

//
// In debug builds, we specially allocate shmem segments.  The layout
// is as follows
//
//   Page 0: "front sentinel"
//     [nothing]
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
// In the "mapped" state, the front and back sentinels have no access
// rights.  They act as guards against buffer overflows and underflows
// in client code; if clients touch a sentinel, they die with SIGSEGV.
//
// The "unmapped" state is used to enforce single-owner semantics of
// the shmem segment.  If a process other than the current owner tries
// to touch the segment, it dies with SIGSEGV.
//

static size_t MappingSize(size_t aNBytes) {
  size_t pageSize = SharedMemory::SystemPageSize();
  MOZ_ASSERT(IsPowerOfTwo(pageSize));

  // Extra padding required to align to pagesize.
  size_t diff = aNBytes & (pageSize - 1);
  diff = (pageSize - diff) & (pageSize - 1);

  CheckedInt<size_t> totalSize = aNBytes;
  totalSize += diff;

#ifdef DEBUG
  // Allocate 2 extra pages to act as guard pages for the shared memory region
  // in debug-mode. No extra space is allocated in release mode.
  totalSize += 2 * pageSize;
#endif

  MOZ_RELEASE_ASSERT(totalSize.isValid());
  return totalSize.value();
}

static Span<char> ConfigureAndGetData(SharedMemory* aSegment) {
  Span<char> memory{reinterpret_cast<char*>(aSegment->memory()),
                    aSegment->Size()};
#ifdef DEBUG
  size_t pageSize = SharedMemory::SystemPageSize();
  auto [frontSentinel, suffix] = memory.SplitAt(pageSize);
  auto [data, backSentinel] = memory.SplitAt(suffix.Length() - pageSize);

  // The sentinel memory regions should be non-readable and non-writable,
  // whereas the data region needs to be readable & writable.
  aSegment->Protect(frontSentinel.data(), frontSentinel.size(), RightsNone);
  aSegment->Protect(data.data(), data.size(), RightsRead | RightsWrite);
  aSegment->Protect(backSentinel.data(), backSentinel.size(), RightsNone);
  return data;
#else
  return memory;
#endif
}

static RefPtr<SharedMemory> AllocSegment(size_t aNBytes,
                                         SharedMemory::SharedMemoryType aType) {
  size_t mappingSize = MappingSize(aNBytes);

  MOZ_RELEASE_ASSERT(aType == SharedMemory::TYPE_BASIC,
                     "Unknown SharedMemoryType!");
  RefPtr<SharedMemory> segment = new SharedMemoryBasic;
  if (!segment->Create(mappingSize) || !segment->Map(mappingSize)) {
    NS_WARNING("Failed to create or map segment");
    return nullptr;
  }
  return segment;
}

Shmem::Shmem(size_t aNBytes, SharedMemoryType aType, bool aUnsafe)
    : Shmem(AllocSegment(aNBytes, aType), aNBytes, aUnsafe) {}

Shmem::Shmem(RefPtr<SharedMemory> aSegment, size_t aNBytes, bool aUnsafe) {
  if (!aSegment) {
    return;
  }
  size_t mappingSize = MappingSize(aNBytes);
  if (mappingSize != aSegment->Size()) {
    NS_WARNING("Segment has an incorrect size");
    return;
  }

  auto data = ConfigureAndGetData(aSegment);
  MOZ_RELEASE_ASSERT(data.size() >= aNBytes);

  mSegment = aSegment;
  mData = data.data();
  mSize = aNBytes;
#ifdef DEBUG
  mUnsafe = aUnsafe;
#endif
}

#ifdef DEBUG
void Shmem::AssertInvariants() const {
  MOZ_ASSERT(mSegment, "null segment");
  MOZ_ASSERT(mData, "null data pointer");
  MOZ_ASSERT(mSize > 0, "invalid size");
  MOZ_ASSERT(MappingSize(mSize) == mSegment->Size(),
             "size doesn't match segment");

  // if the segment isn't owned by the current process, these will
  // trigger SIGSEGV
  *reinterpret_cast<volatile char*>(mData);
  *(reinterpret_cast<volatile char*>(mData) + mSize - 1);
}

void Shmem::RevokeRights() {
  AssertInvariants();

  // If we're not working with an "unsafe" shared memory, revoke access rights
  // to the entire shared memory region.
  if (!mUnsafe) {
    mSegment->Protect(reinterpret_cast<char*>(mSegment->memory()),
                      mSegment->Size(), RightsNone);
  }

  *this = Shmem();
}
#endif

}  // namespace mozilla::ipc

namespace IPC {

void ParamTraits<mozilla::ipc::Shmem>::Write(MessageWriter* aWriter,
                                             paramType&& aParam) {
  aParam.AssertInvariants();

  MOZ_ASSERT(aParam.mSegment->Type() ==
                 mozilla::ipc::SharedMemory::SharedMemoryType::TYPE_BASIC,
             "Only supported type is TYPE_BASIC");

  WriteParam(aWriter, uint64_t(aParam.mSize));
  aParam.mSegment->WriteHandle(aWriter);
#ifdef DEBUG
  WriteParam(aWriter, aParam.mUnsafe);
#endif

  aParam.RevokeRights();
}

bool ParamTraits<mozilla::ipc::Shmem>::Read(MessageReader* aReader,
                                            paramType* aResult) {
  *aResult = mozilla::ipc::Shmem();

  // Size is sent as uint64_t to deal with IPC between processes with different
  // `size_t` sizes.
  uint64_t rawSize = 0;
  if (!ReadParam(aReader, &rawSize)) {
    return false;
  }
  mozilla::CheckedInt<size_t> size{rawSize};
  if (!size.isValid() || size == 0) {
    return false;
  }

  RefPtr<mozilla::ipc::SharedMemory> segment =
      new mozilla::ipc::SharedMemoryBasic;
  if (!segment->ReadHandle(aReader) ||
      !segment->Map(mozilla::ipc::MappingSize(size.value()))) {
    return false;
  }

  bool unsafe = false;
#ifdef DEBUG
  if (!ReadParam(aReader, &unsafe)) {
    return false;
  }
#endif

  *aResult = mozilla::ipc::Shmem(segment, size.value(), unsafe);
  return aResult->IsValid();
}

}  // namespace IPC
