/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGLength.h"
#include "DOMSVGLengthList.h"
#include "DOMSVGAnimatedLengthList.h"
#include "SVGLength.h"
#include "SVGAnimatedLengthList.h"
#include "nsSVGElement.h"
#include "nsSVGLength2.h"
#include "nsIDOMSVGLength.h"
#include "nsError.h"
#include "nsMathUtils.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/FloatingPoint.h"
#include "nsSVGAttrTearoffTable.h"

// See the architecture comment in DOMSVGAnimatedLengthList.h.

namespace mozilla {

static nsSVGAttrTearoffTable<nsSVGLength2, DOMSVGLength>
  sBaseSVGLengthTearOffTable,
  sAnimSVGLengthTearOffTable;

// We could use NS_IMPL_CYCLE_COLLECTION(, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGLength)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGLength)
  // We may not belong to a list, so we must null check tmp->mList.
  if (tmp->mList) {
    tmp->mList->mItems[tmp->mListIndex] = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK(mList)
NS_IMPL_CYCLE_COLLECTION_UNLINK(mSVGElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGLength)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSVGElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMSVGLength)
NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGLength)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGLength)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGLength)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(mozilla::DOMSVGLength) // pseudo-interface
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLength)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// Helper class: AutoChangeLengthNotifier
// Stack-based helper class to pair calls to WillChangeLengthList and
// DidChangeLengthList.
class MOZ_RAII AutoChangeLengthNotifier
{
public:
  explicit AutoChangeLengthNotifier(DOMSVGLength* aLength MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mLength(aLength)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mLength, "Expecting non-null length");
    MOZ_ASSERT(mLength->HasOwner(),
               "Expecting list to have an owner for notification");
    mEmptyOrOldValue =
      mLength->Element()->WillChangeLengthList(mLength->mAttrEnum);
  }

  ~AutoChangeLengthNotifier()
  {
    mLength->Element()->DidChangeLengthList(mLength->mAttrEnum,
                                            mEmptyOrOldValue);
    if (mLength->mList->IsAnimating()) {
      mLength->Element()->AnimationNeedsResample();
    }
  }

private:
  DOMSVGLength* const mLength;
  nsAttrValue   mEmptyOrOldValue;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

DOMSVGLength::DOMSVGLength(DOMSVGLengthList *aList,
                           uint8_t aAttrEnum,
                           uint32_t aListIndex,
                           bool aIsAnimValItem)
  : mList(aList)
  , mListIndex(aListIndex)
  , mAttrEnum(aAttrEnum)
  , mIsAnimValItem(aIsAnimValItem)
  , mUnit(nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER)
  , mValue(0.0f)
  , mVal(nullptr)
{
  // These shifts are in sync with the members in the header.
  MOZ_ASSERT(aList &&
             aAttrEnum < (1 << 4) &&
             aListIndex <= MaxListIndex(),
             "bad arg");

  MOZ_ASSERT(IndexIsValid(), "Bad index for DOMSVGNumber!");
}

DOMSVGLength::DOMSVGLength()
  : mList(nullptr)
  , mListIndex(0)
  , mAttrEnum(0)
  , mIsAnimValItem(false)
  , mUnit(nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER)
  , mValue(0.0f)
  , mVal(nullptr)
{
}

DOMSVGLength::DOMSVGLength(nsSVGLength2* aVal, nsSVGElement* aSVGElement,
                           bool aAnimVal)
  : mList(nullptr)
  , mListIndex(0)
  , mAttrEnum(0)
  , mIsAnimValItem(aAnimVal)
  , mUnit(nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER)
  , mValue(0.0f)
  , mVal(aVal)
  , mSVGElement(aSVGElement)
{
}

DOMSVGLength::~DOMSVGLength()
{
  // Our mList's weak ref to us must be nulled out when we die. If GC has
  // unlinked us using the cycle collector code, then that has already
  // happened, and mList is null.
  if (mList) {
    mList->mItems[mListIndex] = nullptr;
  }

  if (mVal) {
    auto& table = mIsAnimValItem ? sAnimSVGLengthTearOffTable : sBaseSVGLengthTearOffTable;
    table.RemoveTearoff(mVal);
  }
}

already_AddRefed<DOMSVGLength>
DOMSVGLength::GetTearOff(nsSVGLength2* aVal, nsSVGElement* aSVGElement,
                         bool aAnimVal)
{
  auto& table = aAnimVal ? sAnimSVGLengthTearOffTable : sBaseSVGLengthTearOffTable;
  nsRefPtr<DOMSVGLength> domLength = table.GetTearoff(aVal);
  if (!domLength) {
    domLength = new DOMSVGLength(aVal, aSVGElement, aAnimVal);
    table.AddTearoff(aVal, domLength);
  }

  return domLength.forget();
}

