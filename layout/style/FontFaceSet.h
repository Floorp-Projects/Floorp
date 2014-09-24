/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFaceSet_h
#define mozilla_dom_FontFaceSet_h

#include "mozilla/dom/FontFace.h"
#include "mozilla/dom/FontFaceSetBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "gfxUserFontSet.h"
#include "nsCSSRules.h"
#include "nsICSSLoaderObserver.h"
#include "nsPIDOMWindow.h"

struct gfxFontFaceSrc;
class gfxUserFontEntry;
class nsFontFaceLoader;
class nsIPrincipal;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {
class FontFace;
class Promise;
}
}

namespace mozilla {
namespace dom {

class FontFaceSet MOZ_FINAL : public DOMEventTargetHelper
                            , public nsIDOMEventListener
                            , public nsICSSLoaderObserver
{
  friend class UserFontSet;

public:
  /**
   * A gfxUserFontSet that integrates with the layout and style systems to
   * manage @font-face rules and handle network requests for font loading.
   *
   * We would combine this class and FontFaceSet into the one class if it were
   * possible; it's not because FontFaceSet is cycle collected and
   * gfxUserFontSet isn't (and can't be, as gfx classes don't use the cycle
   * collector).  So UserFontSet exists just to override the needed virtual
   * methods from gfxUserFontSet and to forward them on FontFaceSet.
   */
  class UserFontSet MOZ_FINAL : public gfxUserFontSet
  {
    friend class FontFaceSet;

  public:
    explicit UserFontSet(FontFaceSet* aFontFaceSet)
      : mFontFaceSet(aFontFaceSet)
    {
    }

    FontFaceSet* GetFontFaceSet() { return mFontFaceSet; }

    virtual nsresult CheckFontLoad(const gfxFontFaceSrc* aFontFaceSrc,
                                   nsIPrincipal** aPrincipal,
                                   bool* aBypassCache) MOZ_OVERRIDE;
    virtual nsresult StartLoad(gfxUserFontEntry* aUserFontEntry,
                               const gfxFontFaceSrc* aFontFaceSrc) MOZ_OVERRIDE;

  protected:
    virtual bool GetPrivateBrowsing() MOZ_OVERRIDE;
    virtual nsresult SyncLoadFontData(gfxUserFontEntry* aFontToLoad,
                                      const gfxFontFaceSrc* aFontFaceSrc,
                                      uint8_t*& aBuffer,
                                      uint32_t& aBufferLength) MOZ_OVERRIDE;
    virtual nsresult LogMessage(gfxUserFontEntry* aUserFontEntry,
                                const char* aMessage,
                                uint32_t aFlags = nsIScriptError::errorFlag,
                                nsresult aStatus = NS_OK) MOZ_OVERRIDE;
    virtual void DoRebuildUserFontSet() MOZ_OVERRIDE;
    virtual already_AddRefed<gfxUserFontEntry> CreateUserFontEntry(
                                   const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                                   uint32_t aWeight,
                                   int32_t aStretch,
                                   uint32_t aItalicStyle,
                                   const nsTArray<gfxFontFeature>& aFeatureSettings,
                                   uint32_t aLanguageOverride,
                                   gfxSparseBitSet* aUnicodeRanges) MOZ_OVERRIDE;

  private:
    nsRefPtr<FontFaceSet> mFontFaceSet;
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FontFaceSet, DOMEventTargetHelper)
  NS_DECL_NSIDOMEVENTLISTENER

  FontFaceSet(nsPIDOMWindow* aWindow, nsPresContext* aPresContext);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  UserFontSet* EnsureUserFontSet(nsPresContext* aPresContext);
  UserFontSet* GetUserFontSet() { return mUserFontSet; }

  // Called when this font set is no longer associated with a presentation.
  void DestroyUserFontSet();

  // Called by nsFontFaceLoader when the loader has completed normally.
  // It's removed from the mLoaders set.
  void RemoveLoader(nsFontFaceLoader* aLoader);

  bool UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules);

  nsPresContext* GetPresContext() { return mPresContext; }

  // search for @font-face rule that matches a platform font entry
  nsCSSFontFaceRule* FindRuleForEntry(gfxFontEntry* aFontEntry);

  void IncrementGeneration(bool aIsRebuild = false);

