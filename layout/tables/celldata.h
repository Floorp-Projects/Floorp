/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CellData_h__
#define CellData_h__

#include "nsISupports.h"
#include "nsCoord.h"
#include "mozilla/gfx/Types.h"
#include <stdint.h>

class nsTableCellFrame;
class nsCellMap;
class BCCellData;


#define MAX_ROWSPAN 65534 // the cellmap can not handle more.
#define MAX_COLSPAN 1000 // limit as IE and opera do.  If this ever changes,
                         // change COL_SPAN_OFFSET/COL_SPAN_SHIFT accordingly.

/**
  * Data stored by nsCellMap to rationalize rowspan and colspan cells.
  */
class CellData
{
public:
  /** Initialize the mOrigCell pointer
    * @param aOrigCell  the table cell frame which will be stored in mOrigCell.
    */
  void   Init(nsTableCellFrame* aCellFrame);

  /** does a cell originate from here
    * @return    is true if a cell corresponds to this cellmap entry
    */
  bool IsOrig() const;

  /** is the celldata valid
    * @return    is true if no cell originates and the cell is not spanned by
    *            a row- or colspan. mBits are 0 in this case and mOrigCell is
    *            nullptr
    */
  bool IsDead() const;

  /** is the entry spanned by row- or a colspan
    * @return    is true if the entry is spanned by a row- or colspan
    */
  bool IsSpan() const;

  /** is the entry spanned by rowspan
    * @return    is true if the entry is spanned by a rowspan
    */
  bool IsRowSpan() const;

  /** is the entry spanned by a zero rowspan
    * zero rowspans span all cells starting from the originating cell down to
    * the end of the rowgroup or a cell originating in the same column
    * @return    is true if the entry is spanned by a zero rowspan
    */
  bool IsZeroRowSpan() const;

  /** mark the current entry as spanned by a zero rowspan
    * @param aIsZero    if true mark the entry as covered by a zero rowspan
    */
  void SetZeroRowSpan(bool aIsZero);

  /** get the distance from the current entry to the corresponding origin of the rowspan
    * @return    containing the distance in the column to the originating cell
    */
  uint32_t GetRowSpanOffset() const;

  /** set the distance from the current entry to the corresponding origin of the rowspan
    * @param    the distance in the column to the originating cell
    */
  void SetRowSpanOffset(uint32_t aSpan);

  /** is the entry spanned by colspan
    * @return    is true if the entry is spanned by a colspan
    */
  bool IsColSpan() const;

  /** is the entry spanned by a zero colspan
    * zero colspans span all cells starting from the originating cell towards
    * the end of the colgroup or a cell originating in the same row
    * or a rowspanned entry
    * @return    is true if the entry is spanned by a zero colspan
    */
  bool IsZeroColSpan() const;

  /** mark the current entry as spanned by a zero colspan
    * @param aIsZero    if true mark the entry as covered by a zero colspan
    */
  void SetZeroColSpan(bool aIsZero);

  /** get the distance from the current entry to the corresponding origin of the colspan
    * @return    containing the distance in the row to the originating cell
    */
  uint32_t GetColSpanOffset() const;

  /** set the distance from the current entry to the corresponding origin of the colspan
    * @param    the distance in the column to the originating cell
    */
  void SetColSpanOffset(uint32_t aSpan);

  /** is the entry spanned by a row- and a colspan
    * @return    is true if the entry is spanned by a row- and a colspan
    */
  bool IsOverlap() const;

  /** mark the current entry as spanned by a row- and a colspan
    * @param aOverlap    if true mark the entry as covered by a row- and a colspan
    */
  void SetOverlap(bool aOverlap);

  /** get the table cell frame for this entry
    * @return    a pointer to the cellframe, this will be nullptr when the entry
    *            is only a spanned entry
    */
  nsTableCellFrame* GetCellFrame() const;

private:
  friend class nsCellMap;
  friend class BCCellData;

  /** constructor.
    * @param aOrigCell  the table cell frame which will be stored in mOrigCell.
    */
  CellData(nsTableCellFrame* aOrigCell);  // implemented in nsCellMap.cpp

  /** destructor */
  ~CellData(); // implemented in nsCellMap.cpp

protected:

