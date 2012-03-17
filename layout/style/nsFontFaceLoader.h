/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Daggett <jdaggett@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* code for loading in @font-face defined font data */

#ifndef nsFontFaceLoader_h_
#define nsFontFaceLoader_h_

#include "nsCOMPtr.h"
#include "nsIStreamLoader.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsITimer.h"
#include "gfxUserFontSet.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "nsCSSRules.h"

class nsIRequest;
class nsISupports;
class nsPresContext;
class nsIPrincipal;

class nsFontFaceLoader;
class nsCSSFontFaceRule;

// nsUserFontSet - defines the loading mechanism for downloadable fonts
class nsUserFontSet : public gfxUserFontSet
{
public:
  nsUserFontSet(nsPresContext *aContext);
  ~nsUserFontSet();

  // Called when this font set is no longer associated with a presentation.
  void Destroy();

  // starts loading process, creating and initializing a nsFontFaceLoader obj
  // returns whether load process successfully started or not
  nsresult StartLoad(gfxProxyFontEntry *aFontToLoad,
                     const gfxFontFaceSrc *aFontFaceSrc);

  // Called by nsFontFaceLoader when the loader has completed normally.
  // It's removed from the mLoaders set.
  void RemoveLoader(nsFontFaceLoader *aLoader);

  bool UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules);

  nsPresContext *GetPresContext() { return mPresContext; }

  virtual void ReplaceFontEntry(gfxProxyFontEntry *aProxy,
                                gfxFontEntry *aFontEntry);

  nsCSSFontFaceRule *FindRuleForEntry(gfxFontEntry *aFontEntry);

protected:
  // The font-set keeps track of the collection of rules, and their
  // corresponding font entries (whether proxies or real entries),
  // so that we can update the set without having to throw away
  // all the existing fonts.
  struct FontFaceRuleRecord {
    nsRefPtr<gfxFontEntry>       mFontEntry;
    nsFontFaceRuleContainer      mContainer;
  };

  void InsertRule(nsCSSFontFaceRule *aRule, PRUint8 aSheetType,
                  nsTArray<FontFaceRuleRecord>& oldRules,
                  bool& aFontSetModified);

  virtual nsresult LogMessage(gfxProxyFontEntry *aProxy,
                              const char *aMessage,
                              PRUint32 aFlags = nsIScriptError::errorFlag,
                              nsresult aStatus = 0);

  nsresult CheckFontLoad(gfxProxyFontEntry *aFontToLoad,
                         const gfxFontFaceSrc *aFontFaceSrc,
                         nsIPrincipal **aPrincipal);

  virtual nsresult SyncLoadFontData(gfxProxyFontEntry *aFontToLoad,
                                    const gfxFontFaceSrc *aFontFaceSrc,
                                    PRUint8* &aBuffer,
                                    PRUint32 &aBufferLength);

  nsPresContext *mPresContext;  // weak reference

  // Set of all loaders pointing to us. These are not strong pointers,
  // but that's OK because nsFontFaceLoader always calls RemoveLoader on
  // us before it dies (unless we die first).
  nsTHashtable< nsPtrHashKey<nsFontFaceLoader> > mLoaders;

  nsTArray<FontFaceRuleRecord>   mRules;
};

class nsFontFaceLoader : public nsIStreamLoaderObserver
{
public:

  nsFontFaceLoader(gfxProxyFontEntry *aFontToLoad, nsIURI *aFontURI, 
                   nsUserFontSet *aFontSet, nsIChannel *aChannel);
  virtual ~nsFontFaceLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER 

  // initiate the load
  nsresult Init();
  // cancel the load and remove its reference to mFontSet
  void Cancel();

  void DropChannel() { mChannel = nsnull; }

  void StartedLoading(nsIStreamLoader *aStreamLoader);

  static void LoadTimerCallback(nsITimer *aTimer, void *aClosure);

  static nsresult CheckLoadAllowed(nsIPrincipal* aSourcePrincipal,
                                   nsIURI* aTargetURI,
                                   nsISupports* aContext);

private:
  nsRefPtr<gfxProxyFontEntry>  mFontEntry;
  nsRefPtr<gfxFontFamily>      mFontFamily;
  nsCOMPtr<nsIURI>        mFontURI;
  nsRefPtr<nsUserFontSet> mFontSet;
  nsCOMPtr<nsIChannel>    mChannel;
  nsCOMPtr<nsITimer>      mLoadTimer;

  nsIStreamLoader        *mStreamLoader;
};

#endif /* !defined(nsFontFaceLoader_h_) */
