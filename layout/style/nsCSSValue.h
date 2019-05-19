/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of simple property values within CSS declarations */

#ifndef nsCSSValue_h___
#define nsCSSValue_h___

#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/URLExtraData.h"
#include "mozilla/UniquePtr.h"

#include "nsCSSKeywords.h"
#include "nsCSSPropertyID.h"
#include "nsCoord.h"
#include "nsProxyRelease.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsTArray.h"
#include "nsStyleConsts.h"
#include "nsStyleCoord.h"
#include "gfxFontFamilyList.h"

#include <type_traits>

class imgRequestProxy;
class nsAtom;
class nsIContent;

class nsIPrincipal;
class nsIURI;
class nsPresContext;
template <class T>
class nsPtrHashKey;
struct RawServoCssUrlData;

namespace mozilla {
class CSSStyleSheet;
}  // namespace mozilla

// Deletes a linked list iteratively to avoid blowing up the stack (bug 456196).
#define NS_CSS_DELETE_LIST_MEMBER(type_, ptr_, member_) \
  {                                                     \
    type_* cur = (ptr_)->member_;                       \
    (ptr_)->member_ = nullptr;                          \
    while (cur) {                                       \
      type_* dlm_next = cur->member_;                   \
      cur->member_ = nullptr;                           \
      delete cur;                                       \
      cur = dlm_next;                                   \
    }                                                   \
  }
// Ditto, but use NS_RELEASE instead of 'delete' (bug 1221902).
#define NS_CSS_NS_RELEASE_LIST_MEMBER(type_, ptr_, member_) \
  {                                                         \
    type_* cur = (ptr_)->member_;                           \
    (ptr_)->member_ = nullptr;                              \
    while (cur) {                                           \
      type_* dlm_next = cur->member_;                       \
      cur->member_ = nullptr;                               \
      NS_RELEASE(cur);                                      \
      cur = dlm_next;                                       \
    }                                                       \
  }

// Clones a linked list iteratively to avoid blowing up the stack.
// If it fails to clone the entire list then 'to_' is deleted and
// we return null.
#define NS_CSS_CLONE_LIST_MEMBER(type_, from_, member_, to_, args_)      \
  {                                                                      \
    type_* dest = (to_);                                                 \
    (to_)->member_ = nullptr;                                            \
    for (const type_* src = (from_)->member_; src; src = src->member_) { \
      type_* clm_clone = src->Clone args_;                               \
      if (!clm_clone) {                                                  \
        delete (to_);                                                    \
        return nullptr;                                                  \
      }                                                                  \
      dest->member_ = clm_clone;                                         \
      dest = clm_clone;                                                  \
    }                                                                    \
  }

// Forward declaration copied here since ServoBindings.h #includes nsCSSValue.h.
extern "C" {
mozilla::URLExtraData* Servo_CssUrlData_GetExtraData(const RawServoCssUrlData*);
bool Servo_CssUrlData_IsLocalRef(const RawServoCssUrlData* url);
}

namespace mozilla {
namespace css {

struct URLValue final {
 public:
  // aCssUrl must not be null.
  URLValue(already_AddRefed<RawServoCssUrlData> aCssUrl, CORSMode aCORSMode)
      : mURIResolved(false), mCssUrl(aCssUrl), mCORSMode(aCORSMode) {
    MOZ_ASSERT(mCssUrl);
  }

  // Returns true iff all fields of the two URLValue objects are equal.
  //
  // Only safe to call on the main thread, since this will call Equals on the
  // nsIURI and nsIPrincipal objects stored on the URLValue objects.
  bool Equals(const URLValue& aOther) const;

  // Returns true iff we know for sure, by comparing the mBaseURI pointer,
  // the specified url() value mString, and IsLocalRef(), that these
  // two URLValue objects represent the same computed url() value.
  //
  // Doesn't look at mReferrer or mOriginPrincipal.
  //
  // Safe to call from any thread.
  bool DefinitelyEqualURIs(const URLValue& aOther) const;

  // Smae as DefinitelyEqualURIs but additionally compares the nsIPrincipal
  // pointers of the two URLValue objects.
  bool DefinitelyEqualURIsAndPrincipal(const URLValue& aOther) const;

  nsIURI* GetURI() const;

  bool IsLocalRef() const { return Servo_CssUrlData_IsLocalRef(mCssUrl); }

  bool HasRef() const;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URLValue)

  // When matching a local ref URL, resolve it against aURI;
  // Otherwise, ignore aURL and return mURI directly.
  already_AddRefed<nsIURI> ResolveLocalRef(nsIURI* aURI) const;
  already_AddRefed<nsIURI> ResolveLocalRef(nsIContent* aContent) const;

  // Serializes mURI as a computed URI value, taking into account IsLocalRef()
  // and serializing just the fragment if true.
  void GetSourceString(nsString& aRef) const;

  nsDependentCSubstring GetString() const;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  imgRequestProxy* LoadImage(mozilla::dom::Document* aDocument);

  uint64_t LoadID() const { return mLoadID; }

