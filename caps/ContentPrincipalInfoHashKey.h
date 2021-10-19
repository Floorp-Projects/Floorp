/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAPS_PRINCIPALHASHKEY_H_
#define CAPS_PRINCIPALHASHKEY_H_

#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "PLDHashTable.h"
#include "nsHashKeys.h"
#include "nsUnicharUtils.h"

namespace mozilla {

class ContentPrincipalInfoHashKey : public PLDHashEntryHdr {
 public:
  using KeyType = const ipc::ContentPrincipalInfo&;
  using KeyTypePointer = const ipc::ContentPrincipalInfo*;

  explicit ContentPrincipalInfoHashKey(KeyTypePointer aKey)
      : mPrincipalInfo(*aKey) {
    MOZ_COUNT_CTOR(ContentPrincipalInfoHashKey);
  }
  ContentPrincipalInfoHashKey(ContentPrincipalInfoHashKey&& aOther) noexcept
      : mPrincipalInfo(aOther.mPrincipalInfo) {
    MOZ_COUNT_CTOR(ContentPrincipalInfoHashKey);
  }

  MOZ_COUNTED_DTOR(ContentPrincipalInfoHashKey)

  KeyType GetKey() const { return mPrincipalInfo; }

  bool KeyEquals(KeyTypePointer aKey) const {
    // Mocks BasePrincipal::FastEquals()
    return mPrincipalInfo.originNoSuffix() == aKey->originNoSuffix() &&
           mPrincipalInfo.attrs() == aKey->attrs();
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    nsAutoCString suffix;
    aKey->attrs().CreateSuffix(suffix);
    return HashGeneric(HashString(aKey->originNoSuffix()), HashString(suffix));
  }

  enum { ALLOW_MEMMOVE = true };

 protected:
  const ipc::ContentPrincipalInfo mPrincipalInfo;
};

}  // namespace mozilla

#endif  // CAPS_PRINCIPALHASHKEY_H_
