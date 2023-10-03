/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFEMERGEELEMENT_H_
#define DOM_SVG_SVGFEMERGEELEMENT_H_

#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFEMergeElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGFEMergeElementBase = SVGFilterPrimitiveElement;

class SVGFEMergeElement final : public SVGFEMergeElementBase {
  friend nsresult(::NS_NewSVGFEMergeElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEMergeElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEMergeElementBase(std::move(aNodeInfo)) {}
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  FilterPrimitiveDescription GetPrimitiveDescription(
      SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
      const nsTArray<bool>& aInputsAreTainted,
      nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  SVGAnimatedString& GetResultImageName() override {
    return mStringAttributes[RESULT];
  }
  void GetSourceImageNames(nsTArray<SVGStringInfo>& aSources) override;

  // nsIContent
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  nsresult BindToTree(BindContext& aCtx, nsINode& aParent) override;

 protected:
  StringAttributesInfo GetStringInfo() override;

  enum { RESULT };
  SVGAnimatedString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGFEMERGEELEMENT_H_
