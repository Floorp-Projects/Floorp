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
#include "nsCRT.h"
#include "nsCellLayoutData.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsTableCell.h"
#include "nsIStyleContext.h"

#include <stdio.h>/* XXX */ // for printf


static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);

nsCellLayoutData::nsCellLayoutData(nsTableCellFrame *aCellFrame, 
                                   nsReflowMetrics * aDesiredSize, nsSize * aMaxElementSize)

{
  // IMPORTANT: Always intialize instance variables to null
  mCellFrame = nsnull;
  mMargin.top = mMargin.bottom = mMargin.left = mMargin.right = 0;
  
  for (PRInt32  i = 0; i < 4; i++)
    mBorderStyle[i] = nsnull;

  // IMPORTANT: call setters: this
  // guarentees addrefs/releases are called if needed - gpk
  SetCellFrame(aCellFrame);
  SetDesiredSize(aDesiredSize);
  SetMaxElementSize(aMaxElementSize);
}

// don't delete mCellFrame, I didn't allocate it
nsCellLayoutData::~nsCellLayoutData()
{
  mCellFrame = nsnull;
  mColLayoutData = nsnull;
}

nsTableCellFrame * nsCellLayoutData::GetCellFrame()
{ return mCellFrame; }

void nsCellLayoutData::SetCellFrame(nsTableCellFrame * aCellFrame)
{ mCellFrame = aCellFrame; }

nsReflowMetrics * nsCellLayoutData::GetDesiredSize()
{ return &mDesiredSize; }

void nsCellLayoutData::SetDesiredSize(nsReflowMetrics * aDesiredSize)
{ 
  if (nsnull!=aDesiredSize)
    mDesiredSize = *aDesiredSize;
}

nsSize * nsCellLayoutData::GetMaxElementSize()
{ return &mMaxElementSize; }

void nsCellLayoutData::SetMaxElementSize(nsSize * aMaxElementSize)
{ 
  if (nsnull!=aMaxElementSize)
    mMaxElementSize = *aMaxElementSize;
}

/**
  * Get the style molecue for this object (determined, by
  * asking for the frame
  *
  **/
nsStyleMolecule* nsCellLayoutData::GetStyleMolecule()
{
  if (mCellFrame != nsnull) {
    nsStyleMolecule*  styleMol;
    mCellFrame->GetStyleData(kStyleMoleculeSID, (nsStyleStruct*&)styleMol);
    return styleMol;
  }
  return nsnull;
}





/**
  * Given a list of nsCellLayoutData and a index, get the style molecule for
  * that element in the list 
  *
  **/
nsStyleMolecule* nsCellLayoutData::GetStyleMoleculeAt(nsVoidArray* aList,  PRInt32 aIndex)
{
  nsStyleMolecule*  result = nsnull;

  if (aList != nsnull)
  {
    nsCellLayoutData* data = (nsCellLayoutData*)aList->ElementAt(aIndex);
    if (data != nsnull)
      result = data->GetStyleMolecule();
  }
  return result;
}

/**
  * Given a style molecule and an edge, find the margin
  *
  **/
nscoord nsCellLayoutData::GetMargin(nsStyleMolecule* aStyle,PRUint8 aEdge)
{
  nscoord result = 0;

  if (aStyle)
  {
    switch (aEdge)
    {

      case NS_SIDE_TOP:
        result = aStyle->margin.top;
      break;

      case NS_SIDE_RIGHT:
        result = aStyle->margin.right;
      break;


      case NS_SIDE_BOTTOM:
        result = aStyle->margin.bottom;
      break;

      case NS_SIDE_LEFT:
        result = aStyle->margin.left;
      break;

    }
  }
  return result;
}


/**
  * Given a style molecule and an edge, find the border width
  *
  **/
nscoord nsCellLayoutData::GetBorderWidth(nsStyleMolecule* aStyle,PRUint8 aEdge)
{
  nscoord result = 0;

  if (aStyle)
  {
    switch (aEdge)
    {
      case NS_SIDE_TOP:
        result = aStyle->border.top;
      break;

      case NS_SIDE_RIGHT:
        result = aStyle->border.right;
      break;


      case NS_SIDE_BOTTOM:
        result = aStyle->border.bottom;
      break;

      case NS_SIDE_LEFT:
        result = aStyle->border.left;
      break;

    }
  }
  return result;
}


