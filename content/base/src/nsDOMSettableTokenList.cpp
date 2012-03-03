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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
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
nsDOMSettableTokenList::WrapObject(JSContext *cx, XPCWrappedNativeScope *scope,
                                   bool *triedToWrap)
{
  return mozilla::dom::binding::DOMSettableTokenList::create(cx, scope, this,
                                                             triedToWrap);
}
