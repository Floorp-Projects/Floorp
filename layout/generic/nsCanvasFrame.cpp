/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object that goes directly inside the document's scrollbars */

#include "nsCanvasFrame.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsDisplayList.h"
#include "nsCSSFrameConstructor.h"
#include "nsFrameManager.h"
#include "gfxPlatform.h"

// for focus
#include "nsIScrollableFrame.h"
#ifdef DEBUG_CANVAS_FOCUS
#include "nsIDocShell.h"
#endif

//#define DEBUG_CANVAS_FOCUS

using namespace mozilla;
using namespace mozilla::layout;
using namespace mozilla::gfx;

nsIFrame*
NS_NewCanvasFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsCanvasFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsCanvasFrame)

NS_QUERYFRAME_HEAD(nsCanvasFrame)
  NS_QUERYFRAME_ENTRY(nsCanvasFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void
nsCanvasFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsIScrollableFrame* sf =
    PresContext()->GetPresShell()->GetRootScrollFrameAsScrollable();
  if (sf) {
    sf->RemoveScrollPositionListener(this);
  }

  nsContainerFrame::DestroyFrom(aDestructRoot);
}

void
nsCanvasFrame::ScrollPositionWillChange(nscoord aX, nscoord aY)
{
  if (mDoPaintFocus) {
    mDoPaintFocus = false;
    PresContext()->FrameManager()->GetRootFrame()->InvalidateFrameSubtree();
  }
}

NS_IMETHODIMP
nsCanvasFrame::SetHasFocus(bool aHasFocus)
{
  if (mDoPaintFocus != aHasFocus) {
    mDoPaintFocus = aHasFocus;
    PresContext()->FrameManager()->GetRootFrame()->InvalidateFrameSubtree();

    if (!mAddedScrollPositionListener) {
      nsIScrollableFrame* sf =
        PresContext()->GetPresShell()->GetRootScrollFrameAsScrollable();
      if (sf) {
        sf->AddScrollPositionListener(this);
        mAddedScrollPositionListener = true;
      }
    }
  }
  return NS_OK;
}

nsresult
nsCanvasFrame::SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList)
{
  NS_ASSERTION(aListID != kPrincipalList ||
               aChildList.IsEmpty() || aChildList.OnlyChild(),
               "Primary child list can have at most one frame in it");
  return nsContainerFrame::SetInitialChildList(aListID, aChildList);
}

nsresult
nsCanvasFrame::AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList ||
               aListID == kAbsoluteList, "unexpected child list ID");
  NS_PRECONDITION(aListID != kAbsoluteList ||
                  mFrames.IsEmpty(), "already have a child frame");
  if (aListID != kPrincipalList) {
    // We only support the Principal and Absolute child lists.
    return NS_ERROR_INVALID_ARG;
  }

  if (!mFrames.IsEmpty()) {
    // We only allow a single principal child frame.
    return NS_ERROR_INVALID_ARG;
  }

  // Insert the new frames
  NS_ASSERTION(aFrameList.FirstChild() == aFrameList.LastChild(),
               "Only one principal child frame allowed");
#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mFrames.AppendFrames(nullptr, aFrameList);

  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);

  return NS_OK;
}

nsresult
nsCanvasFrame::InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList)
{
  // Because we only support a single child frame inserting is the same
  // as appending
  NS_PRECONDITION(!aPrevFrame, "unexpected previous sibling frame");
  if (aPrevFrame)
    return NS_ERROR_UNEXPECTED;

  return AppendFrames(aListID, aFrameList);
}

nsresult
nsCanvasFrame::RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList ||
               aListID == kAbsoluteList, "unexpected child list ID");
  if (aListID != kPrincipalList && aListID != kAbsoluteList) {
    // We only support the Principal and Absolute child lists.
    return NS_ERROR_INVALID_ARG;
  }

  if (aOldFrame != mFrames.FirstChild())
    return NS_ERROR_FAILURE;

  // Remove the frame and destroy it
  mFrames.DestroyFrame(aOldFrame);

  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
  return NS_OK;
}

