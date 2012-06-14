/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGStringList.h"
#include "DOMSVGTests.h"
#include "nsDOMError.h"
#include "nsCOMPtr.h"
#include "nsSVGAttrTearoffTable.h"

// See the architecture comment in this file's header.

namespace mozilla {

static nsSVGAttrTearoffTable<SVGStringList, DOMSVGStringList>
  sSVGStringListTearoffTable;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(DOMSVGStringList, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGStringList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGStringList)

} // namespace mozilla

DOMCI_DATA(SVGStringList, mozilla::DOMSVGStringList)
namespace mozilla {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGStringList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGStringList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGStringList)
NS_INTERFACE_MAP_END


/* static */ already_AddRefed<DOMSVGStringList>
DOMSVGStringList::GetDOMWrapper(SVGStringList *aList,
                                nsSVGElement *aElement,
                                bool aIsConditionalProcessingAttribute,
                                PRUint8 aAttrEnum)
{
  DOMSVGStringList *wrapper =
    sSVGStringListTearoffTable.GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGStringList(aElement, 
                                   aIsConditionalProcessingAttribute,
                                   aAttrEnum);
    sSVGStringListTearoffTable.AddTearoff(aList, wrapper);
  }
  NS_ADDREF(wrapper);
  return wrapper;
}

DOMSVGStringList::~DOMSVGStringList()
{
  // Script no longer has any references to us.
  sSVGStringListTearoffTable.RemoveTearoff(&InternalList());
}

// ----------------------------------------------------------------------------
// nsIDOMSVGStringList implementation:

NS_IMETHODIMP
DOMSVGStringList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
  *aNumberOfItems = InternalList().Length();
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGStringList::GetLength(PRUint32 *aLength)
{
  return GetNumberOfItems(aLength);
}

NS_IMETHODIMP
DOMSVGStringList::Clear()
{
  if (InternalList().IsExplicitlySet()) {
    nsAttrValue emptyOrOldValue =
      mElement->WillChangeStringList(mIsConditionalProcessingAttribute,
                                     mAttrEnum);
    InternalList().Clear();
    mElement->DidChangeStringList(mIsConditionalProcessingAttribute,
                                  mAttrEnum, emptyOrOldValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGStringList::Initialize(const nsAString & newItem, nsAString & _retval)
{
  if (InternalList().IsExplicitlySet()) {
    InternalList().Clear();
  }
  return InsertItemBefore(newItem, 0, _retval);
}

NS_IMETHODIMP
DOMSVGStringList::GetItem(PRUint32 index,
                          nsAString & _retval)
{
  if (index >= InternalList().Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  _retval = InternalList()[index];
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGStringList::InsertItemBefore(const nsAString & newItem,
                                   PRUint32 index,
                                   nsAString & _retval)
{
  if (newItem.IsEmpty()) { // takes care of DOMStringIsNull too
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  index = NS_MIN(index, InternalList().Length());

  // Ensure we have enough memory so we can avoid complex error handling below:
  if (!InternalList().SetCapacity(InternalList().Length() + 1)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAttrValue emptyOrOldValue =
    mElement->WillChangeStringList(mIsConditionalProcessingAttribute,
                                   mAttrEnum);
  InternalList().InsertItem(index, newItem);

  mElement->DidChangeStringList(mIsConditionalProcessingAttribute, mAttrEnum,
                                emptyOrOldValue);
  _retval = newItem;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGStringList::ReplaceItem(const nsAString & newItem,
                              PRUint32 index,
                              nsAString & _retval)
{
  if (newItem.IsEmpty()) { // takes care of DOMStringIsNull too
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  if (index >= InternalList().Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  _retval = InternalList()[index];
  nsAttrValue emptyOrOldValue =
    mElement->WillChangeStringList(mIsConditionalProcessingAttribute,
                                   mAttrEnum);
  InternalList().ReplaceItem(index, newItem);

  mElement->DidChangeStringList(mIsConditionalProcessingAttribute, mAttrEnum,
                                emptyOrOldValue);
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGStringList::RemoveItem(PRUint32 index,
                             nsAString & _retval)
{
  if (index >= InternalList().Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsAttrValue emptyOrOldValue =
    mElement->WillChangeStringList(mIsConditionalProcessingAttribute,
                                   mAttrEnum);
  InternalList().RemoveItem(index);

  mElement->DidChangeStringList(mIsConditionalProcessingAttribute, mAttrEnum,
                                emptyOrOldValue);
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGStringList::AppendItem(const nsAString & newItem,
                             nsAString & _retval)
{
  return InsertItemBefore(newItem, InternalList().Length(), _retval);
}

SVGStringList &
DOMSVGStringList::InternalList()
{
  if (mIsConditionalProcessingAttribute) {
    nsCOMPtr<DOMSVGTests> tests = do_QueryInterface(mElement);
    return *tests->GetOrCreateStringListAttribute(mAttrEnum);
  }
  return mElement->GetStringListInfo().mStringLists[mAttrEnum];
}

} // namespace mozilla
