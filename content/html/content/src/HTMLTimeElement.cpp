/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLTimeElement.h"
#include "mozilla/dom/HTMLTimeElementBinding.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsVariant.h"
#include "nsGkAtoms.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Time)

namespace mozilla {
namespace dom {

HTMLTimeElement::HTMLTimeElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  SetIsDOMBinding();
}

HTMLTimeElement::~HTMLTimeElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLTimeElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTimeElement, Element)

NS_INTERFACE_MAP_BEGIN(HTMLTimeElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_MAP_END

NS_IMPL_ELEMENT_CLONE(HTMLTimeElement)

JSObject*
HTMLTimeElement::WrapNode(JSContext* cx, JS::Handle<JSObject*> scope)
{
  return HTMLTimeElementBinding::Wrap(cx, scope, this);
}

void
HTMLTimeElement::GetItemValueText(nsAString& text)
{
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::datetime)) {
    GetDateTime(text);
  } else {
    GetTextContentInternal(text);
  }
}

void
HTMLTimeElement::SetItemValueText(const nsAString& text)
{
  ErrorResult rv;
  SetDateTime(text, rv);
}

} // namespace dom
} // namespace mozilla
