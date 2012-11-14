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

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

class nsIAtom;

// nsISupports must be on the primary inheritance chain 
// because nsDOMSettableTokenList is traversed by Element.
class nsDOMSettableTokenList : public nsDOMTokenList,
                               public nsIDOMDOMSettableTokenList
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMDOMSETTABLETOKENLIST

  NS_FORWARD_NSIDOMDOMTOKENLIST(nsDOMTokenList::);

  nsDOMSettableTokenList(mozilla::dom::Element* aElement, nsIAtom* aAttrAtom);

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  virtual ~nsDOMSettableTokenList();
};

#endif // nsDOMSettableTokenList_h___

