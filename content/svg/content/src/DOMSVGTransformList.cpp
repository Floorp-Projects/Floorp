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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "DOMSVGTransformList.h"
#include "DOMSVGTransform.h"
#include "DOMSVGMatrix.h"
#include "SVGAnimatedTransformList.h"
#include "nsSVGElement.h"

// local helper functions
namespace {

void UpdateListIndicesFromIndex(
  nsTArray<mozilla::DOMSVGTransform*>& aItemsArray,
  PRUint32 aStartingIndex)
{
  PRUint32 length = aItemsArray.Length();

  for (PRUint32 i = aStartingIndex; i < length; ++i) {
    if (aItemsArray[i]) {
      aItemsArray[i]->UpdateListIndex(i);
    }
  }
}

} // anonymous namespace

namespace mozilla {

// We could use NS_IMPL_CYCLE_COLLECTION_1, except that in Unlink() we need to
// clear our DOMSVGAnimatedTransformList's weak ref to us to be safe. (The other
// option would be to not unlink and rely on the breaking of the other edges in
// the cycle, as NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGTransformList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGTransformList)
  // No need to null check tmp - script/SMIL can't detach us from mAList
  ( tmp->IsAnimValList() ? tmp->mAList->mAnimVal : tmp->mAList->mBaseVal ) =
    nsnull;
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mAList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGTransformList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mAList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGTransformList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGTransformList)

} // namespace mozilla
DOMCI_DATA(SVGTransformList, mozilla::DOMSVGTransformList)
namespace mozilla {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGTransformList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTransformList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGTransformList)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// DOMSVGTransformList methods:

nsIDOMSVGTransform*
DOMSVGTransformList::GetItemWithoutAddRef(PRUint32 aIndex)
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
DOMSVGTransformList::InternalListLengthWillChange(PRUint32 aNewLength)
{
  PRUint32 oldLength = mItems.Length();

  if (aNewLength > DOMSVGTransform::MaxListIndex()) {
    // It's safe to get out of sync with our internal list as long as we have
    // FEWER items than it does.
    aNewLength = DOMSVGTransform::MaxListIndex();
  }

  nsRefPtr<DOMSVGTransformList> kungFuDeathGrip;
  if (aNewLength < oldLength) {
    // RemovingFromList() might clear last reference to |this|.
    // Retain a temporary reference to keep from dying before returning.
    kungFuDeathGrip = this;
  }

  // If our length will decrease, notify the items that will be removed:
  for (PRUint32 i = aNewLength; i < oldLength; ++i) {
    if (mItems[i]) {
      mItems[i]->RemovingFromList();
    }
  }

  if (!mItems.SetLength(aNewLength)) {
    // We silently ignore SetLength OOM failure since being out of sync is safe
    // so long as we have *fewer* items than our internal list.
    mItems.Clear();
    return;
  }

  // If our length has increased, null out the new pointers:
  for (PRUint32 i = oldLength; i < aNewLength; ++i) {
    mItems[i] = nsnull;
  }
}

SVGTransformList&
DOMSVGTransformList::InternalList()
{
  SVGAnimatedTransformList *alist = Element()->GetAnimatedTransformList();
  return IsAnimValList() && alist->mAnimVal ?
    *alist->mAnimVal :
    alist->mBaseVal;
}

//----------------------------------------------------------------------
// nsIDOMSVGTransformList methods:

/* readonly attribute unsigned long numberOfItems; */
NS_IMETHODIMP
DOMSVGTransformList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
  if (IsAnimValList()) {
    Element()->FlushAnimations();
  }
  *aNumberOfItems = Length();
  return NS_OK;
}

/* readonly attribute unsigned long length; */
NS_IMETHODIMP
DOMSVGTransformList::GetLength(PRUint32 *aLength)
{
  return GetNumberOfItems(aLength);
}

