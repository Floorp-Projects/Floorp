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
#ifndef nsScrollFrame_h___
#define nsScrollFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIDocumentObserver.h"

class nsISupportsArray;

/**
 * The scroll frame creates and manages the scrolling view
 *
 * It only supports having a single child frame that typically is an area
 * frame, but doesn't have to be. The child frame must have a view, though
 *
 * Scroll frames don't support incremental changes, i.e. you can't replace
 * or remove the scrolled frame
 */
class nsGfxScrollFrame : public nsHTMLContainerFrame, 
                                nsIAnonymousContentCreator,
                                nsIDocumentObserver {
public:
  friend nsresult NS_NewGfxScrollFrame(nsIFrame** aNewFrame);

  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  // Called to set the one and only child frame. Returns NS_ERROR_INVALID_ARG
  // if the child frame is NULL, and NS_ERROR_UNEXPECTED if the child list
  // contains more than one frame
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  // Because there can be only one child frame, these two function return
  // NS_ERROR_FAILURE
  NS_IMETHOD AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);

  NS_IMETHOD Destroy(nsIPresContext& aPresContext);

  // This function returns NS_ERROR_NOT_IMPLEMENTED
  NS_IMETHOD RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD DidReflow(nsIPresContext&   aPresContext,
                       nsDidReflowStatus aStatus);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer);

  // added for anonymous content support
  NS_IMETHOD CreateAnonymousContent(nsISupportsArray& aAnonymousItems);
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr); 
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release(void) { return NS_OK; }

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::scrollFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
  NS_IMETHOD GetFrameName(nsString& aResult) const;

 // nsIDocumentObserver
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginReflow(nsIDocument *aDocument,
			                   nsIPresShell* aShell) { return NS_OK; }
  NS_IMETHOD EndReflow(nsIDocument *aDocument,
		                   nsIPresShell* aShell) { return NS_OK; } 
  NS_IMETHOD ContentChanged(nsIDocument* aDoc, 
                            nsIContent* aContent,
                            nsISupports* aSubContent) { return NS_OK; }
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2) { return NS_OK; }
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint);
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer) { return NS_OK; } 
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) { return NS_OK; } 
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled) { return NS_OK; }
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) { return NS_OK; }
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) { return NS_OK; }

protected:
  nsGfxScrollFrame();
  virtual PRIntn GetSkipSides() const;

  virtual void ScrollbarChanged(nscoord aX, nscoord aY);


private:

  void SetAttribute(nsIFrame* aFrame, nsIAtom* aAtom, nscoord aSize);

  nsresult CalculateChildTotalSize(nsIFrame*            aKidFrame,
                                   nsHTMLReflowMetrics& aKidReflowMetrics);

  nsresult GetFrameSize(   nsIFrame* aFrame,
                           nsSize& aSize);
                        
  nsresult SetFrameSize(   nsIFrame* aFrame,
                           nsSize aSize);
 
  nsresult CalculateScrollAreaSize(nsIPresContext&          aPresContext,
                                       const nsHTMLReflowState& aReflowState,
                                       const nsSize&            aSbSize,
                                       nsSize&                  aScrollAreaSize);
  
  void SetScrollbarVisibility(nsIFrame* aScrollbar, PRBool aVisible);


  void DetermineReflowNeed(nsIPresContext&          aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aHscrollbarNeedsReflow, 
                                   PRBool&                  aVscrollbarNeedsReflow, 
                                   PRBool&                  aScrollAreaNeedsReflow, 
                                   nsIFrame*&               aIncrementalChild);

  void ReflowScrollbars(   nsIPresContext&         aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aHscrollbarNeedsReflow, 
                                   PRBool&                  aVscrollbarNeedsReflow, 
                                   PRBool&                  aScrollBarResized, 
                                   nsIFrame*&               aIncrementalChild);

  void ReflowScrollbar(    nsIPresContext&                  aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aScrollBarResized, 
                                   nsIFrame*                aScrollBarFrame,
                                   nsIFrame*&               aIncrementalChild);


  nsresult ReflowFrame(        nsIPresContext&              aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus,
                               nsIFrame*                aFrame,
                               const nsSize&            aAvailable,
                               const nsSize&            aComputed,
                               PRBool&                  aResized,
                               nsIFrame*&               aIncrementalChild);

   void ReflowScrollArea(        nsIPresContext&          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aHscrollbarNeedsReflow, 
                                   PRBool&                  aVscrollbarNeedsReflow, 
                                   PRBool&                  aScrollAreaNeedsReflow, 
                                   nsIFrame*&               aIncrementalChild);

   void LayoutChildren(nsIPresContext&          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState);

   void GetScrollbarDimensions(nsIPresContext& aPresContext,
                               nsSize& aSbSize);

   void AddRemoveScrollbar       (PRBool& aHasScrollbar, nscoord& aSize, nscoord aSbSize, PRBool aAdd);
   void AddHorizontalScrollbar   (const nsSize& aSbSize, nsSize& aScrollAreaSize);
   void AddVerticalScrollbar     (const nsSize& aSbSize, nsSize& aScrollAreaSize);
   void RemoveHorizontalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize);
   void RemoveVerticalScrollbar  (const nsSize& aSbSize, nsSize& aScrollAreaSize);
   nsIScrollableView* GetScrollableView();

   void GetScrolledContentSize(nsSize& aSize);

  nsIFrame* mHScrollbarFrame;
  nsIFrame* mVScrollbarFrame;
  nsIFrame* mScrollAreaFrame;
  PRBool mHasVerticalScrollbar;
  PRBool mHasHorizontalScrollbar;
  nscoord mOnePixel;

};

#endif /* nsScrollFrame_h___ */
