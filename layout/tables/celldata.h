/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef CellData_h__
#define CellData_h__

#include "nsISupports.h"
#include "nsCoord.h"

class nsTableCellFrame;

/** 
  * Data stored by nsCellMap to rationalize rowspan and colspan cells.
  */
class CellData
{
public:
  /** Public constructor.
    * @param aOrigCell  the table cell frame which will be stored in mOrigCell.   
    */
  CellData(nsTableCellFrame* aOrigCell);

  /** destructor */
  ~CellData(); //the constructor and destructor are implemented in nsCellMap.cpp

  /** Initialize the mOrigCell pointer 
    * @param aOrigCell  the table cell frame which will be stored in mOrigCell.   
    */ 
  void   Init(nsTableCellFrame* aCellFrame);

  /** does a cell originate from here
    * @return    is true if a cell corresponds to this cellmap entry
    */
  PRBool IsOrig() const;

  /** is the celldata valid
    * @return    is true if no cell originates and the cell is not spanned by 
    *            a row- or colspan. mBits are 0 in this case and mOrigCell is
    *            nsnull
    */
  PRBool IsDead() const;

  /** is the entry spanned by row- or a colspan
    * @return    is true if the entry is spanned by a row- or colspan
    */
  PRBool IsSpan() const;

  /** is the entry spanned by rowspan
    * @return    is true if the entry is spanned by a rowspan
    */
  PRBool IsRowSpan() const;
  
  /** is the entry spanned by a zero rowspan
    * zero rowspans span all cells starting from the originating cell down to 
    * the end of the rowgroup or a cell originating in the same column
    * @return    is true if the entry is spanned by a zero rowspan
    */
  PRBool IsZeroRowSpan() const;

  /** mark the current entry as spanned by a zero rowspan
    * @param aIsZero    if true mark the entry as covered by a zero rowspan
    */
  void SetZeroRowSpan(PRBool aIsZero);

  /** get the distance from the current entry to the corresponding origin of the rowspan
    * @return    containing the distance in the column to the originating cell
    */
  PRUint32 GetRowSpanOffset() const;

  /** set the distance from the current entry to the corresponding origin of the rowspan
    * @param    the distance in the column to the originating cell
    */
  void SetRowSpanOffset(PRUint32 aSpan);

  /** is the entry spanned by colspan
    * @return    is true if the entry is spanned by a colspan
    */
  PRBool IsColSpan() const;

  /** is the entry spanned by a zero colspan
    * zero colspans span all cells starting from the originating cell towards 
    * the end of the colgroup or a cell originating in the same row 
    * or a rowspanned entry
    * @return    is true if the entry is spanned by a zero colspan
    */
  PRBool IsZeroColSpan() const;

  /** mark the current entry as spanned by a zero colspan
    * @param aIsZero    if true mark the entry as covered by a zero colspan
    */
  void SetZeroColSpan(PRBool aIsZero);

  /** get the distance from the current entry to the corresponding origin of the colspan
    * @return    containing the distance in the row to the originating cell
    */
  PRUint32 GetColSpanOffset() const;

  /** set the distance from the current entry to the corresponding origin of the colspan
    * @param    the distance in the column to the originating cell
    */
  void SetColSpanOffset(PRUint32 aSpan);
  
  /** is the entry spanned by a row- and a colspan
    * @return    is true if the entry is spanned by a row- and a colspan
    */
  PRBool IsOverlap() const;

  /** mark the current entry as spanned by a row- and a colspan
    * @param aOverlap    if true mark the entry as covered by a row- and a colspan
    */
  void SetOverlap(PRBool aOverlap);
  
  /** get the table cell frame for this entry 
    * @return    a pointer to the cellframe, this will be nsnull when the entry 
    *            is only a spanned entry
    */
  nsTableCellFrame* GetCellFrame() const;

protected:

  // this union relies on the assumption that an object (not primitive type) does
  // not start on an odd bit boundary. If mSpan is 0 then mOrigCell is in effect 
  // and the data does not represent a span. If mSpan is 1, then mBits is in
  // effect and the data represents a span.
  // mBits must be an unsigned long because it must match the size of 
  // mOrigCell on both 32- and 64-bit platforms.
  union {
    nsTableCellFrame* mOrigCell;
    unsigned long     mBits;
  };
};

// Border Collapsing Cell Data
enum BCBorderOwner 
{
  eTableOwner        =  0,
  eColGroupOwner     =  1, 
  eAjaColGroupOwner  =  2, // col group to the left 
  eColOwner          =  3,
  eAjaColOwner       =  4, // col to the left
  eRowGroupOwner     =  5, 
  eAjaRowGroupOwner  =  6, // row group above
  eRowOwner          =  7, 
  eAjaRowOwner       =  8, // row above
  eCellOwner         =  9,
  eAjaCellOwner      = 10  // cell to the top or to the left
};

