/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_ipc_SharedMap_h
#define dom_ipc_SharedMap_h

#include "mozilla/dom/MozSharedMapBinding.h"

#include "mozilla/AutoMemMap.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {
namespace ipc {

/**
 * Together, the SharedMap and WritableSharedMap classes allow sharing a
 * dynamically-updated, shared-memory key-value store across processes.
 *
 * The maps may only ever be updated in the parent process, via
 * WritableSharedMap instances. When that map changes, its entire contents are
 * serialized into a contiguous shared memory buffer, and broadcast to all child
 * processes, which in turn update their entire map contents wholesale.
 *
 * Keys are arbitrary UTF-8 strings (currently exposed to JavaScript as UTF-16),
 * and values are structured clone buffers. Values are eagerly encoded whenever
 * they are updated, and lazily decoded each time they're read.
 *
 * Updates are batched. Rather than each key change triggering an immediate
 * update, combined updates are broadcast after a delay. Currently, this
 * requires an explicit flush() call, or spawning a new content process. In the
 * future, it will happen automatically in idle tasks.
 *
 *
 * Whenever a read-only SharedMap is updated, it dispatches a "change" event.
 * The event contains a "changedKeys" property with a list of all keys which
 * were changed in the last update batch. Change events are never dispatched to
 * WritableSharedMap instances.
 */
class SharedMap : public DOMEventTargetHelper
{
  using FileDescriptor = mozilla::ipc::FileDescriptor;

public:

  SharedMap();

  SharedMap(nsIGlobalObject* aGlobal, const FileDescriptor&, size_t);

  // Returns true if the map contains the given (UTF-8) key.
  bool Has(const nsACString& name);

  // If the map contains the given (UTF-8) key, decodes and returns a new copy
  // of its value. Otherwise returns null.
  void Get(JSContext* cx, const nsACString& name, JS::MutableHandleValue aRetVal,
           ErrorResult& aRv);


  // Conversion helpers for WebIDL callers
  bool Has(const nsAString& aName)
  {
    return Has(NS_ConvertUTF16toUTF8(aName));
  }

  void Get(JSContext* aCx, const nsAString& aName, JS::MutableHandleValue aRetVal,
           ErrorResult& aRv)
  {
    return Get(aCx, NS_ConvertUTF16toUTF8(aName), aRetVal, aRv);
  }


  /**
   * WebIDL iterator glue.
   */
  uint32_t GetIterableLength() const
  {
    return EntryArray().Length();
  }

  /**
   * These functions return the key or value, respectively, at the given index.
   * The index *must* be less than the value returned by GetIterableLength(), or
   * the program will crash.
   */
  const nsString GetKeyAtIndex(uint32_t aIndex) const;
  // Note: This function should only be called if the instance has a live,
  // cached wrapper. If it does not, this function will return null, and assert
  // in debug builds.
  // The returned value will always be in the same Realm as that wrapper.
  JS::Value GetValueAtIndex(uint32_t aIndex) const;


  /**
   * Returns a copy of the read-only file descriptor which backs the shared
   * memory region for this map. The file descriptor may be passed between
   * processes, and used to update corresponding instances in child processes.
   */
  FileDescriptor CloneMapFile();

  /**
   * Returns the size of the memory mapped region that backs this map. Must be
   * passed to the SharedMap() constructor or Update() method along with the
   * descriptor returned by CloneMapFile() in order to initialize or update a
   * child SharedMap.
   */
  size_t MapSize() const { return mMap.size(); }

  /**
   * Updates this instance to reflect the contents of the shared memory region
   * in the given map file, and broadcasts a change event for the given set of
   * changed (UTF-8-encoded) keys.
   */
  void Update(const FileDescriptor& aMapFile, size_t aMapSize,
              nsTArray<nsCString>&& aChangedKeys);


  JSObject* WrapObject(JSContext* aCx, JS::HandleObject aGivenProto) override;

protected:
  ~SharedMap() override = default;

  class Entry
  {
  public:
    Entry(Entry&&) = delete;

    explicit Entry(SharedMap& aMap, const nsACString& aName = EmptyCString())
      : mMap(aMap)
      , mName(aName)
      , mData(AsVariant(uint32_t(0)))
    {
    }

    ~Entry() = default;

    /**
     * Encodes or decodes this entry into or from the given OutputBuffer or
     * InputBuffer.
     */
    template<typename Buffer>
    void Code(Buffer& buffer)
    {
      DebugOnly<size_t> startOffset = buffer.cursor();

      buffer.codeString(mName);
      buffer.codeUint32(DataOffset());
      buffer.codeUint32(mSize);

      MOZ_ASSERT(buffer.cursor() == startOffset + HeaderSize());
    }

    /**
     * Returns the size that this entry will take up in the map header. This
     * must be equal to the number of bytes encoded by Code().
     */
    size_t HeaderSize() const
    {
      return (sizeof(uint16_t) + mName.Length() +
              sizeof(DataOffset()) +
              sizeof(mSize));
    }

    /**
     * Updates the value of this entry to the given structured clone data, of
     * which it takes ownership. The passed StructuredCloneData object must not
     * be used after this call.
     */
    void TakeData(StructuredCloneData&&);

    /**
     * This is called while building a new snapshot of the SharedMap. aDestPtr
     * must point to a buffer within the new snapshot with Size() bytes reserved
     * for it, and `aNewOffset` must be the offset of that buffer from the start
     * of the snapshot's memory region.
     *
     * This function copies the raw structured clone data for the entry's value
     * to the new buffer, and updates its internal state for use with the new
     * data. Its offset is updated to aNewOffset, and any StructuredCloneData
     * object it holds is destroyed.
     *
     * After this call, the entry is only valid in reference to the new
     * snapshot, and must not be accessed again until the SharedMap mMap has been
     * updated to point to it.
     */
    void ExtractData(char* aDestPtr, uint32_t aNewOffset);

