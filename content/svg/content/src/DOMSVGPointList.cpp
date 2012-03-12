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
#include "DOMSVGPointList.h"
#include "DOMSVGPoint.h"
#include "nsDOMError.h"
#include "SVGAnimatedPointList.h"
#include "nsCOMPtr.h"
#include "nsSVGAttrTearoffTable.h"

// See the comment in this file's header.

// local helper functions
namespace {

using mozilla::DOMSVGPoint;

void
UpdateListIndicesFromIndex(nsTArray<DOMSVGPoint*>& aItemsArray,
                           PRUint32 aStartingIndex)
{
  PRUint32 length = aItemsArray.Length();

  for (PRUint32 i = aStartingIndex; i < length; ++i) {
    if (aItemsArray[i]) {
      aItemsArray[i]->UpdateListIndex(i);
    }
  }
}

} // namespace

namespace mozilla {

static nsSVGAttrTearoffTable<void, DOMSVGPointList>
  sSVGPointListTearoffTable;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(DOMSVGPointList, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGPointList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGPointList)

} // namespace mozilla
DOMCI_DATA(SVGPointList, mozilla::DOMSVGPointList)
namespace mozilla {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGPointList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPointList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPointList)
NS_INTERFACE_MAP_END


/* static */ already_AddRefed<DOMSVGPointList>
DOMSVGPointList::GetDOMWrapper(void *aList,
                               nsSVGElement *aElement,
                               bool aIsAnimValList)
{
  DOMSVGPointList *wrapper =
    sSVGPointListTearoffTable.GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGPointList(aElement, aIsAnimValList);
    sSVGPointListTearoffTable.AddTearoff(aList, wrapper);
  }
  NS_ADDREF(wrapper);
  return wrapper;
}

/* static */ DOMSVGPointList*
DOMSVGPointList::GetDOMWrapperIfExists(void *aList)
{
  return sSVGPointListTearoffTable.GetTearoff(aList);
}

DOMSVGPointList::~DOMSVGPointList()
{
  // There are now no longer any references to us held by script or list items.
  // Note we must use GetAnimValKey/GetBaseValKey here, NOT InternalList()!
  void *key = mIsAnimValList ?
    InternalAList().GetAnimValKey() :
    InternalAList().GetBaseValKey();
  sSVGPointListTearoffTable.RemoveTearoff(key);
}

nsIDOMSVGPoint*
DOMSVGPointList::GetItemWithoutAddRef(PRUint32 aIndex)
{
  if (IsAnimValList()) {
    Element()->FlushAnimations();
  }
  if (aIndex < Length()) {
    EnsureItemAt(aIndex);
    return mItems[aIndex];
  }
  return nsnull;
}

void
DOMSVGPointList::InternalListWillChangeTo(const SVGPointList& aNewValue)
{
  // When the number of items in our internal counterpart changes, we MUST stay
  // in sync. Everything in the scary comment in
  // DOMSVGLengthList::InternalBaseValListWillChangeTo applies here too!

  PRUint32 oldLength = mItems.Length();

  PRUint32 newLength = aNewValue.Length();
  if (newLength > DOMSVGPoint::MaxListIndex()) {
    // It's safe to get out of sync with our internal list as long as we have
    // FEWER items than it does.
    newLength = DOMSVGPoint::MaxListIndex();
  }

  nsRefPtr<DOMSVGPointList> kungFuDeathGrip;
  if (newLength < oldLength) {
    // RemovingFromList() might clear last reference to |this|.
    // Retain a temporary reference to keep from dying before returning.
    kungFuDeathGrip = this;
  }

  // If our length will decrease, notify the items that will be removed:
  for (PRUint32 i = newLength; i < oldLength; ++i) {
    if (mItems[i]) {
      mItems[i]->RemovingFromList();
    }
  }

  if (!mItems.SetLength(newLength)) {
    // We silently ignore SetLength OOM failure since being out of sync is safe
    // so long as we have *fewer* items than our internal list.
    mItems.Clear();
    return;
  }

  // If our length has increased, null out the new pointers:
  for (PRUint32 i = oldLength; i < newLength; ++i) {
    mItems[i] = nsnull;
  }
}

bool
DOMSVGPointList::AttrIsAnimating() const
{
  return InternalAList().IsAnimating();
}

