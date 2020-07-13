/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGGeometryFrame.h"

// Keep others in (case-insensitive) order:
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "mozilla/dom/SVGGeometryElement.h"
#include "mozilla/dom/SVGGraphicsElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SVGContextPaint.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGUtils.h"
#include "nsDisplayList.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "SVGAnimatedTransformList.h"
#include "SVGMarkerFrame.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

//----------------------------------------------------------------------
// Implementation

nsIFrame* NS_NewSVGGeometryFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGGeometryFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGGeometryFrame)

//----------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(SVGGeometryFrame)
  NS_QUERYFRAME_ENTRY(ISVGDisplayableFrame)
  NS_QUERYFRAME_ENTRY(SVGGeometryFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsIFrame)

void DisplaySVGGeometry::HitTest(nsDisplayListBuilder* aBuilder,
                                 const nsRect& aRect, HitTestState* aState,
                                 nsTArray<nsIFrame*>* aOutFrames) {
  SVGGeometryFrame* frame = static_cast<SVGGeometryFrame*>(mFrame);
  nsPoint pointRelativeToReferenceFrame = aRect.Center();
  // ToReferenceFrame() includes frame->GetPosition(), our user space position.
  nsPoint userSpacePtInAppUnits = pointRelativeToReferenceFrame -
                                  (ToReferenceFrame() - frame->GetPosition());
  gfxPoint userSpacePt =
      gfxPoint(userSpacePtInAppUnits.x, userSpacePtInAppUnits.y) /
      AppUnitsPerCSSPixel();
  if (frame->GetFrameForPoint(userSpacePt)) {
    aOutFrames->AppendElement(frame);
  }
}

void DisplaySVGGeometry::Paint(nsDisplayListBuilder* aBuilder,
                               gfxContext* aCtx) {
  uint32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  // ToReferenceFrame includes our mRect offset, but painting takes
  // account of that too. To avoid double counting, we subtract that
  // here.
  nsPoint offset = ToReferenceFrame() - mFrame->GetPosition();

  gfxPoint devPixelOffset =
      nsLayoutUtils::PointToGfxPoint(offset, appUnitsPerDevPixel);

  gfxMatrix tm = SVGUtils::GetCSSPxToDevPxMatrix(mFrame) *
                 gfxMatrix::Translation(devPixelOffset);
  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  static_cast<SVGGeometryFrame*>(mFrame)->PaintSVG(*aCtx, tm, imgParams);

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, imgParams.result);
}

void DisplaySVGGeometry::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  const auto* geometry =
      static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsPaintedDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                  aInvalidRegion);
}

//----------------------------------------------------------------------
// nsIFrame methods

void SVGGeometryFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  AddStateBits(aParent->GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD);
  nsIFrame::Init(aContent, aParent, aPrevInFlow);
}

nsresult SVGGeometryFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType) {
  // We don't invalidate for transform changes (the layers code does that).
  // Also note that SVGTransformableElement::GetAttributeChangeHint will
  // return nsChangeHint_UpdateOverflow for "transform" attribute changes
  // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.

  if (aNameSpaceID == kNameSpaceID_None &&
      (static_cast<SVGGeometryElement*>(GetContent())
           ->AttributeDefinesGeometry(aAttribute))) {
    nsLayoutUtils::PostRestyleEvent(mContent->AsElement(), RestyleHint{0},
                                    nsChangeHint_InvalidateRenderingObservers);
    SVGUtils::ScheduleReflowSVG(this);
  }
  return NS_OK;
}

/* virtual */
void SVGGeometryFrame::DidSetComputedStyle(ComputedStyle* aOldComputedStyle) {
  nsIFrame::DidSetComputedStyle(aOldComputedStyle);

  if (aOldComputedStyle) {
    SVGGeometryElement* element =
        static_cast<SVGGeometryElement*>(GetContent());

    const auto* oldStyleSVG = aOldComputedStyle->StyleSVG();
    if (!SVGContentUtils::ShapeTypeHasNoCorners(GetContent())) {
      if (StyleSVG()->mStrokeLinecap != oldStyleSVG->mStrokeLinecap &&
          element->IsSVGElement(nsGkAtoms::path)) {
        // If the stroke-linecap changes to or from "butt" then our element
        // needs to update its cached Moz2D Path, since SVGPathData::BuildPath
        // decides whether or not to insert little lines into the path for zero
        // length subpaths base on that property.
        element->ClearAnyCachedPath();
      } else if (HasAnyStateBits(NS_STATE_SVG_CLIPPATH_CHILD)) {
        if (StyleSVG()->mClipRule != oldStyleSVG->mClipRule) {
          // Moz2D Path objects are fill-rule specific.
          // For clipPath we use clip-rule as the path's fill-rule.
          element->ClearAnyCachedPath();
        }
      } else {
        if (StyleSVG()->mFillRule != oldStyleSVG->mFillRule) {
          // Moz2D Path objects are fill-rule specific.
          element->ClearAnyCachedPath();
        }
      }
    }

    if (element->IsGeometryChangedViaCSS(*Style(), *aOldComputedStyle)) {
      element->ClearAnyCachedPath();
    }
  }
}

