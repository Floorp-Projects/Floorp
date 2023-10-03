/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFEMERGENODEELEMENT_H_
#define DOM_SVG_SVGFEMERGENODEELEMENT_H_

#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFEMergeNodeElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGFEMergeNodeElementBase = SVGFilterPrimitiveChildElement;

class SVGFEMergeNodeElement final : public SVGFEMergeNodeElementBase {
  friend nsresult(::NS_NewSVGFEMergeNodeElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEMergeNodeElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEMergeNodeElementBase(std::move(aNodeInfo)) {}
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  NS_IMPL_FROMNODE_WITH_TAG(SVGFEMergeNodeElement, kNameSpaceID_SVG,
                            feMergeNode)

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                 nsAtom* aAttribute) const override;

  const SVGAnimatedString* GetIn1() { return &mStringAttributes[IN1]; }

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> In1();

 protected:
  StringAttributesInfo GetStringInfo() override;

  enum { IN1 };
  SVGAnimatedString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGFEMERGENODEELEMENT_H_
