/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGTransformList.h"

#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/SVGTransformListBinding.h"
#include "DOMSVGTransform.h"
#include "SVGAnimatedTransformList.h"
#include "nsError.h"
#include <algorithm>

// local helper functions
namespace {

void UpdateListIndicesFromIndex(
    FallibleTArray<mozilla::dom::DOMSVGTransform*>& aItemsArray,
    uint32_t aStartingIndex) {
  uint32_t length = aItemsArray.Length();

  for (uint32_t i = aStartingIndex; i < length; ++i) {
    if (aItemsArray[i]) {
      aItemsArray[i]->UpdateListIndex(i);
    }
  }
}

}  // namespace

namespace mozilla {
namespace dom {

// We could use NS_IMPL_CYCLE_COLLECTION(, except that in Unlink() we need to
// clear our SVGAnimatedTransformList's weak ref to us to be safe. (The other
// option would be to not unlink and rely on the breaking of the other edges in
// the cycle, as NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGTransformList)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGTransformList)
  if (tmp->mAList) {
    if (tmp->IsAnimValList()) {
      tmp->mAList->mAnimVal = nullptr;
    } else {
      tmp->mAList->mBaseVal = nullptr;
    }
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mAList)
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGTransformList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMSVGTransformList)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGTransformList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGTransformList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGTransformList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// DOMSVGTransformList methods:

JSObject* DOMSVGTransformList::WrapObject(JSContext* cx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::SVGTransformList_Binding::Wrap(cx, this, aGivenProto);
}

void DOMSVGTransformList::InternalListLengthWillChange(uint32_t aNewLength) {
  uint32_t oldLength = mItems.Length();

  if (aNewLength > DOMSVGTransform::MaxListIndex()) {
    // It's safe to get out of sync with our internal list as long as we have
    // FEWER items than it does.
    aNewLength = DOMSVGTransform::MaxListIndex();
  }

  RefPtr<DOMSVGTransformList> kungFuDeathGrip;
  if (aNewLength < oldLength) {
    // RemovingFromList() might clear last reference to |this|.
    // Retain a temporary reference to keep from dying before returning.
    kungFuDeathGrip = this;
  }

  // If our length will decrease, notify the items that will be removed:
  for (uint32_t i = aNewLength; i < oldLength; ++i) {
    if (mItems[i]) {
      mItems[i]->RemovingFromList();
    }
  }

  if (!mItems.SetLength(aNewLength, fallible)) {
    // We silently ignore SetLength OOM failure since being out of sync is safe
    // so long as we have *fewer* items than our internal list.
    mItems.Clear();
    return;
  }

  // If our length has increased, null out the new pointers:
  for (uint32_t i = oldLength; i < aNewLength; ++i) {
    mItems[i] = nullptr;
  }
}

SVGTransformList& DOMSVGTransformList::InternalList() const {
  SVGAnimatedTransformList* alist = Element()->GetAnimatedTransformList();
  return IsAnimValList() && alist->mAnimVal ? *alist->mAnimVal
                                            : alist->mBaseVal;
}

//----------------------------------------------------------------------
void DOMSVGTransformList::Clear(ErrorResult& error) {
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (LengthNoFlush() > 0) {
    AutoChangeTransformListNotifier notifier(this);
    // Notify any existing DOM items of removal *before* truncating the lists
    // so that they can find their DOMSVGTransform internal counterparts and
    // copy their values. This also notifies the animVal list:
    mAList->InternalBaseValListWillChangeLengthTo(0);

    mItems.Clear();
    InternalList().Clear();
  }
}

already_AddRefed<DOMSVGTransform> DOMSVGTransformList::Initialize(
    DOMSVGTransform& newItem, ErrorResult& error) {
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  // If newItem is already in a list we should insert a clone of newItem, and
  // for consistency, this should happen even if *this* is the list that
  // newItem is currently in. Note that in the case of newItem being in this
  // list, the Clear() call before the InsertItemBefore() call would remove it
  // from this list, and so the InsertItemBefore() call would not insert a
  // clone of newItem, it would actually insert newItem. To prevent that from
  // happening we have to do the clone here, if necessary.

  RefPtr<DOMSVGTransform> domItem = &newItem;
  if (domItem->HasOwner()) {
    domItem = newItem.Clone();
  }

  Clear(error);
  MOZ_ASSERT(!error.Failed(), "How could this fail?");
  return InsertItemBefore(*domItem, 0, error);
}

already_AddRefed<DOMSVGTransform> DOMSVGTransformList::GetItem(
    uint32_t index, ErrorResult& error) {
  bool found;
  RefPtr<DOMSVGTransform> item = IndexedGetter(index, found, error);
  if (!found) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
  }
  return item.forget();
}

already_AddRefed<DOMSVGTransform> DOMSVGTransformList::IndexedGetter(
    uint32_t index, bool& found, ErrorResult& error) {
  if (IsAnimValList()) {
    Element()->FlushAnimations();
  }
  found = index < LengthNoFlush();
  if (found) {
    return GetItemAt(index);
  }
  return nullptr;
}

