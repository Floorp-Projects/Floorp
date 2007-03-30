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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
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

#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsAreaFrame.h"
#include "nsPresContext.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsVoidArray.h"
#include "nsCSSFrameConstructor.h"
#include "nsTableOuterFrame.h"
#include "nsTableFrame.h"
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

  PRUnichar* start = aString.BeginWriting();
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
// stored in the property table. Row/Cell frames query the property table
// to see what values apply to them.

// XXX See bug 69409 - MathML attributes are not mapped to style.

static void
DestroyValueListFunc(void*    aFrame,
                     nsIAtom* aPropertyName,
                     void*    aPropertyValue,
                     void*    aDtorData)
{
  delete NS_STATIC_CAST(nsValueList*, aPropertyValue);
}

static PRUnichar*
GetValueAt(nsIFrame* aTableOrRowFrame,
           nsIAtom*  aAttribute,
           PRInt32   aRowOrColIndex)
{
  nsValueList* valueList = NS_STATIC_CAST(nsValueList*,
    aTableOrRowFrame->GetProperty(aAttribute));
  if (!valueList) {
    // The property isn't there yet, so set it
    nsAutoString values;
    aTableOrRowFrame->GetContent()->GetAttr(kNameSpaceID_None, aAttribute, values);
    if (!values.IsEmpty())
      valueList = new nsValueList(values);
    if (!valueList || !valueList->mArray.Count()) {
      delete valueList; // ok either way, delete is null safe
      return nsnull;
    }
    aTableOrRowFrame->SetProperty(aAttribute, valueList, DestroyValueListFunc);
  }
  PRInt32 count = valueList->mArray.Count();
  return (aRowOrColIndex < count)
         ? (PRUnichar*)(valueList->mArray[aRowOrColIndex])
         : (PRUnichar*)(valueList->mArray[count-1]);
}

