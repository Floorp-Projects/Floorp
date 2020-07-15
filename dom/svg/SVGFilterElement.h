/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFILTERELEMENT_H_
#define DOM_SVG_SVGFILTERELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "mozilla/dom/SVGElement.h"

nsresult NS_NewSVGFilterElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGFilterFrame;
class SVGFilterInstance;

namespace dom {
class DOMSVGAnimatedLength;

using SVGFilterElementBase = SVGElement;

class SVGFilterElement : public SVGFilterElementBase {
  friend class mozilla::SVGFilterFrame;
  friend class mozilla::SVGFilterInstance;

 protected:
  friend nsresult(::NS_NewSVGFilterElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGFilterElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsIContent
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  // SVGSVGElement methods:
  virtual bool HasValidDimensions() const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X();
  already_AddRefed<DOMSVGAnimatedLength> Y();
  already_AddRefed<DOMSVGAnimatedLength> Width();
  already_AddRefed<DOMSVGAnimatedLength> Height();
  already_AddRefed<DOMSVGAnimatedEnumeration> FilterUnits();
  already_AddRefed<DOMSVGAnimatedEnumeration> PrimitiveUnits();
  already_AddRefed<DOMSVGAnimatedString> Href();

 protected:
  virtual LengthAttributesInfo GetLengthInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { FILTERUNITS, PRIMITIVEUNITS };
  SVGAnimatedEnumeration mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGFILTERELEMENT_H_
