/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGOuterSVGFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "nsDisplayList.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObjectLoadingContent.h"
#include "nsSubDocumentFrame.h"
#include "mozilla/PresShell.h"
#include "mozilla/SVGForeignObjectFrame.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGViewElement.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

/**
 * Recursively checks if any atom in the parameter pack is equal to |aString|.
 */
template <typename... Atoms>
bool IsAnyAtomEqual(const nsAString& aString, nsAtom* aFirst, Atoms... aArgs) {
  return aFirst->Equals(aString) || IsAnyAtomEqual(aString, aArgs...);
}
bool IsAnyAtomEqual(const nsAString& aString) { return false; }

namespace mozilla {

//----------------------------------------------------------------------
// Implementation helpers

void SVGOuterSVGFrame::RegisterForeignObject(SVGForeignObjectFrame* aFrame) {
  NS_ASSERTION(aFrame, "Who on earth is calling us?!");

  if (!mForeignObjectHash) {
    mForeignObjectHash =
        MakeUnique<nsTHashtable<nsPtrHashKey<SVGForeignObjectFrame>>>();
  }

  NS_ASSERTION(!mForeignObjectHash->GetEntry(aFrame),
               "SVGForeignObjectFrame already registered!");

  mForeignObjectHash->PutEntry(aFrame);

  NS_ASSERTION(mForeignObjectHash->GetEntry(aFrame),
               "Failed to register SVGForeignObjectFrame!");
}

void SVGOuterSVGFrame::UnregisterForeignObject(SVGForeignObjectFrame* aFrame) {
  NS_ASSERTION(aFrame, "Who on earth is calling us?!");
  NS_ASSERTION(mForeignObjectHash && mForeignObjectHash->GetEntry(aFrame),
               "SVGForeignObjectFrame not in registry!");
  return mForeignObjectHash->RemoveEntry(aFrame);
}

}  // namespace mozilla

//----------------------------------------------------------------------
// Implementation

nsContainerFrame* NS_NewSVGOuterSVGFrame(mozilla::PresShell* aPresShell,
                                         mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGOuterSVGFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGOuterSVGFrame)

SVGOuterSVGFrame::SVGOuterSVGFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext)
    : SVGDisplayContainerFrame(aStyle, aPresContext, kClassID),
      mCallingReflowSVG(false),
      mFullZoom(PresContext()->GetFullZoom()),
      mViewportInitialized(false),
      mIsRootContent(false) {
  // Outer-<svg> has CSS layout, so remove this bit:
  RemoveStateBits(NS_FRAME_SVG_LAYOUT);
}

// helper
static inline bool DependsOnIntrinsicSize(const nsIFrame* aEmbeddingFrame) {
  const nsStylePosition* pos = aEmbeddingFrame->StylePosition();

  // XXX it would be nice to know if the size of aEmbeddingFrame's containing
  // block depends on aEmbeddingFrame, then we'd know if we can return false
  // for eStyleUnit_Percent too.
  return !pos->mWidth.ConvertsToLength() || !pos->mHeight.ConvertsToLength();
}

// The CSS Containment spec says that size-contained replaced elements must be
// treated as having an intrinsic width and height of 0.  That's applicable to
// outer SVG frames, unless they're the outermost element (in which case
// they're not really "replaced", and there's no outer context to contain sizes
// from leaking into). Hence, we check for a parent element before we bother
// testing for 'contain:size'.
static inline bool IsReplacedAndContainSize(const SVGOuterSVGFrame* aFrame) {
  return aFrame->GetContent()->GetParent() &&
         aFrame->StyleDisplay()->IsContainSize();
}

void SVGOuterSVGFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::svg),
               "Content is not an SVG 'svg' element!");

  AddStateBits(NS_FRAME_REFLOW_ROOT | NS_FRAME_FONT_INFLATION_CONTAINER |
               NS_FRAME_FONT_INFLATION_FLOW_ROOT);

  // Check for conditional processing attributes here rather than in
  // nsCSSFrameConstructor::FindSVGData because we want to avoid
  // simply giving failing outer <svg> elements an SVGContainerFrame.
  // We don't create other SVG frames if PassesConditionalProcessingTests
  // returns false, but since we do create SVGOuterSVGFrame frames we
  // prevent them from painting by [ab]use NS_FRAME_IS_NONDISPLAY. The
  // frame will be recreated via an nsChangeHint_ReconstructFrame restyle if
  // the value returned by PassesConditionalProcessingTests changes.
  SVGSVGElement* svg = static_cast<SVGSVGElement*>(aContent);
  if (!svg->PassesConditionalProcessingTests()) {
    AddStateBits(NS_FRAME_IS_NONDISPLAY);
  }

  SVGDisplayContainerFrame::Init(aContent, aParent, aPrevInFlow);

  Document* doc = mContent->GetUncomposedDoc();
  if (doc) {
    // we only care about our content's zoom and pan values if it's the root
    // element
    if (doc->GetRootElement() == mContent) {
      mIsRootContent = true;

      nsIFrame* embeddingFrame;
      if (IsRootOfReplacedElementSubDoc(&embeddingFrame) && embeddingFrame) {
        if (MOZ_UNLIKELY(!embeddingFrame->HasAllStateBits(NS_FRAME_IS_DIRTY))) {
          bool dependsOnIntrinsicSize = DependsOnIntrinsicSize(embeddingFrame);
          if (dependsOnIntrinsicSize ||
              embeddingFrame->StylePosition()->mObjectFit !=
                  StyleObjectFit::Fill) {
            // Looks like this document is loading after the embedding element
            // has had its first reflow, and it cares about our intrinsic size
            // (either for determining its own size, or for sizing/positioning
            // its view to honor "object-fit").  We need it to reflow itself to
            // use our (now-available) intrinsic size:
            auto dirtyHint = dependsOnIntrinsicSize
                                 ? IntrinsicDirty::StyleChange
                                 : IntrinsicDirty::Resize;
            embeddingFrame->PresShell()->FrameNeedsReflow(
                embeddingFrame, dirtyHint, NS_FRAME_IS_DIRTY);
          }
        }
      }
    }
  }
}

//----------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(SVGOuterSVGFrame)
  NS_QUERYFRAME_ENTRY(ISVGSVGFrame)
NS_QUERYFRAME_TAIL_INHERITING(SVGDisplayContainerFrame)

//----------------------------------------------------------------------
// nsIFrame methods
//----------------------------------------------------------------------
// reflowing

/* virtual */
nscoord SVGOuterSVGFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  result = nscoord(0);

  return result;
}

/* virtual */
nscoord SVGOuterSVGFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  SVGSVGElement* svg = static_cast<SVGSVGElement*>(GetContent());
  WritingMode wm = GetWritingMode();
  const SVGAnimatedLength& isize =
      wm.IsVertical() ? svg->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT]
                      : svg->mLengthAttributes[SVGSVGElement::ATTR_WIDTH];

  if (IsReplacedAndContainSize(this)) {
    result = nscoord(0);
  } else if (isize.IsPercentage()) {
    // It looks like our containing block's isize may depend on our isize. In
    // that case our behavior is undefined according to CSS 2.1 section 10.3.2.
    // As a last resort, we'll fall back to returning zero.
    result = nscoord(0);

    // Returning zero may be unhelpful, however, as it leads to unexpected
    // disappearance of %-sized SVGs in orthogonal contexts, where our
    // containing block wants to shrink-wrap. So let's look for an ancestor
    // with non-zero size in this dimension, and use that as a (somewhat
    // arbitrary) result instead.
    nsIFrame* parent = GetParent();
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

/* virtual */
IntrinsicSize SVGOuterSVGFrame::GetIntrinsicSize() {
  // XXXjwatt Note that here we want to return the CSS width/height if they're
  // specified and we're embedded inside an nsIObjectLoadingContent.

  if (IsReplacedAndContainSize(this)) {
    // Intrinsic size of 'contain:size' replaced elements is 0,0.
    return IntrinsicSize(0, 0);
  }

  SVGSVGElement* content = static_cast<SVGSVGElement*>(GetContent());
  const SVGAnimatedLength& width =
      content->mLengthAttributes[SVGSVGElement::ATTR_WIDTH];
  const SVGAnimatedLength& height =
      content->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT];

  IntrinsicSize intrinsicSize;

  if (!width.IsPercentage()) {
    nscoord val =
        nsPresContext::CSSPixelsToAppUnits(width.GetAnimValue(content));
    intrinsicSize.width.emplace(std::max(val, 0));
  }

  if (!height.IsPercentage()) {
    nscoord val =
        nsPresContext::CSSPixelsToAppUnits(height.GetAnimValue(content));
    intrinsicSize.height.emplace(std::max(val, 0));
  }

  return intrinsicSize;
}

