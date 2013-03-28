/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEBlendElement_h
#define mozilla_dom_SVGFEBlendElement_h

#include "nsSVGFilters.h"
#include "nsSVGEnum.h"

nsresult NS_NewSVGFEBlendElement(nsIContent **aResult,
                                 already_AddRefed<nsINodeInfo> aNodeInfo);
namespace mozilla {
namespace dom {

static const unsigned short SVG_FEBLEND_MODE_UNKNOWN = 0;
static const unsigned short SVG_FEBLEND_MODE_NORMAL = 1;
static const unsigned short SVG_FEBLEND_MODE_MULTIPLY = 2;
static const unsigned short SVG_FEBLEND_MODE_SCREEN = 3;
static const unsigned short SVG_FEBLEND_MODE_DARKEN = 4;
static const unsigned short SVG_FEBLEND_MODE_LIGHTEN = 5;

typedef nsSVGFE SVGFEBlendElementBase;

class SVGFEBlendElement : public SVGFEBlendElementBase
{
  friend nsresult (::NS_NewSVGFEBlendElement(nsIContent **aResult,
                                             already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEBlendElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEBlendElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope) MOZ_OVERRIDE;

public:
  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> In1();
  already_AddRefed<nsIDOMSVGAnimatedString> In2();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> Mode();

protected:

  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { MODE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sModeMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1, IN2 };
  nsSVGString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEBlendElement_h
