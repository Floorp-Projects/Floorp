/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLUnknownElement.h"

class nsHTMLUnknownElement : public nsGenericHTMLElement
                           , public nsIDOMHTMLUnknownElement
{
public:
  nsHTMLUnknownElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLUnknownElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Unknown)


nsHTMLUnknownElement::nsHTMLUnknownElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLUnknownElement::~nsHTMLUnknownElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLUnknownElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLUnknownElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLUnknownElement, nsHTMLUnknownElement)

// QueryInterface implementation for nsHTMLUnknownElement
NS_INTERFACE_TABLE_HEAD(nsHTMLUnknownElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLUnknownElement,
                                   nsIDOMHTMLUnknownElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLUnknownElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLUnknownElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLUnknownElement)
