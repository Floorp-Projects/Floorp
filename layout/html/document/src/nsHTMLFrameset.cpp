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

#include "nsHTMLFrameset.h"
#include "nsHTMLContainer.h"
#include "nsLeafFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsIWebShell.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsRepository.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIWebFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsHTMLAtoms.h"
#include "nsIScrollableView.h"
#include "nsStyleCoord.h"
#include "nsIStyleContext.h"
#include "nsCSSLayout.h"
#include "nsHTMLBase.h"
#include "nsIDocumentLoader.h"
class nsHTMLIFrame;
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebFrameIID, NS_IWEBFRAME_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
static NS_DEFINE_IID(kCDocumentLoaderCID, NS_DOCUMENTLOADER_CID);

static NS_DEFINE_IID(kIContentViewerContainerIID,
                     NS_ICONTENT_VIEWER_CONTAINER_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID, NS_IDOCUMENTLOADER_IID);


/*******************************************************************************
 * nsHTMLFramesetFrame
 ******************************************************************************/
PRInt32 nsHTMLFramesetFrame::gMaxNumRowColSpecs = 100;

nsHTMLFramesetFrame::nsHTMLFramesetFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
  mNumRows  = 0;
  mRowSpecs = nsnull;
  mRowSizes = nsnull;
  mNumCols  = 0;
  mColSpecs = nsnull;
  mColSizes = nsnull;
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

/**
  * Translate the rows/cols specs into an array of integer sizes for
  * each cell in the frameset. The sizes are computed by first
  * rationalizing the spec into a set of percentages that total
  * 100. Then the cell sizes are computed.
  */
  // XXX This doesn't do a good job of translating "*,625,*" specs
