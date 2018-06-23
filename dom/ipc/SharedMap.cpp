/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedMap.h"

#include "MemMapSnapshot.h"
#include "ScriptPreloader-inl.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ProcessGlobal.h"

using namespace mozilla::loader;

namespace mozilla {

using namespace ipc;

namespace dom {
namespace ipc {

// Align to size of uintptr_t here, to be safe. It's probably not strictly
// necessary, though.
constexpr size_t kStructuredCloneAlign = sizeof(uintptr_t);


static inline void
AlignTo(size_t* aOffset, size_t aAlign)
{
  if (auto mod = *aOffset % aAlign) {
    *aOffset += aAlign - mod;
  }
}


SharedMap::SharedMap()
{}

SharedMap::SharedMap(nsIGlobalObject* aGlobal, const FileDescriptor& aMapFile,
                     size_t aMapSize)
{
  mMapFile.reset(new FileDescriptor(aMapFile));
  mMapSize = aMapSize;
}


bool
SharedMap::Has(const nsACString& aName)
{
  return mEntries.Contains(aName);
}

void
SharedMap::Get(JSContext* aCx,
               const nsACString& aName,
               JS::MutableHandleValue aRetVal,
               ErrorResult& aRv)
{
  auto res = MaybeRebuild();
  if (res.isErr()) {
    aRv.Throw(res.unwrapErr());
    return;
  }

  Entry* entry = mEntries.Get(aName);
  if (!entry) {
    aRetVal.setNull();
    return;
  }

  entry->Read(aCx, aRetVal, aRv);
}

void
SharedMap::Entry::Read(JSContext* aCx,
                       JS::MutableHandleValue aRetVal,
                       ErrorResult& aRv)
{
  if (mData.is<StructuredCloneData>()) {
    // We have a temporary buffer for a key that was changed after the last
    // snapshot. Just decode it directly.
    auto& holder = mData.as<StructuredCloneData>();
    holder.Read(aCx, aRetVal, aRv);
    return;
  }

  // We have a pointer to a shared memory region containing our structured
  // clone data. Create a temporary buffer to decode that data, and then
  // discard it so that we don't keep a separate process-local copy around any
  // longer than necessary.
  StructuredCloneData holder;
  if (!holder.CopyExternalData(Data(), Size())) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
  }

