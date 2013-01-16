/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOMTokenList specified by HTML5.
 */

#ifndef nsDOMTokenList_h___
#define nsDOMTokenList_h___

#include "nsCOMPtr.h"
#include "nsDOMString.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

class nsAttrValue;
class nsIAtom;

// nsISupports must be on the primary inheritance chain
// because nsDOMSettableTokenList is traversed by Element.
class nsDOMTokenList : public nsISupports,
                       public nsWrapperCache
{
protected:
  typedef mozilla::dom::Element Element;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMTokenList)

  nsDOMTokenList(Element* aElement, nsIAtom* aAttrAtom);

  void DropReference();

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  Element* GetParentObject()
  {
    return mElement;
  }

  uint32_t Length();
  void Item(uint32_t aIndex, nsAString& aResult)
  {
    bool found;
    IndexedGetter(aIndex, found, aResult);
    if (!found) {
      SetDOMStringToNull(aResult);
    }
  }
  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aResult);
  bool Contains(const nsAString& aToken, mozilla::ErrorResult& aError);
  void Add(const nsAString& aToken, mozilla::ErrorResult& aError);
  void Remove(const nsAString& aToken, mozilla::ErrorResult& aError);
  bool Toggle(const nsAString& aToken, mozilla::ErrorResult& aError);
  void Stringify(nsAString& aResult);

protected:
  virtual ~nsDOMTokenList();

  nsresult CheckToken(const nsAString& aStr);
  void AddInternal(const nsAttrValue* aAttr, const nsAString& aToken);
  void RemoveInternal(const nsAttrValue* aAttr, const nsAString& aToken);
  inline const nsAttrValue* GetParsedAttr();

  Element* mElement;
  nsCOMPtr<nsIAtom> mAttrAtom;
};

#endif // nsDOMTokenList_h___