nsRect nsCanvasFrame::CanvasArea() const
{
  // Not clear which overflow rect we want here, but it probably doesn't
  // matter.
  nsRect result(GetVisualOverflowRect());

  nsIScrollableFrame *scrollableFrame = do_QueryFrame(GetParent());
  if (scrollableFrame) {
    nsRect portRect = scrollableFrame->GetScrollPortRect();
    result.UnionRect(result, nsRect(nsPoint(0, 0), portRect.Size()));
  }
  return result;
}

void
nsDisplayCanvasBackgroundColor::Paint(nsDisplayListBuilder* aBuilder,
                                      nsRenderingContext* aCtx)
{
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;
  if (NS_GET_A(mColor) > 0) {
    aCtx->SetColor(mColor);
    aCtx->FillRect(bgClipRect);
  }
}

static void BlitSurface(gfxContext* aDest, const gfxRect& aRect, gfxASurface* aSource)
{
  aDest->Translate(gfxPoint(aRect.x, aRect.y));
  aDest->SetSource(aSource);
  aDest->NewPath();
  aDest->Rectangle(gfxRect(0, 0, aRect.width, aRect.height));
  aDest->Fill();
  aDest->Translate(-gfxPoint(aRect.x, aRect.y));
}

static void BlitSurface(DrawTarget* aDest, const gfxRect& aRect, DrawTarget* aSource)
{
  RefPtr<SourceSurface> source = aSource->Snapshot();
  aDest->DrawSurface(source,
                     Rect(aRect.x, aRect.y, aRect.width, aRect.height),
                     Rect(0, 0, aRect.width, aRect.height));
}

void
nsDisplayCanvasBackgroundImage::Paint(nsDisplayListBuilder* aBuilder,
                                      nsRenderingContext* aCtx)
{
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;

  nsRenderingContext context;
  nsRefPtr<gfxContext> dest = aCtx->ThebesContext();
  nsRefPtr<gfxASurface> surf;
  RefPtr<DrawTarget> dt;
  nsRefPtr<gfxContext> ctx;
  gfxRect destRect;
#ifndef MOZ_GFX_OPTIMIZE_MOBILE
  if (IsSingleFixedPositionImage(aBuilder, bgClipRect, &destRect) &&
      aBuilder->IsPaintingToWindow() && !aBuilder->IsCompositingCheap() &&
      !dest->CurrentMatrix().HasNonIntegerTranslation()) {
    // Snap image rectangle to nearest pixel boundaries. This is the right way
    // to snap for this context, because we checked HasNonIntegerTranslation above.
    destRect.Round();
    if (dest->IsCairo()) {
      surf = static_cast<gfxASurface*>(Frame()->Properties().Get(nsIFrame::CachedBackgroundImage()));
      nsRefPtr<gfxASurface> destSurf = dest->CurrentSurface();
      if (surf && surf->GetType() == destSurf->GetType()) {
        BlitSurface(dest, destRect, surf);
        return;
      }
      surf = destSurf->CreateSimilarSurface(
          gfxContentType::COLOR_ALPHA,
          gfxIntSize(ceil(destRect.width), ceil(destRect.height)));
    } else {
      dt = static_cast<DrawTarget*>(Frame()->Properties().Get(nsIFrame::CachedBackgroundImageDT()));
      DrawTarget* destDT = dest->GetDrawTarget();
      if (dt) {
        BlitSurface(destDT, destRect, dt);
        return;
      }
      dt = destDT->CreateSimilarDrawTarget(IntSize(ceil(destRect.width), ceil(destRect.height)), SurfaceFormat::B8G8R8A8);
    }
    if (surf || dt) {
      if (surf) {
        ctx = new gfxContext(surf);
      } else {
        ctx = new gfxContext(dt);
      }
      ctx->Translate(-gfxPoint(destRect.x, destRect.y));
      context.Init(aCtx->DeviceContext(), ctx);
    }
  }
#endif

  PaintInternal(aBuilder,
                (surf || dt) ? &context : aCtx,
                (surf || dt) ? bgClipRect: mVisibleRect,
                &bgClipRect);

  if (surf) {
    BlitSurface(dest, destRect, surf);
    frame->Properties().Set(nsIFrame::CachedBackgroundImage(),
                            surf.forget().take());
  }
  if (dt) {
    BlitSurface(dest->GetDrawTarget(), destRect, dt);
    frame->Properties().Set(nsIFrame::CachedBackgroundImageDT(), dt.forget().drop());
  }
}

