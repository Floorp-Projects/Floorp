/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of nsIDOMDOMSettableTokenList specified by HTML5.
 */

#include "nsDOMSettableTokenList.h"
#include "dombindings.h"


nsDOMSettableTokenList::nsDOMSettableTokenList(nsGenericElement *aElement, nsIAtom* aAttrAtom)
  : nsDOMTokenList(aElement, aAttrAtom)
{
}

nsDOMSettableTokenList::~nsDOMSettableTokenList()
{
}

DOMCI_DATA(DOMSettableTokenList, nsDOMSettableTokenList)

NS_INTERFACE_TABLE_HEAD(nsDOMSettableTokenList)
  NS_INTERFACE_TABLE1(nsDOMSettableTokenList,
                      nsIDOMDOMSettableTokenList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMSettableTokenList)
NS_INTERFACE_MAP_END_INHERITING(nsDOMTokenList)

NS_IMPL_ADDREF_INHERITED(nsDOMSettableTokenList, nsDOMTokenList)
NS_IMPL_RELEASE_INHERITED(nsDOMSettableTokenList, nsDOMTokenList)

NS_IMETHODIMP
nsDOMSettableTokenList::GetValue(nsAString& aResult)
{
  return ToString(aResult);
}

NS_IMETHODIMP
nsDOMSettableTokenList::SetValue(const nsAString& aValue)
{
  if (!mElement) {
    return NS_OK;
  }

  return mElement->SetAttr(kNameSpaceID_None, mAttrAtom, aValue, true);
}

JSObject*
nsDOMSettableTokenList::WrapObject(JSContext *cx, JSObject *scope,
                                   bool *triedToWrap)
{
  return mozilla::dom::binding::DOMSettableTokenList::create(cx, scope, this,
                                                             triedToWrap);
}
