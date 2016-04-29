/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Class for representing MozMap arguments.  This is an nsTHashtable
 * under the hood, but we don't want to leak that implementation
 * detail.
 */

#ifndef mozilla_dom_MozMap_h
#define mozilla_dom_MozMap_h

#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/Move.h"

namespace mozilla {
namespace dom {

namespace binding_detail {
template<typename DataType>
class MozMapEntry : public nsStringHashKey
{
public:
  explicit MozMapEntry(const nsAString* aKeyTypePointer)
    : nsStringHashKey(aKeyTypePointer)
  {
  }

  // Move constructor so we can do MozMaps of MozMaps.
  MozMapEntry(MozMapEntry<DataType>&& aOther)
    : nsStringHashKey(aOther),
      mData(Move(aOther.mData))
  {
  }

  DataType mData;
};

} // namespace binding_detail

template<typename DataType>
class MozMap : protected nsTHashtable<binding_detail::MozMapEntry<DataType>>
{
public:
  typedef typename binding_detail::MozMapEntry<DataType> EntryType;
  typedef nsTHashtable<EntryType> Base;
  typedef MozMap<DataType> SelfType;

  MozMap()
  {
  }

  // Move constructor so we can do MozMap of MozMap.
  MozMap(SelfType&& aOther) :
    Base(Move(aOther))
  {
  }

  // The return value is only safe to use until an AddEntry call.
  const DataType& Get(const nsAString& aKey) const
  {
    const EntryType* ent = this->GetEntry(aKey);
    MOZ_ASSERT(ent, "Why are you using a key we didn't claim to have?");
    return ent->mData;
  }

  DataType& Get(const nsAString& aKey)
  {
    EntryType* ent = this->GetEntry(aKey);
    MOZ_ASSERT(ent, "Why are you using a key we didn't claim to have?");
    return ent->mData;
  }

  // The return value is only safe to use until an AddEntry call.
  const DataType* GetIfExists(const nsAString& aKey) const
  {
    const EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return nullptr;
    }
    return &ent->mData;
  }

  void GetKeys(nsTArray<nsString>& aKeys) const {
    for (auto iter = this->ConstIter(); !iter.Done(); iter.Next()) {
      aKeys.AppendElement(iter.Get()->GetKey());
    }
  }

  // XXXbz we expose this generic enumerator for tracing.  Otherwise we'd end up
  // with a dependency on BindingUtils.h here for the SequenceTracer bits.
  typedef void (* Enumerator)(DataType* aValue, void* aClosure);
  void EnumerateValues(Enumerator aEnumerator, void *aClosure)
  {
    for (auto iter = this->Iter(); !iter.Done(); iter.Next()) {
      aEnumerator(&iter.Get()->mData, aClosure);
    }
  }

  MOZ_MUST_USE
  DataType* AddEntry(const nsAString& aKey)
  {
    EntryType* ent = this->PutEntry(aKey, fallible);
    if (!ent) {
      return nullptr;
    }
    return &ent->mData;
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MozMap_h