void
nsDisplayCanvasThemedBackground::Paint(nsDisplayListBuilder* aBuilder,
                                       nsRenderingContext* aCtx)
{
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;

  PaintInternal(aBuilder, aCtx, mVisibleRect, &bgClipRect);
}

/**
 * A display item to paint the focus ring for the document.
 *
 * The only reason this can't use nsDisplayGeneric is overriding GetBounds.
 */
class nsDisplayCanvasFocus : public nsDisplayItem {
public:
  nsDisplayCanvasFocus(nsDisplayListBuilder* aBuilder, nsCanvasFrame *aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayCanvasFocus);
  }
  virtual ~nsDisplayCanvasFocus() {
    MOZ_COUNT_DTOR(nsDisplayCanvasFocus);
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) MOZ_OVERRIDE
  {
    *aSnap = false;
    // This is an overestimate, but that's not a problem.
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    return frame->CanvasArea() + ToReferenceFrame();
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE
  {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    frame->PaintFocus(*aCtx, ToReferenceFrame());
  }

  NS_DISPLAY_DECL_NAME("CanvasFocus", TYPE_CANVAS_FOCUS)
};

void
nsCanvasFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, aDirtyRect, aLists);
  }

  // Force a background to be shown. We may have a background propagated to us,
  // in which case StyleBackground wouldn't have the right background
  // and the code in nsFrame::DisplayBorderBackgroundOutline might not give us
  // a background.
  // We don't have any border or outline, and our background draws over
  // the overflow area, so just add nsDisplayCanvasBackground instead of
  // calling DisplayBorderBackgroundOutline.
  if (IsVisibleForPainting(aBuilder)) {
    nsStyleContext* bgSC;
    const nsStyleBackground* bg = nullptr;
    bool isThemed = IsThemed();
    if (!isThemed && nsCSSRendering::FindBackground(this, &bgSC)) {
      bg = bgSC->StyleBackground();
    }
    aLists.BorderBackground()->AppendNewToTop(
        new (aBuilder) nsDisplayCanvasBackgroundColor(aBuilder, this));
  
    if (isThemed) {
      aLists.BorderBackground()->AppendNewToTop(
        new (aBuilder) nsDisplayCanvasThemedBackground(aBuilder, this));
      return;
    }

    if (!bg) {
      return;
    }

    // Create separate items for each background layer.
    NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
      if (bg->mLayers[i].mImage.IsEmpty()) {
        continue;
      }
      aLists.BorderBackground()->AppendNewToTop(
        new (aBuilder) nsDisplayCanvasBackgroundImage(aBuilder, this, i, bg));
    }
  }

  nsIFrame* kid;
  for (kid = GetFirstPrincipalChild(); kid; kid = kid->GetNextSibling()) {
    // Put our child into its own pseudo-stack.
    BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
  }