// These are the max sizes that are stored. If they are exceeded, then the max is stored and
// the actual value is computed when needed.
#define MAX_BORDER_WIDTH      64
#define MAX_CORNER_SUB_WIDTH 128

// BCData stores the top and left border info and the corner connecting the two.
class BCData
{
public:
  BCData();

  ~BCData();

  nscoord GetLeftEdge(BCBorderOwner& aOwner,
                      PRBool&        aStart) const;

  void SetLeftEdge(BCBorderOwner aOwner,
                   nscoord       aSize,
                   PRBool        aStart);

  nscoord GetTopEdge(BCBorderOwner& aOwner,
                     PRBool&        aStart) const;

  void SetTopEdge(BCBorderOwner aOwner,
                  nscoord       aSize,
                  PRBool        aStart);

  PRUint8 GetCorner(PRUint8&       aCornerOwner,
                    PRPackedBool&  aBevel) const;

  void SetCorner(PRUint8 aOwner,
                 PRUint8 aSubSize,
                 PRBool  aBevel);

  PRBool IsLeftStart() const;

  void SetLeftStart(PRBool aValue);

  PRBool IsTopStart() const;

  void SetTopStart(PRBool aValue);

               
protected:
  unsigned mLeftOwner:     4; // owner of left border     
  unsigned mLeftSize:      6; // size in pixels of left border
  unsigned mLeftStart:     1; // set if this is the start of a vertical border segment
  unsigned mCornerSide:    2; // side of the owner of the upper left corner relative to the corner
  unsigned mCornerSubSize: 7; // size of the largest border not in the dominate plane (for example, if
                              // corner is owned by the segment to its top or bottom, then the size is the
                              // max of the border sizes of the segments to its left or right.
  unsigned mCornerBevel:   1; // is the corner beveled (only two segments, perpendicular, not dashed or dotted).
  unsigned mTopOwner:      4; // owner of top border    
  unsigned mTopSize:       6; // size in pixels of top border
  unsigned mTopStart:      1; // set if this is the start of a horizontal border segment
};

// BCCellData entries replace CellData entries in the cell map if the border collapsing model is in
// effect. BCData for a row and col entry contains the left and top borders of cell at that row and 
// col and the corner connecting the two. The right borders of the cells in the last col and the bottom
// borders of the last row are stored in separate BCData entries in the cell map.
class BCCellData : public CellData
{
public:
  BCCellData(nsTableCellFrame* aOrigCell);
  ~BCCellData();

  BCData mData;
};


#define SPAN             0x00000001 // there a row or col span 
#define ROW_SPAN         0x00000002 // there is a row span
#define ROW_SPAN_0       0x00000004 // the row span is 0
#define ROW_SPAN_OFFSET  0x0000FFF8 // the row offset to the data containing the original cell
#define COL_SPAN         0x00010000 // there is a col span
#define COL_SPAN_0       0x00020000 // the col span is 0
#define OVERLAP          0x00040000 // there is a row span and col span but no by same cell
#define COL_SPAN_OFFSET  0xFFF80000 // the col offset to the data containing the original cell
#define ROW_SPAN_SHIFT   3          // num bits to shift to get right justified row span
#define COL_SPAN_SHIFT   19         // num bits to shift to get right justified col span

inline nsTableCellFrame* CellData::GetCellFrame() const
{
  if (SPAN != (SPAN & mBits)) {
    return mOrigCell;
  }
  return nsnull;
}

inline void CellData::Init(nsTableCellFrame* aCellFrame) 
{
  mOrigCell = aCellFrame;
}

inline PRBool CellData::IsOrig() const
{
  return ((nsnull != mOrigCell) && (SPAN != (SPAN & mBits)));
}

inline PRBool CellData::IsDead() const
{
  return (0 == mBits);
}

inline PRBool CellData::IsSpan() const
{
  return (SPAN == (SPAN & mBits));
}

inline PRBool CellData::IsRowSpan() const
{
  return (SPAN     == (SPAN & mBits)) && 
         (ROW_SPAN == (ROW_SPAN & mBits));
}

inline PRBool CellData::IsZeroRowSpan() const
{
  return (SPAN       == (SPAN & mBits))     && 
         (ROW_SPAN   == (ROW_SPAN & mBits)) &&
         (ROW_SPAN_0 == (ROW_SPAN_0 & mBits));
}

inline void CellData::SetZeroRowSpan(PRBool aIsZeroSpan)
{
  if (SPAN == (SPAN & mBits)) {
    if (aIsZeroSpan) {
      mBits |= ROW_SPAN_0;
    }
    else {
      mBits &= ~ROW_SPAN_0;
    }
  }
}

inline PRUint32 CellData::GetRowSpanOffset() const
{
  if ((SPAN == (SPAN & mBits)) && ((ROW_SPAN == (ROW_SPAN & mBits)))) {
    return (PRUint32)((mBits & ROW_SPAN_OFFSET) >> ROW_SPAN_SHIFT);
  }
  return 0;
}