  CORSMode CorsMode() const { return mCORSMode; }

  URLExtraData* ExtraData() const {
    return Servo_CssUrlData_GetExtraData(mCssUrl);
  }

 private:
  // mURI stores the lazily resolved URI.  This may be null if the URI is
  // invalid, even once resolved.
  mutable nsCOMPtr<nsIURI> mURI;

  mutable bool mURIResolved;

  RefPtr<RawServoCssUrlData> mCssUrl;

  const CORSMode mCORSMode;

  // A unique, non-reused ID value for this URLValue over the life of the
  // process.  This value is only valid after LoadImage has been called.
  //
  // We use this as a key in some tables in ImageLoader.  This is better than
  // using the pointer value of the ImageValue object, since we can sometimes
  // delete ImageValues OMT but cannot update the ImageLoader tables until
  // we're back on the main thread.  So to avoid dangling pointers that might
  // get re-used by the time we want to update the ImageLoader tables, we use
  // these IDs.
  uint64_t mLoadID = 0;

  ~URLValue();

 private:
  URLValue(const URLValue& aOther) = delete;
  URLValue& operator=(const URLValue& aOther) = delete;
};

struct GridNamedArea {
  nsString mName;
  uint32_t mColumnStart;
  uint32_t mColumnEnd;
  uint32_t mRowStart;
  uint32_t mRowEnd;
};

struct GridTemplateAreasValue final {
  // Parsed value
  nsTArray<GridNamedArea> mNamedAreas;

  // Original <string> values. Length gives the number of rows,
  // content makes serialization easier.
  nsTArray<nsString> mTemplates;

  // How many columns grid-template-areas contributes to the explicit grid.
  // http://dev.w3.org/csswg/css-grid/#explicit-grid
  uint32_t mNColumns;

  // How many rows grid-template-areas contributes to the explicit grid.
  // http://dev.w3.org/csswg/css-grid/#explicit-grid
  uint32_t NRows() const { return mTemplates.Length(); }

  GridTemplateAreasValue()
      : mNColumns(0)
  // Default constructors for mNamedAreas and mTemplates: empty arrays.
  {}

  bool operator==(const GridTemplateAreasValue& aOther) const {
    return mTemplates == aOther.mTemplates;
  }

  bool operator!=(const GridTemplateAreasValue& aOther) const {
    return !(*this == aOther);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GridTemplateAreasValue)

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  // Private destructor to make sure this isn't used as a stack variable
  // or member variable.
  ~GridTemplateAreasValue() {}

  GridTemplateAreasValue(const GridTemplateAreasValue& aOther) = delete;
  GridTemplateAreasValue& operator=(const GridTemplateAreasValue& aOther) =
      delete;
};

}  // namespace css
}  // namespace mozilla

enum nsCSSUnit : uint32_t {
  eCSSUnit_Null = 0,     // (n/a) null unit, value is not specified

  eCSSUnit_Integer = 70,     // (int) simple value
  eCSSUnit_Enumerated = 71,  // (int) value has enumerated meaning

  eCSSUnit_Percent = 100,  // (float) (1.0 == 100%) value is percentage of
                           // something
  eCSSUnit_Number = 101,   // (float) value is numeric (usually multiplier,
                           // different behavior than percent)

  // Font relative measure
  eCSSUnit_EM = 800,       // (float) == current font size
  eCSSUnit_XHeight = 801,  // (float) distance from top of lower case x to
                           // baseline
  eCSSUnit_Char = 802,     // (float) number of characters, used for width with
                           // monospace font
  eCSSUnit_RootEM = 803,   // (float) == root element font size

  // Screen relative measure
  eCSSUnit_Point = 900,       // (float) 4/3 of a CSS pixel
  eCSSUnit_Inch = 901,        // (float) 96 CSS pixels
  eCSSUnit_Millimeter = 902,  // (float) 96/25.4 CSS pixels
  eCSSUnit_Centimeter = 903,  // (float) 96/2.54 CSS pixels
  eCSSUnit_Pica = 904,        // (float) 12 points == 16 CSS pixls
  eCSSUnit_Quarter = 905,     // (float) 96/101.6 CSS pixels
  eCSSUnit_Pixel = 906,       // (float) CSS pixel unit

  // Angular units
  eCSSUnit_Degree = 1000,  // (float) 360 per circle

  // Frequency units
  eCSSUnit_Hertz = 2000,      // (float) 1/seconds
  eCSSUnit_Kilohertz = 2001,  // (float) 1000 Hertz

  // Time units
  eCSSUnit_Seconds = 3000,       // (float) Standard time
  eCSSUnit_Milliseconds = 3001,  // (float) 1/1000 second

  // Flexible fraction (CSS Grid)
  eCSSUnit_FlexFraction = 4000,  // (float) Fraction of free space
};

struct nsCSSValuePair;
struct nsCSSValuePair_heap;
struct nsCSSValueList;
struct nsCSSValueList_heap;
struct nsCSSValueSharedList;
struct nsCSSValuePairList;
struct nsCSSValuePairList_heap;

