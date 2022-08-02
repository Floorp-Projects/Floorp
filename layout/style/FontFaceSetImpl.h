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
#include "mozilla/RecursiveMutex.h"
#include "gfxUserFontSet.h"
#include "nsICSSLoaderObserver.h"
#include "nsIDOMEventListener.h"

#include <functional>

struct gfxFontFaceSrc;
class gfxFontSrcPrincipal;
class gfxUserFontEntry;
class nsFontFaceLoader;
class nsIChannel;
class nsIPrincipal;
class nsPIDOMWindowInner;
struct RawServoFontFaceRule;

namespace mozilla {
class PostTraversalTask;
class Runnable;
class SharedFontList;
namespace dom {
class FontFace;
}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class FontFaceSetImpl : public nsISupports, public gfxUserFontSet {
  NS_DECL_THREADSAFE_ISUPPORTS

 public:
  // gfxUserFontSet

  already_AddRefed<gfxFontSrcPrincipal> GetStandardFontLoadPrincipal()
      const final;

  void RecordFontLoadDone(uint32_t aFontSize, TimeStamp aDoneTime) override;

  bool BypassCache() final { return mBypassCache; }

 protected:
  virtual nsresult CreateChannelForSyncLoadFontData(
      nsIChannel** aOutChannel, gfxUserFontEntry* aFontToLoad,
      const gfxFontFaceSrc* aFontFaceSrc) = 0;

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

  explicit FontFaceSetImpl(FontFaceSet* aOwner);

 public:
  virtual void Destroy();
  virtual bool IsOnOwningThread() = 0;
  virtual void DispatchToOwningThread(const char* aName,
                                      std::function<void()>&& aFunc) = 0;

  // Called by nsFontFaceLoader when the loader has completed normally.
  // It's removed from the mLoaders set.
  virtual void RemoveLoader(nsFontFaceLoader* aLoader);

  virtual bool UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules) {
    MOZ_ASSERT_UNREACHABLE("Not implemented!");
    return false;
  }

  // search for @font-face rule that matches a platform font entry
  virtual RawServoFontFaceRule* FindRuleForEntry(gfxFontEntry* aFontEntry) {
    MOZ_ASSERT_UNREACHABLE("Not implemented!");
    return nullptr;
  }

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
  virtual void OnFontFaceStatusChanged(FontFaceImpl* aFontFace);

  /**
   * Notification method called by the nsPresContext to indicate that the
   * refresh driver ticked and flushed style and layout.
   * were just flushed.
   */
  virtual void DidRefresh() { MOZ_ASSERT_UNREACHABLE("Not implemented!"); }

  virtual void FlushUserFontSet() = 0;

  static nsPresContext* GetPresContextFor(gfxUserFontSet* aUserFontSet) {
    const auto* set = static_cast<FontFaceSetImpl*>(aUserFontSet);
    return set ? set->GetPresContext() : nullptr;
  }

  virtual void RefreshStandardFontLoadPrincipal();

  virtual dom::Document* GetDocument() const { return nullptr; }

  virtual already_AddRefed<URLExtraData> GetURLExtraData() = 0;

  // -- Web IDL --------------------------------------------------------------

  virtual void EnsureReady() {}
  dom::FontFaceSetLoadStatus Status();

  virtual bool Add(FontFaceImpl* aFontFace, ErrorResult& aRv);
  virtual void Clear();
  virtual bool Delete(FontFaceImpl* aFontFace);

  // For ServoStyleSet to know ahead of time whether a font is loadable.
  virtual void CacheFontLoadability() {
    MOZ_ASSERT_UNREACHABLE("Not implemented!");
  }

  virtual void MarkUserFontSetDirty() {}

  /**
   * Checks to see whether it is time to resolve mReady and dispatch any
   * "loadingdone" and "loadingerror" events.
   */
  virtual void CheckLoadingFinished();

  virtual void FindMatchingFontFaces(const nsACString& aFont,
                                     const nsAString& aText,
                                     nsTArray<FontFace*>& aFontFaces,
                                     ErrorResult& aRv);

  virtual void DispatchCheckLoadingFinishedAfterDelay();

 protected:
  ~FontFaceSetImpl() override;

  virtual uint64_t GetInnerWindowID() = 0;