bool SVGGeometryFrame::IsSVGTransformed(
    gfx::Matrix* aOwnTransform, gfx::Matrix* aFromParentTransform) const {
  bool foundTransform = false;

  // Check if our parent has children-only transforms:
  nsIFrame* parent = GetParent();
  if (parent &&
      parent->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer)) {
    foundTransform =
        static_cast<SVGContainerFrame*>(parent)->HasChildrenOnlyTransform(
            aFromParentTransform);
  }

  SVGElement* content = static_cast<SVGElement*>(GetContent());
  SVGAnimatedTransformList* transformList = content->GetAnimatedTransformList();
  if ((transformList && transformList->HasTransform()) ||
      content->GetAnimateMotionTransform()) {
    if (aOwnTransform) {
      *aOwnTransform = gfx::ToMatrix(
          content->PrependLocalTransformsTo(gfxMatrix(), eUserSpaceToParent));
    }
    foundTransform = true;
  }
  return foundTransform;
}

void SVGGeometryFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                        const nsDisplayListSet& aLists) {
  if (!static_cast<const SVGElement*>(GetContent())->HasValidDimensions() ||
      ((!IsVisibleForPainting() || StyleEffects()->mOpacity == 0.0f) &&
       aBuilder->IsForPainting())) {
    return;
  }
  DisplayOutline(aBuilder, aLists);
  aLists.Content()->AppendNewToTop<DisplaySVGGeometry>(aBuilder, this);
}

//----------------------------------------------------------------------
// ISVGDisplayableFrame methods

void SVGGeometryFrame::PaintSVG(gfxContext& aContext,
                                const gfxMatrix& aTransform,
                                imgDrawingParams& aImgParams,
                                const nsIntRect* aDirtyRect) {
  if (!StyleVisibility()->IsVisible()) {
    return;
  }

  // Matrix to the geometry's user space:
  gfxMatrix newMatrix =
      aContext.CurrentMatrixDouble().PreMultiply(aTransform).NudgeToIntegers();
  if (newMatrix.IsSingular()) {
    return;
  }

  uint32_t paintOrder = StyleSVG()->mPaintOrder;
  if (!paintOrder) {
    Render(&aContext, eRenderFill | eRenderStroke, newMatrix, aImgParams);
    PaintMarkers(aContext, aTransform, aImgParams);
  } else {
    while (paintOrder) {
      auto component = StylePaintOrder(paintOrder & kPaintOrderMask);
      switch (component) {
        case StylePaintOrder::Fill:
          Render(&aContext, eRenderFill, newMatrix, aImgParams);
          break;
        case StylePaintOrder::Stroke:
          Render(&aContext, eRenderStroke, newMatrix, aImgParams);
          break;
        case StylePaintOrder::Markers:
          PaintMarkers(aContext, aTransform, aImgParams);
          break;
        default:
          MOZ_FALLTHROUGH_ASSERT("Unknown paint-order variant, how?");
        case StylePaintOrder::Normal:
          break;
      }
      paintOrder >>= kPaintOrderShift;
    }
  }
}

