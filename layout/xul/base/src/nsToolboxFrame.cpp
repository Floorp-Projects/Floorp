/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

//
// Mike Pinkerton
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsToolboxFrame.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsIHTMLReflow.h"
#include "nsIReflowCommand.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsIWidget.h"
#include "nsXULAtoms.h"


//
// NS_NewToolboxFrame
//
// Creates a new toolbox frame and returns it in |aNewFrame|
//
nsresult
NS_NewToolboxFrame ( nsIFrame*& aNewFrame )
{
  nsToolboxFrame* it = new nsToolboxFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  //it->SetFlags(aFlags);
  aNewFrame = it;
  return NS_OK;
  
} // NS_NewToolboxFrame


//
// nsToolboxFrame cntr
//
// Init, if necessary
//
nsToolboxFrame :: nsToolboxFrame ( )
  : mSumOfToolbarHeights(0), mNumToolbars(0)
{
	//*** anything?
}


//
// nsToolboxFrame dstr
//
// Cleanup, if necessary
//
nsToolboxFrame :: ~nsToolboxFrame ( )
{
	//*** anything?
}


//
// RefreshStyleContext
//
// Not exactly sure what this does ;)
//
void
nsToolboxFrame :: RefreshStyleContext(nsIPresContext* aPresContext,
                                            nsIAtom *         aNewContentPseudo,
                                            nsCOMPtr<nsIStyleContext>* aCurrentStyle,
                                            nsIContent *      aContent,
                                            nsIStyleContext*  aParentStyle)
{
  nsCOMPtr<nsIStyleContext> newStyleContext ( dont_AddRef(aPresContext->ProbePseudoStyleContextFor(aContent,
                                                                              aNewContentPseudo,
                                                                              aParentStyle)) );
  if (newStyleContext != *aCurrentStyle)
    *aCurrentStyle = newStyleContext;
    
} // RefreshStyleContext


//
// ReResolveStyleContext
//
// When the style context changes, make sure that all of our styles are still up to date.
//
NS_IMETHODIMP
nsToolboxFrame :: ReResolveStyleContext ( nsIPresContext* aPresContext, nsIStyleContext* aParentContext)
{
  nsCOMPtr<nsIStyleContext> old ( mStyleContext );
  
  // this re-resolves |mStyleContext|, so it may change
  nsresult rv = nsFrame::ReResolveStyleContext(aPresContext, aParentContext); 
  if (NS_FAILED(rv)) {
    return rv;
  }

  if ( old != mStyleContext ) {
    nsCOMPtr<nsIAtom> grippyRolloverPseudo ( dont_AddRef(NS_NewAtom(":TOOLBOX-ROLLOVER")) );
    RefreshStyleContext(aPresContext, grippyRolloverPseudo, &mGrippyRolloverStyle, mContent, mStyleContext);

    nsCOMPtr<nsIAtom> grippyNormalPseudo ( dont_AddRef(NS_NewAtom(":TOOLBOX-NORMAL")) );
    RefreshStyleContext(aPresContext, grippyNormalPseudo, &mGrippyNormalStyle, mContent, mStyleContext);
  }
  
  return NS_OK;
  
} // ReResolveStyleContext


//
// Paint
//
// Paint our background and border like normal frames, but before we draw the
// children, draw our grippies for each toolbar.
//
NS_IMETHODIMP
nsToolboxFrame :: Paint ( nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
  // take care of bg painting, borders and children
  nsresult retVal = nsHTMLContainerFrame::Paint ( aPresContext, aRenderingContext, aDirtyRect, aWhichLayer );

  // now draw what makes us special
  DrawGrippies ( aPresContext, aRenderingContext );
  // DrawCollapsedBar
  
  return retVal;
  
} // Paint


//
// DrawGrippies
//
// Redraws all the grippies in the toolbox by iterating over each toolbar in the DOM 
// and figuring out how to draw the grippies based on size/visibility information
// 
void
nsToolboxFrame :: DrawGrippies (  nsIPresContext& aPresContext, nsIRenderingContext & aRenderingContext ) const
{
  for ( int i = 0; i < mNumToolbars; ++i ) {
    const TabInfo & grippy = mGrippies[i];
    
    PRBool hilight = (mGrippyHilighted == i) ? PR_TRUE : PR_FALSE;
    DrawGrippy ( aPresContext, aRenderingContext, grippy.mBoundingRect, hilight );
    
  } // for each child

} // DrawGrippies


