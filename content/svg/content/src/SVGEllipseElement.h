/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGEllipseElement_h
#define mozilla_dom_SVGEllipseElement_h

#include "nsSVGPathGeometryElement.h"
#include "nsSVGLength2.h"

nsresult NS_NewSVGEllipseElement(nsIContent **aResult,
                                 already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGPathGeometryElement SVGEllipseElementBase;

class SVGEllipseElement MOZ_FINAL : public SVGEllipseElementBase
{
protected:
  SVGEllipseElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx,
                             JS::Handle<JSObject*> scope) MOZ_OVERRIDE;
  friend nsresult (::NS_NewSVGEllipseElement(nsIContent **aResult,
                                             already_AddRefed<nsINodeInfo> aNodeInfo));

public:
  // nsSVGSVGElement methods:
  virtual bool HasValidDimensions() const MOZ_OVERRIDE;

  // nsSVGPathGeometryElement methods:
  virtual void ConstructPath(gfxContext *aCtx) MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> Cx();
  already_AddRefed<SVGAnimatedLength> Cy();
  already_AddRefed<SVGAnimatedLength> Rx();
  already_AddRefed<SVGAnimatedLength> Ry();

protected:

  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;

  enum { CX, CY, RX, RY };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGEllipseElement_h