#ifdef DEBUG_CANVAS_FOCUS
  nsCOMPtr<nsIContent> focusContent;
  aPresContext->EventStateManager()->
    GetFocusedContent(getter_AddRefs(focusContent));

  bool hasFocus = false;
  nsCOMPtr<nsISupports> container;
  aPresContext->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
  if (docShell) {
    docShell->GetHasFocus(&hasFocus);
    printf("%p - nsCanvasFrame::Paint R:%d,%d,%d,%d  DR: %d,%d,%d,%d\n", this, 
            mRect.x, mRect.y, mRect.width, mRect.height,
            aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  }
  printf("%p - Focus: %s   c: %p  DoPaint:%s\n", docShell.get(), hasFocus?"Y":"N", 
         focusContent.get(), mDoPaintFocus?"Y":"N");
#endif

  if (!mDoPaintFocus)
    return;
  // Only paint the focus if we're visible
  if (!StyleVisibility()->IsVisible())
    return;
  
  aLists.Outlines()->AppendNewToTop(new (aBuilder)
    nsDisplayCanvasFocus(aBuilder, this));
}

void
nsCanvasFrame::PaintFocus(nsRenderingContext& aRenderingContext, nsPoint aPt)
{
  nsRect focusRect(aPt, GetSize());

  nsIScrollableFrame *scrollableFrame = do_QueryFrame(GetParent());
  if (scrollableFrame) {
    nsRect portRect = scrollableFrame->GetScrollPortRect();
    focusRect.width = portRect.width;
    focusRect.height = portRect.height;
    focusRect.MoveBy(scrollableFrame->GetScrollPosition());
  }

 // XXX use the root frame foreground color, but should we find BODY frame
 // for HTML documents?
  nsIFrame* root = mFrames.FirstChild();
  const nsStyleColor* color = root ? root->StyleColor() : StyleColor();
  if (!color) {
    NS_ERROR("current color cannot be found");
    return;
  }

  nsCSSRendering::PaintFocus(PresContext(), aRenderingContext,
                             focusRect, color->mColor);
}

/* virtual */ nscoord
nsCanvasFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinWidth(aRenderingContext);
  return result;
}

/* virtual */ nscoord
nsCanvasFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefWidth(aRenderingContext);
  return result;
}

