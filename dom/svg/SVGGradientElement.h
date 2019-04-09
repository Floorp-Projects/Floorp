/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGRADIENTELEMENT_H__
#define __NS_SVGGRADIENTELEMENT_H__

#include "nsAutoPtr.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedTransformList.h"
#include "mozilla/dom/SVGElement.h"

class nsSVGGradientFrame;
class nsSVGLinearGradientFrame;
class nsSVGRadialGradientFrame;

nsresult NS_NewSVGLinearGradientElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
nsresult NS_NewSVGRadialGradientElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

class DOMSVGAnimatedTransformList;

//--------------------- Gradients------------------------

typedef SVGElement SVGGradientElementBase;

class SVGGradientElement : public SVGGradientElementBase {
  friend class ::nsSVGGradientFrame;

 protected:
  explicit SVGGradientElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override = 0;

 public:
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override = 0;

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  virtual SVGAnimatedTransformList* GetAnimatedTransformList(
      uint32_t aFlags = 0) override;
  virtual nsStaticAtom* GetTransformListAttrName() const override {
    return nsGkAtoms::gradientTransform;
  }

  // WebIDL
  already_AddRefed<DOMSVGAnimatedEnumeration> GradientUnits();
  already_AddRefed<DOMSVGAnimatedTransformList> GradientTransform();
  already_AddRefed<DOMSVGAnimatedEnumeration> SpreadMethod();
  already_AddRefed<DOMSVGAnimatedString> Href();

 protected:
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { GRADIENTUNITS, SPREADMETHOD };
  SVGAnimatedEnumeration mEnumAttributes[2];
  static SVGEnumMapping sSpreadMethodMap[];
  static EnumInfo sEnumInfo[2];

  enum { HREF, XLINK_HREF };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  // SVGGradientElement values
  nsAutoPtr<SVGAnimatedTransformList> mGradientTransform;
};

//---------------------Linear Gradients------------------------

typedef SVGGradientElement SVGLinearGradientElementBase;

class SVGLinearGradientElement : public SVGLinearGradientElementBase {
  friend class ::nsSVGLinearGradientFrame;
  friend nsresult(::NS_NewSVGLinearGradientElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGLinearGradientElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> X1();
  already_AddRefed<DOMSVGAnimatedLength> Y1();
  already_AddRefed<DOMSVGAnimatedLength> X2();
  already_AddRefed<DOMSVGAnimatedLength> Y2();

 protected:
  virtual LengthAttributesInfo GetLengthInfo() override;

  enum { ATTR_X1, ATTR_Y1, ATTR_X2, ATTR_Y2 };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

//-------------------------- Radial Gradients ----------------------------

typedef SVGGradientElement SVGRadialGradientElementBase;

class SVGRadialGradientElement : public SVGRadialGradientElementBase {
  friend class ::nsSVGRadialGradientFrame;
  friend nsresult(::NS_NewSVGRadialGradientElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGRadialGradientElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedLength> Cx();
  already_AddRefed<DOMSVGAnimatedLength> Cy();
  already_AddRefed<DOMSVGAnimatedLength> R();
  already_AddRefed<DOMSVGAnimatedLength> Fx();
  already_AddRefed<DOMSVGAnimatedLength> Fy();
  already_AddRefed<DOMSVGAnimatedLength> Fr();

 protected:
  virtual LengthAttributesInfo GetLengthInfo() override;

  enum { ATTR_CX, ATTR_CY, ATTR_R, ATTR_FX, ATTR_FY, ATTR_FR };
  SVGAnimatedLength mLengthAttributes[6];
  static LengthInfo sLengthInfo[6];
};

}  // namespace dom
}  // namespace mozilla

#endif
