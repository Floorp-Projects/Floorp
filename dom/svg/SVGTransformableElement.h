/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGTRANSFORMABLEELEMENT_H_
#define DOM_SVG_SVGTRANSFORMABLEELEMENT_H_

#include "gfxMatrix.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimatedTransformList.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/UniquePtr.h"

namespace mozilla::dom {

class DOMSVGAnimatedTransformList;
class SVGGraphicsElement;
class SVGMatrix;
class SVGRect;
struct SVGBoundingBoxOptions;

class SVGTransformableElement : public SVGElement {
 public:
  explicit SVGTransformableElement(already_AddRefed<dom::NodeInfo>&& aNodeInfo)
      : SVGElement(std::move(aNodeInfo)) {}
  virtual ~SVGTransformableElement() = default;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override = 0;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedTransformList> Transform();

  // nsIContent interface
  nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                      int32_t aModType) const override;

  // SVGElement overrides
  bool IsEventAttributeNameInternal(nsAtom* aName) override;

  gfxMatrix PrependLocalTransformsTo(
      const gfxMatrix& aMatrix,
      SVGTransformTypes aWhich = eAllTransforms) const override;
  const gfx::Matrix* GetAnimateMotionTransform() const override;
  void SetAnimateMotionTransform(const gfx::Matrix* aMatrix) override;

  SVGAnimatedTransformList* GetAnimatedTransformList(
      uint32_t aFlags = 0) override;
  nsStaticAtom* GetTransformListAttrName() const override {
    return nsGkAtoms::transform;
  }

  bool IsTransformable() override { return true; }

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

  UniquePtr<SVGAnimatedTransformList> mTransforms;

  // XXX maybe move this to property table, to save space on un-animated elems?
  UniquePtr<gfx::Matrix> mAnimateMotionTransform;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGTRANSFORMABLEELEMENT_H_
