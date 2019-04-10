/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SVGTransformableElement_h
#define SVGTransformableElement_h

#include "nsAutoPtr.h"
#include "SVGAnimatedTransformList.h"
#include "gfxMatrix.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/gfx/Matrix.h"

namespace mozilla {
namespace dom {

class DOMSVGAnimatedTransformList;
class SVGGraphicsElement;
class SVGMatrix;
class SVGIRect;
struct SVGBoundingBoxOptions;

class SVGTransformableElement : public SVGElement {
 public:
  explicit SVGTransformableElement(already_AddRefed<dom::NodeInfo>&& aNodeInfo)
      : SVGElement(std::move(aNodeInfo)) {}
  virtual ~SVGTransformableElement() = default;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override = 0;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedTransformList> Transform();
  SVGElement* GetNearestViewportElement();
  SVGElement* GetFarthestViewportElement();
  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<SVGIRect> GetBBox(const SVGBoundingBoxOptions& aOptions,
                                     ErrorResult& rv);
  already_AddRefed<SVGMatrix> GetCTM();
  already_AddRefed<SVGMatrix> GetScreenCTM();
  already_AddRefed<SVGMatrix> GetTransformToElement(
      SVGGraphicsElement& aElement, ErrorResult& rv);

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                      int32_t aModType) const override;

  // SVGElement overrides
  virtual bool IsEventAttributeNameInternal(nsAtom* aName) override;

  virtual gfxMatrix PrependLocalTransformsTo(
      const gfxMatrix& aMatrix,
      SVGTransformTypes aWhich = eAllTransforms) const override;
  virtual const gfx::Matrix* GetAnimateMotionTransform() const override;
  virtual void SetAnimateMotionTransform(const gfx::Matrix* aMatrix) override;

  virtual SVGAnimatedTransformList* GetAnimatedTransformList(
      uint32_t aFlags = 0) override;
  virtual nsStaticAtom* GetTransformListAttrName() const override {
    return nsGkAtoms::transform;
  }

  virtual bool IsTransformable() override { return true; }

 protected:
  /**
   * Helper for overrides of PrependLocalTransformsTo.  If both arguments are
   * provided they are multiplied in the order in which the arguments appear,
   * and the result is returned.  If neither argument is provided, the identity
   * matrix is returned.  If only one argument is provided its transform is
   * returned.
   */
  static gfxMatrix GetUserToParentTransform(
      const gfx::Matrix* aAnimateMotionTransform,
      const SVGAnimatedTransformList* aTransforms);

  nsAutoPtr<SVGAnimatedTransformList> mTransforms;

  // XXX maybe move this to property table, to save space on un-animated elems?
  nsAutoPtr<gfx::Matrix> mAnimateMotionTransform;
};

}  // namespace dom
}  // namespace mozilla

#endif  // SVGTransformableElement_h
