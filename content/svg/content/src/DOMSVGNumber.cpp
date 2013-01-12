/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGNumber.h"
#include "DOMSVGNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "nsSVGElement.h"
#include "nsIDOMSVGNumber.h"
#include "nsError.h"
#include "nsContentUtils.h"

// See the architecture comment in DOMSVGAnimatedNumberList.h.

using namespace mozilla;

// We could use NS_IMPL_CYCLE_COLLECTION_1, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGNumber)
  // We may not belong to a list, so we must null check tmp->mList.
  if (tmp->mList) {
    tmp->mList->mItems[tmp->mListIndex] = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK(mList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGNumber)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mList)
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
                           uint8_t aAttrEnum,
                           uint32_t aListIndex,
                           bool aIsAnimValItem)
  : mList(aList)
  , mListIndex(aListIndex)
  , mAttrEnum(aAttrEnum)
  , mIsAnimValItem(aIsAnimValItem)
  , mValue(0.0f)
{
  // These shifts are in sync with the members in the header.
  NS_ABORT_IF_FALSE(aList &&
                    aAttrEnum < (1 << 4) &&
                    aListIndex <= MaxListIndex(), "bad arg");

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGNumber!");
}

DOMSVGNumber::DOMSVGNumber()
  : mList(nullptr)
  , mListIndex(0)
  , mAttrEnum(0)
  , mIsAnimValItem(false)
  , mValue(0.0f)
{
}

NS_IMETHODIMP
DOMSVGNumber::GetValue(float* aValue)
{
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == false
  }
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
    if (InternalItem() == aValue) {
      return NS_OK;
    }
    nsAttrValue emptyOrOldValue = Element()->WillChangeNumberList(mAttrEnum);
    InternalItem() = aValue;
    Element()->DidChangeNumberList(mAttrEnum, emptyOrOldValue);
    if (mList->mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
    return NS_OK;
  }
  mValue = aValue;
  return NS_OK;
}

void
DOMSVGNumber::InsertingIntoList(DOMSVGNumberList *aList,
                                uint8_t aAttrEnum,
                                uint32_t aListIndex,
                                bool aIsAnimValItem)
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
  mList = nullptr;
  mIsAnimValItem = false;
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

