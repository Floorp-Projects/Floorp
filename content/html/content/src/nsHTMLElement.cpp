/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLElement.h"

#include "nsContentUtils.h"
#include "mozilla/dom/HTMLElementBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsHTMLElement : public nsGenericHTMLElement,
                      public nsIDOMHTMLElement
{
public:
  nsHTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual void GetInnerHTML(nsAString& aInnerHTML,
                            mozilla::ErrorResult& aError) MOZ_OVERRIDE;

  nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

protected:
  virtual JSObject* WrapNode(JSContext *aCx, JSObject *aScope,
                             bool *aTriedToWrap) MOZ_OVERRIDE;
};

// Here, we expand 'NS_IMPL_NS_NEW_HTML_ELEMENT()' by hand.
// (Calling the macro directly (with no args) produces compiler warnings.)
nsGenericHTMLElement*
NS_NewHTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                  FromParser aFromParser)
{
  return new nsHTMLElement(aNodeInfo);
}

nsHTMLElement::nsHTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  SetIsDOMBinding();
}

nsHTMLElement::~nsHTMLElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLElement, Element)
NS_IMPL_RELEASE_INHERITED(nsHTMLElement, Element)

DOMCI_NODE_DATA(HTMLElement, nsHTMLElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLElement)
  NS_HTML_CONTENT_INTERFACE_TABLE0(nsHTMLElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLElement)

void
nsHTMLElement::GetInnerHTML(nsAString& aInnerHTML, ErrorResult& aError)
{
  /**
   * nsGenericHTMLElement::GetInnerHTML escapes < and > characters (at least).
   * .innerHTML should return the HTML code for xmp and plaintext element.
   *
   * This code is a workaround until we implement a HTML5 Serializer
   * with this behavior.
   */
  if (mNodeInfo->Equals(nsGkAtoms::xmp) ||
      mNodeInfo->Equals(nsGkAtoms::plaintext)) {
    nsContentUtils::GetNodeTextContent(this, false, aInnerHTML);
    return;
  }

  nsGenericHTMLElement::GetInnerHTML(aInnerHTML, aError);
}

JSObject*
nsHTMLElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return dom::HTMLElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}