//
// DrawGrippy
//
// Draw a single grippy in the given rectangle, either with or without rollover feedback.
//
void
nsToolboxFrame :: DrawGrippy (  nsIPresContext& aPresContext, nsIRenderingContext & aRenderingContext,
                                  const nsRect & aBoundingRect, PRBool aDrawHilighted ) const
{
  aRenderingContext.PushState();
  
  nsCOMPtr<nsIStyleContext> style ( aDrawHilighted ? mGrippyRolloverStyle : mGrippyNormalStyle ) ;
  
  const nsStyleColor*   grippyColor   = (const nsStyleColor*)style->GetStyleData(eStyleStruct_Color);
  const nsStyleSpacing* grippySpacing = (const nsStyleSpacing*)style->GetStyleData(eStyleStruct_Spacing);
  const nsStyleFont*    grippyFont    = (const nsStyleFont*)style->GetStyleData(eStyleStruct_Font);

  nsToolboxFrame* nonConstSelf = NS_CONST_CAST(nsToolboxFrame*, this);
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, nonConstSelf,
                                    aBoundingRect, aBoundingRect, *grippyColor, *grippySpacing, 0, 0);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, nonConstSelf,
                                aBoundingRect, aBoundingRect, *grippySpacing, style, 0);

  PRBool clipState;
  aRenderingContext.PopState(clipState);

} // DrawGrippy


//
// GetSkipSides
//
// ***What does this do???
//
PRIntn
nsToolboxFrame :: GetSkipSides() const
{
  return 0;

} // GetSkipSides


//
// Reflow
//
// This is responsible for layout out its child elements one on top of the
// other in the order in which they appear in the DOM. It is mostly used for
// toolbars, but I guess they don't have to be, looking at the implementation.
// This doesn't have to explicitly worry about toolbars that grow because of
// too much content because Gecko handles all that for us (grin).
//
// If a toolbar (child) is not visible, we need to leave extra space at the bottom of
// the toolbox for a "expando area" in which the grippies that represent the 
// collapsed toolbars reside.
//
// *** IMPORTANT IMPORTANT IMPORTANT ***
// We need a way to distinguish in the dom between a toolbar being hidden and a toolbar
// being collapsed. Right now I'm using "visible" as "collapsed" since there is no
// way to hide a toolbar from menus, etc.
//
NS_IMETHODIMP 
nsToolboxFrame :: Reflow(nsIPresContext&          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  //*** This temporary and used to initialize the psuedo-styles we use. But where else
  //*** should it go?
  ReResolveStyleContext(&aPresContext, mStyleContext);

  // start with a reasonable desired size (in twips), which will be changed to 
  // the size of our children plus some other stuff below.
  mSumOfToolbarHeights = 0;
  mNumToolbars = 0;
  aDesiredSize.width = 6000;
  aDesiredSize.height = 3000;
  aDesiredSize.ascent = 3000;
  aDesiredSize.descent = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  float p2t;
  aPresContext.GetScaledPixelsToTwips(p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);

  // Resize our frame based on our children (remember to leave room for the grippy on the right)
  nsIFrame* childFrame = mFrames.FirstChild();
  nsPoint offset ( kGrippyWidthInPixels * onePixel, 0 );
  PRUint32 grippyIndex = 0;
  PRBool anyCollapsedToolbars = PR_FALSE;
  while ( childFrame ) {
      
    nsSize maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
		
    nsHTMLReflowState reflowState(aPresContext, childFrame, aReflowState, maxSize);
    nsIHTMLReflow* htmlReflow;
    if (NS_OK == childFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      htmlReflow->WillReflow(aPresContext);
      nsresult result = htmlReflow->Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");       
      htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);  // XXX Should we be sending the DidReflow?
    }

    // set the toolbar to be the width/height desired, then set it's corresponding grippy to match
    nsRect rect(offset.x, offset.y, aDesiredSize.width, aDesiredSize.height);
    childFrame->SetRect(rect);

    nsRect grippyBounds(0, offset.y, kGrippyWidthInPixels * onePixel, aDesiredSize.height);
    TabInfo & grippy = mGrippies[grippyIndex];
    grippy.mToolbar = childFrame;
    grippy.mBoundingRect = nsRect(0, offset.y, kGrippyWidthInPixels * onePixel, aDesiredSize.height);
    grippy.mCollapsed = PR_FALSE;
    grippy.mToolbarHeight = aDesiredSize.height;

    offset.y += aDesiredSize.height;
    
    // advance to the next child
    childFrame->GetNextSibling(childFrame);
    ++grippyIndex;
    
  } // for each child

  // If there are any collapsed toolbars, we need to fix up the positions of the
  // tabs associated with them to lie horizontally (make them as wide as their
  // corresponding toolbar is tall).
  if ( anyCollapsedToolbars ) {
  
    //*** fix mBoundingRect to lie sideways
  
  }
  
  // let our toolbox be as wide as our parent says we can be and as tall
  // as our child toolbars. If any of the toolbars are not visible (collapsed), 
  // we need to add some extra room for the bottom bar.
  aDesiredSize.width = aReflowState.availableWidth - 50;
  aDesiredSize.height = anyCollapsedToolbars ? offset.y + 200 : offset.y;

  // remember how many toolbars we have
  mNumToolbars = grippyIndex;
  
  aStatus = NS_FRAME_COMPLETE;

  return NS_OK;

} // Reflow


