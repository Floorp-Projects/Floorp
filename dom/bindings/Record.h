/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Class for representing record arguments.  Basically an array under the hood.
 */

#ifndef mozilla_dom_Record_h
#define mozilla_dom_Record_h

#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/Move.h"

namespace mozilla {
namespace dom {

namespace binding_detail {
template<typename KeyType, typename ValueType>
class RecordEntry
{
public:
  RecordEntry()
  {
  }

  // Move constructor so we can do Records of Records.
  RecordEntry(RecordEntry<KeyType, ValueType>&& aOther)
    : mKey(std::move(aOther.mKey)),
      mValue(std::move(aOther.mValue))
  {
  }

  KeyType mKey;
  ValueType mValue;
};

} // namespace binding_detail

template<typename KeyType, typename ValueType>
class Record
{
public:
  typedef typename binding_detail::RecordEntry<KeyType, ValueType> EntryType;
  typedef Record<KeyType, ValueType> SelfType;

  Record()
  {
  }

  // Move constructor so we can do Record of Record.
  Record(SelfType&& aOther) :
    mEntries(std::move(aOther.mEntries))
  {
  }

  const nsTArray<EntryType>& Entries() const
  {
    return mEntries;
  }

  nsTArray<EntryType>& Entries()
  {
    return mEntries;
  }

private:
  nsTArray<EntryType> mEntries;
};

} // namespace dom
} // namespace mozilla

template<typename K, typename V>
class nsDefaultComparator<mozilla::dom::binding_detail::RecordEntry<K, V>, K>
{
public:
  bool Equals(const mozilla::dom::binding_detail::RecordEntry<K, V>& aEntry,
              const K& aKey) const
  {
    return aEntry.mKey == aKey;
  }
};

#endif // mozilla_dom_Record_h
