/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsNameSpaceManager.h"
#include "nsBoxLayoutState.h"
#include "nsWidgetsCID.h"
#include "nsViewManager.h"
#include "nsContainerFrame.h"
#include "nsDisplayList.h"
#include <algorithm>

using namespace mozilla;

//
// NS_NewLeafBoxFrame
//
// Creates a new Toolbar frame and returns it
//
nsIFrame*
NS_NewLeafBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsLeafBoxFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsLeafBoxFrame)

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
void
nsLeafBoxFrame::Init(nsIContent*       aContent,
                     nsContainerFrame* aParent,
                     nsIFrame*         aPrevInFlow)
{
  nsLeafFrame::Init(aContent, aParent, aPrevInFlow);

  if (GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER) {
    AddStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT);
  }

  UpdateMouseThrough();
}

nsresult
nsLeafBoxFrame::AttributeChanged(int32_t aNameSpaceID,
                                 nsAtom* aAttribute,
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

void
nsLeafBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                 const nsDisplayListSet& aLists)
{
  // REVIEW: GetFrameForPoint used to not report events for the background
  // layer, whereas this code will put an event receiver for this frame in the
  // BlockBorderBackground() list. But I don't see any need to preserve
  // that anomalous behaviour. The important thing I'm preserving is that
  // leaf boxes continue to receive events in the foreground layer.
  DisplayBorderBackgroundOutline(aBuilder, aLists);

  if (!aBuilder->IsForEventDelivery() || !IsVisibleForPainting(aBuilder))
    return;

  aLists.Content()->AppendToTop(new (aBuilder)
    nsDisplayEventReceiver(aBuilder, this));
}

/* virtual */ nscoord
nsLeafBoxFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  nsBoxLayoutState state(PresContext(), aRenderingContext);

  WritingMode wm = GetWritingMode();
  LogicalSize minSize(wm, GetXULMinSize(state));

  // GetXULMinSize returns border-box size, and we want to return content
  // inline-size.  Since Reflow uses the reflow state's border and padding, we
  // actually just want to subtract what GetXULMinSize added, which is the
  // result of GetXULBorderAndPadding.
  nsMargin bp;
  GetXULBorderAndPadding(bp);

  result = minSize.ISize(wm) - LogicalMargin(wm, bp).IStartEnd(wm);

  return result;
}

/* virtual */ nscoord
nsLeafBoxFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  nsBoxLayoutState state(PresContext(), aRenderingContext);

  WritingMode wm = GetWritingMode();
  LogicalSize prefSize(wm, GetXULPrefSize(state));

  // GetXULPrefSize returns border-box size, and we want to return content
  // inline-size.  Since Reflow uses the reflow state's border and padding, we
  // actually just want to subtract what GetXULPrefSize added, which is the
  // result of GetXULBorderAndPadding.
  nsMargin bp;
  GetXULBorderAndPadding(bp);

  result = prefSize.ISize(wm) - LogicalMargin(wm, bp).IStartEnd(wm);

  return result;
}

nscoord
nsLeafBoxFrame::GetIntrinsicISize()
{
  // No intrinsic width
  return 0;
}

LogicalSize
nsLeafBoxFrame::ComputeAutoSize(gfxContext*         aRenderingContext,
                                WritingMode         aWM,
                                const LogicalSize&  aCBSize,
                                nscoord             aAvailableISize,
                                const LogicalSize&  aMargin,
                                const LogicalSize&  aBorder,
                                const LogicalSize&  aPadding,
                                ComputeSizeFlags    aFlags)
{
  // Important: NOT calling our direct superclass here!
  return nsFrame::ComputeAutoSize(aRenderingContext, aWM,
                                  aCBSize, aAvailableISize,
                                  aMargin, aBorder, aPadding, aFlags);
}

