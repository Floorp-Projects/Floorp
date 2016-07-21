/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGOuterSVGFrame.h"

// Keep others in (case-insensitive) order:
#include "nsDisplayList.h"
#include "nsIDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObjectLoadingContent.h"
#include "nsRenderingContext.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGForeignObjectFrame.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGViewElement.h"
#include "nsSubDocumentFrame.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
// Implementation helpers

void
nsSVGOuterSVGFrame::RegisterForeignObject(nsSVGForeignObjectFrame* aFrame)
{
  NS_ASSERTION(aFrame, "Who on earth is calling us?!");

  if (!mForeignObjectHash) {
    mForeignObjectHash = new nsTHashtable<nsPtrHashKey<nsSVGForeignObjectFrame> >();
  }

  NS_ASSERTION(!mForeignObjectHash->GetEntry(aFrame),
               "nsSVGForeignObjectFrame already registered!");

  mForeignObjectHash->PutEntry(aFrame);

  NS_ASSERTION(mForeignObjectHash->GetEntry(aFrame),
               "Failed to register nsSVGForeignObjectFrame!");
}

void
nsSVGOuterSVGFrame::UnregisterForeignObject(nsSVGForeignObjectFrame* aFrame)
{
  NS_ASSERTION(aFrame, "Who on earth is calling us?!");
  NS_ASSERTION(mForeignObjectHash && mForeignObjectHash->GetEntry(aFrame),
               "nsSVGForeignObjectFrame not in registry!");
  return mForeignObjectHash->RemoveEntry(aFrame);
}

//----------------------------------------------------------------------
// Implementation

nsContainerFrame*
NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGOuterSVGFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGOuterSVGFrame)

nsSVGOuterSVGFrame::nsSVGOuterSVGFrame(nsStyleContext* aContext)
    : nsSVGDisplayContainerFrame(aContext)
    , mFullZoom(aContext->PresContext()->GetFullZoom())
    , mViewportInitialized(false)
    , mIsRootContent(false)
{
  // Outer-<svg> has CSS layout, so remove this bit:
  RemoveStateBits(NS_FRAME_SVG_LAYOUT);
}

// helper
static inline bool
DependsOnIntrinsicSize(const nsIFrame* aEmbeddingFrame)
{
  const nsStylePosition *pos = aEmbeddingFrame->StylePosition();
  const nsStyleCoord &width = pos->mWidth;
  const nsStyleCoord &height = pos->mHeight;

  // XXX it would be nice to know if the size of aEmbeddingFrame's containing
  // block depends on aEmbeddingFrame, then we'd know if we can return false
  // for eStyleUnit_Percent too.
  return !width.ConvertsToLength() ||
         !height.ConvertsToLength();
}

void
nsSVGOuterSVGFrame::Init(nsIContent*       aContent,
                         nsContainerFrame* aParent,
                         nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::svg),
               "Content is not an SVG 'svg' element!");

  AddStateBits(NS_STATE_IS_OUTER_SVG |
               NS_FRAME_FONT_INFLATION_CONTAINER |
               NS_FRAME_FONT_INFLATION_FLOW_ROOT);

  // Check for conditional processing attributes here rather than in
  // nsCSSFrameConstructor::FindSVGData because we want to avoid
  // simply giving failing outer <svg> elements an nsSVGContainerFrame.
  // We don't create other SVG frames if PassesConditionalProcessingTests
  // returns false, but since we do create nsSVGOuterSVGFrame frames we
  // prevent them from painting by [ab]use NS_FRAME_IS_NONDISPLAY. The
  // frame will be recreated via an nsChangeHint_ReconstructFrame restyle if
  // the value returned by PassesConditionalProcessingTests changes.
  SVGSVGElement *svg = static_cast<SVGSVGElement*>(aContent);
  if (!svg->PassesConditionalProcessingTests()) {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

  nsSVGDisplayContainerFrame::Init(aContent, aParent, aPrevInFlow);

  nsIDocument* doc = mContent->GetUncomposedDoc();
  if (doc) {
    // we only care about our content's zoom and pan values if it's the root element
    if (doc->GetRootElement() == mContent) {
      mIsRootContent = true;

      nsIFrame* embeddingFrame;
      if (IsRootOfReplacedElementSubDoc(&embeddingFrame) && embeddingFrame) {
        if (MOZ_UNLIKELY(!embeddingFrame->HasAllStateBits(NS_FRAME_IS_DIRTY)) &&
            DependsOnIntrinsicSize(embeddingFrame)) {
          // Looks like this document is loading after the embedding element
          // has had its first reflow, and that its size depends on our
          // intrinsic size.  We need it to resize itself to use our (now
          // available) intrinsic size:
          embeddingFrame->PresContext()->PresShell()->
            FrameNeedsReflow(embeddingFrame, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
        }
      }
    }
  }
}

//----------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(nsSVGOuterSVGFrame)
  NS_QUERYFRAME_ENTRY(nsISVGSVGFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGDisplayContainerFrame)

//----------------------------------------------------------------------
// nsIFrame methods
//----------------------------------------------------------------------
// reflowing

/* virtual */ nscoord
nsSVGOuterSVGFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  result = nscoord(0);

  return result;
}