/**
  * Given a style molecule and an edge, find the padding
  *
  **/
nscoord nsCellLayoutData::GetPadding(nsStyleMolecule* aStyle,PRUint8 aEdge)
{
  nscoord result = 0;

  if (aStyle)
  {
    switch (aEdge)
    {
      case NS_SIDE_TOP:
        result = aStyle->padding.top;
      break;

      case NS_SIDE_RIGHT:
        result = aStyle->padding.right;
      break;

      case NS_SIDE_BOTTOM:
        result = aStyle->padding.bottom;
      break;

      case NS_SIDE_LEFT:
        result = aStyle->padding.left;
      break;

    }
  }
  return result;
}



/**
  * Given an Edge, find the opposing edge (top<-->bottom, left<-->right)
  *
  **/
PRUint8 nsCellLayoutData::GetOpposingEdge(PRUint8 aEdge)
{
   PRUint8 result;

   switch (aEdge)
    {
   case NS_SIDE_LEFT:
        result = NS_SIDE_RIGHT;
        break;

   case NS_SIDE_RIGHT:
        result = NS_SIDE_LEFT;
        break;

   case NS_SIDE_TOP:
        result = NS_SIDE_BOTTOM;
        break;

   case NS_SIDE_BOTTOM:
        result = NS_SIDE_TOP;
        break;

      default:
        result = NS_SIDE_TOP;
     }
  return result;
}


/*
*
* Determine border style for two cells.
*
  1.If the adjacent elements are of the same type, the wider of the two borders is used. 
    "Wider" takes into account the border-style of 'none', so a "1px solid" border 
    will take precedence over a "20px none" border. 

  2.If there are two or more with the same width, but different style, 
    then the one with a style near the start of the following list will be drawn:

      'blank', 'double', 'solid', 'dashed', 'dotted', 'ridge', 'groove', 'none'

  3.If the borders are of the same width, the border on the element occurring first is used. 
  
    First is defined as aStyle for this method.

  NOTE: This assumes left-to-right, top-to-bottom bias. -- gpk

*
*/


nsStyleMolecule* nsCellLayoutData::CompareCellBorders(nsStyleMolecule* aStyle1,
                                                       PRUint8 aEdge1,
                                                       nsStyleMolecule* aStyle2,
                                                       PRUint8 aEdge2)
{
  PRInt32 width1 = GetBorderWidth(aStyle1,aEdge1);
  PRInt32 width2 = GetBorderWidth(aStyle2,aEdge2);

  nsStyleMolecule* result = nsnull;
  
  if (width1 > width2)
    result =  aStyle1;
  else if (width1 < width2)
    result = aStyle2;
  else // width1 == width2
  {
    if (aStyle1->borderStyle[aEdge1] >= aStyle2->borderStyle[aEdge2])
      result = aStyle1;
    else
      result = aStyle2;
  }
  return result;
}


/**
  * Given a List of cell layout data, compare the edges to see which has the
  * border with the highest precidence. 
  *
  **/

nsStyleMolecule* nsCellLayoutData::FindHighestPrecedentBorder(nsVoidArray* aList,
                                                              PRUint8 aEdge)
{
  nsStyleMolecule* result = nsnull;
  PRInt32 index = 0;
  PRInt32 count = 0;


  NS_ASSERTION(aList,"a List must be valid");
  count = aList->Count();
  if (count)
  {
    nsStyleMolecule* style = nsnull;
    nsStyleMolecule* style2 = nsnull;
    
    style = GetStyleMoleculeAt(aList,index++);
    while (index < count)
    {
      style2 = GetStyleMoleculeAt(aList,index++);
      if (GetMargin(style2,aEdge) == 0)
        style = CompareCellBorders(style,aEdge, style2,aEdge);
    }
    if (style && GetMargin(style,aEdge) != 0)
      result = style;
  }
  return result;
}
  



nsStyleMolecule* nsCellLayoutData::FindInnerBorder( nsStyleMolecule* aStyle,
                                                    nsVoidArray*  aList,
                                                    PRUint8 aEdge)
{
  nsStyleMolecule*  result = nsnull;
  nsStyleMolecule*  altStyle = nsnull;
  PRUint8           opposite = GetOpposingEdge(aEdge);

  if (GetMargin(aStyle,aEdge) == 0)
  {

    nsStyleMolecule* altStyle = FindHighestPrecedentBorder(aList,opposite);
    if (altStyle != nsnull)
      result = CompareCellBorders(aStyle,aEdge,altStyle,opposite);
    else
      result = aStyle;
  }

  return result;
}


