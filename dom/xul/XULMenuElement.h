/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULMenuElement_h
#define mozilla_dom_XULMenuElement_h

#include "XULButtonElement.h"
#include "nsINode.h"
#include "nsXULElement.h"

namespace mozilla::dom {

class KeyboardEvent;

class XULMenuElement final : public XULButtonElement {
 public:
  explicit XULMenuElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : XULButtonElement(std::move(aNodeInfo)) {}

  MOZ_CAN_RUN_SCRIPT void SetActiveMenuChild(Element*);
  Element* GetActiveMenuChild();

  NS_IMPL_FROMNODE_HELPER(XULMenuElement,
                          IsAnyOfXULElements(nsGkAtoms::menu,
                                             nsGkAtoms::menulist));

 private:
  virtual ~XULMenuElement() = default;
  JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;
};

}  // namespace mozilla::dom

#endif  // XULMenuElement_h
