/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEDistantLightElement_h
#define mozilla_dom_SVGFEDistantLightElement_h

#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"

typedef SVGFEUnstyledElement SVGFEDistantLightElementBase;

nsresult NS_NewSVGFEDistantLightElement(nsIContent **aResult,
                                        already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEDistantLightElement : public SVGFEDistantLightElementBase
{
  friend nsresult (::NS_NewSVGFEDistantLightElement(nsIContent **aResult,
                                                    already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEDistantLightElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEDistantLightElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

public:
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedNumber> Azimuth();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Elevation();

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { AZIMUTH, ELEVATION };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEDistantLightElement_h
