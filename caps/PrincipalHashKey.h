/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PrincipalHashKey_h
#define mozilla_PrincipalHashKey_h

#include "BasePrincipal.h"
#include "PLDHashTable.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"

namespace mozilla {

class PrincipalHashKey : public PLDHashEntryHdr {
 public:
  using KeyType = nsIPrincipal*;
  using KeyTypePointer = const nsIPrincipal*;

  explicit PrincipalHashKey(const nsIPrincipal* aKey)
      : mPrincipal(const_cast<nsIPrincipal*>(aKey)) {
    MOZ_ASSERT(aKey);
    MOZ_COUNT_CTOR(PrincipalHashKey);
  }
  PrincipalHashKey(PrincipalHashKey&& aKey) noexcept
      : mPrincipal(std::move(aKey.mPrincipal)) {
    MOZ_COUNT_CTOR(PrincipalHashKey);
  }

  MOZ_COUNTED_DTOR(PrincipalHashKey)

  nsIPrincipal* GetKey() const { return mPrincipal; }

  bool KeyEquals(const nsIPrincipal* aKey) const {
    return BasePrincipal::Cast(mPrincipal)
        ->FastEquals(const_cast<nsIPrincipal*>(aKey));
  }

  static const nsIPrincipal* KeyToPointer(const nsIPrincipal* aKey) {
    return aKey;
  }
  static PLDHashNumber HashKey(const nsIPrincipal* aKey) {
    const auto* bp = BasePrincipal::Cast(aKey);
    return HashGeneric(bp->GetOriginNoSuffixHash(), bp->GetOriginSuffixHash());
  }

  enum { ALLOW_MEMMOVE = true };

 protected:
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

}  // namespace mozilla

#endif