  holder.Read(aCx, aRetVal, aRv);
}

FileDescriptor
SharedMap::CloneMapFile()
{
  if (mMap.initialized()) {
    return mMap.cloneHandle();
  }
  return *mMapFile;
}

void
SharedMap::Update(const FileDescriptor& aMapFile, size_t aMapSize,
                  nsTArray<nsCString>&& aChangedKeys)
{
  MOZ_DIAGNOSTIC_ASSERT(!mWritable);

  mMap.reset();
  if (mMapFile) {
    *mMapFile = aMapFile;
  } else {
    mMapFile.reset(new FileDescriptor(aMapFile));
  }
  mMapSize = aMapSize;
  mEntries.Clear();
}

void
SharedMap::Entry::TakeData(StructuredCloneData&& aHolder)
{
  mData = AsVariant(std::move(aHolder));

  mSize = Holder().Data().Size();
}

void
SharedMap::Entry::ExtractData(char* aDestPtr, uint32_t aNewOffset)
{
  if (mData.is<StructuredCloneData>()) {
    char* ptr = aDestPtr;
    Holder().Data().ForEachDataChunk([&](const char* aData, size_t aSize) {
        memcpy(ptr, aData, aSize);
        ptr += aSize;
        return true;
    });
    MOZ_ASSERT(ptr - aDestPtr == mSize);
  } else {
    memcpy(aDestPtr, Data(), mSize);
  }

  mData = AsVariant(aNewOffset);
}

Result<Ok, nsresult>
SharedMap::MaybeRebuild()
{
  if (!mMapFile) {
    return Ok();
  }

  // This function maps a shared memory region created by Serialize() and reads
  // its header block to build a new mEntries hashtable of its contents.
  //
  // The entries created by this function contain a pointer to this SharedMap
  // instance, and the offsets and sizes of their structured clone data within
  // its shared memory region. When needed, that structured clone data is
  // retrieved directly as indexes into the SharedMap's shared memory region.

  MOZ_TRY(mMap.initWithHandle(*mMapFile, mMapSize));
  mMapFile.reset();

  // We should be able to pass this range as an initializer list or an immediate
  // param, but gcc currently chokes on that if optimization is enabled, and
  // initializes everything to 0.
  Range<uint8_t> range(&mMap.get<uint8_t>()[0], mMap.size());
  InputBuffer buffer(range);

  uint32_t count;
  buffer.codeUint32(count);

  for (uint32_t i = 0; i < count; i++) {
    auto entry = MakeUnique<Entry>(*this);
    entry->Code(buffer);

    // This buffer was created at runtime, during this session, so any errors
    // indicate memory corruption, and are fatal.
    MOZ_RELEASE_ASSERT(!buffer.error());

    // Note: Order of evaluation of function arguments is not guaranteed, so we
    // can't use entry.release() in place of entry.get() without entry->Name()
    // sometimes resulting in a null dereference.
    mEntries.Put(entry->Name(), entry.get());
    Unused << entry.release();
  }

  return Ok();
}

void
SharedMap::MaybeRebuild() const
{
  Unused << const_cast<SharedMap*>(this)->MaybeRebuild();
}

WritableSharedMap::WritableSharedMap()
  : SharedMap()
{
  mWritable = true;
  // Serialize the initial empty contents of the map immediately so that we
  // always have a file descriptor to send to callers of CloneMapFile().
  Unused << Serialize();
  MOZ_RELEASE_ASSERT(mMap.initialized());
}

SharedMap*
WritableSharedMap::GetReadOnly()
{
  if (!mReadOnly) {
    mReadOnly = new SharedMap(ProcessGlobal::Get(), CloneMapFile(),
                              MapSize());
  }
  return mReadOnly;
}

Result<Ok, nsresult>
WritableSharedMap::Serialize()
{
  // Serializes a new snapshot of the map, initializes a new read-only shared
  // memory region with its contents, and updates all entries to point to that
  // new snapshot.
  //
  // The layout of the snapshot is as follows:
  //
  // - A header containing a uint32 count field containing the number of
  //   entries in the map, followed by that number of serialized entry headers,
  //   as produced by Entry::Code.
  //
  // - A data block containing structured clone data for each of the entries'
  //   values. This data is referenced by absolute byte offsets from the start
  //   of the shared memory region, encoded in each of the entry header values.
  //   Each entry's data is aligned to kStructuredCloneAlign, and therefore may
  //   have alignment padding before it.
  //
  // This serialization format is decoded by the MaybeRebuild() method of
  // read-only SharedMap() instances, and used to populate their mEntries
  // hashtables.
  //
  // Writable instances never read the header blocks, but instead directly
  // update their Entry instances to point to the appropriate offsets in the
  // shared memory region created by this function.

  uint32_t count = mEntries.Count();

  size_t dataSize = 0;
  size_t headerSize = sizeof(count);

  for (auto& entry : IterHash(mEntries)) {
    headerSize += entry->HeaderSize();

    dataSize += entry->Size();
    AlignTo(&dataSize, kStructuredCloneAlign);
  }

  size_t offset = headerSize;
  AlignTo(&offset, kStructuredCloneAlign);

  OutputBuffer header;
  header.codeUint32(count);

  MemMapSnapshot mem;
  MOZ_TRY(mem.Init(offset + dataSize));

  auto ptr = mem.Get<char>();

  for (auto& entry : IterHash(mEntries)) {
    AlignTo(&offset, kStructuredCloneAlign);

    entry->ExtractData(&ptr[offset], offset);
    entry->Code(header);

    offset += entry->Size();
  }

  // FIXME: We should create a separate OutputBuffer class which can encode to
  // a static memory region rather than dynamically allocating and then
  // copying.
  memcpy(ptr.get(), header.Get(), header.cursor());

  // We've already updated offsets at this point. We need this to succeed.
  mMap.reset();
  MOZ_RELEASE_ASSERT(mem.Finalize(mMap).isOk());

  return Ok();
}

void
WritableSharedMap::BroadcastChanges()
{
  if (mChangedKeys.IsEmpty()) {
    return;
  }

  if (!Serialize().isOk()) {
    return;
  }

  nsTArray<ContentParent*> parents;
  ContentParent::GetAll(parents);
  for (auto& parent : parents) {
    Unused << parent->SendUpdateSharedData(CloneMapFile(), mMap.size(),
                                           mChangedKeys);
  }

  if (mReadOnly) {
    mReadOnly->Update(CloneMapFile(), mMap.size(),
                      std::move(mChangedKeys));
  }

  mChangedKeys.Clear();
}

void
WritableSharedMap::Delete(const nsACString& aName)
{
  if (mEntries.Remove(aName)) {
    KeyChanged(aName);
  }
}

void
WritableSharedMap::Set(JSContext* aCx,
                       const nsACString& aName,
                       JS::HandleValue aValue,
                       ErrorResult& aRv)
{
  StructuredCloneData holder;

  holder.Write(aCx, aValue, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (!holder.BlobImpls().IsEmpty() ||
      !holder.InputStreams().IsEmpty()) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Entry* entry = mEntries.LookupOrAdd(aName, *this, aName);
  entry->TakeData(std::move(holder));

  KeyChanged(aName);
}

void
WritableSharedMap::Flush()
{
  BroadcastChanges();
}

void
WritableSharedMap::KeyChanged(const nsACString& aName)
{
  if (!mChangedKeys.ContainsSorted(aName)) {
    mChangedKeys.InsertElementSorted(aName);
  }
}

NS_IMPL_ISUPPORTS0(SharedMap)

} // ipc
} // dom
} // mozilla
