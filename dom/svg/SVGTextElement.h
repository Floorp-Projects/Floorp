/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGTEXTELEMENT_H_
#define DOM_SVG_SVGTEXTELEMENT_H_

#include "mozilla/dom/SVGTextPositioningElement.h"

nsresult NS_NewSVGTextElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGTextElementBase = SVGTextPositioningElement;

class SVGTextElement final : public SVGTextElementBase {
 protected:
  explicit SVGTextElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

  friend nsresult(::NS_NewSVGTextElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

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

#endif  // DOM_SVG_SVGTEXTELEMENT_H_
