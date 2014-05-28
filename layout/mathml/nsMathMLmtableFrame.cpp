/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmtableFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsNameSpaceManager.h"
#include "nsRenderingContext.h"
#include "nsCSSRendering.h"

#include "nsTArray.h"
#include "nsTableFrame.h"
#include "celldata.h"

#include "RestyleManager.h"
#include <algorithm>

#include "nsIScriptError.h"
#include "nsContentUtils.h"

using namespace mozilla;

//
// <mtable> -- table or matrix - implementation
//

static int8_t
ParseStyleValue(nsIAtom* aAttribute, const nsAString& aAttributeValue)
{
  if (aAttribute == nsGkAtoms::rowalign_) {
    if (aAttributeValue.EqualsLiteral("top"))
      return NS_STYLE_VERTICAL_ALIGN_TOP;
    else if (aAttributeValue.EqualsLiteral("bottom"))
      return NS_STYLE_VERTICAL_ALIGN_BOTTOM;
    else if (aAttributeValue.EqualsLiteral("center"))
      return NS_STYLE_VERTICAL_ALIGN_MIDDLE;
    else
      return NS_STYLE_VERTICAL_ALIGN_BASELINE;
  } else if (aAttribute == nsGkAtoms::columnalign_) {
    if (aAttributeValue.EqualsLiteral("left"))
      return NS_STYLE_TEXT_ALIGN_LEFT;
    else if (aAttributeValue.EqualsLiteral("right"))
      return NS_STYLE_TEXT_ALIGN_RIGHT;
    else
      return NS_STYLE_TEXT_ALIGN_CENTER;
  } else if (aAttribute == nsGkAtoms::rowlines_ ||
             aAttribute == nsGkAtoms::columnlines_) {
    if (aAttributeValue.EqualsLiteral("solid"))
      return NS_STYLE_BORDER_STYLE_SOLID;
    else if (aAttributeValue.EqualsLiteral("dashed"))
      return NS_STYLE_BORDER_STYLE_DASHED;
    else
      return NS_STYLE_BORDER_STYLE_NONE;
  } else {
    MOZ_CRASH("Unrecognized attribute.");
  }

  return -1;
}

static nsTArray<int8_t>*
ExtractStyleValues(const nsAString& aString, nsIAtom* aAttribute,
                   bool aAllowMultiValues)
{
  nsTArray<int8_t>* styleArray = nullptr;

  const char16_t* start = aString.BeginReading();
  const char16_t* end = aString.EndReading();

  int32_t startIndex = 0;
  int32_t count = 0;

  while (start < end) {
    // Skip leading spaces.
    while ((start < end) && nsCRT::IsAsciiSpace(*start)) {
      start++;
      startIndex++;
    }

    // Look for the end of the string, or another space.
    while ((start < end) && !nsCRT::IsAsciiSpace(*start)) {
      start++;
      count++;
    }

    // Grab the value found and process it.
    if (count > 0) {
      if (!styleArray)
        styleArray = new nsTArray<int8_t>();

      // We want to return a null array if an attribute gives multiple values,
      // but multiple values aren't allowed.
      if (styleArray->Length() > 1 && !aAllowMultiValues) {
        delete styleArray;
        return nullptr;
      }

      nsDependentSubstring valueString(aString, startIndex, count);
      int8_t styleValue = ParseStyleValue(aAttribute, valueString);
      styleArray->AppendElement(styleValue);

      startIndex += count;
      count = 0;
    }
  }
  return styleArray;
}

static nsresult ReportParseError(nsIFrame* aFrame, const char16_t* aAttribute,
                                 const char16_t* aValue)
{
  nsIContent* content = aFrame->GetContent();

  const char16_t* params[] =
    { aValue, aAttribute, content->Tag()->GetUTF16String() };

  return nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                         NS_LITERAL_CSTRING("MathML"),
                                         content->OwnerDoc(),
                                         nsContentUtils::eMATHML_PROPERTIES,
                                         "AttributeParsingError", params, 3);
}

// Each rowalign='top bottom' or columnalign='left right center' (from
// <mtable> or <mtr>) is split once into an nsTArray<int8_t> which is
// stored in the property table. Row/Cell frames query the property table
// to see what values apply to them.

