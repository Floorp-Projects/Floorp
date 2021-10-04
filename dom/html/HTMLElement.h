/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLElement_h
#define mozilla_dom_HTMLElement_h

#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class HTMLElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual ~HTMLElement() = default;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

 protected:
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLElement_h