/* virtual */
AspectRatio SVGOuterSVGFrame::GetIntrinsicRatio() {
  if (IsReplacedAndContainSize(this)) {
    return AspectRatio();
  }

  const StyleAspectRatio& aspectRatio = StylePosition()->mAspectRatio;
  if (!aspectRatio.auto_) {
    return aspectRatio.ratio.AsRatio().ToLayoutRatio();
  }

  // We only have an intrinsic size/ratio if our width and height attributes
  // are both specified and set to non-percentage values, or we have a viewBox
  // rect: http://www.w3.org/TR/SVGMobile12/coords.html#IntrinsicSizing
  // Unfortunately we have to return the ratio as two nscoords whereas what
  // we have are two floats. Using app units allows for some floating point
  // values to work but really small or large numbers will fail.

  SVGSVGElement* content = static_cast<SVGSVGElement*>(GetContent());
  const SVGAnimatedLength& width =
      content->mLengthAttributes[SVGSVGElement::ATTR_WIDTH];
  const SVGAnimatedLength& height =
      content->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT];

  if (!width.IsPercentage() && !height.IsPercentage()) {
    return AspectRatio::FromSize(width.GetAnimValue(content),
                                 height.GetAnimValue(content));
  }

  SVGViewElement* viewElement = content->GetCurrentViewElement();
  const SVGViewBox* viewbox = nullptr;

  // The logic here should match HasViewBox().
  if (viewElement && viewElement->mViewBox.HasRect()) {
    viewbox = &viewElement->mViewBox.GetAnimValue();
  } else if (content->mViewBox.HasRect()) {
    viewbox = &content->mViewBox.GetAnimValue();
  }

  if (viewbox) {
    return AspectRatio::FromSize(viewbox->width, viewbox->height);
  }

  if (aspectRatio.HasRatio()) {
    // For aspect-ratio: "auto && <ratio>" case.
    return aspectRatio.ratio.AsRatio().ToLayoutRatio();
  }

  return SVGDisplayContainerFrame::GetIntrinsicRatio();
}

/* virtual */
nsIFrame::SizeComputationResult SVGOuterSVGFrame::ComputeSize(
    gfxContext* aRenderingContext, WritingMode aWritingMode,
    const LogicalSize& aCBSize, nscoord aAvailableISize,
    const LogicalSize& aMargin, const LogicalSize& aBorder,
    const LogicalSize& aPadding, ComputeSizeFlags aFlags) {
  if (IsRootOfImage() || IsRootOfReplacedElementSubDoc()) {
    // The embedding element has sized itself using the CSS replaced element
    // sizing rules, using our intrinsic dimensions as necessary. The SVG spec
    // says that the width and height of embedded SVG is overridden by the
    // width and height of the embedding element, so we just need to size to
    // the viewport that the embedding element has established for us.
    return {aCBSize, AspectRatioUsage::None};
  }

  LogicalSize cbSize = aCBSize;
  IntrinsicSize intrinsicSize = GetIntrinsicSize();

  if (!mContent->GetParent()) {
    // We're the root of the outermost browsing context, so we need to scale
    // cbSize by the full-zoom so that SVGs with percentage width/height zoom:

    NS_ASSERTION(aCBSize.ISize(aWritingMode) != NS_UNCONSTRAINEDSIZE &&
                     aCBSize.BSize(aWritingMode) != NS_UNCONSTRAINEDSIZE,
                 "root should not have auto-width/height containing block");

    if (!IsContainingWindowElementOfType(nullptr, nsGkAtoms::iframe)) {
      cbSize.ISize(aWritingMode) *= PresContext()->GetFullZoom();
      cbSize.BSize(aWritingMode) *= PresContext()->GetFullZoom();
    }

    // We also need to honour the width and height attributes' default values
    // of 100% when we're the root of a browsing context.  (GetIntrinsicSize()
    // doesn't report these since there's no such thing as a percentage
    // intrinsic size.  Also note that explicit percentage values are mapped
    // into style, so the following isn't for them.)

    SVGSVGElement* content = static_cast<SVGSVGElement*>(GetContent());

    const SVGAnimatedLength& width =
        content->mLengthAttributes[SVGSVGElement::ATTR_WIDTH];
    if (width.IsPercentage()) {
      MOZ_ASSERT(!intrinsicSize.width,
                 "GetIntrinsicSize should have reported no intrinsic width");
      float val = width.GetAnimValInSpecifiedUnits() / 100.0f;
      intrinsicSize.width.emplace(std::max(val, 0.0f) *
                                  cbSize.Width(aWritingMode));
    }

    const SVGAnimatedLength& height =
        content->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT];
    NS_ASSERTION(aCBSize.BSize(aWritingMode) != NS_UNCONSTRAINEDSIZE,
                 "root should not have auto-height containing block");
    if (height.IsPercentage()) {
      MOZ_ASSERT(!intrinsicSize.height,
                 "GetIntrinsicSize should have reported no intrinsic height");
      float val = height.GetAnimValInSpecifiedUnits() / 100.0f;
      intrinsicSize.height.emplace(std::max(val, 0.0f) *
                                   cbSize.Height(aWritingMode));
    }
    MOZ_ASSERT(intrinsicSize.height && intrinsicSize.width,
               "We should have just handled the only situation where"
               "we lack an intrinsic height or width.");
  }

  return {ComputeSizeWithIntrinsicDimensions(
              aRenderingContext, aWritingMode, intrinsicSize,
              GetIntrinsicRatio(), cbSize, aMargin, aBorder, aPadding, aFlags),
          AspectRatioUsage::None};
}

