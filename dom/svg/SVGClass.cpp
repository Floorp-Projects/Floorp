/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGClass.h"

#include "mozilla/dom/SVGAnimatedString.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/Move.h"
#include "mozilla/SMILValue.h"
#include "SMILStringType.h"

namespace mozilla {
namespace dom {

struct DOMAnimatedString final : public SVGAnimatedString {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMAnimatedString)

  DOMAnimatedString(SVGClass* aVal, SVGElement* aSVGElement)
      : SVGAnimatedString(aSVGElement), mVal(aVal) {}

  SVGClass* mVal;  // kept alive because it belongs to content

  void GetBaseVal(nsAString& aResult) override {
    mVal->GetBaseValue(aResult, mSVGElement);
  }

  void SetBaseVal(const nsAString& aValue) override {
    mVal->SetBaseValue(aValue, mSVGElement, true);
  }

  void GetAnimVal(nsAString& aResult) override;

 private:
  ~DOMAnimatedString() {}
};

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMAnimatedString, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMAnimatedString)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMAnimatedString)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMAnimatedString)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<SVGAnimatedString> SVGClass::ToDOMAnimatedString(
    SVGElement* aSVGElement) {
  RefPtr<DOMAnimatedString> result = new DOMAnimatedString(this, aSVGElement);
  return result.forget();
}

/* Implementation */

void SVGClass::SetBaseValue(const nsAString& aValue, SVGElement* aSVGElement,
                            bool aDoSetAttr) {
  NS_ASSERTION(aSVGElement, "Null element passed to SetBaseValue");

  aSVGElement->SetMayHaveClass();
  if (aDoSetAttr) {
    aSVGElement->SetAttr(kNameSpaceID_None, nsGkAtoms::_class, aValue, true);
  }
  if (mAnimVal) {
    aSVGElement->AnimationNeedsResample();
  }
}

void SVGClass::GetBaseValue(nsAString& aValue,
                            const SVGElement* aSVGElement) const {
  aSVGElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_class, aValue);
}

void SVGClass::GetAnimValue(nsAString& aResult,
                            const SVGElement* aSVGElement) const {
  if (mAnimVal) {
    aResult = *mAnimVal;
    return;
  }

  aSVGElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_class, aResult);
}

void SVGClass::SetAnimValue(const nsAString& aValue, SVGElement* aSVGElement) {
  if (mAnimVal && mAnimVal->Equals(aValue)) {
    return;
  }
  if (!mAnimVal) {
    mAnimVal = new nsString();
  }
  *mAnimVal = aValue;
  aSVGElement->SetMayHaveClass();
  aSVGElement->DidAnimateClass();
}

void DOMAnimatedString::GetAnimVal(nsAString& aResult) {
  mSVGElement->FlushAnimations();
  mVal->GetAnimValue(aResult, mSVGElement);
}

UniquePtr<SMILAttr> SVGClass::ToSMILAttr(SVGElement* aSVGElement) {
  return MakeUnique<SMILString>(this, aSVGElement);
}

nsresult SVGClass::SMILString::ValueFromString(
    const nsAString& aStr, const dom::SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  SMILValue val(SMILStringType::Singleton());

  *static_cast<nsAString*>(val.mU.mPtr) = aStr;
  aValue = std::move(val);
  aPreventCachingOfSandwich = false;
  return NS_OK;
}

SMILValue SVGClass::SMILString::GetBaseValue() const {
  SMILValue val(SMILStringType::Singleton());
  mSVGElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                       *static_cast<nsAString*>(val.mU.mPtr));
  return val;
}

void SVGClass::SMILString::ClearAnimValue() {
  if (mVal->mAnimVal) {
    mVal->mAnimVal = nullptr;
    mSVGElement->DidAnimateClass();
  }
}

nsresult SVGClass::SMILString::SetAnimValue(const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == SMILStringType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILStringType::Singleton()) {
    mVal->SetAnimValue(*static_cast<nsAString*>(aValue.mU.mPtr), mSVGElement);
  }
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
