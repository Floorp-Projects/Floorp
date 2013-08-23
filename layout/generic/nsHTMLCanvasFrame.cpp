/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <canvas> element */

#include "nsGkAtoms.h"
#include "nsHTMLCanvasFrame.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include "Layers.h"

#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

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
                                   bool* aSnap) {
    *aSnap = false;
    nsIFrame* f = Frame();
    HTMLCanvasElement *canvas =
      HTMLCanvasElement::FromContent(f->GetContent());
    nsRegion result;
    if (canvas->GetIsOpaque()) {
      result = GetBounds(aBuilder, aSnap);
    }
    return result;
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
    *aSnap = true;
    nsHTMLCanvasFrame* f = static_cast<nsHTMLCanvasFrame*>(Frame());
    return f->GetInnerArea() + ToReferenceFrame();
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters)
  {
    return static_cast<nsHTMLCanvasFrame*>(mFrame)->
      BuildLayer(aBuilder, aManager, this, aContainerParameters);
  }
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const FrameLayerBuilder::ContainerParameters& aParameters)
  {
    if (HTMLCanvasElement::FromContent(mFrame->GetContent())->ShouldForceInactiveLayer(aManager))
      return LAYER_INACTIVE;

    // If compositing is cheap, just do that
    if (aManager->IsCompositingCheap())
      return mozilla::LAYER_ACTIVE;

    return mFrame->AreLayersMarkedActive() ? LAYER_ACTIVE : LAYER_INACTIVE;
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
nsHTMLCanvasFrame::Init(nsIContent* aContent,
                        nsIFrame*   aParent,
                        nsIFrame*   aPrevInFlow)
{
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  // We can fill in the canvas before the canvas frame is created, in
  // which case we never get around to marking the layer active. Therefore,
  // we mark it active here when we create the frame.
  MarkLayersActive(nsChangeHint(0));
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
  } else {
    NS_NOTREACHED("couldn't get canvas size");
  }

  return size;
}

/* virtual */ nscoord
nsHTMLCanvasFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  nscoord result = nsPresContext::CSSPixelsToAppUnits(GetCanvasSize().width);
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsHTMLCanvasFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  // XXX The caller doesn't account for constraints of the height,
  // min-height, and max-height properties.
  nscoord result = nsPresContext::CSSPixelsToAppUnits(GetCanvasSize().width);
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

/* virtual */ nsSize
nsHTMLCanvasFrame::GetIntrinsicRatio()
{
  nsIntSize size(GetCanvasSize());
  return nsSize(nsPresContext::CSSPixelsToAppUnits(size.width),
                nsPresContext::CSSPixelsToAppUnits(size.height));
}

/* virtual */ nsSize
nsHTMLCanvasFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                               nsSize aCBSize, nscoord aAvailableWidth,
                               nsSize aMargin, nsSize aBorder, nsSize aPadding,
                               uint32_t aFlags)
{
  nsIntSize size = GetCanvasSize();

  IntrinsicSize intrinsicSize;
  intrinsicSize.width.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(size.width));
  intrinsicSize.height.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(size.height));

  nsSize intrinsicRatio = GetIntrinsicRatio(); // won't actually be used

  return nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(
                            aRenderingContext, this,
                            intrinsicSize, intrinsicRatio, aCBSize,
                            aMargin, aBorder, aPadding);
}

NS_IMETHODIMP
nsHTMLCanvasFrame::Reflow(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aMetrics,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLCanvasFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsHTMLCanvasFrame::Reflow: availSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus = NS_FRAME_COMPLETE;

  aMetrics.width = aReflowState.ComputedWidth();
  aMetrics.height = aReflowState.ComputedHeight();

  // stash this away so we can compute our inner area later
  mBorderPadding   = aReflowState.mComputedBorderPadding;

  aMetrics.width += mBorderPadding.left + mBorderPadding.right;
  aMetrics.height += mBorderPadding.top + mBorderPadding.bottom;

  if (GetPrevInFlow()) {
    nscoord y = GetContinuationOffset(&aMetrics.width);
    aMetrics.height -= y + mBorderPadding.top;
    aMetrics.height = std::max(0, aMetrics.height);
  }

  aMetrics.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aMetrics);

  // Reflow the single anon block child.
  nsReflowStatus childStatus;
  nsSize availSize(aReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
  nsIFrame* childFrame = mFrames.FirstChild();
  NS_ASSERTION(!childFrame->GetNextSibling(), "HTML canvas should have 1 kid");
  nsHTMLReflowMetrics childDesiredSize(aMetrics.mFlags);
  nsHTMLReflowState childReflowState(aPresContext, aReflowState, childFrame,
                                     availSize);
  ReflowChild(childFrame, aPresContext, childDesiredSize, childReflowState,
              0, 0, 0, childStatus, nullptr);
  FinishReflowChild(childFrame, aPresContext, &childReflowState,
                    childDesiredSize, 0, 0, 0);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsHTMLCanvasFrame::Reflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);

  return NS_OK;
}

// FIXME taken from nsImageFrame, but then had splittable frame stuff
// removed.  That needs to be fixed.
nsRect 
nsHTMLCanvasFrame::GetInnerArea() const
{
  nsRect r;
  r.x = mBorderPadding.left;
  r.y = mBorderPadding.top;
  r.width = mRect.width - mBorderPadding.left - mBorderPadding.right;
  r.height = mRect.height - mBorderPadding.top - mBorderPadding.bottom;
  return r;
}

already_AddRefed<Layer>
nsHTMLCanvasFrame::BuildLayer(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              nsDisplayItem* aItem,
                              const ContainerParameters& aContainerParameters)
{
  nsRect area = GetContentRect() - GetPosition() + aItem->ToReferenceFrame();
  HTMLCanvasElement* element = static_cast<HTMLCanvasElement*>(GetContent());
  nsIntSize canvasSize = GetCanvasSize();

  nsPresContext* presContext = PresContext();
  element->HandlePrintCallback(presContext->Type());

  if (canvasSize.width <= 0 || canvasSize.height <= 0 || area.IsEmpty())
    return nullptr;

  CanvasLayer* oldLayer = static_cast<CanvasLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem));
  nsRefPtr<CanvasLayer> layer = element->GetCanvasLayer(aBuilder, oldLayer, aManager);
  if (!layer)
    return nullptr;

  gfxRect r = gfxRect(presContext->AppUnitsToGfxUnits(area.x),
                      presContext->AppUnitsToGfxUnits(area.y),
                      presContext->AppUnitsToGfxUnits(area.width),
                      presContext->AppUnitsToGfxUnits(area.height));

  // Transform the canvas into the right place
  gfxMatrix transform;
  transform.Translate(r.TopLeft() + aContainerParameters.mOffset);
  transform.Scale(r.Width()/canvasSize.width, r.Height()/canvasSize.height);
  layer->SetBaseTransform(gfx3DMatrix::From2D(transform));
  layer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(this));
  layer->SetVisibleRegion(nsIntRect(0, 0, canvasSize.width, canvasSize.height));

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

  DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox
    clip(aBuilder, this, DisplayListClipState::ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT);

  aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplayCanvas(aBuilder, this));

  DisplaySelectionOverlay(aBuilder, aLists.Content(),
                          nsISelectionDisplay::DISPLAY_IMAGES);
}

nsIAtom*
nsHTMLCanvasFrame::GetType() const
{
  return nsGkAtoms::HTMLCanvasFrame;
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
    offset -= mBorderPadding.top;
    offset = std::max(0, offset);
  }
  return offset;
}

#ifdef ACCESSIBILITY
a11y::AccType
nsHTMLCanvasFrame::AccessibleType()
{
  return a11y::eHTMLCanvasType;
}
#endif

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLCanvasFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLCanvas"), aResult);
}
#endif

