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

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)aString.get();
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

  nsValueList(nsString& aData) {
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
// See bug 69409 - MathML attributes are not mapped to style.
// This code is not suitable for dynamic updates, for example when the
// rowalign and columalign attributes are changed with JavaScript.
// The code doesn't include hooks for AttributeChanged() notifications.

static void
DestroyValueListFunc(nsIPresContext* aPresContext,
                     nsIFrame*       aFrame,
                     nsIAtom*        aPropertyName,
                     void*           aPropertyValue)
{
  delete NS_STATIC_CAST(nsValueList*, aPropertyValue);
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
  if (presShell) {
    nsCOMPtr<nsIFrameManager> frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
    if (frameManager) {
      frameManager->GetFrameProperty(aTableOrRowFrame, aAttributeAtom,
                                     0, (void**)&valueList);
      if (!valueList) {
        // The property isn't there yet, so set it
        nsAutoString values;
        nsCOMPtr<nsIContent> content;
        aTableOrRowFrame->GetContent(getter_AddRefs(content));
        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, aAttributeAtom, values)) {
          valueList = new nsValueList(values);
          if (valueList) {
            frameManager->SetFrameProperty(aTableOrRowFrame, aAttributeAtom,
                                           valueList, DestroyValueListFunc);
          }
        }
      }
    }
  }

  if (valueList) {
    PRInt32 count = valueList->mArray.Count();
    result = (aIndex < count)
           ? (PRUnichar*)(valueList->mArray[aIndex])
           : (PRUnichar*)(valueList->mArray[count-1]);
  }
  return result;
}

static void
MapAlignAttributesInto(nsIPresContext* aPresContext,
                       nsIContent*     aCellContent,
                       nsIFrame*       aCellFrame,
                       nsIFrame*       aCellInnerFrame)
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
  if (NS_SUCCEEDED(rv)) {
    // see if the rowalign attribute is not already set
    nsIAtom* atom = nsMathMLAtoms::rowalign_;
    rv = aCellContent->GetAttr(kNameSpaceID_None, atom, value);
    if (NS_CONTENT_ATTR_NOT_THERE == rv) {
      // see if the rowalign attribute is specified on the row
      rowalign = GetAlignValueAt(aPresContext, rowFrame, atom, index);
      if (!rowalign) {
      	// see if the rowalign attribute is specified on the table
        rowalign = GetAlignValueAt(aPresContext, tableFrame, atom, index);
      }
    }
  }

  //////////////////////////////////////
  // update column on the cell

  PRUnichar* columnalign = nsnull;
  rv = cellFrame->GetColIndex(index);
  if (NS_SUCCEEDED(rv)) {
    // see if the columnalign attribute is not already set
    nsIAtom* atom = nsMathMLAtoms::columnalign_;
    rv = aCellContent->GetAttr(kNameSpaceID_None, atom, value);
    if (NS_CONTENT_ATTR_NOT_THERE == rv) {
      // see if the columnalign attribute is specified on the row
      columnalign = GetAlignValueAt(aPresContext, rowFrame, atom, index);
      if (!columnalign) {
      	// see if the columnalign attribute is specified on the table
        columnalign = GetAlignValueAt(aPresContext, tableFrame, atom, index);
      }
    }
  }

  // set the attributes without notifying that we want a reflow
  if (rowalign) {
    value.Assign(rowalign);
    aCellContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::rowalign_,
                          value, PR_FALSE);
  }
  if (columnalign) {
    value.Assign(columnalign);
    aCellContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::columnalign_,
                          value, PR_FALSE);
  }
  // then, re-resolve the style contexts in our subtree
  if (rowalign || columnalign) {
    nsCOMPtr<nsIStyleContext> parentContext;
    nsCOMPtr<nsIStyleContext> newContext;
    rowFrame->GetStyleContext(getter_AddRefs(parentContext));
    aPresContext->ResolveStyleContextFor(aCellContent, parentContext,
                                         PR_FALSE, getter_AddRefs(newContext));
    nsCOMPtr<nsIStyleContext> oldContext;
    aCellFrame->GetStyleContext(getter_AddRefs(oldContext));
    if (oldContext != newContext) {
      aCellFrame->SetStyleContext(aPresContext, newContext);
      aPresContext->ReParentStyleContext(aCellInnerFrame, newContext);
    }
  }
}

