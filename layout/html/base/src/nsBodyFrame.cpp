/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsBodyFrame.h"
#include "nsIContent.h"
#include "nsIHTMLContent.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIDeviceContext.h"
#include "nsSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsAbsoluteFrame.h"
#include "nsHTMLIIDs.h"
#include "nsIWebShell.h"
#include "nsHTMLValue.h"
#include "nsHTMLParts.h"

static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

nsIAtom* nsBodyFrame::gAbsoluteAtom;

nsresult
NS_NewBodyFrame(nsIFrame*& aResult, PRUint32 aFlags)
{
  nsBodyFrame* it = new nsBodyFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  aResult = it;
  return NS_OK;
}

nsBodyFrame::nsBodyFrame()
{
  mSpaceManager = new nsSpaceManager(this);
  NS_ADDREF(mSpaceManager);
  // XXX for now this is a memory leak
  if (nsnull == gAbsoluteAtom) {
    gAbsoluteAtom = NS_NewAtom("Absolute-list");
  }
}

nsBodyFrame::~nsBodyFrame()
{
  NS_RELEASE(mSpaceManager);
}

/////////////////////////////////////////////////////////////////////////////
// nsISupports

nsresult
nsBodyFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIAbsoluteItemsIID)) {
    nsIAbsoluteItems* tmp = this;
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  }
  return nsBlockFrame::QueryInterface(aIID, aInstancePtr);
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_IMETHODIMP
nsBodyFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  DeleteFrameList(aPresContext, &mAbsoluteFrames);
  return nsBlockFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsBodyFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom*& aListName) const
{
  if (aIndex <= NS_BLOCK_FRAME_LAST_LIST_INDEX) {
    return nsBlockFrame::GetAdditionalChildListName(aIndex, aListName);
  }
  
  nsIAtom* atom = nsnull;
  if (NS_BODY_FRAME_ABSOLUTE_LIST_INDEX == aIndex) {
    atom = gAbsoluteAtom;
    NS_ADDREF(atom);
  }
  aListName = atom;
  return NS_OK;
}

NS_IMETHODIMP
nsBodyFrame::FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const
{
  if (aListName == gAbsoluteAtom) {
    aFirstChild = mAbsoluteFrames;
    return NS_OK;
  }

  return nsBlockFrame::FirstChild(aListName, aFirstChild);
}

#ifdef NS_DEBUG
void
nsBodyFrame::BandData::ComputeAvailSpaceRect()
{
  nsBandTrapezoid*  trapezoid = data;

  if (count > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Stop when we get to space occupied by a right floater, or when we've
    // looked at every trapezoid and none are right floaters
    for (i = 0; i < count; i++) {
      nsBandTrapezoid*  trapezoid = &data[i];
      if (trapezoid->state != nsBandTrapezoid::Available) {
        const nsStyleDisplay* display;
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 j, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (j = 0; j < numFrames; j++) {
            nsIFrame* f = (nsIFrame*)trapezoid->frames->ElementAt(j);
            f->GetStyleData(eStyleStruct_Display,
                            (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              goto foundRightFloater;
            }
          }
        } else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
          if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
            break;
          }
        }
      }
    }
  foundRightFloater:

    if (i > 0) {
      trapezoid = &data[i - 1];
    }
  }

  if (nsBandTrapezoid::Available == trapezoid->state) {
    // The trapezoid is available
    trapezoid->GetRect(availSpace);
  } else {
    const nsStyleDisplay* display;

    // The trapezoid is occupied. That means there's no available space
    trapezoid->GetRect(availSpace);

    // XXX Better handle the case of multiple frames
    if (nsBandTrapezoid::Occupied == trapezoid->state) {
      trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                     (const nsStyleStruct*&)display);
      if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
        availSpace.x = availSpace.XMost();
      }
    }
    availSpace.width = 0;
  }
}
#endif

