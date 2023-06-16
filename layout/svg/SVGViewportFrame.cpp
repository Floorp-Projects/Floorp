/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGViewportFrame.h"

// Keep others in (case-insensitive) order:
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/SVGContainerFrame.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/SVGViewportElement.h"
#include "nsLayoutUtils.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

namespace mozilla {

//----------------------------------------------------------------------
// ISVGDisplayableFrame methods

void SVGViewportFrame::PaintSVG(gfxContext& aContext,
                                const gfxMatrix& aTransform,
                                imgDrawingParams& aImgParams) {
  NS_ASSERTION(HasAnyStateBits(NS_FRAME_IS_NONDISPLAY),
               "Only painting of non-display SVG should take this code path");

  gfxClipAutoSaveRestore autoSaveClip(&aContext);

  if (StyleDisplay()->IsScrollableOverflow()) {
    float x, y, width, height;
    static_cast<SVGViewportElement*>(GetContent())
        ->GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);

    if (width <= 0 || height <= 0) {
      return;
    }

    gfxRect clipRect = SVGUtils::GetClipRectForFrame(this, x, y, width, height);
    autoSaveClip.TransformedClip(aTransform, clipRect);
  }

  SVGDisplayContainerFrame::PaintSVG(aContext, aTransform, aImgParams);
}

void SVGViewportFrame::ReflowSVG() {
  // mRect must be set before FinishAndStoreOverflow is called in order
  // for our overflow areas to be clipped correctly.
  float x, y, width, height;
  static_cast<SVGViewportElement*>(GetContent())
      ->GetAnimatedLengthValues(&x, &y, &width, &height, nullptr);
  mRect = nsLayoutUtils::RoundGfxRectToAppRect(gfxRect(x, y, width, height),
                                               AppUnitsPerCSSPixel());

  // If we have a filter, we need to invalidate ourselves because filter
  // output can change even if none of our descendants need repainting.
  if (StyleEffects()->HasFilters()) {
    InvalidateFrame();
  }

  SVGDisplayContainerFrame::ReflowSVG();
}

void SVGViewportFrame::NotifySVGChanged(uint32_t aFlags) {
  MOZ_ASSERT(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
             "Invalidation logic may need adjusting");

  if (aFlags & COORD_CONTEXT_CHANGED) {
    SVGViewportElement* svg = static_cast<SVGViewportElement*>(GetContent());

    bool xOrYIsPercentage =
        svg->mLengthAttributes[SVGViewportElement::ATTR_X].IsPercentage() ||
        svg->mLengthAttributes[SVGViewportElement::ATTR_Y].IsPercentage();
    bool widthOrHeightIsPercentage =
        svg->mLengthAttributes[SVGViewportElement::ATTR_WIDTH].IsPercentage() ||
        svg->mLengthAttributes[SVGViewportElement::ATTR_HEIGHT].IsPercentage();

    if (xOrYIsPercentage || widthOrHeightIsPercentage) {
      // Ancestor changes can't affect how we render from the perspective of
      // any rendering observers that we may have, so we don't need to
      // invalidate them. We also don't need to invalidate ourself, since our
      // changed ancestor will have invalidated its entire area, which includes
      // our area.
      // For perf reasons we call this before calling NotifySVGChanged() below.
      SVGUtils::ScheduleReflowSVG(this);
    }

    // Coordinate context changes affect mCanvasTM if we have a
    // percentage 'x' or 'y', or if we have a percentage 'width' or 'height' AND
    // a 'viewBox'.

    if (!(aFlags & TRANSFORM_CHANGED) &&
        (xOrYIsPercentage ||
         (widthOrHeightIsPercentage && svg->HasViewBox()))) {
      aFlags |= TRANSFORM_CHANGED;
    }

    if (svg->HasViewBox() || !widthOrHeightIsPercentage) {
      // Remove COORD_CONTEXT_CHANGED, since we establish the coordinate
      // context for our descendants and this notification won't change its
      // dimensions:
      aFlags &= ~COORD_CONTEXT_CHANGED;

      if (!aFlags) {
        return;  // No notification flags left
      }
    }
  }

  SVGDisplayContainerFrame::NotifySVGChanged(aFlags);
}

