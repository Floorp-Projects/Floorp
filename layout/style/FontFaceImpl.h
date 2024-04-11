/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFaceImpl_h
#define mozilla_dom_FontFaceImpl_h

#include "mozilla/dom/FontFaceBinding.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/RWLock.h"
#include "mozilla/ServoStyleConsts.h"
#include "gfxUserFontSet.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"
#include "nsTHashSet.h"

class gfxFontFaceBufferSource;

namespace mozilla {
struct CSSFontFaceDescriptors;
class PostTraversalTask;
struct StyleLockedFontFaceRule;
namespace dom {
class CSSFontFaceRule;
class FontFace;
class FontFaceBufferSource;
struct FontFaceDescriptors;
class FontFaceSetImpl;
class UTF8StringOrArrayBufferOrArrayBufferView;
}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class FontFaceImpl final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FontFaceImpl)

  friend class mozilla::PostTraversalTask;
  friend class FontFaceBufferSource;
  friend class Entry;

 public:
  class Entry final : public gfxUserFontEntry {
    friend class FontFaceImpl;

   public:
    Entry(gfxUserFontSet* aFontSet, nsTArray<gfxFontFaceSrc>&& aFontFaceSrcList,
          gfxUserFontAttributes&& aAttr)
        : gfxUserFontEntry(std::move(aFontFaceSrcList), std::move(aAttr)),
          mFontSet(aFontSet) {}

    void SetLoadState(UserFontLoadState aLoadState) override;
    void GetUserFontSets(nsTArray<RefPtr<gfxUserFontSet>>& aResult) override;
    already_AddRefed<gfxUserFontSet> GetUserFontSet() const override;

#ifdef DEBUG
    bool HasUserFontSet(gfxUserFontSet* aFontSet) const {
      AutoReadLock lock(mLock);
      return mFontSet == aFontSet;
    }
#endif

    void AddFontFace(FontFaceImpl* aOwner);
    void RemoveFontFace(FontFaceImpl* aOwner);
    void FindFontFaceOwners(nsTHashSet<FontFace*>& aOwners);

    RWLock& Lock() const MOZ_RETURN_CAPABILITY(mLock) { return mLock; }

   protected:
    void CheckUserFontSetLocked() MOZ_REQUIRES(mLock);

    // Font set which owns this entry;
    gfxUserFontSet* MOZ_NON_OWNING_REF mFontSet MOZ_GUARDED_BY(mLock);

    // The FontFace objects that use this user font entry.  We need to store
    // an array of these, not just a single pointer, since the user font
    // cache can return the same entry for different FontFaces that have
    // the same descriptor values and come from the same origin.
    AutoTArray<FontFaceImpl*, 1> mFontFaces MOZ_GUARDED_BY(mLock);
  };

#ifdef DEBUG
  void AssertIsOnOwningThread() const;
#else
  void AssertIsOnOwningThread() const {}
#endif

  FontFace* GetOwner() const {
    AssertIsOnOwningThread();
    return mOwner;
  }

  static already_AddRefed<FontFaceImpl> CreateForRule(
      FontFace* aOwner, FontFaceSetImpl* aFontFaceSet,
      StyleLockedFontFaceRule* aRule);

  StyleLockedFontFaceRule* GetRule() { return mRule; }

  bool HasLocalSrc() const;

  bool GetAttributes(gfxUserFontAttributes& aAttr);
  gfxUserFontEntry* CreateUserFontEntry();
  gfxUserFontEntry* GetUserFontEntry() const { return mUserFontEntry; }
  void SetUserFontEntry(gfxUserFontEntry* aEntry);

  /**
   * Returns whether this object is in the specified FontFaceSet.
   */
  bool IsInFontFaceSet(FontFaceSetImpl* aFontFaceSet) const;

  void AddFontFaceSet(FontFaceSetImpl* aFontFaceSet);
  void RemoveFontFaceSet(FontFaceSetImpl* aFontFaceSet);

  FontFaceSetImpl* GetPrimaryFontFaceSet() const { return mFontFaceSet; }

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
   * Takes the gfxFontFaceBufferSource to represent the font data
   * in this object.
   */
  already_AddRefed<gfxFontFaceBufferSource> TakeBufferSource();

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
  void Load(ErrorResult& aRv);

  void Destroy();

  FontFaceImpl(FontFace* aOwner, FontFaceSetImpl* aFontFaceSet);

  void InitializeSourceURL(const nsACString& aURL);
  void InitializeSourceBuffer(uint8_t* aBuffer, uint32_t aLength);

  /**
   * Sets all of the descriptor values in mDescriptors using values passed
   * to the JS constructor.
   * Returns true on success, false if parsing any descriptor failed.
   */
  bool SetDescriptors(const nsACString& aFamily,
                      const FontFaceDescriptors& aDescriptors);

 private:
  ~FontFaceImpl();

  // Helper function for Load.
  void DoLoad();
  void UpdateOwnerPromise();

  // Helper function for the descriptor setter methods.
  // Returns true if the descriptor was modified, false if descriptor is
  // unchanged (which may not be an error: check aRv for actual failure).
  bool SetDescriptor(nsCSSFontDesc aFontDesc, const nsACString& aValue,
                     ErrorResult& aRv);

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

  StyleLockedFontFaceRule* GetData() const {
    AssertIsOnOwningThread();
    return HasRule() ? mRule : mDescriptors;
  }

  /**
   * Returns and takes ownership of the buffer storing the font data.
   */
  void TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength);

  FontFace* MOZ_NON_OWNING_REF mOwner;

  // The @font-face rule this FontFace object is reflecting, if it is a
  // rule backed FontFace.
  RefPtr<StyleLockedFontFaceRule> mRule;

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
  RefPtr<FontFaceBufferSource> mBufferSource;

  // The values corresponding to the font face descriptors, if we are not
  // a rule backed FontFace object.  For rule backed objects, we use
  // the descriptors stored in mRule.
  // FIXME This should hold a unique ptr to just the descriptors inside,
  // so that we don't need to create a rule for it and don't need to
  // assign a fake line number and column number. See bug 1450904.
  RefPtr<StyleLockedFontFaceRule> mDescriptors;

  // The value of the unicode-range descriptor as a gfxCharacterMap.  Valid
  // only when mUnicodeRangeDirty is false.
  RefPtr<gfxCharacterMap> mUnicodeRange;

  // The primary FontFaceSet this FontFace is associated with,
  // regardless of whether it is currently "in" the set.
  RefPtr<FontFaceSetImpl> mFontFaceSet;

  // Other FontFaceSets (apart from mFontFaceSet) that this FontFace
  // appears in.
  nsTArray<RefPtr<FontFaceSetImpl>> mOtherFontFaceSets;

  // Whether mUnicodeRange needs to be rebuilt before being returned from
  // GetUnicodeRangeAsCharacterMap.
  bool mUnicodeRangeDirty;

  // Whether this FontFace appears in mFontFaceSet.
  bool mInFontFaceSet;
};

}  // namespace mozilla::dom

#endif  // !defined(mozilla_dom_FontFaceImpl_h)