DOMSVGLength*
DOMSVGLength::Copy()
{
  NS_ASSERTION(HasOwner() || IsReflectingAttribute(), "unexpected caller");
  DOMSVGLength *copy = new DOMSVGLength();
  uint16_t unit;
  float value;
  if (mVal) {
    unit = mVal->mSpecifiedUnitType;
    value = mIsAnimValItem ? mVal->mAnimVal : mVal->mBaseVal;
  } else {
    SVGLength &length = InternalItem();
    unit = length.GetUnit();
    value = length.GetValueInCurrentUnits();
  }
  copy->NewValueSpecifiedUnits(unit, value);
  return copy;
}

uint16_t
DOMSVGLength::UnitType()
{
  if (mVal) {
    if (mIsAnimValItem) {
      mSVGElement->FlushAnimations();
    }
    return mVal->mSpecifiedUnitType;
  }

  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == false
  }
  return HasOwner() ? InternalItem().GetUnit() : mUnit;
}

NS_IMETHODIMP
DOMSVGLength::GetUnitType(uint16_t* aUnit)
{
  *aUnit = UnitType();
  return NS_OK;
}

float
DOMSVGLength::GetValue(ErrorResult& aRv)
{
  if (mVal) {
    if (mIsAnimValItem) {
      mSVGElement->FlushAnimations();
      return mVal->GetAnimValue(mSVGElement);
    }
    return mVal->GetBaseValue(mSVGElement);
  }

  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == false
  }
  if (HasOwner()) {
    float value = InternalItem().GetValueInUserUnits(Element(), Axis());
    if (!IsFinite(value)) {
      aRv.Throw(NS_ERROR_FAILURE);
    }
    return value;
  } else if (mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER ||
             mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_PX) {
    return mValue;
  }
  // else [SVGWG issue] Can't convert this length's value to user units
  // ReportToConsole
  aRv.Throw(NS_ERROR_FAILURE);
  return 0.0f;
}

NS_IMETHODIMP
DOMSVGLength::GetValue(float* aValue)
{
  ErrorResult rv;
  *aValue = GetValue(rv);
  return rv.StealNSResult();
}

