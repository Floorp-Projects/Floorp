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

class nsTableCellFrame;

/** 
  * Data stored by nsCellMap to rationalize rowspan and colspan cells.
  */
class CellData
{
public:
  CellData(nsTableCellFrame* aOrigCell);

  ~CellData();

  PRBool IsOrig() const;

  PRBool IsSpan() const;

  PRBool IsRowSpan() const;
  PRBool IsZeroRowSpan() const;
  void SetZeroRowSpan(PRBool aIsZero);
  PRUint32 GetRowSpanOffset() const;
  void SetRowSpanOffset(PRUint32 aSpan);

  PRBool IsColSpan() const;
  PRBool IsZeroColSpan() const;
  void SetZeroColSpan(PRBool aIsZero);
  PRUint32 GetColSpanOffset() const;
  void SetColSpanOffset(PRUint32 aSpan);

  PRBool IsOverlap() const;
  void SetOverlap(PRBool aOverlap);

  nsTableCellFrame* GetCellFrame() const;

protected:

  // this union relies on the assumption that an object (not primitive type) does
  // not start on an odd bit boundary. If mSpan is 0 then mOrigCell is in effect 
  // and the data does not represent a span. If mSpan is 1, then mBits is in
  // effect and the data represents a span.
  union {
    nsTableCellFrame* mOrigCell;
    long              mBits;
  };
};

#define SPAN             0x00000001 // there a row or col span 
#define ROW_SPAN         0x00000002 // there is a row span
#define ROW_SPAN_0       0x00000004 // the row span is 0
#define ROW_SPAN_OFFSET  0x0000FFF8 // the row offset to the data containing the original cell
#define COL_SPAN         0x00010000 // there is a col span
#define COL_SPAN_0       0x00020000 // the col span is 0
#define OVERLAP          0x00040000 // there is a row span and col span but no by same cell
#define COL_SPAN_OFFSET  0xFFF80000 // the col offset to the data containing the original cell
#define ROW_SPAN_SHIFT   3          // num bits to shift to get right justified col span
#define COL_SPAN_SHIFT   19         // num bits to shift to get right justified col span

inline nsTableCellFrame* CellData::GetCellFrame() const
{
  if (SPAN != (SPAN & mBits)) {
    return mOrigCell;
  }
  return nsnull;
}

inline PRBool CellData::IsOrig() const
{
  return (SPAN != (SPAN & mBits));
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

#endif
