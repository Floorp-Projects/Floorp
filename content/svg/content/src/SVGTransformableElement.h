/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SVGTransformableElement_h
#define SVGTransformableElement_h

#include "nsSVGElement.h"
#include "gfxMatrix.h"
#include "SVGAnimatedTransformList.h"

namespace mozilla {
class DOMSVGAnimatedTransformList;

namespace dom {

class SVGGraphicsElement;
class SVGMatrix;
class SVGIRect;

class SVGTransformableElement : public nsSVGElement
{
public:
  SVGTransformableElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGElement(aNodeInfo) {}
  virtual ~SVGTransformableElement() {}

  // WebIDL
  already_AddRefed<DOMSVGAnimatedTransformList> Transform();
  nsSVGElement* GetNearestViewportElement();
  nsSVGElement* GetFarthestViewportElement();
  already_AddRefed<SVGIRect> GetBBox(ErrorResult& rv);
  already_AddRefed<SVGMatrix> GetCTM();
  already_AddRefed<SVGMatrix> GetScreenCTM();
  already_AddRefed<SVGMatrix> GetTransformToElement(SVGGraphicsElement& aElement,
                                                    ErrorResult& rv);

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                      int32_t aModType) const;


  virtual bool IsEventAttributeName(nsIAtom* aName) MOZ_OVERRIDE;


  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const;
  virtual const gfxMatrix* GetAnimateMotionTransform() const;
  virtual void SetAnimateMotionTransform(const gfxMatrix* aMatrix);

  virtual SVGAnimatedTransformList*
    GetAnimatedTransformList(uint32_t aFlags = 0);
  virtual nsIAtom* GetTransformListAttrName() const {
    return nsGkAtoms::transform;
  }

  virtual bool IsTransformable() { return true; }

protected:
  // nsSVGElement overrides

  nsAutoPtr<SVGAnimatedTransformList> mTransforms;

  // XXX maybe move this to property table, to save space on un-animated elems?
  nsAutoPtr<gfxMatrix> mAnimateMotionTransform;
};

} // namespace dom
} // namespace mozilla

#endif // SVGTransformableElement_h

