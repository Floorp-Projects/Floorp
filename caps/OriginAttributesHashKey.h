/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_OriginAttributesHashKey_h
#define mozilla_OriginAttributesHashKey_h

#include "OriginAttributes.h"
#include "PLDHashTable.h"
#include "nsHashKeys.h"

namespace mozilla {

class OriginAttributesHashKey : public PLDHashEntryHdr {
 public:
  using KeyType = OriginAttributes;
  using KeyTypeRef = const KeyType&;
  using KeyTypePointer = const KeyType*;

  explicit OriginAttributesHashKey(KeyTypePointer aKey) {
    mOriginAttributes = *aKey;
    MOZ_COUNT_CTOR(OriginAttributesHashKey);
  }
  OriginAttributesHashKey(OriginAttributesHashKey&& aKey) noexcept {
    mOriginAttributes = std::move(aKey.mOriginAttributes);
    MOZ_COUNT_CTOR(OriginAttributesHashKey);
  }

  MOZ_COUNTED_DTOR(OriginAttributesHashKey)

  KeyTypeRef GetKey() const { return mOriginAttributes; }

  bool KeyEquals(KeyTypePointer aKey) const {
    return mOriginAttributes == *aKey;
  }

  static KeyTypePointer KeyToPointer(KeyTypeRef aKey) { return &aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    nsAutoCString suffix;
    aKey->CreateSuffix(suffix);
    return mozilla::HashString(suffix);
  }

  enum { ALLOW_MEMMOVE = true };

 protected:
  OriginAttributes mOriginAttributes;
};

}  // namespace mozilla

#endif