nsresult
nsCanvasFrame::Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsCanvasFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("nsCanvasFrame::Reflow");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  nsCanvasFrame* prevCanvasFrame = static_cast<nsCanvasFrame*>
                                               (GetPrevInFlow());
  if (prevCanvasFrame) {
    AutoFrameListPtr overflow(aPresContext,
                              prevCanvasFrame->StealOverflowFrames());
    if (overflow) {
      NS_ASSERTION(overflow->OnlyChild(),
                   "must have doc root as canvas frame's only child");
      nsContainerFrame::ReparentFrameViewList(*overflow, prevCanvasFrame, this);
      // Prepend overflow to the our child list. There may already be
      // children placeholders for fixed-pos elements, which don't get
      // reflowed but must not be lost until the canvas frame is destroyed.
      mFrames.InsertFrames(this, nullptr, *overflow);
    }
  }

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  SetSize(nsSize(aReflowState.ComputedWidth(), aReflowState.ComputedHeight())); 

  // Reflow our one and only normal child frame. It's either the root
  // element's frame or a placeholder for that frame, if the root element
  // is abs-pos or fixed-pos. We may have additional children which
  // are placeholders for continuations of fixed-pos content, but those
  // don't need to be reflowed. The normal child is always comes before
  // the fixed-pos placeholders, because we insert it at the start
  // of the child list, above.
  nsHTMLReflowMetrics kidDesiredSize(aReflowState);
  if (mFrames.IsEmpty()) {
    // We have no child frame, so return an empty size
    aDesiredSize.Width() = aDesiredSize.Height() = 0;
  } else {
    nsIFrame* kidFrame = mFrames.FirstChild();
    bool kidDirty = (kidFrame->GetStateBits() & NS_FRAME_IS_DIRTY) != 0;

    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                     nsSize(aReflowState.AvailableWidth(),
                                            aReflowState.AvailableHeight()));

    if (aReflowState.mFlags.mVResize &&
        (kidFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)) {
      // Tell our kid it's being vertically resized too.  Bit of a
      // hack for framesets.
      kidReflowState.mFlags.mVResize = true;
    }

    nsPoint kidPt(kidReflowState.ComputedPhysicalMargin().left,
                  kidReflowState.ComputedPhysicalMargin().top);

    kidReflowState.ApplyRelativePositioning(&kidPt);

    // Reflow the frame
    ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                kidPt.x, kidPt.y, 0, aStatus);

    // Complete the reflow and position and size the child frame
    FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, &kidReflowState,
                      kidPt.x, kidPt.y, 0);

    if (!NS_FRAME_IS_FULLY_COMPLETE(aStatus)) {
      nsIFrame* nextFrame = kidFrame->GetNextInFlow();
      NS_ASSERTION(nextFrame || aStatus & NS_FRAME_REFLOW_NEXTINFLOW,
        "If it's incomplete and has no nif yet, it must flag a nif reflow.");
      if (!nextFrame) {
        nextFrame = aPresContext->PresShell()->FrameConstructor()->
          CreateContinuingFrame(aPresContext, kidFrame, this);
        SetOverflowFrames(nsFrameList(nextFrame, nextFrame));
        // Root overflow containers will be normal children of
        // the canvas frame, but that's ok because there
        // aren't any other frames we need to isolate them from
        // during reflow.
      }
      if (NS_FRAME_OVERFLOW_IS_INCOMPLETE(aStatus)) {
        nextFrame->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
      }
    }

    // If the child frame was just inserted, then we're responsible for making sure
    // it repaints
    if (kidDirty) {
      // But we have a new child, which will affect our background, so
      // invalidate our whole rect.
      // Note: Even though we request to be sized to our child's size, our
      // scroll frame ensures that we are always the size of the viewport.
      // Also note: GetPosition() on a CanvasFrame is always going to return
      // (0, 0). We only want to invalidate GetRect() since Get*OverflowRect()
      // could also include overflow to our top and left (out of the viewport)
      // which doesn't need to be painted.
      nsIFrame* viewport = PresContext()->GetPresShell()->GetRootFrame();
      viewport->InvalidateFrame();
    }
    
    // Return our desired size. Normally it's what we're told, but
    // sometimes we can be given an unconstrained height (when a window
    // is sizing-to-content), and we should compute our desired height.
    aDesiredSize.Width() = aReflowState.ComputedWidth();
    if (aReflowState.ComputedHeight() == NS_UNCONSTRAINEDSIZE) {
      aDesiredSize.Height() = kidFrame->GetRect().height +
        kidReflowState.ComputedPhysicalMargin().TopBottom();
    } else {
      aDesiredSize.Height() = aReflowState.ComputedHeight();
    }

    aDesiredSize.SetOverflowAreasToDesiredBounds();
    aDesiredSize.mOverflowAreas.UnionWith(
      kidDesiredSize.mOverflowAreas + kidPt);
  }

  if (prevCanvasFrame) {
    ReflowOverflowContainerChildren(aPresContext, aReflowState,
                                    aDesiredSize.mOverflowAreas, 0,
                                    aStatus);
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowState, aStatus);

  NS_FRAME_TRACE_REFLOW_OUT("nsCanvasFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

nsIAtom*
nsCanvasFrame::GetType() const
{
  return nsGkAtoms::canvasFrame;
}

nsresult 
nsCanvasFrame::GetContentForEvent(WidgetEvent* aEvent,
                                  nsIContent** aContent)
{
  NS_ENSURE_ARG_POINTER(aContent);
  nsresult rv = nsFrame::GetContentForEvent(aEvent,
                                            aContent);
  if (NS_FAILED(rv) || !*aContent) {
    nsIFrame* kid = mFrames.FirstChild();
    if (kid) {
      rv = kid->GetContentForEvent(aEvent,
                                   aContent);
    }
  }

  return rv;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsCanvasFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Canvas"), aResult);
}
#endif
