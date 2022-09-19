/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_xul_XULButtonElement_h__
#define dom_xul_XULButtonElement_h__

#include "nsXULElement.h"

namespace mozilla::dom {

class XULButtonElement final : public nsXULElement {
 public:
  explicit XULButtonElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsXULElement(std::move(aNodeInfo)) {}

  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool MouseClicked(WidgetGUIEvent&);
  MOZ_CAN_RUN_SCRIPT nsresult PostHandleEvent(EventChainPostVisitor&) override;

 private:
  void Blurred();
  bool mIsHandlingKeyEvent = false;
};

}  // namespace mozilla::dom

#endif