/* virtual */ nscoord
nsSVGOuterSVGFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  SVGSVGElement *svg = static_cast<SVGSVGElement*>(mContent);
  WritingMode wm = GetWritingMode();
  const nsSVGLength2& isize = wm.IsVertical()
    ? svg->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT]
    : svg->mLengthAttributes[SVGSVGElement::ATTR_WIDTH];

  if (isize.IsPercentage()) {
    // It looks like our containing block's isize may depend on our isize. In
    // that case our behavior is undefined according to CSS 2.1 section 10.3.2.
    // As a last resort, we'll fall back to returning zero.
    result = nscoord(0);

    // Returning zero may be unhelpful, however, as it leads to unexpected
    // disappearance of %-sized SVGs in orthogonal contexts, where our
    // containing block wants to shrink-wrap. So let's look for an ancestor
    // with non-zero size in this dimension, and use that as a (somewhat
    // arbitrary) result instead.
    nsIFrame *parent = GetParent();
    while (parent) {
      nscoord parentISize = parent->GetLogicalSize(wm).ISize(wm);
      if (parentISize > 0 && parentISize != NS_UNCONSTRAINEDSIZE) {
        result = parentISize;
        break;
      }
      parent = parent->GetParent();
    }
  } else {
    result = nsPresContext::CSSPixelsToAppUnits(isize.GetAnimValue(svg));
    if (result < 0) {
      result = nscoord(0);
    }
  }

  return result;
}

/* virtual */ IntrinsicSize
nsSVGOuterSVGFrame::GetIntrinsicSize()
{
  // XXXjwatt Note that here we want to return the CSS width/height if they're
  // specified and we're embedded inside an nsIObjectLoadingContent.

  IntrinsicSize intrinsicSize;

  SVGSVGElement *content = static_cast<SVGSVGElement*>(mContent);
  nsSVGLength2 &width  = content->mLengthAttributes[SVGSVGElement::ATTR_WIDTH];
  nsSVGLength2 &height = content->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT];

  if (!width.IsPercentage()) {
    nscoord val = nsPresContext::CSSPixelsToAppUnits(width.GetAnimValue(content));
    if (val < 0) val = 0;
    intrinsicSize.width.SetCoordValue(val);
  }

  if (!height.IsPercentage()) {
    nscoord val = nsPresContext::CSSPixelsToAppUnits(height.GetAnimValue(content));
    if (val < 0) val = 0;
    intrinsicSize.height.SetCoordValue(val);
  }

  return intrinsicSize;
}

/* virtual */ nsSize
nsSVGOuterSVGFrame::GetIntrinsicRatio()
{
  // We only have an intrinsic size/ratio if our width and height attributes
  // are both specified and set to non-percentage values, or we have a viewBox
  // rect: http://www.w3.org/TR/SVGMobile12/coords.html#IntrinsicSizing

  SVGSVGElement *content = static_cast<SVGSVGElement*>(mContent);
  nsSVGLength2 &width  = content->mLengthAttributes[SVGSVGElement::ATTR_WIDTH];
  nsSVGLength2 &height = content->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT];

  if (!width.IsPercentage() && !height.IsPercentage()) {
    nsSize ratio(NSToCoordRoundWithClamp(width.GetAnimValue(content)),
                 NSToCoordRoundWithClamp(height.GetAnimValue(content)));
    if (ratio.width < 0) {
      ratio.width = 0;
    }
    if (ratio.height < 0) {
      ratio.height = 0;
    }
    return ratio;
  }

  SVGViewElement* viewElement = content->GetCurrentViewElement();
  const nsSVGViewBoxRect* viewbox = nullptr;

  // The logic here should match HasViewBox().
  if (viewElement && viewElement->mViewBox.HasRect()) {
    viewbox = &viewElement->mViewBox.GetAnimValue();
  } else if (content->mViewBox.HasRect()) {
    viewbox = &content->mViewBox.GetAnimValue();
  }

  if (viewbox) {
    float viewBoxWidth = viewbox->width;
    float viewBoxHeight = viewbox->height;

    if (viewBoxWidth < 0.0f) {
      viewBoxWidth = 0.0f;
    }
    if (viewBoxHeight < 0.0f) {
      viewBoxHeight = 0.0f;
    }
    return nsSize(NSToCoordRoundWithClamp(viewBoxWidth),
                  NSToCoordRoundWithClamp(viewBoxHeight));
  }

  return nsSVGDisplayContainerFrame::GetIntrinsicRatio();
}

/* virtual */
LogicalSize
nsSVGOuterSVGFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                                WritingMode aWM,
                                const LogicalSize& aCBSize,
                                nscoord aAvailableISize,
                                const LogicalSize& aMargin,
                                const LogicalSize& aBorder,
                                const LogicalSize& aPadding,
                                ComputeSizeFlags aFlags)
{
  if (IsRootOfImage() || IsRootOfReplacedElementSubDoc()) {
    // The embedding element has sized itself using the CSS replaced element
    // sizing rules, using our intrinsic dimensions as necessary. The SVG spec
    // says that the width and height of embedded SVG is overridden by the
    // width and height of the embedding element, so we just need to size to
    // the viewport that the embedding element has established for us.
    return aCBSize;
  }

  LogicalSize cbSize = aCBSize;
  IntrinsicSize intrinsicSize = GetIntrinsicSize();

  if (!mContent->GetParent()) {
    // We're the root of the outermost browsing context, so we need to scale
    // cbSize by the full-zoom so that SVGs with percentage width/height zoom:

    NS_ASSERTION(aCBSize.ISize(aWM) != NS_AUTOHEIGHT &&
                 aCBSize.BSize(aWM) != NS_AUTOHEIGHT,
                 "root should not have auto-width/height containing block");
    cbSize.ISize(aWM) *= PresContext()->GetFullZoom();
    cbSize.BSize(aWM) *= PresContext()->GetFullZoom();

    // We also need to honour the width and height attributes' default values
    // of 100% when we're the root of a browsing context.  (GetIntrinsicSize()
    // doesn't report these since there's no such thing as a percentage
    // intrinsic size.  Also note that explicit percentage values are mapped
    // into style, so the following isn't for them.)

    SVGSVGElement* content = static_cast<SVGSVGElement*>(mContent);

    nsSVGLength2 &width =
      content->mLengthAttributes[SVGSVGElement::ATTR_WIDTH];
    if (width.IsPercentage()) {
      MOZ_ASSERT(intrinsicSize.width.GetUnit() == eStyleUnit_None,
                 "GetIntrinsicSize should have reported no intrinsic width");
      float val = width.GetAnimValInSpecifiedUnits() / 100.0f;
      if (val < 0.0f) val = 0.0f;
      intrinsicSize.width.SetCoordValue(val * cbSize.Width(aWM));
    }

    nsSVGLength2 &height =
      content->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT];
    NS_ASSERTION(aCBSize.BSize(aWM) != NS_AUTOHEIGHT,
                 "root should not have auto-height containing block");
    if (height.IsPercentage()) {
      MOZ_ASSERT(intrinsicSize.height.GetUnit() == eStyleUnit_None,
                 "GetIntrinsicSize should have reported no intrinsic height");
      float val = height.GetAnimValInSpecifiedUnits() / 100.0f;
      if (val < 0.0f) val = 0.0f;
      intrinsicSize.height.SetCoordValue(val * cbSize.Height(aWM));
    }
    MOZ_ASSERT(intrinsicSize.height.GetUnit() == eStyleUnit_Coord &&
               intrinsicSize.width.GetUnit() == eStyleUnit_Coord,
               "We should have just handled the only situation where"
               "we lack an intrinsic height or width.");
  }

  return nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(aWM,
                            aRenderingContext, this,
                            intrinsicSize, GetIntrinsicRatio(),
                            cbSize,
                            aMargin,
                            aBorder,
                            aPadding);
}

void
nsSVGOuterSVGFrame::Reflow(nsPresContext*           aPresContext,
                           ReflowOutput&     aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsSVGOuterSVGFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsSVGOuterSVGFrame::Reflow: availSize=%d,%d",
                  aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus = NS_FRAME_COMPLETE;

  aDesiredSize.Width()  = aReflowInput.ComputedWidth() +
                          aReflowInput.ComputedPhysicalBorderPadding().LeftRight();
  aDesiredSize.Height() = aReflowInput.ComputedHeight() +
                          aReflowInput.ComputedPhysicalBorderPadding().TopBottom();

  NS_ASSERTION(!GetPrevInFlow(), "SVG can't currently be broken across pages.");

  SVGSVGElement *svgElem = static_cast<SVGSVGElement*>(mContent);

  nsSVGOuterSVGAnonChildFrame *anonKid =
    static_cast<nsSVGOuterSVGAnonChildFrame*>(PrincipalChildList().FirstChild());

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Initialize
    svgElem->UpdateHasChildrenOnlyTransform();
  }

  // If our SVG viewport has changed, update our content and notify.
  // http://www.w3.org/TR/SVG11/coords.html#ViewportSpace

  svgFloatSize newViewportSize(
    nsPresContext::AppUnitsToFloatCSSPixels(aReflowInput.ComputedWidth()),
    nsPresContext::AppUnitsToFloatCSSPixels(aReflowInput.ComputedHeight()));

  svgFloatSize oldViewportSize = svgElem->GetViewportSize();

  uint32_t changeBits = 0;
  if (newViewportSize != oldViewportSize) {
    // When our viewport size changes, we may need to update the overflow rects
    // of our child frames. This is the case if:
    //
    //  * We have a real/synthetic viewBox (a children-only transform), since
    //    the viewBox transform will change as the viewport dimensions change.
    //
    //  * We do not have a real/synthetic viewBox, but the last time we
    //    reflowed (or the last time UpdateOverflow() was called) we did.
    //
    // We only handle the former case here, in which case we mark all our child
    // frames as dirty so that we reflow them below and update their overflow
    // rects.
    //
    // In the latter case, updating of overflow rects is handled for removal of
    // real viewBox (the viewBox attribute) in AttributeChanged. Synthetic
    // viewBox "removal" (e.g. a document references the same SVG via both an
    // <svg:image> and then as a CSS background image (a synthetic viewBox is
    // used when painting the former, but not when painting the latter)) is
    // handled in SVGSVGElement::FlushImageTransformInvalidation.
    //
    if (svgElem->HasViewBoxOrSyntheticViewBox()) {
      nsIFrame* anonChild = PrincipalChildList().FirstChild();
      anonChild->AddStateBits(NS_FRAME_IS_DIRTY);
      for (nsIFrame* child : anonChild->PrincipalChildList()) {
        child->AddStateBits(NS_FRAME_IS_DIRTY);
      }
    }
    changeBits |= COORD_CONTEXT_CHANGED;
    svgElem->SetViewportSize(newViewportSize);
  }
  if (mFullZoom != PresContext()->GetFullZoom()) {
    changeBits |= FULL_ZOOM_CHANGED;
    mFullZoom = PresContext()->GetFullZoom();
  }
  if (changeBits) {
    NotifyViewportOrTransformChanged(changeBits);
  }
  mViewportInitialized = true;

  // Now that we've marked the necessary children as dirty, call
  // ReflowSVG() or ReflowSVGNonDisplayText() on them, depending
  // on whether we are non-display.
  mCallingReflowSVG = true;
  if (GetStateBits() & NS_FRAME_IS_NONDISPLAY) {
    ReflowSVGNonDisplayText(this);
  } else {
    // Update the mRects and visual overflow rects of all our descendants,
    // including our anonymous wrapper kid:
    anonKid->AddStateBits(mState & NS_FRAME_IS_DIRTY);
    anonKid->ReflowSVG();
    MOZ_ASSERT(!anonKid->GetNextSibling(),
               "We should have one anonymous child frame wrapping our real "
               "children");
  }
  mCallingReflowSVG = false;

  // Set our anonymous kid's offset from our border box:
  anonKid->SetPosition(GetContentRectRelativeToSelf().TopLeft());

  // Including our size in our overflow rects regardless of the value of
  // 'background', 'border', etc. makes sure that we usually (when we clip to
  // our content area) don't have to keep changing our overflow rects as our
  // descendants move about (see perf comment below). Including our size in our
  // scrollable overflow rect also makes sure that we scroll if we're too big
  // for our viewport.
  //
  // <svg> never allows scrolling to anything outside its mRect (only panning),
  // so we must always keep our scrollable overflow set to our size.
  //
  // With regards to visual overflow, we always clip root-<svg> (see our
  // BuildDisplayList method) regardless of the value of the 'overflow'
  // property since that is per-spec, even for the initial 'visible' value. For
  // that reason there's no point in adding descendant visual overflow to our
  // own when this frame is for a root-<svg>. That said, there's also a very
  // good performance reason for us wanting to avoid doing so. If we did, then
  // the frame's overflow would often change as descendants that are partially
  // or fully outside its rect moved (think animation on/off screen), and that
  // would cause us to do a full NS_FRAME_IS_DIRTY reflow and repaint of the
  // entire document tree each such move (see bug 875175).
  //
  // So it's only non-root outer-<svg> that has the visual overflow of its
  // descendants added to its own. (Note that the default user-agent style
  // sheet makes 'hidden' the default value for :not(root(svg)), so usually
  // FinishAndStoreOverflow will still clip this back to the frame's rect.)
  //
  // WARNING!! Keep UpdateBounds below in sync with whatever we do for our
  // overflow rects here! (Again, see bug 875175.)
  //
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  if (!mIsRootContent) {
    aDesiredSize.mOverflowAreas.VisualOverflow().UnionRect(
      aDesiredSize.mOverflowAreas.VisualOverflow(),
      anonKid->GetVisualOverflowRect() + anonKid->GetPosition());
  }
  FinishAndStoreOverflow(&aDesiredSize);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsSVGOuterSVGFrame::Reflow: size=%d,%d",
                  aDesiredSize.Width(), aDesiredSize.Height()));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void
