/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGMaskElement_h
#define mozilla_dom_SVGMaskElement_h

#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGElement.h"

class nsSVGMaskFrame;

nsresult NS_NewSVGMaskElement(nsIContent **aResult,
                              already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

//--------------------- Masks ------------------------

typedef nsSVGElement SVGMaskElementBase;

class SVGMaskElement MOZ_FINAL : public SVGMaskElementBase
{
  friend class ::nsSVGMaskFrame;

protected:
  friend nsresult (::NS_NewSVGMaskElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGMaskElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

public:
  // nsIContent interface
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> MaskUnits();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> MaskContentUnits();
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();

protected:

  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  enum { MASKUNITS, MASKCONTENTUNITS };
  nsSVGEnum mEnumAttributes[2];
  static EnumInfo sEnumInfo[2];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGMaskElement_h