static void
DestroyStylePropertyList(void* aPropertyValue)
{
  delete static_cast<nsTArray<int8_t>*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(RowAlignProperty, DestroyStylePropertyList)
NS_DECLARE_FRAME_PROPERTY(RowLinesProperty, DestroyStylePropertyList)
NS_DECLARE_FRAME_PROPERTY(ColumnAlignProperty, DestroyStylePropertyList)
NS_DECLARE_FRAME_PROPERTY(ColumnLinesProperty, DestroyStylePropertyList)

static const FramePropertyDescriptor*
AttributeToProperty(nsIAtom* aAttribute)
{
  if (aAttribute == nsGkAtoms::rowalign_)
    return RowAlignProperty();
  if (aAttribute == nsGkAtoms::rowlines_)
    return RowLinesProperty();
  if (aAttribute == nsGkAtoms::columnalign_)
    return ColumnAlignProperty();
  NS_ASSERTION(aAttribute == nsGkAtoms::columnlines_, "Invalid attribute");
  return ColumnLinesProperty();
}

/* This method looks for a property that applies to a cell, but it looks
 * recursively because some cell properties can come from the cell, a row,
 * a table, etc. This function searches through the heirarchy for a property
 * and returns its value. The function stops searching after checking a <mtable>
 * frame.
 */
static nsTArray<int8_t>*
FindCellProperty(const nsIFrame* aCellFrame,
                 const FramePropertyDescriptor* aFrameProperty)
{
  const nsIFrame* currentFrame = aCellFrame;
  nsTArray<int8_t>* propertyData = nullptr;

  while (currentFrame) {
    FrameProperties props = currentFrame->Properties();
    propertyData = static_cast<nsTArray<int8_t>*>(props.Get(aFrameProperty));
    bool frameIsTable = (currentFrame->GetType() == nsGkAtoms::tableFrame);

    if (propertyData || frameIsTable)
      currentFrame = nullptr; // A null frame pointer exits the loop
    else
      currentFrame = currentFrame->GetParent(); // Go to the parent frame
  }

  return propertyData;
}

static void
ApplyBorderToStyle(const nsMathMLmtdFrame* aFrame,
                   nsStyleBorder& aStyleBorder)
{
  int32_t rowIndex;
  int32_t columnIndex;
  aFrame->GetRowIndex(rowIndex);
  aFrame->GetColIndex(columnIndex);

  nscoord borderWidth =
    aFrame->PresContext()->GetBorderWidthTable()[NS_STYLE_BORDER_WIDTH_THIN];

  nsTArray<int8_t>* rowLinesList =
    FindCellProperty(aFrame, RowLinesProperty());

  nsTArray<int8_t>* columnLinesList =
    FindCellProperty(aFrame, ColumnLinesProperty());

  // We don't place a row line on top of the first row
  if (rowIndex > 0 && rowLinesList) {
    // If the row number is greater than the number of provided rowline
    // values, we simply repeat the last value.
    int32_t listLength = rowLinesList->Length();
    if (rowIndex < listLength) {
      aStyleBorder.SetBorderStyle(NS_SIDE_TOP,
                    rowLinesList->ElementAt(rowIndex - 1));
    } else {
      aStyleBorder.SetBorderStyle(NS_SIDE_TOP,
                    rowLinesList->ElementAt(listLength - 1));
    }
    aStyleBorder.SetBorderWidth(NS_SIDE_TOP, borderWidth);
  }

  // We don't place a column line on the left of the first column.
  if (columnIndex > 0 && columnLinesList) {
    // If the column number is greater than the number of provided columline
    // values, we simply repeat the last value.
    int32_t listLength = columnLinesList->Length();
    if (columnIndex < listLength) {
      aStyleBorder.SetBorderStyle(NS_SIDE_LEFT,
                    columnLinesList->ElementAt(columnIndex - 1));
    } else {
      aStyleBorder.SetBorderStyle(NS_SIDE_LEFT,
                    columnLinesList->ElementAt(listLength - 1));
    }
    aStyleBorder.SetBorderWidth(NS_SIDE_LEFT, borderWidth);
  }
}

/*
 * A variant of the nsDisplayBorder contains special code to render a border
 * around a nsMathMLmtdFrame based on the rowline and columnline properties
 * set on the cell frame.
 */
class nsDisplaymtdBorder : public nsDisplayBorder {
public:
  nsDisplaymtdBorder(nsDisplayListBuilder* aBuilder, nsMathMLmtdFrame* aFrame)
    : nsDisplayBorder(aBuilder, aFrame)
  {
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE
  {
    nsStyleBorder styleBorder = *mFrame->StyleBorder();
    ApplyBorderToStyle(static_cast<nsMathMLmtdFrame*>(mFrame), styleBorder);
    return CalculateBounds(styleBorder);
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE
  {
    nsStyleBorder styleBorder = *mFrame->StyleBorder();
    ApplyBorderToStyle(static_cast<nsMathMLmtdFrame*>(mFrame), styleBorder);

    nsPoint offset = ToReferenceFrame();
    nsCSSRendering::PaintBorderWithStyleBorder(mFrame->PresContext(), *aCtx,
                                               mFrame, mVisibleRect,
                                               nsRect(offset,
                                                      mFrame->GetSize()),
                                               styleBorder,
                                               mFrame->StyleContext(),
                                               mFrame->GetSkipSides());
  }
};

#ifdef DEBUG
#define DEBUG_VERIFY_THAT_FRAME_IS(_frame, _expected) \
  NS_ASSERTION(NS_STYLE_DISPLAY_##_expected == _frame->StyleDisplay()->mDisplay, "internal error");
#else
#define DEBUG_VERIFY_THAT_FRAME_IS(_frame, _expected)
#endif

static void
ParseFrameAttribute(nsIFrame* aFrame, nsIAtom* aAttribute,
                    bool aAllowMultiValues)
{
  nsAutoString attrValue;

  nsIContent* frameContent = aFrame->GetContent();
  frameContent->GetAttr(kNameSpaceID_None, aAttribute, attrValue);

  if (!attrValue.IsEmpty()) {
    nsTArray<int8_t>* valueList =
      ExtractStyleValues(attrValue, aAttribute, aAllowMultiValues);

    // If valueList is null, that indicates a problem with the attribute value.
    // Only set properties on a valid attribute value.
    if (valueList) {
      // The code reading the property assumes that this list is nonempty.
      NS_ASSERTION(valueList->Length() >= 1, "valueList should not be empty!");
      FrameProperties props = aFrame->Properties();
      props.Set(AttributeToProperty(aAttribute), valueList);
    } else {
      ReportParseError(aFrame, aAttribute->GetUTF16String(), attrValue.get());
    }
  }
}

// map all attribues within a table -- requires the indices of rows and cells.
// so it can only happen after they are made ready by the table base class.
static void
MapAllAttributesIntoCSS(nsIFrame* aTableFrame)
{
  // Map mtable rowalign & rowlines.
  ParseFrameAttribute(aTableFrame, nsGkAtoms::rowalign_, true);
  ParseFrameAttribute(aTableFrame, nsGkAtoms::rowlines_, true);

  // Map mtable columnalign & columnlines.
  ParseFrameAttribute(aTableFrame, nsGkAtoms::columnalign_, true);
  ParseFrameAttribute(aTableFrame, nsGkAtoms::columnlines_, true);

  // mtable is simple and only has one (pseudo) row-group
  nsIFrame* rgFrame = aTableFrame->GetFirstPrincipalChild();
  if (!rgFrame || rgFrame->GetType() != nsGkAtoms::tableRowGroupFrame)
    return;

  nsIFrame* rowFrame = rgFrame->GetFirstPrincipalChild();
  for ( ; rowFrame; rowFrame = rowFrame->GetNextSibling()) {
    DEBUG_VERIFY_THAT_FRAME_IS(rowFrame, TABLE_ROW);
    if (rowFrame->GetType() == nsGkAtoms::tableRowFrame) {
      // Map row rowalign.
      ParseFrameAttribute(rowFrame, nsGkAtoms::rowalign_, false);
      // Map row columnalign.
      ParseFrameAttribute(rowFrame, nsGkAtoms::columnalign_, true);

      nsIFrame* cellFrame = rowFrame->GetFirstPrincipalChild();
      for ( ; cellFrame; cellFrame = cellFrame->GetNextSibling()) {
        DEBUG_VERIFY_THAT_FRAME_IS(cellFrame, TABLE_CELL);
        if (IS_TABLE_CELL(cellFrame->GetType())) {
          // Map cell rowalign.
          ParseFrameAttribute(cellFrame, nsGkAtoms::rowalign_, false);
          // Map row columnalign.
          ParseFrameAttribute(cellFrame, nsGkAtoms::columnalign_, false);
        }
      }
    }
  }
}

// the align attribute of mtable can have a row number which indicates
// from where to anchor the table, e.g., top 5 means anchor the table at
// the top of the 5th row, axis -1 means anchor the table on the axis of
// the last row

// The REC says that the syntax is 
// '\s*(top|bottom|center|baseline|axis)(\s+-?[0-9]+)?\s*' 
// the parsing could have been simpler with that syntax
// but for backward compatibility we make optional 
// the whitespaces between the alignment name and the row number

enum eAlign {
  eAlign_top,
  eAlign_bottom,
  eAlign_center,
  eAlign_baseline,
  eAlign_axis
};

static void
ParseAlignAttribute(nsString& aValue, eAlign& aAlign, int32_t& aRowIndex)
{
  // by default, the table is centered about the axis
  aRowIndex = 0;
  aAlign = eAlign_axis;
  int32_t len = 0;

  // we only have to remove the leading spaces because 
  // ToInteger ignores the whitespaces around the number
  aValue.CompressWhitespace(true, false);

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
    nsresult error;
    aValue.Cut(0, len); // aValue is not a const here
    aRowIndex = aValue.ToInteger(&error);
    if (NS_FAILED(error))
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
  f->List(stdout, 0);
}
#endif

// --------
// implementation of nsMathMLmtableOuterFrame

NS_QUERYFRAME_HEAD(nsMathMLmtableOuterFrame)
  NS_QUERYFRAME_ENTRY(nsIMathMLFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsTableOuterFrame)

nsContainerFrame*
NS_NewMathMLmtableOuterFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtableOuterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmtableOuterFrame)
 
nsMathMLmtableOuterFrame::~nsMathMLmtableOuterFrame()
{
}

nsresult
nsMathMLmtableOuterFrame::AttributeChanged(int32_t  aNameSpaceID,
                                           nsIAtom* aAttribute,
                                           int32_t  aModType)
{
  // Attributes specific to <mtable>:
  // frame         : in mathml.css
  // framespacing  : not yet supported 
  // groupalign    : not yet supported
  // equalrows     : not yet supported 
  // equalcolumns  : not yet supported 
  // displaystyle  : here and in mathml.css
  // align         : in reflow 
  // rowalign      : here
  // rowlines      : here 
  // rowspacing    : not yet supported 
  // columnalign   : here 
  // columnlines   : here 
  // columnspacing : not yet supported 

  // mtable is simple and only has one (pseudo) row-group inside our inner-table
  nsIFrame* tableFrame = mFrames.FirstChild();
  NS_ASSERTION(tableFrame && tableFrame->GetType() == nsGkAtoms::tableFrame,
               "should always have an inner table frame");
  nsIFrame* rgFrame = tableFrame->GetFirstPrincipalChild();
  if (!rgFrame || rgFrame->GetType() != nsGkAtoms::tableRowGroupFrame)
    return NS_OK;

  // align - just need to issue a dirty (resize) reflow command
  if (aAttribute == nsGkAtoms::align) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    return NS_OK;
  }

  // displaystyle - may seem innocuous, but it is actually very harsh --
  // like changing an unit. Blow away and recompute all our automatic
  // presentational data, and issue a style-changed reflow request
  if (aAttribute == nsGkAtoms::displaystyle_) {
    nsMathMLContainerFrame::RebuildAutomaticDataForChildren(GetParent());
    // Need to reflow the parent, not us, because this can actually
    // affect siblings.
    PresContext()->PresShell()->
      FrameNeedsReflow(GetParent(), nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
    return NS_OK;
  }

  // ...and the other attributes affect rows or columns in one way or another

  // Ignore attributes that do not affect layout.
  if (aAttribute != nsGkAtoms::rowalign_ &&
      aAttribute != nsGkAtoms::rowlines_ &&
      aAttribute != nsGkAtoms::columnalign_ &&
      aAttribute != nsGkAtoms::columnlines_) {
    return NS_OK;
  }

  nsPresContext* presContext = tableFrame->PresContext();

  // clear any cached property list for this table
  presContext->PropertyTable()->
    Delete(tableFrame, AttributeToProperty(aAttribute));

  // Reparse the new attribute on the table.
  ParseFrameAttribute(tableFrame, aAttribute, true);

  // Explicitly request a reflow in our subtree to pick up any changes
  presContext->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);

  return NS_OK;
}

nsIFrame*
nsMathMLmtableOuterFrame::GetRowFrameAt(nsPresContext* aPresContext,
                                        int32_t         aRowIndex)
{
  int32_t rowCount = GetRowCount();

  // Negative indices mean to find upwards from the end.
  if (aRowIndex < 0) {
    aRowIndex = rowCount + aRowIndex;
  } else {
    // aRowIndex is 1-based, so convert it to a 0-based index
    --aRowIndex;
  }

  // if our inner table says that the index is valid, find the row now
  if (0 <= aRowIndex && aRowIndex <= rowCount) {
    nsIFrame* tableFrame = mFrames.FirstChild();
    NS_ASSERTION(tableFrame && tableFrame->GetType() == nsGkAtoms::tableFrame,
                 "should always have an inner table frame");
    nsIFrame* rgFrame = tableFrame->GetFirstPrincipalChild();
    if (!rgFrame || rgFrame->GetType() != nsGkAtoms::tableRowGroupFrame)
      return nullptr;
    nsTableIterator rowIter(*rgFrame);
    nsIFrame* rowFrame = rowIter.First();
    for ( ; rowFrame; rowFrame = rowIter.Next()) {
      if (aRowIndex == 0) {
        DEBUG_VERIFY_THAT_FRAME_IS(rowFrame, TABLE_ROW);
        if (rowFrame->GetType() != nsGkAtoms::tableRowFrame)
          return nullptr;

        return rowFrame;
      }
      --aRowIndex;
    }
  }
  return nullptr;
}

void
nsMathMLmtableOuterFrame::Reflow(nsPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  nsAutoString value;
  // we want to return a table that is anchored according to the align attribute

  nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  NS_ASSERTION(aDesiredSize.Height() >= 0, "illegal height for mtable");
  NS_ASSERTION(aDesiredSize.Width() >= 0, "illegal width for mtable");

  // see if the user has set the align attribute on the <mtable>
  int32_t rowIndex = 0;
  eAlign tableAlign = eAlign_axis;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::align, value);
  if (!value.IsEmpty()) {
    ParseAlignAttribute(value, tableAlign, rowIndex);
  }

  // adjustments if there is a specified row from where to anchor the table
  // (conceptually: when there is no row of reference, picture the table as if
  // it is wrapped in a single big fictional row at dy = 0, this way of
  // doing so allows us to have a single code path for all cases).
  nscoord dy = 0;
  nscoord height = aDesiredSize.Height();
  nsIFrame* rowFrame = nullptr;
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
      aDesiredSize.SetTopAscent(dy);
      break;
    case eAlign_bottom:
      aDesiredSize.SetTopAscent(dy + height);
      break;
    case eAlign_center:
      aDesiredSize.SetTopAscent(dy + height / 2);
      break;
    case eAlign_baseline:
      if (rowFrame) {
        // anchor the table on the baseline of the row of reference
        nscoord rowAscent = ((nsTableRowFrame*)rowFrame)->GetMaxCellAscent();
        if (rowAscent) { // the row has at least one cell with 'vertical-align: baseline'
          aDesiredSize.SetTopAscent(dy + rowAscent);
          break;
        }
      }
      // in other situations, fallback to center
      aDesiredSize.SetTopAscent(dy + height / 2);
      break;
    case eAlign_axis:
    default: {
      // XXX should instead use style data from the row of reference here ?
      nsRefPtr<nsFontMetrics> fm;
      nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
      aReflowState.rendContext->SetFont(fm);
      nscoord axisHeight;
      GetAxisHeight(*aReflowState.rendContext,
                    aReflowState.rendContext->FontMetrics(),
                    axisHeight);
      if (rowFrame) {
        // anchor the table on the axis of the row of reference
        // XXX fallback to baseline because it is a hard problem
        // XXX need to fetch the axis of the row; would need rowalign=axis to work better
        nscoord rowAscent = ((nsTableRowFrame*)rowFrame)->GetMaxCellAscent();
        if (rowAscent) { // the row has at least one cell with 'vertical-align: baseline'
          aDesiredSize.SetTopAscent(dy + rowAscent);
          break;
        }
      }
      // in other situations, fallback to using half of the height
      aDesiredSize.SetTopAscent(dy + height / 2 + axisHeight);
    }
  }

  mReference.x = 0;
  mReference.y = aDesiredSize.TopAscent();

  // just make-up a bounding metrics
  mBoundingMetrics = nsBoundingMetrics();
  mBoundingMetrics.ascent = aDesiredSize.TopAscent();
  mBoundingMetrics.descent = aDesiredSize.Height() - aDesiredSize.TopAscent();
  mBoundingMetrics.width = aDesiredSize.Width();
  mBoundingMetrics.leftBearing = 0;
  mBoundingMetrics.rightBearing = aDesiredSize.Width();

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

nsContainerFrame*
NS_NewMathMLmtableFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtableFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmtableFrame)

