/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsAreaFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleChangeList.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsVoidArray.h"
#include "nsLayoutAtoms.h"
#include "nsIFrameManager.h"
#include "nsTableOuterFrame.h"
#include "nsTableCellFrame.h"

#include "nsMathMLmtableFrame.h"

//
// <mtable> -- table or matrix - implementation
//

// helper function to perform an in-place split of a space-delimited string,
// and return an array of pointers for the beginning of each segment, i.e., 
// aOffset[0] is the first string, aOffset[1] is the second string, etc.
// Used to parse attributes like columnalign='left right', rowalign='top bottom'
static void 
SplitString(nsString&    aString, // [IN/OUT]
            nsVoidArray& aOffset) // [OUT]
{
  static const PRUnichar kNullCh = PRUnichar('\0');

  aString.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)aString.GetUnicode();
  PRUnichar* end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (PR_FALSE == nsCRT::IsAsciiSpace(*end))) { // look for space or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      aOffset.AppendElement(start); // record the beginning of this segment
    }

    start = ++end;
  }
}

struct nsValueList
{
	nsString    mData;
	nsVoidArray mArray;

  nsValueList(nsString& aData)
  {
    mData.Assign(aData);
    SplitString(mData, mArray);
  }
};

// Each rowalign='top bottom' or columnalign='left right center' (from
// <mtable> or <mtr>) is split once (lazily) into a nsValueList which is
// stored in the frame manager. Cell frames query the frame manager
// to see what values apply to them.

// XXX these are essentially temporary hacks until the content model
// caters for MathML and the Style System has some provisions for MathML.
// This code is not suitable for dynamic updates, for example when the 
// rowalign and columalign attributes are changed with JavaScript.
// The code doesn't include hooks for AttributeChanged() notifications.

static void
DestroyValueListFunc(nsIPresContext* aPresContext,
                     nsIFrame*       aFrame,
                     nsIAtom*        aPropertyName,
                     void*           aPropertyValue)
{
  if (aPropertyValue)
  {
    delete (nsValueList *)aPropertyValue;
  }
}

static PRUnichar*
GetAlignValueAt(nsIPresContext* aPresContext,
                nsIFrame*       aTableOrRowFrame,
                nsIAtom*        aAttributeAtom,
                PRInt32         aIndex)
{
	PRUnichar* result = nsnull;
  nsValueList* valueList = nsnull;

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (presShell)
  {
    nsCOMPtr<nsIFrameManager> frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
    if (frameManager)
    {
      frameManager->GetFrameProperty(aTableOrRowFrame, aAttributeAtom, 
                                     0, (void**)&valueList);
      if (!valueList)
      {
        // The property isn't there yet, so set it
        nsAutoString values;
        nsCOMPtr<nsIContent> content;
        aTableOrRowFrame->GetContent(getter_AddRefs(content));
        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, aAttributeAtom, values))
        {
          valueList = new nsValueList(values);
          if (valueList)
          {
            frameManager->SetFrameProperty(aTableOrRowFrame, aAttributeAtom,
                                           valueList, DestroyValueListFunc);
          }
        }
      }
    }
  }

  if (valueList)
  {
    PRInt32 count = valueList->mArray.Count();
    result = (aIndex < count)
           ? (PRUnichar*)(valueList->mArray[aIndex])
           : (PRUnichar*)(valueList->mArray[count-1]);
  }
  return result;
}

static void
MapAlignAttributesInto(nsIPresContext* aPresContext,
                       nsIFrame*       aCellFrame,
                       nsIContent*     aCellContent)
{
	nsresult rv;
	PRInt32 index;
  nsAutoString value;

  nsTableCellFrame* cellFrame;
  cellFrame = NS_STATIC_CAST(nsTableCellFrame*, aCellFrame);

  nsIFrame* tableFrame;
  nsIFrame* rowgroupFrame;
  nsIFrame* rowFrame;

  cellFrame->GetParent(&rowFrame);
  rowFrame->GetParent(&rowgroupFrame);
  rowgroupFrame->GetParent(&tableFrame);

  //////////////////////////////////////
  // update rowalign on the cell

  PRUnichar* rowalign = nsnull;
  rv = cellFrame->GetRowIndex(index);
  if (NS_SUCCEEDED(rv)) 
  {
    // see if the rowalign attribute is not already set
    nsIAtom* atom = nsMathMLAtoms::rowalign_;
    rv = aCellContent->GetAttribute(kNameSpaceID_None, atom, value);
    if (NS_CONTENT_ATTR_NOT_THERE == rv)
    {
      // see if the rowalign attribute is specified on the row
      rowalign = GetAlignValueAt(aPresContext, rowFrame, atom, index);
      if (!rowalign)
      {
      	// see if the rowalign attribute is specified on the table
        rowalign = GetAlignValueAt(aPresContext, tableFrame, atom, index);
      }
    }
  }

  //////////////////////////////////////
  // update column on the cell

  PRUnichar* columnalign = nsnull;
  rv = cellFrame->GetColIndex(index);
  if (NS_SUCCEEDED(rv)) 
  {
    // see if the columnalign attribute is not already set
    nsIAtom* atom = nsMathMLAtoms::columnalign_;
    rv = aCellContent->GetAttribute(kNameSpaceID_None, atom, value);
    if (NS_CONTENT_ATTR_NOT_THERE == rv)
    {
      // see if the columnalign attribute is specified on the row
      columnalign = GetAlignValueAt(aPresContext, rowFrame, atom, index);
      if (!columnalign)
      {
      	// see if the columnalign attribute is specified on the table
        columnalign = GetAlignValueAt(aPresContext, tableFrame, atom, index);
      }
    }
  }

  // coalesce notifications
  if (rowalign && columnalign) 
  {
    value.Assign(rowalign);
    aCellContent->SetAttribute(kNameSpaceID_None, nsMathMLAtoms::rowalign_, 
                               value, PR_FALSE);
    value.Assign(columnalign);
    aCellContent->SetAttribute(kNameSpaceID_None, nsMathMLAtoms::columnalign_, 
                               value, PR_TRUE);
  }
  else if (rowalign) 
  {
    value.Assign(rowalign);
    aCellContent->SetAttribute(kNameSpaceID_None, nsMathMLAtoms::rowalign_, 
                               value, PR_TRUE);
  }
  else if (columnalign) 
  {
    value.Assign(columnalign);
    aCellContent->SetAttribute(kNameSpaceID_None, nsMathMLAtoms::columnalign_, 
                               value, PR_TRUE);
  }
}

