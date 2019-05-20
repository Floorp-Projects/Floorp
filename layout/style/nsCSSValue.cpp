/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of simple property values within CSS declarations */

#include "nsCSSValue.h"

#include "mozilla/CORSMode.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoTypes.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/ImageLoader.h"
#include "gfxFontConstants.h"
#include "imgIRequest.h"
#include "imgRequestProxy.h"
#include "mozilla/dom/Document.h"
#include "nsIURIMutator.h"
#include "nsCSSProps.h"
#include "nsNetUtil.h"
#include "nsPresContext.h"
#include "nsStyleUtil.h"
#include "nsDeviceContext.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

nsCSSValue::nsCSSValue(int32_t aValue, nsCSSUnit aUnit) : mUnit(aUnit) {
  MOZ_ASSERT(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated,
             "not an int value");
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated) {
    mValue.mInt = aValue;
  } else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(float aValue, nsCSSUnit aUnit) : mUnit(aUnit) {
  MOZ_ASSERT(eCSSUnit_Percent <= aUnit, "not a float value");
  if (eCSSUnit_Percent <= aUnit) {
    mValue.mFloat = aValue;
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
  } else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(const nsCSSValue& aCopy) : mUnit(aCopy.mUnit) {
  if (eCSSUnit_Percent <= mUnit) {
    mValue.mFloat = aCopy.mValue.mFloat;
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
  } else if (eCSSUnit_Integer <= mUnit && mUnit <= eCSSUnit_Enumerated) {
    mValue.mInt = aCopy.mValue.mInt;
  } else {
    MOZ_ASSERT_UNREACHABLE("unknown unit");
  }
}

nsCSSValue& nsCSSValue::operator=(const nsCSSValue& aCopy) {
  if (this != &aCopy) {
    this->~nsCSSValue();
    new (this) nsCSSValue(aCopy);
  }
  return *this;
}

nsCSSValue& nsCSSValue::operator=(nsCSSValue&& aOther) {
  MOZ_ASSERT(this != &aOther, "Self assigment with rvalue reference");

  Reset();
  mUnit = aOther.mUnit;
  mValue = aOther.mValue;
  aOther.mUnit = eCSSUnit_Null;

  return *this;
}

bool nsCSSValue::operator==(const nsCSSValue& aOther) const {
  if (mUnit != aOther.mUnit) {
    return false;
  }
  if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    return mValue.mInt == aOther.mValue.mInt;
  }
  return mValue.mFloat == aOther.mValue.mFloat;
}

double nsCSSValue::GetAngleValueInDegrees() const {
  // Note that this extends the value from float to double.
  return GetAngleValue();
}

double nsCSSValue::GetAngleValueInRadians() const {
  return GetAngleValueInDegrees() * M_PI / 180.0;
}

nscoord nsCSSValue::GetPixelLength() const {
  MOZ_ASSERT(IsPixelLengthUnit(), "not a fixed length unit");

  double scaleFactor;
  switch (mUnit) {
    case eCSSUnit_Pixel:
      return nsPresContext::CSSPixelsToAppUnits(mValue.mFloat);
    case eCSSUnit_Pica:
      scaleFactor = 16.0;
      break;
    case eCSSUnit_Point:
      scaleFactor = 4 / 3.0;
      break;
    case eCSSUnit_Inch:
      scaleFactor = 96.0;
      break;
    case eCSSUnit_Millimeter:
      scaleFactor = 96 / 25.4;
      break;
    case eCSSUnit_Centimeter:
      scaleFactor = 96 / 2.54;
      break;
    case eCSSUnit_Quarter:
      scaleFactor = 96 / 101.6;
      break;
    default:
      NS_ERROR("should never get here");
      return 0;
  }
  return nsPresContext::CSSPixelsToAppUnits(float(mValue.mFloat * scaleFactor));
}

void nsCSSValue::SetIntValue(int32_t aValue, nsCSSUnit aUnit) {
  MOZ_ASSERT(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated,
             "not an int value");
  Reset();
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
}

void nsCSSValue::SetPercentValue(float aValue) {
  Reset();
  mUnit = eCSSUnit_Percent;
  mValue.mFloat = aValue;
  MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
}

void nsCSSValue::SetFloatValue(float aValue, nsCSSUnit aUnit) {
  MOZ_ASSERT(IsFloatUnit(aUnit), "not a float value");
  Reset();
  if (IsFloatUnit(aUnit)) {
    mUnit = aUnit;
    mValue.mFloat = aValue;
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
  }
}

css::URLValue::~URLValue() {
  if (mLoadID != 0) {
    ImageLoader::DeregisterCSSImageFromAllLoaders(this);
  }
}