void
nsLeafBoxFrame::Reflow(nsPresContext*   aPresContext,
                     ReflowOutput&     aDesiredSize,
                     const ReflowInput& aReflowInput,
                     nsReflowStatus&          aStatus)
{
  // This is mostly a copy of nsBoxFrame::Reflow().
  // We aren't able to share an implementation because of the frame
  // class hierarchy.  If you make changes here, please keep
  // nsBoxFrame::Reflow in sync.

  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsLeafBoxFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  NS_ASSERTION(aReflowInput.ComputedWidth() >=0 &&
               aReflowInput.ComputedHeight() >= 0, "Computed Size < 0");

#ifdef DO_NOISY_REFLOW
  printf("\n-------------Starting LeafBoxFrame Reflow ----------------------------\n");
  printf("%p ** nsLBF::Reflow %d R: ", this, myCounter++);
  switch (aReflowInput.reason) {
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
    default:printf("<unknown>%d", aReflowInput.reason);break;
  }

  printSize("AW", aReflowInput.AvailableWidth());
  printSize("AH", aReflowInput.AvailableHeight());
  printSize("CW", aReflowInput.ComputedWidth());
  printSize("CH", aReflowInput.ComputedHeight());

  printf(" *\n");

#endif

  // create the layout state
  nsBoxLayoutState state(aPresContext, aReflowInput.mRenderingContext);

  nsSize computedSize(aReflowInput.ComputedWidth(),aReflowInput.ComputedHeight());

  nsMargin m;
  m = aReflowInput.ComputedPhysicalBorderPadding();

  //GetXULBorderAndPadding(m);

  // this happens sometimes. So lets handle it gracefully.
  if (aReflowInput.ComputedHeight() == 0) {
    nsSize minSize = GetXULMinSize(state);
    computedSize.height = minSize.height - m.top - m.bottom;
  }

  nsSize prefSize(0,0);

  // if we are told to layout intrinic then get our preferred size.
  if (computedSize.width == NS_INTRINSICSIZE || computedSize.height == NS_INTRINSICSIZE) {
     prefSize = GetXULPrefSize(state);
     nsSize minSize = GetXULMinSize(state);
     nsSize maxSize = GetXULMaxSize(state);
     prefSize = BoundsCheck(minSize, prefSize, maxSize);
  }

  // get our desiredSize
  if (aReflowInput.ComputedWidth() == NS_INTRINSICSIZE) {
    computedSize.width = prefSize.width;
  } else {
    computedSize.width += m.left + m.right;
  }

  if (aReflowInput.ComputedHeight() == NS_INTRINSICSIZE) {
    computedSize.height = prefSize.height;
  } else {
    computedSize.height += m.top + m.bottom;
  }

  // handle reflow state min and max sizes
  // XXXbz the width handling here seems to be wrong, since
  // mComputedMin/MaxWidth is a content-box size, whole
  // computedSize.width is a border-box size...
  if (computedSize.width > aReflowInput.ComputedMaxWidth())
    computedSize.width = aReflowInput.ComputedMaxWidth();

  if (computedSize.width < aReflowInput.ComputedMinWidth())
    computedSize.width = aReflowInput.ComputedMinWidth();

  // Now adjust computedSize.height for our min and max computed
  // height.  The only problem is that those are content-box sizes,
  // while computedSize.height is a border-box size.  So subtract off
  // m.TopBottom() before adjusting, then readd it.
  computedSize.height = std::max(0, computedSize.height - m.TopBottom());
  computedSize.height = NS_CSS_MINMAX(computedSize.height,
                                      aReflowInput.ComputedMinHeight(),
                                      aReflowInput.ComputedMaxHeight());
  computedSize.height += m.TopBottom();

  nsRect r(mRect.x, mRect.y, computedSize.width, computedSize.height);

  SetXULBounds(state, r);

  // layout our children
  XULLayout(state);

  // ok our child could have gotten bigger. So lets get its bounds
  aDesiredSize.Width() = mRect.width;
  aDesiredSize.Height() = mRect.height;
  aDesiredSize.SetBlockStartAscent(GetXULBoxAscent(state));

  // the overflow rect is set in SetXULBounds() above
  aDesiredSize.mOverflowAreas = GetOverflowAreas();

#ifdef DO_NOISY_REFLOW
  {
    printf("%p ** nsLBF(done) W:%d H:%d  ", this, aDesiredSize.Width(), aDesiredSize.Height());

    if (maxElementWidth) {
      printf("MW:%d\n", *maxElementWidth);
    } else {
      printf("MW:?\n");
    }

  }
#endif
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsLeafBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("LeafBox"), aResult);
}
#endif

nsresult
nsLeafBoxFrame::CharacterDataChanged(CharacterDataChangeInfo* aInfo)
{
  MarkIntrinsicISizesDirty();
  return nsLeafFrame::CharacterDataChanged(aInfo);
}

/* virtual */ nsSize
nsLeafBoxFrame::GetXULPrefSize(nsBoxLayoutState& aState)
{
    return nsBox::GetXULPrefSize(aState);
}

/* virtual */ nsSize
nsLeafBoxFrame::GetXULMinSize(nsBoxLayoutState& aState)
{
    return nsBox::GetXULMinSize(aState);
}

/* virtual */ nsSize
nsLeafBoxFrame::GetXULMaxSize(nsBoxLayoutState& aState)
{
    return nsBox::GetXULMaxSize(aState);
}

/* virtual */ nscoord
nsLeafBoxFrame::GetXULFlex()
{
    return nsBox::GetXULFlex();
}

/* virtual */ nscoord
nsLeafBoxFrame::GetXULBoxAscent(nsBoxLayoutState& aState)
{
    return nsBox::GetXULBoxAscent(aState);
}

/* virtual */ void
nsLeafBoxFrame::MarkIntrinsicISizesDirty()
{
  // Don't call base class method, since everything it does is within an
  // IsXULBoxWrapped check.
}

NS_IMETHODIMP
nsLeafBoxFrame::DoXULLayout(nsBoxLayoutState& aState)
{
    return nsBox::DoXULLayout(aState);
}
