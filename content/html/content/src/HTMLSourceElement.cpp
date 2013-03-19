/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSourceElement.h"
#include "mozilla/dom/HTMLSourceElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Source)

namespace mozilla {
namespace dom {

HTMLSourceElement::HTMLSourceElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  SetIsDOMBinding();
}

HTMLSourceElement::~HTMLSourceElement()
{
}


NS_IMPL_ADDREF_INHERITED(HTMLSourceElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLSourceElement, Element)



// QueryInterface implementation for HTMLSourceElement
NS_INTERFACE_TABLE_HEAD(HTMLSourceElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLSourceElement, nsIDOMHTMLSourceElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLSourceElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(HTMLSourceElement)


NS_IMPL_URI_ATTR(HTMLSourceElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLSourceElement, Type, type)
NS_IMPL_STRING_ATTR(HTMLSourceElement, Media, media)

void
HTMLSourceElement::GetItemValueText(nsAString& aValue)
{
  GetSrc(aValue);
}

void
HTMLSourceElement::SetItemValueText(const nsAString& aValue)
{
  SetSrc(aValue);
}

nsresult
HTMLSourceElement::BindToTree(nsIDocument *aDocument,
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

  HTMLMediaElement* media = static_cast<HTMLMediaElement*>(aParent);
  media->NotifyAddedSource();

  return NS_OK;
}

JSObject*
HTMLSourceElement::WrapNode(JSContext* aCx, JSObject* aScope)
{
  return HTMLSourceElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
