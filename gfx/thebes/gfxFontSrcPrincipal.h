/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FONTSRCPRINCIPAL_H
#define MOZILLA_GFX_FONTSRCPRINCIPAL_H

#include "nsCOMPtr.h"
#include "nsIPrincipal.h"
#include "PLDHashTable.h"

namespace mozilla {
namespace net {
class nsSimpleURI;
} // namespace net
} // namespace mozilla

/**
 * A wrapper for an nsIPrincipal that can be used OMT, which has cached
 * information useful for the gfxUserFontSet.
 */
class gfxFontSrcPrincipal
{
public:
  explicit gfxFontSrcPrincipal(nsIPrincipal* aPrincipal);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxFontSrcPrincipal)

  nsIPrincipal* get() { return mPrincipal; }

  bool Equals(gfxFontSrcPrincipal* aOther);

  PLDHashNumber Hash() const { return mHash; }

private:
  ~gfxFontSrcPrincipal();

  // The principal.
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // Precomputed hash for mPrincipal.
  PLDHashNumber mHash;
};

#endif // MOZILLA_GFX_FONTSRCPRINCIPAL_H
