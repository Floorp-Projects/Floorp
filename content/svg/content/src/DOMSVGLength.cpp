/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    tmp->mList->mItems[tmp->mListIndex] = nsnull;
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
  : mList(nsnull)
  , mListIndex(0)
  , mAttrEnum(0)
  , mIsAnimValItem(PR_FALSE)
  , mUnit(nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER)
  , mValue(0.0f)
{
}

NS_IMETHODIMP
DOMSVGLength::GetUnitType(PRUint16* aUnit)
{
#ifdef MOZ_SMIL
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == PR_FALSE
  }
#endif
  *aUnit = HasOwner() ? InternalItem().GetUnit() : mUnit;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGLength::GetValue(float* aValue)
{
#ifdef MOZ_SMIL
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == PR_FALSE
  }
#endif
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
    if (InternalItem().SetFromUserUnitValue(aUserUnitValue, Element(), Axis())) {
      Element()->DidChangeLengthList(mAttrEnum, PR_TRUE);
#ifdef MOZ_SMIL
      if (mList->mAList->IsAnimating()) {
        Element()->AnimationNeedsResample();
      }
#endif
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
#ifdef MOZ_SMIL
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == PR_FALSE
  }
#endif
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
    InternalItem().SetValueInCurrentUnits(aValue);
    Element()->DidChangeLengthList(mAttrEnum, PR_TRUE);
#ifdef MOZ_SMIL
    if (mList->mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
#endif
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
    InternalItem() = value;
    Element()->DidChangeLengthList(mAttrEnum, PR_TRUE);
#ifdef MOZ_SMIL
    if (mList->mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
#endif
    return NS_OK;
  }
  mValue = value.GetValueInCurrentUnits();
  mUnit = value.GetUnit();
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGLength::GetValueAsString(nsAString& aValue)
{
#ifdef MOZ_SMIL
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == PR_FALSE
  }
#endif
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
    InternalItem().SetValueAndUnit(aValue, PRUint8(aUnit));
    Element()->DidChangeLengthList(mAttrEnum, PR_TRUE);
#ifdef MOZ_SMIL
    if (mList->mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
#endif
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
    if (InternalItem().ConvertToUnit(PRUint8(aUnit), Element(), Axis())) {
      return NS_OK;
    }
  } else {
    SVGLength len(mValue, mUnit);
    if (len.ConvertToUnit(PRUint8(aUnit), nsnull, 0)) {
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
  mList = nsnull;
  mIsAnimValItem = PR_FALSE;
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
PRBool
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
