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

#include "nsToolbarFrame.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"


//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewToolbarFrame ( nsIFrame*& aNewFrame )
{
  nsToolbarFrame* it = new nsToolbarFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  aNewFrame = it;
  return NS_OK;
  
} // NS_NewToolbarFrame


//
// nsToolbarFrame cntr
//
// Init, if necessary
//
nsToolbarFrame :: nsToolbarFrame ( )
{
	//*** anything?
}


//
// nsToolbarFrame dstr
//
// Cleanup, if necessary
//
nsToolbarFrame :: ~nsToolbarFrame ( )
{
  printf("Deleting toolbar frame\n");
}


//
// Paint
//
// Paint our background and border like normal frames, but before we draw the
// children, draw our grippies for each toolbar.
//
NS_IMETHODIMP
nsToolbarFrame :: Paint ( nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
#if 0
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

  //*** anything toolbar specific here???
    
  // Now paint the items. Note that child elements have the opportunity to
  // override the visibility property and display even if their parent is
  // hidden
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;

#endif

  return nsBlockFrame::Paint ( aPresContext, aRenderingContext, aDirtyRect, aWhichLayer );
    
} // Paint


//
// Reflow
//
// Handle moving children around.
//
NS_IMETHODIMP
nsToolbarFrame :: Reflow ( nsIPresContext&          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{
  return nsBlockFrame::Reflow ( aPresContext, aDesiredSize, aReflowState, aStatus );

} // Reflow 