nsIFrame* SVGGeometryFrame::GetFrameForPoint(const gfxPoint& aPoint) {
  FillRule fillRule;
  uint16_t hitTestFlags;
  if (HasAnyStateBits(NS_STATE_SVG_CLIPPATH_CHILD)) {
    hitTestFlags = SVG_HIT_TEST_FILL;
    fillRule = SVGUtils::ToFillRule(StyleSVG()->mClipRule);
  } else {
    hitTestFlags = GetHitTestFlags();
    if (!hitTestFlags) {
      return nullptr;
    }
    if (hitTestFlags & SVG_HIT_TEST_CHECK_MRECT) {
      gfxRect rect = nsLayoutUtils::RectToGfxRect(mRect, AppUnitsPerCSSPixel());
      if (!rect.Contains(aPoint)) {
        return nullptr;
      }
    }
    fillRule = SVGUtils::ToFillRule(StyleSVG()->mFillRule);
  }

  bool isHit = false;

  SVGGeometryElement* content = static_cast<SVGGeometryElement*>(GetContent());

  // Using ScreenReferenceDrawTarget() opens us to Moz2D backend specific hit-
  // testing bugs. Maybe we should use a BackendType::CAIRO DT for hit-testing
  // so that we get more consistent/backwards compatible results?
  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  RefPtr<Path> path = content->GetOrBuildPath(drawTarget, fillRule);
  if (!path) {
    return nullptr;  // no path, so we don't paint anything that can be hit
  }

  if (hitTestFlags & SVG_HIT_TEST_FILL) {
    isHit = path->ContainsPoint(ToPoint(aPoint), Matrix());
  }
  if (!isHit && (hitTestFlags & SVG_HIT_TEST_STROKE)) {
    Point point = ToPoint(aPoint);
    SVGContentUtils::AutoStrokeOptions stroke;
    SVGContentUtils::GetStrokeOptions(&stroke, content, Style(), nullptr);
    gfxMatrix userToOuterSVG;
    if (SVGUtils::GetNonScalingStrokeTransform(this, &userToOuterSVG)) {
      // We need to transform the path back into the appropriate ancestor
      // coordinate system in order for non-scaled stroke to be correct.
      // Naturally we also need to transform the point into the same
      // coordinate system in order to hit-test against the path.
      point = ToMatrix(userToOuterSVG).TransformPoint(point);
      RefPtr<PathBuilder> builder =
          path->TransformedCopyToBuilder(ToMatrix(userToOuterSVG), fillRule);
      path = builder->Finish();
    }
    isHit = path->StrokeContainsPoint(stroke, point, Matrix());
  }

  if (isHit && SVGUtils::HitTestClip(this, aPoint)) {
    return this;
  }

  return nullptr;
}

