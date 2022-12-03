/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFOREIGNOBJECTELEMENT_H_
#define DOM_SVG_SVGFOREIGNOBJECTELEMENT_H_

#include "mozilla/dom/SVGGraphicsElement.h"
#include "nsCSSPropertyID.h"
#include "SVGAnimatedLength.h"

nsresult NS_NewSVGForeignObjectElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGForeignObjectFrame;

namespace dom {

class SVGForeignObjectElement final : public SVGGraphicsElement {
  friend class mozilla::SVGForeignObjectFrame;

 protected:
  friend nsresult(::NS_NewSVGForeignObjectElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGForeignObjectElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  // SVGElement specializations:
  virtual gfxMatrix PrependLocalTransformsTo(
      const gfxMatrix& aMatrix,
      SVGTransformTypes aWhich = eAllTransforms) const override;
  bool HasValidDimensions() const override;

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* name) const override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  static nsCSSPropertyID GetCSSPropertyIdForAttrEnum(uint8_t aAttrEnum);

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();

 protected:
  LengthAttributesInfo GetLengthInfo() override;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGFOREIGNOBJECTELEMENT_H_