// --------
// implementation of nsMathMLmtableOuterFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtableOuterFrame, nsMathMLFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtableOuterFrame, nsMathMLFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLmtableOuterFrame, nsTableOuterFrame, nsMathMLFrame)

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

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Init(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParent,
                               nsIStyleContext* aContext,
                               nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsTableOuterFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // now, if our parent implements the nsIMathMLFrame interface, we inherit
  // its scriptlevel and displaystyle. If the parent later wishes to increment
  // with other values, it will do so in its SetInitialChildList() method.

  // XXX the REC says that by default, displaystyle=false in <mtable>

  nsIMathMLFrame* mathMLFrame = nsnull;
  nsresult res = aParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (NS_SUCCEEDED(res) && mathMLFrame) {
    nsPresentationData parentData;
    mathMLFrame->GetPresentationData(parentData);

    mPresentationData.mstyle = parentData.mstyle;
    mPresentationData.scriptLevel = parentData.scriptLevel;
    if (NS_MATHML_IS_DISPLAYSTYLE(parentData.flags))
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    else
      mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
  }
  else {
    // see if our parent has 'display: block'
    // XXX should we restrict this to the top level <math> parent ?
    const nsStyleDisplay* display;
    aParent->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
    if (display->mDisplay == NS_STYLE_DISPLAY_BLOCK) {
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    }
  }

  // see if the displaystyle attribute is there and let it override what we inherited
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsMathMLContainerFrame::GetAttribute(mContent, nsnull,
               nsMathMLAtoms::displaystyle_, value)) {
    if (value.Equals(NS_LITERAL_STRING("true"))) {
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    }
    else if (value.Equals(NS_LITERAL_STRING("false"))) {
      mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
    }
  }

  return rv;
}

