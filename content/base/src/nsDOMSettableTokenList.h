/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of nsIDOMDOMSettableTokenList specified by HTML5.
 */

#ifndef nsDOMSettableTokenList_h___
#define nsDOMSettableTokenList_h___

#include "nsIDOMDOMSettableTokenList.h"
#include "nsDOMTokenList.h"


class nsGenericElement;
class nsIAtom;

class nsDOMSettableTokenList : public nsDOMTokenList,
                               public nsIDOMDOMSettableTokenList
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMDOMSETTABLETOKENLIST

  NS_FORWARD_NSIDOMDOMTOKENLIST(nsDOMTokenList::);

  nsDOMSettableTokenList(nsGenericElement* aElement, nsIAtom* aAttrAtom);

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

protected:
  virtual ~nsDOMSettableTokenList();
};

#endif // nsDOMSettableTokenList_h___

