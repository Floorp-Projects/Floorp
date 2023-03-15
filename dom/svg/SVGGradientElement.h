/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGGRADIENTELEMENT_H_
#define DOM_SVG_SVGGRADIENTELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedTransformList.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/UniquePtr.h"

nsresult NS_NewSVGLinearGradientElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
nsresult NS_NewSVGRadialGradientElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class SVGGradientFrame;
class SVGLinearGradientFrame;
class SVGRadialGradientFrame;

namespace dom {

class DOMSVGAnimatedTransformList;

//--------------------- Gradients------------------------

using SVGGradientElementBase = SVGElement;

class SVGGradientElement : public SVGGradientElementBase {
  friend class mozilla::SVGGradientFrame;

 protected:
  explicit SVGGradientElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override = 0;

 public:
  // nsIContent
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override = 0;

  virtual SVGAnimatedTransformList* GetAnimatedTransformList(
      uint32_t aFlags = 0) override;
  nsStaticAtom* GetTransformListAttrName() const override {
    return nsGkAtoms::gradientTransform;
  }

  // WebIDL
  already_AddRefed<DOMSVGAnimatedEnumeration> GradientUnits();
  already_AddRefed<DOMSVGAnimatedTransformList> GradientTransform();
  already_AddRefed<DOMSVGAnimatedEnumeration> SpreadMethod();
  already_AddRefed<DOMSVGAnimatedString> Href();

 protected:
  EnumAttributesInfo GetEnumInfo() override;
  StringAttributesInfo GetStringInfo() override;

  enum { GRADIENTUNITS, SPREADMETHOD };
  SVGAnimatedEnumeration mEnumAttributes[2];
  static SVGEnumMapping sSpreadMethodMap[];
  static EnumInfo sEnumInfo[2];

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  // SVGGradientElement values
  UniquePtr<SVGAnimatedTransformList> mGradientTransform;
};

//---------------------Linear Gradients------------------------

using SVGLinearGradientElementBase = SVGGradientElement;

class SVGLinearGradientElement final : public SVGLinearGradientElementBase {
  friend class mozilla::SVGLinearGradientFrame;
  friend nsresult(::NS_NewSVGLinearGradientElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGLinearGradientElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X1();
  already_AddRefed<DOMSVGAnimatedLength> Y1();
  already_AddRefed<DOMSVGAnimatedLength> X2();
  already_AddRefed<DOMSVGAnimatedLength> Y2();

 protected:
  LengthAttributesInfo GetLengthInfo() override;

  enum { ATTR_X1, ATTR_Y1, ATTR_X2, ATTR_Y2 };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

//-------------------------- Radial Gradients ----------------------------

using SVGRadialGradientElementBase = SVGGradientElement;

class SVGRadialGradientElement final : public SVGRadialGradientElementBase {
  friend class mozilla::SVGRadialGradientFrame;
  friend nsresult(::NS_NewSVGRadialGradientElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGRadialGradientElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> Cx();
  already_AddRefed<DOMSVGAnimatedLength> Cy();
  already_AddRefed<DOMSVGAnimatedLength> R();
  already_AddRefed<DOMSVGAnimatedLength> Fx();
  already_AddRefed<DOMSVGAnimatedLength> Fy();
  already_AddRefed<DOMSVGAnimatedLength> Fr();

 protected:
  LengthAttributesInfo GetLengthInfo() override;

  enum { ATTR_CX, ATTR_CY, ATTR_R, ATTR_FX, ATTR_FY, ATTR_FR };
  SVGAnimatedLength mLengthAttributes[6];
  static LengthInfo sLengthInfo[6];
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGGRADIENTELEMENT_H_