#ifdef NS_DEBUG
#define DEBUG_VERIFY_THAT_FRAME_IS(_frame, _expected) \
  NS_ASSERTION(NS_STYLE_DISPLAY_##_expected == _frame->GetStyleDisplay()->mDisplay, "internal error");
#else
#define DEBUG_VERIFY_THAT_FRAME_IS(_frame, _expected)
#endif

// map attributes that depend on the index of the row:
// rowalign, rowlines, XXX need rowspacing too
static void
MapRowAttributesIntoCSS(nsIFrame* aTableFrame,
                        nsIFrame* aRowFrame)
{
  DEBUG_VERIFY_THAT_FRAME_IS(aTableFrame, TABLE);
  DEBUG_VERIFY_THAT_FRAME_IS(aRowFrame, TABLE_ROW);
  PRInt32 rowIndex = ((nsTableRowFrame*)aRowFrame)->GetRowIndex();
  nsIContent* rowContent = aRowFrame->GetContent();
  PRUnichar* attr;

  // see if the rowalign attribute is not already set
  if (!rowContent->HasAttr(kNameSpaceID_None, nsGkAtoms::rowalign_) &&
      !rowContent->HasAttr(kNameSpaceID_None, nsGkAtoms::MOZrowalign)) {
    // see if the rowalign attribute was specified on the table
    attr = GetValueAt(aTableFrame, nsGkAtoms::rowalign_, rowIndex);
    if (attr) {
      // set our special -moz attribute on the row without notifying a reflow
      rowContent->SetAttr(kNameSpaceID_None, nsGkAtoms::MOZrowalign,
                          nsDependentString(attr), PR_FALSE);
    }
  }

  // if we are not on the first row, see if |rowlines| was specified on the table.
  // Note that we pass 'rowIndex-1' because the CSS rule in mathml.css is associated
  // to 'border-top', and it is as if we draw the line on behalf of the previous cell.
  // This way of doing so allows us to handle selective lines, [row]\hline[row][row]',
  // and cases of spanning cells without further complications.
  if (rowIndex > 0 &&
      !rowContent->HasAttr(kNameSpaceID_None, nsGkAtoms::MOZrowline)) {
    attr = GetValueAt(aTableFrame, nsGkAtoms::rowlines_, rowIndex-1);
    if (attr) {
      // set our special -moz attribute on the row without notifying a reflow
      rowContent->SetAttr(kNameSpaceID_None, nsGkAtoms::MOZrowline,
                          nsDependentString(attr), PR_FALSE);
    }
  }
}

// map attributes that depend on the index of the column:
// columnalign, columnlines, XXX need columnwidth and columnspacing too
static void
MapColAttributesIntoCSS(nsIFrame* aTableFrame,
                        nsIFrame* aRowFrame,
                        nsIFrame* aCellFrame)
{
  DEBUG_VERIFY_THAT_FRAME_IS(aTableFrame, TABLE);
  DEBUG_VERIFY_THAT_FRAME_IS(aRowFrame, TABLE_ROW);
  DEBUG_VERIFY_THAT_FRAME_IS(aCellFrame, TABLE_CELL);
  PRInt32 rowIndex, colIndex;
  ((nsTableCellFrame*)aCellFrame)->GetCellIndexes(rowIndex, colIndex);
  nsIContent* cellContent = aCellFrame->GetContent();
  PRUnichar* attr;

  // see if the columnalign attribute is not already set
  if (!cellContent->HasAttr(kNameSpaceID_None, nsGkAtoms::columnalign_) &&
      !cellContent->HasAttr(kNameSpaceID_None, nsGkAtoms::MOZcolumnalign)) {
    // see if the columnalign attribute was specified on the row
    attr = GetValueAt(aRowFrame, nsGkAtoms::columnalign_, colIndex);
    if (!attr) {
      // see if the columnalign attribute was specified on the table
      attr = GetValueAt(aTableFrame, nsGkAtoms::columnalign_, colIndex);
    }
    if (attr) {
      // set our special -moz attribute without notifying a reflow
      cellContent->SetAttr(kNameSpaceID_None, nsGkAtoms::MOZcolumnalign,
                           nsDependentString(attr), PR_FALSE);
    }
  }

  // if we are not on the first column, see if |columnlines| was specified on
  // the table. Note that we pass 'colIndex-1' because the CSS rule in mathml.css
  // is associated to 'border-left', and it is as if we draw the line on behalf
  // of the previous cell. This way of doing so allows us to handle selective lines,
  // e.g., 'r|cl', and cases of spanning cells without further complications.
  if (colIndex > 0 &&
      !cellContent->HasAttr(kNameSpaceID_None, nsGkAtoms::MOZcolumnline)) {
    attr = GetValueAt(aTableFrame, nsGkAtoms::columnlines_, colIndex-1);
    if (attr) {
      // set our special -moz attribute without notifying a reflow
      cellContent->SetAttr(kNameSpaceID_None, nsGkAtoms::MOZcolumnline,
                           nsDependentString(attr), PR_FALSE);
    }
  }
}

// map all attribues within a table -- requires the indices of rows and cells.
// so it can only happen after they are made ready by the table base class.
static void
MapAllAttributesIntoCSS(nsIFrame* aTableFrame)
{
  // mtable is simple and only has one (pseudo) row-group
  nsIFrame* rgFrame = aTableFrame->GetFirstChild(nsnull);
  if (!rgFrame || rgFrame->GetType() != nsGkAtoms::tableRowGroupFrame)
    return;

  nsIFrame* rowFrame = rgFrame->GetFirstChild(nsnull);
  for ( ; rowFrame; rowFrame = rowFrame->GetNextSibling()) {
    DEBUG_VERIFY_THAT_FRAME_IS(rowFrame, TABLE_ROW);
    if (rowFrame->GetType() == nsGkAtoms::tableRowFrame) {
      MapRowAttributesIntoCSS(aTableFrame, rowFrame);
      nsIFrame* cellFrame = rowFrame->GetFirstChild(nsnull);
      for ( ; cellFrame; cellFrame = cellFrame->GetNextSibling()) {
        DEBUG_VERIFY_THAT_FRAME_IS(cellFrame, TABLE_CELL);
        if (IS_TABLE_CELL(cellFrame->GetType())) {
          MapColAttributesIntoCSS(aTableFrame, rowFrame, cellFrame);
        }
      }
    }
  }
}

// the align attribute of mtable can have a row number which indicates
// from where to anchor the table, e.g., top5 means anchor the table at
// the top of the 5th row, axis-1 means anchor the table on the axis of
// the last row (could have been nicer if the REC used the '#' separator,
// e.g., top#5, or axis#-1)

enum eAlign {
  eAlign_top,
  eAlign_bottom,
  eAlign_center,
  eAlign_baseline,
  eAlign_axis
};

static void
ParseAlignAttribute(nsString& aValue, eAlign& aAlign, PRInt32& aRowIndex)
{
  // by default, the table is centered about the axis
  aRowIndex = 0;
  aAlign = eAlign_axis;
  PRInt32 len = 0;
  if (0 == aValue.Find("top")) {
    len = 3; // 3 is the length of 'top'
    aAlign = eAlign_top;
  }
  else if (0 == aValue.Find("bottom")) {
    len = 6; // 6 is the length of 'bottom'
    aAlign = eAlign_bottom;
  }
  else if (0 == aValue.Find("center")) {
    len = 6; // 6 is the length of 'center'
    aAlign = eAlign_center;
  }
  else if (0 == aValue.Find("baseline")) {
    len = 8; // 8 is the length of 'baseline'
    aAlign = eAlign_baseline;
  }
  else if (0 == aValue.Find("axis")) {
    len = 4; // 4 is the length of 'axis'
    aAlign = eAlign_axis;
  }
  if (len) {
    PRInt32 error;
    aValue.Cut(0, len); // aValue is not a const here
    aRowIndex = aValue.ToInteger(&error);
    if (error)
      aRowIndex = 0;
  }
}

#ifdef DEBUG_rbs_off
// call ListMathMLTree(mParent) to get the big picture
static void
ListMathMLTree(nsIFrame* atLeast)
{
  // climb up to <math> or <body> if <math> isn't there
  nsIFrame* f = atLeast;
  for ( ; f; f = f->GetParent()) {
    nsIContent* c = f->GetContent();
    if (!c || c->Tag() == nsGkAtoms::math || c->Tag() == nsGkAtoms::body)
      break;
  }
  if (!f) f = atLeast;
  nsIFrameDebug* fdbg;
  CallQueryInterface(f, &fdbg);
  fdbg->List(stdout, 0);
}
#endif

// --------
// implementation of nsMathMLmtableOuterFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtableOuterFrame, nsMathMLFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtableOuterFrame, nsMathMLFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLmtableOuterFrame, nsTableOuterFrame, nsMathMLFrame)

nsIFrame*
NS_NewMathMLmtableOuterFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtableOuterFrame(aContext);
}

