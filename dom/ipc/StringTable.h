/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_ipc_StringTable_h
#define dom_ipc_StringTable_h

#include "mozilla/RangedPtr.h"
#include "nsDataHashtable.h"

/**
 * This file contains helper classes for creating and accessing compact string
 * tables, which can be used as the building blocks of shared memory databases.
 * Each string table a de-duplicated set of strings which can be referenced
 * using their character offsets within a data block and their lengths. The
 * string tables, once created, cannot be modified, and are primarily useful in
 * read-only shared memory or memory mapped files.
 */

namespace mozilla {
namespace dom {
namespace ipc {

/**
 * Contains the character offset and character length of an entry in a string
 * table. This may be used for either 8-bit or 16-bit strings, and is required
 * to retrieve an entry from a string table.
 */
struct StringTableEntry
{
  uint32_t mOffset;
  uint32_t mLength;

  // Ignore mLength. It must be the same for any two strings with the same
  // offset.
  uint32_t Hash() const { return mOffset; }

  bool operator==(const StringTableEntry& aOther) const
  {
    return mOffset == aOther.mOffset;
  }
};

template <typename StringType>
class StringTable
{
  using ElemType = typename StringType::char_type;

public:
  MOZ_IMPLICIT StringTable(const RangedPtr<uint8_t>& aBuffer)
    : mBuffer(aBuffer.ReinterpretCast<ElemType>())
  {
    MOZ_ASSERT(uintptr_t(aBuffer.get()) % alignof(ElemType) == 0,
               "Got misalinged buffer");
  }

  StringType Get(const StringTableEntry& aEntry) const
  {
    StringType res;
    res.AssignLiteral(GetBare(aEntry), aEntry.mLength);
    return res;
  }

  const ElemType* GetBare(const StringTableEntry& aEntry) const
  {
    return &mBuffer[aEntry.mOffset];
  }

private:
  RangedPtr<ElemType> mBuffer;
};

template <typename KeyType, typename StringType>
class StringTableBuilder
{
public:
  using ElemType = typename StringType::char_type;

  StringTableEntry Add(const StringType& aKey)
  {
    const auto& entry = mEntries.LookupForAdd(aKey).OrInsert([&] () {
      Entry newEntry { mSize, aKey };
      mSize += aKey.Length() + 1;

      return newEntry;
    });

    return { entry.mOffset, aKey.Length() };
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

  static constexpr size_t Alignment() { return alignof(ElemType); }

private:
  struct Entry
  {
    uint32_t mOffset;
    StringType mValue;
  };

  nsDataHashtable<KeyType, Entry> mEntries;
  uint32_t mSize = 0;
};

} // namespace ipc
} // namespace dom
} // namespace mozilla

#endif
