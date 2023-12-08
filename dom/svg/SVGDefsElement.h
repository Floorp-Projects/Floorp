/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGDEFSELEMENT_H_
#define DOM_SVG_SVGDEFSELEMENT_H_

#include "SVGGraphicsElement.h"

nsresult NS_NewSVGDefsElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

class SVGDefsElement final : public SVGGraphicsElement {
 protected:
  friend nsresult(::NS_NewSVGDefsElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGDefsElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  // defs elements are not focusable.
  Focusable IsFocusableWithoutStyle(bool aWithMouse) override { return {}; }
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGDEFSELEMENT_H_
