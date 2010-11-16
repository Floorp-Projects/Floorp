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

#include "nsSVGElement.h"
#include "DOMSVGPathSegList.h"
#include "DOMSVGPathSeg.h"
#include "nsDOMError.h"
#include "SVGAnimatedPathSegList.h"
#include "nsCOMPtr.h"
#include "nsSVGAttrTearoffTable.h"

// See the comment in this file's header.

using namespace mozilla;

static nsSVGAttrTearoffTable<void, DOMSVGPathSegList>
  sSVGPathSegListTearoffTable;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(DOMSVGPathSegList, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGPathSegList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGPathSegList)

DOMCI_DATA(SVGPathSegList, DOMSVGPathSegList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGPathSegList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPathSegList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPathSegList)
NS_INTERFACE_MAP_END


/* static */ already_AddRefed<DOMSVGPathSegList>
DOMSVGPathSegList::GetDOMWrapper(void *aList,
                                 nsSVGElement *aElement,
                                 PRBool aIsAnimValList)
{
  DOMSVGPathSegList *wrapper =
    sSVGPathSegListTearoffTable.GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGPathSegList(aElement, aIsAnimValList);
    sSVGPathSegListTearoffTable.AddTearoff(aList, wrapper);
  }
  NS_ADDREF(wrapper);
  return wrapper;
}

/* static */ DOMSVGPathSegList*
DOMSVGPathSegList::GetDOMWrapperIfExists(void *aList)
{
  return sSVGPathSegListTearoffTable.GetTearoff(aList);
}

DOMSVGPathSegList::~DOMSVGPathSegList()
{
  // We no longer have any list items, and there are no script references to
  // us.
  //
  // Do NOT use InternalList() here! That's different!
  void *key = mIsAnimValList ?
    InternalAList().GetAnimValKey() :
    InternalAList().GetBaseValKey();
  sSVGPathSegListTearoffTable.RemoveTearoff(key);
}

void
DOMSVGPathSegList::InternalListWillChangeTo(const SVGPathData& aNewValue)
{
  // When the number of items in our internal counterpart changes, we MUST stay
  // in sync. Everything in the scary comment in
  // DOMSVGLengthList::InternalBaseValListWillChangeTo applies here just as
  // much, but we have the additional issue that failing to stay in sync would
  // mean that - assuming we aren't reading bad memory - we would likely end up
  // decoding command types from argument floats when looking in our
  // SVGPathData's data array! Either way, we'll likely then go down
  // NS_NOTREACHED code paths, or end up reading/setting more bad memory!!

  // The only time that our other DOM list type implementations remove items is
  // if those items become surplus items due to an attribute change or SMIL
  // animation sample shortening the list. In general though, they try to keep
  // their existing DOM items, even when things change. To be consistent, we'd
  // really like to do the same thing. However, because different types of path
  // segment correspond to different DOMSVGPathSeg subclasses, the type of
  // items in our list are generally not the same, which makes this harder for
  // us. We have to remove DOM segments if their type is not the same as the
  // type of the new internal segment at their index.
  //
  // We also need to sync up mInternalDataIndex, but since we need to loop over
  // all the items in the new list checking types anyway, that's almost
  // insignificant in terms of overhead.
  //
  // Note that this method is called on every single SMIL animation resample
  // and we have no way to short circuit the overhead since we don't have a
  // way to tell if the call is due to a new animation, or a resample of an
  // existing animation (when the number and type of items would be the same).
  // (Note that a new animation could start overriding an existing animation at
  // any time, so checking IsAnimating() wouldn't work.) Because we get called
  // on every sample, it would not be acceptable alternative to throw away all
  // our items and let them be recreated lazily, since that would break what
  // script sees!

  PRUint32 length = mItems.Length();
  PRUint32 index = 0;

  PRUint32 dataLength = aNewValue.mData.Length();
  PRUint32 dataIndex = 0; // index into aNewValue's raw data array

  PRUint32 newSegType;

  while (index < length && dataIndex < dataLength) {
    newSegType = SVGPathSegUtils::DecodeType(aNewValue.mData[dataIndex]);
    if (ItemAt(index) && ItemAt(index)->Type() != newSegType) {
      ItemAt(index)->RemovingFromList();
      ItemAt(index) = nsnull;
    }
    // Only after the RemovingFromList() can we touch mInternalDataIndex!
    mItems[index].mInternalDataIndex = dataIndex;
    ++index;
    dataIndex += 1 + SVGPathSegUtils::ArgCountForType(newSegType);
  }

  NS_ABORT_IF_FALSE((index == length && dataIndex <= dataLength) ||
                    (index <= length && dataIndex == dataLength),
                    "very bad - list corruption?");

  if (index < length) {
    // aNewValue has fewer items than our previous internal counterpart

    PRUint32 newLength = index;

    // Remove excess items from the list:
    for (; index < length; ++index) {
      if (ItemAt(index)) {
        ItemAt(index)->RemovingFromList();
        ItemAt(index) = nsnull;
      }
    }

    // Only now may we truncate mItems
    mItems.SetLength(newLength);

  } else if (dataIndex < dataLength) {
    // aNewValue has more items than our previous internal counterpart

    // Sync mItems:
    while (dataIndex < dataLength) {
      if (!mItems.AppendElement(ItemProxy(nsnull, dataIndex))) {
        // OOM
        Clear();
        return;
      }
      dataIndex += 1 + SVGPathSegUtils::ArgCountForType(SVGPathSegUtils::DecodeType(aNewValue.mData[dataIndex]));
    }
  }

  NS_ABORT_IF_FALSE(dataIndex == dataLength, "Serious processing error");
  NS_ABORT_IF_FALSE(index == length, "Serious counting error");
}

