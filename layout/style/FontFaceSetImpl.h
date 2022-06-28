/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFaceSetImpl_h
#define mozilla_dom_FontFaceSetImpl_h

#include "mozilla/dom/FontFace.h"
#include "mozilla/dom/FontFaceSetBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/FontPropertyTypes.h"
#include "gfxUserFontSet.h"
#include "nsICSSLoaderObserver.h"
#include "nsIDOMEventListener.h"

struct gfxFontFaceSrc;
class gfxFontSrcPrincipal;
class gfxUserFontEntry;
class nsFontFaceLoader;
class nsIPrincipal;
class nsPIDOMWindowInner;
struct RawServoFontFaceRule;

namespace mozilla {
class PostTraversalTask;
class SharedFontList;
namespace dom {
class FontFace;
}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class FontFaceSetImpl final : public gfxUserFontSet,
                              public nsIDOMEventListener,
                              public nsICSSLoaderObserver {
  NS_DECL_THREADSAFE_ISUPPORTS

 public:
  // gfxUserFontSet

  already_AddRefed<gfxFontSrcPrincipal> GetStandardFontLoadPrincipal()
      const override {
    return RefPtr{mStandardFontLoadPrincipal}.forget();
  }

  nsPresContext* GetPresContext() const override;

  bool IsFontLoadAllowed(const gfxFontFaceSrc&) override;

  nsresult StartLoad(gfxUserFontEntry* aUserFontEntry,
                     uint32_t aSrcIndex) override;

  void RecordFontLoadDone(uint32_t aFontSize, TimeStamp aDoneTime) override;

  bool BypassCache() final { return mBypassCache; }

 protected:
  // gfxUserFontSet

  bool GetPrivateBrowsing() override { return mPrivateBrowsing; }
  nsresult SyncLoadFontData(gfxUserFontEntry* aFontToLoad,
                            const gfxFontFaceSrc* aFontFaceSrc,
                            uint8_t*& aBuffer,
                            uint32_t& aBufferLength) override;
  nsresult LogMessage(gfxUserFontEntry* aUserFontEntry, uint32_t aSrcIndex,
                      const char* aMessage,
                      uint32_t aFlags = nsIScriptError::errorFlag,
                      nsresult aStatus = NS_OK) override;
  void DoRebuildUserFontSet() override;
  already_AddRefed<gfxUserFontEntry> CreateUserFontEntry(
      const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, WeightRange aWeight,
      StretchRange aStretch, SlantStyleRange aStyle,
      const nsTArray<gfxFontFeature>& aFeatureSettings,
      const nsTArray<gfxFontVariation>& aVariationSettings,
      uint32_t aLanguageOverride, gfxCharacterMap* aUnicodeRanges,
      StyleFontDisplay aFontDisplay, RangeFlags aRangeFlags,
      float aAscentOverride, float aDescentOverride, float aLineGapOverride,
      float aSizeAdjust) override;

 public:
  NS_DECL_NSIDOMEVENTLISTENER

  FontFaceSetImpl(FontFaceSet* aOwner, dom::Document* aDocument);

  void Initialize();
  void Destroy();

  // Called by nsFontFaceLoader when the loader has completed normally.
  // It's removed from the mLoaders set.
  void RemoveLoader(nsFontFaceLoader* aLoader);

  bool UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules);

  // search for @font-face rule that matches a platform font entry
  RawServoFontFaceRule* FindRuleForEntry(gfxFontEntry* aFontEntry);

  /**
   * Finds an existing entry in the user font cache or creates a new user
   * font entry for the given FontFace object.
   */
  static already_AddRefed<gfxUserFontEntry>
  FindOrCreateUserFontEntryFromFontFace(FontFaceImpl* aFontFace);

  /**
   * Notification method called by a FontFace to indicate that its loading
   * status has changed.
   */
  void OnFontFaceStatusChanged(FontFaceImpl* aFontFace);

  /**
   * Notification method called by the nsPresContext to indicate that the
   * refresh driver ticked and flushed style and layout.
   * were just flushed.
   */
  void DidRefresh();

