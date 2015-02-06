/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGNumber.h"
#include "DOMSVGNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "nsSVGElement.h"
#include "nsError.h"
#include "nsContentUtils.h" // for NS_ENSURE_FINITE
#include "mozilla/dom/SVGNumberBinding.h"

// See the architecture comment in DOMSVGAnimatedNumberList.h.

namespace mozilla {

// We could use NS_IMPL_CYCLE_COLLECTION(, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGNumber)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGNumber)
  // We may not belong to a list, so we must null check tmp->mList.
  if (tmp->mList) {
    tmp->mList->mItems[tmp->mListIndex] = nullptr;
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGNumber)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMSVGNumber)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGNumber)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGNumber)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGNumber)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// Helper class: AutoChangeNumberNotifier
// Stack-based helper class to pair calls to WillChangeNumberList and
// DidChangeNumberList.
class MOZ_STACK_CLASS AutoChangeNumberNotifier
{
public:
  explicit AutoChangeNumberNotifier(DOMSVGNumber* aNumber MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mNumber(aNumber)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mNumber, "Expecting non-null number");
    MOZ_ASSERT(mNumber->HasOwner(),
               "Expecting list to have an owner for notification");
    mEmptyOrOldValue =
      mNumber->Element()->WillChangeNumberList(mNumber->mAttrEnum);
  }

  ~AutoChangeNumberNotifier()
  {
    mNumber->Element()->DidChangeNumberList(mNumber->mAttrEnum,
                                            mEmptyOrOldValue);
    if (mNumber->mList->IsAnimating()) {
      mNumber->Element()->AnimationNeedsResample();
    }
  }

private:
  DOMSVGNumber* const mNumber;
  nsAttrValue   mEmptyOrOldValue;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

DOMSVGNumber::DOMSVGNumber(DOMSVGNumberList *aList,
                           uint8_t aAttrEnum,
                           uint32_t aListIndex,
                           bool aIsAnimValItem)
  : mList(aList)
  , mParent(aList)
  , mListIndex(aListIndex)
  , mAttrEnum(aAttrEnum)
  , mIsAnimValItem(aIsAnimValItem)
  , mValue(0.0f)
{
  // These shifts are in sync with the members in the header.
  MOZ_ASSERT(aList &&
             aAttrEnum < (1 << 4) &&
             aListIndex <= MaxListIndex(),
             "bad arg");

  MOZ_ASSERT(IndexIsValid(), "Bad index for DOMSVGNumber!");
}

DOMSVGNumber::DOMSVGNumber(nsISupports* aParent)
  : mList(nullptr)
  , mParent(aParent)
  , mListIndex(0)
  , mAttrEnum(0)
  , mIsAnimValItem(false)
  , mValue(0.0f)
{
}

/* static */ already_AddRefed<DOMSVGNumber>
DOMSVGNumber::Constructor(const dom::GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMSVGNumber> number = new DOMSVGNumber(window);
  return number.forget();
}

/* static */ already_AddRefed<DOMSVGNumber>
DOMSVGNumber::Constructor(const dom::GlobalObject& aGlobal, float aValue,
                          ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DOMSVGNumber> number = new DOMSVGNumber(window);
  number->SetValue(aValue, aRv);
  return number.forget();
}

float
DOMSVGNumber::Value()
{
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == false
  }
  return HasOwner() ? InternalItem() : mValue;
}

void
DOMSVGNumber::SetValue(float aValue, ErrorResult& aRv)
{
  if (mIsAnimValItem) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (HasOwner()) {
    if (InternalItem() == aValue) {
      return;
    }
    AutoChangeNumberNotifier notifier(this);
    InternalItem() = aValue;
    return;
  }

  mValue = aValue;
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

  MOZ_ASSERT(IndexIsValid(), "Bad index for DOMSVGNumber!");
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

JSObject*
DOMSVGNumber::WrapObject(JSContext* aCx)
{
  return dom::SVGNumberBinding::Wrap(aCx, this);
}

} // namespace mozilla
