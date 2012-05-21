/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for HTML <frameset> elements */

#ifndef nsHTMLFrameset_h___
#define nsHTMLFrameset_h___

#include "nsGkAtoms.h"
#include "nsContainerFrame.h"
#include "nsColor.h"
#include "nsIObserver.h"
#include "nsWeakPtr.h"

class  nsIContent;
class  nsIFrame;
class  nsPresContext;
class  nsRenderingContext;
struct nsRect;
struct nsHTMLReflowState;
struct nsSize;
class  nsIAtom;
class  nsHTMLFramesetBorderFrame;
class  nsGUIEvent;
class  nsHTMLFramesetFrame;

#define NO_COLOR 0xFFFFFFFA

// defined at nsHTMLFrameSetElement.h
struct nsFramesetSpec;

struct nsBorderColor 
{
  nscolor mLeft;
  nscolor mRight;
  nscolor mTop;
  nscolor mBottom;

  nsBorderColor() { Set(NO_COLOR); }
  ~nsBorderColor() {}
  void Set(nscolor aColor) { mLeft = mRight = mTop = mBottom = aColor; }
};

enum nsFrameborder {
  eFrameborder_Yes = 0,
  eFrameborder_No,
  eFrameborder_Notset
};

struct nsFramesetDrag {
  nsHTMLFramesetFrame* mSource;    // frameset whose border was dragged to cause the resize
  PRInt32              mIndex;     // index of left col or top row of effected area
  PRInt32              mChange;    // pos for left to right or top to bottom, neg otherwise
  bool                 mVertical;  // vertical if true, otherwise horizontal

  nsFramesetDrag();
  void Reset(bool                 aVertical, 
             PRInt32              aIndex, 
             PRInt32              aChange, 
             nsHTMLFramesetFrame* aSource); 
  void UnSet();
};

/*******************************************************************************
 * nsHTMLFramesetFrame
 ******************************************************************************/
class nsHTMLFramesetFrame : public nsContainerFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsHTMLFramesetFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  nsHTMLFramesetFrame(nsStyleContext* aContext);

  virtual ~nsHTMLFramesetFrame();

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD SetInitialChildList(ChildListID  aListID,
                                 nsFrameList& aChildList);

  static bool    gDragInProgress;

  void GetSizeOfChild(nsIFrame* aChild, nsSize& aSize);

  void GetSizeOfChildAt(PRInt32  aIndexInParent, 
                        nsSize&  aSize, 
                        nsIntPoint& aCellIndex);

  static nsHTMLFramesetFrame* GetFramesetParent(nsIFrame* aChild);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD GetCursor(const nsPoint&    aPoint,
                       nsIFrame::Cursor& aCursor);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual nsIAtom* GetType() const;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  virtual bool IsLeaf() const;
  
  void StartMouseDrag(nsPresContext*            aPresContext, 
                      nsHTMLFramesetBorderFrame* aBorder, 
                      nsGUIEvent*                aEvent);

  void MouseDrag(nsPresContext* aPresContext, 
                 nsGUIEvent*     aEvent);

  void EndMouseDrag(nsPresContext* aPresContext);

  nsFrameborder GetParentFrameborder() { return mParentFrameborder; }

  void SetParentFrameborder(nsFrameborder aValue) { mParentFrameborder = aValue; }

  nsFramesetDrag& GetDrag() { return mDrag; }

  void RecalculateBorderResize();

protected:
  void Scale(nscoord  aDesired, 
             PRInt32  aNumIndicies, 
             PRInt32* aIndicies, 
             PRInt32  aNumItems,
             PRInt32* aItems);

  void CalculateRowCol(nsPresContext*       aPresContext, 
                       nscoord               aSize, 
                       PRInt32               aNumSpecs, 
                       const nsFramesetSpec* aSpecs, 
                       nscoord*              aValues);

  void GenerateRowCol(nsPresContext*       aPresContext,
                      nscoord               aSize,
                      PRInt32               aNumSpecs,
                      const nsFramesetSpec* aSpecs,
                      nscoord*              aValues,
                      nsString&             aNewAttr);

  virtual void GetDesiredSize(nsPresContext*          aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics&     aDesiredSize);

  PRInt32 GetBorderWidth(nsPresContext* aPresContext,
                         bool aTakeForcingIntoAccount);

  PRInt32 GetParentBorderWidth() { return mParentBorderWidth; }

  void SetParentBorderWidth(PRInt32 aWidth) { mParentBorderWidth = aWidth; }

  nscolor GetParentBorderColor() { return mParentBorderColor; }

  void SetParentBorderColor(nscolor aColor) { mParentBorderColor = aColor; }

  nsFrameborder GetFrameBorder();

  nsFrameborder GetFrameBorder(nsIContent* aContent);

  nscolor GetBorderColor();

  nscolor GetBorderColor(nsIContent* aFrameContent);

  bool GetNoResize(nsIFrame* aChildFrame); 
  
  virtual PRIntn GetSkipSides() const;

  void ReflowPlaceChild(nsIFrame*                aChild,
                        nsPresContext*          aPresContext,
                        const nsHTMLReflowState& aReflowState,
                        nsPoint&                 aOffset,
                        nsSize&                  aSize,
                        nsIntPoint*              aCellIndex = 0);
  
  bool CanResize(bool aVertical, 
                   bool aLeft); 

  bool CanChildResize(bool    aVertical, 
                        bool    aLeft, 
                        PRInt32 aChildX,
                        bool    aFrameset);
  
  void SetBorderResize(PRInt32*                   aChildTypes, 
                       nsHTMLFramesetBorderFrame* aBorderFrame);

  bool ChildIsFrameset(nsIFrame* aChild); 

  static int FrameResizePrefCallback(const char* aPref, void* aClosure);

  nsFramesetDrag   mDrag;
  nsBorderColor    mEdgeColors;
  nsHTMLFramesetBorderFrame* mDragger;
  nsHTMLFramesetFrame* mTopLevelFrameset;
  nsHTMLFramesetBorderFrame** mVerBorders;  // vertical borders
  nsHTMLFramesetBorderFrame** mHorBorders;  // horizontal borders
  PRInt32*         mChildTypes; // frameset/frame distinction of children
  nsFrameborder*   mChildFrameborder; // the frameborder attr of children
  nsBorderColor*   mChildBorderColors;
  nscoord*         mRowSizes;  // currently computed row sizes
  nscoord*         mColSizes;  // currently computed col sizes
  nsIntPoint       mFirstDragPoint;
  PRInt32          mNumRows;
  PRInt32          mNumCols;
  PRInt32          mNonBorderChildCount; 
  PRInt32          mNonBlankChildCount; 
  PRInt32          mEdgeVisibility;
  nsFrameborder    mParentFrameborder;
  nscolor          mParentBorderColor;
  PRInt32          mParentBorderWidth;
  PRInt32          mPrevNeighborOrigSize; // used during resize
  PRInt32          mNextNeighborOrigSize;
  PRInt32          mMinDrag;
  PRInt32          mChildCount;
  bool             mForceFrameResizability;
};

#endif