/*
*
* FindRelevantBorder recursively searches up the frame hierarchy for the border
* style that is applicable to the cell. If at any point the frame has a margin
* or the parent frame has padding, then the outer frame for this object takes
* presendence over the inner frame. 

1.Borders on 'table' elements take precedence over borders on any other table elements. 
2.Borders on 'row-groups' take precedence over borders on 'rows', 
  and likewise borders on 'column-groups' take precedence over borders on 'columns'. 
3.Borders on any other type of table element take precedence over 'table-cell' elements. 
 
*
* NOTE: This method assumes that the table cell potentially shares a border.
* It should not be called for internal cells
*
* NOTE: COLUMNS AND COLGROUPS NEED TO BE FIGURED INTO THE ALGORITHM -- GPK!!!
* 
*
*/
nsStyleMolecule* nsCellLayoutData::FindOuterBorder( nsTableFrame* aTableFrame,
                                                    nsIFrame* aFrame, 
                                                    nsStyleMolecule* aStyle,
                                                    PRUint8 aEdge)
{
  // By default, return the passed in style
  nsStyleMolecule* style = aStyle;
  nsIFrame*        frame = aFrame;
  PRBool           done = PR_FALSE;
    
  // The table frame is the outer most frame we test against
  while (done == PR_FALSE)
  {
    done = PR_TRUE; // where done unless the frame's margin is zero
                    // and the parent's padding is zero

    nscoord margin = GetMargin(style,aEdge);

    // if the margin for this style is zero then check to see if the paddding
    // for the parent frame is also zero
    if (margin == 0)
    { 
      nsIFrame*         parentFrame;
      nsStyleMolecule*  parentStyle;

      aFrame->GetGeometricParent(parentFrame);
      aFrame->GetStyleData(kStyleMoleculeSID, (nsStyleStruct*&)parentStyle);

      // if the padding for the parent style is zero just
      // recursively call this routine
      PRInt32 padding = GetPadding(parentStyle,aEdge);
      if (parentStyle && padding == 0)
      {
        style = parentStyle;
        frame = parentFrame;
        // If this frame represents the table frame then
        // the table style is used
        done = PRBool(frame != (nsIFrame*)aTableFrame);
        continue;
      }

    }
  }
  return style;
}



/*

  Border Resolution
  1.Borders on 'table' elements take precedence over borders on any other table elements. 
  2.Borders on 'row-groups' take precedence over borders on 'rows', and likewise borders on 'column-groups' take
    precedence over borders on 'columns'. 
  3.Borders on any other type of table element take precedence over 'table-cell' elements. 
  4.If the adjacent elements are of the same type, the wider of the two borders is used. "Wider" takes into account
    the border-style of 'none', so a "1px solid" border will take precedence over a "20px none" border. 
  5.If the borders are of the same width, the border on the element occurring first is used. 


  How to compare
  1.Those of the one or two cells that have an edge here. 
    Less than two can occur at the edge of the table, but also
    at the edges of "holes" (unoccupied grid cells).
  2.Those of the columns that have an edge here.
  3.Those of the column groups that have an edge here.
  4.Those of the rows that have an edge here.
  5.Those of the row groups that have an edge here.
  6.Those of the table, if this is the edge of the table.

*
* @param aIsFirst -- TRUE if this is the first cell in the row
* @param aIsLast  -- TRUE if this is the last cell in the row
* @param aIsTop -- TRUE if this is the top cell in the column
* @param aIsBottom  -- TRUE if this is the last cell in the column
*/

nsStyleMolecule* nsCellLayoutData::FindBorderStyle(nsTableFrame*    aTableFrame,
                                                   nsStyleMolecule* aCellStyle,
                                                   nsVoidArray*     aList,
                                                   PRUint8          aEdge)
{
  nsStyleMolecule*  style = nsnull;

  if (aList && aList->Count() == 0)
    style = FindOuterBorder(aTableFrame,mCellFrame,aCellStyle,aEdge);
  else
    style = FindInnerBorder(aCellStyle,aList, aEdge);

  if (!style)
    style = aCellStyle;

  return style;
}