void SVGOuterSVGFrame::Reflow(nsPresContext* aPresContext,
                              ReflowOutput& aDesiredSize,
                              const ReflowInput& aReflowInput,
                              nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("SVGOuterSVGFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_FRAME_TRACE(
      NS_FRAME_TRACE_CALLS,
      ("enter SVGOuterSVGFrame::Reflow: availSize=%d,%d",
       aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  MOZ_ASSERT(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aDesiredSize.Width() =
      aReflowInput.ComputedWidth() +
      aReflowInput.ComputedPhysicalBorderPadding().LeftRight();
  aDesiredSize.Height() =
      aReflowInput.ComputedHeight() +
      aReflowInput.ComputedPhysicalBorderPadding().TopBottom();

  NS_ASSERTION(!GetPrevInFlow(), "SVG can't currently be broken across pages.");

  SVGSVGElement* svgElem = static_cast<SVGSVGElement*>(GetContent());

  auto* anonKid = static_cast<SVGOuterSVGAnonChildFrame*>(
      PrincipalChildList().FirstChild());

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
      anonChild->MarkSubtreeDirty();
      for (nsIFrame* child : anonChild->PrincipalChildList()) {
        child->MarkSubtreeDirty();
      }
    }
    changeBits |= COORD_CONTEXT_CHANGED;
    svgElem->SetViewportSize(newViewportSize);
  }
  if (mFullZoom != PresContext()->GetFullZoom() &&
      !IsContainingWindowElementOfType(nullptr, nsGkAtoms::iframe)) {
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
  if (HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
    ReflowSVGNonDisplayText(this);
  } else {
    // Update the mRects and ink overflow rects of all our descendants,
    // including our anonymous wrapper kid:
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
  // With regards to ink overflow, we always clip root-<svg> (see our
  // BuildDisplayList method) regardless of the value of the 'overflow'
  // property since that is per-spec, even for the initial 'visible' value. For
  // that reason there's no point in adding descendant ink overflow to our
  // own when this frame is for a root-<svg>. That said, there's also a very
  // good performance reason for us wanting to avoid doing so. If we did, then
  // the frame's overflow would often change as descendants that are partially
  // or fully outside its rect moved (think animation on/off screen), and that
  // would cause us to do a full NS_FRAME_IS_DIRTY reflow and repaint of the
  // entire document tree each such move (see bug 875175).
  //
  // So it's only non-root outer-<svg> that has the ink overflow of its
  // descendants added to its own. (Note that the default user-agent style
  // sheet makes 'hidden' the default value for :not(root(svg)), so usually
  // FinishAndStoreOverflow will still clip this back to the frame's rect.)
  //
  // WARNING!! Keep UpdateBounds below in sync with whatever we do for our
  // overflow rects here! (Again, see bug 875175.)
  //
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  // An outer SVG will be here as a nondisplay if it fails the conditional
  // processing test. In that case, we don't maintain its overflow.
  if (!HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
    if (!mIsRootContent) {
      aDesiredSize.mOverflowAreas.InkOverflow().UnionRect(
          aDesiredSize.mOverflowAreas.InkOverflow(),
          anonKid->InkOverflowRect() + anonKid->GetPosition());
    }
    FinishAndStoreOverflow(&aDesiredSize);
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit SVGOuterSVGFrame::Reflow: size=%d,%d",
                  aDesiredSize.Width(), aDesiredSize.Height()));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void SVGOuterSVGFrame::DidReflow(nsPresContext* aPresContext,
                                 const ReflowInput* aReflowInput) {
  SVGDisplayContainerFrame::DidReflow(aPresContext, aReflowInput);

  // Make sure elements styled by :hover get updated if script/animation moves
  // them under or out from under the pointer:
  PresShell()->SynthesizeMouseMove(false);
}

/* virtual */
void SVGOuterSVGFrame::UnionChildOverflow(nsOverflowAreas& aOverflowAreas) {
  // See the comments in Reflow above.

  // WARNING!! Keep this in sync with Reflow above!

  if (!mIsRootContent) {
    nsIFrame* anonKid = PrincipalChildList().FirstChild();
    aOverflowAreas.InkOverflow().UnionRect(
        aOverflowAreas.InkOverflow(),
        anonKid->InkOverflowRect() + anonKid->GetPosition());
  }
}

//----------------------------------------------------------------------
// container methods

/**
 * Used to paint/hit-test SVG when SVG display lists are disabled.
 */
class nsDisplayOuterSVG final : public nsPaintedDisplayItem {
 public:
  nsDisplayOuterSVG(nsDisplayListBuilder* aBuilder, SVGOuterSVGFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayOuterSVG);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayOuterSVG)

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*>* aOutFrames) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aContext) override;

  virtual void ComputeInvalidationRegion(
      nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
      nsRegion* aInvalidRegion) const override;

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayItemGenericImageGeometry(this, aBuilder);
  }

  NS_DISPLAY_DECL_NAME("SVGOuterSVG", TYPE_SVG_OUTER_SVG)
};

