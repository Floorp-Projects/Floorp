/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGElement.h"
#include "DOMSVGPointList.h"
#include "DOMSVGPoint.h"
#include "nsError.h"
#include "SVGAnimatedPointList.h"
#include "nsCOMPtr.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsContentUtils.h"
#include "mozilla/dom/SVGPointListBinding.h"
#include <algorithm>

// See the comment in this file's header.

// local helper functions
namespace {

void
UpdateListIndicesFromIndex(FallibleTArray<mozilla::nsISVGPoint*>& aItemsArray,
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

  static inline
nsSVGAttrTearoffTable<void, DOMSVGPointList>&
SVGPointListTearoffTable()
{
  static nsSVGAttrTearoffTable<void, DOMSVGPointList>
    sSVGPointListTearoffTable;
  return sSVGPointListTearoffTable;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGPointList)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGPointList)
  // No unlinking of mElement, we'd need to null out the value pointer (the
  // object it points to is held by the element) and null-check it everywhere.
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGPointList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMSVGPointList)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGPointList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGPointList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGPointList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// Helper class: AutoChangePointListNotifier
// Stack-based helper class to pair calls to WillChangePointList and
// DidChangePointList.
class MOZ_RAII AutoChangePointListNotifier
{
public:
  explicit AutoChangePointListNotifier(DOMSVGPointList* aPointList MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mPointList(aPointList)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mPointList, "Expecting non-null pointList");
    mEmptyOrOldValue =
      mPointList->Element()->WillChangePointList();
  }

  ~AutoChangePointListNotifier()
  {
    mPointList->Element()->DidChangePointList(mEmptyOrOldValue);
    if (mPointList->AttrIsAnimating()) {
      mPointList->Element()->AnimationNeedsResample();
    }
  }

private:
  DOMSVGPointList* const mPointList;
  nsAttrValue      mEmptyOrOldValue;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};


/* static */ already_AddRefed<DOMSVGPointList>
DOMSVGPointList::GetDOMWrapper(void *aList,
                               nsSVGElement *aElement,
                               bool aIsAnimValList)
{
  RefPtr<DOMSVGPointList> wrapper =
    SVGPointListTearoffTable().GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGPointList(aElement, aIsAnimValList);
    SVGPointListTearoffTable().AddTearoff(aList, wrapper);
  }
  return wrapper.forget();
}

/* static */ DOMSVGPointList*
DOMSVGPointList::GetDOMWrapperIfExists(void *aList)
{
  return SVGPointListTearoffTable().GetTearoff(aList);
}

DOMSVGPointList::~DOMSVGPointList()
{
  // There are now no longer any references to us held by script or list items.
  // Note we must use GetAnimValKey/GetBaseValKey here, NOT InternalList()!
  void *key = mIsAnimValList ?
    InternalAList().GetAnimValKey() :
    InternalAList().GetBaseValKey();
  SVGPointListTearoffTable().RemoveTearoff(key);
}

JSObject*
DOMSVGPointList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::SVGPointList_Binding::Wrap(cx, this, aGivenProto);
}

void
DOMSVGPointList::InternalListWillChangeTo(const SVGPointList& aNewValue)
{
  // When the number of items in our internal counterpart changes, we MUST stay
  // in sync. Everything in the scary comment in
  // DOMSVGLengthList::InternalBaseValListWillChangeTo applies here too!

  uint32_t oldLength = mItems.Length();

  uint32_t newLength = aNewValue.Length();
  if (newLength > nsISVGPoint::MaxListIndex()) {
    // It's safe to get out of sync with our internal list as long as we have
    // FEWER items than it does.
    newLength = nsISVGPoint::MaxListIndex();
  }

  RefPtr<DOMSVGPointList> kungFuDeathGrip;
  if (newLength < oldLength) {
    // RemovingFromList() might clear last reference to |this|.
    // Retain a temporary reference to keep from dying before returning.
    kungFuDeathGrip = this;
  }

  // If our length will decrease, notify the items that will be removed:
  for (uint32_t i = newLength; i < oldLength; ++i) {
    if (mItems[i]) {
      mItems[i]->RemovingFromList();
    }
  }

  if (!mItems.SetLength(newLength, fallible)) {
    // We silently ignore SetLength OOM failure since being out of sync is safe
    // so long as we have *fewer* items than our internal list.
    mItems.Clear();
    return;
  }

  // If our length has increased, null out the new pointers:
  for (uint32_t i = oldLength; i < newLength; ++i) {
    mItems[i] = nullptr;
  }
}

