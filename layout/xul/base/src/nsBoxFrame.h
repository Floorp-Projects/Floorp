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
  nsBoxFrame is a frame that can lay its children out either vertically or horizontally.
  It lays them out according to a min max or preferred size.
 
**/

#ifndef nsBoxFrame_h___
#define nsBoxFrame_h___

#include "nsCOMPtr.h"
#include "nsHTMLContainerFrame.h"
#include "nsIBox.h"
#include "nsISpaceManager.h"
class nsBoxFrameInner;
class nsBoxDebugInner;

class nsHTMLReflowCommand;
class nsHTMLInfo;

// flags for box info
#define NS_FRAME_BOX_SIZE_VALID    0x0001
#define NS_FRAME_BOX_IS_COLLAPSED  0x0002
#define NS_FRAME_BOX_NEEDS_RECALC  0x0004
#define NS_FRAME_IS_BOX            0x0008

class nsCalculatedBoxInfo : public nsBoxInfo {
public:
    nsSize calculatedSize;
    PRInt16 mFlags;
    nsCalculatedBoxInfo* next;
    nsIFrame* frame;

    virtual void Recycle(nsIPresShell* aShell);
};

class nsInfoList 
{
public:
    virtual nsCalculatedBoxInfo* GetFirst()=0;
    virtual nsCalculatedBoxInfo* GetLast()=0;
    virtual PRInt32 GetCount()=0;
};

// flags from box
#define NS_STATE_IS_HORIZONTAL           0x00400000
#define NS_STATE_AUTO_STRETCH            0x00800000
#define NS_STATE_IS_ROOT                 0x01000000
#define NS_STATE_CURRENTLY_IN_DEBUG      0x02000000
#define NS_STATE_SET_TO_DEBUG            0x04000000
#define NS_STATE_DEBUG_WAS_SET           0x08000000

class nsBoxFrame : public nsHTMLContainerFrame, public nsIBox
{
public:

  friend nsresult NS_NewBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot = PR_FALSE);
  // gets the rect inside our border and debug border. If you wish to paint inside a box
  // call this method to get the rect so you don't draw on the debug border or outer border.

  virtual void GetInnerRect(nsRect& aInner);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                             nsIFrame**     aFrame);

  NS_IMETHOD GetCursor(nsIPresContext* aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor);


  NS_IMETHOD DidReflow(nsIPresContext* aPresContext,
                      nsDidReflowStatus aStatus);

  NS_IMETHOD ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild);

  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);

 
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

  NS_IMETHOD Paint ( nsIPresContext* aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsRect& aDirtyRect,
                      nsFramePaintLayer aWhichLayer);



  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);

  NS_IMETHOD  SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList);


  NS_IMETHOD GetFrameName(nsString& aResult) const;

  virtual PRBool IsHorizontal() const;

  
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual ~nsBoxFrame();

  virtual void GetChildBoxInfo(PRInt32 aIndex, nsBoxInfo& aSize);

  // nsIBox methods
  NS_IMETHOD GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize);
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr); 
  NS_IMETHOD InvalidateCache(nsIFrame* aChild);
  NS_IMETHOD SetDebug(nsIPresContext* aPresContext, PRBool aDebug);

    enum Halignment {
        hAlign_Left,
        hAlign_Right,
        hAlign_Center
    };

    enum Valignment {
        vAlign_Top,
        vAlign_Middle,
        vAlign_BaseLine,
        vAlign_Bottom
    };
protected:
    nsBoxFrame(nsIPresShell* aPresShell, PRBool aIsRoot = PR_FALSE);

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


    virtual void GetRedefinedMinPrefMax(nsIPresContext* aPresContext, nsIFrame* aFrame, nsCalculatedBoxInfo& aSize);
    virtual nsresult GetChildBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsIFrame* aFrame, nsCalculatedBoxInfo& aSize);
    virtual void ComputeChildsNextPosition( nsCalculatedBoxInfo* aInfo, nscoord& aCurX, nscoord& aCurY, nscoord& aNextX, nscoord& aNextY, const nsSize& aCurrentChildSize, const nsRect& aBoxRect, nscoord aMaxAscent);
    virtual nsresult PlaceChildren(nsIPresContext* aPresContext, nsRect& boxRect);
    virtual void ChildResized(nsIFrame* aFrame, nsHTMLReflowMetrics& aDesiredSize, nsRect& aRect, nscoord& aMaxAscent, nsCalculatedBoxInfo& aInfo, PRBool*& aResized, PRInt32 aInfoCount, nscoord& aChangedIndex, PRBool& aFinished, nscoord aIndex, nsString& aReason);

    virtual void LayoutChildrenInRect(nsRect& aSize, nscoord& aMaxAscent);
    virtual void AddChildSize(nsBoxInfo& aInfo, nsBoxInfo& aChildInfo);
    virtual void BoundsCheck(const nsBoxInfo& aBoxInfo, nsRect& aRect);
    virtual void InvalidateChildren();
    virtual void AddSize(const nsSize& a, nsSize& b, PRBool largest);

    virtual PRIntn GetSkipSides() const { return 0; }

    virtual void GetInset(nsMargin& margin); 
    virtual void CollapseChild(nsIPresContext* aPresContext, nsIFrame* frame, PRBool hide);

    nsresult GenerateDirtyReflowCommand(nsIPresContext* aPresContext,
                                        nsIPresShell&   aPresShell);



    virtual PRBool GetInitialOrientation(PRBool& aIsHorizontal); 
    virtual PRBool GetInitialHAlignment(Halignment& aHalign); 
    virtual PRBool GetInitialVAlignment(Valignment& aValign); 
    virtual PRBool GetInitialAutoStretch(PRBool& aStretch); 
    virtual PRBool GetDefaultFlex(PRInt32& aFlex);

    virtual nsInfoList* GetInfoList();
    NS_IMETHOD  Destroy(nsIPresContext* aPresContext);

private: 
  
    friend class nsBoxFrameInner;
    friend class nsBoxDebug;
    nsBoxFrameInner* mInner;
}; // class nsBoxFrame

#endif

