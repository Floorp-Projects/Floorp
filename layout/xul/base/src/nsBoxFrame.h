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

/**

  Eric D Vaughan
  This class lays out its children either vertically or horizontally
 
**/

#ifndef nsBoxFrame_h___
#define nsBoxFrame_h___

#include "nsHTMLContainerFrame.h"

class nsBoxDataSpring {
public:
    nscoord preferredSize;
    nscoord minSize;
    nscoord maxSize;
    float  springConstant;

    nscoord altMinSize;
    nscoord altMaxSize;
    nscoord altPreferredSize;

    nscoord calculatedSize;
    PRBool  sizeValid;
    PRBool  wasFlowed;

    void init()
    {
        preferredSize = 0;
        springConstant = 0.0;
        minSize = 0;
        maxSize = NS_INTRINSICSIZE;
        altMinSize = 0;
        altMaxSize = NS_INTRINSICSIZE;
        altPreferredSize = 0;

        calculatedSize = 0;
        sizeValid = PR_FALSE;
        wasFlowed = PR_FALSE;
    }

};

class nsBoxFrame : public nsHTMLContainerFrame
{
public:

  friend nsresult NS_NewBoxFrame(nsIFrame*& aNewFrame);

  NS_IMETHOD FlowChildAt(nsIFrame* frame, 
                     nsIPresContext& aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     const nsSize& size,
                     nsIFrame* incrementalChild);


    NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

   NS_IMETHOD  ReResolveStyleContext ( nsIPresContext* aPresContext, 
                                      nsIStyleContext* aParentContext,
                                      PRInt32 aParentChange,
                                      nsStyleChangeList* aChangeList,
                                      PRInt32* aLocalChange) ;

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);


  // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);


  NS_IMETHOD  AppendFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  InsertFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  RemoveFrame(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);

  PRBool IsHorizontal() const { return mHorizontal; }

protected:
    nsBoxFrame();

	  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

    virtual PRIntn GetSkipSides() const { return 0; }

    virtual void GetInset(nsMargin& margin);
  
    virtual void Stretch(nsBoxDataSpring* springs, PRInt32 nSprings, nscoord& size);

    PRBool mHorizontal;

private: 

    nsBoxDataSpring springs[100];
    nscoord totalCount;

}; // class nsBoxFrame



#endif

