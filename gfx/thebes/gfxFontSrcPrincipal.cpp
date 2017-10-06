/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontSrcPrincipal.h"

#include "nsProxyRelease.h"
#include "nsURIHashKey.h"
#include "mozilla/BasePrincipal.h"

using mozilla::BasePrincipal;

gfxFontSrcPrincipal::gfxFontSrcPrincipal(nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  mPrincipal = aPrincipal;

  uint32_t hash = 0;
  mPrincipal->GetHashValue(&hash);
  mHash = hash;
}

gfxFontSrcPrincipal::~gfxFontSrcPrincipal()
{
  NS_ReleaseOnMainThreadSystemGroup("gfxFontSrcPrincipal::mPrincipal",
                                    mPrincipal.forget());
}

bool
gfxFontSrcPrincipal::Equals(gfxFontSrcPrincipal* aOther)
{
  return BasePrincipal::Cast(mPrincipal)->
           FastEquals(BasePrincipal::Cast(aOther->mPrincipal));
}
