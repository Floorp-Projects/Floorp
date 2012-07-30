/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGLength.h"
#include "DOMSVGLengthList.h"
#include "DOMSVGAnimatedLengthList.h"
#include "SVGLength.h"
#include "SVGAnimatedLengthList.h"
#include "nsSVGElement.h"
#include "nsIDOMSVGLength.h"
#include "nsDOMError.h"
#include "nsMathUtils.h"

// See the architecture comment in DOMSVGAnimatedLengthList.h.

namespace mozilla {

// We could use NS_IMPL_CYCLE_COLLECTION_1, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGLength)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGLength)
  // We may not belong to a list, so we must null check tmp->mList.
  if (tmp->mList) {
    tmp->mList->mItems[tmp->mListIndex] = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGLength)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGLength)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGLength)

}
DOMCI_DATA(SVGLength, mozilla::DOMSVGLength)
namespace mozilla {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGLength)
  NS_INTERFACE_MAP_ENTRY(mozilla::DOMSVGLength) // pseudo-interface
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLength)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGLength)
NS_INTERFACE_MAP_END

DOMSVGLength::DOMSVGLength(DOMSVGLengthList *aList,
                           PRUint8 aAttrEnum,
                           PRUint32 aListIndex,
                           PRUint8 aIsAnimValItem)
  : mList(aList)
  , mListIndex(aListIndex)
  , mAttrEnum(aAttrEnum)
  , mIsAnimValItem(aIsAnimValItem)
  , mUnit(nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER)
  , mValue(0.0f)
{
  // These shifts are in sync with the members in the header.
  NS_ABORT_IF_FALSE(aList &&
                    aAttrEnum < (1 << 4) &&
                    aListIndex <= MaxListIndex() &&
                    aIsAnimValItem < (1 << 1), "bad arg");

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGNumber!");
}

DOMSVGLength::DOMSVGLength()
  : mList(nullptr)
  , mListIndex(0)
  , mAttrEnum(0)
  , mIsAnimValItem(false)
  , mUnit(nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER)
  , mValue(0.0f)
{
}

NS_IMETHODIMP
DOMSVGLength::GetUnitType(PRUint16* aUnit)
{
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == false
  }
  *aUnit = HasOwner() ? InternalItem().GetUnit() : mUnit;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGLength::GetValue(float* aValue)
{
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == false
  }
  if (HasOwner()) {
    *aValue = InternalItem().GetValueInUserUnits(Element(), Axis());
    if (NS_finite(*aValue)) {
      return NS_OK;
    }
  } else if (mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER ||
             mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_PX) {
    *aValue = mValue;
    return NS_OK;
  }
  // else [SVGWG issue] Can't convert this length's value to user units
  // ReportToConsole
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DOMSVGLength::SetValue(float aUserUnitValue)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (!NS_finite(aUserUnitValue)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Although the value passed in is in user units, this method does not turn
  // this length into a user unit length. Instead it converts the user unit
  // value to this length's current unit and sets that, leaving this length's
  // unit as it is.

  if (HasOwner()) {
    if (InternalItem().GetValueInUserUnits(Element(), Axis()) ==
        aUserUnitValue) {
      return NS_OK;
    }
    nsAttrValue emptyOrOldValue = Element()->WillChangeLengthList(mAttrEnum);
    if (InternalItem().SetFromUserUnitValue(aUserUnitValue, Element(), Axis()))
    {
      Element()->DidChangeLengthList(mAttrEnum, emptyOrOldValue);
      if (mList->mAList->IsAnimating()) {
        Element()->AnimationNeedsResample();
      }
      return NS_OK;
    }
  } else if (mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER ||
             mUnit == nsIDOMSVGLength::SVG_LENGTHTYPE_PX) {
    mValue = aUserUnitValue;
    return NS_OK;
  }
  // else [SVGWG issue] Can't convert user unit value to this length's unit
  // ReportToConsole
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DOMSVGLength::GetValueInSpecifiedUnits(float* aValue)
{
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == false
  }
  *aValue = HasOwner() ? InternalItem().GetValueInCurrentUnits() : mValue;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGLength::SetValueInSpecifiedUnits(float aValue)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (!NS_finite(aValue)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (HasOwner()) {
    if (InternalItem().GetValueInCurrentUnits() == aValue) {
      return NS_OK;
    }
    nsAttrValue emptyOrOldValue = Element()->WillChangeLengthList(mAttrEnum);
    InternalItem().SetValueInCurrentUnits(aValue);
    Element()->DidChangeLengthList(mAttrEnum, emptyOrOldValue);
    if (mList->mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
    return NS_OK;
  }
  mValue = aValue;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGLength::SetValueAsString(const nsAString& aValue)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  SVGLength value;
  if (!value.SetValueFromString(aValue)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  if (HasOwner()) {
    if (InternalItem() == value) {
      return NS_OK;
    }
    nsAttrValue emptyOrOldValue = Element()->WillChangeLengthList(mAttrEnum);
    InternalItem() = value;
    Element()->DidChangeLengthList(mAttrEnum, emptyOrOldValue);
    if (mList->mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
    return NS_OK;
  }
  mValue = value.GetValueInCurrentUnits();
  mUnit = value.GetUnit();
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGLength::GetValueAsString(nsAString& aValue)
{
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

NS_IMETHODIMP
DOMSVGLength::NewValueSpecifiedUnits(PRUint16 aUnit, float aValue)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (!NS_finite(aValue)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (!SVGLength::IsValidUnitType(aUnit)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
  if (HasOwner()) {
    if (InternalItem().GetUnit() == aUnit &&
        InternalItem().GetValueInCurrentUnits() == aValue) {
      return NS_OK;
    }
    nsAttrValue emptyOrOldValue = Element()->WillChangeLengthList(mAttrEnum);
    InternalItem().SetValueAndUnit(aValue, PRUint8(aUnit));
    Element()->DidChangeLengthList(mAttrEnum, emptyOrOldValue);
    if (mList->mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
    return NS_OK;
  }
  mUnit = PRUint8(aUnit);
  mValue = aValue;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGLength::ConvertToSpecifiedUnits(PRUint16 aUnit)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (!SVGLength::IsValidUnitType(aUnit)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
  if (HasOwner()) {
    if (InternalItem().GetUnit() == aUnit) {
      return NS_OK;
    }
    nsAttrValue emptyOrOldValue = Element()->WillChangeLengthList(mAttrEnum);
    if (InternalItem().ConvertToUnit(PRUint8(aUnit), Element(), Axis())) {
      Element()->DidChangeLengthList(mAttrEnum, emptyOrOldValue);
      return NS_OK;
    }
  } else {
    SVGLength len(mValue, mUnit);
    if (len.ConvertToUnit(PRUint8(aUnit), nullptr, 0)) {
      mValue = len.GetValueInCurrentUnits();
      mUnit = aUnit;
      return NS_OK;
    }
  }
  // else [SVGWG issue] Can't convert unit
  // ReportToConsole
  return NS_ERROR_FAILURE;
}

void
DOMSVGLength::InsertingIntoList(DOMSVGLengthList *aList,
                                PRUint8 aAttrEnum,
                                PRUint32 aListIndex,
                                PRUint8 aIsAnimValItem)
{
  NS_ASSERTION(!HasOwner(), "Inserting item that is already in a list");

  mList = aList;
  mAttrEnum = aAttrEnum;
  mListIndex = aListIndex;
  mIsAnimValItem = aIsAnimValItem;

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGLength!");
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