#ifdef NS_DEBUG
NS_IMETHODIMP
nsBodyFrame::Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect)
{
  // Note: all absolutely positioned elements have views so we don't
  // need to worry about painting them
  nsresult rv = nsBlockFrame::Paint(aPresContext, aRenderingContext,
                                    aDirtyRect);

  if (nsIFrame::GetShowFrameBorders()) {
    // Render the bands in the spacemanager
    BandData band;
    nsISpaceManager* sm = mSpaceManager;
    nscoord y = 0;
    while (y < mRect.height) {
      sm->GetBandData(y, nsSize(mRect.width, mRect.height - y), band);
      band.ComputeAvailSpaceRect();

      // Render a box and a diagonal line through the band
      aRenderingContext.SetColor(NS_RGB(0,255,0));
      aRenderingContext.DrawRect(0, band.availSpace.y,
                                 mRect.width, band.availSpace.height);
      aRenderingContext.DrawLine(0, band.availSpace.y,
                                 mRect.width, band.availSpace.YMost());

      // Render boxes and opposite diagonal lines around the
      // unavailable parts of the band.
      PRInt32 i;
      for (i = 0; i < band.count; i++) {
        nsBandTrapezoid* trapezoid = &band.data[i];
        if (nsBandTrapezoid::Available != trapezoid->state) {
          nsRect r;
          trapezoid->GetRect(r);
          if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
            aRenderingContext.SetColor(NS_RGB(0,255,128));
          }
          else {
            aRenderingContext.SetColor(NS_RGB(128,255,0));
          }
          aRenderingContext.DrawRect(r);
          aRenderingContext.DrawLine(r.x, r.YMost(), r.XMost(), r.y);
        }
      }
      y = band.availSpace.YMost();
    }
  }

  return rv;
}
#endif

NS_IMETHODIMP
nsBodyFrame::Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsBodyFrame::Reflow: maxSize=%d,%d reason=%d",
                  aReflowState.maxSize.width,
                  aReflowState.maxSize.height,
                  aReflowState.reason));

  // Make a copy of the reflow state so we can set the space manager
  nsHTMLReflowState reflowState(aReflowState);
  reflowState.spaceManager = mSpaceManager;

  if (eReflowReason_Resize == reflowState.reason) {
    // Clear any regions that are marked as unavailable
    // XXX Temporary hack until everything is incremental...
    mSpaceManager->ClearRegions();
  }

  // XXX We need to peek at incremental reflow commands and see if the next
  // frame is one of the absolutely positioned frames...
  nsresult  rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, reflowState, aStatus);

  // Reflow any absolutely positioned frames that need reflowing
  // XXX We shouldn't really be doing this for all incremental reflow commands
  ReflowAbsoluteItems(aPresContext, reflowState);

  // Compute our desired size. Take into account any floaters when computing the
  // height
  if (mSpaceManager->YMost() > aDesiredSize.height) {
    aDesiredSize.height = mSpaceManager->YMost();
  }

  // Also take into account absolutely positioned elements
  const nsStyleDisplay* display= (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);

  if (NS_STYLE_OVERFLOW_HIDDEN != display->mOverflow) {
    for (PRInt32 i = 0; i < mAbsoluteItems.Count(); i++) {
      // Get the anchor frame
      nsAbsoluteFrame*  anchorFrame = (nsAbsoluteFrame*)mAbsoluteItems[i];
      nsIFrame*         absoluteFrame = anchorFrame->GetAbsoluteFrame();
      nsRect            rect;
  
      absoluteFrame->GetRect(rect);
      nscoord xmost = rect.XMost();
      nscoord ymost = rect.YMost();
      if (xmost > aDesiredSize.width) {
        aDesiredSize.width = xmost;
      }
      if (ymost > aDesiredSize.height) {
        aDesiredSize.height = ymost;
      }
    }
  }

  // XXX This code is really temporary; the lower level frame
  // classes need to contribute to the area that needs damage
  // repair. This class should only worry about damage repairing
  // it's border+padding area.
  nsRect  damageArea(0, 0, 0, 0);

  // Decide how much to repaint based on the reflow type.
  // Note: we don't have to handle the initial reflow case and the
  // resize reflow case, because they're handled by the scroll frame
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIReflowCommand::ReflowType  reflowType;
    aReflowState.reflowCommand->GetType(reflowType);

    // For append reflow commands that target the flowed frames just
    // repaint the newly added part of the frame.
    if (nsIReflowCommand::FrameAppended == reflowType) {
      // It's an append reflow command
      damageArea.y = mRect.YMost();
      damageArea.width = aDesiredSize.width;
      damageArea.height = aDesiredSize.height - mRect.height;
      if ((damageArea.height < 0) ||
          (aDesiredSize.height == mRect.height)) {
        // Since we don't know what changed, assume it all changed.
        damageArea.y = 0;
        damageArea.height = aDesiredSize.height;
      }
    } else {
      // Ideally the frame that is the target of the reflow command
      // (or its parent frame) would generate a damage rect, but
      // since none of the frame classes know how to do this then
      // for the time being just repaint the entire frame
      damageArea.width = aDesiredSize.width;
      // If the new height is smaller than the old height then make
      // sure we erase whatever used to be displayed
      damageArea.height = PR_MAX(aDesiredSize.height, mRect.height);
    }
  }

  // If this is really the body, force a repaint of the damage area
  if ((NS_BODY_THE_BODY & mFlags) && !damageArea.IsEmpty()) {
    Invalidate(damageArea);
  }

  return rv;
}

