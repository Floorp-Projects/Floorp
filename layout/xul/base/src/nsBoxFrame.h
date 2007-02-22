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

/**

  Eric D Vaughan
  nsBoxFrame is a frame that can lay its children out either vertically or horizontally.
  It lays them out according to a min max or preferred size.
 
**/

#ifndef nsBoxFrame_h___
#define nsBoxFrame_h___

#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
class nsBoxLayoutState;

class nsHTMLReflowCommand;
class nsHTMLInfo;

// flags for box info
#define NS_FRAME_BOX_SIZE_VALID    0x0001
#define NS_FRAME_BOX_IS_COLLAPSED  0x0002
#define NS_FRAME_BOX_NEEDS_RECALC  0x0004


// flags from box
#define NS_STATE_BOX_CHILD_RESERVED      0x00100000
#define NS_STATE_STACK_NOT_POSITIONED    0x00200000
//#define NS_STATE_IS_HORIZONTAL           0x00400000  moved to nsIFrame.h
#define NS_STATE_AUTO_STRETCH            0x00800000
//#define NS_STATE_IS_ROOT                 0x01000000  moved to nsIFrame.h
#define NS_STATE_CURRENTLY_IN_DEBUG      0x02000000
//#define NS_STATE_SET_TO_DEBUG            0x04000000  moved to nsIFrame.h
//#define NS_STATE_DEBUG_WAS_SET           0x08000000  moved to nsIFrame.h
#define NS_STATE_IS_COLLAPSED            0x10000000
//#define NS_STATE_STYLE_CHANGE            0x20000000  moved to nsIFrame.h
#define NS_STATE_EQUAL_SIZE              0x40000000
//#define NS_STATE_IS_DIRECTION_NORMAL     0x80000000  moved to nsIFrame.h

nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext,
                         PRBool aIsRoot = PR_FALSE,
                         nsIBoxLayout* aLayoutManager = nsnull);

class nsBoxFrame : public nsContainerFrame
{
public:

  friend nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell, 
                                  nsStyleContext* aContext,
                                  PRBool aIsRoot,
                                  nsIBoxLayout* aLayoutManager);

  // gets the rect inside our border and debug border. If you wish to paint inside a box
  // call this method to get the rect so you don't draw on the debug border or outer border.

  // Override addref/release to not assert
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  // ------ nsIBox -------------
  NS_IMETHOD SetLayoutManager(nsIBoxLayout* aLayout);
  NS_IMETHOD GetLayoutManager(nsIBoxLayout** aLayout);
  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild);

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetFlex(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);
#ifdef DEBUG_LAYOUT
  NS_IMETHOD SetDebug(nsBoxLayoutState& aBoxLayoutState, PRBool aDebug);
  NS_IMETHOD GetDebug(PRBool& aDebug);