nsMathMLmtableOuterFrame::~nsMathMLmtableOuterFrame()
{
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Init(nsIContent*      aContent,
                               nsIFrame*        aParent,
                               nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsTableOuterFrame::Init(aContent, aParent, aPrevInFlow);
  nsMathMLFrame::MapCommonAttributesIntoCSS(PresContext(), aContent);
  return rv;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // XXX the REC says that by default, displaystyle=false in <mtable>

  // let the base class inherit the scriptlevel and displaystyle from our parent
  nsMathMLFrame::InheritAutomaticData(aParent);

  // see if the displaystyle attribute is there and let it override what we inherited
  if (mContent->Tag() == nsGkAtoms::mtable_)
    nsMathMLFrame::FindAttrDisplaystyle(mContent, mPresentationData);

  return NS_OK;
}

// displaystyle is special in mtable...
// Since UpdatePresentation() and UpdatePresentationDataFromChildAt() can be called
// by a parent, ensure that the displaystyle attribute of mtable takes precedence
NS_IMETHODIMP
nsMathMLmtableOuterFrame::UpdatePresentationData(PRInt32  aScriptLevelIncrement,
                                                 PRUint32 aFlagsValues,
                                                 PRUint32 aWhichFlags)
{
  if (NS_MATHML_HAS_EXPLICIT_DISPLAYSTYLE(mPresentationData.flags)) {
    // our current state takes precedence, disallow updating the displastyle
    aWhichFlags &= ~NS_MATHML_DISPLAYSTYLE;
    aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
  }

  return nsMathMLFrame::UpdatePresentationData(
    aScriptLevelIncrement, aFlagsValues, aWhichFlags);
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::UpdatePresentationDataFromChildAt(PRInt32  aFirstIndex,
                                                            PRInt32  aLastIndex,
                                                            PRInt32  aScriptLevelIncrement,
                                                            PRUint32 aFlagsValues,
                                                            PRUint32 aWhichFlags)
{
  if (NS_MATHML_HAS_EXPLICIT_DISPLAYSTYLE(mPresentationData.flags)) {
    // our current state takes precedence, disallow updating the displastyle
    aWhichFlags &= ~NS_MATHML_DISPLAYSTYLE;
    aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
  }

  nsMathMLContainerFrame::PropagatePresentationDataFromChildAt(this,
    aFirstIndex, aLastIndex, aScriptLevelIncrement, aFlagsValues, aWhichFlags);

  return NS_OK; 
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                           nsIAtom* aAttribute,
                                           PRInt32  aModType)
{
  // Attributes common to MathML tags
  if (nsMathMLFrame::CommonAttributeChangedFor(PresContext(), mContent, aAttribute))
    return NS_OK;

  // Attributes specific to <mtable>:
  // frame         : in mathml.css
  // framespacing  : not yet supported 
  // groupalign    : not yet supported
  // equalrows     : not yet supported 
  // equalcolumns  : not yet supported 
  // displaystyle  : here 
  // align         : in reflow 
  // rowalign      : here
  // rowlines      : here 
  // rowspacing    : not yet supported 
  // columnalign   : here 
  // columnlines   : here 
  // columnspacing : not yet supported 

  // mtable is simple and only has one (pseudo) row-group inside our inner-table
  nsIFrame* tableFrame = mFrames.FirstChild();
  if (!tableFrame || tableFrame->GetType() != nsGkAtoms::tableFrame)
    return NS_OK;
  nsIFrame* rgFrame = tableFrame->GetFirstChild(nsnull);
  if (!rgFrame || rgFrame->GetType() != nsGkAtoms::tableRowGroupFrame)
    return NS_OK;

  // align - just need to issue a dirty (resize) reflow command
  if (aAttribute == nsGkAtoms::align) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eResize);
    return NS_OK;
  }

  // displaystyle - may seem innocuous, but it is actually very harsh --
  // like changing an unit. Blow away and recompute all our automatic
  // presentational data, and issue a style-changed reflow request
  if (aAttribute == nsGkAtoms::displaystyle_) {
    nsMathMLContainerFrame::RebuildAutomaticDataForChildren(mParent);
    nsMathMLContainerFrame::PropagateScriptStyleFor(tableFrame, mPresentationData.scriptLevel);
    PresContext()->PresShell()->
      FrameNeedsReflow(mParent, nsIPresShell::eStyleChange);
    return NS_OK;
  }

  // ...and the other attributes affect rows or columns in one way or another
  nsIAtom* MOZrowAtom = nsnull;
  nsIAtom* MOZcolAtom = nsnull;
  if (aAttribute == nsGkAtoms::rowalign_)
    MOZrowAtom = nsGkAtoms::MOZrowalign;
  else if (aAttribute == nsGkAtoms::rowlines_)
    MOZrowAtom = nsGkAtoms::MOZrowline;
  else if (aAttribute == nsGkAtoms::columnalign_)
    MOZcolAtom = nsGkAtoms::MOZcolumnalign;
  else if (aAttribute == nsGkAtoms::columnlines_)
    MOZcolAtom = nsGkAtoms::MOZcolumnline;

  if (!MOZrowAtom && !MOZcolAtom)
    return NS_OK;

  // clear any cached nsValueList for this table
  tableFrame->DeleteProperty(aAttribute);

  // unset any -moz attribute that we may have set earlier, and re-sync
  nsIFrame* rowFrame = rgFrame->GetFirstChild(nsnull);
  for ( ; rowFrame; rowFrame = rowFrame->GetNextSibling()) {
    if (rowFrame->GetType() == nsGkAtoms::tableRowFrame) {
      if (MOZrowAtom) { // let rows do the work
        rowFrame->GetContent()->UnsetAttr(kNameSpaceID_None, MOZrowAtom, PR_FALSE);
        MapRowAttributesIntoCSS(tableFrame, rowFrame);    
      } else { // let cells do the work
        nsIFrame* cellFrame = rowFrame->GetFirstChild(nsnull);
        for ( ; cellFrame; cellFrame = cellFrame->GetNextSibling()) {
          if (IS_TABLE_CELL(cellFrame->GetType())) {
            cellFrame->GetContent()->UnsetAttr(kNameSpaceID_None, MOZcolAtom, PR_FALSE);
            MapColAttributesIntoCSS(tableFrame, rowFrame, cellFrame);
          }
        }
      }
    }
  }

  // Explicitly request a re-resolve and reflow in our subtree to pick up any changes
  PresContext()->PresShell()->FrameConstructor()->
    PostRestyleEvent(mContent, eReStyle_Self, nsChangeHint_ReflowFrame);

  return NS_OK;
}