nsSVGOuterSVGFrame::DidReflow(nsPresContext*   aPresContext,
                              const ReflowInput*  aReflowInput,
                              nsDidReflowStatus aStatus)
{
  nsSVGDisplayContainerFrame::DidReflow(aPresContext,aReflowInput,aStatus);

  // Make sure elements styled by :hover get updated if script/animation moves
  // them under or out from under the pointer:
  PresContext()->PresShell()->SynthesizeMouseMove(false);
}

/* virtual */ void
nsSVGOuterSVGFrame::UnionChildOverflow(nsOverflowAreas& aOverflowAreas)
{
  // See the comments in Reflow above.

  // WARNING!! Keep this in sync with Reflow above!

  if (!mIsRootContent) {
    nsIFrame *anonKid = PrincipalChildList().FirstChild();
    aOverflowAreas.VisualOverflow().UnionRect(
      aOverflowAreas.VisualOverflow(),
      anonKid->GetVisualOverflowRect() + anonKid->GetPosition());
  }
}


//----------------------------------------------------------------------
// container methods

/**
 * Used to paint/hit-test SVG when SVG display lists are disabled.
 */
class nsDisplayOuterSVG : public nsDisplayItem {
public:
  nsDisplayOuterSVG(nsDisplayListBuilder* aBuilder,
                    nsSVGOuterSVGFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayOuterSVG);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOuterSVG() {
    MOZ_COUNT_DTOR(nsDisplayOuterSVG);
  }
#endif

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override;

  nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayItemGenericImageGeometry(this, aBuilder);
  }

  NS_DISPLAY_DECL_NAME("SVGOuterSVG", TYPE_SVG_OUTER_SVG)
};

void
nsDisplayOuterSVG::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                           HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsSVGOuterSVGFrame *outerSVGFrame = static_cast<nsSVGOuterSVGFrame*>(mFrame);

  nsPoint refFrameToContentBox =
    ToReferenceFrame() + outerSVGFrame->GetContentRectRelativeToSelf().TopLeft();

  nsPoint pointRelativeToContentBox =
    nsPoint(aRect.x + aRect.width / 2, aRect.y + aRect.height / 2) -
      refFrameToContentBox;

  gfxPoint svgViewportRelativePoint =
    gfxPoint(pointRelativeToContentBox.x, pointRelativeToContentBox.y) /
      outerSVGFrame->PresContext()->AppUnitsPerCSSPixel();

  nsSVGOuterSVGAnonChildFrame *anonKid =
    static_cast<nsSVGOuterSVGAnonChildFrame*>(
      outerSVGFrame->PrincipalChildList().FirstChild());

  nsIFrame* frame =
    nsSVGUtils::HitTestChildren(anonKid, svgViewportRelativePoint);
  if (frame) {
    aOutFrames->AppendElement(frame);
  }
}

