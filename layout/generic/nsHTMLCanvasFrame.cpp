/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <canvas> element */

#include "nsHTMLCanvasFrame.h"

#include "nsGkAtoms.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include "nsStyleUtil.h"
#include "ImageLayers.h"
#include "Layers.h"
#include "ActiveLayerTracker.h"

#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::gfx;

/* Helper for our nsIFrame::GetIntrinsicSize() impl. Takes the result of
 * "GetCanvasSize()" as a parameter, which may help avoid redundant
 * indirect calls to GetCanvasSize().
 *
 * @param aCanvasSizeInPx The canvas's size in CSS pixels, as returned
 *                        by GetCanvasSize().
 * @return The canvas's intrinsic size, as an IntrinsicSize object.
 */
static IntrinsicSize
IntrinsicSizeFromCanvasSize(const nsIntSize& aCanvasSizeInPx)
{
  IntrinsicSize intrinsicSize;
  intrinsicSize.width.SetCoordValue(
    nsPresContext::CSSPixelsToAppUnits(aCanvasSizeInPx.width));
  intrinsicSize.height.SetCoordValue(
    nsPresContext::CSSPixelsToAppUnits(aCanvasSizeInPx.height));

  return intrinsicSize;
}

/* Helper for our nsIFrame::GetIntrinsicRatio() impl. Takes the result of
 * "GetCanvasSize()" as a parameter, which may help avoid redundant
 * indirect calls to GetCanvasSize().
 *
 * @param aCanvasSizeInPx The canvas's size in CSS pixels, as returned
 *                        by GetCanvasSize().
 * @return The canvas's intrinsic ratio, as a nsSize.
 */
static nsSize
IntrinsicRatioFromCanvasSize(const nsIntSize& aCanvasSizeInPx)
{
  return nsSize(nsPresContext::CSSPixelsToAppUnits(aCanvasSizeInPx.width),
                nsPresContext::CSSPixelsToAppUnits(aCanvasSizeInPx.height));
}

class nsDisplayCanvas : public nsDisplayItem {
public:
  nsDisplayCanvas(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayCanvas);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayCanvas() {
    MOZ_COUNT_DTOR(nsDisplayCanvas);
  }
#endif

  NS_DISPLAY_DECL_NAME("nsDisplayCanvas", TYPE_CANVAS)

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override {
    *aSnap = false;
    nsHTMLCanvasFrame* f = static_cast<nsHTMLCanvasFrame*>(Frame());
    HTMLCanvasElement* canvas =
      HTMLCanvasElement::FromContent(f->GetContent());
    nsRegion result;
    if (canvas->GetIsOpaque()) {
      // OK, the entire region painted by the canvas is opaque. But what is
      // that region? It's the canvas's "dest rect" (controlled by the
      // object-fit/object-position CSS properties), clipped to the container's
      // content box (which is what GetBounds() returns). So, we grab those
      // rects and intersect them.
      nsRect constraintRect = GetBounds(aBuilder, aSnap);

      // Need intrinsic size & ratio, for ComputeObjectDestRect:
      nsIntSize canvasSize = f->GetCanvasSize();
      IntrinsicSize intrinsicSize = IntrinsicSizeFromCanvasSize(canvasSize);
      nsSize intrinsicRatio = IntrinsicRatioFromCanvasSize(canvasSize);

      const nsRect destRect =
        nsLayoutUtils::ComputeObjectDestRect(constraintRect,
                                             intrinsicSize, intrinsicRatio,
                                             f->StylePosition());
      return nsRegion(destRect.Intersect(constraintRect));
    }
    return result;
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override {
    *aSnap = true;
    nsHTMLCanvasFrame* f = static_cast<nsHTMLCanvasFrame*>(Frame());
    return f->GetInnerArea() + ToReferenceFrame();
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override
  {
    return static_cast<nsHTMLCanvasFrame*>(mFrame)->
      BuildLayer(aBuilder, aManager, this, aContainerParameters);
  }
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    if (HTMLCanvasElement::FromContent(mFrame->GetContent())->ShouldForceInactiveLayer(aManager))
      return LAYER_INACTIVE;

    // If compositing is cheap, just do that
    if (aManager->IsCompositingCheap() ||
        ActiveLayerTracker::IsContentActive(mFrame))
      return mozilla::LAYER_ACTIVE;

    return LAYER_INACTIVE;
  }
};


nsIFrame*
NS_NewHTMLCanvasFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsHTMLCanvasFrame(aContext);
}

NS_QUERYFRAME_HEAD(nsHTMLCanvasFrame)
  NS_QUERYFRAME_ENTRY(nsHTMLCanvasFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLCanvasFrame)

void
nsHTMLCanvasFrame::Init(nsIContent*       aContent,
                        nsContainerFrame* aParent,
                        nsIFrame*         aPrevInFlow)
{
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  // We can fill in the canvas before the canvas frame is created, in
  // which case we never get around to marking the content as active. Therefore,
  // we mark it active here when we create the frame.
  ActiveLayerTracker::NotifyContentChange(this);
}

