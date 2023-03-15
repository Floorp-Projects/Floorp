/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGMASKELEMENT_H_
#define DOM_SVG_SVGMASKELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "mozilla/dom/SVGElement.h"

nsresult NS_NewSVGMaskElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGMaskFrame;

namespace dom {

//--------------------- Masks ------------------------

using SVGMaskElementBase = SVGElement;

class SVGMaskElement final : public SVGMaskElementBase {
  friend class mozilla::SVGMaskFrame;

 protected:
  friend nsresult(::NS_NewSVGMaskElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGMaskElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsIContent interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // SVGSVGElement methods:
  bool HasValidDimensions() const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedEnumeration> MaskUnits();
  already_AddRefed<DOMSVGAnimatedEnumeration> MaskContentUnits();
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();

 protected:
  LengthAttributesInfo GetLengthInfo() override;
  EnumAttributesInfo GetEnumInfo() override;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { MASKUNITS, MASKCONTENTUNITS };
  SVGAnimatedEnumeration mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGMASKELEMENT_H_
