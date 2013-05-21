/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLElement.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

class HTMLElement : public nsGenericHTMLElement,
                    public nsIDOMHTMLElement
{
public:
  HTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLElement();

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

  virtual nsresult Clone(nsINodeInfo* aNodeInfo,
                         nsINode** aResult) const MOZ_OVERRIDE;

  virtual nsIDOMNode* AsDOMNode() { return this; }

protected:
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

HTMLElement::HTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  SetIsDOMBinding();
}

HTMLElement::~HTMLElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLElement, Element)

NS_INTERFACE_TABLE_HEAD(HTMLElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
  NS_HTML_CONTENT_INTERFACE_TABLE0(HTMLElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_MAP_END

NS_IMPL_ELEMENT_CLONE(HTMLElement)

void
HTMLElement::GetInnerHTML(nsAString& aInnerHTML, ErrorResult& aError)
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
HTMLElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return dom::HTMLElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla

// Here, we expand 'NS_IMPL_NS_NEW_HTML_ELEMENT()' by hand.
// (Calling the macro directly (with no args) produces compiler warnings.)
nsGenericHTMLElement*
NS_NewHTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                  mozilla::dom::FromParser aFromParser)
{
  return new mozilla::dom::HTMLElement(aNodeInfo);
}
