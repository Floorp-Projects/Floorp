/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLLIElement_h
#define mozilla_dom_HTMLLIElement_h

#include "mozilla/Util.h"
#include "nsIDOMHTMLLIElement.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLLIElement : public nsGenericHTMLElement,
                      public nsIDOMHTMLLIElement
{
public:
  HTMLLIElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
    SetIsDOMBinding();
  }
  virtual ~HTMLLIElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLLIElement
  NS_DECL_NSIDOMHTMLLIELEMENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL API
  void GetType(nsString& aType)
  {
    GetHTMLAttr(nsGkAtoms::type, aType);
  }
  void SetType(const nsAString& aType, mozilla::ErrorResult& rv)
  {
    SetHTMLAttr(nsGkAtoms::type, aType, rv);
  }
  int32_t Value() const
  {
    return GetIntAttr(nsGkAtoms::value, 0);
  }
  void SetValue(int32_t aValue, mozilla::ErrorResult& rv)
  {
    SetHTMLIntAttr(nsGkAtoms::value, aValue, rv);
  }

protected:
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope,
                             bool *aTriedToWrap) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLLIElement_h