/* void clear (); */
NS_IMETHODIMP
DOMSVGTransformList::Clear()
{
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (Length() > 0) {
    // Notify any existing DOM items of removal *before* truncating the lists
    // so that they can find their SVGTransform internal counterparts and copy
    // their values. This also notifies the animVal list:
    mAList->InternalBaseValListWillChangeLengthTo(0);

    mItems.Clear();
    InternalList().Clear();
    Element()->DidChangeTransformList(true);
    if (mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
  }
  return NS_OK;
}

/* nsIDOMSVGTransform initialize (in nsIDOMSVGTransform newItem); */
NS_IMETHODIMP
DOMSVGTransformList::Initialize(nsIDOMSVGTransform *newItem,
                                nsIDOMSVGTransform **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  // If newItem is already in a list we should insert a clone of newItem, and
  // for consistency, this should happen even if *this* is the list that
  // newItem is currently in. Note that in the case of newItem being in this
  // list, the Clear() call before the InsertItemBefore() call would remove it
  // from this list, and so the InsertItemBefore() call would not insert a
  // clone of newItem, it would actually insert newItem. To prevent that from
  // happening we have to do the clone here, if necessary.

  nsCOMPtr<DOMSVGTransform> domItem = do_QueryInterface(newItem);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  if (domItem->HasOwner()) {
    newItem = domItem->Clone();
  }

  Clear();
  return InsertItemBefore(newItem, 0, _retval);
}

/* nsIDOMSVGTransform getItem (in unsigned long index); */
NS_IMETHODIMP
DOMSVGTransformList::GetItem(PRUint32 index, nsIDOMSVGTransform **_retval)
{
  *_retval = GetItemWithoutAddRef(index);
  if (!*_retval) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform insertItemBefore (in nsIDOMSVGTransform newItem,
 *                                      in unsigned long index); */
NS_IMETHODIMP
DOMSVGTransformList::InsertItemBefore(nsIDOMSVGTransform *newItem,
                                      PRUint32 index,
                                      nsIDOMSVGTransform **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  index = NS_MIN(index, Length());
  if (index >= DOMSVGTransform::MaxListIndex()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<DOMSVGTransform> domItem = do_QueryInterface(newItem);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  if (domItem->HasOwner()) {
    domItem = domItem->Clone(); // must do this before changing anything!
  }

  // Ensure we have enough memory so we can avoid complex error handling below:
  if (!mItems.SetCapacity(mItems.Length() + 1) ||
      !InternalList().SetCapacity(InternalList().Length() + 1)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Now that we know we're inserting, keep animVal list in sync as necessary.
  MaybeInsertNullInAnimValListAt(index);

  InternalList().InsertItem(index, domItem->ToSVGTransform());
  mItems.InsertElementAt(index, domItem.get());

  // This MUST come after the insertion into InternalList(), or else under the
  // insertion into InternalList() the values read from domItem would be bad
  // data from InternalList() itself!:
  domItem->InsertingIntoList(this, index, IsAnimValList());

  UpdateListIndicesFromIndex(mItems, index + 1);

  Element()->DidChangeTransformList(true);
  if (mAList->IsAnimating()) {
    Element()->AnimationNeedsResample();
  }
  *_retval = domItem.forget().get();
  return NS_OK;
}

/* nsIDOMSVGTransform replaceItem (in nsIDOMSVGTransform newItem,
 *                                 in unsigned long index); */
NS_IMETHODIMP
DOMSVGTransformList::ReplaceItem(nsIDOMSVGTransform *newItem,
                                 PRUint32 index,
                                 nsIDOMSVGTransform **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  nsCOMPtr<DOMSVGTransform> domItem = do_QueryInterface(newItem);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  if (index >= Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  if (domItem->HasOwner()) {
    domItem = domItem->Clone(); // must do this before changing anything!
  }

  if (mItems[index]) {
    // Notify any existing DOM item of removal *before* modifying the lists so
    // that the DOM item can copy the *old* value at its index:
    mItems[index]->RemovingFromList();
  }

  InternalList()[index] = domItem->ToSVGTransform();
  mItems[index] = domItem;

  // This MUST come after the ToSVGPoint() call, otherwise that call
  // would end up reading bad data from InternalList()!
  domItem->InsertingIntoList(this, index, IsAnimValList());

  Element()->DidChangeTransformList(true);
  if (mAList->IsAnimating()) {
    Element()->AnimationNeedsResample();
  }
  NS_ADDREF(*_retval = domItem.get());
  return NS_OK;
}

/* nsIDOMSVGTransform removeItem (in unsigned long index); */
NS_IMETHODIMP
DOMSVGTransformList::RemoveItem(PRUint32 index, nsIDOMSVGTransform **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (index >= Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Now that we know we're removing, keep animVal list in sync as necessary.
  // Do this *before* touching InternalList() so the removed item can get its
  // internal value.
  MaybeRemoveItemFromAnimValListAt(index);

  // We have to return the removed item, so make sure it exists:
  EnsureItemAt(index);

  // Notify the DOM item of removal *before* modifying the lists so that the
  // DOM item can copy its *old* value:
  mItems[index]->RemovingFromList();
  NS_ADDREF(*_retval = mItems[index]);

  InternalList().RemoveItem(index);
  mItems.RemoveElementAt(index);

  UpdateListIndicesFromIndex(mItems, index);

  Element()->DidChangeTransformList(true);
  if (mAList->IsAnimating()) {
    Element()->AnimationNeedsResample();
  }
  return NS_OK;
}

/* nsIDOMSVGTransform appendItem (in nsIDOMSVGTransform newItem); */
NS_IMETHODIMP
DOMSVGTransformList::AppendItem(nsIDOMSVGTransform *newItem,
                                nsIDOMSVGTransform **_retval)
{
  return InsertItemBefore(newItem, Length(), _retval);
}

/* nsIDOMSVGTransform createSVGTransformFromMatrix (in nsIDOMSVGMatrix matrix);
 */
NS_IMETHODIMP
DOMSVGTransformList::CreateSVGTransformFromMatrix(nsIDOMSVGMatrix *matrix,
                                                  nsIDOMSVGTransform **_retval)
{
  nsCOMPtr<DOMSVGMatrix> domItem = do_QueryInterface(matrix);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }

  NS_ADDREF(*_retval = new DOMSVGTransform(domItem->Matrix()));
  return NS_OK;
}

/* nsIDOMSVGTransform consolidate (); */
NS_IMETHODIMP
DOMSVGTransformList::Consolidate(nsIDOMSVGTransform **_retval)
{
  *_retval = nsnull;
  if (IsAnimValList()) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  if (Length() == 0)
    return NS_OK;

  // Note that SVG 1.1 says, "The consolidation operation creates new
  // SVGTransform object as the first and only item in the list" hence, even if
  // Length() == 1 we can't return that one item (after making it a matrix
  // type). We must orphan the existing item and then make a new one.

  // First calculate our matrix
  gfxMatrix mx = InternalList().GetConsolidationMatrix();

  // Then orphan the existing items
  Clear();

  // And append the new transform
  nsRefPtr<DOMSVGTransform> transform = new DOMSVGTransform(mx);
  return InsertItemBefore(transform, Length(), _retval);
}

//----------------------------------------------------------------------
// Implementation helpers:

void
DOMSVGTransformList::EnsureItemAt(PRUint32 aIndex)
{
  if (!mItems[aIndex]) {
    mItems[aIndex] = new DOMSVGTransform(this, aIndex, IsAnimValList());
  }
}

void
DOMSVGTransformList::MaybeInsertNullInAnimValListAt(PRUint32 aIndex)
{
  NS_ABORT_IF_FALSE(!IsAnimValList(), "call from baseVal to animVal");

  DOMSVGTransformList* animVal = mAList->mAnimVal;

  if (!animVal || mAList->IsAnimating()) {
    // No animVal list wrapper, or animVal not a clone of baseVal
    return;
  }

  NS_ABORT_IF_FALSE(animVal->mItems.Length() == mItems.Length(),
                    "animVal list not in sync!");

  animVal->mItems.InsertElementAt(aIndex,
                                  static_cast<DOMSVGTransform*>(nsnull));

  UpdateListIndicesFromIndex(animVal->mItems, aIndex + 1);
}

void
DOMSVGTransformList::MaybeRemoveItemFromAnimValListAt(PRUint32 aIndex)
{
  NS_ABORT_IF_FALSE(!IsAnimValList(), "call from baseVal to animVal");

  // This needs to be a strong reference; otherwise, the RemovingFromList call
  // below might drop the last reference to animVal before we're done with it.
  nsRefPtr<DOMSVGTransformList> animVal = mAList->mAnimVal;

  if (!animVal || mAList->IsAnimating()) {
    // No animVal list wrapper, or animVal not a clone of baseVal
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