void
nsDisplayOuterSVG::Paint(nsDisplayListBuilder* aBuilder,
                         nsRenderingContext* aContext)
{
#if defined(DEBUG) && defined(SVG_DEBUG_PAINT_TIMING)
  PRTime start = PR_Now();
#endif

  // Create an SVGAutoRenderState so we can call SetPaintingToWindow on it.
  SVGAutoRenderState state(aContext->GetDrawTarget());

  if (aBuilder->IsPaintingToWindow()) {
    state.SetPaintingToWindow(true);
  }

  nsRect viewportRect =
    mFrame->GetContentRectRelativeToSelf() + ToReferenceFrame();

  nsRect clipRect = mVisibleRect.Intersect(viewportRect);

  uint32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  nsIntRect contentAreaDirtyRect =
    (clipRect - viewportRect.TopLeft()).
      ToOutsidePixels(appUnitsPerDevPixel);

  gfxPoint devPixelOffset =
    nsLayoutUtils::PointToGfxPoint(viewportRect.TopLeft(), appUnitsPerDevPixel);

  aContext->ThebesContext()->Save();
  // We include the offset of our frame and a scale from device pixels to user
  // units (i.e. CSS px) in the matrix that we pass to our children):
  gfxMatrix tm = nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(mFrame) *
                   gfxMatrix::Translation(devPixelOffset);
  DrawResult result =
    nsSVGUtils::PaintFrameWithEffects(mFrame, *aContext->ThebesContext(), tm,
                                      &contentAreaDirtyRect);
  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
  aContext->ThebesContext()->Restore();

#if defined(DEBUG) && defined(SVG_DEBUG_PAINT_TIMING)
  PRTime end = PR_Now();
  printf("SVG Paint Timing: %f ms\n", (end-start)/1000.0);
#endif
}

nsRegion
nsSVGOuterSVGFrame::FindInvalidatedForeignObjectFrameChildren(nsIFrame* aFrame)
{
  nsRegion result;
  if (mForeignObjectHash && mForeignObjectHash->Count()) {
    for (auto it = mForeignObjectHash->Iter(); !it.Done(); it.Next()) {
      result.Or(result, it.Get()->GetKey()->GetInvalidRegion());
    }
  }
  return result;
}

void
nsDisplayOuterSVG::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                             const nsDisplayItemGeometry* aGeometry,
                                             nsRegion* aInvalidRegion)
{
  nsSVGOuterSVGFrame *frame = static_cast<nsSVGOuterSVGFrame*>(mFrame);
  frame->InvalidateSVG(frame->FindInvalidatedForeignObjectFrameChildren(frame));

  nsRegion result = frame->GetInvalidRegion();
  result.MoveBy(ToReferenceFrame());
  frame->ClearInvalidRegion();

  nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
  aInvalidRegion->Or(*aInvalidRegion, result);

  auto geometry =
    static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
    geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }
}

