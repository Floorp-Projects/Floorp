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
#include "nsCOMPtr.h"
#include "nsFrameSetFrame.h"
#include "nsIHTMLContent.h"
#include "nsLeafFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsIWebShell.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsIComponentManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsHTMLAtoms.h"
#include "nsIScrollableView.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsIStyleContext.h"
#include "nsIDocumentLoader.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLParts.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
// masks for mEdgeVisibility
#define LEFT_VIS   0x0001
#define RIGHT_VIS  0x0002
#define TOP_VIS    0x0004
#define BOTTOM_VIS 0x0008
#define ALL_VIS    0x000F
#define NONE_VIS   0x0000

// Use these defines
#define NULL_POINT_X -1000000
#define NULL_POINT_Y 1000000

static PRInt32 LEFT_EDGE  = -1;
static PRInt32 RIGHT_EDGE = 1000000;

static NS_DEFINE_IID(kIFramesetFrameIID, NS_IFRAMESETFRAME_IID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

/*******************************************************************************
 * nsFramesetDrag
 ******************************************************************************/
nsFramesetDrag::nsFramesetDrag() 
{
  UnSet();
}

nsFramesetDrag::nsFramesetDrag(PRBool               aVertical, 
                               PRInt32              aIndex, 
                               PRInt32              aChange, 
                               nsHTMLFramesetFrame* aSource) 
{
  mVertical = aVertical;
  mIndex    = aIndex;
  mChange   = aChange; 
  mSource   = aSource;
}
void nsFramesetDrag::Reset(PRBool               aVertical, 
                           PRInt32              aIndex, 
                           PRInt32              aChange, 
                           nsHTMLFramesetFrame* aSource) 
{
  mVertical = aVertical;
  mIndex    = aIndex;
  mChange   = aChange;
  mSource   = aSource;
  mActive   = PR_TRUE;
}

void nsFramesetDrag::UnSet()
{
  mVertical = PR_TRUE;
  mIndex    = -1;
  mChange   = 0;
  mSource   = nsnull;
  mActive   = PR_FALSE;
}

/*******************************************************************************
 * nsHTMLFramesetBorderFrame
 ******************************************************************************/
class nsHTMLFramesetBorderFrame : public nsLeafFrame {

public:
  NS_IMETHOD GetFrameName(nsString& aResult) const;

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, 
                              nsIFrame**     aFrame);

  NS_IMETHOD GetCursor(nsIPresContext& aPresContext,
                             nsPoint&        aPoint,
                             PRInt32&        aCursor);
  
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  PRBool GetVisibility() { return mVisibility; }
  void SetVisibility(PRBool aVisibility);
  void SetColor(nscolor aColor);

protected:
  nsHTMLFramesetBorderFrame(PRInt32 aWidth, PRBool aVertical, PRBool aVisible);
  virtual ~nsHTMLFramesetBorderFrame();
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);
  PRInt32 mWidth;
  PRBool mVertical;
  PRBool mVisibility;
  nscolor mColor;
  // the prev and next neighbors are indexes into the row (for a horizontal border) or col (for
  // a vertical border) of nsHTMLFramesetFrames or nsHTMLFrames
  PRInt32 mPrevNeighbor; 
  PRInt32 mNextNeighbor;
  PRBool mCanResize;
  friend class nsHTMLFramesetFrame;
};
/*******************************************************************************
 * nsHTMLFramesetBlankFrame
 ******************************************************************************/
class nsHTMLFramesetBlankFrame : public nsLeafFrame {

public:
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

protected:
  virtual ~nsHTMLFramesetBlankFrame();
  virtual void GetDesiredSize(nsIPresContext*          aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics&     aDesiredSize);
  friend class nsHTMLFramesetFrame;
  friend class nsHTMLFrameset;
};

/*******************************************************************************
 * nsHTMLFramesetFrame
 ******************************************************************************/
PRInt32 nsHTMLFramesetFrame::gMaxNumRowColSpecs = 25;
PRBool  nsHTMLFramesetFrame::gDragInProgress = PR_FALSE;

nsHTMLFramesetFrame::nsHTMLFramesetFrame()
  : nsHTMLContainerFrame()
{
  mNumRows             = 0;
  mRowSpecs            = nsnull;
  mRowSizes            = nsnull;
  mNumCols             = 0;
  mColSpecs            = nsnull;
  mColSizes            = nsnull;
  mEdgeVisibility      = 0;
  mParentFrameborder   = eFrameborder_Yes; // default
  mParentBorderWidth   = -1; // default not set
  mParentBorderColor   = NO_COLOR; // default not set
  mLastDragPoint.x     = mLastDragPoint.y = 0;
  mMinDrag             = 0;
  mNonBorderChildCount = 0;
  mNonBlankChildCount  = 0;
  mDragger             = nsnull;
  mChildCount          = 0;
  mTopLevelFrameset    = nsnull;
  mEdgeColors.Set(NO_COLOR);
}

nsHTMLFramesetFrame::~nsHTMLFramesetFrame()
{
  if (mRowSizes) delete [] mRowSizes;
  if (mRowSpecs) delete [] mRowSpecs;
  if (mColSizes) delete [] mColSizes;
  if (mColSpecs) delete [] mColSpecs;
  mRowSizes = mColSizes = nsnull;
  mRowSpecs = mColSpecs = nsnull;
}

nsresult nsHTMLFramesetFrame::QueryInterface(const nsIID& aIID, 
                                             void**       aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  } else if (aIID.Equals(kIFramesetFrameIID)) {
    *aInstancePtr = (void*)this;
    return NS_OK;
  } 

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsHTMLFramesetFrame::Init(nsIPresContext&  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIStyleContext* aContext,
                          nsIFrame*        aPrevInFlow)
{
  nsHTMLContainerFrame::Init(aPresContext, aContent, aParent,
                             aContext, aPrevInFlow);
  // find the highest ancestor that is a frameset
  nsresult rv = NS_OK;
  nsIFrame* parentFrame = nsnull;
  GetParent((nsIFrame**)&parentFrame);
  mTopLevelFrameset = (nsHTMLFramesetFrame*)this;
  while (parentFrame) {
    nsHTMLFramesetFrame* frameset;
    rv = parentFrame->QueryInterface(kIFramesetFrameIID, (void**)&frameset);
    if (NS_SUCCEEDED(rv)) {
      mTopLevelFrameset = frameset;
      parentFrame->GetParent((nsIFrame**)&parentFrame);
    } else {
      break;
    }
  }
  return rv;
}

// XXX should this try to allocate twips based on an even pixel boundary?
void nsHTMLFramesetFrame::Scale(nscoord  aDesired, 
                                PRInt32  aNumIndicies, 
                                PRInt32* aIndicies, 
                                PRInt32* aItems)
{
  PRInt32 actual = 0;
  PRInt32 i, j;
  // get the actual total
  for (i = 0; i < aNumIndicies; i++) {
    j = aIndicies[i];
    actual += aItems[j];
  }

  float factor = (float)aDesired / (float)actual;
  actual = 0;
  // scale the items up or down
  for (i = 0; i < aNumIndicies; i++) {
    j = aIndicies[i];
    aItems[j] = NSToCoordRound((float)aItems[j] * factor);
    actual += aItems[j];
  }

  if (aDesired != actual) {
    PRInt32 unit = (aDesired > actual) ? 1 : -1;
    i = 0;
    while (aDesired != actual) {
      j = aIndicies[i];
      aItems[j] += unit;
      actual += unit;
      i++;
    }
  }
}
  