NS_METHOD
nsBodyFrame::CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame)
{
  nsBodyFrame* cf = new nsBodyFrame;
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  cf->Init(aPresContext, mContent, aParent, aStyleContext);
  cf->SetFlags(mFlags);
  cf->AppendToFlow(this);
  aContinuingFrame = cf;
  return NS_OK;
}

static void
AddToPadding(nsIPresContext* aPresContext,
             nsStyleUnit aStyleUnit,
             nscoord aValue, 
             PRBool aVertical,
             nsStyleCoord& aStyleCoord)
{
  if (eStyleUnit_Coord == aStyleUnit) {
    nscoord coord;
    coord = aStyleCoord.GetCoordValue();
    coord += aValue;
    aStyleCoord.SetCoordValue(coord);
  } 
  else if (eStyleUnit_Percent == aStyleUnit) {
    nsRect visibleArea;
    aPresContext->GetVisibleArea(visibleArea);
    float increment = (aVertical)
      ? ((float)aValue) / ((float)visibleArea.height)
      : ((float)aValue) / ((float)visibleArea.width);
    float percent = aStyleCoord.GetPercentValue();
    percent += increment;
    aStyleCoord.SetPercentValue(percent);
  }
}

// XXX ick. pfuii. gotta go!
NS_METHOD 
nsBodyFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
  if ((0 == (NS_BODY_THE_BODY & mFlags)) || (nsnull == mContent)) {
    return NS_OK;
  }

  // marginwidth/marginheight set in the body cancels
  // marginwidth/marginheight set in the web shell. However, if
  // marginwidth is set in the web shell but marginheight is not set
  // it is as if marginheight were set to 0. The same logic applies
  // when marginheight is set and marginwidth is not set.
      
  PRInt32 marginWidth = -1, marginHeight = -1;
  
  nsISupports* container;
  aPresContext->GetContainer(&container);
  if (nsnull != container) {
    nsIWebShell* webShell = nsnull;
    container->QueryInterface(kIWebShellIID, (void**) &webShell);
    if (nsnull != webShell) {
      webShell->GetMarginWidth(marginWidth);   // -1 indicates not set
      webShell->GetMarginHeight(marginHeight); // -1 indicates not set
      if ((marginWidth >= 0) && (marginHeight < 0)) { // nav quirk 
        marginHeight = 0;
      }
      if ((marginHeight >= 0) && (marginWidth < 0)) { // nav quirk
        marginWidth = 0;
      }
      NS_RELEASE(webShell);
    }
    NS_RELEASE(container);
  }

  nsHTMLValue value;
  float p2t;
  aPresContext->GetScaledPixelsToTwips(p2t);
  nsIHTMLContent* hc;
  if (NS_OK == mContent->QueryInterface(kIHTMLContentIID, (void**) &hc)) {
    hc->GetAttribute(nsHTMLAtoms::marginwidth, value);
    if (eHTMLUnit_Pixel == value.GetUnit()) { // marginwidth is set in body 
      marginWidth = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      if (marginWidth < 0) {
        marginWidth = 0;
      }
    }
    hc->GetAttribute(nsHTMLAtoms::marginheight, value);
    if (eHTMLUnit_Pixel == value.GetUnit()) { // marginheight is set in body
      marginHeight = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      if (marginHeight < 0) {
        marginHeight = 0;
      }
    }
    NS_RELEASE(hc);
  }

  nsStyleSpacing* spacing = nsnull;
  nsStyleCoord styleCoord;
  if (marginWidth > 0) {  // add marginwidth to padding
    spacing = (nsStyleSpacing*)mStyleContext->GetMutableStyleData(eStyleStruct_Spacing);
    AddToPadding(aPresContext, spacing->mPadding.GetLeftUnit(), 
                 marginWidth, PR_FALSE, spacing->mPadding.GetLeft(styleCoord));
    spacing->mPadding.SetLeft(styleCoord);
    AddToPadding(aPresContext, spacing->mPadding.GetRightUnit(), 
                 marginWidth, PR_FALSE, spacing->mPadding.GetRight(styleCoord));
    spacing->mPadding.SetRight(styleCoord);
  }
  if (marginHeight > 0) { // add marginheight to padding
    if (nsnull == spacing) {
      spacing = (nsStyleSpacing*)mStyleContext->GetMutableStyleData(eStyleStruct_Spacing);
    }
    AddToPadding(aPresContext, spacing->mPadding.GetTopUnit(), 
                 marginHeight, PR_TRUE, spacing->mPadding.GetTop(styleCoord));
    spacing->mPadding.SetTop(styleCoord);
    AddToPadding(aPresContext, spacing->mPadding.GetBottomUnit(), 
                 marginHeight, PR_TRUE, spacing->mPadding.GetBottom(styleCoord));
    spacing->mPadding.SetBottom(styleCoord);
  }

  if (nsnull != spacing) {
    mStyleContext->RecalcAutomaticData(aPresContext);
  }
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Helper functions

