/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFace_h
#define mozilla_dom_FontFace_h

#include "mozilla/dom/FontFaceBinding.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/ServoStyleConsts.h"
#include "gfxUserFontSet.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"
#include "nsWrapperCache.h"

class gfxFontFaceBufferSource;
struct RawServoFontFaceRule;

namespace mozilla {
struct CSSFontFaceDescriptors;
class PostTraversalTask;
namespace dom {
class CSSFontFaceRule;
class FontFaceBufferSource;
struct FontFaceDescriptors;
class FontFaceSet;
class Promise;
class UTF8StringOrArrayBufferOrArrayBufferView;
}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace dom {

class FontFace final : public nsISupports, public nsWrapperCache {
  friend class mozilla::PostTraversalTask;
  friend class FontFaceBufferSource;
  friend class Entry;

 public:
  class Entry final : public gfxUserFontEntry {
    friend class FontFace;

   public:
    Entry(gfxUserFontSet* aFontSet,
          const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, WeightRange aWeight,
          StretchRange aStretch, SlantStyleRange aStyle,
          const nsTArray<gfxFontFeature>& aFeatureSettings,
          const nsTArray<gfxFontVariation>& aVariationSettings,
          uint32_t aLanguageOverride, gfxCharacterMap* aUnicodeRanges,
          StyleFontDisplay aFontDisplay, RangeFlags aRangeFlags,
          float aAscentOverride, float aDescentOverride, float aLineGapOverride,
          float aSizeAdjust)
        : gfxUserFontEntry(aFontSet, aFontFaceSrcList, aWeight, aStretch,
                           aStyle, aFeatureSettings, aVariationSettings,
                           aLanguageOverride, aUnicodeRanges, aFontDisplay,
                           aRangeFlags, aAscentOverride, aDescentOverride,
                           aLineGapOverride, aSizeAdjust) {}

    virtual void SetLoadState(UserFontLoadState aLoadState) override;
    virtual void GetUserFontSets(nsTArray<gfxUserFontSet*>& aResult) override;
    const AutoTArray<FontFace*, 1>& GetFontFaces() { return mFontFaces; }

   protected:
    // The FontFace objects that use this user font entry.  We need to store
    // an array of these, not just a single pointer, since the user font
    // cache can return the same entry for different FontFaces that have
    // the same descriptor values and come from the same origin.
    AutoTArray<FontFace*, 1> mFontFaces;
  };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FontFace)

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<FontFace> CreateForRule(nsISupports* aGlobal,
                                                  FontFaceSet* aFontFaceSet,
                                                  RawServoFontFaceRule* aRule);

  RawServoFontFaceRule* GetRule() { return mRule; }

  bool HasLocalSrc() const;
  Maybe<StyleComputedFontWeightRange> GetFontWeight() const;
  Maybe<StyleComputedFontStretchRange> GetFontStretch() const;
  Maybe<StyleComputedFontStyleDescriptor> GetFontStyle() const;
  Maybe<StyleFontDisplay> GetFontDisplay() const;
  void GetFontFeatureSettings(nsTArray<gfxFontFeature>&) const;
  void GetFontVariationSettings(nsTArray<gfxFontVariation>&) const;
  void GetSources(nsTArray<StyleFontFaceSourceListComponent>&) const;
  Maybe<StyleFontLanguageOverride> GetFontLanguageOverride() const;
  Maybe<StylePercentage> GetAscentOverride() const;
  Maybe<StylePercentage> GetDescentOverride() const;
  Maybe<StylePercentage> GetLineGapOverride() const;
  Maybe<StylePercentage> GetSizeAdjust() const;

  gfxUserFontEntry* CreateUserFontEntry();
  gfxUserFontEntry* GetUserFontEntry() const { return mUserFontEntry; }
  void SetUserFontEntry(gfxUserFontEntry* aEntry);

  /**
   * Returns whether this object is in the specified FontFaceSet.
   */
  bool IsInFontFaceSet(FontFaceSet* aFontFaceSet) const;

  void AddFontFaceSet(FontFaceSet* aFontFaceSet);
  void RemoveFontFaceSet(FontFaceSet* aFontFaceSet);

  FontFaceSet* GetPrimaryFontFaceSet() const { return mFontFaceSet; }

  /**
   * Gets the family name of the FontFace as a raw string (such as 'Times', as
   * opposed to GetFamily, which returns a CSS-escaped string, such as
   * '"Times"').  Returns null if a valid family name was not available.
   */
  nsAtom* GetFamilyName() const;

  /**
   * Returns whether this object is CSS-connected, i.e. reflecting an
   * @font-face rule.
   */
  bool HasRule() const { return mRule; }

  /**
   * Breaks the connection between this FontFace and its @font-face rule.
   */
  void DisconnectFromRule();

  /**
   * Returns whether there is an ArrayBuffer or ArrayBufferView of font
   * data.
   */
  bool HasFontData() const;

  /**
   * Creates a gfxFontFaceBufferSource to represent the font data
   * in this object.
   */
  already_AddRefed<gfxFontFaceBufferSource> CreateBufferSource();

  /**
   * Gets a pointer to and the length of the font data stored in the
   * ArrayBuffer or ArrayBufferView.
   */
  bool GetData(uint8_t*& aBuffer, uint32_t& aLength);

  /**
   * Returns the value of the unicode-range descriptor as a gfxCharacterMap.
   */
  gfxCharacterMap* GetUnicodeRangeAsCharacterMap();

  // Web IDL
  static already_AddRefed<FontFace> Constructor(
      const GlobalObject& aGlobal, const nsACString& aFamily,
      const UTF8StringOrArrayBufferOrArrayBufferView& aSource,
      const FontFaceDescriptors& aDescriptors, ErrorResult& aRV);

  void GetFamily(nsACString& aResult);
  void SetFamily(const nsACString& aValue, ErrorResult& aRv);
  void GetStyle(nsACString& aResult);
  void SetStyle(const nsACString& aValue, ErrorResult& aRv);
  void GetWeight(nsACString& aResult);
  void SetWeight(const nsACString& aValue, ErrorResult& aRv);
  void GetStretch(nsACString& aResult);
  void SetStretch(const nsACString& aValue, ErrorResult& aRv);
  void GetUnicodeRange(nsACString& aResult);
  void SetUnicodeRange(const nsACString& aValue, ErrorResult& aRv);
  void GetVariant(nsACString& aResult);
  void SetVariant(const nsACString& aValue, ErrorResult& aRv);
  void GetFeatureSettings(nsACString& aResult);
  void SetFeatureSettings(const nsACString& aValue, ErrorResult& aRv);
  void GetVariationSettings(nsACString& aResult);
  void SetVariationSettings(const nsACString& aValue, ErrorResult& aRv);
  void GetDisplay(nsACString& aResult);
  void SetDisplay(const nsACString& aValue, ErrorResult& aRv);
  void GetAscentOverride(nsACString& aResult);
  void SetAscentOverride(const nsACString& aValue, ErrorResult& aRv);
  void GetDescentOverride(nsACString& aResult);
  void SetDescentOverride(const nsACString& aValue, ErrorResult& aRv);
  void GetLineGapOverride(nsACString& aResult);
  void SetLineGapOverride(const nsACString& aValue, ErrorResult& aRv);
  void GetSizeAdjust(nsACString& aResult);
  void SetSizeAdjust(const nsACString& aValue, ErrorResult& aRv);

  FontFaceLoadStatus Status();
  Promise* Load(ErrorResult& aRv);
  Promise* GetLoaded(ErrorResult& aRv);

 private:
  FontFace(nsISupports* aParent, FontFaceSet* aFontFaceSet);
  ~FontFace();

  void InitializeSource(const UTF8StringOrArrayBufferOrArrayBufferView&);

  // Helper function for Load.
  void DoLoad();

  // Helper function for the descriptor setter methods.
  // Returns true if the descriptor was modified, false if descriptor is
  // unchanged (which may not be an error: check aRv for actual failure).
  bool SetDescriptor(nsCSSFontDesc aFontDesc, const nsACString& aValue,
                     ErrorResult& aRv);

  /**
   * Sets all of the descriptor values in mDescriptors using values passed
   * to the JS constructor.
   * Returns true on success, false if parsing any descriptor failed.
   */
  bool SetDescriptors(const nsACString& aFamily,
                      const FontFaceDescriptors& aDescriptors);

  /**
   * Called when a descriptor has been modified, so font-face sets can
   * be told to refresh.
   */
  void DescriptorUpdated();

  /**
   * Sets the current loading status.
   */
  void SetStatus(FontFaceLoadStatus aStatus);

  void GetDesc(nsCSSFontDesc aDescID, nsACString& aResult) const;

  already_AddRefed<URLExtraData> GetURLExtraData() const;

  RawServoFontFaceRule* GetData() const {
    return HasRule() ? mRule : mDescriptors;
  }

  /**
   * Returns and takes ownership of the buffer storing the font data.
   */
  void TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength);

