/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_TableArea_h_
#define mozilla_TableArea_h_

#include "nsRect.h"

namespace mozilla {

struct TableArea
{
  TableArea()
    : mStartCol(0), mStartRow(0), mColCount(0), mRowCount(0) { }
  TableArea(int32_t aStartCol, int32_t aStartRow,
            int32_t aColCount, int32_t aRowCount)
    : mStartCol(aStartCol),
      mStartRow(aStartRow),
      mColCount(aColCount),
      mRowCount(aRowCount) { }

  int32_t& StartCol() { return mStartCol; }
  int32_t& StartRow() { return mStartRow; }
  int32_t& ColCount() { return mColCount; }
  int32_t& RowCount() { return mRowCount; }

  int32_t StartCol() const { return mStartCol; }
  int32_t StartRow() const { return mStartRow; }
  int32_t ColCount() const { return mColCount; }
  int32_t RowCount() const { return mRowCount; }
  int32_t EndCol() const { return mStartCol + mColCount; }
  int32_t EndRow() const { return mStartRow + mRowCount; }

  void UnionArea(const TableArea& aArea1, const TableArea& aArea2)
    {
      nsIntRect rect(aArea1.mStartCol, aArea1.mStartRow,
                     aArea1.mColCount, aArea1.mRowCount);
      rect.UnionRect(rect, nsIntRect(aArea2.mStartCol, aArea2.mStartRow,
                                     aArea2.mColCount, aArea2.mRowCount));
      rect.GetRect(&mStartCol, &mStartRow, &mColCount, &mRowCount);
    }

private:
  int32_t mStartCol, mStartRow, mColCount, mRowCount;
};

} // namespace mozilla

#endif // mozilla_TableArea_h_
