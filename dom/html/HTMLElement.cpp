/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLElement.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

class HTMLElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~HTMLElement();

  NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo,
                         nsINode** aResult) const override;

protected:
  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;
};

HTMLElement::HTMLElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
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
    if (!nsContentUtils::GetNodeTextContent(this, false, aInnerHTML)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  }

  return nsGenericHTMLElement::GetInnerHTML(aInnerHTML);
}

JSObject*
HTMLElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::HTMLElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

// Here, we expand 'NS_IMPL_NS_NEW_HTML_ELEMENT()' by hand.
// (Calling the macro directly (with no args) produces compiler warnings.)
nsGenericHTMLElement*
NS_NewHTMLElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                  mozilla::dom::FromParser aFromParser)
{
  return new mozilla::dom::HTMLElement(aNodeInfo);
}
