/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XULMenuElement_h
#define mozilla_dom_XULMenuElement_h

#include "nsXULElement.h"

namespace mozilla {
namespace dom {

class KeyboardEvent;

class XULMenuElement final : public nsXULElement
{
public:

  explicit XULMenuElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsXULElement(std::move(aNodeInfo))
  {
  }

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Element> GetActiveChild();
  MOZ_CAN_RUN_SCRIPT void SetActiveChild(Element* arg);
  MOZ_CAN_RUN_SCRIPT bool HandleKeyPress(KeyboardEvent& keyEvent);
  MOZ_CAN_RUN_SCRIPT bool OpenedWithKey();

private:
  virtual ~XULMenuElement() {}
  JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) final;

  nsIFrame* GetFrame();
};

} // namespace dom
} // namespace mozilla

#endif // XULMenuElement_h
