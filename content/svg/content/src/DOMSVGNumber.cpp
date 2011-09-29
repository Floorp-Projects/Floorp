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

#include "DOMSVGNumber.h"
#include "DOMSVGNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "nsSVGElement.h"
#include "nsIDOMSVGNumber.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"

// See the architecture comment in DOMSVGAnimatedNumberList.h.

using namespace mozilla;

// We could use NS_IMPL_CYCLE_COLLECTION_1, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGNumber)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGNumber)
  // We may not belong to a list, so we must null check tmp->mList.
  if (tmp->mList) {
    tmp->mList->mItems[tmp->mListIndex] = nsnull;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGNumber)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGNumber)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGNumber)

DOMCI_DATA(SVGNumber, DOMSVGNumber)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGNumber)
  NS_INTERFACE_MAP_ENTRY(DOMSVGNumber) // pseudo-interface
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGNumber)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGNumber)
NS_INTERFACE_MAP_END

DOMSVGNumber::DOMSVGNumber(DOMSVGNumberList *aList,
                           PRUint8 aAttrEnum,
                           PRUint32 aListIndex,
                           PRUint8 aIsAnimValItem)
  : mList(aList)
  , mListIndex(aListIndex)
  , mAttrEnum(aAttrEnum)
  , mIsAnimValItem(aIsAnimValItem)
  , mValue(0.0f)
{
  // These shifts are in sync with the members in the header.
  NS_ABORT_IF_FALSE(aList &&
                    aAttrEnum < (1 << 4) &&
                    aListIndex <= MaxListIndex() &&
                    aIsAnimValItem < (1 << 1), "bad arg");

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGNumber!");
}

DOMSVGNumber::DOMSVGNumber()
  : mList(nsnull)
  , mListIndex(0)
  , mAttrEnum(0)
  , mIsAnimValItem(PR_FALSE)
  , mValue(0.0f)
{
}

NS_IMETHODIMP
DOMSVGNumber::GetValue(float* aValue)
{
#ifdef MOZ_SMIL
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == PR_FALSE
  }
#endif
  *aValue = HasOwner() ? InternalItem() : mValue;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGNumber::SetValue(float aValue)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  NS_ENSURE_FINITE(aValue, NS_ERROR_ILLEGAL_VALUE);

  if (HasOwner()) {
    InternalItem() = aValue;
    Element()->DidChangeNumberList(mAttrEnum, PR_TRUE);
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

void
DOMSVGNumber::InsertingIntoList(DOMSVGNumberList *aList,
                                PRUint8 aAttrEnum,
                                PRUint32 aListIndex,
                                PRUint8 aIsAnimValItem)
{
  NS_ASSERTION(!HasOwner(), "Inserting item that is already in a list");

  mList = aList;
  mAttrEnum = aAttrEnum;
  mListIndex = aListIndex;
  mIsAnimValItem = aIsAnimValItem;

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGNumber!");
}

void
DOMSVGNumber::RemovingFromList()
{
  mValue = InternalItem();
  mList = nsnull;
  mIsAnimValItem = PR_FALSE;
}

float
DOMSVGNumber::ToSVGNumber()
{
  return HasOwner() ? InternalItem() : mValue;
}

float&
DOMSVGNumber::InternalItem()
{
  SVGAnimatedNumberList *alist = Element()->GetAnimatedNumberList(mAttrEnum);
  return mIsAnimValItem && alist->mAnimVal ?
    (*alist->mAnimVal)[mListIndex] :
    alist->mBaseVal[mListIndex];
}

#ifdef DEBUG
bool
DOMSVGNumber::IndexIsValid()
{
  SVGAnimatedNumberList *alist = Element()->GetAnimatedNumberList(mAttrEnum);
  return (mIsAnimValItem &&
          mListIndex < alist->GetAnimValue().Length()) ||
         (!mIsAnimValItem &&
          mListIndex < alist->GetBaseValue().Length());
}
#endif

