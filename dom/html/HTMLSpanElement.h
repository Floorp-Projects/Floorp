/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSpanElement_h
#define mozilla_dom_HTMLSpanElement_h

#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

class HTMLSpanElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLSpanElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {}

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

 protected:
  virtual ~HTMLSpanElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLSpanElement_h
