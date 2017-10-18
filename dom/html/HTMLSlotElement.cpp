/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/HTMLSlotElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "nsGkAtoms.h"
#include "nsDocument.h"

nsGenericHTMLElement*
NS_NewHTMLSlotElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                      mozilla::dom::FromParser aFromParser)
{
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);
  /* Disabled for now
  if (nsContentUtils::IsWebComponentsEnabled()) {
    already_AddRefed<mozilla::dom::NodeInfo> nodeInfoArg(nodeInfo.forget());
    return new mozilla::dom::HTMLSlotElement(nodeInfoArg);
  }
  */

  already_AddRefed<mozilla::dom::NodeInfo> nodeInfoArg(nodeInfo.forget());
  return new mozilla::dom::HTMLUnknownElement(nodeInfoArg);
}

namespace mozilla {
namespace dom {

HTMLSlotElement::HTMLSlotElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLSlotElement::~HTMLSlotElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLSlotElement, nsGenericHTMLElement)
NS_IMPL_RELEASE_INHERITED(HTMLSlotElement, nsGenericHTMLElement)

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLSlotElement,
                                   nsGenericHTMLElement,
                                   mAssignedNodes)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HTMLSlotElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLSlotElement)

void
HTMLSlotElement::AssignedNodes(const AssignedNodesOptions& aOptions,
                               nsTArray<RefPtr<nsINode>>& aNodes)
{
  aNodes = mAssignedNodes;
}

JSObject*
HTMLSlotElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLSlotElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