void SVGGeometryFrame::ReflowSVG() {
  NS_ASSERTION(SVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  MOZ_ASSERT(!HasAnyStateBits(NS_FRAME_IS_NONDISPLAY),
             "ReflowSVG mechanism not designed for this");

  if (!SVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  uint32_t flags = SVGUtils::eBBoxIncludeFill | SVGUtils::eBBoxIncludeStroke |
                   SVGUtils::eBBoxIncludeMarkers;
  // Our "visual" overflow rect needs to be valid for building display lists
  // for hit testing, which means that for certain values of 'pointer-events'
  // it needs to include the geometry of the fill or stroke even when the fill/
  // stroke don't actually render (e.g. when stroke="none" or
  // stroke-opacity="0"). GetHitTestFlags() accounts for 'pointer-events'.
  uint16_t hitTestFlags = GetHitTestFlags();
  if ((hitTestFlags & SVG_HIT_TEST_FILL)) {
    flags |= SVGUtils::eBBoxIncludeFillGeometry;
  }
  if ((hitTestFlags & SVG_HIT_TEST_STROKE)) {
    flags |= SVGUtils::eBBoxIncludeStrokeGeometry;
  }

  gfxRect extent = GetBBoxContribution(Matrix(), flags).ToThebesRect();
  mRect = nsLayoutUtils::RoundGfxRectToAppRect(extent, AppUnitsPerCSSPixel());

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    SVGObserverUtils::UpdateEffects(this);
  }

  nsRect overflow = nsRect(nsPoint(0, 0), mRect.Size());
  nsOverflowAreas overflowAreas(overflow, overflow);
  FinishAndStoreOverflow(overflowAreas, mRect.Size());

  RemoveStateBits(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                  NS_FRAME_HAS_DIRTY_CHILDREN);

  // Invalidate, but only if this is not our first reflow (since if it is our
  // first reflow then we haven't had our first paint yet).
  if (!GetParent()->HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    InvalidateFrame();
  }
}

void SVGGeometryFrame::NotifySVGChanged(uint32_t aFlags) {
  MOZ_ASSERT(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
             "Invalidation logic may need adjusting");

  // Changes to our ancestors may affect how we render when we are rendered as
  // part of our ancestor (specifically, if our coordinate context changes size
  // and we have percentage lengths defining our geometry, then we need to be
  // reflowed). However, ancestor changes cannot affect how we render when we
  // are rendered as part of any rendering observers that we may have.
  // Therefore no need to notify rendering observers here.

  // Don't try to be too smart trying to avoid the ScheduleReflowSVG calls
  // for the stroke properties examined below. Checking HasStroke() is not
  // enough, since what we care about is whether we include the stroke in our
  // overflow rects or not, and we sometimes deliberately include stroke
  // when it's not visible. See the complexities of GetBBoxContribution.

  if (aFlags & COORD_CONTEXT_CHANGED) {
    auto* geom = static_cast<SVGGeometryElement*>(GetContent());
    // Stroke currently contributes to our mRect, which is why we have to take
    // account of stroke-width here. Note that we do not need to take account
    // of stroke-dashoffset since, although that can have a percentage value
    // that is resolved against our coordinate context, it does not affect our
    // mRect.
    const auto& strokeWidth = StyleSVG()->mStrokeWidth;
    if (geom->GeometryDependsOnCoordCtx() ||
        (strokeWidth.IsLengthPercentage() &&
         strokeWidth.AsLengthPercentage().HasPercent())) {
      geom->ClearAnyCachedPath();
      SVGUtils::ScheduleReflowSVG(this);
    }
  }

  if ((aFlags & TRANSFORM_CHANGED) && StyleSVGReset()->HasNonScalingStroke()) {
    // Stroke currently contributes to our mRect, and our stroke depends on
    // the transform to our outer-<svg> if |vector-effect:non-scaling-stroke|.
    SVGUtils::ScheduleReflowSVG(this);
  }
}

SVGBBox SVGGeometryFrame::GetBBoxContribution(const Matrix& aToBBoxUserspace,
                                              uint32_t aFlags) {
  SVGBBox bbox;

  if (aToBBoxUserspace.IsSingular()) {
    // XXX ReportToConsole
    return bbox;
  }

  if ((aFlags & SVGUtils::eForGetClientRects) &&
      aToBBoxUserspace.PreservesAxisAlignedRectangles()) {
    Rect rect = NSRectToRect(mRect, AppUnitsPerCSSPixel());
    bbox = aToBBoxUserspace.TransformBounds(rect);
    return bbox;
  }

  SVGGeometryElement* element = static_cast<SVGGeometryElement*>(GetContent());

  bool getFill = (aFlags & SVGUtils::eBBoxIncludeFillGeometry) ||
                 ((aFlags & SVGUtils::eBBoxIncludeFill) &&
                  !StyleSVG()->mFill.kind.IsNone());

  bool getStroke =
      (aFlags & SVGUtils::eBBoxIncludeStrokeGeometry) ||
      ((aFlags & SVGUtils::eBBoxIncludeStroke) && SVGUtils::HasStroke(this));

  SVGContentUtils::AutoStrokeOptions strokeOptions;
  if (getStroke) {
    SVGContentUtils::GetStrokeOptions(&strokeOptions, element, Style(), nullptr,
                                      SVGContentUtils::eIgnoreStrokeDashing);
  } else {
    // Override the default line width of 1.f so that when we call
    // GetGeometryBounds below the result doesn't include stroke bounds.
    strokeOptions.mLineWidth = 0.f;
  }

  Rect simpleBounds;
  bool gotSimpleBounds = false;
  gfxMatrix userToOuterSVG;
  if (getStroke &&
      SVGUtils::GetNonScalingStrokeTransform(this, &userToOuterSVG)) {
    Matrix moz2dUserToOuterSVG = ToMatrix(userToOuterSVG);
    if (moz2dUserToOuterSVG.IsSingular()) {
      return bbox;
    }
    gotSimpleBounds = element->GetGeometryBounds(
        &simpleBounds, strokeOptions, aToBBoxUserspace, &moz2dUserToOuterSVG);
  } else {
    gotSimpleBounds = element->GetGeometryBounds(&simpleBounds, strokeOptions,
                                                 aToBBoxUserspace);
  }

  if (gotSimpleBounds) {
    bbox = simpleBounds;
  } else {
    // Get the bounds using a Moz2D Path object (more expensive):
    RefPtr<DrawTarget> tmpDT;
#ifdef XP_WIN
    // Unfortunately D2D backed DrawTarget produces bounds with rounding errors
    // when whole number results are expected, even in the case of trivial
    // calculations. To avoid that and meet the expectations of web content we
    // have to use a CAIRO DrawTarget. The most efficient way to do that is to
    // wrap the cached cairo_surface_t from ScreenReferenceSurface():
    RefPtr<gfxASurface> refSurf =
        gfxPlatform::GetPlatform()->ScreenReferenceSurface();
    tmpDT = gfxPlatform::CreateDrawTargetForSurface(refSurf, IntSize(1, 1));
#else
    tmpDT = gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
#endif

    FillRule fillRule = SVGUtils::ToFillRule(
        HasAnyStateBits(NS_STATE_SVG_CLIPPATH_CHILD) ? StyleSVG()->mClipRule
                                                     : StyleSVG()->mFillRule);
    RefPtr<Path> pathInUserSpace = element->GetOrBuildPath(tmpDT, fillRule);
    if (!pathInUserSpace) {
      return bbox;
    }
    RefPtr<Path> pathInBBoxSpace;
    if (aToBBoxUserspace.IsIdentity()) {
      pathInBBoxSpace = pathInUserSpace;
    } else {
      RefPtr<PathBuilder> builder =
          pathInUserSpace->TransformedCopyToBuilder(aToBBoxUserspace, fillRule);
      pathInBBoxSpace = builder->Finish();
      if (!pathInBBoxSpace) {
        return bbox;
      }
    }

    // Be careful when replacing the following logic to get the fill and stroke
    // extents independently (instead of computing the stroke extents from the
    // path extents). You may think that you can just use the stroke extents if
    // there is both a fill and a stroke. In reality it's necessary to
    // calculate both the fill and stroke extents, and take the union of the
    // two. There are two reasons for this:
    //
    // # Due to stroke dashing, in certain cases the fill extents could
    //   actually extend outside the stroke extents.
    // # If the stroke is very thin, cairo won't paint any stroke, and so the
    //   stroke bounds that it will return will be empty.

    Rect pathBBoxExtents = pathInBBoxSpace->GetBounds();
    if (!pathBBoxExtents.IsFinite()) {
      // This can happen in the case that we only have a move-to command in the
      // path commands, in which case we know nothing gets rendered.
      return bbox;
    }

    // Account for fill:
    if (getFill) {
      bbox = pathBBoxExtents;
    }

    // Account for stroke:
    if (getStroke) {
#if 0
      // This disabled code is how we would calculate the stroke bounds using
      // Moz2D Path::GetStrokedBounds(). Unfortunately at the time of writing
      // it there are two problems that prevent us from using it.
      //
      // First, it seems that some of the Moz2D backends are really dumb. Not
      // only do some GetStrokeOptions() implementations sometimes
      // significantly overestimate the stroke bounds, but if an argument is
      // passed for the aTransform parameter then they just return bounds-of-
      // transformed-bounds.  These two things combined can lead the bounds to
      // be unacceptably oversized, leading to massive over-invalidation.
      //
      // Second, the way we account for non-scaling-stroke by transforming the
      // path using the transform to the outer-<svg> element is not compatible
      // with the way that SVGGeometryFrame::Reflow() inserts a scale
      // into aToBBoxUserspace and then scales the bounds that we return.
      SVGContentUtils::AutoStrokeOptions strokeOptions;
      SVGContentUtils::GetStrokeOptions(&strokeOptions, element,
                                        Style(), nullptr,
                                        SVGContentUtils::eIgnoreStrokeDashing);
      Rect strokeBBoxExtents;
      gfxMatrix userToOuterSVG;
      if (SVGUtils::GetNonScalingStrokeTransform(this, &userToOuterSVG)) {
        Matrix outerSVGToUser = ToMatrix(userToOuterSVG);
        outerSVGToUser.Invert();
        Matrix outerSVGToBBox = aToBBoxUserspace * outerSVGToUser;
        RefPtr<PathBuilder> builder =
          pathInUserSpace->TransformedCopyToBuilder(ToMatrix(userToOuterSVG));
        RefPtr<Path> pathInOuterSVGSpace = builder->Finish();
        strokeBBoxExtents =
          pathInOuterSVGSpace->GetStrokedBounds(strokeOptions, outerSVGToBBox);
      } else {
        strokeBBoxExtents =
          pathInUserSpace->GetStrokedBounds(strokeOptions, aToBBoxUserspace);
      }
      MOZ_ASSERT(strokeBBoxExtents.IsFinite(), "bbox is about to go bad");
      bbox.UnionEdges(strokeBBoxExtents);
#else
      // For now we just use SVGUtils::PathExtentsToMaxStrokeExtents:
      gfxRect strokeBBoxExtents = SVGUtils::PathExtentsToMaxStrokeExtents(
          ThebesRect(pathBBoxExtents), this, ThebesMatrix(aToBBoxUserspace));
      MOZ_ASSERT(ToRect(strokeBBoxExtents).IsFinite(),
                 "bbox is about to go bad");
      bbox.UnionEdges(strokeBBoxExtents);
#endif
    }
  }

  // Account for markers:
  if ((aFlags & SVGUtils::eBBoxIncludeMarkers) != 0 && element->IsMarkable()) {
    SVGMarkerFrame* markerFrames[SVGMark::eTypeCount];
    if (SVGObserverUtils::GetAndObserveMarkers(this, &markerFrames)) {
      nsTArray<SVGMark> marks;
      element->GetMarkPoints(&marks);
      if (uint32_t num = marks.Length()) {
        float strokeWidth = SVGUtils::GetStrokeWidth(this);
        for (uint32_t i = 0; i < num; i++) {
          const SVGMark& mark = marks[i];
          SVGMarkerFrame* frame = markerFrames[mark.type];
          if (frame) {
            SVGBBox mbbox = frame->GetMarkBBoxContribution(
                aToBBoxUserspace, aFlags, this, mark, strokeWidth);
            MOZ_ASSERT(mbbox.IsFinite(), "bbox is about to go bad");
            bbox.UnionEdges(mbbox);
          }
        }
      }
    }
  }

  return bbox;
}

//----------------------------------------------------------------------
// SVGGeometryFrame methods:

gfxMatrix SVGGeometryFrame::GetCanvasTM() {
  NS_ASSERTION(GetParent(), "null parent");

  auto* parent = static_cast<SVGContainerFrame*>(GetParent());
  auto* content = static_cast<SVGGraphicsElement*>(GetContent());

  return content->PrependLocalTransformsTo(parent->GetCanvasTM());
}

void SVGGeometryFrame::Render(gfxContext* aContext, uint32_t aRenderComponents,
                              const gfxMatrix& aTransform,
                              imgDrawingParams& aImgParams) {
  MOZ_ASSERT(!aTransform.IsSingular());

  DrawTarget* drawTarget = aContext->GetDrawTarget();

  MOZ_ASSERT(drawTarget);
  if (!drawTarget->IsValid()) {
    return;
  }

  FillRule fillRule = SVGUtils::ToFillRule(
      HasAnyStateBits(NS_STATE_SVG_CLIPPATH_CHILD) ? StyleSVG()->mClipRule
                                                   : StyleSVG()->mFillRule);

  SVGGeometryElement* element = static_cast<SVGGeometryElement*>(GetContent());

  AntialiasMode aaMode =
      (StyleSVG()->mShapeRendering == StyleShapeRendering::Optimizespeed ||
       StyleSVG()->mShapeRendering == StyleShapeRendering::Crispedges)
          ? AntialiasMode::NONE
          : AntialiasMode::SUBPIXEL;

  // We wait as late as possible before setting the transform so that we don't
  // set it unnecessarily if we return early (it's an expensive operation for
  // some backends).
  gfxContextMatrixAutoSaveRestore autoRestoreTransform(aContext);
  aContext->SetMatrixDouble(aTransform);

  if (HasAnyStateBits(NS_STATE_SVG_CLIPPATH_CHILD)) {
    // We don't complicate this code with GetAsSimplePath since the cost of
    // masking will dwarf Path creation overhead anyway.
    RefPtr<Path> path = element->GetOrBuildPath(drawTarget, fillRule);
    if (path) {
      ColorPattern white(ToDeviceColor(sRGBColor(1.0f, 1.0f, 1.0f, 1.0f)));
      drawTarget->Fill(path, white,
                       DrawOptions(1.0f, CompositionOp::OP_OVER, aaMode));
    }
    return;
  }

  SVGGeometryElement::SimplePath simplePath;
  RefPtr<Path> path;

  element->GetAsSimplePath(&simplePath);
  if (!simplePath.IsPath()) {
    path = element->GetOrBuildPath(drawTarget, fillRule);
    if (!path) {
      return;
    }
  }

  SVGContextPaint* contextPaint =
      SVGContextPaint::GetContextPaint(GetContent());

  if (aRenderComponents & eRenderFill) {
    GeneralPattern fillPattern;
    SVGUtils::MakeFillPatternFor(this, aContext, &fillPattern, aImgParams,
                                 contextPaint);

    if (fillPattern.GetPattern()) {
      DrawOptions drawOptions(1.0f, CompositionOp::OP_OVER, aaMode);
      if (simplePath.IsRect()) {
        drawTarget->FillRect(simplePath.AsRect(), fillPattern, drawOptions);
      } else if (path) {
        drawTarget->Fill(path, fillPattern, drawOptions);
      }
    }
  }

  if ((aRenderComponents & eRenderStroke) &&
      SVGUtils::HasStroke(this, contextPaint)) {
    // Account for vector-effect:non-scaling-stroke:
    gfxMatrix userToOuterSVG;
    if (SVGUtils::GetNonScalingStrokeTransform(this, &userToOuterSVG)) {
      // A simple Rect can't be transformed with rotate/skew, so let's switch
      // to using a real path:
      if (!path) {
        path = element->GetOrBuildPath(drawTarget, fillRule);
        if (!path) {
          return;
        }
        simplePath.Reset();
      }
      // We need to transform the path back into the appropriate ancestor
      // coordinate system, and paint it it that coordinate system, in order
      // for non-scaled stroke to paint correctly.
      gfxMatrix outerSVGToUser = userToOuterSVG;
      outerSVGToUser.Invert();
      aContext->Multiply(outerSVGToUser);
      RefPtr<PathBuilder> builder =
          path->TransformedCopyToBuilder(ToMatrix(userToOuterSVG), fillRule);
      path = builder->Finish();
    }
    GeneralPattern strokePattern;
    SVGUtils::MakeStrokePatternFor(this, aContext, &strokePattern, aImgParams,
                                   contextPaint);

    if (strokePattern.GetPattern()) {
      SVGContentUtils::AutoStrokeOptions strokeOptions;
      SVGContentUtils::GetStrokeOptions(&strokeOptions,
                                        static_cast<SVGElement*>(GetContent()),
                                        Style(), contextPaint);
      // GetStrokeOptions may set the line width to zero as an optimization
      if (strokeOptions.mLineWidth <= 0) {
        return;
      }
      DrawOptions drawOptions(1.0f, CompositionOp::OP_OVER, aaMode);
      if (simplePath.IsRect()) {
        drawTarget->StrokeRect(simplePath.AsRect(), strokePattern,
                               strokeOptions, drawOptions);
      } else if (simplePath.IsLine()) {
        drawTarget->StrokeLine(simplePath.Point1(), simplePath.Point2(),
                               strokePattern, strokeOptions, drawOptions);
      } else {
        drawTarget->Stroke(path, strokePattern, strokeOptions, drawOptions);
      }
    }
  }
}

void SVGGeometryFrame::PaintMarkers(gfxContext& aContext,
                                    const gfxMatrix& aTransform,
                                    imgDrawingParams& aImgParams) {
  auto* element = static_cast<SVGGeometryElement*>(GetContent());

  if (element->IsMarkable()) {
    SVGMarkerFrame* markerFrames[SVGMark::eTypeCount];
    if (SVGObserverUtils::GetAndObserveMarkers(this, &markerFrames)) {
      nsTArray<SVGMark> marks;
      element->GetMarkPoints(&marks);
      if (uint32_t num = marks.Length()) {
        SVGContextPaint* contextPaint =
            SVGContextPaint::GetContextPaint(GetContent());
        float strokeWidth = SVGUtils::GetStrokeWidth(this, contextPaint);
        for (uint32_t i = 0; i < num; i++) {
          const SVGMark& mark = marks[i];
          SVGMarkerFrame* frame = markerFrames[mark.type];
          if (frame) {
            frame->PaintMark(aContext, aTransform, this, mark, strokeWidth,
                             aImgParams);
          }
        }
      }
    }
  }
}

uint16_t SVGGeometryFrame::GetHitTestFlags() {
  return SVGUtils::GetGeometryHitTestFlags(this);
}
}  // namespace mozilla