void nsHTMLFramesetFrame::CalculateRowCol(nsIPresContext* aPresContext, nscoord aSize, 
                                          PRInt32 aNumSpecs, nsFramesetSpec* aSpecs, 
                                          nscoord* aValues)
{
  // Total up the three different type of grid cell
  // percentages. Transfer the resulting values into result for
  // temporary storage (we don't overwrite the cell specs)
  PRInt32 free    = 0;
  PRInt32 percent = 0;
  PRInt32 pixel   = 0;
  float p2t = aPresContext->GetPixelsToTwips();

  int i; // feeble compiler
  for (i = 0; i < aNumSpecs; i++) {
    switch (aSpecs[i].mUnit) {
      case eFramesetUnit_Pixel:
        // Now that we know the size we are laying out in, turn fixed
        // pixel dimensions into percents.
        // XXX maybe use UnitConverter for proper rounding? MMP
        aValues[i] = (nscoord) NS_TO_INT_ROUND(100 * p2t * aSpecs[i].mValue / aSize);
        if (aValues[i] < 1) aValues[i] = 1;
        pixel += aValues[i];
        break;
      case eFramesetUnit_Percent:
        aValues[i] = aSpecs[i].mValue;
        percent += aValues[i];
        break;
      case eFramesetUnit_Free:
        aValues[i] = aSpecs[i].mValue;
        free += aValues[i];
        break;
    }
  }

  if (percent + pixel < 100) {
    // The user didn't explicitly use up all the space. Spread the
    // left over space to the free percentage cells first, then to
    // the normal percentage cells second, and finally to the fixed
    // cells as a last resort.
    if (free != 0) {
      // We have some free percentage cells that want to soak up the
      // excess space. Spread out the left over space to those cells.
      int used = 0;
      int last = -1;
      int leftOver = 100 - (percent + pixel);
      for (int i = 0; i < aNumSpecs; i++) {
        if (eFramesetUnit_Free == aSpecs[i].mUnit) {
          aValues[i] = aValues[i] * leftOver / free;
          if (aValues[i] < 1) aValues[i] = 1;
          used += aValues[i];
          last = i;
        }
      }
      int remainder = 100 - percent - pixel - used;
      if ((remainder > 0) && (last >= 0)) {
        // Slop the extra space into the last qualifying cell
        aValues[last] += remainder;
      }
    } else if (percent != 0) {
      // We have no free cells but have some normal percentage
      // cells. Distribute the extra space among them.
      int used = 0;
      int last = -1;
      int leftOver = 100 - pixel;
      for (int i = 0; i < aNumSpecs; i++) {
        if (eFramesetUnit_Percent == aSpecs[i].mUnit) {
          aValues[i] = aValues[i] * leftOver / percent;
          used += aValues[i];
          last = i;
        }
      }
      if ((used < leftOver) && (last >= 0)) {
        // Slop the extra space into the last qualifying cell
        aValues[last] = aValues[last] + (leftOver - used);
      }
    } else {
      // Give the leftover space to the fixed percentage
      // cells. Recall that at the start of this method we converted
      // the fixed pixel values into percentages.
      int used = 0;
      for (int i = 0; i < aNumSpecs; i++) {
        aValues[i] = aValues[i] * 100 / pixel;
        used += aValues[i];
      }
      if ((used < 100) && (aNumSpecs > 0)) {
        // Slop the extra space into the last cell
        aValues[aNumSpecs - 1] += (100 - used);
      }
    }
  } else if (percent + pixel > 100) {
    // The user overallocated the space. We need to shrink something.
    // If there is not too much fixed percentage added, we can
    // just shrink the normal percentage cells to make things fit.
    if (pixel <= 100) {
      int used = 0;
      int last = -1;
      int val = 100 - pixel;
      for (int i = 0; i < aNumSpecs; i++) {
        if (eFramesetUnit_Percent == aSpecs[i].mUnit) {
          aValues[i] = aValues[i] * val / percent;
          used += aValues[i];
          last = i;
        }
      }
      // Since integer division always truncates, we either made
      // it fit exactly, or overcompensated and made it too small.
      if ((used < val) && (last >= 0)) {
        aValues[last] += (val - used);
      }
    } else {
      // There is too much fixed percentage as well. We will just
      // shrink all the cells.
      int used = 0;
      for (int i = 0; i < aNumSpecs; i++) {
        if (eFramesetUnit_Free == aSpecs[i].mUnit) {
          aValues[i] = 0;
        } else {
          aValues[i] = aValues[i] * 100 / (percent + pixel);
        }
        used += aValues[i];
      }
      if ((used < 100) && (aNumSpecs > 0)) {
        aValues[aNumSpecs-1] += (100 - used);
      }
    }
  }

  // Now map the result which contains nothing but percentages into
  // fixed pixel values.
  int sum = 0;
  for (i = 0; i < aNumSpecs; i++) {
    aValues[i] = (aValues[i] * aSize) / 100;
    sum += aValues[i];
  }
  //Assert.Assertion(sum <= aSize);
  if ((sum < aSize) && (aNumSpecs > 0)) {
    // Any remaining pixels (from roundoff) go into the last cell
    aValues[aNumSpecs-1] += aSize - sum;
  }
}

PRIntn
nsHTMLFramesetFrame::GetSkipSides() const
{
  return 0;
}

void 
nsHTMLFramesetFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                    const nsReflowState& aReflowState,
                                    nsReflowMetrics& aDesiredSize)
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
  nsIContent* content;
  aChild->GetContent(content);
  nsIContent* contentParent = content->GetParent();
  nsIAtom* tag = ((nsHTMLTagContent*)contentParent)->GetTag();
  nsHTMLFramesetFrame* parent = nsnull;
  if (nsHTMLAtoms::frameset == tag) {
    aChild->GetGeometricParent((nsIFrame*&)parent);
  }
  NS_RELEASE(contentParent);
  NS_RELEASE(content);
  NS_RELEASE(tag);

  return parent;
}

void nsHTMLFramesetFrame::GetSizeOfChildAt(PRInt32 aIndexInParent, nsSize& aSize, nsPoint& aCellIndex)
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


