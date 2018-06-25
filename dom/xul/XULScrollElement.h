/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULScrollElement_h__
#define XULScrollElement_h__

#include "nsXULElement.h"
#include "Units.h"

namespace mozilla {
namespace dom {


class XULScrollElement final : public nsXULElement
{
public:
  explicit XULScrollElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo): nsXULElement(aNodeInfo)
  {
  }

  MOZ_CAN_RUN_SCRIPT void ScrollByIndex(int32_t aIndex, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void EnsureElementIsVisible(Element& aChild, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void ScrollToElement(Element& child, ErrorResult& aRv);

protected:
  virtual ~XULScrollElement() {}
  JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) final;
};

} // namespace dom
} // namespace mozilla

#endif // XULScrollElement_h