  /**
   * Adds the specified FontFace to the mUnavailableFaces array.  This is called
   * when a new FontFace object has just been created in JS by the author.
   */
  void AddUnavailableFontFace(FontFace* aFontFace);

  /**
   * Removes the specified FontFace from the mUnavailableFaces array.  This
   * is called when a FontFace object is about be destroyed.
   */
  void RemoveUnavailableFontFace(FontFace* aFontFace);

  /**
   * Finds an existing entry in the user font cache or creates a new user
   * font entry for the given FontFace object.
   */
  already_AddRefed<gfxUserFontEntry>
    FindOrCreateUserFontEntryFromFontFace(FontFace* aFontFace);

  /**
   * Notification method called by a FontFace once it has been initialized.
   *
   * This is needed for the FontFaceSet to handle a FontFace that was created
   * and inserted into the set immediately, before the event loop has spun and
   * the FontFace's initialization tasks have run.
   */
  void OnFontFaceInitialized(FontFace* aFontFace);

  /**
   * Notification method called by a FontFace to indicate that its loading
   * status has changed.
   */
  void OnFontFaceStatusChanged(FontFace* aFontFace);

  /**
   * Notification method called by the nsPresContext to indicate that the
   * refresh driver ticked and flushed style and layout.
   * were just flushed.
   */
  void DidRefresh();

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(mozilla::CSSStyleSheet* aSheet,
                              bool aWasAlternate,
                              nsresult aStatus);

  // -- Web IDL --------------------------------------------------------------

  IMPL_EVENT_HANDLER(loading)
  IMPL_EVENT_HANDLER(loadingdone)
  IMPL_EVENT_HANDLER(loadingerror)
  already_AddRefed<mozilla::dom::Promise> Load(const nsAString& aFont,
                                               const nsAString& aText,
                                               mozilla::ErrorResult& aRv);
  bool Check(const nsAString& aFont,
             const nsAString& aText,
             mozilla::ErrorResult& aRv);
  mozilla::dom::Promise* GetReady(mozilla::ErrorResult& aRv);
  mozilla::dom::FontFaceSetLoadStatus Status();

  FontFaceSet* Add(FontFace& aFontFace, mozilla::ErrorResult& aRv);
  void Clear();
  bool Delete(FontFace& aFontFace, mozilla::ErrorResult& aRv);
  bool Has(FontFace& aFontFace);
  FontFace* IndexedGetter(uint32_t aIndex, bool& aFound);
  uint32_t Length();

private:
  ~FontFaceSet();

  /**
   * Returns whether the given FontFace is currently "in" the FontFaceSet.
   */
  bool HasAvailableFontFace(FontFace* aFontFace);

  /**
   * Removes any listeners and observers.
   */
  void Disconnect();

  /**
   * Calls DisconnectFromRule on the given FontFace and removes its entry from
   * mRuleFaceMap.
   */
  void DisconnectFromRule(FontFace* aFontFace);

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
   * Checks to see whether it is time to resolve mReady and dispatch any
   * "loadingdone" and "loadingerror" events.
   */
  void CheckLoadingFinished();

  /**
   * Dispatches a CSSFontFaceLoadEvent to this object.
   */
  void DispatchLoadingFinishedEvent(
                                const nsAString& aType,
                                const nsTArray<FontFace*>& aFontFaces);

  // Note: if you add new cycle collected objects to FontFaceRecord,
  // make sure to update FontFaceSet's cycle collection macros
  // accordingly.
  struct FontFaceRecord {
    nsRefPtr<FontFace> mFontFace;
    uint8_t mSheetType;
  };

  FontFace* FontFaceForRule(nsCSSFontFaceRule* aRule);

  already_AddRefed<gfxUserFontEntry> FindOrCreateUserFontEntryFromFontFace(
                                                   const nsAString& aFamilyName,
                                                   FontFace* aFontFace,
                                                   uint8_t aSheetType);

  // search for @font-face rule that matches a userfont font entry
  nsCSSFontFaceRule* FindRuleForUserFontEntry(gfxUserFontEntry* aUserFontEntry);

  // search for user font entry for the given @font-face rule
  gfxUserFontEntry* FindUserFontEntryForRule(nsCSSFontFaceRule* aRule);