void nsHTMLFramesetFrame::GetSizeOfChild(nsIFrame* aChild, 
                                         nsSize& aSize)
{
  // Reflow only creates children frames for <frameset> and <frame> content.
  // this assumption is used here
  int i = 0;
  for (nsIFrame* child = mFirstChild; child; child->GetNextSibling(child)) {
    if (aChild == child) {
      nsPoint ignore;
      GetSizeOfChildAt(i, aSize, ignore);
    }
    i++;
  }
  aSize.width  = 0;
  aSize.height = 0;
}  


  
NS_IMETHODIMP
nsHTMLFramesetFrame::Paint(nsIPresContext& aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect)
{
//printf("frameset paint %d (%d,%d,%d,%d) ", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
}

void nsHTMLFramesetFrame::ParseRowCol(nsIAtom* aAttrType, PRInt32& aNumSpecs, nsFramesetSpec** aSpecs) 
{
  nsHTMLValue value;
  nsAutoString rowsCols;
  nsHTMLFrameset* content = (nsHTMLFrameset*)mContent;
  if (eContentAttr_HasValue == content->GetAttribute(aAttrType, value)) {
    if (eHTMLUnit_String == value.GetUnit()) {
      value.GetStringValue(rowsCols);
      nsFramesetSpec* specs = new nsFramesetSpec[gMaxNumRowColSpecs];
      aNumSpecs = ParseRowColSpec(rowsCols, gMaxNumRowColSpecs, specs);
      *aSpecs = new nsFramesetSpec[aNumSpecs];
      for (int i = 0; i < aNumSpecs; i++) {
        (*aSpecs)[i] = specs[i];
      }
      delete [] specs;
      return;
    }
  }
  aNumSpecs = 1; 
  *aSpecs = new nsFramesetSpec[1];
  aSpecs[0]->mUnit  = eFramesetUnit_Free;
  aSpecs[0]->mValue = 1;
}

/**
  * Translate a "rows" or "cols" spec into an array of nsFramesetSpecs
  */
