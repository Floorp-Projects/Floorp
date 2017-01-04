/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGElement.h"
#include "DOMSVGNumberList.h"
#include "DOMSVGNumber.h"
#include "nsError.h"
#include "SVGAnimatedNumberList.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/SVGNumberListBinding.h"
#include <algorithm>

// See the comment in this file's header.

// local helper functions
namespace {

using mozilla::DOMSVGNumber;

void UpdateListIndicesFromIndex(FallibleTArray<DOMSVGNumber*>& aItemsArray,
                                uint32_t aStartingIndex)
{
  uint32_t length = aItemsArray.Length();

  for (uint32_t i = aStartingIndex; i < length; ++i) {
    if (aItemsArray[i]) {
      aItemsArray[i]->UpdateListIndex(i);
    }
  }
}

} // namespace

namespace mozilla {

// We could use NS_IMPL_CYCLE_COLLECTION(, except that in Unlink() we need to
// clear our DOMSVGAnimatedNumberList's weak ref to us to be safe. (The other
// option would be to not unlink and rely on the breaking of the other edges in
// the cycle, as NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGNumberList)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGNumberList)
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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGNumberList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMSVGNumberList)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGNumberList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGNumberList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGNumberList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


JSObject*
DOMSVGNumberList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::SVGNumberListBinding::Wrap(cx, this, aGivenProto);
}

//----------------------------------------------------------------------
// Helper class: AutoChangeNumberListNotifier
// Stack-based helper class to pair calls to WillChangeNumberList and
// DidChangeNumberList.
class MOZ_RAII AutoChangeNumberListNotifier
{
public:
  explicit AutoChangeNumberListNotifier(DOMSVGNumberList* aNumberList MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mNumberList(aNumberList)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mNumberList, "Expecting non-null numberList");
    mEmptyOrOldValue =
      mNumberList->Element()->WillChangeNumberList(mNumberList->AttrEnum());
  }

  ~AutoChangeNumberListNotifier()
  {
    mNumberList->Element()->DidChangeNumberList(mNumberList->AttrEnum(),
                                                mEmptyOrOldValue);
    if (mNumberList->IsAnimating()) {
      mNumberList->Element()->AnimationNeedsResample();
    }
  }

private:
  DOMSVGNumberList* const mNumberList;
  nsAttrValue       mEmptyOrOldValue;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

void
DOMSVGNumberList::InternalListLengthWillChange(uint32_t aNewLength)
{
  uint32_t oldLength = mItems.Length();

  if (aNewLength > DOMSVGNumber::MaxListIndex()) {
    // It's safe to get out of sync with our internal list as long as we have
    // FEWER items than it does.
    aNewLength = DOMSVGNumber::MaxListIndex();
  }

  RefPtr<DOMSVGNumberList> kungFuDeathGrip;
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

SVGNumberList&
DOMSVGNumberList::InternalList() const
{
  SVGAnimatedNumberList *alist = Element()->GetAnimatedNumberList(AttrEnum());
  return IsAnimValList() && alist->mAnimVal ? *alist->mAnimVal : alist->mBaseVal;
}

void
DOMSVGNumberList::Clear(ErrorResult& error)
{
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (LengthNoFlush() > 0) {
    AutoChangeNumberListNotifier notifier(this);
    // Notify any existing DOM items of removal *before* truncating the lists
    // so that they can find their SVGNumber internal counterparts and copy
    // their values. This also notifies the animVal list:
    mAList->InternalBaseValListWillChangeTo(SVGNumberList());

    mItems.Clear();
    InternalList().Clear();
  }
}

already_AddRefed<DOMSVGNumber>
DOMSVGNumberList::Initialize(DOMSVGNumber& aItem,
                             ErrorResult& error)
{
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
  RefPtr<DOMSVGNumber> domItem = aItem.HasOwner() ? aItem.Clone() : &aItem;

  Clear(error);
  MOZ_ASSERT(!error.Failed());
  return InsertItemBefore(*domItem, 0, error);
}

already_AddRefed<DOMSVGNumber>
DOMSVGNumberList::GetItem(uint32_t index, ErrorResult& error)
{
  bool found;
  RefPtr<DOMSVGNumber> item = IndexedGetter(index, found, error);
  if (!found) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
  }
  return item.forget();
}

already_AddRefed<DOMSVGNumber>
DOMSVGNumberList::IndexedGetter(uint32_t index, bool& found, ErrorResult& error)
{
  if (IsAnimValList()) {
    Element()->FlushAnimations();
  }
  found = index < LengthNoFlush();
  if (found) {
    return GetItemAt(index);
  }
  return nullptr;
}

