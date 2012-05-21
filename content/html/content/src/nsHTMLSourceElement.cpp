/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsIDOMHTMLSourceElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "nsHTMLMediaElement.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"

class nsHTMLSourceElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLSourceElement
{
public:
  nsHTMLSourceElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLSourceElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLSourceElement
  NS_DECL_NSIDOMHTMLSOURCEELEMENT

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // Override BindToTree() so that we can trigger a load when we add a
  // child source element.
  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Source)


nsHTMLSourceElement::nsHTMLSourceElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLSourceElement::~nsHTMLSourceElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLSourceElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLSourceElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLSourceElement, nsHTMLSourceElement)

// QueryInterface implementation for nsHTMLSourceElement
NS_INTERFACE_TABLE_HEAD(nsHTMLSourceElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLSourceElement, nsIDOMHTMLSourceElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLSourceElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLSourceElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLSourceElement)


NS_IMPL_URI_ATTR(nsHTMLSourceElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLSourceElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLSourceElement, Media, media)

nsresult
nsHTMLSourceElement::BindToTree(nsIDocument *aDocument,
                                nsIContent *aParent,
                                nsIContent *aBindingParent,
                                bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument,
                                                 aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aParent || !aParent->IsNodeOfType(nsINode::eMEDIA))
    return NS_OK;

  nsHTMLMediaElement* media = static_cast<nsHTMLMediaElement*>(aParent);
  media->NotifyAddedSource();

  return NS_OK;
}