nsIFrame*
nsMathMLmtableOuterFrame::GetRowFrameAt(nsPresContext* aPresContext,
                                        PRInt32         aRowIndex)
{
  PRInt32 rowCount, colCount;
  GetTableSize(rowCount, colCount);

  // Negative indices mean to find upwards from the end.
  if (aRowIndex < 0) {
    aRowIndex = rowCount + aRowIndex;
  }
  // aRowIndex is 1-based, so convert it to a 0-based index
  --aRowIndex;

  // if our inner table says that the index is valid, find the row now
  if (0 <= aRowIndex && aRowIndex <= rowCount) {
    nsIFrame* tableFrame = mFrames.FirstChild();
    if (!tableFrame || tableFrame->GetType() != nsGkAtoms::tableFrame)
      return nsnull;
    nsIFrame* rgFrame = tableFrame->GetFirstChild(nsnull);
    if (!rgFrame || rgFrame->GetType() != nsGkAtoms::tableRowGroupFrame)
      return nsnull;
    nsTableIterator rowIter(*rgFrame);
    nsIFrame* rowFrame = rowIter.First();
    for ( ; rowFrame; rowFrame = rowIter.Next()) {
      if (aRowIndex == 0) {
        DEBUG_VERIFY_THAT_FRAME_IS(rowFrame, TABLE_ROW);
        if (rowFrame->GetType() != nsGkAtoms::tableRowFrame)
          return nsnull;

        return rowFrame;
      }
      --aRowIndex;
    }
  }
  return nsnull;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Reflow(nsPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  nsresult rv;
  nsAutoString value;
  // we want to return a table that is anchored according to the align attribute

  rv = nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, aReflowState,
                                 aStatus);
  NS_ASSERTION(aDesiredSize.height >= 0, "illegal height for mtable");
  NS_ASSERTION(aDesiredSize.width >= 0, "illegal width for mtable");

  // see if the user has set the align attribute on the <mtable>
  // XXX should we also check <mstyle> ?
  PRInt32 rowIndex = 0;
  eAlign tableAlign = eAlign_axis;
  GetAttribute(mContent, nsnull, nsGkAtoms::align, value);
  if (!value.IsEmpty()) {
    ParseAlignAttribute(value, tableAlign, rowIndex);
  }

  // adjustments if there is a specified row from where to anchor the table
  // (conceptually: when there is no row of reference, picture the table as if
  // it is wrapped in a single big fictional row at dy = 0, this way of
  // doing so allows us to have a single code path for all cases).
  nscoord dy = 0;
  nscoord height = aDesiredSize.height;
  nsIFrame* rowFrame = nsnull;
  if (rowIndex) {
    rowFrame = GetRowFrameAt(aPresContext, rowIndex);
    if (rowFrame) {
      // translate the coordinates to be relative to us
      nsIFrame* frame = rowFrame;
      height = frame->GetSize().height;
      do {
        dy += frame->GetPosition().y;
        frame = frame->GetParent();
      } while (frame != this);
    }
  }
  switch (tableAlign) {
    case eAlign_top:
      aDesiredSize.ascent = dy;
      break;
    case eAlign_bottom:
      aDesiredSize.ascent = dy + height;
      break;
    case eAlign_center:
      aDesiredSize.ascent = dy + height/2;
      break;
    case eAlign_baseline:
      if (rowFrame) {
        // anchor the table on the baseline of the row of reference
        nscoord rowAscent = ((nsTableRowFrame*)rowFrame)->GetMaxCellAscent();
        if (rowAscent) { // the row has at least one cell with 'vertical-align: baseline'
          aDesiredSize.ascent = dy + rowAscent;
          break;
        }
      }
      // in other situations, fallback to center
      aDesiredSize.ascent = dy + height/2;
      break;
    case eAlign_axis:
    default: {
      // XXX should instead use style data from the row of reference here ?
      aReflowState.rendContext->SetFont(GetStyleFont()->mFont, nsnull);
      nsCOMPtr<nsIFontMetrics> fm;
      aReflowState.rendContext->GetFontMetrics(*getter_AddRefs(fm));
      nscoord axisHeight;
      GetAxisHeight(*aReflowState.rendContext, fm, axisHeight);
      if (rowFrame) {
        // anchor the table on the axis of the row of reference
        // XXX fallback to baseline because it is a hard problem
        // XXX need to fetch the axis of the row; would need rowalign=axis to work better
        nscoord rowAscent = ((nsTableRowFrame*)rowFrame)->GetMaxCellAscent();
        if (rowAscent) { // the row has at least one cell with 'vertical-align: baseline'
          aDesiredSize.ascent = dy + rowAscent;
          break;
        }
      }
      // in other situations, fallback to using half of the height
      aDesiredSize.ascent = dy + height/2 + axisHeight;
    }
  }

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  // just make-up a bounding metrics
  mBoundingMetrics.Clear();
  mBoundingMetrics.ascent = aDesiredSize.ascent;
  mBoundingMetrics.descent = aDesiredSize.height - aDesiredSize.ascent;
  mBoundingMetrics.width = aDesiredSize.width;
  mBoundingMetrics.leftBearing = 0;
  mBoundingMetrics.rightBearing = aDesiredSize.width;

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  return rv;
}

