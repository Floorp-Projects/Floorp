/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGViewportElement_h
#define mozilla_dom_SVGViewportElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/FromParser.h"
#include "nsAutoPtr.h"
#include "nsIContentInlines.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedViewBox.h"
#include "SVGGraphicsElement.h"
#include "SVGImageContext.h"
#include "nsISVGPoint.h"
#include "SVGPreserveAspectRatio.h"

class nsSVGOuterSVGFrame;
class nsSVGViewportFrame;

namespace mozilla {
class AutoPreserveAspectRatioOverride;

namespace dom {
class DOMSVGAnimatedPreserveAspectRatio;
class SVGAnimatedRect;
class SVGViewElement;
class SVGViewportElement;

class svgFloatSize {
 public:
  svgFloatSize(float aWidth, float aHeight) : width(aWidth), height(aHeight) {}
  bool operator!=(const svgFloatSize& rhs) {
    return width != rhs.width || height != rhs.height;
  }
  float width;
  float height;
};

class SVGViewportElement : public SVGGraphicsElement {
  friend class ::nsSVGOuterSVGFrame;
  friend class ::nsSVGViewportFrame;

 protected:
  explicit SVGViewportElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  ~SVGViewportElement() = default;

 public:
  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;

  // SVGElement specializations:
  virtual gfxMatrix PrependLocalTransformsTo(
      const gfxMatrix& aMatrix,
      SVGTransformTypes aWhich = eAllTransforms) const override;

  virtual bool HasValidDimensions() const override;

  // SVGViewportElement methods:

  float GetLength(uint8_t aCtxType);

  // public helpers:

  /**
   * Returns true if this element has a base/anim value for its "viewBox"
   * attribute that defines a viewBox rectangle with finite values, or
   * if there is a view element overriding this element's viewBox and it
   * has a valid viewBox.
   *
   * Note that this does not check whether we need to synthesize a viewBox,
   * so you must call ShouldSynthesizeViewBox() if you need to chck that too.
   *
   * Note also that this method does not pay attention to whether the width or
   * height values of the viewBox rect are positive!
   */
  bool HasViewBox() const { return GetViewBoxInternal().HasRect(); }

  /**
   * Returns true if we should synthesize a viewBox for ourselves (that is, if
   * we're the root element in an image document, and we're not currently being
   * painted for an <svg:image> element).
   *
   * Only call this method if HasViewBox() returns false.
   */
  bool ShouldSynthesizeViewBox() const;

  bool HasViewBoxOrSyntheticViewBox() const {
    return HasViewBox() || ShouldSynthesizeViewBox();
  }

  bool HasChildrenOnlyTransform() const { return mHasChildrenOnlyTransform; }

  void UpdateHasChildrenOnlyTransform();

  enum ChildrenOnlyTransformChangedFlags { eDuringReflow = 1 };

  /**
   * This method notifies the style system that the overflow rects of our
   * immediate childrens' frames need to be updated. It is called by our own
   * frame when changes (e.g. to currentScale) cause our children-only
   * transform to change.
   *
   * The reason we have this method instead of overriding
   * GetAttributeChangeHint is because we need to act on non-attribute (e.g.
   * currentScale) changes in addition to attribute (e.g. viewBox) changes.
   */
  void ChildrenOnlyTransformChanged(uint32_t aFlags = 0);

  gfx::Matrix GetViewBoxTransform() const;

  svgFloatSize GetViewportSize() const {
    return svgFloatSize(mViewportWidth, mViewportHeight);
  }

  void SetViewportSize(const svgFloatSize& aSize) {
    mViewportWidth = aSize.width;
    mViewportHeight = aSize.height;
  }

  // WebIDL
  already_AddRefed<SVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();
  virtual SVGAnimatedViewBox* GetAnimatedViewBox() override;

 protected:
  // implementation helpers:

  bool IsRoot() const {
    NS_ASSERTION((IsInUncomposedDoc() && !GetParent()) ==
                     (OwnerDoc()->GetRootElement() == this),
                 "Can't determine if we're root");
    return IsInUncomposedDoc() && !GetParent();
  }

  /**
   * Returns true if either this is an SVG <svg> element that is the child of
   * another non-foreignObject SVG element, or this is a SVG <symbol> element
   * this is the root of a use-element shadow tree.
   */
  bool IsInner() const {
    const nsIContent* parent = GetFlattenedTreeParent();
    return parent && parent->IsSVGElement() &&
           !parent->IsSVGElement(nsGkAtoms::foreignObject);
  }

  /**
   * Returns the explicit or default preserveAspectRatio, unless we're
   * synthesizing a viewBox, in which case it returns the "none" value.
   */
  virtual SVGPreserveAspectRatio GetPreserveAspectRatioWithOverride() const {
    return mPreserveAspectRatio.GetAnimValue();
  }

  /**
   * Returns the explicit viewBox rect, if specified, or else a synthesized
   * viewBox, if appropriate, or else a viewBox matching the dimensions of the
   * SVG viewport.
   */
  SVGViewBox GetViewBoxWithSynthesis(float aViewportWidth,
                                     float aViewportHeight) const;

  /**
   * Retrieve the value of currentScale and currentTranslate.
   */
  virtual SVGPoint GetCurrentTranslate() const { return SVGPoint(0.0f, 0.0f); }
  virtual float GetCurrentScale() const { return 1.0f; }

  enum { ATTR_X, ATTR_Y, ATTR_WIDTH, ATTR_HEIGHT };
  SVGAnimatedLength mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];
  virtual LengthAttributesInfo GetLengthInfo() override;

  virtual SVGAnimatedPreserveAspectRatio* GetAnimatedPreserveAspectRatio()
      override;

  virtual const SVGAnimatedViewBox& GetViewBoxInternal() const {
    return mViewBox;
  }
  virtual SVGAnimatedTransformList* GetTransformInternal() const {
    return mTransforms;
  }
  SVGAnimatedViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  // The size of the rectangular SVG viewport into which we render. This is
  // not (necessarily) the same as the content area. See:
  //
  //   http://www.w3.org/TR/SVG11/coords.html#ViewportSpace
  //
  // XXXjwatt Currently only used for outer <svg>, but maybe we could use -1 to
  // flag this as an inner <svg> to save the overhead of GetCtx calls?
  // XXXjwatt our frame should probably reset these when it's destroyed.
  float mViewportWidth, mViewportHeight;

  bool mHasChildrenOnlyTransform;
};

}  // namespace dom

}  // namespace mozilla

#endif  // SVGViewportElement_h
