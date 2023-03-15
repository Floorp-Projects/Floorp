/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGSTOPELEMENT_H_
#define DOM_SVG_SVGSTOPELEMENT_H_

#include "mozilla/dom/SVGElement.h"
#include "SVGAnimatedNumber.h"

nsresult NS_NewSVGStopElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGStopElementBase = SVGElement;

class SVGStopElement final : public SVGStopElementBase {
 protected:
  friend nsresult(::NS_NewSVGStopElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGStopElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsIContent interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedNumber> Offset();

 protected:
  NumberAttributesInfo GetNumberInfo() override;
  SVGAnimatedNumber mOffset;
  static NumberInfo sNumberInfo;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGSTOPELEMENT_H_
