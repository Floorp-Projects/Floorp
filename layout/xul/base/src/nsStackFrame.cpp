/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsStackFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHTMLIIDs.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIReflowCommand.h"
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
nsStackFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                             const nsPoint& aPoint, 
                             nsFramePaintLayer aWhichLayer,    
                             nsIFrame**     aFrame)
{   
  return nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);

  /*
  nsRect r(mRect);

  if (!r.Contains(aPoint))
     return NS_ERROR_FAILURE;

  // is it inside our border, padding, and debugborder or insets?
  nsMargin im(0,0,0,0);
  GetInset(im);
  nsMargin border(0,0,0,0);
  nsStyleBorderPadding  bPad;
  aFrame->GetStyle(eStyleStruct_BorderPaddingShortcut, (nsStyleStruct&)bPad);
  bPad.GetBorderPadding(borderPadding);
  r.Deflate(im);
  r.Deflate(border);    

  // no? Then it must be in our border so return us.
  if (!r.Contains(aPoint)) {
      *aFrame = this;
      return NS_OK;
  }


  nsIFrame* first = mFrames.FirstChild();

 

  // look at the children in reverse order
  nsresult rv;
      
  if (first) {
      nsPoint tmp;
      tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);
      rv = GetStackedFrameForPoint(aPresContext, first, nsRect(0,0,mRect.width, mRect.height), tmp, aFrame);
  } else
      rv = NS_ERROR_FAILURE;

  if (!NS_SUCCEEDED(rv)) {
        const nsStyleColor* color =
    (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

      PRBool        transparentBG = NS_STYLE_BG_COLOR_TRANSPARENT ==
                                    (color->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);

      PRBool backgroundImage = (color->mBackgroundImage.Length() > 0);

      if (!transparentBG || backgroundImage)
      {
          *aFrame = this;
          rv = NS_OK;
      }
  }

 
  #ifdef NS_DEBUG
                            printf("\n------------");

      if (*aFrame)
      nsFrame::ListTag(stdout, *aFrame);
                            printf("--------------\n");
  #endif


  return rv;
  */
}


nsresult
nsStackFrame::GetStackedFrameForPoint(nsIPresContext* aPresContext,
                                      nsIFrame* aChild,
                                      const nsRect& aRect,
                                      const nsPoint& aPoint, 
                                      nsIFrame**     aFrame)
{
    // look at all the children is reverse order. Use the stack to do 
    // this.
    nsIFrame* next;
    nsresult rv;
    aChild->GetNextSibling(&next);   
    if (next != nsnull) {
       rv = GetStackedFrameForPoint(aPresContext, next, aRect, aPoint, aFrame);
       if (NS_SUCCEEDED(rv) && *aFrame)  
           return rv;
    }

    rv = aChild->GetFrameForPoint(aPresContext, aPoint, NS_FRAME_PAINT_LAYER_FOREGROUND, aFrame);
    if (NS_SUCCEEDED(rv) && *aFrame)  
        return rv;
    return aChild->GetFrameForPoint(aPresContext, aPoint, NS_FRAME_PAINT_LAYER_BACKGROUND, aFrame);
}

void
nsStackFrame::PaintChildren(nsIPresContext*      aPresContext,
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
nsStackFrame::PaintChild(nsIPresContext*      aPresContext,
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
