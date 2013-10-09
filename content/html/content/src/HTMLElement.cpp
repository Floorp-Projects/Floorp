/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLElement.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

class HTMLElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  HTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLElement();

  NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo* aNodeInfo,
                         nsINode** aResult) const MOZ_OVERRIDE;

protected:
  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

HTMLElement::HTMLElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLElement::~HTMLElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLElement)

NS_IMETHODIMP
HTMLElement::GetInnerHTML(nsAString& aInnerHTML)
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
    return NS_OK;
  }

  return nsGenericHTMLElement::GetInnerHTML(aInnerHTML);
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
