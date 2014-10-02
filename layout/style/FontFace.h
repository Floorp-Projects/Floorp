/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFace_h
#define mozilla_dom_FontFace_h

#include "mozilla/dom/FontFaceBinding.h"
#include "gfxUserFontSet.h"
#include "nsCSSProperty.h"
#include "nsCSSValue.h"
#include "nsWrapperCache.h"

class nsCSSFontFaceRule;
class nsPresContext;

namespace mozilla {
struct CSSFontFaceDescriptors;
namespace dom {
struct FontFaceDescriptors;
class Promise;
class StringOrArrayBufferOrArrayBufferView;
}
}

namespace mozilla {
namespace dom {

class FontFace MOZ_FINAL : public nsISupports,
                           public nsWrapperCache
{
  friend class Entry;

public:
  class Entry MOZ_FINAL : public gfxUserFontEntry {
  public:
    Entry(gfxUserFontSet* aFontSet,
          const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
          uint32_t aWeight,
          int32_t aStretch,
          uint32_t aItalicStyle,
          const nsTArray<gfxFontFeature>& aFeatureSettings,
          uint32_t aLanguageOverride,
          gfxSparseBitSet* aUnicodeRanges)
      : gfxUserFontEntry(aFontSet, aFontFaceSrcList, aWeight, aStretch,
                         aItalicStyle, aFeatureSettings, aLanguageOverride,
                         aUnicodeRanges) {}

    virtual void SetLoadState(UserFontLoadState aLoadState) MOZ_OVERRIDE;

  protected:
    // Look up this user font entry's corresponding FontFace object on the
    // FontFaceSet.  Can return null.
    FontFace* GetFontFace();
  };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FontFace)

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<FontFace> CreateForRule(
                                              nsISupports* aGlobal,
                                              nsPresContext* aPresContext,
                                              nsCSSFontFaceRule* aRule,
                                              gfxUserFontEntry* aUserFontEntry);

  // Web IDL
  static already_AddRefed<FontFace>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aFamily,
              const mozilla::dom::StringOrArrayBufferOrArrayBufferView& aSource,
              const mozilla::dom::FontFaceDescriptors& aDescriptors,
              ErrorResult& aRV);

  void GetFamily(nsString& aResult);
  void SetFamily(const nsAString& aValue, mozilla::ErrorResult& aRv);
  void GetStyle(nsString& aResult);
  void SetStyle(const nsAString& aValue, mozilla::ErrorResult& aRv);
  void GetWeight(nsString& aResult);
  void SetWeight(const nsAString& aValue, mozilla::ErrorResult& aRv);
  void GetStretch(nsString& aResult);
  void SetStretch(const nsAString& aValue, mozilla::ErrorResult& aRv);
  void GetUnicodeRange(nsString& aResult);
  void SetUnicodeRange(const nsAString& aValue, mozilla::ErrorResult& aRv);
  void GetVariant(nsString& aResult);
  void SetVariant(const nsAString& aValue, mozilla::ErrorResult& aRv);
  void GetFeatureSettings(nsString& aResult);
  void SetFeatureSettings(const nsAString& aValue, mozilla::ErrorResult& aRv);

  mozilla::dom::FontFaceLoadStatus Status();
  mozilla::dom::Promise* Load();
  mozilla::dom::Promise* Loaded();

private:
  FontFace(nsISupports* aParent, nsPresContext* aPresContext);
  ~FontFace();

  /**
   * Parses a @font-face descriptor value, storing the result in aResult.
   * Returns whether the parsing was successful.
   */
  bool ParseDescriptor(nsCSSFontDesc aDescID, const nsAString& aString,
                       nsCSSValue& aResult);

  // Helper function for the descriptor setter methods.
  void SetDescriptor(nsCSSFontDesc aFontDesc,
                     const nsAString& aValue,
                     mozilla::ErrorResult& aRv);

  /**
   * Sets the current loading status.
   */
  void SetStatus(mozilla::dom::FontFaceLoadStatus aStatus);

  void GetDesc(nsCSSFontDesc aDescID, nsCSSValue& aResult) const;
  void GetDesc(nsCSSFontDesc aDescID,
               nsCSSProperty aPropID,
               nsString& aResult) const;

  nsCOMPtr<nsISupports> mParent;
  nsPresContext* mPresContext;

  nsRefPtr<mozilla::dom::Promise> mLoaded;

  // The @font-face rule this FontFace object is reflecting, if it is a
  // CSS-connected FontFace.
  nsRefPtr<nsCSSFontFaceRule> mRule;

  // The current load status of the font represented by this FontFace.
  // Note that we can't just reflect the value of the gfxUserFontEntry's
  // status, since the spec sometimes requires us to go through the event
  // loop before updating the status, rather than doing it immediately.
  mozilla::dom::FontFaceLoadStatus mStatus;

  // The values corresponding to the font face descriptors, if we are not
  // a CSS-connected FontFace object.  For CSS-connected objects, we use
  // the descriptors stored in mRule.
  nsAutoPtr<mozilla::CSSFontFaceDescriptors> mDescriptors;
};

} // namespace dom
} // namespace mozilla

#endif // !defined(mozilla_dom_FontFace_h)