bool css::URLValue::Equals(const URLValue& aOther) const {
  MOZ_ASSERT(NS_IsMainThread());

  bool eq;
  const URLExtraData* self = ExtraData();
  const URLExtraData* other = aOther.ExtraData();
  return GetString() == aOther.GetString() &&
         (GetURI() == aOther.GetURI() ||  // handles null == null
          (mURI && aOther.mURI &&
           NS_SUCCEEDED(mURI->Equals(aOther.mURI, &eq)) && eq)) &&
         (self->BaseURI() == other->BaseURI() ||
          (NS_SUCCEEDED(self->BaseURI()->Equals(other->BaseURI(), &eq)) &&
           eq)) &&
         self->Principal()->Equals(other->Principal()) &&
         IsLocalRef() == aOther.IsLocalRef();
}

bool css::URLValue::DefinitelyEqualURIs(const URLValue& aOther) const {
  if (ExtraData()->BaseURI() != aOther.ExtraData()->BaseURI()) {
    return false;
  }
  return GetString() == aOther.GetString();
}

bool css::URLValue::DefinitelyEqualURIsAndPrincipal(
    const URLValue& aOther) const {
  return ExtraData()->Principal() == aOther.ExtraData()->Principal() &&
         DefinitelyEqualURIs(aOther);
}

nsDependentCSubstring css::URLValue::GetString() const {
  const uint8_t* chars;
  uint32_t len;
  Servo_CssUrlData_GetSerialization(mCssUrl, &chars, &len);
  return nsDependentCSubstring(reinterpret_cast<const char*>(chars), len);
}

nsIURI* css::URLValue::GetURI() const {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mURIResolved) {
    MOZ_ASSERT(!mURI);
    nsCOMPtr<nsIURI> newURI;
    NS_NewURI(getter_AddRefs(newURI), GetString(), nullptr,
              ExtraData()->BaseURI());
    mURI = newURI.forget();
    mURIResolved = true;
  }

  return mURI;
}

bool css::URLValue::HasRef() const {
  if (IsLocalRef()) {
    return true;
  }

  if (nsIURI* uri = GetURI()) {
    nsAutoCString ref;
    nsresult rv = uri->GetRef(ref);
    return NS_SUCCEEDED(rv) && !ref.IsEmpty();
  }

  return false;
}

already_AddRefed<nsIURI> css::URLValue::ResolveLocalRef(nsIURI* aURI) const {
  nsCOMPtr<nsIURI> result = GetURI();

  if (result && IsLocalRef()) {
    nsCString ref;
    mURI->GetRef(ref);

    nsresult rv = NS_MutateURI(aURI).SetRef(ref).Finalize(result);

    if (NS_FAILED(rv)) {
      // If setting the ref failed, just return the original URI.
      result = aURI;
    }
  }

  return result.forget();
}

already_AddRefed<nsIURI> css::URLValue::ResolveLocalRef(
    nsIContent* aContent) const {
  nsCOMPtr<nsIURI> url = aContent->GetBaseURI();
  return ResolveLocalRef(url);
}

void css::URLValue::GetSourceString(nsString& aRef) const {
  nsIURI* uri = GetURI();
  if (!uri) {
    aRef.Truncate();
    return;
  }

  nsCString cref;
  if (IsLocalRef()) {
    // XXXheycam It's possible we can just return mString in this case, since
    // it should be the "#fragment" string the URLValue was created with.
    uri->GetRef(cref);
    cref.Insert('#', 0);
  } else {
    // It's not entirely clear how to best handle failure here. Ensuring the
    // string is empty seems safest.
    nsresult rv = uri->GetSpec(cref);
    if (NS_FAILED(rv)) {
      cref.Truncate();
    }
  }

  aRef = NS_ConvertUTF8toUTF16(cref);
}

size_t css::URLValue::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mURI
  // - mString

  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
  }
  return n;
}

imgRequestProxy* css::URLValue::LoadImage(Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());

  static uint64_t sNextLoadID = 1;

  if (mLoadID == 0) {
    mLoadID = sNextLoadID++;
  }

  // NB: If aDocument is not the original document, we may not be able to load
  // images from aDocument.  Instead we do the image load from the original doc
  // and clone it to aDocument.
  Document* loadingDoc = aDocument->GetOriginalDocument();
  if (!loadingDoc) {
    loadingDoc = aDocument;
  }

  // Kick off the load in the loading document.
  ImageLoader::LoadImage(this, loadingDoc);

  // Register the image in the document that's using it.
  return aDocument->StyleImageLoader()->RegisterCSSImage(this);
}

size_t mozilla::css::GridTemplateAreasValue::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mNamedAreas.ShallowSizeOfExcludingThis(aMallocSizeOf);
    n += mTemplates.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}