void nsDisplayOuterSVG::HitTest(nsDisplayListBuilder* aBuilder,
                                const nsRect& aRect, HitTestState* aState,
                                nsTArray<nsIFrame*>* aOutFrames) {
  SVGOuterSVGFrame* outerSVGFrame = static_cast<SVGOuterSVGFrame*>(mFrame);

  nsPoint refFrameToContentBox =
      ToReferenceFrame() +
      outerSVGFrame->GetContentRectRelativeToSelf().TopLeft();

  nsPoint pointRelativeToContentBox =
      nsPoint(aRect.x + aRect.width / 2, aRect.y + aRect.height / 2) -
      refFrameToContentBox;

  gfxPoint svgViewportRelativePoint =
      gfxPoint(pointRelativeToContentBox.x, pointRelativeToContentBox.y) /
      AppUnitsPerCSSPixel();

  auto* anonKid = static_cast<SVGOuterSVGAnonChildFrame*>(
      outerSVGFrame->PrincipalChildList().FirstChild());

  nsIFrame* frame =
      SVGUtils::HitTestChildren(anonKid, svgViewportRelativePoint);
  if (frame) {
    aOutFrames->AppendElement(frame);
  }
}

void nsDisplayOuterSVG::Paint(nsDisplayListBuilder* aBuilder,
                              gfxContext* aContext) {
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

  nsRect clipRect = GetPaintRect().Intersect(viewportRect);

  uint32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  nsIntRect contentAreaDirtyRect =
      (clipRect - viewportRect.TopLeft()).ToOutsidePixels(appUnitsPerDevPixel);

  gfxPoint devPixelOffset = nsLayoutUtils::PointToGfxPoint(
      viewportRect.TopLeft(), appUnitsPerDevPixel);

  aContext->Save();
  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  // We include the offset of our frame and a scale from device pixels to user
  // units (i.e. CSS px) in the matrix that we pass to our children):
  gfxMatrix tm = SVGUtils::GetCSSPxToDevPxMatrix(mFrame) *
                 gfxMatrix::Translation(devPixelOffset);
  SVGUtils::PaintFrameWithEffects(mFrame, *aContext, tm, imgParams,
                                  &contentAreaDirtyRect);
  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, imgParams.result);
  aContext->Restore();

#if defined(DEBUG) && defined(SVG_DEBUG_PAINT_TIMING)
  PRTime end = PR_Now();
  printf("SVG Paint Timing: %f ms\n", (end - start) / 1000.0);
#endif
}

nsRegion SVGOuterSVGFrame::FindInvalidatedForeignObjectFrameChildren(
    nsIFrame* aFrame) {
  nsRegion result;
  if (mForeignObjectHash && mForeignObjectHash->Count()) {
    for (auto it = mForeignObjectHash->Iter(); !it.Done(); it.Next()) {
      result.Or(result, it.Get()->GetKey()->GetInvalidRegion());
    }
  }
  return result;
}

void nsDisplayOuterSVG::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto* frame = static_cast<SVGOuterSVGFrame*>(mFrame);
  frame->InvalidateSVG(frame->FindInvalidatedForeignObjectFrameChildren(frame));

  nsRegion result = frame->GetInvalidRegion();
  result.MoveBy(ToReferenceFrame());
  frame->ClearInvalidRegion();

  nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
  aInvalidRegion->Or(*aInvalidRegion, result);

  const auto* geometry =
      static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }
}

