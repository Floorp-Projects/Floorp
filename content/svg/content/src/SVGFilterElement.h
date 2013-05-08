/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFilterElement_h
#define mozilla_dom_SVGFilterElement_h

#include "nsSVGEnum.h"
#include "nsSVGElement.h"
#include "nsSVGIntegerPair.h"
#include "nsSVGLength2.h"
#include "nsSVGString.h"

typedef nsSVGElement SVGFilterElementBase;

class nsSVGFilterFrame;
class nsAutoFilterInstance;

nsresult NS_NewSVGFilterElement(nsIContent **aResult,
                                already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {
class SVGAnimatedLength;

class SVGFilterElement : public SVGFilterElementBase
{
  friend class ::nsSVGFilterFrame;
  friend class ::nsAutoFilterInstance;

protected:
  friend nsresult (::NS_NewSVGFilterElement(nsIContent **aResult,
                                            already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGFilterElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

public:
  // nsIContent
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  // Invalidate users of this filter
  void Invalidate();

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> FilterUnits();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> PrimitiveUnits();
  already_AddRefed<nsIDOMSVGAnimatedInteger> FilterResX();
  already_AddRefed<nsIDOMSVGAnimatedInteger> FilterResY();
  void SetFilterRes(uint32_t filterResX, uint32_t filterResY);
  already_AddRefed<nsIDOMSVGAnimatedString> Href();

protected:

  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;
  virtual IntegerPairAttributesInfo GetIntegerPairInfo() MOZ_OVERRIDE;
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { FILTERRES };
  nsSVGIntegerPair mIntegerPairAttributes[1];
  static IntegerPairInfo sIntegerPairInfo[1];

  enum { FILTERUNITS, PRIMITIVEUNITS };
  nsSVGEnum mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFilterElement_h