// --------
// implementation of nsMathMLmtableFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtableFrame, nsTableFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtableFrame, nsTableFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED0(nsMathMLmtableFrame, nsTableFrame)

nsIFrame*
NS_NewMathMLmtableFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtableFrame(aContext);
}

nsMathMLmtableFrame::~nsMathMLmtableFrame()
{
}

NS_IMETHODIMP
nsMathMLmtableFrame::SetInitialChildList(nsIAtom*  aListName,
                                         nsIFrame* aChildList)
{
  nsresult rv = nsTableFrame::SetInitialChildList(aListName, aChildList);
  if (NS_FAILED(rv)) return rv;
  MapAllAttributesIntoCSS(this);
  return rv;
}

void
nsMathMLmtableFrame::RestyleTable()
{
  // re-sync MathML specific style data that may have changed
  MapAllAttributesIntoCSS(this);

  // Explicitly request a re-resolve and reflow in our subtree to pick up any changes
  PresContext()->PresShell()->FrameConstructor()->
    PostRestyleEvent(mContent, eReStyle_Self, nsChangeHint_ReflowFrame);
}

// --------
// implementation of nsMathMLmtrFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtrFrame, nsTableRowFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtrFrame, nsTableRowFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED0(nsMathMLmtrFrame, nsTableRowFrame)