already_AddRefed<DOMSVGTransform> DOMSVGTransformList::InsertItemBefore(
    DOMSVGTransform& newItem, uint32_t index, ErrorResult& error) {
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  index = std::min(index, LengthNoFlush());
  if (index >= DOMSVGTransform::MaxListIndex()) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  RefPtr<DOMSVGTransform> domItem = &newItem;
  if (newItem.HasOwner()) {
    domItem = newItem.Clone();  // must do this before changing anything!
  }

  // Ensure we have enough memory so we can avoid complex error handling below:
  if (!mItems.SetCapacity(mItems.Length() + 1, fallible) ||
      !InternalList().SetCapacity(InternalList().Length() + 1)) {
    error.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }
  if (AnimListMirrorsBaseList()) {
    if (!mAList->mAnimVal->mItems.SetCapacity(
            mAList->mAnimVal->mItems.Length() + 1, fallible)) {
      error.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  AutoChangeTransformListNotifier notifier(this);
  // Now that we know we're inserting, keep animVal list in sync as necessary.
  MaybeInsertNullInAnimValListAt(index);

  InternalList().InsertItem(index, domItem->ToSVGTransform());
  MOZ_ALWAYS_TRUE(mItems.InsertElementAt(index, domItem.get(), fallible));

  // This MUST come after the insertion into InternalList(), or else under the
  // insertion into InternalList() the values read from domItem would be bad
  // data from InternalList() itself!:
  domItem->InsertingIntoList(this, index, IsAnimValList());

  UpdateListIndicesFromIndex(mItems, index + 1);

  return domItem.forget();
}

already_AddRefed<DOMSVGTransform> DOMSVGTransformList::ReplaceItem(
    DOMSVGTransform& newItem, uint32_t index, ErrorResult& error) {
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  if (index >= LengthNoFlush()) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  RefPtr<DOMSVGTransform> domItem = &newItem;
  if (newItem.HasOwner()) {
    domItem = newItem.Clone();  // must do this before changing anything!
  }

  AutoChangeTransformListNotifier notifier(this);
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

  return domItem.forget();
}

already_AddRefed<DOMSVGTransform> DOMSVGTransformList::RemoveItem(
    uint32_t index, ErrorResult& error) {
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  if (index >= LengthNoFlush()) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  AutoChangeTransformListNotifier notifier(this);
  // Now that we know we're removing, keep animVal list in sync as necessary.
  // Do this *before* touching InternalList() so the removed item can get its
  // internal value.
  MaybeRemoveItemFromAnimValListAt(index);

  // We have to return the removed item, so get it, creating it if necessary:
  RefPtr<DOMSVGTransform> result = GetItemAt(index);

  // Notify the DOM item of removal *before* modifying the lists so that the
  // DOM item can copy its *old* value:
  result->RemovingFromList();

  InternalList().RemoveItem(index);
  mItems.RemoveElementAt(index);

  UpdateListIndicesFromIndex(mItems, index);

  return result.forget();
}

already_AddRefed<DOMSVGTransform>
DOMSVGTransformList::CreateSVGTransformFromMatrix(const DOMMatrix2DInit& matrix,
                                                  ErrorResult& rv) {
  RefPtr<DOMSVGTransform> result = new DOMSVGTransform(matrix, rv);
  return result.forget();
}

already_AddRefed<DOMSVGTransform> DOMSVGTransformList::Consolidate(
    ErrorResult& error) {
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  if (LengthNoFlush() == 0) {
    return nullptr;
  }

  // Note that SVG 1.1 says, "The consolidation operation creates new
  // SVGTransform object as the first and only item in the list" hence, even if
  // LengthNoFlush() == 1 we can't return that one item (after making it a
  // matrix type). We must orphan the existing item and then make a new one.

  // First calculate our matrix
  gfxMatrix mx = InternalList().GetConsolidationMatrix();

  // Then orphan the existing items
  Clear(error);
  MOZ_ASSERT(!error.Failed(), "How could this fail?");

  // And append the new transform
  RefPtr<DOMSVGTransform> transform = new DOMSVGTransform(mx);
  return InsertItemBefore(*transform, LengthNoFlush(), error);
}

//----------------------------------------------------------------------
// Implementation helpers:

already_AddRefed<DOMSVGTransform> DOMSVGTransformList::GetItemAt(
    uint32_t aIndex) {
  MOZ_ASSERT(aIndex < mItems.Length());

  if (!mItems[aIndex]) {
    mItems[aIndex] = new DOMSVGTransform(this, aIndex, IsAnimValList());
  }
  RefPtr<DOMSVGTransform> result = mItems[aIndex];
  return result.forget();
}

void DOMSVGTransformList::MaybeInsertNullInAnimValListAt(uint32_t aIndex) {
  MOZ_ASSERT(!IsAnimValList(), "call from baseVal to animVal");

  if (!AnimListMirrorsBaseList()) {
    return;
  }

  DOMSVGTransformList* animVal = mAList->mAnimVal;

  MOZ_ASSERT(animVal, "AnimListMirrorsBaseList() promised a non-null animVal");
  MOZ_ASSERT(animVal->mItems.Length() == mItems.Length(),
             "animVal list not in sync!");
  MOZ_ALWAYS_TRUE(animVal->mItems.InsertElementAt(aIndex, nullptr, fallible));

  UpdateListIndicesFromIndex(animVal->mItems, aIndex + 1);
}

void DOMSVGTransformList::MaybeRemoveItemFromAnimValListAt(uint32_t aIndex) {
  MOZ_ASSERT(!IsAnimValList(), "call from baseVal to animVal");

  if (!AnimListMirrorsBaseList()) {
    return;
  }

  // This needs to be a strong reference; otherwise, the RemovingFromList call
  // below might drop the last reference to animVal before we're done with it.
  RefPtr<DOMSVGTransformList> animVal = mAList->mAnimVal;

  MOZ_ASSERT(animVal, "AnimListMirrorsBaseList() promised a non-null animVal");
  MOZ_ASSERT(animVal->mItems.Length() == mItems.Length(),
             "animVal list not in sync!");

  if (animVal->mItems[aIndex]) {
    animVal->mItems[aIndex]->RemovingFromList();
  }
  animVal->mItems.RemoveElementAt(aIndex);

  UpdateListIndicesFromIndex(animVal->mItems, aIndex);
}

}  // namespace dom
}  // namespace mozilla