nsHTMLCanvasFrame::~nsHTMLCanvasFrame()
{
}

nsIntSize
nsHTMLCanvasFrame::GetCanvasSize()
{
  nsIntSize size(0,0);
  HTMLCanvasElement *canvas =
    HTMLCanvasElement::FromContentOrNull(GetContent());
  if (canvas) {
    size = canvas->GetSize();
    MOZ_ASSERT(size.width >= 0 && size.height >= 0,
               "we should've required <canvas> width/height attrs to be "
               "unsigned (non-negative) values");
  } else {
    NS_NOTREACHED("couldn't get canvas size");
  }

  return size;
}

/* virtual */ nscoord
nsHTMLCanvasFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  bool vertical = GetWritingMode().IsVertical();
  nscoord result = nsPresContext::CSSPixelsToAppUnits(
    vertical ? GetCanvasSize().height : GetCanvasSize().width);
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsHTMLCanvasFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  bool vertical = GetWritingMode().IsVertical();
  nscoord result = nsPresContext::CSSPixelsToAppUnits(
    vertical ? GetCanvasSize().height : GetCanvasSize().width);
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

/* virtual */ IntrinsicSize
nsHTMLCanvasFrame::GetIntrinsicSize()
{
  return IntrinsicSizeFromCanvasSize(GetCanvasSize());
}

/* virtual */ nsSize
nsHTMLCanvasFrame::GetIntrinsicRatio()
{
  return IntrinsicRatioFromCanvasSize(GetCanvasSize());
}

/* virtual */
LogicalSize
nsHTMLCanvasFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                               WritingMode aWM,
                               const LogicalSize& aCBSize,
                               nscoord aAvailableISize,
                               const LogicalSize& aMargin,
                               const LogicalSize& aBorder,
                               const LogicalSize& aPadding,
                               ComputeSizeFlags aFlags)
{
  nsIntSize size = GetCanvasSize();

  IntrinsicSize intrinsicSize;
  intrinsicSize.width.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(size.width));
  intrinsicSize.height.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(size.height));

  nsSize intrinsicRatio = GetIntrinsicRatio(); // won't actually be used

  return ComputeSizeWithIntrinsicDimensions(aRenderingContext, aWM,
                                            intrinsicSize, intrinsicRatio,
                                            aCBSize, aMargin, aBorder, aPadding,
                                            aFlags);
}

void
nsHTMLCanvasFrame::Reflow(nsPresContext*           aPresContext,
                          ReflowOutput&     aMetrics,
                          const ReflowInput& aReflowInput,
                          nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsHTMLCanvasFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsHTMLCanvasFrame::Reflow: availSize=%d,%d",
                  aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus.Reset();

  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize finalSize(wm,
                        aReflowInput.ComputedISize(),
                        aReflowInput.ComputedBSize());

  // stash this away so we can compute our inner area later
  mBorderPadding   = aReflowInput.ComputedLogicalBorderPadding();

  finalSize.ISize(wm) += mBorderPadding.IStartEnd(wm);
  finalSize.BSize(wm) += mBorderPadding.BStartEnd(wm);

  if (GetPrevInFlow()) {
    nscoord y = GetContinuationOffset(&finalSize.ISize(wm));
    finalSize.BSize(wm) -= y + mBorderPadding.BStart(wm);
    finalSize.BSize(wm) = std::max(0, finalSize.BSize(wm));
  }

  aMetrics.SetSize(wm, finalSize);
  aMetrics.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aMetrics);

  // Reflow the single anon block child.
  nsReflowStatus childStatus;
  nsIFrame* childFrame = mFrames.FirstChild();
  WritingMode childWM = childFrame->GetWritingMode();
  LogicalSize availSize = aReflowInput.ComputedSize(childWM);
  availSize.BSize(childWM) = NS_UNCONSTRAINEDSIZE;
  NS_ASSERTION(!childFrame->GetNextSibling(), "HTML canvas should have 1 kid");
  ReflowOutput childDesiredSize(aReflowInput.GetWritingMode(), aMetrics.mFlags);
  ReflowInput childReflowInput(aPresContext, aReflowInput, childFrame,
                                     availSize);
  ReflowChild(childFrame, aPresContext, childDesiredSize, childReflowInput,
              0, 0, 0, childStatus, nullptr);
  FinishReflowChild(childFrame, aPresContext, childDesiredSize,
                    &childReflowInput, 0, 0, 0);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsHTMLCanvasFrame::Reflow: size=%d,%d",
                   aMetrics.ISize(wm), aMetrics.BSize(wm)));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

