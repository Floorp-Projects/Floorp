/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAttrValueInlines_h__
#define nsAttrValueInlines_h__

#include <stdint.h>

#include "nsAttrValue.h"
#include "mozilla/Attributes.h"

struct MiscContainer;

struct MiscContainer final
{
  typedef nsAttrValue::ValueType ValueType;

  ValueType mType;
  // mStringBits points to either nsIAtom* or nsStringBuffer* and is used when
  // mType isn't eCSSDeclaration.
  // Note eStringBase and eAtomBase is used also to handle the type of
  // mStringBits.
  uintptr_t mStringBits;
  union {
    struct {
      union {
        int32_t mInteger;
        nscolor mColor;
        uint32_t mEnumValue;
        int32_t mPercent;
        mozilla::css::Declaration* mCSSDeclaration;
        mozilla::css::URLValue* mURL;
        mozilla::css::ImageValue* mImage;
        nsAttrValue::AtomArray* mAtomArray;
        nsIntMargin* mIntMargin;
        const nsSVGAngle* mSVGAngle;
        const nsSVGIntegerPair* mSVGIntegerPair;
        const nsSVGLength2* mSVGLength;
        const mozilla::SVGLengthList* mSVGLengthList;
        const mozilla::SVGNumberList* mSVGNumberList;
        const nsSVGNumberPair* mSVGNumberPair;
        const mozilla::SVGPathData* mSVGPathData;
        const mozilla::SVGPointList* mSVGPointList;
        const mozilla::SVGAnimatedPreserveAspectRatio* mSVGPreserveAspectRatio;
        const mozilla::SVGStringList* mSVGStringList;
        const mozilla::SVGTransformList* mSVGTransformList;
        const nsSVGViewBox* mSVGViewBox;
      };
      uint32_t mRefCount : 31;
      uint32_t mCached : 1;
    } mValue;
    double mDoubleValue;
  };

  MiscContainer()
    : mType(nsAttrValue::eColor),
      mStringBits(0)
  {
    MOZ_COUNT_CTOR(MiscContainer);
    mValue.mColor = 0;
    mValue.mRefCount = 0;
    mValue.mCached = 0;
  }

protected:
  // Only nsAttrValue should be able to delete us.
  friend class nsAttrValue;

  ~MiscContainer()
  {
    if (IsRefCounted()) {
      MOZ_ASSERT(mValue.mRefCount == 0);
      MOZ_ASSERT(!mValue.mCached);
    }
    MOZ_COUNT_DTOR(MiscContainer);
  }

public:
  bool GetString(nsAString& aString) const;

  inline bool IsRefCounted() const
  {
    // Nothing stops us from refcounting (and sharing) other types of
    // MiscContainer (except eDoubleValue types) but there's no compelling
    // reason to 
    return mType == nsAttrValue::eCSSDeclaration;
  }

  inline int32_t AddRef() {
    MOZ_ASSERT(IsRefCounted());
    return ++mValue.mRefCount;
  }

  inline int32_t Release() {
    MOZ_ASSERT(IsRefCounted());
    return --mValue.mRefCount;
  }

  void Cache();
  void Evict();
};

/**
 * Implementation of inline methods
 */

inline int32_t
nsAttrValue::GetIntegerValue() const
{
  NS_PRECONDITION(Type() == eInteger, "wrong type");
  return (BaseType() == eIntegerBase)
         ? GetIntInternal()
         : GetMiscContainer()->mValue.mInteger;
}

inline int16_t
nsAttrValue::GetEnumValue() const
{
  NS_PRECONDITION(Type() == eEnum, "wrong type");
  // We don't need to worry about sign extension here since we're
  // returning an int16_t which will cut away the top bits.
  return static_cast<int16_t>((
    (BaseType() == eIntegerBase)
    ? static_cast<uint32_t>(GetIntInternal())
    : GetMiscContainer()->mValue.mEnumValue)
      >> NS_ATTRVALUE_ENUMTABLEINDEX_BITS);
}

inline float
nsAttrValue::GetPercentValue() const
{
  NS_PRECONDITION(Type() == ePercent, "wrong type");
  return ((BaseType() == eIntegerBase)
          ? GetIntInternal()
          : GetMiscContainer()->mValue.mPercent)
            / 100.0f;
}

inline nsAttrValue::AtomArray*
nsAttrValue::GetAtomArrayValue() const
{
  NS_PRECONDITION(Type() == eAtomArray, "wrong type");
  return GetMiscContainer()->mValue.mAtomArray;
}

inline mozilla::css::Declaration*
nsAttrValue::GetCSSDeclarationValue() const
{
  NS_PRECONDITION(Type() == eCSSDeclaration, "wrong type");
  return GetMiscContainer()->mValue.mCSSDeclaration;
}

inline mozilla::css::URLValue*
nsAttrValue::GetURLValue() const
{
  NS_PRECONDITION(Type() == eURL, "wrong type");
  return GetMiscContainer()->mValue.mURL;
}

inline mozilla::css::ImageValue*
nsAttrValue::GetImageValue() const
{
  NS_PRECONDITION(Type() == eImage, "wrong type");
  return GetMiscContainer()->mValue.mImage;
}

inline double
nsAttrValue::GetDoubleValue() const
{
  NS_PRECONDITION(Type() == eDoubleValue, "wrong type");
  return GetMiscContainer()->mDoubleValue;
}

inline bool
nsAttrValue::GetIntMarginValue(nsIntMargin& aMargin) const
{
  NS_PRECONDITION(Type() == eIntMarginValue, "wrong type");
  nsIntMargin* m = GetMiscContainer()->mValue.mIntMargin;
  if (!m)
    return false;
  aMargin = *m;
  return true;
}

inline bool
nsAttrValue::IsSVGType(ValueType aType) const
{
  return aType >= eSVGTypesBegin && aType <= eSVGTypesEnd;
}

inline bool
nsAttrValue::StoresOwnData() const
{
  if (BaseType() != eOtherBase) {
    return true;
  }
  ValueType t = Type();
  return t != eCSSDeclaration && !IsSVGType(t);
}

inline void
nsAttrValue::SetPtrValueAndType(void* aValue, ValueBaseType aType)
{
  NS_ASSERTION(!(NS_PTR_TO_INT32(aValue) & ~NS_ATTRVALUE_POINTERVALUE_MASK),
               "pointer not properly aligned, this will crash");
  mBits = reinterpret_cast<intptr_t>(aValue) | aType;
}

inline void
nsAttrValue::ResetIfSet()
{
  if (mBits) {
    Reset();
  }
}

inline MiscContainer*
nsAttrValue::GetMiscContainer() const
{
  NS_ASSERTION(BaseType() == eOtherBase, "wrong type");
  return static_cast<MiscContainer*>(GetPtr());
}

inline int32_t
nsAttrValue::GetIntInternal() const
{
  NS_ASSERTION(BaseType() == eIntegerBase,
               "getting integer from non-integer");
  // Make sure we get a signed value.
  // Lets hope the optimizer optimizes this into a shift. Unfortunatly signed
  // bitshift right is implementaion dependant.
  return static_cast<int32_t>(mBits & ~NS_ATTRVALUE_INTEGERTYPE_MASK) /
         NS_ATTRVALUE_INTEGERTYPE_MULTIPLIER;
}

#endif