nsIFrame*
NS_NewMathMLmtrFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtrFrame(aContext);
}

nsMathMLmtrFrame::~nsMathMLmtrFrame()
{
}

NS_IMETHODIMP
nsMathMLmtrFrame::Init(nsIContent* aContent,
                       nsIFrame*   aParent,
                       nsIFrame*   aPrevInFlow)
{
  nsresult rv = nsTableRowFrame::Init(aContent, aParent, aPrevInFlow);
  nsMathMLFrame::MapCommonAttributesIntoCSS(PresContext(), aContent);
  return rv;
}

NS_IMETHODIMP
nsMathMLmtrFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   PRInt32  aModType)
{
  // Attributes common to MathML tags
  if (nsMathMLFrame::CommonAttributeChangedFor(PresContext(), mContent, aAttribute))
    return NS_OK;

  // Attributes specific to <mtr>:
  // groupalign  : Not yet supported.
  // rowalign    : Fully specified in mathml.css, and so HasAttributeDependentStyle() will
  //               pick it up and nsCSSFrameConstructor will issue a PostRestyleEvent().
  // columnalign : Need an explicit re-style call.

  if (aAttribute == nsGkAtoms::rowalign_) {
    // unset any -moz attribute that we may have set earlier, and re-sync
    mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::MOZrowalign, PR_FALSE);
    MapRowAttributesIntoCSS(nsTableFrame::GetTableFrame(this), this);
    // That's all - see comment above.
    return NS_OK;
  }

  if (aAttribute != nsGkAtoms::columnalign_)
    return NS_OK;

  // Clear any cached columnalign's nsValueList for this row
  DeleteProperty(aAttribute);

  // Clear any internal -moz attribute that we may have set earlier
  // in our cells and re-sync their columnalign attribute
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  nsIFrame* cellFrame = GetFirstChild(nsnull);
  for ( ; cellFrame; cellFrame = cellFrame->GetNextSibling()) {
    if (IS_TABLE_CELL(cellFrame->GetType())) {
      cellFrame->GetContent()->
        UnsetAttr(kNameSpaceID_None, nsGkAtoms::MOZcolumnalign, PR_FALSE);
      MapColAttributesIntoCSS(tableFrame, this, cellFrame);
    }
  }

  // Explicitly request a re-resolve and reflow in our subtree to pick up any changes
  PresContext()->PresShell()->FrameConstructor()->
    PostRestyleEvent(mContent, eReStyle_Self, nsChangeHint_ReflowFrame);

  return NS_OK;
}

