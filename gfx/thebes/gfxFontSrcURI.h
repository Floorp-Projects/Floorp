/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FONTSRCURI_H
#define MOZILLA_GFX_FONTSRCURI_H

#include "nsCOMPtr.h"
#include "nsTString.h"
#include "PLDHashTable.h"

class nsIURI;

namespace mozilla {
namespace net {
class nsSimpleURI;
}  // namespace net
}  // namespace mozilla

/**
 * A wrapper for an nsIURI that can be used OMT, which has cached information
 * useful for the gfxUserFontSet.
 */
class gfxFontSrcURI final {
 public:
  explicit gfxFontSrcURI(nsIURI* aURI);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxFontSrcURI)

  nsIURI* get() { return mURI; }

  bool Equals(gfxFontSrcURI* aOther);
  nsresult GetSpec(nsACString& aResult);
  nsCString GetSpecOrDefault();

  PLDHashNumber Hash() const { return mHash; }

  bool InheritsSecurityContext() {
    EnsureInitialized();
    return mInheritsSecurityContext;
  }

  bool SyncLoadIsOK() {
    EnsureInitialized();
    return mSyncLoadIsOK;
  }

 private:
  ~gfxFontSrcURI();

  void EnsureInitialized();

  // The URI.
  nsCOMPtr<nsIURI> mURI;

  // If the nsIURI is an nsSimpleURI for a data: URL, this is a pointer to it.
  // (Just a weak reference since mURI holds the strong reference.)
  //
  // We store this so that we don't duplicate the URL spec for data: URLs,
  // which can be much larger than other URLs.
  mozilla::net::nsSimpleURI* mSimpleURI;

  // If the nsIURI is not an nsSimpleURI, this is its spec.
  nsCString mSpec;

  // Precomputed hash for mURI.
  PLDHashNumber mHash;

  // Whether the font has been initialized on the main thread.
  bool mInitialized = false;

  // Whether the nsIURI's protocol handler has the URI_INHERITS_SECURITY_CONTEXT
  // flag.
  bool mInheritsSecurityContext = false;

  // Whether the nsIURI's protocol handler has teh URI_SYNC_LOAD_IS_OK flag.
  bool mSyncLoadIsOK = false;
};

#endif  // MOZILLA_GFX_FONTSRCURI_H
