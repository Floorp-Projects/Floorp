/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULTooltipElement_h__
#define XULTooltipElement_h__

#include "XULPopupElement.h"

namespace mozilla::dom {

nsXULElement* NS_NewXULTooltipElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

class XULTooltipElement final : public XULPopupElement {
 public:
  explicit XULTooltipElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : XULPopupElement(std::move(aNodeInfo)) {}
  nsresult Init();

  void AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;
  nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) override;

 protected:
  virtual ~XULTooltipElement() = default;
};

}  // namespace mozilla::dom

#endif  // XULPopupElement_h
