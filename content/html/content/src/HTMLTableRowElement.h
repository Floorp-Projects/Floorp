/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_HTMLTableRowElement_h
#define mozilla_dom_HTMLTableRowElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLTableRowElement.h"

class nsIDOMHTMLTableElement;
class nsIDOMHTMLTableSectionElement;

namespace mozilla {
namespace dom {

class HTMLTableRowElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLTableRowElement
{
public:
  HTMLTableRowElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLTableRowElement, nsGkAtoms::tr)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLTableRowElement
  NS_DECL_NSIDOMHTMLTABLEROWELEMENT

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(HTMLTableRowElement,
                                                     nsGenericHTMLElement)

protected:
  already_AddRefed<nsIDOMHTMLTableSectionElement> GetSection() const;
  already_AddRefed<nsIDOMHTMLTableElement> GetTable() const;
  nsRefPtr<nsContentList> mCells;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLTableRowElement_h */
