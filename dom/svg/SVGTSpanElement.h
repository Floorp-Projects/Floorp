/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGTSPANELEMENT_H_
#define DOM_SVG_SVGTSPANELEMENT_H_

#include "mozilla/dom/SVGTextPositioningElement.h"

nsresult NS_NewSVGTSpanElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGTSpanElementBase = SVGTextPositioningElement;

class SVGTSpanElement final : public SVGTSpanElementBase {
 protected:
  friend nsresult(::NS_NewSVGTSpanElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGTSpanElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsIContent interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

 protected:
  EnumAttributesInfo GetEnumInfo() override;
  LengthAttributesInfo GetLengthInfo() override;

  SVGAnimatedEnumeration mEnumAttributes[1];
  SVGAnimatedEnumeration* EnumAttributes() override { return mEnumAttributes; }

  SVGAnimatedLength mLengthAttributes[1];
  SVGAnimatedLength* LengthAttributes() override { return mLengthAttributes; }
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGTSPANELEMENT_H_