#endif
  virtual Valignment GetVAlign() const { return mValign; }
  virtual Halignment GetHAlign() const { return mHalign; }
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);

  virtual PRBool GetMouseThrough() const;
  virtual PRBool ComputesOwnOverflowArea() { return PR_FALSE; }

  // ----- child and sibling operations ---

  // ----- public methods -------
  
  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        asPrevInFlow);

 
  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  virtual void MarkIntrinsicWidthsDirty();
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  AppendFrames(nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  InsertFrames(nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  RemoveFrame(nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);

  NS_IMETHOD  SetInitialChildList(nsIAtom*        aListName,
                                  nsIFrame*       aChildList);

  NS_IMETHOD DidSetStyleContext();

  virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  NS_IMETHOD DidReflow(nsPresContext*           aPresContext,
                       const nsHTMLReflowState*  aReflowState,
                       nsDidReflowStatus         aStatus);

  virtual ~nsBoxFrame();

  virtual nsresult GetContentOf(nsIContent** aContent);
  
  nsBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull);
 
  static nsresult CreateViewForFrame(nsPresContext* aPresContext,
                                     nsIFrame* aChild,
                                     nsStyleContext* aStyleContext,
                                     PRBool aForce);

  // virtual so nsStackFrame, nsButtonBoxFrame, nsSliderFrame and nsMenuFrame
  // can override it
  NS_IMETHOD BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  nsIBox* GetBoxAt(PRInt32 aIndex) { return mFrames.FrameAt(aIndex); }
  PRInt32 GetChildCount() { return mFrames.GetLength(); }
  
#ifdef DEBUG_LAYOUT
    virtual void SetDebugOnChildList(nsBoxLayoutState& aState, nsIBox* aChild, PRBool aDebug);
    nsresult DisplayDebugInfoFor(nsIBox*  aBox, 
                                 nsPoint& aPoint);
#endif

  static nsresult LayoutChildAt(nsBoxLayoutState& aState, nsIBox* aBox, const nsRect& aRect);

  // Fire DOM event. If no aContent argument use frame's mContent.
  // XXX This will be deprecated, because it is not good to fire synchronous DOM events
  // from layout. It's better to use nsFrame::FireDOMEvent() which is asynchronous.
  void FireDOMEventSynch(const nsAString& aDOMEventName, nsIContent *aContent = nsnull);

  /**
   * Utility method to redirect events on descendants to this frame.
   * Supports 'allowevents' attribute on descendant elements to allow those
   * elements and their descendants to receive events.
   */
  nsresult WrapListsInRedirector(nsDisplayListBuilder*   aBuilder,
                                 const nsDisplayListSet& aIn,
                                 const nsDisplayListSet& aOut);

protected:
#ifdef DEBUG_LAYOUT
    virtual void GetBoxName(nsAutoString& aName);
    void PaintXULDebugBackground(nsIRenderingContext& aRenderingContext,
                                 nsPoint aPt);
    void PaintXULDebugOverlay(nsIRenderingContext& aRenderingContext,
                              nsPoint aPt);
#endif

    virtual PRBool GetWasCollapsed(nsBoxLayoutState& aState);
    virtual void SetWasCollapsed(nsBoxLayoutState& aState, PRBool aWas);

    virtual PRBool GetInitialEqualSize(PRBool& aEqualSize); 
    virtual void GetInitialOrientation(PRBool& aIsHorizontal);
    virtual void GetInitialDirection(PRBool& aIsNormal);
    virtual PRBool GetInitialHAlignment(Halignment& aHalign); 
    virtual PRBool GetInitialVAlignment(Valignment& aValign); 
    virtual PRBool GetInitialAutoStretch(PRBool& aStretch); 
  
    virtual void Destroy();

    nsSize mPrefSize;
    nsSize mMinSize;
    nsSize mMaxSize;
    nscoord mFlex;
    nscoord mAscent;

    nsCOMPtr<nsIBoxLayout> mLayoutManager;

protected:
    nsresult RegUnregAccessKey(PRBool aDoReg);

  NS_HIDDEN_(void) CheckBoxOrder(nsBoxLayoutState& aState);

private: 

    // helper methods
    static PRBool AdjustTargetToScope(nsIFrame* aParent, nsIFrame*& aTargetFrame);




#ifdef DEBUG_LAYOUT
    nsresult SetDebug(nsPresContext* aPresContext, PRBool aDebug);
    PRBool GetInitialDebug(PRBool& aDebug);
    void GetDebugPref(nsPresContext* aPresContext);

    void GetDebugBorder(nsMargin& aInset);
    void GetDebugPadding(nsMargin& aInset);
    void GetDebugMargin(nsMargin& aInset);

#endif

    nsresult GetFrameSizeWithMargin(nsIBox* aBox, nsSize& aSize);

    void PixelMarginToTwips(nsPresContext* aPresContext, nsMargin& aMarginPixels);

#ifdef DEBUG_LAYOUT
    void GetValue(nsPresContext* aPresContext, const nsSize& a, const nsSize& b, char* value);
    void GetValue(nsPresContext* aPresContext, PRInt32 a, PRInt32 b, char* value);
    void DrawSpacer(nsPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRBool aHorizontal, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord spacerSize);
    void DrawLine(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2);
    void FillRect(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height);
#endif
    void UpdateMouseThrough();

    void CacheAttributes();

    nsIBox* GetBoxForFrame(nsIFrame* aFrame, PRBool& aIsAdaptor);

    // instance variables.
    Halignment mHalign;
    Valignment mValign;

    eMouseThrough mMouseThrough;

#ifdef DEBUG_LAYOUT
    static PRBool gDebug;
    static nsIBox* mDebugChild;
#endif

}; // class nsBoxFrame

#endif

