/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEDistantLightElement_h
#define mozilla_dom_SVGFEDistantLightElement_h

#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"

nsresult NS_NewSVGFEDistantLightElement(nsIContent **aResult,
                                        already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGFELightElement SVGFEDistantLightElementBase;

class SVGFEDistantLightElement : public SVGFEDistantLightElementBase
{
  friend nsresult (::NS_NewSVGFEDistantLightElement(nsIContent **aResult,
                                                    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
protected:
  explicit SVGFEDistantLightElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : SVGFEDistantLightElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

public:
  virtual AttributeMap ComputeLightAttributes(nsSVGFilterInstance* aInstance) MOZ_OVERRIDE;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedNumber> Azimuth();
  already_AddRefed<SVGAnimatedNumber> Elevation();

protected:
  virtual NumberAttributesInfo GetNumberInfo() MOZ_OVERRIDE;

  enum { AZIMUTH, ELEVATION };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEDistantLightElement_h
