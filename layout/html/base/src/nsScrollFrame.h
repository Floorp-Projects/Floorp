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
#ifndef nsScrollFrame_h___
#define nsScrollFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIStatefulFrame.h"

/**
 * The scroll frame creates and manages the scrolling view
 *
 * It only supports having a single child frame that typically is an area
 * frame, but doesn't have to be. The child frame must have a view, though
 *
 * Scroll frames don't support incremental changes, i.e. you can't replace
 * or remove the scrolled frame
 */
class nsScrollFrame : public nsHTMLContainerFrame,
                      public nsIScrollableFrame,
                      public nsIStatefulFrame
{
public:
  friend nsresult NS_NewScrollFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  // Called to set the one and only child frame. Returns NS_ERROR_INVALID_ARG
  // if the child frame is NULL, and NS_ERROR_UNEXPECTED if the child list
  // contains more than one frame
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  // Because there can be only one child frame, these two function return
  // NS_ERROR_FAILURE
  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);

  // This function returns NS_ERROR_NOT_IMPLEMENTED
  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD DidReflow(nsIPresContext*   aPresContext,
                       nsDidReflowStatus aStatus);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);

  NS_IMETHOD  GetScrollPreference(nsIPresContext* aPresContext, nsScrollPref* aScrollPreference) const;

  NS_IMETHOD GetScrollbarSizes(nsIPresContext* aPresContext, 
                               nscoord *aVbarWidth, 
                               nscoord *aHbarHeight) const;

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::scrollFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

    // nsIScrollableFrame
  NS_IMETHOD  SetScrolledFrame(nsIPresContext* aPresContext, nsIFrame *aScrolledFrame);
  NS_IMETHOD  GetScrolledFrame(nsIPresContext* aPresContext, nsIFrame *&aScrolledFrame) const;
  NS_IMETHOD  GetScrollbarVisibility(nsIPresContext* aPresContext,
                                     PRBool *aVerticalVisible,
                                     PRBool *aHorizontalVisible) const;
  NS_IMETHOD GetScrollableView(nsIPresContext* aContext, nsIScrollableView** aResult) { return NS_OK; };

  NS_IMETHOD SetScrollbarVisibility(nsIPresContext* aPresContext,
                                    PRBool aVerticalVisible,
                                    PRBool aHorizontalVisible);

  NS_IMETHOD GetClipSize(nsIPresContext* aPresContext, 
                         nscoord *aWidth, 
                         nscoord *aHeight) const;

  NS_IMETHOD GetScrollPosition(nsIPresContext* aContext, nscoord &aX, nscoord& aY) const;
  NS_IMETHOD ScrollTo(nsIPresContext* aContext, nscoord aX, nscoord aY, PRUint32 aFlags);
  NS_IMETHOD GetScrollbarBox(PRBool aVertical, nsIBox** aResult) { *aResult = nsnull; return NS_OK; };

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr); 
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release(void) { return NS_OK; }

  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

  //nsIStatefulFrame
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

protected:
  nsScrollFrame();
  virtual PRIntn GetSkipSides() const;

   // Creation of the widget for the scrolling view is factored into a virtual method so
   // that sub-classes may control widget creation.
  virtual nsresult CreateScrollingViewWidget(nsIView* aView, const nsStyleDisplay* aDisplay);
   // Getting the view for scollframe may be overriden to provide a parent view for te scroll frame
  virtual nsresult GetScrollingParentView(nsIPresContext* aPresContext,
                                          nsIFrame*       aParent,
                                          nsIView**       aParentView);

private:
  nsresult CreateScrollingView(nsIPresContext* aPresContext);

  nsresult CalculateScrollAreaSize(nsIPresContext*          aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsMargin&                aMargin,
                                   nscoord                  aSBWidth,
                                   nscoord                  aSBHeight,
                                   nsSize*                  aScrollAreaSize,
                                   PRBool*                  aRoomForVerticalScrollbar);

  nsresult CalculateChildTotalSize(nsIFrame*            aKidFrame,
                                   nsHTMLReflowMetrics& aKidReflowMetrics);
};

#endif /* nsScrollFrame_h___ */