  // this union relies on the assumption that an object (not primitive type) does
  // not start on an odd bit boundary. If mSpan is 0 then mOrigCell is in effect
  // and the data does not represent a span. If mSpan is 1, then mBits is in
  // effect and the data represents a span.
  // mBits must match the size of mOrigCell on both 32- and 64-bit platforms.
  union {
    nsTableCellFrame* mOrigCell;
    uintptr_t         mBits;
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

typedef uint16_t BCPixelSize;

// These are the max sizes that are stored. If they are exceeded, then the max is stored and
// the actual value is computed when needed.
#define MAX_BORDER_WIDTH nscoord((1u << (sizeof(BCPixelSize) * 8)) - 1)

static inline nscoord
BC_BORDER_TOP_HALF_COORD(int32_t p2t, uint16_t px)    { return (px - px / 2) * p2t; }
static inline nscoord
BC_BORDER_RIGHT_HALF_COORD(int32_t p2t, uint16_t px)  { return (     px / 2) * p2t; }
static inline nscoord
BC_BORDER_BOTTOM_HALF_COORD(int32_t p2t, uint16_t px) { return (     px / 2) * p2t; }
static inline nscoord
BC_BORDER_LEFT_HALF_COORD(int32_t p2t, uint16_t px)   { return (px - px / 2) * p2t; }

#define BC_BORDER_TOP_HALF(px)    ((px) - (px) / 2)
#define BC_BORDER_RIGHT_HALF(px)  ((px) / 2)
#define BC_BORDER_BOTTOM_HALF(px) ((px) / 2)
#define BC_BORDER_LEFT_HALF(px)   ((px) - (px) / 2)

// BCData stores the top and left border info and the corner connecting the two.
class BCData
{
public:
  BCData();

  ~BCData();

  nscoord GetLeftEdge(BCBorderOwner& aOwner,
                      bool&        aStart) const;

  void SetLeftEdge(BCBorderOwner aOwner,
                   nscoord       aSize,
                   bool          aStart);

  nscoord GetTopEdge(BCBorderOwner& aOwner,
                     bool&        aStart) const;

  void SetTopEdge(BCBorderOwner aOwner,
                  nscoord       aSize,
                  bool          aStart);

  BCPixelSize GetCorner(mozilla::Side&       aCornerOwner,
                        bool&  aBevel) const;

  void SetCorner(BCPixelSize aSubSize,
                 mozilla::Side aOwner,
                 bool    aBevel);

  bool IsLeftStart() const;

  void SetLeftStart(bool aValue);

  bool IsTopStart() const;