PRBool
DOMSVGPathSegList::AttrIsAnimating() const
{
  return const_cast<DOMSVGPathSegList*>(this)->InternalAList().IsAnimating();
}

SVGPathData&
DOMSVGPathSegList::InternalList()
{
  SVGAnimatedPathSegList *alist = mElement->GetAnimPathSegList();
  return mIsAnimValList && alist->IsAnimating() ? *alist->mAnimVal : alist->mBaseVal;
}

SVGAnimatedPathSegList&
DOMSVGPathSegList::InternalAList()
{
  NS_ABORT_IF_FALSE(mElement->GetAnimPathSegList(), "Internal error");
  return *mElement->GetAnimPathSegList();
}

// ----------------------------------------------------------------------------
// nsIDOMSVGPathSegList implementation:

NS_IMETHODIMP
DOMSVGPathSegList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
#ifdef MOZ_SMIL
  if (IsAnimValList()) {
    Element()->FlushAnimations();
  }
#endif
  *aNumberOfItems = Length();
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPathSegList::Clear()
{
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (Length() > 0) {
    // DOM list items that are to be removed must be removed before we change
    // the internal list, otherwise they wouldn't be able to copy their
    // internal counterparts' values!

    InternalListWillChangeTo(SVGPathData()); // clears mItems

    if (!AttrIsAnimating()) {
      // The anim val list is in sync with the base val list
      DOMSVGPathSegList *animList =
        GetDOMWrapperIfExists(InternalAList().GetAnimValKey());
      if (animList) {
        animList->InternalListWillChangeTo(SVGPathData()); // clears its mItems
      }
    }

    InternalList().Clear();
    Element()->DidChangePathSegList(PR_TRUE);
#ifdef MOZ_SMIL
    if (AttrIsAnimating()) {
      Element()->AnimationNeedsResample();
    }
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPathSegList::Initialize(nsIDOMSVGPathSeg *aNewItem,
                              nsIDOMSVGPathSeg **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  // If aNewItem is already in a list we should insert a clone of aNewItem,
  // and for consistency, this should happen even if *this* is the list that
  // aNewItem is currently in. Note that in the case of aNewItem being in this
  // list, the Clear() call before the InsertItemBefore() call would remove it
  // from this list, and so the InsertItemBefore() call would not insert a
  // clone of aNewItem, it would actually insert aNewItem. To prevent that
  // from happening we have to do the clone here, if necessary.

  nsCOMPtr<DOMSVGPathSeg> domItem = do_QueryInterface(aNewItem);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  if (domItem->HasOwner()) {
    aNewItem = domItem->Clone();
  }

  Clear();
  return InsertItemBefore(aNewItem, 0, _retval);
}

NS_IMETHODIMP
DOMSVGPathSegList::GetItem(PRUint32 aIndex,
                           nsIDOMSVGPathSeg **_retval)
{
#ifdef MOZ_SMIL
  if (IsAnimValList()) {
    Element()->FlushAnimations();
  }
#endif
  if (aIndex < Length()) {
    EnsureItemAt(aIndex);
    NS_ADDREF(*_retval = ItemAt(aIndex));
    return NS_OK;
  }
  *_retval = nsnull;
  return NS_ERROR_DOM_INDEX_SIZE_ERR;
}

NS_IMETHODIMP
DOMSVGPathSegList::InsertItemBefore(nsIDOMSVGPathSeg *aNewItem,
                                    PRUint32 aIndex,
                                    nsIDOMSVGPathSeg **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  nsCOMPtr<DOMSVGPathSeg> domItem = do_QueryInterface(aNewItem);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  if (domItem->HasOwner()) {
    domItem = domItem->Clone(); // must do this before changing anything!
  }
  PRUint32 internalIndex;
  if (aIndex < Length()) {
    internalIndex = mItems[aIndex].mInternalDataIndex;
  } else {
    aIndex = Length();
    internalIndex = InternalList().mData.Length();
  }
  PRUint32 argCount = SVGPathSegUtils::ArgCountForType(domItem->Type());

  // Ensure we have enough memory so we can avoid complex error handling below:
  if (!mItems.SetCapacity(mItems.Length() + 1) ||
      !InternalList().mData.SetCapacity(InternalList().mData.Length() + 1 + argCount)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  float segAsRaw[1 + NS_SVG_PATH_SEG_MAX_ARGS];
  domItem->ToSVGPathSegEncodedData(segAsRaw);

  InternalList().mData.InsertElementsAt(internalIndex, segAsRaw, 1 + argCount);
  mItems.InsertElementAt(aIndex, ItemProxy(domItem.get(), internalIndex));

  // This MUST come after the insertion into InternalList(), or else under the
  // insertion into InternalList() the data read from domItem would be bad data
  // from InternalList() itself!:
  domItem->InsertingIntoList(this, aIndex, IsAnimValList());

  for (PRUint32 i = aIndex + 1; i < Length(); ++i) {
    mItems[i].mInternalDataIndex += 1 + argCount;
    if (ItemAt(i)) {
      ItemAt(i)->UpdateListIndex(i);
    }
  }

  Element()->DidChangePathSegList(PR_TRUE);
#ifdef MOZ_SMIL
  if (AttrIsAnimating()) {
    Element()->AnimationNeedsResample();
  }
#endif
  *_retval = domItem.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPathSegList::ReplaceItem(nsIDOMSVGPathSeg *aNewItem,
                               PRUint32 aIndex,
                               nsIDOMSVGPathSeg **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  nsCOMPtr<DOMSVGPathSeg> domItem = do_QueryInterface(aNewItem);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  if (aIndex >= Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  if (domItem->HasOwner()) {
    domItem = domItem->Clone(); // must do this before changing anything!
  }

  if (ItemAt(aIndex)) {
    // Notify any existing DOM item of removal *before* modifying the lists so
    // that the DOM item can copy the *old* value at its index:
    ItemAt(aIndex)->RemovingFromList();
  }

  PRUint32 internalIndex = mItems[aIndex].mInternalDataIndex;
  // We use InternalList() to get oldArgCount since we may not have a DOM
  // wrapper at the index being replaced.
  PRUint32 oldType = SVGPathSegUtils::DecodeType(InternalList().mData[internalIndex]);
  PRUint32 oldArgCount = SVGPathSegUtils::ArgCountForType(oldType);
  PRUint32 newArgCount = SVGPathSegUtils::ArgCountForType(domItem->Type());

  float segAsRaw[1 + NS_SVG_PATH_SEG_MAX_ARGS];
  domItem->ToSVGPathSegEncodedData(segAsRaw);

  PRBool ok = !!InternalList().mData.ReplaceElementsAt(
                  internalIndex, 1 + oldArgCount,
                  segAsRaw, 1 + newArgCount);
  if (!ok) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  ItemAt(aIndex) = domItem;

  // This MUST come after the ToSVGPathSegEncodedData call otherwise that call
  // would end up reading bad data from InternalList()!
  domItem->InsertingIntoList(this, aIndex, IsAnimValList());

  PRUint32 delta = newArgCount - oldArgCount;
  if (delta != 0) {
    for (PRUint32 i = aIndex + 1; i < Length(); ++i) {
      mItems[i].mInternalDataIndex += delta;
    }
  }

  Element()->DidChangePathSegList(PR_TRUE);
#ifdef MOZ_SMIL
  if (AttrIsAnimating()) {
    Element()->AnimationNeedsResample();
  }
#endif
  NS_ADDREF(*_retval = domItem.get());
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPathSegList::RemoveItem(PRUint32 aIndex,
                              nsIDOMSVGPathSeg **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (aIndex >= Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  // We have to return the removed item, so make sure it exists:
  EnsureItemAt(aIndex);

  // Notify the DOM item of removal *before* modifying the lists so that the
  // DOM item can copy its *old* value:
  ItemAt(aIndex)->RemovingFromList();
  NS_ADDREF(*_retval = ItemAt(aIndex));

  PRUint32 internalIndex = mItems[aIndex].mInternalDataIndex;
  PRUint32 segType = SVGPathSegUtils::DecodeType(InternalList().mData[internalIndex]);
  PRUint32 argCount = SVGPathSegUtils::ArgCountForType(segType);

  InternalList().mData.RemoveElementsAt(internalIndex, 1 + argCount);
  mItems.RemoveElementAt(aIndex);

  for (PRUint32 i = aIndex; i < Length(); ++i) {
    mItems[i].mInternalDataIndex -= 1 + argCount;
    if (ItemAt(i)) {
      ItemAt(i)->UpdateListIndex(i);
    }
  }

  Element()->DidChangePathSegList(PR_TRUE);
#ifdef MOZ_SMIL
  if (AttrIsAnimating()) {
    Element()->AnimationNeedsResample();
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPathSegList::AppendItem(nsIDOMSVGPathSeg *aNewItem,
                              nsIDOMSVGPathSeg **_retval)
{
  return InsertItemBefore(aNewItem, Length(), _retval);
}

void
DOMSVGPathSegList::EnsureItemAt(PRUint32 aIndex)
{
  if (!ItemAt(aIndex)) {
    ItemAt(aIndex) = DOMSVGPathSeg::CreateFor(this, aIndex, IsAnimValList());
  }
}