  /**
   * Returns whether the given FontFace is currently "in" the FontFaceSet.
   */
  bool HasAvailableFontFace(FontFaceImpl* aFontFace);

  /**
   * Returns whether there might be any pending font loads, which should cause
   * the mReady Promise not to be resolved yet.
   */
  virtual bool MightHavePendingFontLoads();

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

  void OnLoadingStarted();
  void OnLoadingFinished();

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
  virtual RawServoFontFaceRule* FindRuleForUserFontEntry(
      gfxUserFontEntry* aUserFontEntry) {
    return nullptr;
  }

  virtual void FindMatchingFontFaces(
      const nsTHashSet<FontFace*>& aMatchingFaces,
      nsTArray<FontFace*>& aFontFaces);

  nsresult CheckFontLoad(const gfxFontFaceSrc* aFontFaceSrc,
                         gfxFontSrcPrincipal** aPrincipal, bool* aBypassCache);

  void InsertNonRuleFontFace(FontFaceImpl* aFontFace, bool& aFontSetModified);

  /**
   * Returns whether we have any loading FontFace objects in the FontFaceSet.
   */
  bool HasLoadingFontFaces();

  // Whether mReady is pending, or would be when created.
  bool ReadyPromiseIsPending() const;

  // Helper function for HasLoadingFontFaces.
  virtual void UpdateHasLoadingFontFaces();

  void ParseFontShorthandForMatching(const nsACString& aFont,
                                     StyleFontFamilyList& aFamilyList,
                                     FontWeight& aWeight, FontStretch& aStretch,
                                     FontSlantStyle& aStyle, ErrorResult& aRv);

  virtual TimeStamp GetNavigationStartTimeStamp() = 0;

  mutable RecursiveMutex mMutex;

  FontFaceSet* MOZ_NON_OWNING_REF mOwner MOZ_GUARDED_BY(mMutex);

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
  mutable RefPtr<gfxFontSrcPrincipal> mStandardFontLoadPrincipal
      MOZ_GUARDED_BY(mMutex);

  // Set of all loaders pointing to us. These are not strong pointers,
  // but that's OK because nsFontFaceLoader always calls RemoveLoader on
  // us before it dies (unless we die first).
  nsTHashtable<nsPtrHashKey<nsFontFaceLoader>> mLoaders MOZ_GUARDED_BY(mMutex);

  // The non rule backed FontFace objects that have been added to this
  // FontFaceSet.
  nsTArray<FontFaceRecord> mNonRuleFaces MOZ_GUARDED_BY(mMutex);

  // The overall status of the loading or loaded fonts in the FontFaceSet.
  dom::FontFaceSetLoadStatus mStatus MOZ_GUARDED_BY(mMutex);

  // A map from gfxFontFaceSrc pointer identity to whether the load is allowed
  // by CSP or other checks. We store this here because querying CSP off the
  // main thread is not a great idea.
  //
  // We could use just the pointer and use this as a hash set, but then we'd
  // have no way to verify that we've checked all the loads we should.
  nsTHashMap<nsPtrHashKey<const gfxFontFaceSrc>, bool> mAllowedFontLoads
      MOZ_GUARDED_BY(mMutex);

  // Whether mNonRuleFaces has changed since last time UpdateRules ran.
  bool mNonRuleFacesDirty MOZ_GUARDED_BY(mMutex);

  // Whether any FontFace objects in mRuleFaces or mNonRuleFaces are
  // loading.  Only valid when mHasLoadingFontFacesIsDirty is false.  Don't use
  // this variable directly; call the HasLoadingFontFaces method instead.
  bool mHasLoadingFontFaces MOZ_GUARDED_BY(mMutex);

  // This variable is only valid when mLoadingDirty is false.
  bool mHasLoadingFontFacesIsDirty MOZ_GUARDED_BY(mMutex);

  // Whether CheckLoadingFinished calls should be ignored.  See comment in
  // OnFontFaceStatusChanged.
  bool mDelayedLoadCheck MOZ_GUARDED_BY(mMutex);

  // Whether the docshell for our document indicated that loads should
  // bypass the cache.
  bool mBypassCache;

  // Whether the docshell for our document indicates that we are in private
  // browsing mode.
  bool mPrivateBrowsing;
};

}  // namespace mozilla::dom

#endif  // !defined(mozilla_dom_FontFaceSetImpl_h)
