/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGStringList.h"

#include "mozilla/dom/SVGStringListBinding.h"
#include "mozilla/dom/SVGTests.h"
#include "nsError.h"
#include "nsCOMPtr.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsQueryObject.h"
#include <algorithm>

// See the architecture comment in this file's header.

namespace mozilla {

using namespace dom;

static inline
nsSVGAttrTearoffTable<SVGStringList, DOMSVGStringList>&
SVGStringListTearoffTable()
{
  static nsSVGAttrTearoffTable<SVGStringList, DOMSVGStringList>
    sSVGStringListTearoffTable;
  return sSVGStringListTearoffTable;
}

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGStringList, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGStringList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGStringList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGStringList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// Helper class: AutoChangeStringListNotifier
// Stack-based helper class to pair calls to WillChangeStringListList and
// DidChangeStringListList.
class MOZ_STACK_CLASS AutoChangeStringListNotifier
{
public:
  explicit AutoChangeStringListNotifier(DOMSVGStringList* aStringList MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mStringList(aStringList)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mStringList, "Expecting non-null stringList");
    mEmptyOrOldValue =
      mStringList->mElement->WillChangeStringList(mStringList->mIsConditionalProcessingAttribute,
                                                  mStringList->mAttrEnum);
  }

  ~AutoChangeStringListNotifier()
  {
    mStringList->mElement->DidChangeStringList(mStringList->mIsConditionalProcessingAttribute,
                                               mStringList->mAttrEnum, mEmptyOrOldValue);
  }

private:
  DOMSVGStringList* const mStringList;
  nsAttrValue       mEmptyOrOldValue;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/* static */ already_AddRefed<DOMSVGStringList>
DOMSVGStringList::GetDOMWrapper(SVGStringList *aList,
                                nsSVGElement *aElement,
                                bool aIsConditionalProcessingAttribute,
                                uint8_t aAttrEnum)
{
  nsRefPtr<DOMSVGStringList> wrapper =
    SVGStringListTearoffTable().GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGStringList(aElement, 
                                   aIsConditionalProcessingAttribute,
                                   aAttrEnum);
    SVGStringListTearoffTable().AddTearoff(aList, wrapper);
  }
  return wrapper.forget();
}

DOMSVGStringList::~DOMSVGStringList()
{
  // Script no longer has any references to us.
  SVGStringListTearoffTable().RemoveTearoff(&InternalList());
}

/* virtual */ JSObject*
DOMSVGStringList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGStringListBinding::Wrap(aCx, this, aGivenProto);
}

// ----------------------------------------------------------------------------
// SVGStringList implementation:

uint32_t
DOMSVGStringList::NumberOfItems() const
{
  return InternalList().Length();
}

uint32_t
DOMSVGStringList::Length() const
{
  return NumberOfItems();
}

void
DOMSVGStringList::Clear()
{
  if (InternalList().IsExplicitlySet()) {
    AutoChangeStringListNotifier notifier(this);
    InternalList().Clear();
  }
}

void
DOMSVGStringList::Initialize(const nsAString& aNewItem, nsAString& aRetval,
                             ErrorResult& aRv)
{
  if (InternalList().IsExplicitlySet()) {
    InternalList().Clear();
  }
  InsertItemBefore(aNewItem, 0, aRetval, aRv);
}

void
DOMSVGStringList::GetItem(uint32_t aIndex, nsAString& aRetval, ErrorResult& aRv)
{
  bool found;
  IndexedGetter(aIndex, found, aRetval);
  if (!found) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
  }
}

void
DOMSVGStringList::IndexedGetter(uint32_t aIndex, bool& aFound,
                                nsAString& aRetval)
{
  aFound = aIndex < InternalList().Length();
  if (aFound) {
    aRetval = InternalList()[aIndex];
  }
}

void
DOMSVGStringList::InsertItemBefore(const nsAString& aNewItem, uint32_t aIndex,
                                   nsAString& aRetval, ErrorResult& aRv)
{
  if (aNewItem.IsEmpty()) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }
  aIndex = std::min(aIndex, InternalList().Length());

  // Ensure we have enough memory so we can avoid complex error handling below:
  if (!InternalList().SetCapacity(InternalList().Length() + 1)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  AutoChangeStringListNotifier notifier(this);
  InternalList().InsertItem(aIndex, aNewItem);
  aRetval = aNewItem;
}

void
DOMSVGStringList::ReplaceItem(const nsAString& aNewItem, uint32_t aIndex,
                              nsAString& aRetval, ErrorResult& aRv)
{
  if (aNewItem.IsEmpty()) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }
  if (aIndex >= InternalList().Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  aRetval = InternalList()[aIndex];
  AutoChangeStringListNotifier notifier(this);
  InternalList().ReplaceItem(aIndex, aNewItem);
}

void
DOMSVGStringList::RemoveItem(uint32_t aIndex, nsAString& aRetval,
                             ErrorResult& aRv)
{
  if (aIndex >= InternalList().Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  AutoChangeStringListNotifier notifier(this);
  InternalList().RemoveItem(aIndex);
}

void
DOMSVGStringList::AppendItem(const nsAString& aNewItem, nsAString& aRetval,
                             ErrorResult& aRv)
{
  InsertItemBefore(aNewItem, InternalList().Length(), aRetval, aRv);
}

SVGStringList &
DOMSVGStringList::InternalList() const
{
  if (mIsConditionalProcessingAttribute) {
    nsCOMPtr<dom::SVGTests> tests = do_QueryObject(mElement.get());
    return tests->mStringListAttributes[mAttrEnum];
  }
  return mElement->GetStringListInfo().mStringLists[mAttrEnum];
}

} // namespace mozilla