PRInt32 
nsHTMLFramesetFrame::ParseRowColSpec(nsString& aSpec, PRInt32 aMaxNumValues, 
                                     nsFramesetSpec* aSpecs) 
{
  static const PRUnichar ASTER('*');
  static const PRUnichar PERCENT('%');
  static const PRUnichar COMMA(',');

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
    PRInt32 value = 1;
    aSpecs[i].mUnit = eFramesetUnit_Pixel;
    if (end > start) {
      PRInt32 numberEnd = end - 1;
      PRUnichar ch = aSpec.CharAt(numberEnd);
      if (ASTER == ch) {
        aSpecs[i].mUnit = eFramesetUnit_Free;
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

NS_IMETHODIMP
nsHTMLFramesetFrame::Reflow(nsIPresContext&      aPresContext,
                            nsReflowMetrics&     aDesiredSize,
                            const nsReflowState& aReflowState,
                            nsReflowStatus&      aStatus)
{
  PRBool firstTime = (0 == mNumRows);

  if (firstTime) {  // row, col specs have not been parsed 
     ParseRowCol(nsHTMLAtoms::rows, mNumRows, &mRowSpecs);
     ParseRowCol(nsHTMLAtoms::cols, mNumCols, &mColSpecs);
     mRowSizes = new nscoord[mNumRows];
     mColSizes = new nscoord[mNumCols];
  }

  // XXX check to see if our size actually changes before recomputing

  // Always get the size so that the caller knows how big we are
  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize);

  nscoord width  = (aDesiredSize.width <= aReflowState.maxSize.width)
    ? aDesiredSize.width : aReflowState.maxSize.width;
  nscoord height = (aDesiredSize.height <= aReflowState.maxSize.height)
    ? aDesiredSize.height : aReflowState.maxSize.height;

  CalculateRowCol(&aPresContext, width, mNumCols, mColSpecs, mColSizes);
  CalculateRowCol(&aPresContext, height, mNumRows, mRowSpecs, mRowSizes);

  // create the children frames; skip those which aren't frameset or frame
  if (firstTime) {
    mChildCount = 0;
    nsIFrame* lastFrame = nsnull;
    nsHTMLFrameset* content = (nsHTMLFrameset*)mContent;
    PRInt32 numChildren = content->ChildCount();
    for (int i = 0; i < numChildren; i++) {
      nsHTMLTagContent* child = (nsHTMLTagContent*)(content->ChildAt(i));
      if (nsnull == child) {
        continue;
      }
      nsIAtom* tag = child->GetTag();
      if ((nsHTMLAtoms::frameset == tag) || (nsHTMLAtoms::frame == tag)) {
        nsIFrame* frame;
        nsresult result = nsHTMLBase::CreateFrame(&aPresContext, this, child, nsnull, frame); 
        NS_RELEASE(child);
        if (NS_OK != result) {
          return result;
        }
        if (nsnull == lastFrame) {
          mFirstChild = frame;
        } else {
          lastFrame->SetNextSibling(frame);
        }
        lastFrame = frame;
        mChildCount++;
      }
    }
  }

  // reflow the children
  PRInt32 lastRow = 0;
  PRInt32 i       = 0;
  nsPoint offset(0,0);
  for (nsIFrame* child = mFirstChild; child; child->GetNextSibling(child)) {
    nsSize size;
    nsPoint cellIndex;
    GetSizeOfChildAt(i, size, cellIndex);
    nsReflowState reflowState(child, aReflowState, size);

    child->WillReflow(aPresContext);
    nsReflowMetrics metrics(nsnull);
    metrics.width = size.width;
    metrics.height= size.height;
    aStatus = ReflowChild(child, &aPresContext, metrics, reflowState);
    NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
    // Place and size the child
    nsRect rect(offset.x, offset.y, size.width, size.height);
    child->SetRect(rect);
    child->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);

    if (lastRow != cellIndex.y) {  // changed to next row
      offset.x = 0;
      offset.y += metrics.height;
    } else {                       // in same row
      offset.x += metrics.width;
    }
    lastRow = cellIndex.y;
    i++;
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

/*******************************************************************************
 * nsHTMLFrameset
 ******************************************************************************/
nsHTMLFrameset::nsHTMLFrameset(nsIAtom* aTag, nsIWebShell* aParentWebShell)
  : nsHTMLContainer(aTag), mParentWebShell(aParentWebShell)
{
}

nsHTMLFrameset::~nsHTMLFrameset()
{
  mParentWebShell = nsnull;
}

void nsHTMLFrameset::SetAttribute(nsIAtom* aAttribute, const nsString& aString)
{
  nsHTMLValue val;
  if (ParseImageProperty(aAttribute, aString, val)) { // convert width or height to pixels
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
  nsHTMLContainer::SetAttribute(aAttribute, aString);
}

void nsHTMLFrameset::MapAttributesInto(nsIStyleContext* aContext, 
                                       nsIPresContext* aPresContext)
{
  MapImagePropertiesInto(aContext, aPresContext);
  MapImageBorderInto(aContext, aPresContext, nsnull);
}

nsresult
nsHTMLFrameset::CreateFrame(nsIPresContext*  aPresContext,
                            nsIFrame*        aParentFrame,
                            nsIStyleContext* aStyleContext,
                            nsIFrame*&       aResult)
{
  nsIFrame* frame = new nsHTMLFramesetFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  frame->SetStyleContext(aPresContext, aStyleContext);
  return NS_OK;
}

nsresult
NS_NewHTMLFrameset(nsIHTMLContent** aInstancePtrResult,
                   nsIAtom* aTag, nsIWebShell* aWebShell)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsHTMLFrameset(aTag, aWebShell);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}



