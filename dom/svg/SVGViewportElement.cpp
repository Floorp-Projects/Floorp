/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGViewportElement.h"

#include <stdint.h>
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Likely.h"
#include "mozilla/SMILTypes.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/SVGViewElement.h"

#include "DOMSVGLength.h"
#include "DOMSVGPoint.h"
#include "nsContentUtils.h"
#include "nsFrameSelection.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsISVGSVGFrame.h"
#include "nsLayoutUtils.h"
#include "nsStyleUtil.h"

#include <algorithm>
#include "prtime.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

SVGElement::LengthInfo SVGViewportElement::sLengthInfo[4] = {
    {nsGkAtoms::x, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::y, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
    {nsGkAtoms::width, 100, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::height, 100, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
};

//----------------------------------------------------------------------
// Implementation

SVGViewportElement::SVGViewportElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGGraphicsElement(std::move(aNodeInfo)),
      mViewportWidth(0),
      mViewportHeight(0),
      mHasChildrenOnlyTransform(false) {}

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedRect> SVGViewportElement::ViewBox() {
  return mViewBox.ToSVGAnimatedRect(this);
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGViewportElement::PreserveAspectRatio() {
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGViewportElement::IsAttributeMapped(const nsAtom* name) const {
  // We want to map the 'width' and 'height' attributes into style for
  // outer-<svg>, except when the attributes aren't set (since their default
  // values of '100%' can cause unexpected and undesirable behaviour for SVG
  // inline in HTML). We rely on SVGElement::UpdateContentStyleRule() to
  // prevent mapping of the default values into style (it only maps attributes
  // that are set). We also rely on a check in SVGElement::
  // UpdateContentStyleRule() to prevent us mapping the attributes when they're
  // given a <length> value that is not currently recognized by the SVG
  // specification.

  if (!IsInner() && (name == nsGkAtoms::width || name == nsGkAtoms::height)) {
    return true;
  }

  static const MappedAttributeEntry* const map[] = {sColorMap,
                                                    sFEFloodMap,
                                                    sFillStrokeMap,
                                                    sFiltersMap,
                                                    sFontSpecificationMap,
                                                    sGradientStopMap,
                                                    sGraphicsMap,
                                                    sLightingEffectsMap,
                                                    sMarkersMap,
                                                    sTextContentElementsMap,
                                                    sViewportsMap};

  return FindAttributeDependence(name, map) ||
         SVGGraphicsElement::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// SVGElement overrides

// Helper for GetViewBoxTransform on root <svg> node
// * aLength: internal value for our <svg> width or height attribute.
// * aViewportLength: length of the corresponding dimension of the viewport.
// * aSelf: the outermost <svg> node itself.
// NOTE: aSelf is not an ancestor viewport element, so it can't be used to
// resolve percentage lengths. (It can only be used to resolve
// 'em'/'ex'-valued units).
inline float ComputeSynthesizedViewBoxDimension(
    const SVGAnimatedLength& aLength, float aViewportLength,
    const SVGViewportElement* aSelf) {
  if (aLength.IsPercentage()) {
    return aViewportLength * aLength.GetAnimValInSpecifiedUnits() / 100.0f;
  }

  return aLength.GetAnimValue(const_cast<SVGViewportElement*>(aSelf));
}

//----------------------------------------------------------------------
// public helpers:

void SVGViewportElement::UpdateHasChildrenOnlyTransform() {
  bool hasChildrenOnlyTransform =
      HasViewBoxOrSyntheticViewBox() ||
      (IsRoot() && (GetCurrentTranslate() != SVGPoint(0.0f, 0.0f) ||
                    GetCurrentScale() != 1.0f));
  mHasChildrenOnlyTransform = hasChildrenOnlyTransform;
}

void SVGViewportElement::ChildrenOnlyTransformChanged(uint32_t aFlags) {
  // Avoid wasteful calls:
  MOZ_ASSERT(!(GetPrimaryFrame()->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
             "Non-display SVG frames don't maintain overflow rects");

  nsChangeHint changeHint;

  bool hadChildrenOnlyTransform = mHasChildrenOnlyTransform;

  UpdateHasChildrenOnlyTransform();

  if (hadChildrenOnlyTransform != mHasChildrenOnlyTransform) {
    // Reconstruct the frame tree to handle stacking context changes:
    // XXXjwatt don't do this for root-<svg> or even outer-<svg>?
    changeHint = nsChangeHint_ReconstructFrame;
  } else {
    // We just assume the old and new transforms are different.
    changeHint = nsChangeHint(nsChangeHint_UpdateOverflow |
                              nsChangeHint_ChildrenOnlyTransform);
  }

  // If we're not reconstructing the frame tree, then we only call
  // PostRestyleEvent if we're not being called under reflow to avoid recursing
  // to death. See bug 767056 comments 10 and 12. Since our nsSVGOuterSVGFrame
  // is being reflowed we're going to invalidate and repaint its entire area
  // anyway (which will include our children).
  if ((changeHint & nsChangeHint_ReconstructFrame) ||
      !(aFlags & eDuringReflow)) {
    nsLayoutUtils::PostRestyleEvent(this, RestyleHint{0}, changeHint);
  }
}

gfx::Matrix SVGViewportElement::GetViewBoxTransform() const {
  float viewportWidth, viewportHeight;
  if (IsInner()) {
    SVGViewportElement* ctx = GetCtx();
    viewportWidth = mLengthAttributes[ATTR_WIDTH].GetAnimValue(ctx);
    viewportHeight = mLengthAttributes[ATTR_HEIGHT].GetAnimValue(ctx);
  } else {
    viewportWidth = mViewportWidth;
    viewportHeight = mViewportHeight;
  }

  if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
    return gfx::Matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);  // singular
  }

  SVGViewBox viewBox = GetViewBoxWithSynthesis(viewportWidth, viewportHeight);

  if (viewBox.width <= 0.0f || viewBox.height <= 0.0f) {
    return gfx::Matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);  // singular
  }

  return SVGContentUtils::GetViewBoxTransform(
      viewportWidth, viewportHeight, viewBox.x, viewBox.y, viewBox.width,
      viewBox.height, GetPreserveAspectRatioWithOverride());
}
//----------------------------------------------------------------------
// SVGViewportElement

float SVGViewportElement::GetLength(uint8_t aCtxType) {
  const SVGViewBox* viewbox = GetViewBoxInternal().HasRect()
                                  ? &GetViewBoxInternal().GetAnimValue()
                                  : nullptr;

  float h = 0.0f, w = 0.0f;
  bool shouldComputeWidth =
           (aCtxType == SVGContentUtils::X || aCtxType == SVGContentUtils::XY),
       shouldComputeHeight =
           (aCtxType == SVGContentUtils::Y || aCtxType == SVGContentUtils::XY);

  if (viewbox) {
    w = viewbox->width;
    h = viewbox->height;
  } else if (IsInner()) {
    SVGViewportElement* ctx = GetCtx();
    if (shouldComputeWidth) {
      w = mLengthAttributes[ATTR_WIDTH].GetAnimValue(ctx);
    }
    if (shouldComputeHeight) {
      h = mLengthAttributes[ATTR_HEIGHT].GetAnimValue(ctx);
    }
  } else if (ShouldSynthesizeViewBox()) {
    if (shouldComputeWidth) {
      w = ComputeSynthesizedViewBoxDimension(mLengthAttributes[ATTR_WIDTH],
                                             mViewportWidth, this);
    }
    if (shouldComputeHeight) {
      h = ComputeSynthesizedViewBoxDimension(mLengthAttributes[ATTR_HEIGHT],
                                             mViewportHeight, this);
    }
  } else {
    w = mViewportWidth;
    h = mViewportHeight;
  }

  w = std::max(w, 0.0f);
  h = std::max(h, 0.0f);

  switch (aCtxType) {
    case SVGContentUtils::X:
      return w;
    case SVGContentUtils::Y:
      return h;
    case SVGContentUtils::XY:
      return float(SVGContentUtils::ComputeNormalizedHypotenuse(w, h));
  }
  return 0;
}

//----------------------------------------------------------------------
// SVGElement methods

/* virtual */
gfxMatrix SVGViewportElement::PrependLocalTransformsTo(
    const gfxMatrix& aMatrix, SVGTransformTypes aWhich) const {
  // 'transform' attribute (or an override from a fragment identifier):
  gfxMatrix userToParent;

  if (aWhich == eUserSpaceToParent || aWhich == eAllTransforms) {
    userToParent = GetUserToParentTransform(mAnimateMotionTransform,
                                            GetTransformInternal());
    if (aWhich == eUserSpaceToParent) {
      return userToParent * aMatrix;
    }
  }

  gfxMatrix childToUser;

  if (IsInner()) {
    float x, y;
    const_cast<SVGViewportElement*>(this)->GetAnimatedLengthValues(&x, &y,
                                                                   nullptr);
    childToUser = ThebesMatrix(GetViewBoxTransform().PostTranslate(x, y));
  } else if (IsRoot()) {
    SVGPoint translate = GetCurrentTranslate();
    float scale = GetCurrentScale();
    childToUser =
        ThebesMatrix(GetViewBoxTransform()
                         .PostScale(scale, scale)
                         .PostTranslate(translate.GetX(), translate.GetY()));
  } else {
    // outer-<svg>, but inline in some other content:
    childToUser = ThebesMatrix(GetViewBoxTransform());
  }

  if (aWhich == eAllTransforms) {
    return childToUser * userToParent * aMatrix;
  }

  MOZ_ASSERT(aWhich == eChildToUserSpace, "Unknown TransformTypes");

  // The following may look broken because pre-multiplying our eChildToUserSpace
  // transform with another matrix without including our eUserSpaceToParent
  // transform between the two wouldn't make sense.  We don't expect that to
  // ever happen though.  We get here either when the identity matrix has been
  // passed because our caller just wants our eChildToUserSpace transform, or
  // when our eUserSpaceToParent transform has already been multiplied into the
  // matrix that our caller passes (such as when we're called from PaintSVG).
  return childToUser * aMatrix;
}

/* virtual */
bool SVGViewportElement::HasValidDimensions() const {
  return !IsInner() ||
         ((!mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() ||
           mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
          (!mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() ||
           mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0));
}

SVGAnimatedViewBox* SVGViewportElement::GetAnimatedViewBox() {
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio*
SVGViewportElement::GetAnimatedPreserveAspectRatio() {
  return &mPreserveAspectRatio;
}

bool SVGViewportElement::ShouldSynthesizeViewBox() const {
  MOZ_ASSERT(!HasViewBox(), "Should only be called if we lack a viewBox");

  return IsRoot() && OwnerDoc()->IsBeingUsedAsImage();
}

//----------------------------------------------------------------------
// implementation helpers

SVGViewBox SVGViewportElement::GetViewBoxWithSynthesis(
    float aViewportWidth, float aViewportHeight) const {
  if (GetViewBoxInternal().HasRect()) {
    return GetViewBoxInternal().GetAnimValue();
  }

  if (ShouldSynthesizeViewBox()) {
    // Special case -- fake a viewBox, using height & width attrs.
    // (Use |this| as context, since if we get here, we're outermost <svg>.)
    return SVGViewBox(
        0, 0,
        ComputeSynthesizedViewBoxDimension(mLengthAttributes[ATTR_WIDTH],
                                           mViewportWidth, this),
        ComputeSynthesizedViewBoxDimension(mLengthAttributes[ATTR_HEIGHT],
                                           mViewportHeight, this));
  }

  // No viewBox attribute, so we shouldn't auto-scale. This is equivalent
  // to having a viewBox that exactly matches our viewport size.
  return SVGViewBox(0, 0, aViewportWidth, aViewportHeight);
}

SVGElement::LengthAttributesInfo SVGViewportElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

}  // namespace dom
}  // namespace mozilla