inline void CellData::SetRowSpanOffset(PRUint32 aSpan) 
{
  mBits &= ~ROW_SPAN_OFFSET;
  mBits |= (aSpan << ROW_SPAN_SHIFT);
  mBits |= SPAN;
  mBits |= ROW_SPAN;
}

inline PRBool CellData::IsColSpan() const
{
  return (SPAN     == (SPAN & mBits)) && 
         (COL_SPAN == (COL_SPAN & mBits));
}

inline PRBool CellData::IsZeroColSpan() const
{
  return (SPAN       == (SPAN & mBits))     && 
         (COL_SPAN   == (COL_SPAN & mBits)) &&
         (COL_SPAN_0 == (COL_SPAN_0 & mBits));
}

inline void CellData::SetZeroColSpan(PRBool aIsZeroSpan)
{
  if (SPAN == (SPAN & mBits)) {
    if (aIsZeroSpan) {
      mBits |= COL_SPAN_0;
    }
    else {
      mBits &= ~COL_SPAN_0;
    }
  }
}

inline PRUint32 CellData::GetColSpanOffset() const
{
  if ((SPAN == (SPAN & mBits)) && ((COL_SPAN == (COL_SPAN & mBits)))) {
    return (PRUint32)((mBits & COL_SPAN_OFFSET) >> COL_SPAN_SHIFT);
  }
  return 0;
}

inline void CellData::SetColSpanOffset(PRUint32 aSpan) 
{
  mBits &= ~COL_SPAN_OFFSET;
  mBits |= (aSpan << COL_SPAN_SHIFT);

  mBits |= SPAN;
  mBits |= COL_SPAN;
}

inline PRBool CellData::IsOverlap() const
{
  return (SPAN == (SPAN & mBits)) && (OVERLAP == (OVERLAP & mBits));
}

inline void CellData::SetOverlap(PRBool aOverlap) 
{
  if (SPAN == (SPAN & mBits)) {
    if (aOverlap) {
      mBits |= OVERLAP;
    }
    else {
      mBits &= ~OVERLAP;
    }
  }
}

inline BCData::BCData()
{
  mLeftOwner = mTopOwner = eCellOwner;
  mLeftStart = mTopStart = 1;
  mLeftSize = mCornerSide = mCornerSubSize = mTopSize = 0;
}

inline BCData::~BCData()
{
}

inline nscoord BCData::GetLeftEdge(BCBorderOwner& aOwner,
                                   PRBool&        aStart) const
{
  aOwner = (BCBorderOwner)mLeftOwner;
  aStart = (PRBool)mLeftStart;

  return (nscoord)mLeftSize;
}

inline void BCData::SetLeftEdge(BCBorderOwner  aOwner,
                                nscoord        aSize,
                                PRBool         aStart)
{
  mLeftOwner = aOwner;
  mLeftSize  = (aSize > MAX_BORDER_WIDTH) ? MAX_BORDER_WIDTH : aSize;
  mLeftStart = aStart;
}

inline nscoord BCData::GetTopEdge(BCBorderOwner& aOwner,
                                  PRBool&        aStart) const
{
  aOwner = (BCBorderOwner)mTopOwner;
  aStart = (PRBool)mTopStart;

  return (nscoord)mTopSize;
}

inline void BCData::SetTopEdge(BCBorderOwner  aOwner,
                               nscoord        aSize,
                               PRBool         aStart)
{
  mTopOwner = aOwner;
  mTopSize  = (aSize > MAX_BORDER_WIDTH) ? MAX_BORDER_WIDTH : aSize;
  mTopStart = aStart;
}

inline PRUint8 BCData::GetCorner(PRUint8&       aOwnerSide,
                                 PRPackedBool&  aBevel) const
{
  aOwnerSide = mCornerSide;
  aBevel     = (PRBool)mCornerBevel;
  return (PRUint8)mCornerSubSize;
}

inline void BCData::SetCorner(PRUint8 aSubSize,
                              PRUint8 aOwnerSide,
                              PRBool  aBevel)
{
  mCornerSubSize = (aSubSize > MAX_CORNER_SUB_WIDTH) ? MAX_CORNER_SUB_WIDTH : aSubSize;
  mCornerSide    = aOwnerSide;
  mCornerBevel   = aBevel;
}

inline PRBool BCData::IsLeftStart() const
{
  return (PRBool)mLeftStart;
}

inline void BCData::SetLeftStart(PRBool aValue)
{
  mLeftStart = aValue;
}

inline PRBool BCData::IsTopStart() const
{
  return (PRBool)mTopStart;
}

inline void BCData::SetTopStart(PRBool aValue)
{
  mTopStart = aValue;
}

#endif
