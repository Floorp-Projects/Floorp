/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGForeignObjectElement_h
#define mozilla_dom_SVGForeignObjectElement_h

#include "mozilla/dom/SVGGraphicsElement.h"
#include "nsSVGLength2.h"

nsresult NS_NewSVGForeignObjectElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo>&& aNodeInfo);

class nsSVGForeignObjectFrame;

namespace mozilla {
namespace dom {

class SVGForeignObjectElement MOZ_FINAL : public SVGGraphicsElement
{
  friend class ::nsSVGForeignObjectFrame;

protected:
  friend nsresult (::NS_NewSVGForeignObjectElement(nsIContent **aResult,
                                                   already_AddRefed<nsINodeInfo>&& aNodeInfo));
  SVGForeignObjectElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx) MOZ_OVERRIDE;

public:
  // nsSVGElement specializations:
  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const MOZ_OVERRIDE;
  virtual bool HasValidDimensions() const MOZ_OVERRIDE;

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedLength> X();
  already_AddRefed<SVGAnimatedLength> Y();
  already_AddRefed<SVGAnimatedLength> Width();
  already_AddRefed<SVGAnimatedLength> Height();

protected:

  virtual LengthAttributesInfo GetLengthInfo() MOZ_OVERRIDE;

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGForeignObjectElement_h