  /**
   * Returns whether the "layout.css.font-loading-api.enabled" pref is true.
   */
  static bool PrefEnabled();

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet, bool aWasDeferred,
                              nsresult aStatus) override;

  void FlushUserFontSet();

  static nsPresContext* GetPresContextFor(gfxUserFontSet* aUserFontSet) {
    const auto* set = static_cast<FontFaceSetImpl*>(aUserFontSet);
    return set ? set->GetPresContext() : nullptr;
  }

  void RefreshStandardFontLoadPrincipal();

  dom::Document* Document() const { return mDocument; }

  // -- Web IDL --------------------------------------------------------------

  void EnsureReady();
  dom::FontFaceSetLoadStatus Status();

  bool Add(FontFaceImpl* aFontFace, ErrorResult& aRv);
  void Clear();
  bool Delete(FontFaceImpl* aFontFace);

  // For ServoStyleSet to know ahead of time whether a font is loadable.
  void CacheFontLoadability();

  void MarkUserFontSetDirty();

  /**
   * Checks to see whether it is time to resolve mReady and dispatch any
   * "loadingdone" and "loadingerror" events.
   */
  void CheckLoadingFinished();

  void FindMatchingFontFaces(const nsACString& aFont, const nsAString& aText,
                             nsTArray<FontFace*>& aFontFaces, ErrorResult& aRv);

  void DispatchCheckLoadingFinishedAfterDelay();

 private:
  ~FontFaceSetImpl();

  /**
   * Returns whether the given FontFace is currently "in" the FontFaceSet.
   */
  bool HasAvailableFontFace(FontFaceImpl* aFontFace);

  void RemoveDOMContentLoadedListener();

  /**
   * Returns whether there might be any pending font loads, which should cause
   * the mReady Promise not to be resolved yet.
   */
  bool MightHavePendingFontLoads();

  /**
   * Checks to see whether it is time to replace mReady and dispatch a
   * "loading" event.
   */
  void CheckLoadingStarted();

  /**
   * Callback for invoking CheckLoadingFinished after going through the
   * event loop.  See OnFontFaceStatusChanged.
   */
  void CheckLoadingFinishedAfterDelay();

  // Note: if you add new cycle collected objects to FontFaceRecord,
  // make sure to update FontFaceSet's cycle collection macros
  // accordingly.
  struct FontFaceRecord {
    RefPtr<FontFaceImpl> mFontFace;
    Maybe<StyleOrigin> mOrigin;  // only relevant for mRuleFaces entries
  };

  static already_AddRefed<gfxUserFontEntry>
  FindOrCreateUserFontEntryFromFontFace(const nsACString& aFamilyName,
                                        FontFaceImpl* aFontFace, StyleOrigin);

  // search for @font-face rule that matches a userfont font entry
  RawServoFontFaceRule* FindRuleForUserFontEntry(
      gfxUserFontEntry* aUserFontEntry);

  already_AddRefed<gfxFontSrcPrincipal> GetStandardFontLoadPrincipal();
  nsresult CheckFontLoad(const gfxFontFaceSrc* aFontFaceSrc,
                         gfxFontSrcPrincipal** aPrincipal, bool* aBypassCache);

  void InsertRuleFontFace(FontFaceImpl* aFontFace, FontFace* aFontFaceOwner,
                          StyleOrigin aOrigin,
                          nsTArray<FontFaceRecord>& aOldRecords,
                          bool& aFontSetModified);
  void InsertNonRuleFontFace(FontFaceImpl* aFontFace, bool& aFontSetModified);

#ifdef DEBUG
  bool HasRuleFontFace(FontFaceImpl* aFontFace);
#endif

  /**
   * Returns whether we have any loading FontFace objects in the FontFaceSet.
   */
  bool HasLoadingFontFaces();

  // Whether mReady is pending, or would be when created.
  bool ReadyPromiseIsPending() const;

  // Helper function for HasLoadingFontFaces.
  void UpdateHasLoadingFontFaces();

  void ParseFontShorthandForMatching(const nsACString& aFont,
                                     StyleFontFamilyList& aFamilyList,
                                     FontWeight& aWeight, FontStretch& aStretch,
                                     FontSlantStyle& aStyle, ErrorResult& aRv);

  TimeStamp GetNavigationStartTimeStamp();

  FontFaceSet* MOZ_NON_OWNING_REF mOwner;

  // The document this is a FontFaceSet for.
  RefPtr<dom::Document> mDocument;

  // The document's node principal, which is the principal font loads for
  // this FontFaceSet will generally use.  (This principal is not used for
  // @font-face rules in UA and user sheets, where the principal of the
  // sheet is used instead.)
  //
  // This field is used from GetStandardFontLoadPrincipal.  When on a
  // style worker thread, we use mStandardFontLoadPrincipal assuming
  // it is up to date.
  //
  // Because mDocument's principal can change over time,
  // its value must be updated by a call to ResetStandardFontLoadPrincipal.
  RefPtr<gfxFontSrcPrincipal> mStandardFontLoadPrincipal;

  // Set of all loaders pointing to us. These are not strong pointers,
  // but that's OK because nsFontFaceLoader always calls RemoveLoader on
  // us before it dies (unless we die first).
  nsTHashtable<nsPtrHashKey<nsFontFaceLoader>> mLoaders;

  // The @font-face rule backed FontFace objects in the FontFaceSet.
  nsTArray<FontFaceRecord> mRuleFaces;

  // The non rule backed FontFace objects that have been added to this
  // FontFaceSet.
  nsTArray<FontFaceRecord> mNonRuleFaces;

  // The overall status of the loading or loaded fonts in the FontFaceSet.
  dom::FontFaceSetLoadStatus mStatus;

  // A map from gfxFontFaceSrc pointer identity to whether the load is allowed
  // by CSP or other checks. We store this here because querying CSP off the
  // main thread is not a great idea.
  //
  // We could use just the pointer and use this as a hash set, but then we'd
  // have no way to verify that we've checked all the loads we should.
  nsTHashMap<nsPtrHashKey<const gfxFontFaceSrc>, bool> mAllowedFontLoads;

  // Whether mNonRuleFaces has changed since last time UpdateRules ran.
  bool mNonRuleFacesDirty;

  // Whether any FontFace objects in mRuleFaces or mNonRuleFaces are
  // loading.  Only valid when mHasLoadingFontFacesIsDirty is false.  Don't use
  // this variable directly; call the HasLoadingFontFaces method instead.
  bool mHasLoadingFontFaces;

  // This variable is only valid when mLoadingDirty is false.
  bool mHasLoadingFontFacesIsDirty;

  // Whether CheckLoadingFinished calls should be ignored.  See comment in
  // OnFontFaceStatusChanged.
  bool mDelayedLoadCheck;

  // Whether the docshell for our document indicated that loads should
  // bypass the cache.
  bool mBypassCache;

  // Whether the docshell for our document indicates that we are in private
  // browsing mode.
  bool mPrivateBrowsing;
};

}  // namespace mozilla::dom

#endif  // !defined(mozilla_dom_FontFaceSetImpl_h)
