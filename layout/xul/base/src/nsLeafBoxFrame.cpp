/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsLeafBoxFrame.h"
#include "nsBoxFrame.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsBoxLayoutState.h"
#include "nsWidgetsCID.h"
#include "nsViewManager.h"
#include "nsContainerFrame.h"
#include "nsDisplayList.h"

//
// NS_NewLeafBoxFrame
//
// Creates a new Toolbar frame and returns it
//
nsIFrame*
NS_NewLeafBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsLeafBoxFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsLeafBoxFrame)

nsLeafBoxFrame::nsLeafBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext)
    : nsLeafFrame(aContext)
{
}

#ifdef DEBUG_LAYOUT
void
nsLeafBoxFrame::GetBoxName(nsAutoString& aName)
{
   GetFrameName(aName);
}
#endif


/**
 * Initialize us. This is a good time to get the alignment of the box
 */
NS_IMETHODIMP
nsLeafBoxFrame::Init(
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsLeafFrame::Init(aContent, aParent, aPrevInFlow);
  NS_ENSURE_SUCCESS(rv, rv);

  if (GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER) {
    AddStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  }

  UpdateMouseThrough();

  return rv;
}

NS_IMETHODIMP
nsLeafBoxFrame::AttributeChanged(int32_t aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 int32_t aModType)
{
  nsresult rv = nsLeafFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                              aModType);

  if (aAttribute == nsGkAtoms::mousethrough) 
    UpdateMouseThrough();

  return rv;
}

void nsLeafBoxFrame::UpdateMouseThrough()
{
  if (mContent) {
    static nsIContent::AttrValuesArray strings[] =
      {&nsGkAtoms::never, &nsGkAtoms::always, nullptr};
    switch (mContent->FindAttrValueIn(kNameSpaceID_None,
                                      nsGkAtoms::mousethrough,
                                      strings, eCaseMatters)) {
      case 0: AddStateBits(NS_FRAME_MOUSE_THROUGH_NEVER); break;
      case 1: AddStateBits(NS_FRAME_MOUSE_THROUGH_ALWAYS); break;
      case 2: {
          RemoveStateBits(NS_FRAME_MOUSE_THROUGH_ALWAYS);
          RemoveStateBits(NS_FRAME_MOUSE_THROUGH_NEVER);
          break;
      }
    }
  }
}

NS_IMETHODIMP
nsLeafBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                 const nsRect&           aDirtyRect,
                                 const nsDisplayListSet& aLists)
{
  // REVIEW: GetFrameForPoint used to not report events for the background
  // layer, whereas this code will put an event receiver for this frame in the
  // BlockBorderBackground() list. But I don't see any need to preserve
  // that anomalous behaviour. The important thing I'm preserving is that
  // leaf boxes continue to receive events in the foreground layer.
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aBuilder->IsForEventDelivery() || !IsVisibleForPainting(aBuilder))
    return NS_OK;

  return aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayEventReceiver(aBuilder, this));
}

/* virtual */ nscoord
nsLeafBoxFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  nsBoxLayoutState state(PresContext(), aRenderingContext);
  nsSize minSize = GetMinSize(state);

  // GetMinSize returns border-box width, and we want to return content
  // width.  Since Reflow uses the reflow state's border and padding, we
  // actually just want to subtract what GetMinSize added, which is the
  // result of GetBorderAndPadding.
  nsMargin bp;
  GetBorderAndPadding(bp);

  result = minSize.width - bp.LeftRight();

  return result;
}

/* virtual */ nscoord
nsLeafBoxFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  nsBoxLayoutState state(PresContext(), aRenderingContext);
  nsSize prefSize = GetPrefSize(state);

  // GetPrefSize returns border-box width, and we want to return content
  // width.  Since Reflow uses the reflow state's border and padding, we
  // actually just want to subtract what GetPrefSize added, which is the
  // result of GetBorderAndPadding.
  nsMargin bp;
  GetBorderAndPadding(bp);

  result = prefSize.width - bp.LeftRight();

  return result;
}

nscoord
nsLeafBoxFrame::GetIntrinsicWidth()
{
  // No intrinsic width
  return 0;
}

nsSize
nsLeafBoxFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                nsSize aCBSize, nscoord aAvailableWidth,
                                nsSize aMargin, nsSize aBorder,
                                nsSize aPadding, bool aShrinkWrap)
{
  // Important: NOT calling our direct superclass here!
  return nsFrame::ComputeAutoSize(aRenderingContext, aCBSize, aAvailableWidth,
                                  aMargin, aBorder, aPadding, aShrinkWrap);
}