void nsCellLayoutData::CalculateBorders(nsTableFrame*     aTableFrame,
                                        nsStyleMolecule*  aCellStyle,
                                        nsVoidArray*      aBoundaryCells[4])
{ 

  for (PRInt32 edge = 0; edge < 4; edge++)
    mBorderStyle[edge] = FindBorderStyle(aTableFrame,aCellStyle,aBoundaryCells[edge],edge);
}


/**
  * Given a List of cell layout data, compare the edges to see which has the
  * border with the highest precidence. 
  *
  **/

nscoord nsCellLayoutData::FindLargestMargin(nsVoidArray* aList,PRUint8 aEdge)
{
  nscoord result = 0;
  PRInt32 index = 0;
  PRInt32 count = 0;


  NS_ASSERTION(aList,"a List must be valid");
  count = aList->Count();
  if (count)
  {
    nsStyleMolecule* style = nsnull;
    
    nscoord value = 0;
    while (index < count)
    {
      style = GetStyleMoleculeAt(aList,index++);
      value = GetMargin(style,aEdge);
      if (value > result)
        result = value;
    }
  }
  return result;
}




void nsCellLayoutData::CalculateMargins(nsTableFrame*     aTableFrame,
                                        nsStyleMolecule*  aCellStyle,
                                        nsVoidArray*      aBoundaryCells[4])
{ 
  // By default the margin is just the margin found in the 
  // table cells style 
  mMargin = aCellStyle->margin;

  // Left and Top Margins are collapsed with their neightbors
  // Right and Bottom Margins are simple left as they are
  nscoord value;

  // The left and top sides margins are the difference between
  // their inherint value and the value of the margin of the 
  // object to the left or right of them.

  value = FindLargestMargin(aBoundaryCells[NS_SIDE_LEFT],NS_SIDE_RIGHT);
  if (value > mMargin.left)
    mMargin.left = 0;
  else
    mMargin.left -= value;

  value = FindLargestMargin(aBoundaryCells[NS_SIDE_TOP],NS_SIDE_BOTTOM);
  if (value > mMargin.top)
    mMargin.top = 0;
  else
    mMargin.top -= value;
}


void nsCellLayoutData::RecalcLayoutData(nsTableFrame* aTableFrame,
                                        nsVoidArray* aBoundaryCells[4])

{
  nsStyleMolecule* cellStyle;

  mCellFrame->GetStyleData(kStyleMoleculeSID, (nsStyleStruct*&)cellStyle);
  
  CalculateBorders(aTableFrame,cellStyle,aBoundaryCells);
  CalculateMargins(aTableFrame,cellStyle,aBoundaryCells);
}


void nsCellLayoutData::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 indent;

  nsTableCell* cell;

  mCellFrame->GetContent((nsIContent*&)cell);
  if (cell != nsnull)
  {
    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
    fprintf(out,"RowSpan = %d ColSpan = %d \n",cell->GetRowSpan(),cell->GetColSpan());
    
    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
    fprintf(out,"Margin -- Top: %d Left: %d Bottom: %d Right: %d \n",  
                NS_TWIPS_TO_POINTS_INT(mMargin.top),
                NS_TWIPS_TO_POINTS_INT(mMargin.left),
                NS_TWIPS_TO_POINTS_INT(mMargin.bottom),
                NS_TWIPS_TO_POINTS_INT(mMargin.right));


    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

    nscoord top,left,bottom,right;
    
    top = (mBorderStyle[NS_SIDE_TOP] ? mBorderStyle[NS_SIDE_TOP]->border.top : 0);
    left = (mBorderStyle[NS_SIDE_LEFT] ? mBorderStyle[NS_SIDE_LEFT]->border.left : 0);
    bottom = (mBorderStyle[NS_SIDE_BOTTOM] ? mBorderStyle[NS_SIDE_BOTTOM]->border.bottom : 0);
    right = (mBorderStyle[NS_SIDE_RIGHT] ? mBorderStyle[NS_SIDE_RIGHT]->border.right : 0);


    fprintf(out,"Border -- Top: %d Left: %d Bottom: %d Right: %d \n",  
                NS_TWIPS_TO_POINTS_INT(top),
                NS_TWIPS_TO_POINTS_INT(left),
                NS_TWIPS_TO_POINTS_INT(bottom),
                NS_TWIPS_TO_POINTS_INT(right));



    cell->List(out,aIndent);
    NS_RELEASE(cell);
  }
}