void
DOMSVGLength::SetValue(float aUserUnitValue, ErrorResult& aRv)
{
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (mVal) {
    mVal->SetBaseValue(aUserUnitValue, mSVGElement, true);
    return;
  }

  // Although the value passed in is in user units, this method does not turn
  // this length into a user unit length. Instead it converts the user unit
  // value to this length's current unit and sets that, leaving this length's
  // unit as it is.

  if (HasOwner()) {
    if (InternalItem().GetValueInUserUnits(Element(), Axis()) ==
        aUserUnitValue) {
      return;
    }
    float uuPerUnit = InternalItem().GetUserUnitsPerUnit(Element(), Axis());
    if (uuPerUnit > 0) {
      float newValue = aUserUnitValue / uuPerUnit;
      if (IsFinite(newValue)) {
        AutoChangeLengthNotifier notifier(this);
        InternalItem().SetValueAndUnit(newValue, InternalItem().GetUnit());
        return;
      }
    }
  } else if (mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER ||
             mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_PX) {
    mValue = aUserUnitValue;
    return;
  }
  // else [SVGWG issue] Can't convert user unit value to this length's unit
  // ReportToConsole
  aRv.Throw(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
DOMSVGLength::SetValue(float aUserUnitValue)
{
  if (!IsFinite(aUserUnitValue)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  ErrorResult rv;
  SetValue(aUserUnitValue, rv);
  return rv.StealNSResult();
}

float
DOMSVGLength::ValueInSpecifiedUnits()
{
  if (mVal) {
    if (mIsAnimValItem) {
      mSVGElement->FlushAnimations();
      return mVal->mAnimVal;
    }
    return mVal->mBaseVal;
  }

  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == false
  }
  return HasOwner() ? InternalItem().GetValueInCurrentUnits() : mValue;
}

NS_IMETHODIMP
DOMSVGLength::GetValueInSpecifiedUnits(float* aValue)
{
  *aValue = ValueInSpecifiedUnits();
  return NS_OK;
}

void
DOMSVGLength::SetValueInSpecifiedUnits(float aValue, ErrorResult& aRv)
{
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (mVal) {
    mVal->SetBaseValueInSpecifiedUnits(aValue, mSVGElement, true);
    return;
  }

  if (HasOwner()) {
    if (InternalItem().GetValueInCurrentUnits() == aValue) {
      return;
    }
    AutoChangeLengthNotifier notifier(this);
    InternalItem().SetValueInCurrentUnits(aValue);
    return;
  }
  mValue = aValue;
}

NS_IMETHODIMP
DOMSVGLength::SetValueInSpecifiedUnits(float aValue)
{
  if (!IsFinite(aValue)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  ErrorResult rv;
  SetValueInSpecifiedUnits(aValue, rv);
  return rv.StealNSResult();
}

void
DOMSVGLength::SetValueAsString(const nsAString& aValue, ErrorResult& aRv)
{
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (mVal) {
    aRv = mVal->SetBaseValueString(aValue, mSVGElement, true);
    return;
  }

  SVGLength value;
  if (!value.SetValueFromString(aValue)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }
  if (HasOwner()) {
    if (InternalItem() == value) {
      return;
    }
    AutoChangeLengthNotifier notifier(this);
    InternalItem() = value;
    return;
  }
  mValue = value.GetValueInCurrentUnits();
  mUnit = value.GetUnit();
}

NS_IMETHODIMP
DOMSVGLength::SetValueAsString(const nsAString& aValue)
{
  ErrorResult rv;
  SetValueAsString(aValue, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
DOMSVGLength::GetValueAsString(nsAString& aValue)
{
  if (mVal) {
    if (mIsAnimValItem) {
      mSVGElement->FlushAnimations();
      mVal->GetAnimValueString(aValue);
    } else {
      mVal->GetBaseValueString(aValue);
    }
    return NS_OK;
  }

  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == false
  }
  if (HasOwner()) {
    InternalItem().GetValueAsString(aValue);
    return NS_OK;
  }
  SVGLength(mValue, mUnit).GetValueAsString(aValue);
  return NS_OK;
}

void
DOMSVGLength::NewValueSpecifiedUnits(uint16_t aUnit, float aValue,
                                     ErrorResult& aRv)
{
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (mVal) {
    mVal->NewValueSpecifiedUnits(aUnit, aValue, mSVGElement);
    return;
  }

  if (!SVGLength::IsValidUnitType(aUnit)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }
  if (HasOwner()) {
    if (InternalItem().GetUnit() == aUnit &&
        InternalItem().GetValueInCurrentUnits() == aValue) {
      return;
    }
    AutoChangeLengthNotifier notifier(this);
    InternalItem().SetValueAndUnit(aValue, uint8_t(aUnit));
    return;
  }
  mUnit = uint8_t(aUnit);
  mValue = aValue;
}

NS_IMETHODIMP
DOMSVGLength::NewValueSpecifiedUnits(uint16_t aUnit, float aValue)
{
  if (!IsFinite(aValue)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  ErrorResult rv;
  NewValueSpecifiedUnits(aUnit, aValue, rv);
  return rv.StealNSResult();
}

void
DOMSVGLength::ConvertToSpecifiedUnits(uint16_t aUnit, ErrorResult& aRv)
{
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (mVal) {
    mVal->ConvertToSpecifiedUnits(aUnit, mSVGElement);
    return;
  }

  if (!SVGLength::IsValidUnitType(aUnit)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }
  if (HasOwner()) {
    if (InternalItem().GetUnit() == aUnit) {
      return;
    }
    float val = InternalItem().GetValueInSpecifiedUnit(
                                 aUnit, Element(), Axis());
    if (IsFinite(val)) {
      AutoChangeLengthNotifier notifier(this);
      InternalItem().SetValueAndUnit(val, aUnit);
      return;
    }
  } else {
    SVGLength len(mValue, mUnit);
    float val = len.GetValueInSpecifiedUnit(aUnit, nullptr, 0);
    if (IsFinite(val)) {
      mValue = val;
      mUnit = aUnit;
      return;
    }
  }
  // else [SVGWG issue] Can't convert unit
  // ReportToConsole
  aRv.Throw(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
DOMSVGLength::ConvertToSpecifiedUnits(uint16_t aUnit)
{
  ErrorResult rv;
  ConvertToSpecifiedUnits(aUnit, rv);
  return rv.StealNSResult();
}

JSObject*
DOMSVGLength::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::SVGLengthBinding::Wrap(aCx, this, aGivenProto);
}

void
DOMSVGLength::InsertingIntoList(DOMSVGLengthList *aList,
                                uint8_t aAttrEnum,
                                uint32_t aListIndex,
                                bool aIsAnimValItem)
{
  NS_ASSERTION(!HasOwner(), "Inserting item that is already in a list");

  mList = aList;
  mAttrEnum = aAttrEnum;
  mListIndex = aListIndex;
  mIsAnimValItem = aIsAnimValItem;

  MOZ_ASSERT(IndexIsValid(), "Bad index for DOMSVGLength!");
}

void
DOMSVGLength::RemovingFromList()
{
  mValue = InternalItem().GetValueInCurrentUnits();
  mUnit  = InternalItem().GetUnit();
  mList = nullptr;
  mIsAnimValItem = false;
}

SVGLength
DOMSVGLength::ToSVGLength()
{
  if (HasOwner()) {
    return SVGLength(InternalItem().GetValueInCurrentUnits(),
                     InternalItem().GetUnit());
  }
  return SVGLength(mValue, mUnit);
}

SVGLength&
DOMSVGLength::InternalItem()
{
  SVGAnimatedLengthList *alist = Element()->GetAnimatedLengthList(mAttrEnum);
  return mIsAnimValItem && alist->mAnimVal ?
    (*alist->mAnimVal)[mListIndex] :
    alist->mBaseVal[mListIndex];
}

#ifdef DEBUG
bool
DOMSVGLength::IndexIsValid()
{
  SVGAnimatedLengthList *alist = Element()->GetAnimatedLengthList(mAttrEnum);
  return (mIsAnimValItem &&
          mListIndex < alist->GetAnimValue().Length()) ||
         (!mIsAnimValItem &&
          mListIndex < alist->GetBaseValue().Length());
}
#endif

} // namespace mozilla