nsMathMLmtableFrame::~nsMathMLmtableFrame()
{
}

void
nsMathMLmtableFrame::SetInitialChildList(ChildListID  aListID,
                                         nsFrameList& aChildList)
{
  nsTableFrame::SetInitialChildList(aListID, aChildList);
  MapAllAttributesIntoCSS(this);
}

void
nsMathMLmtableFrame::RestyleTable()
{
  // re-sync MathML specific style data that may have changed
  MapAllAttributesIntoCSS(this);

  // Explicitly request a re-resolve and reflow in our subtree to pick up any changes
  PresContext()->RestyleManager()->
    PostRestyleEvent(mContent->AsElement(), eRestyle_Subtree,
                     nsChangeHint_AllReflowHints);
}

// --------
// implementation of nsMathMLmtrFrame

nsContainerFrame*
NS_NewMathMLmtrFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtrFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmtrFrame)

nsMathMLmtrFrame::~nsMathMLmtrFrame()
{
}

nsresult
nsMathMLmtrFrame::AttributeChanged(int32_t  aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   int32_t  aModType)
{
  // Attributes specific to <mtr>:
  // groupalign  : Not yet supported.
  // rowalign    : Here
  // columnalign : Here

  nsPresContext* presContext = PresContext();

  if (aAttribute != nsGkAtoms::rowalign_ &&
      aAttribute != nsGkAtoms::columnalign_) {
    return NS_OK;
  }

  presContext->PropertyTable()->Delete(this, AttributeToProperty(aAttribute));

  bool allowMultiValues = (aAttribute == nsGkAtoms::columnalign_);

  // Reparse the new attribute.
  ParseFrameAttribute(this, aAttribute, allowMultiValues);

  // Explicitly request a reflow in our subtree to pick up any changes
  presContext->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);

  return NS_OK;
}

