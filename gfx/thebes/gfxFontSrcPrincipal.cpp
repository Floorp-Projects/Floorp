/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontSrcPrincipal.h"

#include "nsURIHashKey.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/HashFunctions.h"

using mozilla::BasePrincipal;

gfxFontSrcPrincipal::gfxFontSrcPrincipal(nsIPrincipal* aNodePrincipal,
                                         nsIPrincipal* aStoragePrincipal)
    : mNodePrincipal(aNodePrincipal),
      mStoragePrincipal(mozilla::StaticPrefs::privacy_partition_network_state()
                            ? aStoragePrincipal
                            : aNodePrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aNodePrincipal);
  MOZ_ASSERT(aStoragePrincipal);

  nsAutoCString suffix;
  mStoragePrincipal->GetOriginSuffix(suffix);

  mHash = mozilla::AddToHash(mStoragePrincipal->GetHashValue(),
                             mozilla::HashString(suffix));
}

gfxFontSrcPrincipal::~gfxFontSrcPrincipal() = default;

bool gfxFontSrcPrincipal::Equals(gfxFontSrcPrincipal* aOther) {
  return BasePrincipal::Cast(mStoragePrincipal)
      ->FastEquals(BasePrincipal::Cast(aOther->mStoragePrincipal));
}