  // Acts like mLoaded->MaybeReject(aResult), except it doesn't create mLoaded
  // if it doesn't already exist.
  void Reject(nsresult aResult);

  // Creates mLoaded if it doesn't already exist. It may immediately resolve or
  // reject mLoaded based on mStatus and mLoadedRejection.
  void EnsurePromise();

  void DoResolve();
  void DoReject(nsresult aResult);

  nsCOMPtr<nsISupports> mParent;

  // A Promise that is fulfilled once the font represented by this FontFace is
  // loaded, and is rejected if the load fails. This promise is created lazily
  // when JS asks for it.
  RefPtr<Promise> mLoaded;

  // Saves the rejection code for mLoaded if mLoaded hasn't been created yet.
  nsresult mLoadedRejection;

  // The @font-face rule this FontFace object is reflecting, if it is a
  // rule backed FontFace.
  RefPtr<RawServoFontFaceRule> mRule;

  // The FontFace object's user font entry.  This is initially null, but is set
  // during FontFaceSet::UpdateRules and when a FontFace is explicitly loaded.
  RefPtr<Entry> mUserFontEntry;

  // The current load status of the font represented by this FontFace.
  // Note that we can't just reflect the value of the gfxUserFontEntry's
  // status, since the spec sometimes requires us to go through the event
  // loop before updating the status, rather than doing it immediately.
  FontFaceLoadStatus mStatus;

