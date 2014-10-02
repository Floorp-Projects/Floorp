/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFaceSet_h
#define mozilla_dom_FontFaceSet_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/FontFace.h"
#include "mozilla/dom/FontFaceSetBinding.h"
#include "gfxUserFontSet.h"
#include "nsCSSRules.h"
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
    UserFontSet(FontFaceSet* aFontFaceSet)
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

  // Note: if you add new cycle collected objects to FontFaceRecord,
  // make sure to update FontFaceSet's cycle collection macros
  // accordingly.
  struct FontFaceRecord {
    nsRefPtr<FontFace> mFontFace;
    uint8_t mSheetType;
  };

  FontFace* FontFaceForRule(nsCSSFontFaceRule* aRule);
  void InsertRule(nsCSSFontFaceRule* aRule, uint8_t aSheetType,
                  nsTArray<FontFaceRecord>& aOldRecords,
                  bool& aFontSetModified);

  already_AddRefed<gfxUserFontEntry> FindOrCreateUserFontEntryFromRule(
                                                   const nsAString& aFamilyName,
                                                   nsCSSFontFaceRule* aRule,
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

  nsRefPtr<UserFontSet> mUserFontSet;
  nsPresContext* mPresContext;

  nsRefPtr<mozilla::dom::Promise> mReady;

  // Set of all loaders pointing to us. These are not strong pointers,
  // but that's OK because nsFontFaceLoader always calls RemoveLoader on
  // us before it dies (unless we die first).
  nsTHashtable< nsPtrHashKey<nsFontFaceLoader> > mLoaders;

  // The CSS-connected FontFace objects in the FontFaceSet.
  nsTArray<FontFaceRecord> mConnectedFaces;
};

} // namespace dom
} // namespace mozilla

#endif // !defined(mozilla_dom_FontFaceSet_h)
