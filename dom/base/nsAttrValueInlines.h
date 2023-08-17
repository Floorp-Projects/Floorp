/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAttrValueInlines_h__
#define nsAttrValueInlines_h__

#include <stdint.h>

#include "nsAttrValue.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/dom/DOMString.h"

namespace mozilla {
class ShadowParts;
}

struct MiscContainer final {
  using ValueType = nsAttrValue::ValueType;

  ValueType mType;
  // mStringBits points to either nsAtom* or nsStringBuffer* and is used when
  // mType isn't eCSSDeclaration.
  // Note eStringBase and eAtomBase is used also to handle the type of
  // mStringBits.
  //
  // Note that we use an atomic here so that we can use Compare-And-Swap
  // to cache the serialization during the parallel servo traversal. This case
  // (which happens when the main thread is blocked) is the only case where
  // mStringBits is mutated off-main-thread. The Atomic needs to be
  // ReleaseAcquire so that the pointer to the serialization does not become
  // observable to other threads before the initialization of the pointed-to
  // memory is also observable.
  mozilla::Atomic<uintptr_t, mozilla::ReleaseAcquire> mStringBits;
  union {
    struct {
      union {
        int32_t mInteger;
        nscolor mColor;
        uint32_t mEnumValue;
        mozilla::DeclarationBlock* mCSSDeclaration;
        nsIURI* mURL;
        const mozilla::AttrAtomArray* mAtomArray;
        const mozilla::ShadowParts* mShadowParts;
        const mozilla::SVGAnimatedIntegerPair* mSVGAnimatedIntegerPair;
        const mozilla::SVGAnimatedLength* mSVGLength;
        const mozilla::SVGAnimatedNumberPair* mSVGAnimatedNumberPair;
        const mozilla::SVGAnimatedOrient* mSVGAnimatedOrient;
        const mozilla::SVGAnimatedPreserveAspectRatio*
            mSVGAnimatedPreserveAspectRatio;
        const mozilla::SVGAnimatedViewBox* mSVGAnimatedViewBox;
        const mozilla::SVGLengthList* mSVGLengthList;
        const mozilla::SVGNumberList* mSVGNumberList;
        const mozilla::SVGPathData* mSVGPathData;
        const mozilla::SVGPointList* mSVGPointList;
        const mozilla::SVGStringList* mSVGStringList;
        const mozilla::SVGTransformList* mSVGTransformList;
      };
      uint32_t mRefCount : 31;
      uint32_t mCached : 1;
    } mValue;
    double mDoubleValue;
  };

  MiscContainer() : mType(nsAttrValue::eColor), mStringBits(0) {
    MOZ_COUNT_CTOR(MiscContainer);
    mValue.mColor = 0;
    mValue.mRefCount = 0;
    mValue.mCached = 0;
  }

 protected:
  // Only nsAttrValue should be able to delete us.
  friend class nsAttrValue;

  ~MiscContainer() {
    if (IsRefCounted()) {
      MOZ_ASSERT(mValue.mRefCount == 0);
      MOZ_ASSERT(!mValue.mCached);
    }
    MOZ_COUNT_DTOR(MiscContainer);
  }

 public:
  bool GetString(nsAString& aString) const;

  void* GetStringOrAtomPtr(bool& aIsString) const {
    uintptr_t bits = mStringBits;
    aIsString =
        nsAttrValue::ValueBaseType(mStringBits & NS_ATTRVALUE_BASETYPE_MASK) ==
        nsAttrValue::eStringBase;
    return reinterpret_cast<void*>(bits & NS_ATTRVALUE_POINTERVALUE_MASK);
  }

  nsAtom* GetStoredAtom() const {
    bool isString = false;
    void* ptr = GetStringOrAtomPtr(isString);
    return isString ? nullptr : static_cast<nsAtom*>(ptr);
  }

  nsStringBuffer* GetStoredStringBuffer() const {
    bool isString = false;
    void* ptr = GetStringOrAtomPtr(isString);
    return isString ? static_cast<nsStringBuffer*>(ptr) : nullptr;
  }

  void SetStringBitsMainThread(uintptr_t aBits) {
    // mStringBits is atomic, but the callers of this function are
    // single-threaded so they don't have to worry about it.
    MOZ_ASSERT(!mozilla::IsInServoTraversal());
    MOZ_ASSERT(NS_IsMainThread());
    mStringBits = aBits;
  }

