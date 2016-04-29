/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PseudoElementHashEntry_h
#define mozilla_PseudoElementHashEntry_h

#include "mozilla/dom/Element.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/HashFunctions.h"
#include "PLDHashTable.h"

namespace mozilla {

// A hash entry that uses a RefPtr<dom::Element>, CSSPseudoElementType pair
class PseudoElementHashEntry : public PLDHashEntryHdr
{
public:
  typedef NonOwningAnimationTarget KeyType;
  typedef const NonOwningAnimationTarget* KeyTypePointer;

  explicit PseudoElementHashEntry(KeyTypePointer aKey)
    : mElement(aKey->mElement)
    , mPseudoType(aKey->mPseudoType) { }
  explicit PseudoElementHashEntry(const PseudoElementHashEntry& aCopy)=default;

  ~PseudoElementHashEntry() = default;

  KeyType GetKey() const { return { mElement, mPseudoType }; }
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

    // Convert the scoped enum into an integer while adding it to hash.
    // Note: CSSPseudoElementTypeBase is uint8_t, so we convert it into
    //       uint8_t directly to avoid including the header.
    return mozilla::HashGeneric(aKey->mElement,
                                static_cast<uint8_t>(aKey->mPseudoType));
  }
  enum { ALLOW_MEMMOVE = true };

  RefPtr<dom::Element> mElement;
  CSSPseudoElementType mPseudoType;
};

} // namespace mozilla

#endif // mozilla_PseudoElementHashEntry_h