// --------
// implementation of nsMathMLmtableOuterFrame

nsresult
NS_NewMathMLmtableOuterFrame (nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmtableOuterFrame* it = new (aPresShell) nsMathMLmtableOuterFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
}

nsMathMLmtableOuterFrame::nsMathMLmtableOuterFrame()
  :nsTableOuterFrame()
{
}

nsMathMLmtableOuterFrame::~nsMathMLmtableOuterFrame()
{
}

NS_INTERFACE_MAP_BEGIN(nsMathMLmtableOuterFrame)
  NS_INTERFACE_MAP_ENTRY(nsIMathMLFrame)
NS_INTERFACE_MAP_END_INHERITING(nsTableOuterFrame)

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLmtableOuterFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLmtableOuterFrame::Release(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Init(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParent,
                               nsIStyleContext* aContext,
                               nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsTableOuterFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  return rv;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Reflow(nsIPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  // we want to return a table that is centered according to the align attribute
  nsresult rv = nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // see if the user has set the align attribute on the <mtable>
  // XXX should we also check <mstyle> ?
  nsAutoString value;
  PRBool alignAttribute = PR_FALSE;

  if (NS_CONTENT_ATTR_HAS_VALUE == 
      nsMathMLContainerFrame::GetAttribute(mContent, nsnull,
                   nsMathMLAtoms::align_, value)) {
    if (value.EqualsWithConversion("top")) {
      aDesiredSize.ascent = 0;
      aDesiredSize.descent = aDesiredSize.height;
      alignAttribute = PR_TRUE;
    }
    else if (value.EqualsWithConversion("bottom")) {
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
      alignAttribute = PR_TRUE;
    }
    else if (value.EqualsWithConversion("center") ||
             value.EqualsWithConversion("baseline")) {
      aDesiredSize.ascent = aDesiredSize.height/2;
      aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;
      alignAttribute = PR_TRUE;
    }
  }
  if (!alignAttribute) {
    // by default, center about the axis

    nsStyleFont font;
    mStyleContext->GetStyle(eStyleStruct_Font, font);
    nsCOMPtr<nsIFontMetrics> fm;
    aReflowState.rendContext->SetFont(font.mFont);
    aReflowState.rendContext->GetFontMetrics(*getter_AddRefs(fm));
    nscoord axisHeight;
    nsMathMLContainerFrame::GetAxisHeight(*aReflowState.rendContext, fm, axisHeight);

    aDesiredSize.ascent = aDesiredSize.height/2 + axisHeight;
    aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;
  }

  // just make-up a bounding metrics
  mBoundingMetrics.Clear();
  mBoundingMetrics.ascent = aDesiredSize.ascent;
  mBoundingMetrics.descent = aDesiredSize.descent;
  mBoundingMetrics.width = aDesiredSize.width;
  mBoundingMetrics.leftBearing = 0;
  mBoundingMetrics.rightBearing = aDesiredSize.width;

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  if ((eReflowReason_Initial == aReflowState.reason) &&
      (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth))
  {
    // XXX Temporary hack! We are going to reflow again because
    // the table frame code is skipping the Pass 2 reflow when,
    // at the Pass 1 reflow, the available size is unconstrained.
    // Skipping the Pass2 messes the alignments...
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
//    ReflowDirtyChild(presShell, mFrames.FirstChild());
    mParent->ReflowDirtyChild(presShell, this);
  }

  return rv;
}


// --------
// implementation of nsMathMLmtdFrame

nsresult
NS_NewMathMLmtdFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmtdFrame* it = new (aPresShell) nsMathMLmtdFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmtdFrame::nsMathMLmtdFrame()
{
}

nsMathMLmtdFrame::~nsMathMLmtdFrame()
{
}

NS_IMETHODIMP
nsMathMLmtdFrame::Reflow(nsIPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
	// XXX temp. hack.
  if (NS_FRAME_FIRST_REFLOW & mState) {
    MapAlignAttributesInto(aPresContext, mParent, mContent);
  }

  // Let the base class do the reflow
  nsresult rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // more about <maligngroup/> and <malignmark/> later
  // ...
  return rv;
}