/**
  * Translate the rows/cols specs into an array of integer sizes for
  * each cell in the frameset. Sizes are allocated based on the priorities of the
  * specifier - fixed sizes have the highest priority, percentage sizes have the next
  * highest priority and relative sizes have the lowest.
  */
void nsHTMLFramesetFrame::CalculateRowCol(nsIPresContext* aPresContext, 
                                          nscoord         aSize, 
                                          PRInt32         aNumSpecs, 
                                          nsFramesetSpec* aSpecs, 
                                          nscoord*        aValues)
{
  PRInt32  fixedTotal = 0;
  PRInt32  numFixed = 0;
  PRInt32* fixed = new PRInt32[aNumSpecs];
  PRInt32  numPercent = 0;
  PRInt32* percent = new PRInt32[aNumSpecs];
  PRInt32  relativeSums = 0;
  PRInt32  numRelative = 0;
  PRInt32* relative= new PRInt32[aNumSpecs];

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  PRInt32 i, j;
 
  // initialize the fixed, percent, relative indices, allocate the fixed sizes and zero the others
  for (i = 0; i < aNumSpecs; i++) {   
    aValues[i] = 0;
    switch (aSpecs[i].mUnit) {
      case eFramesetUnit_Fixed:
        aValues[i] = NSToCoordRound(p2t * aSpecs[i].mValue);
        fixedTotal += aValues[i];
        fixed[numFixed] = i;
        numFixed++;
        break;
      case eFramesetUnit_Percent:
        percent[numPercent] = i;
        numPercent++;
        break;
      case eFramesetUnit_Relative:
        relative[numRelative] = i;
        numRelative++;
        relativeSums += aSpecs[i].mValue;
        break;
    }
  }

  // scale the fixed sizes if they total too much (or too little and there aren't any percent or relative)
  if ((fixedTotal > aSize) || ((fixedTotal < aSize) && (0 == numPercent) && (0 == numRelative))) { 
    Scale(aSize, numFixed, fixed, aValues);
    delete [] fixed; delete [] percent; delete [] relative;
    return;
  }

  PRInt32 percentMax = aSize - fixedTotal;
  PRInt32 percentTotal = 0;
  // allocate the percentage sizes from what is left over from the fixed allocation
  for (i = 0; i < numPercent; i++) {
    j = percent[i];
    aValues[j] = NSToCoordRound((float)aSpecs[j].mValue * (float)percentMax / 100.0f);
    percentTotal += aValues[j];
  }

  // scale the percent sizes if they total too much (or too little and there aren't any relative)
  if ((percentTotal > percentMax) || ((percentTotal < percentMax) && (0 == numRelative))) { 
    Scale(percentMax, numPercent, percent, aValues);
    delete [] fixed; delete [] percent; delete [] relative;
    return;
  }

  PRInt32 relativeMax = percentMax - percentTotal;
  PRInt32 relativeTotal = 0;
  // allocate the relative sizes from what is left over from the percent allocation
  for (i = 0; i < numRelative; i++) {
    j = relative[i];
    aValues[j] = NSToCoordRound((float)aSpecs[j].mValue * (float)relativeMax / (float)relativeSums);
    relativeTotal += aValues[j];
  }

  // scale the relative sizes if they take up too much or too little
  if (relativeTotal != relativeMax) { 
    Scale(relativeMax, numRelative, relative, aValues);
  }
  
  delete [] fixed; delete [] percent; delete [] relative;
}


PRInt32 nsHTMLFramesetFrame::GetBorderWidth(nsIPresContext* aPresContext) 
{
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nsHTMLValue htmlVal;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**)&content);
  if (nsnull != content) {
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::border, htmlVal))) {
      nsHTMLUnit unit = htmlVal.GetUnit();
      PRInt32 intVal = 0;
      if (eHTMLUnit_Pixel == unit) {
        intVal = htmlVal.GetPixelValue();
      } else if (eHTMLUnit_Integer == unit) {
        intVal = htmlVal.GetIntValue();
      } 
      if (intVal < 0) {
        intVal = 0;
      }
      NS_RELEASE(content);
      return NSIntPixelsToTwips(intVal, p2t);
    }
    NS_RELEASE(content);
  }

  if (mParentBorderWidth >= 0) {
    return mParentBorderWidth;
  }

  return NSIntPixelsToTwips(6, p2t);
}


PRIntn
nsHTMLFramesetFrame::GetSkipSides() const
{
  return 0;
}

void 
nsHTMLFramesetFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsHTMLReflowMetrics&     aDesiredSize)
{
  nsHTMLFramesetFrame* framesetParent = GetFramesetParent(this);
  if (nsnull == framesetParent) {
    nsRect area;
    aPresContext->GetVisibleArea(area);

    aDesiredSize.width = area.width;
    aDesiredSize.height= area.height;
  } else {
    nsSize size;
    framesetParent->GetSizeOfChild(this, size);
    aDesiredSize.width  = size.width;
    aDesiredSize.height = size.height;
  } 
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}


nsHTMLFramesetFrame* nsHTMLFramesetFrame::GetFramesetParent(nsIFrame* aChild)
{
  nsHTMLFramesetFrame* parent = nsnull;
  nsIContent* content = nsnull;
  aChild->GetContent(&content);
  if (nsnull != content) { 
    nsIContent* contentParent = nsnull;
    content->GetParent(contentParent);
    if (nsnull != contentParent) {
      nsIHTMLContent* contentParent2 = nsnull;
      contentParent->QueryInterface(kIHTMLContentIID, (void**)&contentParent2);
      if (nsnull != contentParent2) {
        nsIAtom* tag;
        contentParent2->GetTag(tag);
        if (nsHTMLAtoms::frameset == tag) {
          aChild->GetParent((nsIFrame**)&parent);
        }
        NS_IF_RELEASE(tag);
        NS_RELEASE(contentParent2);
      }
      NS_RELEASE(contentParent);
    }
    NS_RELEASE(content);
  }

  return parent;
}

// only valid for non border children
void nsHTMLFramesetFrame::GetSizeOfChildAt(PRInt32  aIndexInParent, 
                                           nsSize&  aSize, 
                                           nsPoint& aCellIndex)
{
  PRInt32 row = aIndexInParent / mNumCols;
  PRInt32 col = aIndexInParent - (row * mNumCols); // remainder from dividing index by mNumCols
  if ((row < mNumRows) && (col < mNumCols)) {
    aSize.width  = mColSizes[col];
    aSize.height = mRowSizes[row];
    aCellIndex.x = col;
    aCellIndex.y = row;
  } else {
    aSize.width = aSize.height = aCellIndex.x = aCellIndex.y = 0;
  }
}

// only valid for non border children
void nsHTMLFramesetFrame::GetSizeOfChild(nsIFrame* aChild, 
                                         nsSize&   aSize)
{
  // Reflow only creates children frames for <frameset> and <frame> content.
  // this assumption is used here
  int i = 0;
  for (nsIFrame* child = mFrames.FirstChild(); child;
       child->GetNextSibling(&child)) {
    if (aChild == child) {
      nsPoint ignore;
      GetSizeOfChildAt(i, aSize, ignore);
      return;
    }
    i++;
  }
  aSize.width  = 0;
  aSize.height = 0;
}  

  
NS_METHOD nsHTMLFramesetFrame::HandleEvent(nsIPresContext& aPresContext, 
                                           nsGUIEvent*     aEvent,
                                           nsEventStatus&  aEventStatus)
{
  if (mDragger) {
    // the nsFramesetBorderFrame has captured NS_MOUSE_DOWN
    switch (aEvent->message) {
      case NS_MOUSE_MOVE:
        MouseDrag(aPresContext, aEvent);
	      break;
      case NS_MOUSE_LEFT_BUTTON_UP:
        EndMouseDrag();
	      break;
    }
    aEventStatus = nsEventStatus_eConsumeNoDefault;
  } else {
    aEventStatus = nsEventStatus_eIgnore;
  }
  return NS_OK;
}

