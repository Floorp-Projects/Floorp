/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsHTMLFrameset_h___
#define nsHTMLFrameset_h___

#include "nsHTMLAtoms.h"
#include "nsHTMLContainerFrame.h"
#include "nsColor.h"

class  nsIContent;
class  nsIFrame;
class  nsIPresContext;
class  nsIRenderingContext;
struct nsRect;
struct nsHTMLReflowState;
struct nsSize;
class  nsIAtom;
class  nsIWebShell;
class  nsHTMLFramesetBorderFrame;
struct nsGUIEvent;
class  nsHTMLFramesetFrame;

#define NS_IFRAMESETFRAME_IID \
{ 0xf47deac0, 0x4200, 0x11d2, { 0x80, 0x3c, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }

#define NO_COLOR 0xFFFFFFFA

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

enum nsFramesetUnit {
  eFramesetUnit_Fixed = 0,
  eFramesetUnit_Percent,
  eFramesetUnit_Relative
};

enum nsFrameborder {
  eFrameborder_Yes = 0,
  eFrameborder_No,
  eFrameborder_Notset
};

struct nsFramesetSpec {
  nsFramesetUnit mUnit;
  nscoord        mValue;
};

struct nsFramesetDrag {
  PRBool           mVertical;
  PRInt32          mIndex;     // index of left col or top row of effected area
  PRInt32          mChange;    // + for left to right or top to bottom
  nsHTMLFramesetFrame* mSource;
  PRBool           mConsumed;
  nsFramesetDrag(PRBool aVertical, PRInt32 aIndex, PRInt32 aChange, 
                 nsHTMLFramesetFrame* aSource) 
    : mVertical(aVertical), mIndex(aIndex), mChange(aChange), 
      mSource(aSource), mConsumed(PR_FALSE)
    {}
  nsFramesetDrag() 
    : mVertical(PR_TRUE), mIndex(-1), mChange(0), 
      mSource(nsnull), mConsumed(PR_TRUE)
  {}
};


/*******************************************************************************
 * nsHTMLFramesetFrame
 ******************************************************************************/
class nsHTMLFramesetFrame : public nsHTMLContainerFrame {

public:
  nsHTMLFramesetFrame();

  virtual ~nsHTMLFramesetFrame();

  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  static PRBool  gDragInProgress;
  static PRInt32 gMaxNumRowColSpecs;

  void GetSizeOfChild(nsIFrame* aChild, nsSize& aSize);

  void GetSizeOfChildAt(PRInt32 aIndexInParent, nsSize& aSize, nsPoint& aCellIndex);

  static nsHTMLFramesetFrame* GetFramesetParent(nsIFrame* aChild);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, 
                              nsIFrame**     aFrame);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsFramesetDrag*          aDrag,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  VerifyTree() const;

  void StartMouseDrag(nsIPresContext& aPresContext, nsHTMLFramesetBorderFrame* aBorder, 
                      nsGUIEvent* aEvent);
  void MouseDrag(nsIPresContext& aPresContext, nsGUIEvent* aEvent);
  void EndMouseDrag();

  nsFrameborder GetParentFrameborder() { return mParentFrameborder; }
  void SetParentFrameborder(nsFrameborder aValue) { mParentFrameborder = aValue; }

protected:
  void Scale(nscoord aDesired, PRInt32 aNumIndicies, 
             PRInt32* aIndicies, PRInt32* aItems);

  void CalculateRowCol(nsIPresContext* aPresContext, nscoord aSize, PRInt32 aNumSpecs, 
                       nsFramesetSpec* aSpecs, nscoord* aValues);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  PRInt32 GetBorderWidth(nsIPresContext* aPresContext);
  PRInt32 GetParentBorderWidth() { return mParentBorderWidth; }
  void    SetParentBorderWidth(PRInt32 aWidth) { mParentBorderWidth = aWidth; }
  nscolor GetParentBorderColor() { return mParentBorderColor; }
  void    SetParentBorderColor(nscolor aColor) { mParentBorderColor = aColor; }

  nsFrameborder GetFrameBorder(PRBool aStandardMode);
  nsFrameborder GetFrameBorder(nsIContent* aContent, PRBool aStandardMode);
  nscolor GetBorderColor();
  nscolor GetBorderColor(nsIContent* aFrameContent);
  PRBool GetNoResize(nsIFrame* aChildFrame); 
  
  virtual PRIntn GetSkipSides() const;

  void ParseRowCol(nsIAtom* aAttrType, PRInt32& aNumSpecs, nsFramesetSpec** aSpecs); 

  PRInt32 ParseRowColSpec(nsString& aSpec, PRInt32 aMaxNumValues,
                          nsFramesetSpec* aSpecs);

  void ReflowPlaceChild(nsIFrame*                aChild,
                        nsIPresContext&          aPresContext,
                        const nsHTMLReflowState& aReflowState,
                        nsPoint&                 aOffset,
                        nsSize&                  aSize,
                        nsFramesetDrag*          aDrag = 0,
                        nsPoint*                 aCellIndex = 0);
  
  PRBool CanResize(PRBool aVertical, PRBool aLeft); 
  PRBool CanChildResize(PRBool aVertical, PRBool aLeft, PRInt32 aChildX, PRBool aFrameset); 
  void SetBorderResize(PRInt32* aChildTypes, nsHTMLFramesetBorderFrame* aBorderFrame);
  PRBool ChildIsFrameset(nsIFrame* aChild); 

  PRInt32          mNumRows;
  nsFramesetSpec*  mRowSpecs;  // parsed, non-computed dimensions
  nscoord*         mRowSizes;  // currently computed row sizes 
  PRInt32          mNumCols;
  nsFramesetSpec*  mColSpecs;  // parsed, non-computed dimensions
  nscoord*         mColSizes;  // currently computed col sizes 
  PRInt32          mNonBorderChildCount; 
  PRInt32          mNonBlankChildCount; 
  PRInt32          mEdgeVisibility;
  nsBorderColor    mEdgeColors;
  nsFrameborder    mParentFrameborder;
  nscolor          mParentBorderColor;
  PRInt32          mParentBorderWidth;

  nsHTMLFramesetBorderFrame* mDragger;
  nsPoint          mLastDragPoint;
  PRInt32          mMinDrag;
  PRInt32          mChildCount;
};

#if 0
/*******************************************************************************
 * nsHTMLFrameset
 ******************************************************************************/
class nsHTMLFrameset : public nsHTMLContainer {
public:
 
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRBool aNotify);

protected:
  nsHTMLFrameset(nsIAtom* aTag, nsIWebShell* aParentWebShell);
  virtual  ~nsHTMLFrameset();

  friend nsresult NS_NewHTMLFrameset(nsIHTMLContent** aInstancePtrResult,
                                     nsIAtom* aTag, nsIWebShell* aWebShell);
  //friend class nsHTMLFramesetFrame;

  // this is held for a short time until the frame uses it, so it is not ref counted
  nsIWebShell*    mParentWebShell; 
};
#endif

#endif
