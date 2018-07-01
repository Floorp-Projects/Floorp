/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_ipc_SharedStringMap_h
#define dom_ipc_SharedStringMap_h

#include "mozilla/AutoMemMap.h"
#include "mozilla/Result.h"
#include "mozilla/TypeTraits.h"
#include "nsDataHashtable.h"

namespace mozilla {
namespace dom {
namespace ipc {

class SharedStringMapBuilder;

/**
 * This class provides a simple, read-only key-value string store, with all
 * data packed into a single segment of memory, which can be shared between
 * processes.
 *
 * Look-ups are performed by binary search of a static table in the mapped
 * memory region, and all returned strings are literals which reference the
 * mapped data. No copies are performed on instantiation or look-up.
 *
 * Important: The mapped memory created by this class is persistent. Once an
 * instance has been initialized, the memory that it allocates can never be
 * freed before process shutdown. Do not use it for short-lived mappings.
 */
class SharedStringMap
{
  using FileDescriptor = mozilla::ipc::FileDescriptor;

public:
  /**
   * The header at the beginning of the shared memory region describing its
   * layout. The layout of the shared memory is as follows:
   *
   * - Header:
   *   A Header struct describing the contents of the rest of the memory region.
   *
   * - Optional alignment padding for Header[].
   *
   * - Entry[header.mEntryCount]:
   *   An array of Entry structs, one for each entry in the map. Entries are
   *   lexocographically sorted by key.
   *
   * - StringTable<nsCString>:
   *   A region of flat, null-terminated C strings. Entry key strings are
   *   encoded as character offsets into this region.
   *
   * - Optional alignment padding for char16_t[]
   *
   * - StringTable<nsString>:
   *   A region of flat, null-terminated UTF-16 strings. Entry value strings are
   *   encoded as character (*not* byte) offsets into this region.
   */
  struct Header {
    // The number of entries in this map.
    uint32_t mEntryCount;

    // The raw byte offset of the beginning of the key string table, from the
    // start of the shared memory region, and its size in bytes.
    size_t mKeyStringsOffset;
    size_t mKeyStringsSize;

    // The raw byte offset of the beginning of the value string table, from the
    // start of the shared memory region, and its size in bytes (*not*
    // characters).
    size_t mValueStringsOffset;
    size_t mValueStringsSize;
  };

  /**
   * Contains the character offset and character length of an entry in a string
   * table. This may be used for either 8-bit or 16-bit strings, and is required
   * to retrieve an entry from a string table.
   */
  struct StringEntry {
    uint32_t mOffset;
    uint32_t mLength;
  };

  /**
   * Describes a value in the string map, as offsets into the key and value
   * string tables.
   */
  struct Entry {
    // The offset and size of the entry's UTF-8 key in the key string table.
    StringEntry mKey;
    // The offset and size of the entry's UTF-16 value in the value string table.
    StringEntry mValue;
  };

  NS_INLINE_DECL_REFCOUNTING(SharedStringMap)

  // Note: These constructors are infallible on the premise that this class
  // is used primarily in cases where it is critical to platform
  // functionality.
  explicit SharedStringMap(const FileDescriptor&, size_t);
  explicit SharedStringMap(SharedStringMapBuilder&&);

  /**
   * Searches for the given value in the map, and returns true if it exists.
   */
  bool Has(const nsCString& aKey);

  /**
   * Searches for the given value in the map, and, if it exists, returns true
   * and places its value in aValue.
   *
   * The returned value is a literal string which references the mapped memory
   * region.
   */
  bool Get(const nsCString& aKey, nsAString& aValue);

private:
  /**
   * Searches for an entry for the given key. If found, returns true, and
   * places its index in the entry array in aIndex.
   */
  bool Find(const nsCString& aKey, size_t* aIndex);

public:
  /**
   * Returns the number of entries in the map.
   */
  uint32_t Count() const { return EntryCount(); }

  /**
   * Returns the string entry at the given index. Keys are guaranteed to be
   * sorted lexographically.
   *
   * The given index *must* be less than the value returned by Count().
   *
   * The returned value is a literal string which references the mapped memory
   * region.
   */
  nsCString GetKeyAt(uint32_t aIndex) const
  {
    MOZ_ASSERT(aIndex < Count());
    return KeyTable().Get(Entries()[aIndex].mKey);
  }

  /**
   * Returns the string value for the entry at the given index.
   *
   * The given index *must* be less than the value returned by Count().
   *
   * The returned value is a literal string which references the mapped memory
   * region.
   */
  nsString GetValueAt(uint32_t aIndex) const
  {
    MOZ_ASSERT(aIndex < Count());
    return ValueTable().Get(Entries()[aIndex].mValue);
  }