// --------
// implementation of nsMathMLmtdFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtdFrame, nsTableCellFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtdFrame, nsTableCellFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED0(nsMathMLmtdFrame, nsTableCellFrame)

nsIFrame*
NS_NewMathMLmtdFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtdFrame(aContext);
}

nsMathMLmtdFrame::~nsMathMLmtdFrame()
{
}

NS_IMETHODIMP
nsMathMLmtdFrame::Init(nsIContent* aContent,
                       nsIFrame*   aParent,
                       nsIFrame*   aPrevInFlow)
{
  nsresult rv = nsTableCellFrame::Init(aContent, aParent, aPrevInFlow);
  nsMathMLFrame::MapCommonAttributesIntoCSS(PresContext(), aContent);
  return rv;
}

PRInt32
nsMathMLmtdFrame::GetRowSpan()
{
  PRInt32 rowspan = 1;

  // Don't look at the content's rowspan if we're not an mtd.
  if (mContent->Tag() == nsGkAtoms::mtd_) {
    nsAutoString value;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::rowspan, value);
    if (!value.IsEmpty()) {
      PRInt32 error;
      rowspan = value.ToInteger(&error);
      if (error || rowspan < 0)
        rowspan = 1;
    }
  }
  return rowspan;
}

PRInt32
nsMathMLmtdFrame::GetColSpan()
{
  PRInt32 colspan = 1;

  // Don't look at the content's rowspan if we're not an mtd.
  if (mContent->Tag() == nsGkAtoms::mtd_) {
    nsAutoString value;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::columnspan_, value);
    if (!value.IsEmpty()) {
      PRInt32 error;
      colspan = value.ToInteger(&error);
      if (error || colspan < 0)
        colspan = 1;
    }
  }
  return colspan;
}

NS_IMETHODIMP
nsMathMLmtdFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   PRInt32  aModType)
{
  // Attributes common to MathML tags
  if (nsMathMLFrame::CommonAttributeChangedFor(PresContext(), mContent, aAttribute))
    return NS_OK;

  // Attributes specific to <mtd>:
  // groupalign  : Not yet supported
  // rowalign    : in mathml.css
  // columnalign : here
  // rowspan     : here
  // columnspan  : here

  if (aAttribute == nsGkAtoms::columnalign_) {
    // unset any -moz attribute that we may have set earlier, and re-sync
    mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::MOZcolumnalign, PR_FALSE);
    MapColAttributesIntoCSS(nsTableFrame::GetTableFrame(this), mParent, this);
    return NS_OK;
  }

  if (aAttribute == nsGkAtoms::rowspan ||
      aAttribute == nsGkAtoms::columnspan_) {
    // use the naming expected by the base class 
    if (aAttribute == nsGkAtoms::columnspan_)
      aAttribute = nsGkAtoms::colspan;
    return nsTableCellFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
  }

  return NS_OK;
}

// --------
// implementation of nsMathMLmtdInnerFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtdInnerFrame, nsMathMLFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtdInnerFrame, nsMathMLFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLmtdInnerFrame, nsBlockFrame, nsMathMLFrame)

nsIFrame*
NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtdInnerFrame(aContext);
}

nsMathMLmtdInnerFrame::~nsMathMLmtdInnerFrame()
{
}

NS_IMETHODIMP
nsMathMLmtdInnerFrame::Init(nsIContent*      aContent,
                            nsIFrame*        aParent,
                            nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBlockFrame::Init(aContent, aParent, aPrevInFlow);

  // record that children that are ignorable whitespace should be excluded
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  return rv;
}

NS_IMETHODIMP
nsMathMLmtdInnerFrame::Reflow(nsPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  // Let the base class do the reflow
  nsresult rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // more about <maligngroup/> and <malignmark/> later
  // ...
  return rv;
}
