/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGRectElement_h
#define mozilla_dom_SVGRectElement_h

#include "nsSVGPathGeometryElement.h"
#include "nsSVGLength2.h"

nsresult NS_NewSVGRectElement(nsIContent **aResult,
                              already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

typedef nsSVGPathGeometryElement SVGRectElementBase;

namespace mozilla {
namespace dom {

class SVGRectElement MOZ_FINAL : public SVGRectElementBase
{
protected:
  explicit SVGRectElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx) MOZ_OVERRIDE;
  friend nsresult (::NS_NewSVGRectElement(nsIContent **aResult,
                                          already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

public:
  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const MOZ_OVERRIDE;

  // nsSVGPathGeometryElement methods:
  virtual void GetAsSimplePath(SimplePath* aSimplePath) MOZ_OVERRIDE;
  virtual TemporaryRef<Path> BuildPath(PathBuilder* aBuilder = nullptr) MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Height();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Rx();
  already_AddRefed<SVGAnimatedLength> Ry();

protected:

  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT, ATTR_RX, ATTR_RY };
  nsSVGLength2 mLengthAttributes[6];
  static LengthInfo sLengthInfo[6];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGRectElement_h