// FIXME taken from nsImageFrame, but then had splittable frame stuff
// removed.  That needs to be fixed.
// XXXdholbert As in nsImageFrame, this function's clients should probably
// just be calling GetContentRectRelativeToSelf().
nsRect 
nsHTMLCanvasFrame::GetInnerArea() const
{
  nsMargin bp = mBorderPadding.GetPhysicalMargin(GetWritingMode());
  nsRect r;
  r.x = bp.left;
  r.y = bp.top;
  r.width = mRect.width - bp.left - bp.right;
  r.height = mRect.height - bp.top - bp.bottom;
  return r;
}

already_AddRefed<Layer>
nsHTMLCanvasFrame::BuildLayer(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              nsDisplayItem* aItem,
                              const ContainerLayerParameters& aContainerParameters)
{
  nsRect area = GetContentRectRelativeToSelf() + aItem->ToReferenceFrame();
  HTMLCanvasElement* element = static_cast<HTMLCanvasElement*>(GetContent());
  nsIntSize canvasSizeInPx = GetCanvasSize();

  nsPresContext* presContext = PresContext();
  element->HandlePrintCallback(presContext->Type());

  if (canvasSizeInPx.width <= 0 || canvasSizeInPx.height <= 0 || area.IsEmpty())
    return nullptr;

  CanvasLayer* oldLayer = static_cast<CanvasLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem));
  RefPtr<Layer> layer = element->GetCanvasLayer(aBuilder, oldLayer, aManager);
  if (!layer)
    return nullptr;

  IntrinsicSize intrinsicSize = IntrinsicSizeFromCanvasSize(canvasSizeInPx);
  nsSize intrinsicRatio = IntrinsicRatioFromCanvasSize(canvasSizeInPx);

  nsRect dest =
    nsLayoutUtils::ComputeObjectDestRect(area, intrinsicSize, intrinsicRatio,
                                         StylePosition());

  gfxRect destGFXRect = presContext->AppUnitsToGfxUnits(dest);

  // Transform the canvas into the right place
  gfxPoint p = destGFXRect.TopLeft() + aContainerParameters.mOffset;
  Matrix transform = Matrix::Translation(p.x, p.y);
  transform.PreScale(destGFXRect.Width() / canvasSizeInPx.width,
                     destGFXRect.Height() / canvasSizeInPx.height);
  layer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
  if (layer->GetType() == layers::Layer::TYPE_CANVAS) {
    RefPtr<CanvasLayer> canvasLayer = static_cast<CanvasLayer*>(layer.get());
    canvasLayer->SetSamplingFilter(nsLayoutUtils::GetSamplingFilterForFrame(this));
  } else if (layer->GetType() == layers::Layer::TYPE_IMAGE) {
    RefPtr<ImageLayer> imageLayer = static_cast<ImageLayer*>(layer.get());
    imageLayer->SetSamplingFilter(nsLayoutUtils::GetSamplingFilterForFrame(this));
  }

  return layer.forget();
}

void
nsHTMLCanvasFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                    const nsRect&           aDirtyRect,
                                    const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return;

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  uint32_t clipFlags =
    nsStyleUtil::ObjectPropsMightCauseOverflow(StylePosition()) ?
    0 : DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT;

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox
    clip(aBuilder, this, clipFlags);

  aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplayCanvas(aBuilder, this));

  DisplaySelectionOverlay(aBuilder, aLists.Content(),
                          nsISelectionDisplay::DISPLAY_IMAGES);
}

// get the offset into the content area of the image where aImg starts if it is a continuation.
// from nsImageFrame
nscoord
nsHTMLCanvasFrame::GetContinuationOffset(nscoord* aWidth) const
{
  nscoord offset = 0;
  if (aWidth) {
    *aWidth = 0;
  }

  if (GetPrevInFlow()) {
    for (nsIFrame* prevInFlow = GetPrevInFlow() ; prevInFlow; prevInFlow = prevInFlow->GetPrevInFlow()) {
      nsRect rect = prevInFlow->GetRect();
      if (aWidth) {
        *aWidth = rect.width;
      }
      offset += rect.height;
    }
    offset -= mBorderPadding.GetPhysicalMargin(GetWritingMode()).top;
    offset = std::max(0, offset);
  }
  return offset;
}

void
nsHTMLCanvasFrame::DoUpdateStyleOfOwnedAnonBoxes(ServoStyleSet& aStyleSet,
                                                 nsStyleChangeList& aChangeList,
                                                 nsChangeHint aHintForThisFrame)
{
  MOZ_ASSERT(mFrames.FirstChild(), "Must have our canvas content anon box");
  MOZ_ASSERT(!mFrames.FirstChild()->GetNextSibling(),
             "Must only have our canvas content anon box");
  UpdateStyleOfChildAnonBox(mFrames.FirstChild(),
                            aStyleSet, aChangeList, aHintForThisFrame);
}

#ifdef ACCESSIBILITY
a11y::AccType
nsHTMLCanvasFrame::AccessibleType()
{
  return a11y::eHTMLCanvasType;
}
#endif

#ifdef DEBUG_FRAME_DUMP
nsresult
nsHTMLCanvasFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLCanvas"), aResult);
}
#endif