  inline bool IsRefCounted() const {
    // Nothing stops us from refcounting (and sharing) other types of
    // MiscContainer (except eDoubleValue types) but there's no compelling
    // reason to.
    return mType == nsAttrValue::eAtomArray ||
           mType == nsAttrValue::eCSSDeclaration ||
           mType == nsAttrValue::eShadowParts;
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

inline int32_t nsAttrValue::GetIntegerValue() const {
  MOZ_ASSERT(Type() == eInteger, "wrong type");
  return (BaseType() == eIntegerBase) ? GetIntInternal()
                                      : GetMiscContainer()->mValue.mInteger;
}

inline int16_t nsAttrValue::GetEnumValue() const {
  MOZ_ASSERT(Type() == eEnum, "wrong type");
  // We don't need to worry about sign extension here since we're
  // returning an int16_t which will cut away the top bits.
  return static_cast<int16_t>(((BaseType() == eIntegerBase)
                                   ? static_cast<uint32_t>(GetIntInternal())
                                   : GetMiscContainer()->mValue.mEnumValue) >>
                              NS_ATTRVALUE_ENUMTABLEINDEX_BITS);
}

inline double nsAttrValue::GetPercentValue() const {
  MOZ_ASSERT(Type() == ePercent, "wrong type");
  if (BaseType() == eIntegerBase) {
    return GetIntInternal() / 100.0f;
  }
  return GetMiscContainer()->mDoubleValue / 100.0f;
}

inline const mozilla::AttrAtomArray* nsAttrValue::GetAtomArrayValue() const {
  MOZ_ASSERT(Type() == eAtomArray, "wrong type");
  return GetMiscContainer()->mValue.mAtomArray;
}

inline mozilla::DeclarationBlock* nsAttrValue::GetCSSDeclarationValue() const {
  MOZ_ASSERT(Type() == eCSSDeclaration, "wrong type");
  return GetMiscContainer()->mValue.mCSSDeclaration;
}

inline nsIURI* nsAttrValue::GetURLValue() const {
  MOZ_ASSERT(Type() == eURL, "wrong type");
  return GetMiscContainer()->mValue.mURL;
}

inline double nsAttrValue::GetDoubleValue() const {
  MOZ_ASSERT(Type() == eDoubleValue, "wrong type");
  return GetMiscContainer()->mDoubleValue;
}

inline bool nsAttrValue::IsSVGType(ValueType aType) const {
  return aType >= eSVGTypesBegin && aType <= eSVGTypesEnd;
}

inline bool nsAttrValue::StoresOwnData() const {
  if (BaseType() != eOtherBase) {
    return true;
  }
  ValueType t = Type();
  return t != eCSSDeclaration && !IsSVGType(t);
}

inline void nsAttrValue::SetPtrValueAndType(void* aValue, ValueBaseType aType) {
  NS_ASSERTION(!(NS_PTR_TO_INT32(aValue) & ~NS_ATTRVALUE_POINTERVALUE_MASK),
               "pointer not properly aligned, this will crash");
  mBits = reinterpret_cast<intptr_t>(aValue) | aType;
}

inline void nsAttrValue::ResetIfSet() {
  if (mBits) {
    Reset();
  }
}

inline MiscContainer* nsAttrValue::GetMiscContainer() const {
  NS_ASSERTION(BaseType() == eOtherBase, "wrong type");
  return static_cast<MiscContainer*>(GetPtr());
}

inline int32_t nsAttrValue::GetIntInternal() const {
  NS_ASSERTION(BaseType() == eIntegerBase, "getting integer from non-integer");
  // Make sure we get a signed value.
  // Lets hope the optimizer optimizes this into a shift. Unfortunatly signed
  // bitshift right is implementaion dependant.
  return static_cast<int32_t>(mBits & ~NS_ATTRVALUE_INTEGERTYPE_MASK) /
         NS_ATTRVALUE_INTEGERTYPE_MULTIPLIER;
}

inline nsAttrValue::ValueType nsAttrValue::Type() const {
  switch (BaseType()) {
    case eIntegerBase: {
      return static_cast<ValueType>(mBits & NS_ATTRVALUE_INTEGERTYPE_MASK);
    }
    case eOtherBase: {
      return GetMiscContainer()->mType;
    }
    default: {
      return static_cast<ValueType>(static_cast<uint16_t>(BaseType()));
    }
  }
}

inline nsAtom* nsAttrValue::GetAtomValue() const {
  MOZ_ASSERT(Type() == eAtom, "wrong type");
  return reinterpret_cast<nsAtom*>(GetPtr());
}

inline void nsAttrValue::ToString(mozilla::dom::DOMString& aResult) const {
  switch (Type()) {
    case eString: {
      nsStringBuffer* str = static_cast<nsStringBuffer*>(GetPtr());
      if (str) {
        aResult.SetKnownLiveStringBuffer(
            str, str->StorageSize() / sizeof(char16_t) - 1);
      }
      // else aResult is already empty
      return;
    }
    case eAtom: {
      nsAtom* atom = static_cast<nsAtom*>(GetPtr());
      aResult.SetKnownLiveAtom(atom, mozilla::dom::DOMString::eNullNotExpected);
      break;
    }
    default: {
      ToString(aResult.AsAString());
    }
  }
}

inline const mozilla::ShadowParts& nsAttrValue::GetShadowPartsValue() const {
  MOZ_ASSERT(Type() == eShadowParts);
  return *GetMiscContainer()->mValue.mShadowParts;
}

#endif