class nsCSSValue {
 public:
  explicit nsCSSValue() : mUnit(eCSSUnit_Null) {}

  nsCSSValue(int32_t aValue, nsCSSUnit aUnit);
  nsCSSValue(float aValue, nsCSSUnit aUnit);
  nsCSSValue(const nsCSSValue& aCopy);
  nsCSSValue(nsCSSValue&& aOther) : mUnit(aOther.mUnit), mValue(aOther.mValue) {
    aOther.mUnit = eCSSUnit_Null;
  }
  template <typename T,
            typename = typename std::enable_if<std::is_enum<T>::value>::type>
  explicit nsCSSValue(T aValue) : mUnit(eCSSUnit_Enumerated) {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within mValue.mInt");
    mValue.mInt = static_cast<int32_t>(aValue);
  }

  nsCSSValue& operator=(const nsCSSValue& aCopy);
  nsCSSValue& operator=(nsCSSValue&& aCopy);
  bool operator==(const nsCSSValue& aOther) const;

  bool operator!=(const nsCSSValue& aOther) const { return !(*this == aOther); }

  nsCSSUnit GetUnit() const { return mUnit; }
  bool IsLengthUnit() const {
    return eCSSUnit_EM <= mUnit && mUnit <= eCSSUnit_Pixel;
  }
  bool IsLengthPercentUnit() const {
    return IsLengthUnit() || mUnit == eCSSUnit_Percent;
  }
  /**
   * What the spec calls relative length units is, for us, split
   * between relative length units and pixel length units.
   *
   * A "relative" length unit is a multiple of some derived metric,
   * such as a font em-size, which itself was controlled by an input CSS
   * length. Relative length units should not be scaled by zooming, since
   * the underlying CSS length would already have been scaled.
   */
  bool IsRelativeLengthUnit() const {
    return eCSSUnit_EM <= mUnit && mUnit <= eCSSUnit_RootEM;
  }
  /**
   * A "pixel" length unit is a some multiple of CSS pixels.
   */
  static bool IsPixelLengthUnit(nsCSSUnit aUnit) {
    return eCSSUnit_Point <= aUnit && aUnit <= eCSSUnit_Pixel;
  }
  bool IsPixelLengthUnit() const { return IsPixelLengthUnit(mUnit); }
  static bool IsPercentLengthUnit(nsCSSUnit aUnit) {
    return aUnit == eCSSUnit_Percent;
  }
  bool IsPercentLengthUnit() { return IsPercentLengthUnit(mUnit); }
  static bool IsFloatUnit(nsCSSUnit aUnit) { return eCSSUnit_Number <= aUnit; }
  bool IsAngularUnit() const { return eCSSUnit_Degree == mUnit; }
  bool IsFrequencyUnit() const {
    return eCSSUnit_Hertz <= mUnit && mUnit <= eCSSUnit_Kilohertz;
  }
  bool IsTimeUnit() const {
    return eCSSUnit_Seconds <= mUnit && mUnit <= eCSSUnit_Milliseconds;
  }

  int32_t GetIntValue() const {
    MOZ_ASSERT(mUnit == eCSSUnit_Integer || mUnit == eCSSUnit_Enumerated,
               "not an int value");
    return mValue.mInt;
  }

  nsCSSKeyword GetKeywordValue() const {
    MOZ_ASSERT(mUnit == eCSSUnit_Enumerated, "not a keyword value");
    return static_cast<nsCSSKeyword>(mValue.mInt);
  }

  float GetPercentValue() const {
    MOZ_ASSERT(mUnit == eCSSUnit_Percent, "not a percent value");
    return mValue.mFloat;
  }

  float GetFloatValue() const {
    MOZ_ASSERT(eCSSUnit_Number <= mUnit, "not a float value");
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
    return mValue.mFloat;
  }

  float GetAngleValue() const {
    MOZ_ASSERT(eCSSUnit_Degree == mUnit, "not an angle value");
    return mValue.mFloat;
  }

  // Converts any angle to radians.
  double GetAngleValueInRadians() const;

  // Converts any angle to degrees.
  double GetAngleValueInDegrees() const;

  nscoord GetPixelLength() const;

  void Reset() { mUnit = eCSSUnit_Null; }
  ~nsCSSValue() { Reset(); }

 public:
  void SetIntValue(int32_t aValue, nsCSSUnit aUnit);
  template <typename T,
            typename = typename std::enable_if<std::is_enum<T>::value>::type>
  void SetEnumValue(T aValue) {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within mValue.mInt");
    SetIntValue(static_cast<int32_t>(aValue), eCSSUnit_Enumerated);
  }
  void SetPercentValue(float aValue);
  void SetFloatValue(float aValue, nsCSSUnit aUnit);
  // converts the nscoord to pixels
  void SetIntegerCoordValue(nscoord aCoord);

 protected:
  nsCSSUnit mUnit;
  union {
    int32_t mInt;
    float mFloat;
  } mValue;
};

#endif /* nsCSSValue_h___ */