    // Returns the UTF-8-encoded name of the entry, which is used as its key in
    // the map.
    const nsCString& Name() const { return mName; }

    // Decodes the entry's value into the current Realm of the given JS context
    // and puts the result in aRetVal on success.
    void Read(JSContext* aCx, JS::MutableHandleValue aRetVal,
              ErrorResult& aRv);

    // Returns the byte size of the entry's raw structured clone data.
    uint32_t Size() const { return mSize; }

  private:
    // Returns a pointer to the entry value's structured clone data within the
    // SharedMap's mapped memory region. This is *only* valid shen mData
    // contains a uint32_t.
    const char* Data() const
    {
      return mMap.Data() + DataOffset();
    }

    // Returns the offset of the entry value's structured clone data within the
    // SharedMap's mapped memory region. This is *only* valid shen mData
    // contains a uint32_t.
    uint32_t& DataOffset()
    {
      return mData.as<uint32_t>();
    }
    const uint32_t& DataOffset() const
    {
      return mData.as<uint32_t>();
    }

    // Returns the temporary StructuredCloneData object containing the entry's
    // value. This is *only* value when mData contains a StructuredCloneDAta
    // object.
    const StructuredCloneData& Holder() const
    {
      return mData.as<StructuredCloneData>();
    }

    SharedMap& mMap;

    // The entry's (UTF-8 encoded) name, which serves as its key in the map.
    nsCString mName;

    /**
     * This member provides a reference to the entry's structured clone data.
     * Its type varies depending on the state of the entry:
     *
     * - For entries which have been snapshotted into a shared memory region,
     *   this is a uint32_t offset into the parent SharedMap's Data() buffer.
     *
     * - For entries which have been changed in a WritableSharedMap instance,
     *   but not serialized to a shared memory snapshot yet, this is a
     *   StructuredCloneData instance, containing a process-local copy of the
     *   data. This will be discarded the next time the map is serialized, and
     *   replaced with a buffer offset, as described above.
     */
    Variant<uint32_t, StructuredCloneData> mData;

    // The size, in bytes, of the entry's structured clone data.
    uint32_t mSize = 0;
  };

  const nsTArray<Entry*>& EntryArray() const;

  // Rebuilds the entry hashtable mEntries from the values serialized in the
  // current snapshot, if necessary. The hashtable is rebuilt lazily after
  // construction and after every Update() call, so this function must be called
  // before any attempt to access mEntries.
  Result<Ok, nsresult> MaybeRebuild();
  void MaybeRebuild() const;

  // Note: This header is included by WebIDL binding headers, and therefore
  // can't include "windows.h". Since FileDescriptor.h does include "windows.h"
  // on Windows, we can only forward declare FileDescriptor, and can't include
  // it as an inline member.
  UniquePtr<FileDescriptor> mMapFile;
  // The size of the memory-mapped region backed by mMapFile, in bytes.
  size_t mMapSize = 0;

  mutable nsClassHashtable<nsCStringHashKey, Entry> mEntries;
  mutable Maybe<nsTArray<Entry*>> mEntryArray;

  // Manages the memory mapping of the current snapshot. This is initialized
  // lazily after each SharedMap construction or updated, based on the values in
  // mMapFile and mMapSize.
  loader::AutoMemMap mMap;

  bool mWritable = false;

  // Returns a pointer to the beginning of the memory mapped snapshot. Entry
  // offsets are relative to this pointer, and Entry objects access their
  // structured clone data by indexing this pointer.
  char* Data() { return mMap.get<char>().get(); }
};

class WritableSharedMap final : public SharedMap
{
public:

  WritableSharedMap();

  // Sets the value of the given (UTF-8 encoded) key to a structured clone
  // snapshot of the given value.
  void Set(JSContext* cx, const nsACString& name, JS::HandleValue value, ErrorResult& aRv);

  // Deletes the given (UTF-8 encoded) key from the map.
  void Delete(const nsACString& name);


  // Conversion helpers for WebIDL callers
  void Set(JSContext* aCx, const nsAString& aName, JS::HandleValue aValue, ErrorResult& aRv)
  {
    return Set(aCx, NS_ConvertUTF16toUTF8(aName), aValue, aRv);
  }

  void Delete(const nsAString& aName)
  {
    return Delete(NS_ConvertUTF16toUTF8(aName));
  }


  // Flushes any queued changes to a new snapshot, and broadcasts it to all
  // child SharedMap instances.
  void Flush();


  /**
   * Returns the read-only SharedMap instance corresponding to this
   * WritableSharedMap for use in the parent process.
   */
  SharedMap* GetReadOnly();


  JSObject* WrapObject(JSContext* aCx, JS::HandleObject aGivenProto) override;

protected:
  ~WritableSharedMap() override = default;

private:
  // The set of (UTF-8 encoded) keys which have changed, or been deleted, since
  // the last snapshot.
  nsTArray<nsCString> mChangedKeys;

  RefPtr<SharedMap> mReadOnly;

  // Creates a new snapshot of the map, and updates all Entry instance to
  // reference its data.
  Result<Ok, nsresult> Serialize();

  // If there have been any changes since the last snapshot, creates a new
  // serialization and broadcasts it to all child SharedMap instances.
  void BroadcastChanges();

  // Marks the given (UTF-8 encoded) key as having changed. This adds it to
  // mChangedKeys, if not already present. In the future, it will also schedule
  // a flush the next time the event loop is idle.
  void KeyChanged(const nsACString& aName);
};

} // ipc
} // dom
} // mozilla

#endif // dom_ipc_SharedMap_h