nsresult
nsSVGOuterSVGFrame::AttributeChanged(int32_t  aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     int32_t  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      !(GetStateBits() & (NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_NONDISPLAY))) {
    if (aAttribute == nsGkAtoms::viewBox ||
        aAttribute == nsGkAtoms::preserveAspectRatio ||
        aAttribute == nsGkAtoms::transform) {

      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nullptr;

      nsSVGUtils::NotifyChildrenOfSVGChange(PrincipalChildList().FirstChild(),
                aAttribute == nsGkAtoms::viewBox ?
                  TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED : TRANSFORM_CHANGED);

      if (aAttribute != nsGkAtoms::transform) {
        static_cast<SVGSVGElement*>(mContent)->ChildrenOnlyTransformChanged();
      }

    } else if (aAttribute == nsGkAtoms::width ||
               aAttribute == nsGkAtoms::height) {

      // Don't call ChildrenOnlyTransformChanged() here, since we call it
      // under Reflow if the width/height actually changed.

      nsIFrame* embeddingFrame;
      if (IsRootOfReplacedElementSubDoc(&embeddingFrame) && embeddingFrame) {
        if (DependsOnIntrinsicSize(embeddingFrame)) {
          // Tell embeddingFrame's presShell it needs to be reflowed (which takes
          // care of reflowing us too).
          embeddingFrame->PresContext()->PresShell()->
            FrameNeedsReflow(embeddingFrame, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
        }
        // else our width and height is overridden - don't reflow anything
      } else {
        // We are not embedded by reference, so our 'width' and 'height'
        // attributes are not overridden - we need to reflow.
        PresContext()->PresShell()->
          FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
      }
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// painting

void
nsSVGOuterSVGFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  if (GetStateBits() & NS_FRAME_IS_NONDISPLAY) {
    return;
  }

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  // Per-spec, we always clip root-<svg> even when 'overflow' has its initial
  // value of 'visible'. See also the "visual overflow" comments in Reflow.
  DisplayListClipState::AutoSaveRestore autoSR(aBuilder);
  if (mIsRootContent ||
      StyleDisplay()->IsScrollableOverflow()) {
    autoSR.ClipContainingBlockDescendantsToContentBox(aBuilder, this);
  }

  if ((aBuilder->IsForEventDelivery() &&
       NS_SVGDisplayListHitTestingEnabled()) ||
      (!aBuilder->IsForEventDelivery() &&
       NS_SVGDisplayListPaintingEnabled())) {
    nsDisplayList *contentList = aLists.Content();
    nsDisplayListSet set(contentList, contentList, contentList,
                         contentList, contentList, contentList);
    BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, set);
  } else if (IsVisibleForPainting(aBuilder) || !aBuilder->IsForPainting()) {
    aLists.Content()->AppendNewToTop(
      new (aBuilder) nsDisplayOuterSVG(aBuilder, this));
  }
}

nsSplittableType
nsSVGOuterSVGFrame::GetSplittableType() const
{
  return NS_FRAME_NOT_SPLITTABLE;
}

nsIAtom *
nsSVGOuterSVGFrame::GetType() const
{
  return nsGkAtoms::svgOuterSVGFrame;
}

//----------------------------------------------------------------------
// nsISVGSVGFrame methods:

void
nsSVGOuterSVGFrame::NotifyViewportOrTransformChanged(uint32_t aFlags)
{
  MOZ_ASSERT(aFlags &&
             !(aFlags & ~(COORD_CONTEXT_CHANGED | TRANSFORM_CHANGED |
                          FULL_ZOOM_CHANGED)),
             "Unexpected aFlags value");

  // No point in doing anything when were not init'ed yet:
  if (!mViewportInitialized) {
    return;
  }

  SVGSVGElement *content = static_cast<SVGSVGElement*>(mContent);

  if (aFlags & COORD_CONTEXT_CHANGED) {
    if (content->HasViewBoxRect()) {
      // Percentage lengths on children resolve against the viewBox rect so we
      // don't need to notify them of the viewport change, but the viewBox
      // transform will have changed, so we need to notify them of that instead.
      aFlags = TRANSFORM_CHANGED;
    }
    else if (content->ShouldSynthesizeViewBox()) {
      // In the case of a synthesized viewBox, the synthetic viewBox's rect
      // changes as the viewport changes. As a result we need to maintain the
      // COORD_CONTEXT_CHANGED flag.
      aFlags |= TRANSFORM_CHANGED;
    }
    else if (mCanvasTM && mCanvasTM->IsSingular()) {
      // A width/height of zero will result in us having a singular mCanvasTM
      // even when we don't have a viewBox. So we also want to recompute our
      // mCanvasTM for this width/height change even though we don't have a
      // viewBox.
      aFlags |= TRANSFORM_CHANGED;
    }
  }

  bool haveNonFulLZoomTransformChange = (aFlags & TRANSFORM_CHANGED);

  if (aFlags & FULL_ZOOM_CHANGED) {
    // Convert FULL_ZOOM_CHANGED to TRANSFORM_CHANGED:
    aFlags = (aFlags & ~FULL_ZOOM_CHANGED) | TRANSFORM_CHANGED;
  }

  if (aFlags & TRANSFORM_CHANGED) {
    // Make sure our canvas transform matrix gets (lazily) recalculated:
    mCanvasTM = nullptr;

    if (haveNonFulLZoomTransformChange &&
        !(mState & NS_FRAME_IS_NONDISPLAY)) {
      uint32_t flags = (mState & NS_FRAME_IN_REFLOW) ?
                         SVGSVGElement::eDuringReflow : 0;
      content->ChildrenOnlyTransformChanged(flags);
    }
  }

  nsSVGUtils::NotifyChildrenOfSVGChange(PrincipalChildList().FirstChild(), aFlags);
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods:

DrawResult
nsSVGOuterSVGFrame::PaintSVG(gfxContext& aContext,
                             const gfxMatrix& aTransform,
                             const nsIntRect* aDirtyRect)
{
  NS_ASSERTION(PrincipalChildList().FirstChild()->GetType() ==
                 nsGkAtoms::svgOuterSVGAnonChildFrame &&
               !PrincipalChildList().FirstChild()->GetNextSibling(),
               "We should have a single, anonymous, child");
  nsSVGOuterSVGAnonChildFrame *anonKid =
    static_cast<nsSVGOuterSVGAnonChildFrame*>(PrincipalChildList().FirstChild());
  return anonKid->PaintSVG(aContext, aTransform, aDirtyRect);
}

SVGBBox
nsSVGOuterSVGFrame::GetBBoxContribution(const gfx::Matrix &aToBBoxUserspace,
                                        uint32_t aFlags)
{
  NS_ASSERTION(PrincipalChildList().FirstChild()->GetType() ==
                 nsGkAtoms::svgOuterSVGAnonChildFrame &&
               !PrincipalChildList().FirstChild()->GetNextSibling(),
               "We should have a single, anonymous, child");
  // We must defer to our child so that we don't include our
  // content->PrependLocalTransformsTo() transforms.
  nsSVGOuterSVGAnonChildFrame *anonKid =
    static_cast<nsSVGOuterSVGAnonChildFrame*>(PrincipalChildList().FirstChild());
  return anonKid->GetBBoxContribution(aToBBoxUserspace, aFlags);
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGOuterSVGFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    SVGSVGElement *content = static_cast<SVGSVGElement*>(mContent);

    float devPxPerCSSPx =
      1.0f / PresContext()->AppUnitsToFloatCSSPixels(
                                PresContext()->AppUnitsPerDevPixel());

    gfxMatrix tm = content->PrependLocalTransformsTo(
                     gfxMatrix::Scaling(devPxPerCSSPx, devPxPerCSSPx));
    mCanvasTM = new gfxMatrix(tm);
  }
  return *mCanvasTM;
}

//----------------------------------------------------------------------
// Implementation helpers

bool
nsSVGOuterSVGFrame::IsRootOfReplacedElementSubDoc(nsIFrame **aEmbeddingFrame)
{
  if (!mContent->GetParent()) {
    // Our content is the document element
    nsCOMPtr<nsIDocShell> docShell = PresContext()->GetDocShell();
    nsCOMPtr<nsPIDOMWindowOuter> window;
    if (docShell) {
      window = docShell->GetWindow();
    }

    if (window) {
      nsCOMPtr<nsIDOMElement> frameElement = window->GetFrameElement();
      nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(frameElement);
      if (olc) {
        // Our document is inside an HTML 'object', 'embed' or 'applet' element
        if (aEmbeddingFrame) {
          nsCOMPtr<nsIContent> element = do_QueryInterface(frameElement);
          *aEmbeddingFrame = element->GetPrimaryFrame();
          NS_ASSERTION(*aEmbeddingFrame, "Yikes, no embedding frame!");
        }
        return true;
      }
    }
  }
  if (aEmbeddingFrame) {
    *aEmbeddingFrame = nullptr;
  }
  return false;
}

bool
nsSVGOuterSVGFrame::IsRootOfImage()
{
  if (!mContent->GetParent()) {
    // Our content is the document element
    nsIDocument* doc = mContent->GetUncomposedDoc();
    if (doc && doc->IsBeingUsedAsImage()) {
      // Our document is being used as an image
      return true;
    }
  }

  return false;
}

bool
nsSVGOuterSVGFrame::VerticalScrollbarNotNeeded() const
{
  nsSVGLength2 &height = static_cast<SVGSVGElement*>(mContent)->
                           mLengthAttributes[SVGSVGElement::ATTR_HEIGHT];
  return height.IsPercentage() && height.GetBaseValInSpecifiedUnits() <= 100;
}


//----------------------------------------------------------------------
// Implementation of nsSVGOuterSVGAnonChildFrame

nsContainerFrame*
NS_NewSVGOuterSVGAnonChildFrame(nsIPresShell* aPresShell,
                                nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGOuterSVGAnonChildFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGOuterSVGAnonChildFrame)

#ifdef DEBUG
void
nsSVGOuterSVGAnonChildFrame::Init(nsIContent*       aContent,
                                  nsContainerFrame* aParent,
                                  nsIFrame*         aPrevInFlow)
{
  MOZ_ASSERT(aParent->GetType() == nsGkAtoms::svgOuterSVGFrame,
             "Unexpected parent");
  nsSVGDisplayContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif

nsIAtom *
nsSVGOuterSVGAnonChildFrame::GetType() const
{
  return nsGkAtoms::svgOuterSVGAnonChildFrame;
}

bool
nsSVGOuterSVGAnonChildFrame::HasChildrenOnlyTransform(gfx::Matrix *aTransform) const
{
  // We must claim our nsSVGOuterSVGFrame's children-only transforms as our own
  // so that the children we are used to wrap are transformed properly.

  SVGSVGElement *content = static_cast<SVGSVGElement*>(mContent);

  bool hasTransform = content->HasChildrenOnlyTransform();

  if (hasTransform && aTransform) {
    // Outer-<svg> doesn't use x/y, so we can pass eChildToUserSpace here.
    gfxMatrix identity;
    *aTransform = gfx::ToMatrix(
      content->PrependLocalTransformsTo(identity, eChildToUserSpace));
  }

  return hasTransform;
}
