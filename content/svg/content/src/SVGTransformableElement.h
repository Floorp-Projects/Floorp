/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SVGTransformableElement_h
#define SVGTransformableElement_h

#include "mozilla/Attributes.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsSVGElement.h"
#include "gfxMatrix.h"
#include "mozilla/gfx/Matrix.h"

namespace mozilla {
namespace dom {

class SVGAnimatedTransformList;
class SVGGraphicsElement;
class SVGMatrix;
class SVGIRect;

class SVGTransformableElement : public nsSVGElement
{
public:
  SVGTransformableElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
    : nsSVGElement(aNodeInfo) {}
  virtual ~SVGTransformableElement() {}

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE = 0;

  // WebIDL
  already_AddRefed<SVGAnimatedTransformList> Transform();
  nsSVGElement* GetNearestViewportElement();
  nsSVGElement* GetFarthestViewportElement();
  already_AddRefed<SVGIRect> GetBBox(ErrorResult& rv);
  already_AddRefed<SVGMatrix> GetCTM();
  already_AddRefed<SVGMatrix> GetScreenCTM();
  already_AddRefed<SVGMatrix> GetTransformToElement(SVGGraphicsElement& aElement,
                                                    ErrorResult& rv);

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const MOZ_OVERRIDE;

  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                      int32_t aModType) const MOZ_OVERRIDE;


  virtual bool IsEventAttributeName(nsIAtom* aName) MOZ_OVERRIDE;


  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const MOZ_OVERRIDE;
  virtual const gfx::Matrix* GetAnimateMotionTransform() const MOZ_OVERRIDE;
  virtual void SetAnimateMotionTransform(const gfx::Matrix* aMatrix) MOZ_OVERRIDE;

  virtual nsSVGAnimatedTransformList*
    GetAnimatedTransformList(uint32_t aFlags = 0) MOZ_OVERRIDE;
  virtual nsIAtom* GetTransformListAttrName() const MOZ_OVERRIDE {
    return nsGkAtoms::transform;
  }

  virtual bool IsTransformable() MOZ_OVERRIDE { return true; }

protected:
  // nsSVGElement overrides

  nsAutoPtr<nsSVGAnimatedTransformList> mTransforms;

  // XXX maybe move this to property table, to save space on un-animated elems?
  nsAutoPtr<gfx::Matrix> mAnimateMotionTransform;
};

} // namespace dom
} // namespace mozilla

#endif // SVGTransformableElement_h