NS_IMETHODIMP
nsLeafBoxFrame::Reflow(nsPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  // This is mostly a copy of nsBoxFrame::Reflow().
  // We aren't able to share an implementation because of the frame
  // class hierarchy.  If you make changes here, please keep
  // nsBoxFrame::Reflow in sync.

  DO_GLOBAL_REFLOW_COUNT("nsLeafBoxFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(aReflowState.ComputedWidth() >=0 &&
               aReflowState.ComputedHeight() >= 0, "Computed Size < 0");

#ifdef DO_NOISY_REFLOW
  printf("\n-------------Starting LeafBoxFrame Reflow ----------------------------\n");
  printf("%p ** nsLBF::Reflow %d R: ", this, myCounter++);
  switch (aReflowState.reason) {
    case eReflowReason_Initial:
      printf("Ini");break;
    case eReflowReason_Incremental:
      printf("Inc");break;
    case eReflowReason_Resize:
      printf("Rsz");break;
    case eReflowReason_StyleChange:
      printf("Sty");break;
    case eReflowReason_Dirty:
      printf("Drt ");
      break;
    default:printf("<unknown>%d", aReflowState.reason);break;
  }
  
  printSize("AW", aReflowState.availableWidth);
  printSize("AH", aReflowState.availableHeight);
  printSize("CW", aReflowState.ComputedWidth());
  printSize("CH", aReflowState.ComputedHeight());

  printf(" *\n");

#endif

  aStatus = NS_FRAME_COMPLETE;

  // create the layout state
  nsBoxLayoutState state(aPresContext, aReflowState.rendContext);

  nsSize computedSize(aReflowState.ComputedWidth(),aReflowState.ComputedHeight());

  nsMargin m;
  m = aReflowState.mComputedBorderPadding;

  //GetBorderAndPadding(m);

  // this happens sometimes. So lets handle it gracefully.
  if (aReflowState.ComputedHeight() == 0) {
    nsSize minSize = GetMinSize(state);
    computedSize.height = minSize.height - m.top - m.bottom;
  }

  nsSize prefSize(0,0);

  // if we are told to layout intrinic then get our preferred size.
  if (computedSize.width == NS_INTRINSICSIZE || computedSize.height == NS_INTRINSICSIZE) {
     prefSize = GetPrefSize(state);
     nsSize minSize = GetMinSize(state);
     nsSize maxSize = GetMaxSize(state);
     prefSize = BoundsCheck(minSize, prefSize, maxSize);
  }

  // get our desiredSize
  if (aReflowState.ComputedWidth() == NS_INTRINSICSIZE) {
    computedSize.width = prefSize.width;
  } else {
    computedSize.width += m.left + m.right;
  }

  if (aReflowState.ComputedHeight() == NS_INTRINSICSIZE) {
    computedSize.height = prefSize.height;
  } else {
    computedSize.height += m.top + m.bottom;
  }

  // handle reflow state min and max sizes
  // XXXbz the width handling here seems to be wrong, since
  // mComputedMin/MaxWidth is a content-box size, whole
  // computedSize.width is a border-box size...
  if (computedSize.width > aReflowState.mComputedMaxWidth)
    computedSize.width = aReflowState.mComputedMaxWidth;

  if (computedSize.width < aReflowState.mComputedMinWidth)
    computedSize.width = aReflowState.mComputedMinWidth;

  // Now adjust computedSize.height for our min and max computed
  // height.  The only problem is that those are content-box sizes,
  // while computedSize.height is a border-box size.  So subtract off
  // m.TopBottom() before adjusting, then readd it.
  computedSize.height = NS_MAX(0, computedSize.height - m.TopBottom());
  computedSize.height = NS_CSS_MINMAX(computedSize.height,
                                      aReflowState.mComputedMinHeight,
                                      aReflowState.mComputedMaxHeight);
  computedSize.height += m.TopBottom();

  nsRect r(mRect.x, mRect.y, computedSize.width, computedSize.height);

  SetBounds(state, r);
 
  // layout our children
  Layout(state);
  
  // ok our child could have gotten bigger. So lets get its bounds
  aDesiredSize.width  = mRect.width;
  aDesiredSize.height = mRect.height;
  aDesiredSize.ascent = GetBoxAscent(state);

  // the overflow rect is set in SetBounds() above
  aDesiredSize.mOverflowAreas = GetOverflowAreas();

#ifdef DO_NOISY_REFLOW
  {
    printf("%p ** nsLBF(done) W:%d H:%d  ", this, aDesiredSize.width, aDesiredSize.height);

    if (maxElementWidth) {
      printf("MW:%d\n", *maxElementWidth); 
    } else {
      printf("MW:?\n"); 
    }

  }
#endif

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsLeafBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("LeafBox"), aResult);
}
#endif

nsIAtom*
nsLeafBoxFrame::GetType() const
{
  return nsGkAtoms::leafBoxFrame;
}

NS_IMETHODIMP
nsLeafBoxFrame::CharacterDataChanged(CharacterDataChangeInfo* aInfo)
{
  MarkIntrinsicWidthsDirty();
  return nsLeafFrame::CharacterDataChanged(aInfo);
}

/* virtual */ nsSize
nsLeafBoxFrame::GetPrefSize(nsBoxLayoutState& aState)
{
    return nsBox::GetPrefSize(aState);
}

/* virtual */ nsSize
nsLeafBoxFrame::GetMinSize(nsBoxLayoutState& aState)
{
    return nsBox::GetMinSize(aState);
}

/* virtual */ nsSize
nsLeafBoxFrame::GetMaxSize(nsBoxLayoutState& aState)
{
    return nsBox::GetMaxSize(aState);
}

/* virtual */ nscoord
nsLeafBoxFrame::GetFlex(nsBoxLayoutState& aState)
{
    return nsBox::GetFlex(aState);
}

/* virtual */ nscoord
nsLeafBoxFrame::GetBoxAscent(nsBoxLayoutState& aState)
{
    return nsBox::GetBoxAscent(aState);
}

/* virtual */ void
nsLeafBoxFrame::MarkIntrinsicWidthsDirty()
{
  // Don't call base class method, since everything it does is within an
  // IsBoxWrapped check.
}

NS_IMETHODIMP
nsLeafBoxFrame::DoLayout(nsBoxLayoutState& aState)
{
    return nsBox::DoLayout(aState);
}
