/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PseudoElementHashEntry_h
#define mozilla_PseudoElementHashEntry_h

#include "mozilla/dom/Element.h"
#include "mozilla/HashFunctions.h"
#include "nsCSSPseudoElements.h"
#include "PLDHashTable.h"

namespace mozilla {

struct PseudoElementHashKey
{
  dom::Element* mElement;
  nsCSSPseudoElements::Type mPseudoType;
};

// A hash entry that uses a RefPtr<dom::Element>, nsCSSPseudoElements::Type pair
class PseudoElementHashEntry : public PLDHashEntryHdr
{
public:
  typedef PseudoElementHashKey KeyType;
  typedef const PseudoElementHashKey* KeyTypePointer;

  explicit PseudoElementHashEntry(KeyTypePointer aKey)
    : mElement(aKey->mElement)
    , mPseudoType(aKey->mPseudoType) { }
  explicit PseudoElementHashEntry(const PseudoElementHashEntry& aCopy)=default;

  ~PseudoElementHashEntry() = default;

  KeyType GetKey() const { return {mElement, mPseudoType}; }
  bool KeyEquals(KeyTypePointer aKey) const
  {
    return mElement == aKey->mElement &&
           mPseudoType == aKey->mPseudoType;
  }

  static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    if (!aKey)
      return 0;

    return mozilla::HashGeneric(aKey->mElement, aKey->mPseudoType);
  }
  enum { ALLOW_MEMMOVE = true };

  RefPtr<dom::Element> mElement;
  nsCSSPseudoElements::Type mPseudoType;
};

} // namespace mozilla

#endif // mozilla_PseudoElementHashEntry_h
