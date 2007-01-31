/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsLeafBoxFrame.h"
#include "nsBoxFrame.h"
#include "nsCOMPtr.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsStyleContext.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsBoxLayoutState.h"
#include "nsWidgetsCID.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsDisplayList.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

//
// NS_NewLeafBoxFrame
//
// Creates a new Toolbar frame and returns it
//
nsIFrame*
NS_NewLeafBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsLeafBoxFrame(aPresShell, aContext);
} // NS_NewLeafBoxFrame

nsLeafBoxFrame::nsLeafBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext)
    : nsLeafFrame(aContext), mMouseThrough(unset)
{
    mState |= NS_FRAME_IS_BOX;
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

   // see if we need a widget
  if (aParent && aParent->IsBoxFrame()) {
    if (aParent->ChildrenMustHaveWidgets()) {
        nsHTMLContainerFrame::CreateViewForFrame(this, nsnull, PR_TRUE); 
        nsIView* view = GetView();

        if (!view->HasWidget())
           view->CreateWidget(kWidgetCID);   
    }
  }
  
  mMouseThrough = unset;

  UpdateMouseThrough();

  return rv;
}

NS_IMETHODIMP
nsLeafBoxFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 PRInt32 aModType)
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
      {&nsGkAtoms::never, &nsGkAtoms::always, nsnull};
    switch (mContent->FindAttrValueIn(kNameSpaceID_None,
                                      nsGkAtoms::mousethrough,
                                      strings, eCaseMatters)) {
      case 0: mMouseThrough = never; break;
      case 1: mMouseThrough = always; break;
    }
  }
}

PRBool
nsLeafBoxFrame::GetMouseThrough() const
{
  switch (mMouseThrough)
  {
    case always:
      return PR_TRUE;
    case never:
      return PR_FALSE;
    case unset:
      if (mParent && mParent->IsBoxFrame())
        return mParent->GetMouseThrough();
  }

  return PR_FALSE;
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
      nsDisplayEventReceiver(this));
}

/* virtual */ nscoord
nsLeafBoxFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  nsBoxLayoutState state(GetPresContext(), aRenderingContext);
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
nsLeafBoxFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  nsBoxLayoutState state(GetPresContext(), aRenderingContext);
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

  NS_ASSERTION(aReflowState.ComputedWidth() >=0 && aReflowState.mComputedHeight >= 0, "Computed Size < 0");

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
  printSize("CH", aReflowState.mComputedHeight);

  printf(" *\n");

#endif

  aStatus = NS_FRAME_COMPLETE;

  // create the layout state
  nsBoxLayoutState state(aPresContext, aReflowState.rendContext);

  nsSize computedSize(aReflowState.ComputedWidth(),aReflowState.mComputedHeight);

  nsMargin m;
  m = aReflowState.mComputedBorderPadding;

  //GetBorderAndPadding(m);

  // this happens sometimes. So lets handle it gracefully.
  if (aReflowState.mComputedHeight == 0) {
    nsSize minSize = GetMinSize(state);
    computedSize.height = minSize.height - m.top - m.bottom;
  }

  nsSize prefSize(0,0);

  // if we are told to layout intrinic then get our preferred size.
  if (computedSize.width == NS_INTRINSICSIZE || computedSize.height == NS_INTRINSICSIZE) {
     prefSize = GetPrefSize(state);
     nsSize minSize = GetMinSize(state);
     nsSize maxSize = GetMaxSize(state);
     BoundsCheck(minSize, prefSize, maxSize);
  }

  // get our desiredSize
  if (aReflowState.ComputedWidth() == NS_INTRINSICSIZE) {
    computedSize.width = prefSize.width;
  } else {
    computedSize.width += m.left + m.right;
  }

  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE) {
    computedSize.height = prefSize.height;
  } else {
    computedSize.height += m.top + m.bottom;
  }

  // handle reflow state min and max sizes

  if (computedSize.width > aReflowState.mComputedMaxWidth)
    computedSize.width = aReflowState.mComputedMaxWidth;

  if (computedSize.height > aReflowState.mComputedMaxHeight)
    computedSize.height = aReflowState.mComputedMaxHeight;

  if (computedSize.width < aReflowState.mComputedMinWidth)
    computedSize.width = aReflowState.mComputedMinWidth;

  if (computedSize.height < aReflowState.mComputedMinHeight)
    computedSize.height = aReflowState.mComputedMinHeight;

  nsRect r(mRect.x, mRect.y, computedSize.width, computedSize.height);

  SetBounds(state, r);
 
  // layout our children
  Layout(state);
  
  // ok our child could have gotten bigger. So lets get its bounds
  aDesiredSize.width  = mRect.width;
  aDesiredSize.height = mRect.height;
  aDesiredSize.ascent = GetBoxAscent(state);

  // NS_FRAME_OUTSIDE_CHILDREN is set in SetBounds() above
  if (mState & NS_FRAME_OUTSIDE_CHILDREN) {
    nsRect* overflowArea = GetOverflowAreaProperty();
    NS_ASSERTION(overflowArea, "Failed to set overflow area property");
    aDesiredSize.mOverflowArea = *overflowArea;
  } else {
    aDesiredSize.mOverflowArea = nsRect(nsPoint(0, 0), GetSize());
  }

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

PRBool
nsLeafBoxFrame::IsFrameOfType(PRUint32 aFlags) const
{
  // This is bogus, but it's what we've always done.
  return !(aFlags & ~(eReplaced | eReplacedContainsBlock));
}

#ifdef DEBUG
NS_IMETHODIMP
nsLeafBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("LeafBox"), aResult);
}
#endif

NS_IMETHODIMP_(nsrefcnt) 
nsLeafBoxFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsLeafBoxFrame::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP
nsLeafBoxFrame::CharacterDataChanged(nsPresContext* aPresContext,
                                     nsIContent*     aChild,
                                     PRBool          aAppend)
{
  MarkIntrinsicWidthsDirty();
  return nsLeafFrame::CharacterDataChanged(aPresContext, aChild, aAppend);
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

PRBool
nsLeafBoxFrame::GetWasCollapsed(nsBoxLayoutState& aState)
{
    return nsBox::GetWasCollapsed(aState);
}

void
nsLeafBoxFrame::SetWasCollapsed(nsBoxLayoutState& aState, PRBool aWas)
{
    nsBox::SetWasCollapsed(aState, aWas);
}