nsresult SVGOuterSVGFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None &&
      !HasAnyStateBits(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_NONDISPLAY)) {
    if (aAttribute == nsGkAtoms::viewBox ||
        aAttribute == nsGkAtoms::preserveAspectRatio ||
        aAttribute == nsGkAtoms::transform) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nullptr;

      SVGUtils::NotifyChildrenOfSVGChange(
          PrincipalChildList().FirstChild(),
          aAttribute == nsGkAtoms::viewBox
              ? TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED
              : TRANSFORM_CHANGED);

      if (aAttribute != nsGkAtoms::transform) {
        static_cast<SVGSVGElement*>(GetContent())
            ->ChildrenOnlyTransformChanged();
      }
    }
    if (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height ||
        aAttribute == nsGkAtoms::viewBox) {
      // Don't call ChildrenOnlyTransformChanged() here, since we call it
      // under Reflow if the width/height/viewBox actually changed.

      nsIFrame* embeddingFrame;
      if (IsRootOfReplacedElementSubDoc(&embeddingFrame) && embeddingFrame) {
        bool dependsOnIntrinsicSize = DependsOnIntrinsicSize(embeddingFrame);
        if (dependsOnIntrinsicSize ||
            embeddingFrame->StylePosition()->mObjectFit !=
                StyleObjectFit::Fill) {
          // Tell embeddingFrame's presShell it needs to be reflowed (which
          // takes care of reflowing us too). And if it depends on our
          // intrinsic size, then we need to invalidate its intrinsic sizes
          // (via the IntrinsicDirty::StyleChange hint.)
          auto dirtyHint = dependsOnIntrinsicSize ? IntrinsicDirty::StyleChange
                                                  : IntrinsicDirty::Resize;
          embeddingFrame->PresShell()->FrameNeedsReflow(
              embeddingFrame, dirtyHint, NS_FRAME_IS_DIRTY);
        }  // else our width/height/viewBox are irrelevant to the outer doc.
      } else {
        // We are not embedded by reference, so our 'width' and 'height'
        // attributes are not overridden (and viewBox may influence our
        // intrinsic aspect ratio).  We need to reflow.
        PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                      NS_FRAME_IS_DIRTY);
      }
    }
  }

  return NS_OK;
}

bool SVGOuterSVGFrame::IsSVGTransformed(Matrix* aOwnTransform,
                                        Matrix* aFromParentTransform) const {
  // Our anonymous child's HasChildrenOnlyTransform() implementation makes sure
  // our children-only transforms are applied to our children.  We only care
  // about transforms that transform our own frame here.

  bool foundTransform = false;

  SVGSVGElement* content = static_cast<SVGSVGElement*>(GetContent());
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

//----------------------------------------------------------------------
// painting

void SVGOuterSVGFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                        const nsDisplayListSet& aLists) {
  if (HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
    return;
  }

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  nsRect visibleRect = aBuilder->GetVisibleRect();
  nsRect dirtyRect = aBuilder->GetDirtyRect();

  // Per-spec, we always clip root-<svg> even when 'overflow' has its initial
  // value of 'visible'. See also the "ink overflow" comments in Reflow.
  DisplayListClipState::AutoSaveRestore autoSR(aBuilder);
  if (mIsRootContent || StyleDisplay()->IsScrollableOverflow()) {
    autoSR.ClipContainingBlockDescendantsToContentBox(aBuilder, this);
    visibleRect = visibleRect.Intersect(GetContentRectRelativeToSelf());
    dirtyRect = dirtyRect.Intersect(GetContentRectRelativeToSelf());
  }

  nsDisplayListBuilder::AutoBuildingDisplayList building(
      aBuilder, this, visibleRect, dirtyRect);

  if ((aBuilder->IsForEventDelivery() &&
       NS_SVGDisplayListHitTestingEnabled()) ||
      (!aBuilder->IsForEventDelivery() && NS_SVGDisplayListPaintingEnabled())) {
    nsDisplayList* contentList = aLists.Content();
    nsDisplayListSet set(contentList, contentList, contentList, contentList,
                         contentList, contentList);
    BuildDisplayListForNonBlockChildren(aBuilder, set);
  } else if (IsVisibleForPainting() || !aBuilder->IsForPainting()) {
    aLists.Content()->AppendNewToTop<nsDisplayOuterSVG>(aBuilder, this);
  }
}

//----------------------------------------------------------------------
// ISVGSVGFrame methods:

void SVGOuterSVGFrame::NotifyViewportOrTransformChanged(uint32_t aFlags) {
  MOZ_ASSERT(aFlags && !(aFlags & ~(COORD_CONTEXT_CHANGED | TRANSFORM_CHANGED |
                                    FULL_ZOOM_CHANGED)),
             "Unexpected aFlags value");

  // No point in doing anything when were not init'ed yet:
  if (!mViewportInitialized) {
    return;
  }

  SVGSVGElement* content = static_cast<SVGSVGElement*>(GetContent());

  if (aFlags & COORD_CONTEXT_CHANGED) {
    if (content->HasViewBox()) {
      // Percentage lengths on children resolve against the viewBox rect so we
      // don't need to notify them of the viewport change, but the viewBox
      // transform will have changed, so we need to notify them of that instead.
      aFlags = TRANSFORM_CHANGED;
    } else if (content->ShouldSynthesizeViewBox()) {
      // In the case of a synthesized viewBox, the synthetic viewBox's rect
      // changes as the viewport changes. As a result we need to maintain the
      // COORD_CONTEXT_CHANGED flag.
      aFlags |= TRANSFORM_CHANGED;
    } else if (mCanvasTM && mCanvasTM->IsSingular()) {
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

    if (haveNonFulLZoomTransformChange && !(mState & NS_FRAME_IS_NONDISPLAY)) {
      uint32_t flags =
          (mState & NS_FRAME_IN_REFLOW) ? SVGSVGElement::eDuringReflow : 0;
      content->ChildrenOnlyTransformChanged(flags);
    }
  }

  SVGUtils::NotifyChildrenOfSVGChange(PrincipalChildList().FirstChild(),
                                      aFlags);
}

//----------------------------------------------------------------------
// ISVGDisplayableFrame methods:

void SVGOuterSVGFrame::PaintSVG(gfxContext& aContext,
                                const gfxMatrix& aTransform,
                                imgDrawingParams& aImgParams,
                                const nsIntRect* aDirtyRect) {
  NS_ASSERTION(
      PrincipalChildList().FirstChild()->IsSVGOuterSVGAnonChildFrame() &&
          !PrincipalChildList().FirstChild()->GetNextSibling(),
      "We should have a single, anonymous, child");
  auto* anonKid = static_cast<SVGOuterSVGAnonChildFrame*>(
      PrincipalChildList().FirstChild());
  anonKid->PaintSVG(aContext, aTransform, aImgParams, aDirtyRect);
}

SVGBBox SVGOuterSVGFrame::GetBBoxContribution(
    const gfx::Matrix& aToBBoxUserspace, uint32_t aFlags) {
  NS_ASSERTION(
      PrincipalChildList().FirstChild()->IsSVGOuterSVGAnonChildFrame() &&
          !PrincipalChildList().FirstChild()->GetNextSibling(),
      "We should have a single, anonymous, child");
  // We must defer to our child so that we don't include our
  // content->PrependLocalTransformsTo() transforms.
  auto* anonKid = static_cast<SVGOuterSVGAnonChildFrame*>(
      PrincipalChildList().FirstChild());
  return anonKid->GetBBoxContribution(aToBBoxUserspace, aFlags);
}

//----------------------------------------------------------------------
// SVGContainerFrame methods:

gfxMatrix SVGOuterSVGFrame::GetCanvasTM() {
  if (!mCanvasTM) {
    SVGSVGElement* content = static_cast<SVGSVGElement*>(GetContent());

    float devPxPerCSSPx = 1.0f / nsPresContext::AppUnitsToFloatCSSPixels(
                                     PresContext()->AppUnitsPerDevPixel());

    gfxMatrix tm = content->PrependLocalTransformsTo(
        gfxMatrix::Scaling(devPxPerCSSPx, devPxPerCSSPx));
    mCanvasTM = MakeUnique<gfxMatrix>(tm);
  }
  return *mCanvasTM;
}

//----------------------------------------------------------------------
// Implementation helpers

template <typename... Args>
bool SVGOuterSVGFrame::IsContainingWindowElementOfType(
    nsIFrame** aContainingWindowFrame, Args... aArgs) const {
  if (!mContent->GetParent()) {
    // Our content is the document element
    if (nsCOMPtr<nsIDocShell> docShell = PresContext()->GetDocShell()) {
      RefPtr<BrowsingContext> bc = docShell->GetBrowsingContext();
      const Maybe<nsString>& type = bc->GetEmbedderElementType();

      if (type && ::IsAnyAtomEqual(*type, aArgs...)) {
        if (aContainingWindowFrame) {
          if (const Element* const element = bc->GetEmbedderElement()) {
            *aContainingWindowFrame = element->GetPrimaryFrame();
            NS_ASSERTION(*aContainingWindowFrame, "Yikes, no frame!");
          }
        }
        return true;
      }
    }
  }
  if (aContainingWindowFrame) {
    *aContainingWindowFrame = nullptr;
  }
  return false;
}

bool SVGOuterSVGFrame::IsRootOfReplacedElementSubDoc(
    nsIFrame** aEmbeddingFrame) {
  return IsContainingWindowElementOfType(aEmbeddingFrame, nsGkAtoms::object,
                                         nsGkAtoms::embed);
}

bool SVGOuterSVGFrame::IsRootOfImage() {
  if (!mContent->GetParent()) {
    // Our content is the document element
    Document* doc = mContent->GetUncomposedDoc();
    if (doc && doc->IsBeingUsedAsImage()) {
      // Our document is being used as an image
      return true;
    }
  }

  return false;
}

bool SVGOuterSVGFrame::VerticalScrollbarNotNeeded() const {
  const SVGAnimatedLength& height =
      static_cast<SVGSVGElement*>(GetContent())
          ->mLengthAttributes[SVGSVGElement::ATTR_HEIGHT];
  return height.IsPercentage() && height.GetBaseValInSpecifiedUnits() <= 100;
}

void SVGOuterSVGFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  nsIFrame* anonKid = PrincipalChildList().FirstChild();
  MOZ_ASSERT(anonKid->IsSVGOuterSVGAnonChildFrame());
  aResult.AppendElement(OwnedAnonBox(anonKid));
}

}  // namespace mozilla

