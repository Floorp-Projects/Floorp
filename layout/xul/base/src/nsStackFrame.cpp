/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsStackFrame.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsUnitConversion.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleChangeList.h"
#include "nsCSSRendering.h"
#include "nsIViewManager.h"
#include "nsBoxLayoutState.h"
#include "nsStackLayout.h"

nsresult
NS_NewStackFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsStackFrame* it = new (aPresShell) nsStackFrame(aPresShell, aLayoutManager);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
  
} // NS_NewStackFrame

nsStackFrame::nsStackFrame(nsIPresShell* aPresShell, nsIBoxLayout* aLayoutManager):nsBoxFrame(aPresShell)
{
    // if no layout manager specified us the stack layout
  nsCOMPtr<nsIBoxLayout> layout = aLayoutManager;

  if (layout == nsnull) {
    NS_NewStackLayout(aPresShell, layout);
  }

  SetLayoutManager(layout);
}


NS_IMETHODIMP  
nsStackFrame::GetFrameForPoint(nsPresContext* aPresContext,
                               const nsPoint& aPoint, 
                               nsFramePaintLayer aWhichLayer,    
                               nsIFrame** aFrame)
{
  if (aWhichLayer != NS_FRAME_PAINT_LAYER_BACKGROUND)
    return NS_ERROR_FAILURE;

  return nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer,
                                      aFrame);
}

/* virtual */ nsresult
nsStackFrame::GetFrameForPointChild(nsPresContext*   aPresContext,
                                    const nsPoint&    aPoint,
                                    nsFramePaintLayer aWhichLayer,    
                                    nsIFrame*         aChild,
                                    PRBool            aCheckMouseThrough,
                                    nsIFrame**        aFrame)
{
  if (aWhichLayer != NS_FRAME_PAINT_LAYER_BACKGROUND)
    return NS_ERROR_FAILURE;

  nsresult rv = nsBoxFrame::GetFrameForPointChild(aPresContext, aPoint,
                                           NS_FRAME_PAINT_LAYER_FOREGROUND,
                                           aChild, aCheckMouseThrough, aFrame);
  if (NS_SUCCEEDED(rv))
    return rv;
  return nsBoxFrame::GetFrameForPointChild(aPresContext, aPoint,
                                           NS_FRAME_PAINT_LAYER_BACKGROUND,
                                           aChild, aCheckMouseThrough, aFrame);
}

void
nsStackFrame::PaintChildren(nsPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer,
                            PRUint32             aFlags)
{
  // we need to make sure we paint background then foreground of each child because they
  // are stacked. Otherwise the foreground of the first child could be on the top of the
  // background of the second.
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND)
  {
      nsBoxFrame::PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  }
}

// Paint one child frame
void
nsStackFrame::PaintChild(nsPresContext*      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDirtyRect,
                         nsIFrame*            aFrame,
                         nsFramePaintLayer    aWhichLayer,
                         PRUint32             aFlags)
{
  // we need to make sure we paint background then foreground of each child because they
  // are stacked. Otherwise the foreground of the first child could be on the top of the
  // background of the second.
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND)
  {
    nsBoxFrame::PaintChild(aPresContext, aRenderingContext, aDirtyRect, aFrame, NS_FRAME_PAINT_LAYER_BACKGROUND);
    nsBoxFrame::PaintChild(aPresContext, aRenderingContext, aDirtyRect, aFrame, NS_FRAME_PAINT_LAYER_FOREGROUND);
  } 
}
