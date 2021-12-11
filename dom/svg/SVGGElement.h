/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGGELEMENT_H_
#define DOM_SVG_SVGGELEMENT_H_

#include "mozilla/dom/SVGGraphicsElement.h"

nsresult NS_NewSVGGElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class SVGGElement final : public SVGGraphicsElement {
 protected:
  explicit SVGGElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;
  friend nsresult(::NS_NewSVGGElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 public:
  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  bool IsNodeOfType(uint32_t aFlags) const override;
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGGELEMENT_H_