//
// GetFrameForPoint
//
// Override to process events in our own frame
//
NS_IMETHODIMP
nsToolboxFrame :: GetFrameForPoint(const nsPoint& aPoint, 
                                  nsIFrame**     aFrame)
{
  nsIFrame* incoming = *aFrame;
  nsresult retVal = nsHTMLContainerFrame::GetFrameForPoint(aPoint, aFrame);
  
  if ( retVal == NS_ERROR_FAILURE ) {
    *aFrame = this;
    retVal = NS_OK;
  }
    
  return retVal;
}


//
// HandleEvent
//
// 
NS_IMETHODIMP
nsToolboxFrame :: HandleEvent ( nsIPresContext& aPresContext, 
                                   nsGUIEvent*     aEvent,
                                   nsEventStatus&  aEventStatus)
{
  if ( !aEvent )
    return nsEventStatus_eIgnore;
 
  switch ( aEvent->message ) {

//  case NS_MOUSE_LEFT_CLICK:
    case NS_MOUSE_LEFT_BUTTON_UP:
      OnMouseLeftClick ( aEvent->point );
      break;
    
    case NS_MOUSE_MOVE:
      OnMouseMove ( aEvent->point );
      break;
      
    case NS_MOUSE_EXIT:
      OnMouseExit ( );
      break;

  } // case of which event

  return nsEventStatus_eIgnore;
  
} // HandleEvent


//
// OnMouseMove
//
// Handle mouse move events for hilighting and unhilighting the grippies
//
void
nsToolboxFrame :: OnMouseMove ( nsPoint & aMouseLoc )
{
	for ( int i = 0; i < mNumToolbars; ++i ) {
		if ( mGrippies[i].mBoundingRect.Contains(aMouseLoc) ) {
			if ( i != mGrippyHilighted ) {
				// unhilight the old one
				if ( mGrippyHilighted != kNoGrippyHilighted )
					Invalidate ( mGrippies[mGrippyHilighted].mBoundingRect, PR_FALSE );
					
				// hilight the new one and remember it
				mGrippyHilighted = i;
				Invalidate ( mGrippies[i].mBoundingRect, PR_FALSE );
			} // if in a new tab
		}
	} // for each toolbar

} // OnMouseMove


//
// OnMouseLeftClick
//
// Check if a click is in a grippy and expand/collapse appropriately.
//
void
nsToolboxFrame :: OnMouseLeftClick ( nsPoint & aMouseLoc )
{
	for ( int i = 0; i < mNumToolbars; ++i ) {
		if ( mGrippies[i].mBoundingRect.Contains(aMouseLoc) ) {
			TabInfo & clickedTab = mGrippies[i];			
			if ( clickedTab.mCollapsed )
				ExpandToolbar ( clickedTab );
			else
				CollapseToolbar ( clickedTab );
			
			// don't keep repeating this process since toolbars have now be
			// relaid out and a new toolbar may be under the current mouse
			// location!
			break;
		}
	}
	
} // OnMouseLeftClick


//
// OnMouseExit
//
// Update the grippies that may have been hilighted while the mouse was within the
// manager.
//
void
nsToolboxFrame :: OnMouseExit ( )
{
	if ( mGrippyHilighted != kNoGrippyHilighted ) {
		Invalidate ( mGrippies[mGrippyHilighted].mBoundingRect, PR_FALSE );
		mGrippyHilighted = kNoGrippyHilighted;
	}

} // OnMouseExit



//
// CollapseToolbar
//
// Given the tab that was clicked on, collapse its corresponding toolbar. This
// assumes that the tab is expanded.
//
void
nsToolboxFrame :: CollapseToolbar ( TabInfo & inTab ) 
{
  const nsCOMPtr<nsIAtom> kSelectedAtom ( dont_AddRef( NS_NewAtom("collapsed")) );
   
  nsIContent* toolbar = nsnull;
  inTab.mToolbar->GetContent ( toolbar );
  if ( toolbar )
    toolbar->SetAttribute ( nsXULAtoms::nameSpaceID, kSelectedAtom, "true", PR_TRUE );
  NS_IF_RELEASE(toolbar);
  
} // CollapseToolbar


//
// ExpandToolbar
//
// Given the collapsed (horizontal) tab that was clicked on, expand its
// corresponding toolbar. This assumes the tab is collapsed.
//
void
nsToolboxFrame :: ExpandToolbar ( TabInfo & inTab ) 
{
  const nsCOMPtr<nsIAtom> kSelectedAtom ( dont_AddRef( NS_NewAtom("collapsed")) );
    
  mContent->UnsetAttribute ( nsXULAtoms::nameSpaceID, kSelectedAtom, PR_TRUE );

} // ExpandToolbar