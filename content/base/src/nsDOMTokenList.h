/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of nsIDOMDOMTokenList specified by HTML5.
 */

#ifndef nsDOMTokenList_h___
#define nsDOMTokenList_h___

#include "nsGenericElement.h"
#include "nsIDOMDOMTokenList.h"

class nsAttrValue;

// nsISupports must be on the primary inheritance chain 
// because nsDOMSettableTokenList is traversed by nsGenericElement.
class nsDOMTokenList : public nsIDOMDOMTokenList,
                       public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMTokenList)
  NS_DECL_NSIDOMDOMTOKENLIST

  nsDOMTokenList(nsGenericElement* aElement, nsIAtom* aAttrAtom);

  void DropReference();

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  nsINode *GetParentObject()
  {
    return mElement;
  }

  const nsAttrValue* GetParsedAttr() {
    if (!mElement) {
      return nsnull;
    }
    return mElement->GetAttrInfo(kNameSpaceID_None, mAttrAtom).mValue;
  }

protected:
  virtual ~nsDOMTokenList();

  nsresult CheckToken(const nsAString& aStr);
  void AddInternal(const nsAttrValue* aAttr, const nsAString& aToken);
  void RemoveInternal(const nsAttrValue* aAttr, const nsAString& aToken);

  nsGenericElement* mElement;
  nsCOMPtr<nsIAtom> mAttrAtom;
};

#endif // nsDOMTokenList_h___