  nsresult StartLoad(gfxUserFontEntry* aUserFontEntry,
                     const gfxFontFaceSrc* aFontFaceSrc);
  nsresult CheckFontLoad(const gfxFontFaceSrc* aFontFaceSrc,
                         nsIPrincipal** aPrincipal,
                         bool* aBypassCache);
  bool GetPrivateBrowsing();
  nsresult SyncLoadFontData(gfxUserFontEntry* aFontToLoad,
                            const gfxFontFaceSrc* aFontFaceSrc,
                            uint8_t*& aBuffer,
                            uint32_t& aBufferLength);
  nsresult LogMessage(gfxUserFontEntry* aUserFontEntry,
                      const char* aMessage,
                      uint32_t aFlags,
                      nsresult aStatus);
  void DoRebuildUserFontSet();

  void InsertRuleFontFace(FontFace* aFontFace, uint8_t aSheetType,
                          nsTArray<FontFaceRecord>& aOldRecords,
                          bool& aFontSetModified);
  void InsertNonRuleFontFace(FontFace* aFontFace, bool& aFontSetModified);

#ifdef DEBUG
  bool HasRuleFontFace(FontFace* aFontFace);
#endif

  /**
   * Returns whether we have any loading FontFace objects in the FontFaceSet.
   */
  bool HasLoadingFontFaces();

  // Helper function for HasLoadingFontFaces.
  void UpdateHasLoadingFontFaces();

  nsRefPtr<UserFontSet> mUserFontSet;
  nsPresContext* mPresContext;

  // The document this is a FontFaceSet for.
  nsCOMPtr<nsIDocument> mDocument;

  // A Promise that is fulfilled once all of the FontFace objects
  // in mRuleFaces and mNonRuleFaces that started or were loading at the
  // time the Promise was created have finished loading.  It is rejected if
  // any of those fonts failed to load.  mReady is replaced with
  // a new Promise object whenever mReady is settled and another
  // FontFace in mRuleFaces or mNonRuleFaces starts to load.
  nsRefPtr<mozilla::dom::Promise> mReady;

  // Set of all loaders pointing to us. These are not strong pointers,
  // but that's OK because nsFontFaceLoader always calls RemoveLoader on
  // us before it dies (unless we die first).
  nsTHashtable< nsPtrHashKey<nsFontFaceLoader> > mLoaders;

  // The @font-face rule backed FontFace objects in the FontFaceSet.
  nsTArray<FontFaceRecord> mRuleFaces;

  // The non rule backed FontFace objects that have been added to this
  // FontFaceSet and their corresponding user font entries.
  nsTArray<nsRefPtr<FontFace>> mNonRuleFaces;

  // The non rule backed FontFace objects that have not been added to
  // this FontFaceSet.
  nsTArray<FontFace*> mUnavailableFaces;

  // Map of nsCSSFontFaceRule objects to FontFace objects.  We hold a weak
  // reference to both; for actively used FontFaces, mRuleFaces will hold
  // a strong reference to the FontFace and the FontFace will hold on to
  // the nsCSSFontFaceRule.  FontFaceSet::DisconnectFromRule will ensure its
  // entry in this array will be removed.
  nsDataHashtable<nsPtrHashKey<nsCSSFontFaceRule>, FontFace*> mRuleFaceMap;

  // The overall status of the loading or loaded fonts in the FontFaceSet.
  mozilla::dom::FontFaceSetLoadStatus mStatus;

  // Whether mNonRuleFaces has changed since last time UpdateRules ran.
  bool mNonRuleFacesDirty;

  // Whether we have called MaybeResolve() on mReady.
  bool mReadyIsResolved;

  // Whether we have already dispatched loading events for the current set
  // of loading FontFaces.
  bool mDispatchedLoadingEvent;

  // Whether any FontFace objects in mRuleFaces or mNonRuleFaces are
  // loading.  Only valid when mHasLoadingFontFacesIsDirty is false.  Don't use
  // this variable directly; call the HasLoadingFontFaces method instead.
  bool mHasLoadingFontFaces;

  // This variable is only valid when mLoadingDirty is false.
  bool mHasLoadingFontFacesIsDirty;
};

} // namespace dom
} // namespace mozilla

#endif // !defined(mozilla_dom_FontFaceSet_h)
