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
 * The Initial Developer of the Original Code is
 * Robert Longson <longsonr@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2011
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
    return NS_ERROR_DOM_SVG_INVALID_VALUE_ERR;
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
    return NS_ERROR_DOM_SVG_INVALID_VALUE_ERR;
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
    return tests->mStringListAttributes[mAttrEnum];
  }
  return mElement->GetStringListInfo().mStringLists[mAttrEnum];
}

} // namespace mozilla
