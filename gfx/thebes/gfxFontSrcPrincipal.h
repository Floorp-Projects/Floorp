/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FONTSRCPRINCIPAL_H
#define MOZILLA_GFX_FONTSRCPRINCIPAL_H

#include "nsCOMPtr.h"
#include "PLDHashTable.h"

class nsIPrincipal;

namespace mozilla {
namespace net {
class nsSimpleURI;
}  // namespace net
}  // namespace mozilla

/**
 * A wrapper for an nsIPrincipal that can be used OMT, which has cached
 * information useful for the gfxUserFontSet.
 *
 * TODO(emilio): This has grown a bit more complex, but nsIPrincipal is now
 * thread-safe, re-evaluate the existence of this class.
 */
class gfxFontSrcPrincipal {
 public:
  explicit gfxFontSrcPrincipal(nsIPrincipal* aNodePrincipal,
                               nsIPrincipal* aStoragePrincipal);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxFontSrcPrincipal)

  nsIPrincipal* NodePrincipal() const { return mNodePrincipal; }

  nsIPrincipal* StoragePrincipal() const { return mStoragePrincipal; }

  bool Equals(gfxFontSrcPrincipal* aOther);

  PLDHashNumber Hash() const { return mHash; }

 private:
  ~gfxFontSrcPrincipal();

  // The principal of the node.
  nsCOMPtr<nsIPrincipal> mNodePrincipal;

  // The principal used for storage.
  nsCOMPtr<nsIPrincipal> mStoragePrincipal;

  // Precomputed hash for mStoragePrincipal.
  PLDHashNumber mHash;
};

#endif  // MOZILLA_GFX_FONTSRCPRINCIPAL_H
