/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGRADIENTELEMENT_H__
#define __NS_SVGGRADIENTELEMENT_H__

#include "nsSVGElement.h"
#include "nsSVGLength2.h"
#include "nsSVGEnum.h"
#include "nsSVGString.h"
#include "SVGAnimatedTransformList.h"

static const unsigned short SVG_SPREADMETHOD_UNKNOWN = 0;
static const unsigned short SVG_SPREADMETHOD_PAD     = 1;
static const unsigned short SVG_SPREADMETHOD_REFLECT = 2;
static const unsigned short SVG_SPREADMETHOD_REPEAT  = 3;

class nsSVGGradientFrame;
class nsSVGLinearGradientFrame;
class nsSVGRadialGradientFrame;

nsresult
NS_NewSVGLinearGradientElement(nsIContent** aResult,
                               already_AddRefed<nsINodeInfo> aNodeInfo);
nsresult
NS_NewSVGRadialGradientElement(nsIContent** aResult,
                               already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {

class DOMSVGAnimatedTransformList;

namespace dom {

//--------------------- Gradients------------------------

typedef nsSVGElement SVGGradientElementBase;

class SVGGradientElement : public SVGGradientElementBase
{
  friend class ::nsSVGGradientFrame;

protected:
  SVGGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject*
  WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE = 0;

public:
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE = 0;

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual SVGAnimatedTransformList*
    GetAnimatedTransformList(uint32_t aFlags = 0);
  virtual nsIAtom* GetTransformListAttrName() const {
    return nsGkAtoms::gradientTransform;
  }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> GradientUnits();
  already_AddRefed<DOMSVGAnimatedTransformList> GradientTransform();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> SpreadMethod();
  already_AddRefed<nsIDOMSVGAnimatedString> Href();

protected:
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { GRADIENTUNITS, SPREADMETHOD };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sSpreadMethodMap[];
  static EnumInfo sEnumInfo[2];

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

  // SVGGradientElement values
  nsAutoPtr<SVGAnimatedTransformList> mGradientTransform;
};

//---------------------Linear Gradients------------------------

typedef SVGGradientElement SVGLinearGradientElementBase;

class SVGLinearGradientElement : public SVGLinearGradientElementBase
{
  friend class ::nsSVGLinearGradientFrame;
  friend nsresult
    (::NS_NewSVGLinearGradientElement(nsIContent** aResult,
                                      already_AddRefed<nsINodeInfo> aNodeInfo));

protected:
  SVGLinearGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject*
  WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

public:
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X1();
  already_AddRefed<SVGAnimatedLength> Y1();
  already_AddRefed<SVGAnimatedLength> X2();
  already_AddRefed<SVGAnimatedLength> Y2();

protected:

  virtual LengthAttributesInfo GetLengthInfo();

  enum { ATTR_X1, ATTR_Y1, ATTR_X2, ATTR_Y2 };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

//-------------------------- Radial Gradients ----------------------------

typedef SVGGradientElement SVGRadialGradientElementBase;

class SVGRadialGradientElement : public SVGRadialGradientElementBase
{
  friend class ::nsSVGRadialGradientFrame;
  friend nsresult
    (::NS_NewSVGRadialGradientElement(nsIContent** aResult,
                                      already_AddRefed<nsINodeInfo> aNodeInfo));

protected:
  SVGRadialGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject*
  WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

public:
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> Cx();
  already_AddRefed<SVGAnimatedLength> Cy();
  already_AddRefed<SVGAnimatedLength> R();
  already_AddRefed<SVGAnimatedLength> Fx();
  already_AddRefed<SVGAnimatedLength> Fy();
protected:

  virtual LengthAttributesInfo GetLengthInfo();

  enum { ATTR_CX, ATTR_CY, ATTR_R, ATTR_FX, ATTR_FY };
  nsSVGLength2 mLengthAttributes[5];
  static LengthInfo sLengthInfo[5];
};

} // namespace dom
} // namespace mozilla

#endif
