/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLTableSectionElement_h
#define mozilla_dom_HTMLTableSectionElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLTableSectionElem.h"

namespace mozilla {
namespace dom {

class HTMLTableSectionElement : public nsGenericHTMLElement,
                                public nsIDOMHTMLTableSectionElement
{
public:
  HTMLTableSectionElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLTableSectionElement
  NS_DECL_NSIDOMHTMLTABLESECTIONELEMENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(HTMLTableSectionElement,
                                                     nsGenericHTMLElement)

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  nsRefPtr<nsContentList> mRows;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLTableSectionElement_h */