#if 0
PRBool 
nsHTMLFramesetFrame::IsGrabbingMouse()
{
  PRBool result = PR_FALSE;
  nsIView* view;
  GetView(&view);
  if (view) {
    nsIViewManager* viewMan;
    view->GetViewManager(viewMan);
    if (viewMan) {
      nsIView* grabber;
      viewMan->GetMouseEventGrabber(grabber);
      if (grabber == view) {
        // the nsFramesetBorderFrame has captured NS_MOUSE_DOWN
        result = PR_TRUE;
      }
      NS_RELEASE(viewMan);
    }
  }
  return result;
}
#endif

NS_IMETHODIMP
nsHTMLFramesetFrame::GetCursor(nsIPresContext& aPresContext,
                               nsPoint&        aPoint,
                               PRInt32&        aCursor)
{
  if (mDragger) {
    aCursor = (mDragger->mVertical) ? NS_STYLE_CURSOR_W_RESIZE : NS_STYLE_CURSOR_N_RESIZE;
  } else {
    aCursor = NS_STYLE_CURSOR_DEFAULT;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFramesetFrame::GetFrameForPoint(const nsPoint& aPoint, 
                                      nsIFrame**     aFrame)
{
  //XXX Temporary to deal with event handling in both this and FramsetBorderFrame
  if (mDragger) { 
    *aFrame = this;
    return NS_OK;
  } else {
    return nsContainerFrame::GetFrameForPoint(aPoint, aFrame);
  }
}

NS_IMETHODIMP
nsHTMLFramesetFrame::Paint(nsIPresContext&      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nsFramePaintLayer    aWhichLayer)
{
  //printf("frameset paint %X (%d,%d,%d,%d) \n", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext,
                                     aDirtyRect, aWhichLayer);
}

void nsHTMLFramesetFrame::ParseRowCol(nsIAtom* aAttrType, PRInt32& aNumSpecs, nsFramesetSpec** aSpecs) 
{
  nsHTMLValue value;
  nsAutoString rowsCols;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**)&content);
  if (nsnull != content) {
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetHTMLAttribute(aAttrType, value)) {
      if (eHTMLUnit_String == value.GetUnit()) {
        value.GetStringValue(rowsCols);
        nsFramesetSpec* specs = new nsFramesetSpec[gMaxNumRowColSpecs];
        aNumSpecs = ParseRowColSpec(rowsCols, gMaxNumRowColSpecs, specs);
        *aSpecs = new nsFramesetSpec[aNumSpecs];
        for (int i = 0; i < aNumSpecs; i++) {
          (*aSpecs)[i] = specs[i];
        }
        delete [] specs;
        NS_RELEASE(content);
        return;
      }
    }
    NS_RELEASE(content);
  }
  aNumSpecs = 1; 
  *aSpecs = new nsFramesetSpec[1];
  aSpecs[0]->mUnit  = eFramesetUnit_Relative;
  aSpecs[0]->mValue = 1;
}

/**
  * Translate a "rows" or "cols" spec into an array of nsFramesetSpecs
  */
PRInt32 
nsHTMLFramesetFrame::ParseRowColSpec(nsString&       aSpec, 
                                     PRInt32         aMaxNumValues, 
                                     nsFramesetSpec* aSpecs) 
{
  static const PRUnichar ASTER('*');
  static const PRUnichar PERCENT('%');
  static const PRUnichar COMMA(',');

  aSpec.Trim(" \n\r\t"); // remove leading and trailing whitespace  
  
  // Count the commas 
  PRInt32 commaX = aSpec.Find(COMMA);
  PRInt32 count = 1;
  while (commaX >= 0) {
    count++;
    commaX = aSpec.Find(COMMA, commaX + 1);
  }

  if (count > aMaxNumValues) {
    NS_ASSERTION(0, "Not enough space for values");
    count = aMaxNumValues;
  }

  // Parse each comma separated token

  PRInt32 start = 0;
  PRInt32 specLen = aSpec.Length();

  for (PRInt32 i = 0; i < count; i++) {
    // Find our comma
    commaX = aSpec.Find(COMMA, start);
    PRInt32 end = (commaX < 0) ? specLen : commaX;

    // Note: If end == start then it means that the token has no
    // data in it other than a terminating comma (or the end of the spec)
    aSpecs[i].mUnit = eFramesetUnit_Fixed;
    if (end > start) {
      PRInt32 numberEnd = end - 1;
      PRUnichar ch = aSpec.CharAt(numberEnd);
      if (ASTER == ch) {
        aSpecs[i].mUnit = eFramesetUnit_Relative;
        numberEnd--;
      } else if (PERCENT == ch) {
        aSpecs[i].mUnit = eFramesetUnit_Percent;
        numberEnd--;
      }

      // Translate value to an integer
      nsString token("");
      aSpec.Mid(token, start, 1 + numberEnd - start);
      //aValues[i] = nsCRT::atoi(token);  XXX this is broken, consequently the next 3 lines?
      char* tokenIso = token.ToNewCString(); 
      aSpecs[i].mValue = atoi(tokenIso);
      delete [] tokenIso; 
      if (eFramesetUnit_Percent == aSpecs[i].mUnit) {
        if (aSpecs[i].mValue <= 0) {
          aSpecs[i].mValue = 100 / count;
        }
      }
      if (aSpecs[i].mValue < 1) aSpecs[i].mValue = 1;

      start = end + 1;
    }
  }
  return count;
}

void 
nsHTMLFramesetFrame::ReflowPlaceChild(nsIFrame*                aChild,
                                      nsIPresContext&          aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsPoint&                 aOffset,
                                      nsSize&                  aSize,
                                      nsPoint*                 aCellIndex)
{
  if (mDrag.mActive && aCellIndex && ChildIsFrameset(aChild)) {
    nsIContent* childContent;
    aChild->GetContent(&childContent);
    if (childContent) {
      nsIAtom* tag;
      childContent->GetTag(tag);
      if (nsHTMLAtoms::frameset == tag) {
        nsHTMLFramesetFrame* childFS = (nsHTMLFramesetFrame*)aChild;
        PRBool  vertical = PR_TRUE;
        PRInt32 side     = RIGHT_EDGE;
        if (mDrag.mVertical) {
          if (mDrag.mIndex == aCellIndex->x) {
            childFS->mColSizes[childFS->mNumCols-1] += mDrag.mChange;
          } else if (mDrag.mIndex == RIGHT_EDGE) {
            childFS->mColSizes[childFS->mNumCols-1] += mDrag.mChange;
          } else if (mDrag.mIndex+1 == aCellIndex->x) {
            side = LEFT_EDGE;
            childFS->mColSizes[0] -= mDrag.mChange;
          } else if (mDrag.mIndex == LEFT_EDGE) {
            side = LEFT_EDGE;
            childFS->mColSizes[0] -= mDrag.mChange;
          }
        } else {
          vertical = PR_FALSE;
          if (mDrag.mIndex == aCellIndex->y) {
            childFS->mRowSizes[childFS->mNumRows-1] += mDrag.mChange;
          } else if (mDrag.mIndex == RIGHT_EDGE) {
            childFS->mRowSizes[childFS->mNumRows-1] += mDrag.mChange;
          } else if (mDrag.mIndex+1 == aCellIndex->y) { 
            side = LEFT_EDGE;
            childFS->mRowSizes[0] -= mDrag.mChange;
          } else if (mDrag.mIndex == LEFT_EDGE) {
            side = LEFT_EDGE;
            childFS->mRowSizes[0] -= mDrag.mChange;
          }
        }
        childFS->GetDrag().Reset(vertical, side, mDrag.mChange, mDrag.mSource);
      }
      NS_IF_RELEASE(tag);
      NS_RELEASE(childContent);
    }
  }

  // reflow and place the child
  nsIHTMLReflow* htmlReflow;
  if (NS_OK == aChild->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
    nsHTMLReflowState  reflowState(aPresContext, aReflowState, aChild, aSize);
    nsHTMLReflowMetrics metrics(nsnull);
    metrics.width = aSize.width;
    metrics.height= aSize.height;
    nsReflowStatus status;
  
    ReflowChild(aChild, aPresContext, metrics, reflowState, status);
    NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "bad status");
  
    // Place and size the child
    nsRect rect(aOffset.x, aOffset.y, aSize.width, aSize.height);
    aChild->SetRect(rect);
    htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED); // this call is needed
  }
}