  // Represents where a FontFace's data is coming from.
  enum SourceType {
    eSourceType_FontFaceRule = 1,
    eSourceType_URLs,
    eSourceType_Buffer
  };

  // Where the font data for this FontFace is coming from.
  SourceType mSourceType;

  // If the FontFace was constructed with an ArrayBuffer(View), this is a
  // copy of the data from it.
  uint8_t* mSourceBuffer;
  uint32_t mSourceBufferLength;

  // The values corresponding to the font face descriptors, if we are not
  // a rule backed FontFace object.  For rule backed objects, we use
  // the descriptors stored in mRule.
  // FIXME This should hold a unique ptr to just the descriptors inside,
  // so that we don't need to create a rule for it and don't need to
  // assign a fake line number and column number. See bug 1450904.
  RefPtr<RawServoFontFaceRule> mDescriptors;

  // The value of the unicode-range descriptor as a gfxCharacterMap.  Valid
  // only when mUnicodeRangeDirty is false.
  RefPtr<gfxCharacterMap> mUnicodeRange;

  // The primary FontFaceSet this FontFace is associated with,
  // regardless of whether it is currently "in" the set.
  RefPtr<FontFaceSet> mFontFaceSet;

  // Other FontFaceSets (apart from mFontFaceSet) that this FontFace
  // appears in.
  nsTArray<RefPtr<FontFaceSet>> mOtherFontFaceSets;

  // Whether mUnicodeRange needs to be rebuilt before being returned from
  // GetUnicodeRangeAsCharacterMap.
  bool mUnicodeRangeDirty;

  // Whether this FontFace appears in mFontFaceSet.
  bool mInFontFaceSet;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_FontFace_h)
