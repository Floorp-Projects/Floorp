/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CellData_h__
#define CellData_h__

#include "nsISupports.h"
#include "nsITableCellLayout.h" // for MAX_COLSPAN / MAX_ROWSPAN
#include "nsCoord.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/WritingModes.h"
#include <stdint.h>

class nsTableCellFrame;
class nsCellMap;
class BCCellData;


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
  explicit CellData(nsTableCellFrame* aOrigCell);  // implemented in nsCellMap.cpp

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

// The half of border on inline/block-axis start side
static inline BCPixelSize
BC_BORDER_START_HALF(BCPixelSize px) { return px - px / 2; }
// The half of border on inline/block-axis end side
static inline BCPixelSize
BC_BORDER_END_HALF(BCPixelSize px) { return px / 2; }

static inline nscoord
BC_BORDER_START_HALF_COORD(int32_t p2t, BCPixelSize px)
  { return BC_BORDER_START_HALF(px) * p2t; }
static inline nscoord
BC_BORDER_END_HALF_COORD(int32_t p2t, BCPixelSize px)
  { return BC_BORDER_END_HALF(px) * p2t; }

// BCData stores the bstart and istart border info and the corner connecting the two.
class BCData
{
public:
  BCData();

  ~BCData();

  nscoord GetIStartEdge(BCBorderOwner& aOwner,
                        bool&          aStart) const;

  void SetIStartEdge(BCBorderOwner aOwner,
                     nscoord       aSize,
                     bool          aStart);

  nscoord GetBStartEdge(BCBorderOwner& aOwner,
                        bool&          aStart) const;

  void SetBStartEdge(BCBorderOwner aOwner,
                     nscoord       aSize,
                     bool          aStart);

  BCPixelSize GetCorner(mozilla::LogicalSide& aCornerOwner,
                        bool&                 aBevel) const;

  void SetCorner(BCPixelSize          aSubSize,
                 mozilla::LogicalSide aOwner,
                 bool                 aBevel);

  bool IsIStartStart() const;

  void SetIStartStart(bool aValue);

  bool IsBStartStart() const;

  void SetBStartStart(bool aValue);


protected:
  BCPixelSize mIStartSize;    // size in pixels of iStart border
  BCPixelSize mBStartSize;    // size in pixels of bStart border
  BCPixelSize mCornerSubSize; // size of the largest border not in the
                              //   dominant plane (for example, if corner is
                              //   owned by the segment to its bStart or bEnd,
                              //   then the size is the max of the border
                              //   sizes of the segments to its iStart or iEnd.
  unsigned mIStartOwner:   4; // owner of iStart border
  unsigned mBStartOwner:   4; // owner of bStart border
  unsigned mIStartStart:   1; // set if this is the start of a block-dir border segment
  unsigned mBStartStart:   1; // set if this is the start of an inline-dir border segment
  unsigned mCornerSide:    2; // LogicalSide of the owner of the bStart-iStart corner relative to the corner
  unsigned mCornerBevel:   1; // is the corner beveled (only two segments, perpendicular, not dashed or dotted).
};

// BCCellData entries replace CellData entries in the cell map if the border collapsing model is in
// effect. BCData for a row and col entry contains the left and top borders of cell at that row and
// col and the corner connecting the two. The right borders of the cells in the last col and the bottom
// borders of the last row are stored in separate BCData entries in the cell map.
class BCCellData : public CellData
{
public:
  explicit BCCellData(nsTableCellFrame* aOrigCell);
  ~BCCellData();

  BCData mData;
};


// The layout of a celldata is as follows.  The top 10 bits are the colspan
// offset (which is enough to represent our allowed values 1-1000 for colspan).
// Then there are two bits of flags.
// XXXmats Then one unused bit that we should decide how to use in bug 862624.
// Then 16 bits of rowspan offset (which
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
#define COL_SPAN         (1 << (COL_SPAN_SHIFT - 2)) // there is a col span
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
  mIStartOwner = mBStartOwner = eCellOwner;
  mIStartStart = mBStartStart = 1;
  mIStartSize = mCornerSubSize = mBStartSize = 0;
  mCornerSide = mozilla::eLogicalSideBStart;
  mCornerBevel = false;
}

inline BCData::~BCData()
{
}

inline nscoord BCData::GetIStartEdge(BCBorderOwner& aOwner,
                                     bool&          aStart) const
{
  aOwner = (BCBorderOwner)mIStartOwner;
  aStart = (bool)mIStartStart;

  return (nscoord)mIStartSize;
}

inline void BCData::SetIStartEdge(BCBorderOwner  aOwner,
                                  nscoord        aSize,
                                  bool           aStart)
{
  mIStartOwner = aOwner;
  mIStartSize  = (aSize > MAX_BORDER_WIDTH) ? MAX_BORDER_WIDTH : aSize;
  mIStartStart = aStart;
}

inline nscoord BCData::GetBStartEdge(BCBorderOwner& aOwner,
                                     bool&          aStart) const
{
  aOwner = (BCBorderOwner)mBStartOwner;
  aStart = (bool)mBStartStart;

  return (nscoord)mBStartSize;
}

inline void BCData::SetBStartEdge(BCBorderOwner  aOwner,
                                  nscoord        aSize,
                                  bool           aStart)
{
  mBStartOwner = aOwner;
  mBStartSize  = (aSize > MAX_BORDER_WIDTH) ? MAX_BORDER_WIDTH : aSize;
  mBStartStart = aStart;
}

inline BCPixelSize BCData::GetCorner(mozilla::LogicalSide& aOwnerSide,
                                     bool&                 aBevel) const
{
  aOwnerSide = mozilla::LogicalSide(mCornerSide);
  aBevel     = (bool)mCornerBevel;
  return mCornerSubSize;
}

inline void BCData::SetCorner(BCPixelSize          aSubSize,
                              mozilla::LogicalSide aOwnerSide,
                              bool                 aBevel)
{
  mCornerSubSize = aSubSize;
  mCornerSide    = aOwnerSide;
  mCornerBevel   = aBevel;
}

inline bool BCData::IsIStartStart() const
{
  return (bool)mIStartStart;
}

inline void BCData::SetIStartStart(bool aValue)
{
  mIStartStart = aValue;
}

inline bool BCData::IsBStartStart() const
{
  return (bool)mBStartStart;
}

inline void BCData::SetBStartStart(bool aValue)
{
  mBStartStart = aValue;
}

#endif
