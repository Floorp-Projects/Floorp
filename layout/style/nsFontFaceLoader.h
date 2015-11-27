/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* code for loading in @font-face defined font data */

#ifndef nsFontFaceLoader_h_
#define nsFontFaceLoader_h_

#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsIStreamLoader.h"
#include "nsIChannel.h"
#include "gfxUserFontSet.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "nsCSSRules.h"

class nsIPrincipal;

class nsFontFaceLoader : public nsIStreamLoaderObserver
{
public:
  nsFontFaceLoader(gfxUserFontEntry* aFontToLoad, nsIURI* aFontURI,
                   mozilla::dom::FontFaceSet* aFontFaceSet,
                   nsIChannel* aChannel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER 

  // initiate the load
  nsresult Init();
  // cancel the load and remove its reference to mFontFaceSet
  void Cancel();

  void DropChannel() { mChannel = nullptr; }

  void StartedLoading(nsIStreamLoader* aStreamLoader);

  static void LoadTimerCallback(nsITimer* aTimer, void* aClosure);

  static nsresult CheckLoadAllowed(nsIPrincipal* aSourcePrincipal,
                                   nsIURI* aTargetURI,
                                   nsISupports* aContext);
  gfxUserFontEntry* GetUserFontEntry() const { return mUserFontEntry; }

protected:
  virtual ~nsFontFaceLoader();

private:
  RefPtr<gfxUserFontEntry>  mUserFontEntry;
  nsCOMPtr<nsIURI>        mFontURI;
  RefPtr<mozilla::dom::FontFaceSet> mFontFaceSet;
  nsCOMPtr<nsIChannel>    mChannel;
  nsCOMPtr<nsITimer>      mLoadTimer;
  TimeStamp               mStartTime;
  nsIStreamLoader*        mStreamLoader;
};

#endif /* !defined(nsFontFaceLoader_h_) */
