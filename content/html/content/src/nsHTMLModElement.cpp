/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMHTMLModElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"

class nsHTMLModElement : public nsGenericHTMLElement,
                         public nsIDOMHTMLModElement
{
public:
  nsHTMLModElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLModElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLModElement
  NS_DECL_NSIDOMHTMLMODELEMENT

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Mod)

nsHTMLModElement::nsHTMLModElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLModElement::~nsHTMLModElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLModElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLModElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLModElement, nsHTMLModElement)

// QueryInterface implementation for nsHTMLModElement
NS_INTERFACE_TABLE_HEAD(nsHTMLModElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLModElement,
                                   nsIDOMHTMLModElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLModElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLModElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLModElement)


NS_IMPL_URI_ATTR(nsHTMLModElement, Cite, cite)
NS_IMPL_STRING_ATTR(nsHTMLModElement, DateTime, datetime)
