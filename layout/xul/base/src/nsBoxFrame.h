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
#include "nsContainerBox.h"
class nsBoxLayoutState;

class nsHTMLReflowCommand;
class nsHTMLInfo;

// flags for box info
#define NS_FRAME_BOX_SIZE_VALID    0x0001
#define NS_FRAME_BOX_IS_COLLAPSED  0x0002
#define NS_FRAME_BOX_NEEDS_RECALC  0x0004
#define NS_FRAME_IS_BOX            0x0008


// flags from box
#define NS_STATE_BOX_CHILD_RESERVED      0x00100000
#define NS_STATE_STACK_NOT_POSITIONED    0x00200000
#define NS_STATE_IS_HORIZONTAL           0x00400000
#define NS_STATE_AUTO_STRETCH            0x00800000
#define NS_STATE_IS_ROOT                 0x01000000
#define NS_STATE_CURRENTLY_IN_DEBUG      0x02000000
#define NS_STATE_SET_TO_DEBUG            0x04000000
#define NS_STATE_DEBUG_WAS_SET           0x08000000
#define NS_STATE_IS_COLLAPSED            0x10000000
#define NS_STATE_STYLE_CHANGE            0x20000000
#define NS_STATE_EQUAL_SIZE              0x40000000
#define NS_STATE_IS_DIRECTION_NORMAL     0x80000000

nsresult NS_NewBoxFrame(nsIPresShell* aPresShell, 
                        nsIFrame** aNewFrame, 
                        PRBool aIsRoot = PR_FALSE,
                        nsIBoxLayout* aLayoutManager = nsnull);

class nsBoxFrame : public nsContainerFrame, public nsContainerBox
{
public:

  friend nsresult NS_NewBoxFrame(nsIPresShell* aPresShell, 
                                 nsIFrame** aNewFrame, 
                                 PRBool aIsRoot,
                                 nsIBoxLayout* aLayoutManager);

  // gets the rect inside our border and debug border. If you wish to paint inside a box
  // call this method to get the rect so you don't draw on the debug border or outer border.

  // ------ nsISupports --------

  NS_DECL_ISUPPORTS_INHERITED

  // ------ nsIBox -------------

  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetFlex(nsBoxLayoutState& aBoxLayoutState, nscoord& aFlex);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
#ifdef DEBUG_LAYOUT
  NS_IMETHOD SetDebug(nsBoxLayoutState& aBoxLayoutState, PRBool aDebug);
  NS_IMETHOD GetDebug(PRBool& aDebug);
#endif
  NS_IMETHOD GetFrame(nsIFrame** aFrame);
  NS_IMETHOD GetVAlign(Valignment& aAlign);
  NS_IMETHOD GetHAlign(Halignment& aAlign);
  NS_IMETHOD NeedsRecalc();
  NS_IMETHOD GetInset(nsMargin& aInset);
  NS_IMETHOD BeginLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD SetParent(const nsIFrame* aParent);

  //NS_IMETHOD GetMouseThrough(PRBool& aMouseThrough);

  // ----- child and sibling operations ---

  // ----- public methods -------
  
  NS_IMETHOD GetFrameForPoint(nsPresContext*   aPresContext,
                              const nsPoint&    aPoint,
                              nsFramePaintLayer aWhichLayer,    
                              nsIFrame**        aFrame);

  NS_IMETHOD GetCursor(nsPresContext* aPresContext,
                       nsPoint&        aPoint,
                       PRInt32&        aCursor);


  NS_IMETHOD ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild);

  NS_IMETHOD  Init(nsPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsStyleContext*  aContext,
                   nsIFrame*        asPrevInFlow);

 
  NS_IMETHOD AttributeChanged(nsPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);


  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  AppendFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  InsertFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  RemoveFrame(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);

  NS_IMETHOD  SetInitialChildList(nsPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList);

  virtual nsIAtom* GetType() const;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  NS_IMETHOD DidReflow(nsPresContext*           aPresContext,
                       const nsHTMLReflowState*  aReflowState,
                       nsDidReflowStatus         aStatus);

  virtual PRBool IsHorizontal() const;
  virtual PRBool IsNormalDirection() const;

  virtual ~nsBoxFrame();

  virtual nsresult GetContentOf(nsIContent** aContent);
  virtual nsresult SyncLayout(nsBoxLayoutState& aBoxLayoutState);
  virtual void CheckFrameOrder();
  
  nsBoxFrame(nsIPresShell* aPresShell, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull);
 
  static nsresult CreateViewForFrame(nsPresContext* aPresContext,
                                     nsIFrame* aChild,
                                     nsStyleContext* aStyleContext,
                                     PRBool aForce);

  NS_IMETHOD  Paint(nsPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags = 0);

  // returns true if it is an Initial Reflow and doing Print Preview
  static PRBool IsInitialReflowForPrintPreview(nsBoxLayoutState& aState, PRBool& aIsChrome);

