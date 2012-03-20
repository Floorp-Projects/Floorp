/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSVGPathGeometryFrame.h"

#include "nsGkAtoms.h"
#include "nsRenderingContext.h"
#include "nsSVGMarkerFrame.h"
#include "nsSVGUtils.h"
#include "nsSVGEffects.h"
#include "nsSVGGraphicElement.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsSVGRect.h"
#include "nsSVGPathGeometryElement.h"
#include "gfxContext.h"
#include "gfxPlatform.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGPathGeometryFrame(nsIPresShell* aPresShell,
                           nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGPathGeometryFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGPathGeometryFrame)

//----------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(nsSVGPathGeometryFrame)
  NS_QUERYFRAME_ENTRY(nsISVGChildFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGPathGeometryFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGPathGeometryFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                         nsIAtom*        aAttribute,
                                         PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (static_cast<nsSVGPathGeometryElement*>
                  (mContent)->AttributeDefinesGeometry(aAttribute) ||
       aAttribute == nsGkAtoms::transform))
    nsSVGUtils::InvalidateAndScheduleBoundsUpdate(this);

  return NS_OK;
}

/* virtual */ void
nsSVGPathGeometryFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsSVGPathGeometryFrameBase::DidSetStyleContext(aOldStyleContext);

  // XXX: we'd like to use the style_hint mechanism and the
  // ContentStateChanged/AttributeChanged functions for style changes
  // to get slightly finer granularity, but unfortunately the
  // style_hints don't map very well onto svg. Here seems to be the
  // best place to deal with style changes:

  nsSVGUtils::InvalidateAndScheduleBoundsUpdate(this);
}