// --------
// implementation of nsMathMLmtdFrame

nsContainerFrame*
NS_NewMathMLmtdFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtdFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmtdFrame)

nsMathMLmtdFrame::~nsMathMLmtdFrame()
{
}

int32_t
nsMathMLmtdFrame::GetRowSpan()
{
  int32_t rowspan = 1;

  // Don't look at the content's rowspan if we're not an mtd or a pseudo cell.
  if ((mContent->Tag() == nsGkAtoms::mtd_) && !StyleContext()->GetPseudo()) {
    nsAutoString value;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::rowspan, value);
    if (!value.IsEmpty()) {
      nsresult error;
      rowspan = value.ToInteger(&error);
      if (NS_FAILED(error) || rowspan < 0)
        rowspan = 1;
      rowspan = std::min(rowspan, MAX_ROWSPAN);
    }
  }
  return rowspan;
}

int32_t
nsMathMLmtdFrame::GetColSpan()
{
  int32_t colspan = 1;

  // Don't look at the content's colspan if we're not an mtd or a pseudo cell.
  if ((mContent->Tag() == nsGkAtoms::mtd_) && !StyleContext()->GetPseudo()) {
    nsAutoString value;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::columnspan_, value);
    if (!value.IsEmpty()) {
      nsresult error;
      colspan = value.ToInteger(&error);
      if (NS_FAILED(error) || colspan < 0 || colspan > MAX_COLSPAN)
        colspan = 1;
    }
  }
  return colspan;
}

