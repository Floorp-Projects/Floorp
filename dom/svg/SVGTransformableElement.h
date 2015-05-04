/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
struct SVGBoundingBoxOptions;

class SVGTransformableElement : public nsSVGElement
{
public:
  explicit SVGTransformableElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsSVGElement(aNodeInfo) {}
  virtual ~SVGTransformableElement() {}

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override = 0;

  // WebIDL
  already_AddRefed<SVGAnimatedTransformList> Transform();
  nsSVGElement* GetNearestViewportElement();
  nsSVGElement* GetFarthestViewportElement();
  already_AddRefed<SVGIRect> GetBBox(const SVGBoundingBoxOptions& aOptions, 
                                     ErrorResult& rv);
  already_AddRefed<SVGMatrix> GetCTM();
  already_AddRefed<SVGMatrix> GetScreenCTM();
  already_AddRefed<SVGMatrix> GetTransformToElement(SVGGraphicsElement& aElement,
                                                    ErrorResult& rv);

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const override;

  nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                      int32_t aModType) const override;


  // nsSVGElement overrides
  virtual bool IsEventAttributeName(nsIAtom* aName) override;


  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const override;
  virtual const gfx::Matrix* GetAnimateMotionTransform() const override;
  virtual void SetAnimateMotionTransform(const gfx::Matrix* aMatrix) override;

  virtual nsSVGAnimatedTransformList*
    GetAnimatedTransformList(uint32_t aFlags = 0) override;
  virtual nsIAtom* GetTransformListAttrName() const override {
    return nsGkAtoms::transform;
  }

  virtual bool IsTransformable() override { return true; }

protected:
  nsAutoPtr<nsSVGAnimatedTransformList> mTransforms;

  // XXX maybe move this to property table, to save space on un-animated elems?
  nsAutoPtr<gfx::Matrix> mAnimateMotionTransform;
};

} // namespace dom
} // namespace mozilla

#endif // SVGTransformableElement_h