// helper to let the update of presentation data pass through
// a subtree that may contain non-math container frames
void
UpdatePresentationDataFor(nsIPresContext* aPresContext,
                          nsIFrame*       aFrame,
                          PRInt32         aScriptLevelIncrement,
                          PRUint32        aFlagsValues,
                          PRUint32        aFlagsToUpdate)
{
  nsIMathMLFrame* mathMLFrame = nsnull;
  nsresult rv = aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (NS_SUCCEEDED(rv) && mathMLFrame) {
    // update
    mathMLFrame->UpdatePresentationData(
      aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
  }
  // propagate down the subtrees
  nsIFrame* childFrame;
  aFrame->FirstChild(aPresContext, nsnull, &childFrame);
  while (childFrame) {
    UpdatePresentationDataFor(aPresContext, childFrame,
      aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
    childFrame->GetNextSibling(&childFrame);
  }
}

// helper to let the scriptstyle re-resolution pass through
// a subtree that may contain non-math container frames
void
ReResolveScriptStyleFor(nsIPresContext*  aPresContext,
                        nsIFrame*        aFrame,
                        PRInt32          aScriptLevel)
{
  nsIFrame* childFrame = nsnull;
  nsCOMPtr<nsIStyleContext> styleContext;
  aFrame->GetStyleContext(getter_AddRefs(styleContext));
  aFrame->FirstChild(aPresContext, nsnull, &childFrame);
  while (childFrame) {
    nsIMathMLFrame* mathMLFrame;
    nsresult res = childFrame->QueryInterface(
      NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (NS_SUCCEEDED(res) && mathMLFrame) {
      mathMLFrame->ReResolveScriptStyle(aPresContext, styleContext, aScriptLevel);
    }
    else {
      ReResolveScriptStyleFor(aPresContext, childFrame, aScriptLevel);
    }
    childFrame->GetNextSibling(&childFrame);
  }
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                                            PRInt32         aFirstIndex,
                                                            PRInt32         aLastIndex,
                                                            PRInt32         aScriptLevelIncrement,
                                                            PRUint32        aFlagsValues,
                                                            PRUint32        aFlagsToUpdate)
{
  PRInt32 index = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if ((index >= aFirstIndex) &&
        ((aLastIndex <= 0) || ((aLastIndex > 0) && (index <= aLastIndex)))) {
      UpdatePresentationDataFor(aPresContext, childFrame,
        aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
    }
    index++;
    childFrame->GetNextSibling(&childFrame);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::ReResolveScriptStyle(nsIPresContext*  aPresContext,
                                               nsIStyleContext* aParentContext,
                                               PRInt32          aParentScriptLevel)
{
  // pass aParentScriptLevel -- it is as if we were not there...
  ReResolveScriptStyleFor(aPresContext, this, aParentScriptLevel);
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Reflow(nsIPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  nsresult rv;
  // we want to return a table that is centered according to the align attribute

  nsHTMLReflowState reflowState(aReflowState);
  if ((NS_FRAME_FIRST_REFLOW & mState) &&
      (NS_UNCONSTRAINEDSIZE == reflowState.availableWidth)) {
    // We are going to reflow twice because the table frame code is
    // skipping the Pass 2 reflow when, at the Pass 1 reflow, the available
    // size is unconstrained. Skipping the Pass2 messes the MathML vertical
    // alignments that are resolved during the reflow of cell frames.

    nscoord oldAvailableWidth = reflowState.availableWidth;
    nscoord oldComputedWidth = reflowState.mComputedWidth;

    reflowState.availableWidth = NS_UNCONSTRAINEDSIZE;
    reflowState.mComputedWidth = NS_UNCONSTRAINEDSIZE;
    reflowState.reason = eReflowReason_Initial;

    rv = nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, reflowState, aStatus);

    // The second reflow will just be a reflow with a constrained width
    reflowState.availableWidth = aDesiredSize.width;
    reflowState.mComputedWidth = oldComputedWidth;
    reflowState.reason = eReflowReason_StyleChange;

    mState &= ~NS_FRAME_FIRST_REFLOW;
  }
  else if (mRect.width) {
    reflowState.availableWidth = mRect.width;
  }

  rv = nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
  NS_ASSERTION(aDesiredSize.height >= 0, "illegal height for mtable");
  NS_ASSERTION(aDesiredSize.width >= 0, "illegal width for mtable");

  // see if the user has set the align attribute on the <mtable>
  // XXX should we also check <mstyle> ?
  nsAutoString value;
  PRBool alignAttribute = PR_FALSE;

  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsMathMLContainerFrame::GetAttribute(mContent, nsnull,
                   nsMathMLAtoms::align_, value)) {
    if (value.Equals(NS_LITERAL_STRING("top"))) {
      aDesiredSize.ascent = 0;
      aDesiredSize.descent = aDesiredSize.height;
      alignAttribute = PR_TRUE;
    }
    else if (value.Equals(NS_LITERAL_STRING("bottom"))) {
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
      alignAttribute = PR_TRUE;
    }
    else if (value.Equals(NS_LITERAL_STRING("center")) ||
             value.Equals(NS_LITERAL_STRING("baseline"))) {
      aDesiredSize.ascent = aDesiredSize.height/2;
      aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;
      alignAttribute = PR_TRUE;
    }
  }
  if (!alignAttribute) {
    // by default, center about the axis

    const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
      mStyleContext->GetStyleData(eStyleStruct_Font));
    nsCOMPtr<nsIFontMetrics> fm;
    aPresContext->GetMetricsFor(font->mFont, getter_AddRefs(fm));

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

  return rv;
}


// --------
// implementation of nsMathMLmtdInnerFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtdInnerFrame, nsMathMLFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtdInnerFrame, nsMathMLFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLmtdInnerFrame, nsBlockFrame, nsMathMLFrame)

nsresult
NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmtdInnerFrame* it = new (aPresShell) nsMathMLmtdInnerFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmtdInnerFrame::nsMathMLmtdInnerFrame()
{
}

nsMathMLmtdInnerFrame::~nsMathMLmtdInnerFrame()
{
}

NS_IMETHODIMP
nsMathMLmtdInnerFrame::Init(nsIPresContext*  aPresContext,
                       nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIStyleContext* aContext,
                       nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsBlockFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // record that children that are ignorable whitespace should be excluded
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  // now, get our outermost parent that implements the nsIMathMLFrame interface,
  // we will inherit its scriptlevel and displaystyle. If that parent later wishes
  // to increment with other values, it will do so in its SetInitialChildList() method.
  nsIFrame* parent = aParent;
  while (parent) {
    nsIMathMLFrame* mathMLFrame = nsnull;
    nsresult res = parent->QueryInterface(
      NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (NS_SUCCEEDED(res) && mathMLFrame) {
    	nsPresentationData parentData;
      mathMLFrame->GetPresentationData(parentData);
      mPresentationData.mstyle = parentData.mstyle;
      mPresentationData.scriptLevel = parentData.scriptLevel;
      if (NS_MATHML_IS_DISPLAYSTYLE(parentData.flags))
        mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
      else
        mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
      break;
    }
    parent->GetParent(&parent);
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLmtdInnerFrame::ReResolveScriptStyle(nsIPresContext*  aPresContext,
                                       nsIStyleContext* aParentContext,
                                       PRInt32          aParentScriptLevel)
{
  // pass aParentScriptLevel -- it is as if we were not there...
  ReResolveScriptStyleFor(aPresContext, this, aParentScriptLevel);
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmtdInnerFrame::Reflow(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  // Map attributes to style (hopefully, bug 69409 will eventually help here).
  if (NS_FRAME_FIRST_REFLOW & mState) {
    MapAlignAttributesInto(aPresContext, mContent, mParent, this);
  }

  // Let the base class do the reflow
  nsresult rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // more about <maligngroup/> and <malignmark/> later
  // ...
  return rv;
}
