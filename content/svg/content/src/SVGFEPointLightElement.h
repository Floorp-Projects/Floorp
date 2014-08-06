/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEPointLightElement_h
#define mozilla_dom_SVGFEPointLightElement_h

#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"

nsresult NS_NewSVGFEPointLightElement(nsIContent **aResult,
                                      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGFELightElement SVGFEPointLightElementBase;

class SVGFEPointLightElement : public SVGFEPointLightElementBase
{
  friend nsresult (::NS_NewSVGFEPointLightElement(nsIContent **aResult,
                                                  already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
protected:
  SVGFEPointLightElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : SVGFEPointLightElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext *cx) MOZ_OVERRIDE;

public:
  virtual AttributeMap ComputeLightAttributes(nsSVGFilterInstance* aInstance) MOZ_OVERRIDE;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedNumber> X();
  already_AddRefed<SVGAnimatedNumber> Y();
  already_AddRefed<SVGAnimatedNumber> Z();

protected:
  virtual NumberAttributesInfo GetNumberInfo() MOZ_OVERRIDE;

  enum { ATTR_X, ATTR_Y, ATTR_Z };
  nsSVGNumber2 mNumberAttributes[3];
  static NumberInfo sNumberInfo[3];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEPointLightElement_h