  /**
   * Returns a copy of the read-only file descriptor which backs the shared
   * memory region for this map. The file descriptor may be passed between
   * processes, and used to construct new instances of SharedStringMap with
   * the same data as this instance.
   */
  FileDescriptor CloneFileDescriptor() const;

  size_t MapSize() const { return mMap.size(); }

protected:
  ~SharedStringMap() = default;

private:
  template <typename StringType>
  class StringTable
  {
    using ElemType = decltype(DeclVal<StringType>()[0]);

  public:
    MOZ_IMPLICIT StringTable(const RangedPtr<uint8_t>& aBuffer)
      : mBuffer(aBuffer.ReinterpretCast<ElemType>())
    {
      MOZ_ASSERT(uintptr_t(aBuffer.get()) % alignof(ElemType) == 0,
                 "Got misalinged buffer");
    }

    StringType Get(const StringEntry& aEntry) const
    {
      StringType res;
      res.AssignLiteral(GetBare(aEntry), aEntry.mLength);
      return res;
    }

    const ElemType* GetBare(const StringEntry& aEntry) const
    {
      return &mBuffer[aEntry.mOffset];
    }

  private:
    RangedPtr<ElemType> mBuffer;
  };


  // Type-safe getters for values in the shared memory region:
  const Header& GetHeader() const
  {
    return mMap.get<Header>()[0];
  }

  RangedPtr<const Entry> Entries() const
  {
    return { reinterpret_cast<const Entry*>(&GetHeader() + 1),
             EntryCount() };
  }

  uint32_t EntryCount() const
  {
    return GetHeader().mEntryCount;
  }

  StringTable<nsCString> KeyTable() const
  {
    auto& header = GetHeader();
    return { { &mMap.get<uint8_t>()[header.mKeyStringsOffset],
               header.mKeyStringsSize } };
  }

  StringTable<nsString> ValueTable() const
  {
    auto& header = GetHeader();
    return { { &mMap.get<uint8_t>()[header.mValueStringsOffset],
               header.mValueStringsSize } };
  }


  loader::AutoMemMap mMap;
};

/**
 * A helper class which builds the contiguous look-up table used by
 * SharedStringMap. Each key-value pair in the final map is added to the
 * builder, before it is finalized and transformed into a snapshot.
 */
class MOZ_RAII SharedStringMapBuilder
{
public:
  SharedStringMapBuilder() = default;

  /**
   * Adds a key-value pair to the map.
   */
  void Add(const nsCString& aKey, const nsString& aValue);

  /**
   * Finalizes the binary representation of the map, writes it to a shared
   * memory region, and then initializes the given AutoMemMap with a reference
   * to the read-only copy of it.
   */
  Result<Ok, nsresult> Finalize(loader::AutoMemMap& aMap);

private:
  template <typename KeyType, typename StringType>
  class StringTableBuilder
  {
  public:
    using ElemType = typename StringType::char_type;

    uint32_t Add(const StringType& aKey)
    {
      auto entry = mEntries.LookupForAdd(aKey).OrInsert([&] () {
        Entry newEntry { mSize, aKey };
        mSize += aKey.Length() + 1;

        return newEntry;
      });

      return entry.mOffset;
    }

    void Write(const RangedPtr<uint8_t>& aBuffer)
    {
      auto buffer = aBuffer.ReinterpretCast<ElemType>();

      for (auto iter = mEntries.Iter(); !iter.Done(); iter.Next()) {
        auto& entry = iter.Data();
        memcpy(&buffer[entry.mOffset], entry.mValue.BeginReading(),
               sizeof(ElemType) * (entry.mValue.Length() + 1));
      }
    }

    uint32_t Count() const { return mEntries.Count(); }

    uint32_t Size() const { return mSize * sizeof(ElemType); }

    void Clear() { mEntries.Clear(); }

  private:
    struct Entry
    {
      uint32_t mOffset;
      StringType mValue;
    };

    nsDataHashtable<KeyType, Entry> mEntries;
    uint32_t mSize = 0;
  };

  using Entry = SharedStringMap::Entry;

  StringTableBuilder<nsCStringHashKey, nsCString> mKeyTable;
  StringTableBuilder<nsStringHashKey, nsString> mValueTable;

  nsDataHashtable<nsCStringHashKey, Entry> mEntries;
};

} // ipc
} // dom
} // mozilla

#endif // dom_ipc_SharedStringMap_h

