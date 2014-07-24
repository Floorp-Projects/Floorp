/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for HTML <frameset> elements */

#ifndef nsHTMLFrameset_h___
#define nsHTMLFrameset_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsColor.h"

class  nsIContent;
class  nsPresContext;
struct nsRect;
struct nsHTMLReflowState;
struct nsSize;
class  nsIAtom;
class  nsHTMLFramesetBorderFrame;
class  nsHTMLFramesetFrame;

#define NO_COLOR 0xFFFFFFFA

// defined at HTMLFrameSetElement.h
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
  int32_t              mIndex;     // index of left col or top row of effected area
  int32_t              mChange;    // pos for left to right or top to bottom, neg otherwise
  bool                 mVertical;  // vertical if true, otherwise horizontal

  nsFramesetDrag();
  void Reset(bool                 aVertical, 
             int32_t              aIndex, 
             int32_t              aChange, 
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

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;

  virtual void SetInitialChildList(ChildListID  aListID,
                                   nsFrameList& aChildList) MOZ_OVERRIDE;

  static bool    gDragInProgress;

  void GetSizeOfChild(nsIFrame* aChild, mozilla::WritingMode aWM,
                      mozilla::LogicalSize& aSize);

  void GetSizeOfChildAt(int32_t  aIndexInParent,
                        mozilla::WritingMode aWM,
                        mozilla::LogicalSize&  aSize,
                        nsIntPoint& aCellIndex);

  virtual nsresult HandleEvent(nsPresContext* aPresContext, 
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  virtual nsresult GetCursor(const nsPoint&    aPoint,
                             nsIFrame::Cursor& aCursor) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  virtual bool IsLeaf() const MOZ_OVERRIDE;
  
  void StartMouseDrag(nsPresContext* aPresContext,
                      nsHTMLFramesetBorderFrame* aBorder,
                      mozilla::WidgetGUIEvent* aEvent);

  void MouseDrag(nsPresContext* aPresContext, 
                 mozilla::WidgetGUIEvent* aEvent);

  void EndMouseDrag(nsPresContext* aPresContext);

  nsFrameborder GetParentFrameborder() { return mParentFrameborder; }

  void SetParentFrameborder(nsFrameborder aValue) { mParentFrameborder = aValue; }

  nsFramesetDrag& GetDrag() { return mDrag; }

  void RecalculateBorderResize();

protected:
  void Scale(nscoord  aDesired, 
             int32_t  aNumIndicies, 
             int32_t* aIndicies, 
             int32_t  aNumItems,
             int32_t* aItems);

  void CalculateRowCol(nsPresContext*       aPresContext, 
                       nscoord               aSize, 
                       int32_t               aNumSpecs, 
                       const nsFramesetSpec* aSpecs, 
                       nscoord*              aValues);

  void GenerateRowCol(nsPresContext*       aPresContext,
                      nscoord               aSize,
                      int32_t               aNumSpecs,
                      const nsFramesetSpec* aSpecs,
                      nscoord*              aValues,
                      nsString&             aNewAttr);

  virtual void GetDesiredSize(nsPresContext*          aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics&     aDesiredSize);

  int32_t GetBorderWidth(nsPresContext* aPresContext,
                         bool aTakeForcingIntoAccount);

  int32_t GetParentBorderWidth() { return mParentBorderWidth; }

  void SetParentBorderWidth(int32_t aWidth) { mParentBorderWidth = aWidth; }

  nscolor GetParentBorderColor() { return mParentBorderColor; }

  void SetParentBorderColor(nscolor aColor) { mParentBorderColor = aColor; }

  nsFrameborder GetFrameBorder();

  nsFrameborder GetFrameBorder(nsIContent* aContent);

  nscolor GetBorderColor();

  nscolor GetBorderColor(nsIContent* aFrameContent);

  bool GetNoResize(nsIFrame* aChildFrame); 
  
  void ReflowPlaceChild(nsIFrame*                aChild,
                        nsPresContext*          aPresContext,
                        const nsHTMLReflowState& aReflowState,
                        nsPoint&                 aOffset,
                        nsSize&                  aSize,
                        nsIntPoint*              aCellIndex = 0);
  
  bool CanResize(bool aVertical, bool aLeft); 

  bool CanChildResize(bool aVertical, bool aLeft, int32_t aChildX);
  
  void SetBorderResize(nsHTMLFramesetBorderFrame* aBorderFrame);

  static void FrameResizePrefCallback(const char* aPref, void* aClosure);

  nsFramesetDrag   mDrag;
  nsBorderColor    mEdgeColors;
  nsHTMLFramesetBorderFrame* mDragger;
  nsHTMLFramesetFrame* mTopLevelFrameset;
  nsHTMLFramesetBorderFrame** mVerBorders;  // vertical borders
  nsHTMLFramesetBorderFrame** mHorBorders;  // horizontal borders
  nsFrameborder*   mChildFrameborder; // the frameborder attr of children
  nsBorderColor*   mChildBorderColors;
  nscoord*         mRowSizes;  // currently computed row sizes
  nscoord*         mColSizes;  // currently computed col sizes
  nsIntPoint       mFirstDragPoint;
  int32_t          mNumRows;
  int32_t          mNumCols;
  int32_t          mNonBorderChildCount; 
  int32_t          mNonBlankChildCount; 
  int32_t          mEdgeVisibility;
  nsFrameborder    mParentFrameborder;
  nscolor          mParentBorderColor;
  int32_t          mParentBorderWidth;
  int32_t          mPrevNeighborOrigSize; // used during resize
  int32_t          mNextNeighborOrigSize;
  int32_t          mMinDrag;
  int32_t          mChildCount;
  bool             mForceFrameResizability;
};

#endif
