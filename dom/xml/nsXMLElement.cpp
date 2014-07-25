/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXMLElement.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/ElementInlines.h"
#include "nsContentUtils.h" // nsAutoScriptBlocker

using namespace mozilla::dom;

nsresult
NS_NewXMLElement(Element** aInstancePtrResult,
                 already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
{
  nsXMLElement* it = new nsXMLElement(aNodeInfo);
  NS_ADDREF(*aInstancePtrResult = it);
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsXMLElement, Element,
                            nsIDOMNode, nsIDOMElement)

JSObject*
nsXMLElement::WrapNode(JSContext *aCx)
{
  return ElementBinding::Wrap(aCx, this);
}

NS_IMPL_ELEMENT_CLONE(nsXMLElement)