bool
DOMSVGPointList::AttrIsAnimating() const
{
  return InternalAList().IsAnimating();
}

bool
DOMSVGPointList::AnimListMirrorsBaseList() const
{
  return GetDOMWrapperIfExists(InternalAList().GetAnimValKey()) &&
           !AttrIsAnimating();
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
  MOZ_ASSERT(mElement->GetAnimatedPointList(), "Internal error");
  return *mElement->GetAnimatedPointList();
}

// ----------------------------------------------------------------------------
// nsIDOMSVGPointList implementation:

void
DOMSVGPointList::Clear(ErrorResult& aError)
{
  if (IsAnimValList()) {
    aError.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (LengthNoFlush() > 0) {
    AutoChangePointListNotifier notifier(this);
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
  }
}

already_AddRefed<nsISVGPoint>
DOMSVGPointList::Initialize(nsISVGPoint& aNewItem, ErrorResult& aError)
{
  if (IsAnimValList()) {
    aError.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  // If aNewItem is already in a list we should insert a clone of aNewItem,
  // and for consistency, this should happen even if *this* is the list that
  // aNewItem is currently in. Note that in the case of aNewItem being in this
  // list, the Clear() call before the InsertItemBefore() call would remove it
  // from this list, and so the InsertItemBefore() call would not insert a
  // clone of aNewItem, it would actually insert aNewItem. To prevent that
  // from happening we have to do the clone here, if necessary.

  nsCOMPtr<nsISVGPoint> domItem = &aNewItem;
  if (domItem->HasOwner() || domItem->IsReadonly() ||
      domItem->IsTranslatePoint()) {
    domItem = domItem->Copy(); // must do this before changing anything!
  }

  ErrorResult rv;
  Clear(rv);
  MOZ_ASSERT(!rv.Failed());
  return InsertItemBefore(*domItem, 0, aError);
}

already_AddRefed<nsISVGPoint>
DOMSVGPointList::GetItem(uint32_t index, ErrorResult& error)
{
  bool found;
  RefPtr<nsISVGPoint> item = IndexedGetter(index, found, error);
  if (!found) {
    error.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
  }
  return item.forget();
}

already_AddRefed<nsISVGPoint>
DOMSVGPointList::IndexedGetter(uint32_t aIndex, bool& aFound,
                               ErrorResult& aError)
{
  if (IsAnimValList()) {
    Element()->FlushAnimations();
  }
  aFound = aIndex < LengthNoFlush();
  if (aFound) {
    return GetItemAt(aIndex);
  }
  return nullptr;
}

already_AddRefed<nsISVGPoint>
DOMSVGPointList::InsertItemBefore(nsISVGPoint& aNewItem, uint32_t aIndex,
                                  ErrorResult& aError)
{
  if (IsAnimValList()) {
    aError.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  aIndex = std::min(aIndex, LengthNoFlush());
  if (aIndex >= nsISVGPoint::MaxListIndex()) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsISVGPoint> domItem = &aNewItem;
  if (domItem->HasOwner() || domItem->IsReadonly() ||
      domItem->IsTranslatePoint()) {
    domItem = domItem->Copy(); // must do this before changing anything!
  }

  // Ensure we have enough memory so we can avoid complex error handling below:
  if (!mItems.SetCapacity(mItems.Length() + 1, fallible) ||
      !InternalList().SetCapacity(InternalList().Length() + 1)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }
  if (AnimListMirrorsBaseList()) {
    DOMSVGPointList *animVal =
      GetDOMWrapperIfExists(InternalAList().GetAnimValKey());
    MOZ_ASSERT(animVal, "animVal must be a valid pointer");
    if (!animVal->mItems.SetCapacity(
          animVal->mItems.Length() + 1, fallible)) {
      aError.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  AutoChangePointListNotifier notifier(this);
  // Now that we know we're inserting, keep animVal list in sync as necessary.
  MaybeInsertNullInAnimValListAt(aIndex);

  InternalList().InsertItem(aIndex, domItem->ToSVGPoint());
  MOZ_ALWAYS_TRUE(mItems.InsertElementAt(aIndex, domItem, fallible));

  // This MUST come after the insertion into InternalList(), or else under the
  // insertion into InternalList() the values read from domItem would be bad
  // data from InternalList() itself!:
  domItem->InsertingIntoList(this, aIndex, IsAnimValList());

  UpdateListIndicesFromIndex(mItems, aIndex + 1);

  return domItem.forget();
}

already_AddRefed<nsISVGPoint>
DOMSVGPointList::ReplaceItem(nsISVGPoint& aNewItem, uint32_t aIndex,
                             ErrorResult& aError)
{
  if (IsAnimValList()) {
    aError.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  if (aIndex >= LengthNoFlush()) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsISVGPoint> domItem = &aNewItem;
  if (domItem->HasOwner() || domItem->IsReadonly() ||
      domItem->IsTranslatePoint()) {
    domItem = domItem->Copy(); // must do this before changing anything!
  }

  AutoChangePointListNotifier notifier(this);
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

  return domItem.forget();
}

already_AddRefed<nsISVGPoint>
DOMSVGPointList::RemoveItem(uint32_t aIndex, ErrorResult& aError)
{
  if (IsAnimValList()) {
    aError.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return nullptr;
  }

  if (aIndex >= LengthNoFlush()) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  AutoChangePointListNotifier notifier(this);
  // Now that we know we're removing, keep animVal list in sync as necessary.
  // Do this *before* touching InternalList() so the removed item can get its
  // internal value.
  MaybeRemoveItemFromAnimValListAt(aIndex);

  // We have to return the removed item, so get it, creating it if necessary:
  RefPtr<nsISVGPoint> result = GetItemAt(aIndex);

  // Notify the DOM item of removal *before* modifying the lists so that the
  // DOM item can copy its *old* value:
  mItems[aIndex]->RemovingFromList();

  InternalList().RemoveItem(aIndex);
  mItems.RemoveElementAt(aIndex);

  UpdateListIndicesFromIndex(mItems, aIndex);

  return result.forget();
}

already_AddRefed<nsISVGPoint>
DOMSVGPointList::GetItemAt(uint32_t aIndex)
{
  MOZ_ASSERT(aIndex < mItems.Length());

  if (!mItems[aIndex]) {
    mItems[aIndex] = new DOMSVGPoint(this, aIndex, IsAnimValList());
  }
  RefPtr<nsISVGPoint> result = mItems[aIndex];
  return result.forget();
}

void
DOMSVGPointList::MaybeInsertNullInAnimValListAt(uint32_t aIndex)
{
  MOZ_ASSERT(!IsAnimValList(), "call from baseVal to animVal");

  if (!AnimListMirrorsBaseList()) {
    return;
  }

  // The anim val list is in sync with the base val list
  DOMSVGPointList *animVal =
    GetDOMWrapperIfExists(InternalAList().GetAnimValKey());

  MOZ_ASSERT(animVal, "AnimListMirrorsBaseList() promised a non-null animVal");
  MOZ_ASSERT(animVal->mItems.Length() == mItems.Length(),
             "animVal list not in sync!");
  MOZ_ALWAYS_TRUE(animVal->mItems.InsertElementAt(aIndex, nullptr, fallible));

  UpdateListIndicesFromIndex(animVal->mItems, aIndex + 1);
}

void
DOMSVGPointList::MaybeRemoveItemFromAnimValListAt(uint32_t aIndex)
{
  MOZ_ASSERT(!IsAnimValList(), "call from baseVal to animVal");

  if (!AnimListMirrorsBaseList()) {
    return;
  }

  // This needs to be a strong reference; otherwise, the RemovingFromList call
  // below might drop the last reference to animVal before we're done with it.
  RefPtr<DOMSVGPointList> animVal =
    GetDOMWrapperIfExists(InternalAList().GetAnimValKey());

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