  void SetTopStart(bool aValue);


protected:
  BCPixelSize mLeftSize;      // size in pixels of left border
  BCPixelSize mTopSize;       // size in pixels of top border
  BCPixelSize mCornerSubSize; // size of the largest border not in the
                              //   dominant plane (for example, if corner is
                              //   owned by the segment to its top or bottom,
                              //   then the size is the max of the border
                              //   sizes of the segments to its left or right.
  unsigned mLeftOwner:     4; // owner of left border
  unsigned mTopOwner:      4; // owner of top border
  unsigned mLeftStart:     1; // set if this is the start of a vertical border segment
  unsigned mTopStart:      1; // set if this is the start of a horizontal border segment
  unsigned mCornerSide:    2; // mozilla::Side of the owner of the upper left corner relative to the corner
  unsigned mCornerBevel:   1; // is the corner beveled (only two segments, perpendicular, not dashed or dotted).
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


// The layout of a celldata is as follows.  The top 10 bits are the colspan
// offset (which is enough to represent our allowed values 1-1000 for colspan).
// Then there are three bits of flags.  Then 16 bits of rowspan offset (which
// lets us represent numbers up to 65535.  Then another 3 bits of flags.

// num bits to shift right to get right aligned col span
#define COL_SPAN_SHIFT   22
// num bits to shift right to get right aligned row span
#define ROW_SPAN_SHIFT   3

// the col offset to the data containing the original cell.
#define COL_SPAN_OFFSET  (0x3FF << COL_SPAN_SHIFT)
// the row offset to the data containing the original cell
#define ROW_SPAN_OFFSET  (0xFFFF << ROW_SPAN_SHIFT)

// And the flags
#define SPAN             0x00000001 // there a row or col span
#define ROW_SPAN         0x00000002 // there is a row span
#define ROW_SPAN_0       0x00000004 // the row span is 0
#define COL_SPAN         (1 << (COL_SPAN_SHIFT - 3)) // there is a col span
#define COL_SPAN_0       (1 << (COL_SPAN_SHIFT - 2)) // the col span is 0
#define OVERLAP          (1 << (COL_SPAN_SHIFT - 1)) // there is a row span and
                                                     // col span but not by
                                                     // same cell

inline nsTableCellFrame* CellData::GetCellFrame() const
{
  if (SPAN != (SPAN & mBits)) {
    return mOrigCell;
  }
  return nullptr;
}

inline void CellData::Init(nsTableCellFrame* aCellFrame)
{
  mOrigCell = aCellFrame;
}

inline bool CellData::IsOrig() const
{
  return ((nullptr != mOrigCell) && (SPAN != (SPAN & mBits)));
}

inline bool CellData::IsDead() const
{
  return (0 == mBits);
}

inline bool CellData::IsSpan() const
{
  return (SPAN == (SPAN & mBits));
}

inline bool CellData::IsRowSpan() const
{
  return (SPAN     == (SPAN & mBits)) &&
         (ROW_SPAN == (ROW_SPAN & mBits));
}

inline bool CellData::IsZeroRowSpan() const
{
  return (SPAN       == (SPAN & mBits))     &&
         (ROW_SPAN   == (ROW_SPAN & mBits)) &&
         (ROW_SPAN_0 == (ROW_SPAN_0 & mBits));
}

inline void CellData::SetZeroRowSpan(bool aIsZeroSpan)
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

inline uint32_t CellData::GetRowSpanOffset() const
{
  if ((SPAN == (SPAN & mBits)) && ((ROW_SPAN == (ROW_SPAN & mBits)))) {
    return (uint32_t)((mBits & ROW_SPAN_OFFSET) >> ROW_SPAN_SHIFT);
  }
  return 0;
}

inline void CellData::SetRowSpanOffset(uint32_t aSpan)
{
  mBits &= ~ROW_SPAN_OFFSET;
  mBits |= (aSpan << ROW_SPAN_SHIFT);
  mBits |= SPAN;
  mBits |= ROW_SPAN;
}

inline bool CellData::IsColSpan() const
{
  return (SPAN     == (SPAN & mBits)) &&
         (COL_SPAN == (COL_SPAN & mBits));
}

inline bool CellData::IsZeroColSpan() const
{
  return (SPAN       == (SPAN & mBits))     &&
         (COL_SPAN   == (COL_SPAN & mBits)) &&
         (COL_SPAN_0 == (COL_SPAN_0 & mBits));
}

inline void CellData::SetZeroColSpan(bool aIsZeroSpan)
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

inline uint32_t CellData::GetColSpanOffset() const
{
  if ((SPAN == (SPAN & mBits)) && ((COL_SPAN == (COL_SPAN & mBits)))) {
    return (uint32_t)((mBits & COL_SPAN_OFFSET) >> COL_SPAN_SHIFT);
  }
  return 0;
}

inline void CellData::SetColSpanOffset(uint32_t aSpan)
{
  mBits &= ~COL_SPAN_OFFSET;
  mBits |= (aSpan << COL_SPAN_SHIFT);

  mBits |= SPAN;
  mBits |= COL_SPAN;
}

inline bool CellData::IsOverlap() const
{
  return (SPAN == (SPAN & mBits)) && (OVERLAP == (OVERLAP & mBits));
}

inline void CellData::SetOverlap(bool aOverlap)
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
  mLeftSize = mCornerSubSize = mTopSize = 0;
  mCornerSide = NS_SIDE_TOP;
  mCornerBevel = false;
}

inline BCData::~BCData()
{
}

inline nscoord BCData::GetLeftEdge(BCBorderOwner& aOwner,
                                   bool&        aStart) const
{
  aOwner = (BCBorderOwner)mLeftOwner;
  aStart = (bool)mLeftStart;

  return (nscoord)mLeftSize;
}

inline void BCData::SetLeftEdge(BCBorderOwner  aOwner,
                                nscoord        aSize,
                                bool           aStart)
{
  mLeftOwner = aOwner;
  mLeftSize  = (aSize > MAX_BORDER_WIDTH) ? MAX_BORDER_WIDTH : aSize;
  mLeftStart = aStart;
}

inline nscoord BCData::GetTopEdge(BCBorderOwner& aOwner,
                                  bool&        aStart) const
{
  aOwner = (BCBorderOwner)mTopOwner;
  aStart = (bool)mTopStart;

  return (nscoord)mTopSize;
}

inline void BCData::SetTopEdge(BCBorderOwner  aOwner,
                               nscoord        aSize,
                               bool           aStart)
{
  mTopOwner = aOwner;
  mTopSize  = (aSize > MAX_BORDER_WIDTH) ? MAX_BORDER_WIDTH : aSize;
  mTopStart = aStart;
}

inline BCPixelSize BCData::GetCorner(mozilla::Side& aOwnerSide,
                                     bool&       aBevel) const
{
  aOwnerSide = mozilla::Side(mCornerSide);
  aBevel     = (bool)mCornerBevel;
  return mCornerSubSize;
}

inline void BCData::SetCorner(BCPixelSize aSubSize,
                              mozilla::Side aOwnerSide,
                              bool    aBevel)
{
  mCornerSubSize = aSubSize;
  mCornerSide    = aOwnerSide;
  mCornerBevel   = aBevel;
}

inline bool BCData::IsLeftStart() const
{
  return (bool)mLeftStart;
}

inline void BCData::SetLeftStart(bool aValue)
{
  mLeftStart = aValue;
}

inline bool BCData::IsTopStart() const
{
  return (bool)mTopStart;
}

inline void BCData::SetTopStart(bool aValue)
{
  mTopStart = aValue;
}

#endif