SVGPointList&
DOMSVGPointList::InternalList() const
{
  SVGAnimatedPointList *alist = mElement->GetAnimatedPointList();
  return mIsAnimValList && alist->IsAnimating() ? *alist->mAnimVal : alist->mBaseVal;
}

SVGAnimatedPointList&
DOMSVGPointList::InternalAList() const
{
  NS_ABORT_IF_FALSE(mElement->GetAnimatedPointList(), "Internal error");
  return *mElement->GetAnimatedPointList();
}

// ----------------------------------------------------------------------------
// nsIDOMSVGPointList implementation:

NS_IMETHODIMP
DOMSVGPointList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
  if (IsAnimValList()) {
    Element()->FlushAnimations();
  }
  *aNumberOfItems = Length();
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPointList::Clear()
{
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (Length() > 0) {
    nsAttrValue emptyOrOldValue = Element()->WillChangePointList();
    // DOM list items that are to be removed must be removed before we change
    // the internal list, otherwise they wouldn't be able to copy their
    // internal counterparts' values!

    InternalListWillChangeTo(SVGPointList()); // clears mItems

    if (!AttrIsAnimating()) {
      // The anim val list is in sync with the base val list
      DOMSVGPointList *animList =
        GetDOMWrapperIfExists(InternalAList().GetAnimValKey());
      if (animList) {
        animList->InternalListWillChangeTo(SVGPointList()); // clears its mItems
      }
    }

    InternalList().Clear();
    Element()->DidChangePointList(emptyOrOldValue);
    if (AttrIsAnimating()) {
      Element()->AnimationNeedsResample();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPointList::Initialize(nsIDOMSVGPoint *aNewItem,
                            nsIDOMSVGPoint **_retval)
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

  nsCOMPtr<DOMSVGPoint> domItem = do_QueryInterface(aNewItem);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  if (domItem->HasOwner() || domItem->IsReadonly()) {
    aNewItem = domItem->Clone();
  }

  Clear();
  return InsertItemBefore(aNewItem, 0, _retval);
}

NS_IMETHODIMP
DOMSVGPointList::GetItem(PRUint32 aIndex,
                         nsIDOMSVGPoint **_retval)
{
  *_retval = GetItemWithoutAddRef(aIndex);
  if (!*_retval) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPointList::InsertItemBefore(nsIDOMSVGPoint *aNewItem,
                                  PRUint32 aIndex,
                                  nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  aIndex = NS_MIN(aIndex, Length());
  if (aIndex >= DOMSVGPoint::MaxListIndex()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<DOMSVGPoint> domItem = do_QueryInterface(aNewItem);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  if (domItem->HasOwner() || domItem->IsReadonly()) {
    domItem = domItem->Clone(); // must do this before changing anything!
  }

  // Ensure we have enough memory so we can avoid complex error handling below:
  if (!mItems.SetCapacity(mItems.Length() + 1) ||
      !InternalList().SetCapacity(InternalList().Length() + 1)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAttrValue emptyOrOldValue = Element()->WillChangePointList();
  // Now that we know we're inserting, keep animVal list in sync as necessary.
  MaybeInsertNullInAnimValListAt(aIndex);

  InternalList().InsertItem(aIndex, domItem->ToSVGPoint());
  mItems.InsertElementAt(aIndex, domItem.get());

  // This MUST come after the insertion into InternalList(), or else under the
  // insertion into InternalList() the values read from domItem would be bad
  // data from InternalList() itself!:
  domItem->InsertingIntoList(this, aIndex, IsAnimValList());

  UpdateListIndicesFromIndex(mItems, aIndex + 1);

  Element()->DidChangePointList(emptyOrOldValue);
  if (AttrIsAnimating()) {
    Element()->AnimationNeedsResample();
  }
  *_retval = domItem.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPointList::ReplaceItem(nsIDOMSVGPoint *aNewItem,
                             PRUint32 aIndex,
                             nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  nsCOMPtr<DOMSVGPoint> domItem = do_QueryInterface(aNewItem);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  if (aIndex >= Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  if (domItem->HasOwner() || domItem->IsReadonly()) {
    domItem = domItem->Clone(); // must do this before changing anything!
  }

  nsAttrValue emptyOrOldValue = Element()->WillChangePointList();
  if (mItems[aIndex]) {
    // Notify any existing DOM item of removal *before* modifying the lists so
    // that the DOM item can copy the *old* value at its index:
    mItems[aIndex]->RemovingFromList();
  }

  InternalList()[aIndex] = domItem->ToSVGPoint();
  mItems[aIndex] = domItem;

  // This MUST come after the ToSVGPoint() call, otherwise that call
  // would end up reading bad data from InternalList()!
  domItem->InsertingIntoList(this, aIndex, IsAnimValList());

  Element()->DidChangePointList(emptyOrOldValue);
  if (AttrIsAnimating()) {
    Element()->AnimationNeedsResample();
  }
  NS_ADDREF(*_retval = domItem.get());
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPointList::RemoveItem(PRUint32 aIndex,
                            nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (aIndex >= Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsAttrValue emptyOrOldValue = Element()->WillChangePointList();
  // Now that we know we're removing, keep animVal list in sync as necessary.
  // Do this *before* touching InternalList() so the removed item can get its
  // internal value.
  MaybeRemoveItemFromAnimValListAt(aIndex);

  // We have to return the removed item, so make sure it exists:
  EnsureItemAt(aIndex);

  // Notify the DOM item of removal *before* modifying the lists so that the
  // DOM item can copy its *old* value:
  mItems[aIndex]->RemovingFromList();
  NS_ADDREF(*_retval = mItems[aIndex]);

  InternalList().RemoveItem(aIndex);
  mItems.RemoveElementAt(aIndex);

  UpdateListIndicesFromIndex(mItems, aIndex);

  Element()->DidChangePointList(emptyOrOldValue);
  if (AttrIsAnimating()) {
    Element()->AnimationNeedsResample();
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPointList::AppendItem(nsIDOMSVGPoint *aNewItem,
                            nsIDOMSVGPoint **_retval)
{
  return InsertItemBefore(aNewItem, Length(), _retval);
}

NS_IMETHODIMP
DOMSVGPointList::GetLength(PRUint32 *aNumberOfItems)
{
  return GetNumberOfItems(aNumberOfItems);
}

void
DOMSVGPointList::EnsureItemAt(PRUint32 aIndex)
{
  if (!mItems[aIndex]) {
    mItems[aIndex] = new DOMSVGPoint(this, aIndex, IsAnimValList());
  }
}

void
DOMSVGPointList::MaybeInsertNullInAnimValListAt(PRUint32 aIndex)
{
  NS_ABORT_IF_FALSE(!IsAnimValList(), "call from baseVal to animVal");

  if (AttrIsAnimating()) {
    // animVal not a clone of baseVal
    return;
  }

  // The anim val list is in sync with the base val list
  DOMSVGPointList *animVal =
    GetDOMWrapperIfExists(InternalAList().GetAnimValKey());
  if (!animVal) {
    // No animVal list wrapper
    return;
  }

  NS_ABORT_IF_FALSE(animVal->mItems.Length() == mItems.Length(),
                    "animVal list not in sync!");

  animVal->mItems.InsertElementAt(aIndex, static_cast<DOMSVGPoint*>(nsnull));

  UpdateListIndicesFromIndex(animVal->mItems, aIndex + 1);
}

void
DOMSVGPointList::MaybeRemoveItemFromAnimValListAt(PRUint32 aIndex)
{
  NS_ABORT_IF_FALSE(!IsAnimValList(), "call from baseVal to animVal");

  if (AttrIsAnimating()) {
    // animVal not a clone of baseVal
    return;
  }

  // This needs to be a strong reference; otherwise, the RemovingFromList call
  // below might drop the last reference to animVal before we're done with it.
  nsRefPtr<DOMSVGPointList> animVal =
    GetDOMWrapperIfExists(InternalAList().GetAnimValKey());
  if (!animVal) {
    // No animVal list wrapper
    return;
  }

  NS_ABORT_IF_FALSE(animVal->mItems.Length() == mItems.Length(),
                    "animVal list not in sync!");

  if (animVal->mItems[aIndex]) {
    animVal->mItems[aIndex]->RemovingFromList();
  }
  animVal->mItems.RemoveElementAt(aIndex);

  UpdateListIndicesFromIndex(animVal->mItems, aIndex);
}

} // namespace mozilla
