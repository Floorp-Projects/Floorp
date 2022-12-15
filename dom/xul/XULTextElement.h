/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULTextElement_h__
#define XULTextElement_h__

#include "nsXULElement.h"

namespace mozilla::dom {

class XULTextElement final : public nsXULElement {
 public:
  explicit XULTextElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsXULElement(std::move(aNodeInfo)) {}

  bool Disabled() { return GetXULBoolAttr(nsGkAtoms::disabled); }
  MOZ_CAN_RUN_SCRIPT void SetDisabled(bool aValue) {
    SetXULBoolAttr(nsGkAtoms::disabled, aValue, mozilla::IgnoreErrors());
  }
  void GetValue(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::value, aValue);
  }
  MOZ_CAN_RUN_SCRIPT void SetValue(const nsAString& aValue) {
    SetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue, true);
  }
  void GetAccessKey(DOMString& aValue) const {
    GetXULAttr(nsGkAtoms::accesskey, aValue);
  }
  MOZ_CAN_RUN_SCRIPT void SetAccessKey(const nsAString& aValue) {
    SetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, aValue, true);
  }

  nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                      int32_t aModType) const override;

  NS_IMPL_FROMNODE_HELPER(XULTextElement,
                          IsAnyOfXULElements(nsGkAtoms::label,
                                             nsGkAtoms::description));

 protected:
  virtual ~XULTextElement() = default;
  JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;
};

}  // namespace mozilla::dom

#endif  // XULTextElement_h