nsresult
nsMathMLmtdFrame::AttributeChanged(int32_t  aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   int32_t  aModType)
{
  // Attributes specific to <mtd>:
  // groupalign  : Not yet supported
  // rowalign    : here
  // columnalign : here
  // rowspan     : here
  // columnspan  : here

  if (aAttribute == nsGkAtoms::rowalign_ ||
      aAttribute == nsGkAtoms::columnalign_) {

    nsPresContext* presContext = PresContext();
    presContext->PropertyTable()->Delete(this, AttributeToProperty(aAttribute));

    // Reparse the attribute.
    ParseFrameAttribute(this, aAttribute, false);
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

uint8_t
nsMathMLmtdFrame::GetVerticalAlign() const
{
  // Set the default alignment in case no alignment was specified
  uint8_t alignment = nsTableCellFrame::GetVerticalAlign();

  nsTArray<int8_t>* alignmentList = FindCellProperty(this, RowAlignProperty());

  if (alignmentList) {
    int32_t rowIndex;
    GetRowIndex(rowIndex);

    // If the row number is greater than the number of provided rowalign values,
    // we simply repeat the last value.
    if (rowIndex < (int32_t)alignmentList->Length())
      alignment = alignmentList->ElementAt(rowIndex);
    else
      alignment = alignmentList->ElementAt(alignmentList->Length() - 1);
  }

  return alignment;
}

nsresult
nsMathMLmtdFrame::ProcessBorders(nsTableFrame* aFrame,
                                 nsDisplayListBuilder* aBuilder,
                                 const nsDisplayListSet& aLists)
{
  aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
                                            nsDisplaymtdBorder(aBuilder, this));
  return NS_OK;
}

nsMargin*
nsMathMLmtdFrame::GetBorderWidth(nsMargin& aBorder) const
{
  nsStyleBorder styleBorder = *StyleBorder();
  ApplyBorderToStyle(this, styleBorder);
  aBorder = styleBorder.GetComputedBorder();
  return &aBorder;
}

// --------
// implementation of nsMathMLmtdInnerFrame

NS_QUERYFRAME_HEAD(nsMathMLmtdInnerFrame)
  NS_QUERYFRAME_ENTRY(nsIMathMLFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

nsContainerFrame*
NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmtdInnerFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmtdInnerFrame)

nsMathMLmtdInnerFrame::nsMathMLmtdInnerFrame(nsStyleContext* aContext)
  : nsBlockFrame(aContext)
{
  // Make a copy of the parent nsStyleText for later modificaiton.
  mUniqueStyleText = new (PresContext()) nsStyleText(*StyleText());
}

nsMathMLmtdInnerFrame::~nsMathMLmtdInnerFrame()
{
  mUniqueStyleText->Destroy(PresContext());
}

void
nsMathMLmtdInnerFrame::Reflow(nsPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  // Let the base class do the reflow
  nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // more about <maligngroup/> and <malignmark/> later
  // ...
}

const
nsStyleText* nsMathMLmtdInnerFrame::StyleTextForLineLayout()
{
  // Set the default alignment in case nothing was specified
  uint8_t alignment = StyleText()->mTextAlign;

  nsTArray<int8_t>* alignmentList =
    FindCellProperty(this, ColumnAlignProperty());

  if (alignmentList) {
    nsMathMLmtdFrame* cellFrame = (nsMathMLmtdFrame*)GetParent();
    int32_t columnIndex;
    cellFrame->GetColIndex(columnIndex);

    // If the column number is greater than the number of provided columalign
    // values, we simply repeat the last value.
    if (columnIndex < (int32_t)alignmentList->Length())
      alignment = alignmentList->ElementAt(columnIndex);
    else
      alignment = alignmentList->ElementAt(alignmentList->Length() - 1);
  }

  mUniqueStyleText->mTextAlign = alignment;
  return mUniqueStyleText;
}

/* virtual */ void
nsMathMLmtdInnerFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsBlockFrame::DidSetStyleContext(aOldStyleContext);
  mUniqueStyleText->Destroy(PresContext());
  mUniqueStyleText = new (PresContext()) nsStyleText(*StyleText());
}
