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
#include "nsCOMPtr.h"


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
  if (eFramePaintLayer_Underlay == aWhichLayer) {
    const nsStyleDisplay* disp = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
    if (disp->mVisible && mRect.width && mRect.height) {
      // Paint our background and border
      PRIntn skipSides = GetSkipSides();
      const nsStyleColor* color = (const nsStyleColor*)
        mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *color, *spacing, 0, 0);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, skipSides);
    }
  }

  //*** draw grippies
  //*** draw collapsed area under toolbars (is that a frame itself???)
  
  // Now paint the toolbars. Note that child elements have the opportunity to
  // override the visibility property and display even if their parent is
  // hidden
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
  
} // Paint


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
NS_IMETHODIMP 
nsToolboxFrame :: Reflow(nsIPresContext&          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  // start with a reasonable desired size (in twips), which will be changed to 
  // the size of our children plus some other stuff below.
  aDesiredSize.width = 6000;
  aDesiredSize.height = 3000;
  aDesiredSize.ascent = 3000;
  aDesiredSize.descent = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  // Resize our frame based on our children
  nsIFrame* childFrame = mFrames.FirstChild();
  nsPoint offset ( 200, 0 );   // offset 10 pixels in to leave room for grippy
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

    nsRect rect(offset.x, offset.y, aDesiredSize.width, aDesiredSize.height);
    childFrame->SetRect(rect);

    offset.y += aDesiredSize.height;
    
    // advance to the next child
    childFrame->GetNextSibling(childFrame);
    
  } // for each child

  // let our toolbox be as wide as our parent says we can be and as tall
  // as our child toolbars
  aDesiredSize.width = aReflowState.availableWidth - 200;
  aDesiredSize.height = offset.y;

  aStatus = NS_FRAME_COMPLETE;

  return NS_OK;

} // Reflow