static
nsFrameborder GetFrameBorderHelper(nsIHTMLContent* aContent, 
                                   PRBool          aStandardMode)
{
  if (nsnull != aContent) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (aContent->GetHTMLAttribute(nsHTMLAtoms::frameborder, value))) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        PRInt32 intValue;
        intValue = value.GetIntValue();
        if (!aStandardMode) {
          if (NS_STYLE_FRAME_YES == intValue) {
            intValue = NS_STYLE_FRAME_1;
          } 
          else if (NS_STYLE_FRAME_NO == intValue) {
            intValue = NS_STYLE_FRAME_0;
          }
        }
        if (NS_STYLE_FRAME_0 == intValue) {
          return eFrameborder_No;
        } 
        else if (NS_STYLE_FRAME_1 == intValue) {
          return eFrameborder_Yes;
        }
      }      
    }
  }
  return eFrameborder_Notset;
}

nsFrameborder nsHTMLFramesetFrame::GetFrameBorder(PRBool aStandardMode) 
{
  nsFrameborder result = eFrameborder_Notset;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    result = GetFrameBorderHelper(content, aStandardMode);
    NS_RELEASE(content);
  }
  if (eFrameborder_Notset == result) {
    return mParentFrameborder;
  }
  return result;
}

nsFrameborder nsHTMLFramesetFrame::GetFrameBorder(nsIContent* aContent, 
                                                  PRBool      aStandardMode)
{
  nsFrameborder result = eFrameborder_Notset;
  nsIHTMLContent* content = nsnull;
  aContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    result = GetFrameBorderHelper(content, aStandardMode);
    NS_RELEASE(content);
  }
  if (eFrameborder_Notset == result) {
    return GetFrameBorder(aStandardMode);
  }
  return result;
}

nscolor nsHTMLFramesetFrame::GetBorderColor() 
{
  nscolor result = NO_COLOR;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**)&content);
  if (nsnull != content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::bordercolor, value))) {
      if (value.GetUnit() == eHTMLUnit_Color) {
        result = value.GetColorValue();
      } else if (value.GetUnit() == eHTMLUnit_String) {
        nsAutoString buffer;
        value.GetStringValue(buffer);
        char cbuf[40];
        buffer.ToCString(cbuf, sizeof(cbuf));
        NS_ColorNameToRGB(cbuf, &result);
      }
    }
    NS_RELEASE(content);
  }
  if (NO_COLOR == result) {
    return mParentBorderColor;
  } 
  return result;
}

nscolor nsHTMLFramesetFrame::GetBorderColor(nsIContent* aContent) 
{
  nscolor result = NO_COLOR;
  nsIHTMLContent* content = nsnull;
  aContent->QueryInterface(kIHTMLContentIID, (void**)&content);
  if (nsnull != content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::bordercolor, value))) {
      if (value.GetUnit() == eHTMLUnit_Color) {
        result = value.GetColorValue();
      } else if (value.GetUnit() == eHTMLUnit_String) {
        nsAutoString buffer;
        value.GetStringValue(buffer);
        char cbuf[40];
        buffer.ToCString(cbuf, sizeof(cbuf));
        NS_ColorNameToRGB(cbuf, &result);
      }
    }
    NS_RELEASE(content);
  }
  if (NO_COLOR == result) {
    return GetBorderColor();
  }
  return result;
}


#define FRAMESET 0
#define FRAME 1
#define BLANK 2


static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