SVGBBox SVGViewportFrame::GetBBoxContribution(const Matrix& aToBBoxUserspace,
                                              uint32_t aFlags) {
  // XXXjwatt It seems like authors would want the result to be clipped by the
  // viewport we establish if IsScrollableOverflow() is true.  We should
  // consider doing that.  See bug 1350755.

  SVGBBox bbox;

  if (aFlags & SVGUtils::eForGetClientRects) {
    // XXXjwatt For consistency with the old code this code includes the
    // viewport we establish in the result, but only includes the bounds of our
    // descendants if they are not clipped to that viewport.  However, this is
    // both inconsistent with Chrome and with the specs.  See bug 1350755.
    // Ideally getClientRects/getBoundingClientRect should be consistent with
    // getBBox.
    float x, y, w, h;
    static_cast<SVGViewportElement*>(GetContent())
        ->GetAnimatedLengthValues(&x, &y, &w, &h, nullptr);
    if (w < 0.0f) w = 0.0f;
    if (h < 0.0f) h = 0.0f;
    Rect viewport(x, y, w, h);
    bbox = aToBBoxUserspace.TransformBounds(viewport);
    if (StyleDisplay()->IsScrollableOverflow()) {
      return bbox;
    }
    // Else we're not clipping to our viewport so we fall through and include
    // the bounds of our children.
  }

  SVGBBox descendantsBbox =
      SVGDisplayContainerFrame::GetBBoxContribution(aToBBoxUserspace, aFlags);

  bbox.UnionEdges(descendantsBbox);

  return bbox;
}

nsresult SVGViewportFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None &&
      !HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
    SVGViewportElement* content =
        static_cast<SVGViewportElement*>(GetContent());

    if (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height) {
      nsLayoutUtils::PostRestyleEvent(
          mContent->AsElement(), RestyleHint{0},
          nsChangeHint_InvalidateRenderingObservers);
      SVGUtils::ScheduleReflowSVG(this);

      if (content->HasViewBoxOrSyntheticViewBox()) {
        // make sure our cached transform matrix gets (lazily) updated
        mCanvasTM = nullptr;
        content->ChildrenOnlyTransformChanged();
        SVGUtils::NotifyChildrenOfSVGChange(this, TRANSFORM_CHANGED);
      } else {
        uint32_t flags = COORD_CONTEXT_CHANGED;
        if (mCanvasTM && mCanvasTM->IsSingular()) {
          mCanvasTM = nullptr;
          flags |= TRANSFORM_CHANGED;
        }
        SVGUtils::NotifyChildrenOfSVGChange(this, flags);
      }

    } else if (aAttribute == nsGkAtoms::transform ||
               aAttribute == nsGkAtoms::preserveAspectRatio ||
               aAttribute == nsGkAtoms::viewBox || aAttribute == nsGkAtoms::x ||
               aAttribute == nsGkAtoms::y) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nullptr;

      SVGUtils::NotifyChildrenOfSVGChange(
          this, aAttribute == nsGkAtoms::viewBox
                    ? TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED
                    : TRANSFORM_CHANGED);

      // We don't invalidate for transform changes (the layers code does that).
      // Also note that SVGTransformableElement::GetAttributeChangeHint will
      // return nsChangeHint_UpdateOverflow for "transform" attribute changes
      // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.

      if (aAttribute == nsGkAtoms::x || aAttribute == nsGkAtoms::y) {
        nsLayoutUtils::PostRestyleEvent(
            mContent->AsElement(), RestyleHint{0},
            nsChangeHint_InvalidateRenderingObservers);
        SVGUtils::ScheduleReflowSVG(this);
      } else if (aAttribute == nsGkAtoms::viewBox ||
                 (aAttribute == nsGkAtoms::preserveAspectRatio &&
                  content->HasViewBoxOrSyntheticViewBox())) {
        content->ChildrenOnlyTransformChanged();
        // SchedulePaint sets a global state flag so we only need to call it
        // once (on ourself is fine), not once on each child (despite bug
        // 828240).
        SchedulePaint();
      }
    }
  }

  return NS_OK;
}

nsIFrame* SVGViewportFrame::GetFrameForPoint(const gfxPoint& aPoint) {
  MOZ_ASSERT_UNREACHABLE("A clipPath cannot contain svg or symbol elements");
  return nullptr;
}

//----------------------------------------------------------------------
// ISVGSVGFrame methods:

void SVGViewportFrame::NotifyViewportOrTransformChanged(uint32_t aFlags) {
  // The dimensions of inner-<svg> frames are purely defined by their "width"
  // and "height" attributes, and transform changes can only occur as a result
  // of changes to their "width", "height", "viewBox" or "preserveAspectRatio"
  // attributes. Changes to all of these attributes are handled in
  // AttributeChanged(), so we should never be called.
  NS_ERROR("Not called for SVGViewportFrame");
}

//----------------------------------------------------------------------
// SVGContainerFrame methods:

bool SVGViewportFrame::HasChildrenOnlyTransform(gfx::Matrix* aTransform) const {
  SVGViewportElement* content = static_cast<SVGViewportElement*>(GetContent());

  if (content->HasViewBoxOrSyntheticViewBox()) {
    // XXX Maybe return false if the transform is the identity transform?
    if (aTransform) {
      *aTransform = content->GetViewBoxTransform();
    }
    return true;
  }
  return false;
}

}  // namespace mozilla