already_AddRefed<DOMSVGNumber>
DOMSVGNumberList::InsertItemBefore(DOMSVGNumber& aItem,
                                   uint32_t index,
                                   ErrorResult& error)
{
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  index = std::min(index, LengthNoFlush());
  if (index >= DOMSVGNumber::MaxListIndex()) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  // must do this before changing anything!
  RefPtr<DOMSVGNumber> domItem = aItem.HasOwner() ? aItem.Clone() : &aItem;

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

  AutoChangeNumberListNotifier notifier(this);
  // Now that we know we're inserting, keep animVal list in sync as necessary.
  MaybeInsertNullInAnimValListAt(index);

  InternalList().InsertItem(index, domItem->ToSVGNumber());
  MOZ_ALWAYS_TRUE(mItems.InsertElementAt(index, domItem, fallible));

  // This MUST come after the insertion into InternalList(), or else under the
  // insertion into InternalList() the values read from domItem would be bad
  // data from InternalList() itself!:
  domItem->InsertingIntoList(this, AttrEnum(), index, IsAnimValList());

  UpdateListIndicesFromIndex(mItems, index + 1);

  return domItem.forget();
}

already_AddRefed<DOMSVGNumber>
DOMSVGNumberList::ReplaceItem(DOMSVGNumber& aItem,
                              uint32_t index,
                              ErrorResult& error)
{
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  if (index >= LengthNoFlush()) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  // must do this before changing anything!
  RefPtr<DOMSVGNumber> domItem = aItem.HasOwner() ? aItem.Clone() : &aItem;

  AutoChangeNumberListNotifier notifier(this);
  if (mItems[index]) {
    // Notify any existing DOM item of removal *before* modifying the lists so
    // that the DOM item can copy the *old* value at its index:
    mItems[index]->RemovingFromList();
  }

  InternalList()[index] = domItem->ToSVGNumber();
  mItems[index] = domItem;

  // This MUST come after the ToSVGPoint() call, otherwise that call
  // would end up reading bad data from InternalList()!
  domItem->InsertingIntoList(this, AttrEnum(), index, IsAnimValList());

  return domItem.forget();
}

already_AddRefed<DOMSVGNumber>
DOMSVGNumberList::RemoveItem(uint32_t index,
                             ErrorResult& error)
{
  if (IsAnimValList()) {
    error.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  if (index >= LengthNoFlush()) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  // Now that we know we're removing, keep animVal list in sync as necessary.
  // Do this *before* touching InternalList() so the removed item can get its
  // internal value.
  MaybeRemoveItemFromAnimValListAt(index);

  // We have to return the removed item, so get it, creating it if necessary:
  RefPtr<DOMSVGNumber> result = GetItemAt(index);

  AutoChangeNumberListNotifier notifier(this);
  // Notify the DOM item of removal *before* modifying the lists so that the
  // DOM item can copy its *old* value:
  mItems[index]->RemovingFromList();

  InternalList().RemoveItem(index);
  mItems.RemoveElementAt(index);

  UpdateListIndicesFromIndex(mItems, index);

  return result.forget();
}

already_AddRefed<DOMSVGNumber>
DOMSVGNumberList::GetItemAt(uint32_t aIndex)
{
  MOZ_ASSERT(aIndex < mItems.Length());

  if (!mItems[aIndex]) {
    mItems[aIndex] = new DOMSVGNumber(this, AttrEnum(), aIndex, IsAnimValList());
  }
  RefPtr<DOMSVGNumber> result = mItems[aIndex];
  return result.forget();
}

void
DOMSVGNumberList::MaybeInsertNullInAnimValListAt(uint32_t aIndex)
{
  MOZ_ASSERT(!IsAnimValList(), "call from baseVal to animVal");

  if (!AnimListMirrorsBaseList()) {
    return;
  }

  DOMSVGNumberList* animVal = mAList->mAnimVal;

  MOZ_ASSERT(animVal, "AnimListMirrorsBaseList() promised a non-null animVal");
  MOZ_ASSERT(animVal->mItems.Length() == mItems.Length(),
             "animVal list not in sync!");
  MOZ_ALWAYS_TRUE(animVal->mItems.InsertElementAt(aIndex, nullptr, fallible));

  UpdateListIndicesFromIndex(animVal->mItems, aIndex + 1);
}

void
DOMSVGNumberList::MaybeRemoveItemFromAnimValListAt(uint32_t aIndex)
{
  MOZ_ASSERT(!IsAnimValList(), "call from baseVal to animVal");

  if (!AnimListMirrorsBaseList()) {
    return;
  }

  // This needs to be a strong reference; otherwise, the RemovingFromList call
  // below might drop the last reference to animVal before we're done with it.
  RefPtr<DOMSVGNumberList> animVal = mAList->mAnimVal;

  MOZ_ASSERT(animVal, "AnimListMirrorsBaseList() promised a non-null animVal");
  MOZ_ASSERT(animVal->mItems.Length() == mItems.Length(),
             "animVal list not in sync!");

  if (animVal->mItems[aIndex]) {
    animVal->mItems[aIndex]->RemovingFromList();
  }
  animVal->mItems.RemoveElementAt(aIndex);

  UpdateListIndicesFromIndex(animVal->mItems, aIndex);
}

} // namespace mozilla
