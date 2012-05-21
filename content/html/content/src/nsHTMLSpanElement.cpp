/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsIDOMHTMLElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIAtom.h"
#include "nsRuleData.h"

class nsHTMLSpanElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLElement
{
public:
  nsHTMLSpanElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLSpanElement();

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


NS_IMPL_NS_NEW_HTML_ELEMENT(Span)


nsHTMLSpanElement::nsHTMLSpanElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLSpanElement::~nsHTMLSpanElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLSpanElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLSpanElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLSpanElement, nsHTMLSpanElement)

// QueryInterface implementation for nsHTMLSpanElement
NS_INTERFACE_TABLE_HEAD(nsHTMLSpanElement)
  NS_HTML_CONTENT_INTERFACE_TABLE0(nsHTMLSpanElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLSpanElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLSpanElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLSpanElement)