//----------------------------------------------------------------------
// Implementation of SVGOuterSVGAnonChildFrame

nsContainerFrame* NS_NewSVGOuterSVGAnonChildFrame(
    mozilla::PresShell* aPresShell, mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGOuterSVGAnonChildFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGOuterSVGAnonChildFrame)

#ifdef DEBUG
void SVGOuterSVGAnonChildFrame::Init(nsIContent* aContent,
                                     nsContainerFrame* aParent,
                                     nsIFrame* aPrevInFlow) {
  MOZ_ASSERT(aParent->IsSVGOuterSVGFrame(), "Unexpected parent");
  SVGDisplayContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif

void SVGOuterSVGAnonChildFrame::BuildDisplayList(
    nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) {
  // Wrap our contents into an nsDisplaySVGWrapper.
  // We wrap this frame instead of the SVGOuterSVGFrame so that the wrapper
  // doesn't contain the <svg> element's CSS styles, like backgrounds or
  // borders. Creating the nsDisplaySVGWrapper here also means that it'll be
  // inside the nsDisplayTransform for our viewbox transform. The
  // nsDisplaySVGWrapper's reference frame is this frame, because this frame
  // always returns true from IsSVGTransformed.
  nsDisplayList newList;
  nsDisplayListSet set(&newList, &newList, &newList, &newList, &newList,
                       &newList);
  BuildDisplayListForNonBlockChildren(aBuilder, set);
  aLists.Content()->AppendNewToTop<nsDisplaySVGWrapper>(aBuilder, this,
                                                        &newList);
}

static Matrix ComputeOuterSVGAnonChildFrameTransform(
    const SVGOuterSVGAnonChildFrame* aFrame) {
  // Our elements 'transform' attribute is applied to our SVGOuterSVGFrame
  // parent, and the element's children-only transforms are applied to us, the
  // anonymous child frame. Since we are the child frame, we apply the
  // children-only transforms as if they are our own transform.
  SVGSVGElement* content = static_cast<SVGSVGElement*>(aFrame->GetContent());

  if (!content->HasChildrenOnlyTransform()) {
    return Matrix();
  }

  // Outer-<svg> doesn't use x/y, so we can pass eChildToUserSpace here.
  gfxMatrix ownMatrix =
      content->PrependLocalTransformsTo(gfxMatrix(), eChildToUserSpace);

  if (ownMatrix.HasNonTranslation()) {
    // viewBox, currentScale and currentTranslate should only produce a
    // rectilinear transform.
    MOZ_ASSERT(ownMatrix.IsRectilinear(),
               "Non-rectilinear transform will break the following logic");

    // The nsDisplayTransform code will apply this transform to our frame,
    // including to our frame position.  We don't want our frame position to
    // be scaled though, so we need to correct for that in the transform.
    // XXX Yeah, this is a bit hacky.
    CSSPoint pos = CSSPixel::FromAppUnits(aFrame->GetPosition());
    CSSPoint scaledPos = CSSPoint(ownMatrix._11 * pos.x, ownMatrix._22 * pos.y);
    CSSPoint deltaPos = scaledPos - pos;
    ownMatrix *= gfxMatrix::Translation(-deltaPos.x, -deltaPos.y);
  }

  return gfx::ToMatrix(ownMatrix);
}

// We want this frame to be a reference frame. An easy way to achieve that is
// to always return true from this method, even for identity transforms.
// This frame being a reference frame ensures that the offset between this
// <svg> element and the parent reference frame is completely absorbed by the
// nsDisplayTransform that's created for this frame, and that this offset does
// not affect our descendants' transforms. Consequently, if the <svg> element
// moves, e.g. during scrolling, the transform matrices of our contents are
// unaffected. This simplifies invalidation.
bool SVGOuterSVGAnonChildFrame::IsSVGTransformed(
    Matrix* aOwnTransform, Matrix* aFromParentTransform) const {
  if (aOwnTransform) {
    *aOwnTransform = ComputeOuterSVGAnonChildFrameTransform(this);
  }

  return true;
}

}  // namespace mozilla