protected:
#ifdef DEBUG_LAYOUT
    virtual void GetBoxName(nsAutoString& aName);
    virtual void PropagateDebug(nsBoxLayoutState& aState);
#endif

    virtual PRBool HasStyleChange();
    virtual void SetStyleChangeFlag(PRBool aDirty);




    // Paint one child frame
    virtual void PaintChild(nsPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsIFrame*            aFrame,
                            nsFramePaintLayer    aWhichLayer,
                            PRUint32             aFlags = 0);

    virtual void PaintChildren(nsPresContext*      aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               const nsRect&        aDirtyRect,
                               nsFramePaintLayer    aWhichLayer,
                               PRUint32             aFlags = 0);

    virtual PRBool GetInitialEqualSize(PRBool& aEqualSize); 
    virtual void GetInitialOrientation(PRBool& aIsHorizontal);
    virtual void GetInitialDirection(PRBool& aIsNormal);
    virtual PRBool GetInitialHAlignment(Halignment& aHalign); 
    virtual PRBool GetInitialVAlignment(Valignment& aValign); 
    virtual PRBool GetInitialAutoStretch(PRBool& aStretch); 
  
    NS_IMETHOD  Destroy(nsPresContext* aPresContext);

    nsSize mPrefSize;
    nsSize mMinSize;
    nsSize mMaxSize;
    nscoord mFlex;
    nscoord mAscent;

protected:
    nsresult RegUnregAccessKey(nsPresContext* aPresContext,
                               PRBool aDoReg);
    void FireDOMEvent(nsPresContext *aPresContext, const nsAString& aDOMEventName);
    virtual nsresult GetFrameForPointChild(nsPresContext*   aPresContext,
                                           const nsPoint&    aPoint,
                                           nsFramePaintLayer aWhichLayer,    
                                           nsIFrame*         aChild,
                                           PRBool            aCheckMouseThrough,
                                           nsIFrame**        aFrame);

private: 

    // helper methods
    void TranslateEventCoords(nsPresContext* aPresContext,
                                    const nsPoint& aPoint,
                                    nsPoint& aResult);

    static PRBool AdjustTargetToScope(nsIFrame* aParent, nsIFrame*& aTargetFrame);




#ifdef DEBUG_LAYOUT
    nsresult SetDebug(nsPresContext* aPresContext, PRBool aDebug);
    PRBool GetInitialDebug(PRBool& aDebug);
    void GetDebugPref(nsPresContext* aPresContext);

    nsresult DisplayDebugInfoFor(nsIBox*         aBox, 
                                 nsPresContext* aPresContext,
                                 nsPoint&        aPoint,
                                 PRInt32&        aCursor);

    void GetDebugBorder(nsMargin& aInset);
    void GetDebugPadding(nsMargin& aInset);
    void GetDebugMargin(nsMargin& aInset);

#endif

    nsresult GetFrameSizeWithMargin(nsIBox* aBox, nsSize& aSize);

    void PixelMarginToTwips(nsPresContext* aPresContext, nsMargin& aMarginPixels);

#ifdef DEBUG_LAYOUT
    void GetValue(nsPresContext* aPresContext, const nsSize& a, const nsSize& b, char* value);
    void GetValue(nsPresContext* aPresContext, PRInt32 a, PRInt32 b, char* value);
#endif
    void DrawSpacer(nsPresContext* aPresContext, nsIRenderingContext& aRenderingContext, PRBool aHorizontal, PRInt32 flex, nscoord x, nscoord y, nscoord size, nscoord spacerSize);
    void DrawLine(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x1, nscoord y1, nscoord x2, nscoord y2);
    void FillRect(nsIRenderingContext& aRenderingContext,  PRBool aHorizontal, nscoord x, nscoord y, nscoord width, nscoord height);
    void UpdateMouseThrough();

    void CacheAttributes();

    nsIBox* GetBoxForFrame(nsIFrame* aFrame, PRBool& aIsAdaptor);

    // instance variables.
    Halignment mHalign;
    Valignment mValign;

    nsPresContext* mPresContext;

#ifdef DEBUG_LAYOUT
    static PRBool gDebug;
    static nsIBox* mDebugChild;
#endif

}; // class nsBoxFrame

#endif