nsIAtom *
nsSVGPathGeometryFrame::GetType() const
{
  return nsGkAtoms::svgPathGeometryFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGPathGeometryFrame::PaintSVG(nsRenderingContext *aContext,
                                 const nsIntRect *aDirtyRect)
{
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  /* render */
  Render(aContext);

  if (static_cast<nsSVGPathGeometryElement*>(mContent)->IsMarkable()) {
    MarkerProperties properties = GetMarkerProperties(this);
      
    if (properties.MarkersExist()) {
      float strokeWidth = GetStrokeWidth();
        
      nsTArray<nsSVGMark> marks;
      static_cast<nsSVGPathGeometryElement*>
                 (mContent)->GetMarkPoints(&marks);
        
      PRUint32 num = marks.Length();

      if (num) {
        nsSVGMarkerFrame *frame = properties.GetMarkerStartFrame();
        if (frame)
          frame->PaintMark(aContext, this, &marks[0], strokeWidth);

        frame = properties.GetMarkerMidFrame();
        if (frame) {
          for (PRUint32 i = 1; i < num - 1; i++)
            frame->PaintMark(aContext, this, &marks[i], strokeWidth);
        }

        frame = properties.GetMarkerEndFrame();
        if (frame)
          frame->PaintMark(aContext, this, &marks[num-1], strokeWidth);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGPathGeometryFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  PRUint16 fillRule, hitTestFlags;
  if (GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD) {
    hitTestFlags = SVG_HIT_TEST_FILL;
    fillRule = GetClipRule();
  } else {
    hitTestFlags = GetHitTestFlags();
    // XXX once bug 614732 is fixed, aPoint won't need any conversion in order
    // to compare it with mRect.
    gfxMatrix canvasTM = GetCanvasTM();
    if (canvasTM.IsSingular()) {
      return nsnull;
    }
    nsPoint point =
      nsSVGUtils::TransformOuterSVGPointToChildFrame(aPoint, canvasTM, PresContext());
    if (!hitTestFlags || ((hitTestFlags & SVG_HIT_TEST_CHECK_MRECT) &&
                          !mRect.Contains(point)))
      return nsnull;
    fillRule = GetStyleSVG()->mFillRule;
  }

  bool isHit = false;

  nsRefPtr<gfxContext> context =
    new gfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface());

  GeneratePath(context);
  gfxPoint userSpacePoint =
    context->DeviceToUser(gfxPoint(PresContext()->AppUnitsToGfxUnits(aPoint.x),
                                   PresContext()->AppUnitsToGfxUnits(aPoint.y)));

  if (fillRule == NS_STYLE_FILL_RULE_EVENODD)
    context->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
  else
    context->SetFillRule(gfxContext::FILL_RULE_WINDING);

  if (hitTestFlags & SVG_HIT_TEST_FILL)
    isHit = context->PointInFill(userSpacePoint);
  if (!isHit && (hitTestFlags & SVG_HIT_TEST_STROKE)) {
    SetupCairoStrokeHitGeometry(context);
    isHit = context->PointInStroke(userSpacePoint);
  }

  if (isHit && nsSVGUtils::HitTestClip(this, aPoint))
    return this;

  return nsnull;
}

NS_IMETHODIMP_(nsRect)
nsSVGPathGeometryFrame::GetCoveredRegion()
{
  // See bug 614732 comment 32:
  //return nsSVGUtils::TransformFrameRectToOuterSVG(mRect, GetCanvasTM(), PresContext());
  return mCoveredRegion;
}

void
nsSVGPathGeometryFrame::UpdateBounds()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingUpdateBounds(this),
               "This call is probaby a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                    "UpdateBounds mechanism not designed for this");

  if (!nsSVGUtils::NeedsUpdatedBounds(this)) {
    return;
  }

  gfxRect extent = GetBBoxContribution(gfxMatrix(),
    nsSVGUtils::eBBoxIncludeFill | nsSVGUtils::eBBoxIgnoreFillIfNone |
    nsSVGUtils::eBBoxIncludeStroke | nsSVGUtils::eBBoxIgnoreStrokeIfNone |
    nsSVGUtils::eBBoxIncludeMarkers);
  mRect = nsLayoutUtils::RoundGfxRectToAppRect(extent,
            PresContext()->AppUnitsPerCSSPixel());

  // See bug 614732 comment 32.
  mCoveredRegion = nsSVGUtils::TransformFrameRectToOuterSVG(
    mRect, GetCanvasTM(), PresContext());

  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);

  if (!(GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // We only invalidate if our outer-<svg> has already had its
    // initial reflow (since if it hasn't, its entire area will be
    // invalidated when it gets that initial reflow):
    nsSVGUtils::InvalidateBounds(this, true);
  }
}

void
nsSVGPathGeometryFrame::NotifySVGChanged(PRUint32 aFlags)
{
  NS_ABORT_IF_FALSE(!(aFlags & DO_NOT_NOTIFY_RENDERING_OBSERVERS) ||
                    (GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                    "Must be NS_STATE_SVG_NONDISPLAY_CHILD!");

  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  if (!(aFlags & DO_NOT_NOTIFY_RENDERING_OBSERVERS)) {
    nsSVGUtils::InvalidateAndScheduleBoundsUpdate(this);
  }
}

gfxRect
nsSVGPathGeometryFrame::GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                            PRUint32 aFlags)
{
  if (aToBBoxUserspace.IsSingular()) {
    // XXX ReportToConsole
    return gfxRect(0.0, 0.0, 0.0, 0.0);
  }

  nsRefPtr<gfxContext> context =
    new gfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface());

  GeneratePath(context, &aToBBoxUserspace);
  context->IdentityMatrix();

  gfxRect bbox;

  // Be careful when replacing the following logic to get the fill and stroke
  // extents independently (instead of computing the stroke extents from the
  // path extents). You may think that you can just use the stroke extents if
  // there is both a fill and a stroke. In reality it's necessary to calculate
  // both the fill and stroke extents, and take the union of the two. There are
  // two reasons for this:
  //
  // # Due to stroke dashing, in certain cases the fill extents could actually
  //   extend outside the stroke extents.
  // # If the stroke is very thin, cairo won't paint any stroke, and so the
  //   stroke bounds that it will return will be empty.

  gfxRect pathExtents = context->GetUserPathExtent();

  // Account for fill:
  if ((aFlags & nsSVGUtils::eBBoxIncludeFill) != 0 &&
      ((aFlags & nsSVGUtils::eBBoxIgnoreFillIfNone) == 0 ||
       GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None)) {
    bbox = pathExtents;
  }

  // Account for stroke:
  if ((aFlags & nsSVGUtils::eBBoxIncludeStroke) != 0 &&
      ((aFlags & nsSVGUtils::eBBoxIgnoreStrokeIfNone) == 0 || HasStroke())) {
    // We can't use context->GetUserStrokeExtent() since it doesn't work for
    // device space extents. Instead we approximate the stroke extents from
    // pathExtents using PathExtentsToMaxStrokeExtents.
    if (pathExtents.Width() <= 0 && pathExtents.Height() <= 0) {
      // We have a zero length path, but it may still have non-empty stroke
      // bounds depending on the value of stroke-linecap. We need to fix up
      // pathExtents before it can be used with PathExtentsToMaxStrokeExtents
      // though, because if pathExtents is empty, its position will not have
      // been set. Happily we can use context->GetUserStrokeExtent() to find
      // the center point of the extents even though it gets the extents wrong.
      SetupCairoStrokeGeometry(context);
      pathExtents.MoveTo(context->GetUserStrokeExtent().Center());
      pathExtents.SizeTo(0, 0);
    }
    bbox =
      bbox.Union(nsSVGUtils::PathExtentsToMaxStrokeExtents(pathExtents,
                                                           this,
                                                           aToBBoxUserspace));
  }

  // Account for markers:
  if ((aFlags & nsSVGUtils::eBBoxIncludeMarkers) != 0 &&
      static_cast<nsSVGPathGeometryElement*>(mContent)->IsMarkable()) {

    float strokeWidth = GetStrokeWidth();
    MarkerProperties properties = GetMarkerProperties(this);

    if (properties.MarkersExist()) {
      nsTArray<nsSVGMark> marks;
      static_cast<nsSVGPathGeometryElement*>(mContent)->GetMarkPoints(&marks);
      PRUint32 num = marks.Length();

      if (num) {
        nsSVGMarkerFrame *frame = properties.GetMarkerStartFrame();
        if (frame) {
          gfxRect mbbox =
            frame->GetMarkBBoxContribution(aToBBoxUserspace, aFlags, this,
                                           &marks[0], strokeWidth);
          bbox.UnionRect(bbox, mbbox);
        }

        frame = properties.GetMarkerMidFrame();
        if (frame) {
          for (PRUint32 i = 1; i < num - 1; i++) {
            gfxRect mbbox =
              frame->GetMarkBBoxContribution(aToBBoxUserspace, aFlags, this,
                                             &marks[i], strokeWidth);
            bbox.UnionRect(bbox, mbbox);
          }
        }

        frame = properties.GetMarkerEndFrame();
        if (frame) {
          gfxRect mbbox =
            frame->GetMarkBBoxContribution(aToBBoxUserspace, aFlags, this,
                                           &marks[num-1], strokeWidth);
          bbox.UnionRect(bbox, mbbox);
        }
      }
    }
  }

  return bbox;
}

//----------------------------------------------------------------------
// nsSVGGeometryFrame methods:

gfxMatrix
nsSVGPathGeometryFrame::GetCanvasTM()
{
  NS_ASSERTION(mParent, "null parent");

  nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(mParent);
  nsSVGGraphicElement *content = static_cast<nsSVGGraphicElement*>(mContent);

  return content->PrependLocalTransformsTo(parent->GetCanvasTM());
}

//----------------------------------------------------------------------
// nsSVGPathGeometryFrame methods:

nsSVGPathGeometryFrame::MarkerProperties
nsSVGPathGeometryFrame::GetMarkerProperties(nsSVGPathGeometryFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  MarkerProperties result;
  const nsStyleSVG *style = aFrame->GetStyleSVG();
  result.mMarkerStart =
    nsSVGEffects::GetMarkerProperty(style->mMarkerStart, aFrame,
                                    nsSVGEffects::MarkerBeginProperty());
  result.mMarkerMid =
    nsSVGEffects::GetMarkerProperty(style->mMarkerMid, aFrame,
                                    nsSVGEffects::MarkerMiddleProperty());
  result.mMarkerEnd =
    nsSVGEffects::GetMarkerProperty(style->mMarkerEnd, aFrame,
                                    nsSVGEffects::MarkerEndProperty());
  return result;
}

nsSVGMarkerFrame *
nsSVGPathGeometryFrame::MarkerProperties::GetMarkerStartFrame()
{
  if (!mMarkerStart)
    return nsnull;
  return static_cast<nsSVGMarkerFrame *>
    (mMarkerStart->GetReferencedFrame(nsGkAtoms::svgMarkerFrame, nsnull));
}

nsSVGMarkerFrame *
nsSVGPathGeometryFrame::MarkerProperties::GetMarkerMidFrame()
{
  if (!mMarkerMid)
    return nsnull;
  return static_cast<nsSVGMarkerFrame *>
    (mMarkerMid->GetReferencedFrame(nsGkAtoms::svgMarkerFrame, nsnull));
}

nsSVGMarkerFrame *
nsSVGPathGeometryFrame::MarkerProperties::GetMarkerEndFrame()
{
  if (!mMarkerEnd)
    return nsnull;
  return static_cast<nsSVGMarkerFrame *>
    (mMarkerEnd->GetReferencedFrame(nsGkAtoms::svgMarkerFrame, nsnull));
}

void
nsSVGPathGeometryFrame::Render(nsRenderingContext *aContext)
{
  gfxContext *gfx = aContext->ThebesContext();

  PRUint16 renderMode = SVGAutoRenderState::GetRenderMode(aContext);

  switch (GetStyleSVG()->mShapeRendering) {
  case NS_STYLE_SHAPE_RENDERING_OPTIMIZESPEED:
  case NS_STYLE_SHAPE_RENDERING_CRISPEDGES:
    gfx->SetAntialiasMode(gfxContext::MODE_ALIASED);
    break;
  default:
    gfx->SetAntialiasMode(gfxContext::MODE_COVERAGE);
    break;
  }

  /* save/restore the state so we don't screw up the xform */
  gfx->Save();

  GeneratePath(gfx);

  if (renderMode != SVGAutoRenderState::NORMAL) {
    gfx->Restore();

    if (GetClipRule() == NS_STYLE_FILL_RULE_EVENODD)
      gfx->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
    else
      gfx->SetFillRule(gfxContext::FILL_RULE_WINDING);

    if (renderMode == SVGAutoRenderState::CLIP_MASK) {
      gfx->SetColor(gfxRGBA(1.0f, 1.0f, 1.0f, 1.0f));
      gfx->Fill();
      gfx->NewPath();
    }

    return;
  }

  if (SetupCairoFill(gfx)) {
    gfx->Fill();
  }

  if (SetupCairoStroke(gfx)) {
    gfx->Stroke();
  }

  gfx->NewPath();

  gfx->Restore();
}

void
nsSVGPathGeometryFrame::GeneratePath(gfxContext* aContext,
                                     const gfxMatrix *aOverrideTransform)
{
  gfxMatrix matrix;
  if (aOverrideTransform) {
    matrix = *aOverrideTransform;
  } else {
    matrix = GetCanvasTM();
  }

  if (matrix.IsSingular()) {
    aContext->IdentityMatrix();
    aContext->NewPath();
    return;
  }

  aContext->Multiply(matrix);

  // Hack to let SVGPathData::ConstructPath know if we have square caps:
  const nsStyleSVG* style = GetStyleSVG();
  if (style->mStrokeLinecap == NS_STYLE_STROKE_LINECAP_SQUARE) {
    aContext->SetLineCap(gfxContext::LINE_CAP_SQUARE);
  }

  aContext->NewPath();
  static_cast<nsSVGPathGeometryElement*>(mContent)->ConstructPath(aContext);
}