NS_IMETHODIMP
nsHTMLFramesetFrame::Reflow(nsIPresContext&          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{
  //printf("FramesetFrame2::Reflow %X (%d,%d) \n", this, aReflowState.availableWidth, aReflowState.availableHeight); 
  // Always get the size so that the caller knows how big we are
  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize);
  PRBool firstTime = (0 == mNumRows);

  if (firstTime) { 
    // create the view. a view is needed since it needs to be a mouse grabber

    nsIView* view;
    nsresult result = nsComponentManager::CreateInstance(kViewCID, nsnull, kIViewIID,
                                                   (void **)&view);
	  nsCOMPtr<nsIPresShell> presShell;
    aPresContext.GetShell(getter_AddRefs(presShell));
	  nsCOMPtr<nsIViewManager> viewMan;
    presShell->GetViewManager(getter_AddRefs(viewMan));

    nsIFrame* parWithView;
	  nsIView *parView;
    GetParentWithView(&parWithView);
	  parWithView->GetView(&parView);
    nsRect boundBox(0, 0, aDesiredSize.width, aDesiredSize.height); 
    result = view->Init(viewMan, boundBox, parView, nsnull);
    viewMan->InsertChild(parView, view, 0);
    SetView(view);

    // parse the rows= cols= data
    ParseRowCol(nsHTMLAtoms::rows, mNumRows, &mRowSpecs);
    ParseRowCol(nsHTMLAtoms::cols, mNumCols, &mColSpecs);
    mRowSizes  = new nscoord[mNumRows];
    mColSizes  = new nscoord[mNumCols];
  }
  PRInt32 numCells = mNumRows*mNumCols;

  nscoord width  = (aDesiredSize.width <= aReflowState.availableWidth)
    ? aDesiredSize.width : aReflowState.availableWidth;
  nscoord height = (aDesiredSize.height <= aReflowState.availableHeight)
    ? aDesiredSize.height : aReflowState.availableHeight;
  // subtract out the width of all of the potential borders. There are
  // only borders between <frame>s. There are none on the edges (e.g the
  // leftmost <frame> has no left border).
  PRInt32 borderWidth = GetBorderWidth(&aPresContext);
  width  -= (mNumCols - 1) * borderWidth;
  height -= (mNumRows - 1) * borderWidth;

  if (!mDrag.mActive && ( (firstTime) ||
                  ( (mRect.width != 0) && (mRect.height != 0) &&
                    (aDesiredSize.width != 0) && (aDesiredSize.height != 0) &&
                    ((aDesiredSize.width != mRect.width) || (aDesiredSize.height != mRect.height))) ) )  { 
    CalculateRowCol(&aPresContext, width, mNumCols, mColSpecs, mColSizes);
    CalculateRowCol(&aPresContext, height, mNumRows, mRowSpecs, mRowSizes);
  }

  PRBool*    verBordersVis;                // vertical borders visibility    
  nsHTMLFramesetBorderFrame** verBorders;  // vertical borders
  nscolor*   verBorderColors;
  PRBool*    horBordersVis;                // horizontal borders visibility
  nsHTMLFramesetBorderFrame** horBorders;  // horizontal borders
  nscolor*   horBorderColors;
  PRInt32*   childTypes;                   // frameset/frame distinction of children  
  nsFrameborder* childFrameborder;         // the frameborder attr of children 
  nsFrameborder frameborder;
  frameborder = GetFrameBorder(PR_FALSE);
  nsBorderColor* childBorderColors;             
  nscolor borderColor;
  if (firstTime) {
    verBorders    = new nsHTMLFramesetBorderFrame*[mNumCols];  // 1 more than number of ver borders
    verBordersVis = new PRBool[mNumCols];
    verBorderColors = new nscolor[mNumCols];
    for (int verX  = 0; verX < mNumCols; verX++) {
      verBorders[verX]    = nsnull;
      verBordersVis[verX] = PR_FALSE;
      verBorderColors[verX] = NO_COLOR;
    }
  
    horBorders    = new nsHTMLFramesetBorderFrame*[mNumRows];  // 1 more than number of hor borders
    horBordersVis = new PRBool[mNumRows];
    horBorderColors = new nscolor[mNumRows];
    for (int horX = 0; horX < mNumRows; horX++) {
      horBorders[horX]    = nsnull;
      horBordersVis[horX] = PR_FALSE;
      horBorderColors[horX] = NO_COLOR;
    }
    childTypes = new PRInt32[numCells]; 
    childFrameborder  = new nsFrameborder[numCells]; 
    childBorderColors  = new nsBorderColor[numCells]; 
    borderColor = GetBorderColor();
  }
  
  // create the children frames; skip content which isn't <frameset> or <frame>
  nsIFrame* lastChild = nsnull;
  if (firstTime) {
    mChildCount = 0; // number of <frame> or <frameset> children
    if (nsnull != mContent) {
      nsIFrame* frame;
      PRInt32 numChildren; // number of any type of children
      mContent->ChildCount(numChildren);
      for (int childX = 0; childX < numChildren; childX++) {
        if (mChildCount == numCells) { // we have more <frame> or <frameset> than cells
          break;
        }
        nsIContent* childCon;
        mContent->ChildAt(childX, childCon);
        nsIHTMLContent* child = nsnull;
        childCon->QueryInterface(kIHTMLContentIID, (void**)&child);
        NS_RELEASE(childCon);
        if (nsnull == child) {
          continue;
        }
        nsIAtom* tag;
        child->GetTag(tag);
        if ((nsHTMLAtoms::frameset == tag) || (nsHTMLAtoms::frame == tag)) {
          nsIStyleContext* kidSC;
          nsresult         result;

          aPresContext.ResolveStyleContextFor(child, mStyleContext,
                                              PR_FALSE, &kidSC);
          if (nsHTMLAtoms::frameset == tag) {
            result = NS_NewHTMLFramesetFrame(frame);
            frame->Init(aPresContext, child, this, kidSC, nsnull);

            childTypes[mChildCount] = FRAMESET;
            nsHTMLFramesetFrame* childFrame = (nsHTMLFramesetFrame*)frame;
            childFrame->SetParentFrameborder(frameborder);
            childFrame->SetParentBorderWidth(borderWidth);
            childFrame->SetParentBorderColor(borderColor);
            childBorderColors[mChildCount].Set(childFrame->GetBorderColor());
          } else { // frame
            result = NS_NewHTMLFrameOuterFrame(frame);
            frame->Init(aPresContext, child, this, kidSC, nsnull);

            childTypes[mChildCount] = FRAME;
            //
            childFrameborder[mChildCount] = GetFrameBorder(child, PR_FALSE);
            childBorderColors[mChildCount].Set(GetBorderColor(child));
          }
          NS_RELEASE(kidSC);

          if (NS_OK != result) {
            NS_RELEASE(tag);
            NS_RELEASE(child);
            return result;
          }

          if (nsnull == lastChild) {
            mFrames.SetFrames(frame);
          } else {
            lastChild->SetNextSibling(frame);
          }
          lastChild = frame;
          mChildCount++;
        }
        NS_RELEASE(child);
        NS_IF_RELEASE(tag);
      }
      mNonBlankChildCount = mChildCount;
      // add blank frames for frameset cells that had no content provided
      for (int blankX = mChildCount; blankX < numCells; blankX++) {
        // XXX the blank frame is using the content of its parent - at some point it 
        // should just have null content, if we support that
        nsHTMLFramesetBlankFrame* blankFrame = new nsHTMLFramesetBlankFrame;
        nsIStyleContext* pseudoStyleContext;
        aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::framesetBlankPseudo,
                                                  mStyleContext, PR_FALSE,
                                                  &pseudoStyleContext);
        blankFrame->Init(aPresContext, mContent, this, pseudoStyleContext, nsnull);
        NS_RELEASE(pseudoStyleContext);

        if (nsnull == lastChild) {
          mFrames.SetFrames(blankFrame);
        } else {
          lastChild->SetNextSibling(blankFrame);
        }
        lastChild = blankFrame;
        childTypes[mChildCount] = BLANK;
        childBorderColors[mChildCount].Set(NO_COLOR);
        mChildCount++;
      }
      mNonBorderChildCount = mChildCount;
    }
  }

  // reflow the children
  PRInt32 lastRow = 0;
  PRInt32 lastCol = 0;
  PRInt32 borderChildX = mNonBorderChildCount; // index of border children
  nsHTMLFramesetBorderFrame* borderFrame = nsnull;
  nsPoint offset(0,0);
  nsSize size, lastSize;
  nsIFrame* child = mFrames.FirstChild();

  for (PRInt32 childX = 0; childX < mNonBorderChildCount; childX++) {
    nsPoint cellIndex;
    GetSizeOfChildAt(childX, size, cellIndex);

    if (lastRow != cellIndex.y) {  // changed to next row
      offset.x = 0;
      offset.y += lastSize.height;
      if ((borderWidth > 0) && (eFrameborder_No != frameborder)) {
        if (firstTime) { // create horizontal border
          borderFrame = new nsHTMLFramesetBorderFrame(borderWidth, PR_FALSE, PR_FALSE);
          nsIStyleContext* pseudoStyleContext;
          aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::horizontalFramesetBorderPseudo,
                                                    mStyleContext, PR_FALSE,
                                                    &pseudoStyleContext);
          borderFrame->Init(aPresContext, mContent, this, pseudoStyleContext, nsnull);
          NS_RELEASE(pseudoStyleContext);

          mChildCount++;
          lastChild->SetNextSibling(borderFrame);
          lastChild = borderFrame;
          horBorders[cellIndex.y-1] = borderFrame;
          // set the neighbors for determining drag boundaries
          borderFrame->mPrevNeighbor = lastRow; 
          borderFrame->mNextNeighbor = cellIndex.y;  
        } else {
          borderFrame = (nsHTMLFramesetBorderFrame*)mFrames.FrameAt(borderChildX);
          borderChildX++;
        }
        nsSize borderSize(aDesiredSize.width, borderWidth);
        ReflowPlaceChild(borderFrame, aPresContext, aReflowState, offset, borderSize);
        offset.y += borderWidth;
      }
    } else {
      if ((cellIndex.x > 0) && (borderWidth > 0)) {  // moved to next col in same row
        if (0 == cellIndex.y) { // in 1st row
          if (firstTime) { // create vertical border
            borderFrame = new nsHTMLFramesetBorderFrame(borderWidth, PR_TRUE, PR_FALSE);
            nsIStyleContext* pseudoStyleContext;
            aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::verticalFramesetBorderPseudo,
                                                      mStyleContext,
                                                      PR_FALSE,
                                                      &pseudoStyleContext);
            borderFrame->Init(aPresContext, mContent, this, pseudoStyleContext, nsnull);
            NS_RELEASE(pseudoStyleContext);

            mChildCount++;
            lastChild->SetNextSibling(borderFrame);
            lastChild = borderFrame;
            verBorders[cellIndex.x-1] = borderFrame;
            // set the neighbors for determining drag boundaries
            borderFrame->mPrevNeighbor = lastCol; 
            borderFrame->mNextNeighbor = cellIndex.x;  
          } else {         
            borderFrame = (nsHTMLFramesetBorderFrame*)mFrames.FrameAt(borderChildX);
            borderChildX++;
          }
          nsSize borderSize(borderWidth, aDesiredSize.height);
          ReflowPlaceChild(borderFrame, aPresContext, aReflowState, offset, borderSize);
        }
        offset.x += borderWidth;
      }
    }

    ReflowPlaceChild(child, aPresContext, aReflowState, offset, size, &cellIndex);

    if (firstTime) {
      PRInt32 childVis; 
      if (FRAMESET == childTypes[childX]) {
        nsHTMLFramesetFrame* childFS = (nsHTMLFramesetFrame*)child; 
        childVis = childFS->mEdgeVisibility;
        childBorderColors[childX] = childFS->mEdgeColors;
      } else if (FRAME == childTypes[childX]) {
        if (eFrameborder_Yes == childFrameborder[childX]) {
          childVis = ALL_VIS;
        } else if (eFrameborder_No == childFrameborder[childX]) {
          childVis = NONE_VIS;
        } else {  // notset
          childVis = (eFrameborder_No == frameborder) ? NONE_VIS : ALL_VIS;
        }
      } else {  // blank 
        childVis = NONE_VIS;
      }
      nsBorderColor childColors = childBorderColors[childX];
      // set the visibility, color of our edge borders based on children
      if (0 == cellIndex.x) {
        if (!(mEdgeVisibility & LEFT_VIS)) {
          mEdgeVisibility |= (LEFT_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mLeft) {
          mEdgeColors.mLeft = childColors.mLeft;
        }
      }
      if (0 == cellIndex.y) {
        if (!(mEdgeVisibility & TOP_VIS)) {
          mEdgeVisibility |= (TOP_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mTop) {
          mEdgeColors.mTop = childColors.mTop;
        }
      }
      if (mNumCols-1 == cellIndex.x) {
        if (!(mEdgeVisibility & RIGHT_VIS)) {
          mEdgeVisibility |= (RIGHT_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mRight) {
          mEdgeColors.mRight = childColors.mRight;
        }
      }
      if (mNumRows-1 == cellIndex.y) {
        if (!(mEdgeVisibility & BOTTOM_VIS)) {
          mEdgeVisibility |= (BOTTOM_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mBottom) {
          mEdgeColors.mBottom = childColors.mBottom;
        }
      }
      // set the visibility of borders that the child may affect
      if (childVis & RIGHT_VIS) {
        verBordersVis[cellIndex.x] = PR_TRUE;
      }
      if (childVis & BOTTOM_VIS) {
        horBordersVis[cellIndex.y] = PR_TRUE;
      }
      if ((cellIndex.x > 0) && (childVis & LEFT_VIS)) {
        verBordersVis[cellIndex.x-1] = PR_TRUE;
      }
      if ((cellIndex.y > 0) && (childVis & TOP_VIS)) {
        horBordersVis[cellIndex.y-1] = PR_TRUE;
      }
      // set the colors of borders that the child may affect
      if (NO_COLOR == verBorderColors[cellIndex.x]) {
        verBorderColors[cellIndex.x] = childBorderColors[childX].mRight;
      }
      if (NO_COLOR == horBorderColors[cellIndex.y]) {
        horBorderColors[cellIndex.y] = childBorderColors[childX].mBottom;
      }
      if ((cellIndex.x > 0) && (NO_COLOR == verBorderColors[cellIndex.x-1])) {
        verBorderColors[cellIndex.x-1] = childBorderColors[childX].mLeft;
      }
      if ((cellIndex.y > 0) && (NO_COLOR == horBorderColors[cellIndex.y-1])) {
        horBorderColors[cellIndex.y-1] = childBorderColors[childX].mTop;
      }
    }
    lastRow  = cellIndex.y;
    lastCol  = cellIndex.x;
    lastSize = size;
    offset.x += size.width;
    child->GetNextSibling(&child);
  }

  if (firstTime) {
    nscolor childColor;
    // set the visibility, color, mouse sensitivity of borders
    for (int verX = 0; verX < mNumCols-1; verX++) {
      if (verBorders[verX]) {
        verBorders[verX]->SetVisibility(verBordersVis[verX]);
        SetBorderResize(childTypes, verBorders[verX]);
        childColor = (NO_COLOR == verBorderColors[verX]) ? borderColor : verBorderColors[verX];
        verBorders[verX]->SetColor(childColor);
      }
    }
    for (int horX = 0; horX < mNumRows-1; horX++) {
      if (horBorders[horX]) {
        horBorders[horX]->SetVisibility(horBordersVis[horX]);
        SetBorderResize(childTypes, horBorders[horX]);
        childColor = (NO_COLOR == horBorderColors[horX]) ? borderColor : horBorderColors[horX]; 
        horBorders[horX]->SetColor(childColor);
      }
    }

    delete[] verBordersVis;    
    delete[] verBorders;
    delete[] verBorderColors;
    delete[] horBordersVis; 
    delete[] horBorders;
    delete[] horBorderColors;
    delete[] childTypes; 
    delete[] childFrameborder;
    delete[] childBorderColors;
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  mDrag.UnSet();
  return NS_OK;
}

PRBool 
nsHTMLFramesetFrame::ChildIsFrameset(nsIFrame* aChild) 
{
  nsIFrame* childFrame = nsnull;
  aChild->QueryInterface(kIFramesetFrameIID, (void**)&childFrame);
  if (childFrame) {
    return PR_TRUE;
  }
  return PR_FALSE;
}


PRBool 
nsHTMLFramesetFrame::CanResize(PRBool aVertical, 
                               PRBool aLeft) 
{
  nsIFrame* child;
  PRInt32 childX;
  PRInt32 startX;
  if (aVertical) {
    startX = (aLeft) ? 0 : mNumCols-1;
    for (childX = startX; childX < mNonBorderChildCount; childX += mNumCols) {
      child = mFrames.FrameAt(childX);
      if (!CanChildResize(aVertical, aLeft, childX, ChildIsFrameset(child))) {
        return PR_FALSE;
      }
    } 
  } else {
    startX = (aLeft) ? 0 : (mNumRows - 1) * mNumCols;
    PRInt32 endX = startX + mNumCols;
    for (childX = startX; childX < endX; childX++) {
      child = mFrames.FrameAt(childX);
      if (!CanChildResize(aVertical, aLeft, childX, ChildIsFrameset(child))) {
        return PR_FALSE;
      }
    }
  }
  return PR_TRUE;
}

PRBool
nsHTMLFramesetFrame::GetNoResize(nsIFrame* aChildFrame) 
{
  PRBool result = PR_FALSE;
  nsIContent* content = nsnull;
  aChildFrame->GetContent(&content);
  if (nsnull != content) {
    nsIHTMLContent* htmlContent = nsnull;
    content->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
    if (nsnull != htmlContent) {
      nsHTMLValue value;
      if (NS_CONTENT_ATTR_HAS_VALUE == htmlContent->GetHTMLAttribute(nsHTMLAtoms::noresize, value)) {
        result = PR_TRUE;
      }
      NS_RELEASE(htmlContent);
    }
    NS_RELEASE(content);
  }
  return result;
}

PRBool 
nsHTMLFramesetFrame::CanChildResize(PRBool  aVertical, 
                                    PRBool  aLeft, 
                                    PRInt32 aChildX, 
                                    PRBool  aFrameset) 
{
  nsIFrame* child = mFrames.FrameAt(aChildX);
  if (aFrameset) {
    return ((nsHTMLFramesetFrame*)child)->CanResize(aVertical, aLeft);
  } else {
    return !GetNoResize(child);
  }
}

void 
nsHTMLFramesetFrame::SetBorderResize(PRInt32*                   aChildTypes, 
                                     nsHTMLFramesetBorderFrame* aBorderFrame)
{
  if (aBorderFrame->mVertical) {
    for (int rowX = 0; rowX < mNumRows; rowX++) {
      PRInt32 childX = aBorderFrame->mPrevNeighbor + (rowX * mNumCols);
      if (!CanChildResize(PR_TRUE, PR_FALSE, childX, (FRAMESET == aChildTypes[childX])) ||
          !CanChildResize(PR_TRUE, PR_TRUE, childX+1,(FRAMESET == aChildTypes[childX+1]))) {
        aBorderFrame->mCanResize = PR_FALSE;
      }
    }
  } else {
    PRInt32 childX = aBorderFrame->mPrevNeighbor * mNumCols;
    PRInt32 endX   = childX + mNumCols;
    for (; childX < endX; childX++) {
      if (!CanChildResize(PR_FALSE, PR_FALSE, childX, (FRAMESET == aChildTypes[childX]))) {
        aBorderFrame->mCanResize = PR_FALSE;
      }
    }
    endX = endX + mNumCols;
    for (; childX < endX; childX++) {
      if (!CanChildResize(PR_FALSE, PR_TRUE, childX, (FRAMESET == aChildTypes[childX]))) {
        aBorderFrame->mCanResize = PR_FALSE;
      }
    }
  }
}
  
        
NS_IMETHODIMP
nsHTMLFramesetFrame::VerifyTree() const
{
  // XXX Completely disabled for now; once pseud-frames are reworked
  // then we can turn it back on.
  return NS_OK;
}

void
nsHTMLFramesetFrame::StartMouseDrag(nsIPresContext&            aPresContext, 
                                    nsHTMLFramesetBorderFrame* aBorder, 
                                    nsGUIEvent*                aEvent)
{
  if (mMinDrag == 0) {
    float p2t;
    aPresContext.GetPixelsToTwips(&p2t);
    mMinDrag = NSIntPixelsToTwips(2, p2t);  // set min drag and min frame size to 2 pixels
  }

#if 0
  PRInt32 index;
  IndexOf(aBorder, index);
  NS_ASSERTION((nsnull != aBorder) && (index >= 0), "invalid dragger");
#endif
  nsIView* view;
  GetView(&view);
  if (view) {
    nsIViewManager* viewMan;
    view->GetViewManager(viewMan);
    if (viewMan) {
      PRBool ignore;
      viewMan->GrabMouseEvents(view, ignore);
      NS_RELEASE(viewMan);
      mDragger = aBorder;

      //XXX This should go away!  Border should have own view instead
      view->SetViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN);

      // can't set it to this event's point - it is not in framesetframe coords
      mLastDragPoint.MoveTo(NULL_POINT_X, NULL_POINT_Y);

      gDragInProgress = PR_TRUE;
    }
  }
}
  

void
nsHTMLFramesetFrame::MouseDrag(nsIPresContext& aPresContext, 
                               nsGUIEvent*     aEvent)
{
  if ((mLastDragPoint.x == NULL_POINT_X) &&
      (mLastDragPoint.y == NULL_POINT_Y)) {
    mLastDragPoint.x = aEvent->point.x;
    mLastDragPoint.y = aEvent->point.y;
    return;
  }

  PRInt32 change; // measured positive from left-to-right or top-to-bottom
  if (mDragger->mVertical) {
    change = aEvent->point.x - mLastDragPoint.x;
    if (change < 0) {
      if (mMinDrag >= -change) {
        return;
      }
      PRInt32 index = mDragger->mPrevNeighbor;
      if (mColSizes[index] + change < mMinDrag) {
        change = mMinDrag - mColSizes[index];
      }
    } else if (change > 0) {
      if (mMinDrag >= change) {
        return;
      }
      PRInt32 index = mDragger->mNextNeighbor;
      if (mColSizes[index] - change < mMinDrag) {
        change = mColSizes[index] - mMinDrag;
      }
    }
    mColSizes[mDragger->mPrevNeighbor] += change;
    mColSizes[mDragger->mNextNeighbor] -= change;
  } else {
    change = aEvent->point.y - mLastDragPoint.y;
    if (change < 0) {
      if (mMinDrag >= -change) {
        return;
      }
      PRInt32 index = mDragger->mPrevNeighbor;
      if (mRowSizes[index] + change < mMinDrag) {
        change = mMinDrag - mRowSizes[index];
      }
    } else if (change > 0) {
      if (mMinDrag >= change) {
        return;
      }
      PRInt32 index = mDragger->mNextNeighbor;
      if (mRowSizes[index] - change < mMinDrag) {
        change = mRowSizes[index] - mMinDrag;
      }
    }
    mRowSizes[mDragger->mPrevNeighbor] += change;
    mRowSizes[mDragger->mNextNeighbor] -= change;
  }

  if (change != 0) {
    mDrag.Reset(mDragger->mVertical, mDragger->mPrevNeighbor, change, this);
    nsCOMPtr<nsIPresShell> shell;
    aPresContext.GetShell(getter_AddRefs(shell));
    shell->ResizeReflow(mTopLevelFrameset->mRect.width, mTopLevelFrameset->mRect.height);
  }

  mLastDragPoint.x = aEvent->point.x;
  mLastDragPoint.y = aEvent->point.y;
}  

void
nsHTMLFramesetFrame::EndMouseDrag()
{
  nsIView* view;
  GetView(&view);
  if (view) {
    nsIViewManager* viewMan;
    view->GetViewManager(viewMan);
    if (viewMan) {
      mDragger = nsnull;
      PRBool ignore;
      viewMan->GrabMouseEvents(nsnull, ignore);
      NS_RELEASE(viewMan);
      //XXX This should go away!  Border should have own view instead
      view->ClearViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN);
    }
  }
  gDragInProgress = PR_FALSE;
}  

nsresult
NS_NewHTMLFramesetFrame(nsIFrame*& aResult)
{
  nsIFrame* frame = new nsHTMLFramesetFrame;
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

/*******************************************************************************
 * nsHTMLFramesetBorderFrame
 ******************************************************************************/
nsHTMLFramesetBorderFrame::nsHTMLFramesetBorderFrame(PRInt32 aWidth, 
                                                     PRBool  aVertical, 
                                                     PRBool  aVisibility)
  : nsLeafFrame(), mWidth(aWidth), mVertical(aVertical), mVisibility(aVisibility)
{
   mCanResize    = PR_TRUE;
   mColor        = NO_COLOR;
   mPrevNeighbor = 0;
   mNextNeighbor = 0;
}

nsHTMLFramesetBorderFrame::~nsHTMLFramesetBorderFrame()
{
  //printf("nsHTMLFramesetBorderFrame destructor %p \n", this);
}

void nsHTMLFramesetBorderFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                               const nsHTMLReflowState& aReflowState,
                                               nsHTMLReflowMetrics&     aDesiredSize)
{
  aDesiredSize.width   = aReflowState.availableWidth;
  aDesiredSize.height  = aReflowState.availableHeight;
  aDesiredSize.ascent  = aDesiredSize.width;
  aDesiredSize.descent = 0;
}

void nsHTMLFramesetBorderFrame::SetVisibility(PRBool aVisibility)
{ 
  mVisibility = aVisibility; 
}

void nsHTMLFramesetBorderFrame::SetColor(nscolor aColor)
{ 
  mColor = aColor;
}


NS_IMETHODIMP
nsHTMLFramesetBorderFrame::Reflow(nsIPresContext&          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize);
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_METHOD
nsHTMLFramesetBorderFrame::Paint(nsIPresContext&      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect,
                                 nsFramePaintLayer    aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) {
    return NS_OK;
  }
  //printf("border frame paint %X (%d,%d,%d,%d) \n", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  nscolor WHITE    = NS_RGB(255, 255, 255);
  nscolor bgColor  = NS_RGB(200,200,200);
  nscolor fgColor  = NS_RGB(0,0,0);
  nscolor hltColor = NS_RGB(255,255,255);
  nscolor sdwColor = NS_RGB(128,128,128);

  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground,  bgColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetForeground,  fgColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DShadow,    sdwColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DHighlight, hltColor);
   NS_RELEASE(lookAndFeel);
  }

  float t2p;
  aPresContext.GetTwipsToPixels(&t2p);
  nscoord widthInPixels = NSTwipsToIntPixels(mWidth, t2p);
  float p2t;
  aPresContext.GetPixelsToTwips(&p2t);
  nscoord pixelWidth    = NSIntPixelsToTwips(1, p2t);

  if (widthInPixels <= 0) {
    return NS_OK;
  }

  nscoord x0 = 0;
  nscoord y0 = 0;
  nscoord x1 = (mVertical) ? x0 : mRect.width;
  nscoord y1 = (mVertical) ? mRect.height : x0;

  nscolor color = WHITE;
  if (mVisibility) {
    color = (NO_COLOR == mColor) ? bgColor : mColor;
  }
  aRenderingContext.SetColor(color);
  // draw grey or white first
  for (int i = 0; i < widthInPixels; i++) {
    aRenderingContext.DrawLine (x0, y0, x1, y1);
    if (mVertical) {
      x0 += pixelWidth;
      x1 =  x0;
    } else {
      y0 += pixelWidth;
      y1 =  y0;
    }
  }

  if (!mVisibility) {
    return NS_OK;
  }

  if (widthInPixels >= 5) {
    aRenderingContext.SetColor(hltColor);
    x0 = (mVertical) ? pixelWidth : 0;
    y0 = (mVertical) ? 0 : pixelWidth;
    x1 = (mVertical) ? x0 : mRect.width;
    y1 = (mVertical) ? mRect.height : y0;
    aRenderingContext.DrawLine(x0, y0, x1, y1);
  }

  if (widthInPixels >= 2) {
    aRenderingContext.SetColor(sdwColor);
    x0 = (mVertical) ? mRect.width - (2 * pixelWidth) : 0;
    y0 = (mVertical) ? 0 : mRect.height - (2 * pixelWidth);
    x1 = (mVertical) ? x0 : mRect.width;
    y1 = (mVertical) ? mRect.height : y0;
    aRenderingContext.DrawLine(x0, y0, x1, y1);
  }

  if (widthInPixels >= 1) {
    aRenderingContext.SetColor(fgColor);
    x0 = (mVertical) ? mRect.width - pixelWidth : 0;
    y0 = (mVertical) ? 0 : mRect.height - pixelWidth;
    x1 = (mVertical) ? x0 : mRect.width;
    y1 = (mVertical) ? mRect.height : y0;
    aRenderingContext.DrawLine(x0, y0, x1, y1);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLFramesetBorderFrame::HandleEvent(nsIPresContext& aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus&  aEventStatus)
{
  aEventStatus = nsEventStatus_eIgnore;

  //XXX Mouse setting logic removed.  The remaining logic should also move.
  if (!mCanResize) {
    return NS_OK;
  }

  switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      nsHTMLFramesetFrame* parentFrame = nsnull;
      GetParent((nsIFrame**)&parentFrame);
      parentFrame->StartMouseDrag(aPresContext, this, aEvent);
      aEventStatus = nsEventStatus_eConsumeNoDefault;
	    break;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFramesetBorderFrame::GetFrameForPoint(const nsPoint& aPoint, 
                                            nsIFrame**     aFrame)
{
  *aFrame = this;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFramesetBorderFrame::GetCursor(nsIPresContext& aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor)
{
  if (!mCanResize) {
    aCursor = NS_STYLE_CURSOR_DEFAULT;
  } else {   
    aCursor = (mVertical) ? NS_STYLE_CURSOR_W_RESIZE : NS_STYLE_CURSOR_N_RESIZE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLFramesetBorderFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("FramesetBorder", aResult);
}

/*******************************************************************************
 * nsHTMLFramesetBlankFrame
 ******************************************************************************/

nsHTMLFramesetBlankFrame::~nsHTMLFramesetBlankFrame()
{
  //printf("nsHTMLFramesetBlankFrame destructor %p \n", this);
}

void nsHTMLFramesetBlankFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                              const nsHTMLReflowState& aReflowState,
                                              nsHTMLReflowMetrics&     aDesiredSize)
{
  aDesiredSize.width   = aReflowState.availableWidth;
  aDesiredSize.height  = aReflowState.availableHeight;
  aDesiredSize.ascent  = aDesiredSize.width;
  aDesiredSize.descent = 0;
}

NS_IMETHODIMP
nsHTMLFramesetBlankFrame::Reflow(nsIPresContext&          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize);
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_METHOD
nsHTMLFramesetBlankFrame::Paint(nsIPresContext&      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) {
    return NS_OK;
  }
  nscolor white = NS_RGB(255,255,255);
  aRenderingContext.SetColor (white);
  // XXX FillRect doesn't seem to work
  //aRenderingContext.FillRect (mRect);

  float p2t;
  aPresContext.GetPixelsToTwips(&p2t);
  nscoord x0 = 0;
  nscoord y0 = 0;
  nscoord x1 = x0;
  nscoord y1 = mRect.height;
  nscoord pixel = NSIntPixelsToTwips(1, p2t);

  aRenderingContext.SetColor(white);
  for (int i = 0; i < mRect.width; i += pixel) {
    aRenderingContext.DrawLine (x0, y0, x1, y1);
    x0 += NSIntPixelsToTwips(1, p2t);
    x1 =  x0;
  }

  return NS_OK;
}


NS_IMETHODIMP nsHTMLFramesetBlankFrame::List(FILE*   out, 
                                             PRInt32 aIndent) const
{
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);   // Indent
  fprintf(out, "%p BLANK \n", this);
  return nsLeafFrame::List(out, aIndent);
}