// Add the frame to the end of the child list
void nsBodyFrame::AddAbsoluteFrame(nsIFrame* aFrame)
{
  if (nsnull == mAbsoluteFrames) {
    mAbsoluteFrames = aFrame;
  } else {
    nsIFrame* lastChild = LastFrame(mAbsoluteFrames);
  
    lastChild->SetNextSibling(aFrame);
    aFrame->SetNextSibling(nsnull);
    // XXX Eliminate mChildCount...
  }
}

/////////////////////////////////////////////////////////////////////////////

// nsIAbsoluteItems

NS_METHOD nsBodyFrame::AddAbsoluteItem(nsAbsoluteFrame* aAnchorFrame)
{
  // Add the absolute anchor frame to our list of absolutely positioned
  // items.
  mAbsoluteItems.AppendElement(aAnchorFrame);
  return NS_OK;
}

PRBool
nsBodyFrame::IsAbsoluteFrame(nsIFrame* aFrame)
{
  // Check whether the frame is in our list of absolutely positioned frames
  for (nsIFrame* f = mAbsoluteFrames; nsnull != f; f->GetNextSibling(f)) {
    if (f == aFrame) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

NS_METHOD nsBodyFrame::RemoveAbsoluteItem(nsAbsoluteFrame* aAnchorFrame)
{
  NS_NOTYETIMPLEMENTED("removing an absolutely positioned frame");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Called at the end of the Reflow() member function so we can process
// any abolutely positioned items that need to be reflowed
void
nsBodyFrame::ReflowAbsoluteItems(nsIPresContext& aPresContext,
                                 const nsHTMLReflowState& aReflowState)
{
  for (PRInt32 i = 0; i < mAbsoluteItems.Count(); i++) {
    // Get the anchor frame and its absolutely positioned frame
    nsAbsoluteFrame*  anchorFrame = (nsAbsoluteFrame*)mAbsoluteItems[i];
    nsIFrame*         absoluteFrame = anchorFrame->GetAbsoluteFrame();
    PRBool            placeFrame = PR_FALSE;
    PRBool            reflowFrame = PR_FALSE;
    nsReflowReason    reflowReason = eReflowReason_Resize;

    // Get its style information
    const nsStyleDisplay*  display;
    const nsStylePosition* position;
     
    absoluteFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
    absoluteFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);

    // See whether the frame is a newly added frame
    if (!IsAbsoluteFrame(absoluteFrame)) {
      // The absolutely position item hasn't yet been added to our child list
      absoluteFrame->SetGeometricParent(this);

      // Add the absolutely positioned frame to the end of the child list
      AddAbsoluteFrame(absoluteFrame);

      // See whether this is the frame's initial reflow
      nsFrameState  frameState;
      absoluteFrame->GetFrameState(frameState);
      if (frameState & NS_FRAME_FIRST_REFLOW) {
        reflowReason = eReflowReason_Initial;
      }

      // We need to place and reflow the absolutely positioned frame
      placeFrame = reflowFrame = PR_TRUE;

    } else {
      // We need to place the frame if the left-offset or the top-offset are
      // auto or a percentage
      if ((eStyleUnit_Coord != position->mLeftOffset.GetUnit()) ||
          (eStyleUnit_Coord != position->mTopOffset.GetUnit())) {
        placeFrame = PR_TRUE;
      }

      // We need to reflow the frame if its width or its height is auto or
      // a percentage
      if ((eStyleUnit_Coord != position->mWidth.GetUnit()) ||
          (eStyleUnit_Coord != position->mHeight.GetUnit())) {
        reflowFrame = PR_TRUE;
      }
    }

    if (placeFrame || reflowFrame) {
      // Get the rect for the absolutely positioned element
      nsRect  rect;
      ComputeAbsoluteFrameBounds(anchorFrame, aReflowState, position, rect);
  
      nsIHTMLReflow*  htmlReflow;
      if (NS_OK == absoluteFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
        htmlReflow->WillReflow(aPresContext);
        absoluteFrame->MoveTo(rect.x, rect.y);

        if (reflowFrame) {
          // Resize reflow the absolutely positioned element
          nsSize  availSize(rect.width, rect.height);
      
          if (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow) {
            // Don't constrain the height since the container should be enlarged
            // to contain overflowing frames
            availSize.height = NS_UNCONSTRAINEDSIZE;
          }
      
          nsHTMLReflowMetrics desiredSize(nsnull);
          nsHTMLReflowState   reflowState(aPresContext, absoluteFrame,
                                          aReflowState, availSize,
                                          reflowReason);
          nsReflowStatus      status;
          htmlReflow->Reflow(aPresContext, desiredSize, reflowState, status);
        
          // Figure out what size to actually use. If we let the child choose its
          // size, then use what the child requested. Otherwise, use the value
          // specified in the style information
          if ((eStyleUnit_Auto == position->mWidth.GetUnit()) ||
              ((desiredSize.width > availSize.width) &&
               (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow))) {
            rect.width = desiredSize.width;
          }
          if ((eStyleUnit_Auto == position->mHeight.GetUnit()) ||
              (NS_UNCONSTRAINEDSIZE == rect.height) ||
              ((desiredSize.height > rect.height) &&
               (NS_STYLE_OVERFLOW_VISIBLE == display->mOverflow))) {
            rect.height = desiredSize.height;
          }
          absoluteFrame->SetRect(rect);
        }
      }
    }
  }
}

// Translate aPoint from aFrameFrom's coordinate space to our coordinate space
void nsBodyFrame::TranslatePoint(nsIFrame* aFrameFrom, nsPoint& aPoint) const
{
  nsIFrame* parent;

  aFrameFrom->GetGeometricParent(parent);
  while ((nsnull != parent) && (parent != (nsIFrame*)this)) {
    nsPoint origin;

    parent->GetOrigin(origin);
    aPoint += origin;
    parent->GetGeometricParent(parent);
  }
}

void nsBodyFrame::ComputeAbsoluteFrameBounds(nsIFrame*                aAnchorFrame,
                                             const nsHTMLReflowState& aReflowState,
                                             const nsStylePosition*   aPosition,
                                             nsRect&                  aRect) const
{
  // Compute the offset and size of the view based on the position properties,
  // and the inner rect of the containing block (which we get from the reflow
  // state)
  //
  // If either the left or top are 'auto' then get the offset of the anchor
  // frame from this frame
  nsPoint offset;
  if ((eStyleUnit_Auto == aPosition->mLeftOffset.GetUnit()) ||
      (eStyleUnit_Auto == aPosition->mTopOffset.GetUnit())) {
    aAnchorFrame->GetOrigin(offset);
    TranslatePoint(aAnchorFrame, offset);
  }

  // left-offset
  if (eStyleUnit_Auto == aPosition->mLeftOffset.GetUnit()) {
    // Use the current x-offset of the anchor frame translated into our
    // coordinate space
    aRect.x = offset.x;
  } else if (eStyleUnit_Coord == aPosition->mLeftOffset.GetUnit()) {
    aRect.x = aPosition->mLeftOffset.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == aPosition->mLeftOffset.GetUnit(),
                 "unexpected offset type");
    // Percentage values refer to the width of the containing block. If the
    // width is unconstrained then just use 0
    if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.width) {
      aRect.x = 0;
    } else {
      aRect.x = (nscoord)((float)aReflowState.maxSize.width *
                          aPosition->mLeftOffset.GetPercentValue());
    }
  }

  // top-offset
  if (eStyleUnit_Auto == aPosition->mTopOffset.GetUnit()) {
    // Use the current y-offset of the anchor frame translated into our
    // coordinate space
    aRect.y = offset.y;
  } else if (eStyleUnit_Coord == aPosition->mTopOffset.GetUnit()) {
    aRect.y = aPosition->mTopOffset.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == aPosition->mTopOffset.GetUnit(),
                 "unexpected offset type");
    // Percentage values refer to the height of the containing block. If the
    // height is unconstrained then interpret it like 'auto'
    if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.height) {
      aRect.y = offset.y;
    } else {
      aRect.y = (nscoord)((float)aReflowState.maxSize.height *
                          aPosition->mTopOffset.GetPercentValue());
    }
  }

  // XXX We aren't properly handling 'auto' width and height
  // XXX The width/height represent the size of the content area only, and not
  // the frame size...

  // width
  if (eStyleUnit_Auto == aPosition->mWidth.GetUnit()) {
    // Use the right-edge of the containing block
    aRect.width = aReflowState.maxSize.width;
  } else if (eStyleUnit_Coord == aPosition->mWidth.GetUnit()) {
    aRect.width = aPosition->mWidth.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == aPosition->mWidth.GetUnit(),
                 "unexpected width type");
    // Percentage values refer to the width of the containing block
    if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.width) {
      aRect.width = NS_UNCONSTRAINEDSIZE;
    } else {
      aRect.width = (nscoord)((float)aReflowState.maxSize.width *
                              aPosition->mWidth.GetPercentValue());
    }
  }

  // height
  if (eStyleUnit_Auto == aPosition->mHeight.GetUnit()) {
    // Allow it to be as high as it wants
    aRect.height = NS_UNCONSTRAINEDSIZE;
  } else if (eStyleUnit_Coord == aPosition->mHeight.GetUnit()) {
    aRect.height = aPosition->mHeight.GetCoordValue();
  } else {
    NS_ASSERTION(eStyleUnit_Percent == aPosition->mHeight.GetUnit(),
                 "unexpected height type");
    // Percentage values refer to the height of the containing block. If the
    // height is unconstrained, then interpret it like 'auto' and make the
    // height unconstrained
    if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.height) {
      aRect.height = NS_UNCONSTRAINEDSIZE;
    } else {
      aRect.height = (nscoord)((float)aReflowState.maxSize.height * 
                               aPosition->mHeight.GetPercentValue());
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// Diagnostics

NS_IMETHODIMP
nsBodyFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Body", aResult);
}

NS_IMETHODIMP
nsBodyFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter* aFilter) const
{
  nsresult  rv = nsBlockFrame::List(out, aIndent, aFilter);

  // Output absolutely positioned frames
  if (nsnull != mAbsoluteFrames) {
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
    fprintf(out, "absolute-items <\n");
  }
  nsIFrame* f = mAbsoluteFrames;
  while (nsnull != f) {
    f->List(out, aIndent+1, aFilter);
    f->GetNextSibling(f);
  }
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);

  return rv;
}


