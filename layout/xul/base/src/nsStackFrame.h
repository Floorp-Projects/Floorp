/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at one time. So the
  can be flipped though like a deck of cards.
 
**/

#ifndef nsStackFrame_h___
#define nsStackFrame_h___

#include "nsBoxFrame.h"

class nsStackFrame : public nsBoxFrame
{
public:

  friend nsresult NS_NewStackFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

 

  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                             nsIFrame**     aFrame);

  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
      aResult = "Stack";
      return NS_OK;
  }

  


protected:

  nsStackFrame(nsIPresShell* aShell);

    // Paint one child frame
  virtual void PaintChild(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsIFrame*            aFrame,
                             nsFramePaintLayer    aWhichLayer);

  virtual void PaintChildren(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer);
    virtual void ComputeChildsNextPosition( nsCalculatedBoxInfo* aInfo, nscoord& aCurX, nscoord& aCurY, nscoord& aNextX, nscoord& aNextY, const nsSize& aCurrentChildSize, const nsRect& aBoxRect, nscoord aMaxAscent);
    virtual void ChildResized(nsIFrame* aFrame, nsHTMLReflowMetrics& aDesiredSize, nsRect& aRect, nscoord& aMaxAscent, nsCalculatedBoxInfo& aInfo, PRBool*& aResized, PRInt32 aInfoCount, nscoord& aChangedIndex, PRBool& aFinished, nscoord aIndex, nsString& aReason);
    virtual void LayoutChildrenInRect(nsRect& aSize, nscoord& aMaxAscent);
    virtual void AddChildSize(nsBoxInfo& aInfo, nsBoxInfo& aChildInfo);

private:
    virtual nsresult GetStackedFrameForPoint(nsIPresContext* aPresContext,
                                         nsIFrame* aChild,
                                         const nsRect& aRect,
                                         const nsPoint& aPoint, 
                                         nsIFrame**     aFrame);
}; // class nsStackFrame



#endif

