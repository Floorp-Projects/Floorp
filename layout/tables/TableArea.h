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
  TableArea() : mRect() { }
  TableArea(int32_t aStartCol, int32_t aStartRow,
            int32_t aColCount, int32_t aRowCount)
    : mRect(aStartCol, aStartRow, aColCount, aRowCount) { }

  int32_t& StartCol() { return mRect.x; }
  int32_t& StartRow() { return mRect.y; }
  int32_t& ColCount() { return mRect.width; }
  int32_t& RowCount() { return mRect.height; }

  int32_t StartCol() const { return mRect.x; }
  int32_t StartRow() const { return mRect.y; }
  int32_t ColCount() const { return mRect.width; }
  int32_t RowCount() const { return mRect.height; }
  int32_t EndCol() const { return mRect.XMost(); }
  int32_t EndRow() const { return mRect.YMost(); }

  void UnionArea(const TableArea& aArea1, const TableArea& aArea2)
    { mRect.UnionRect(aArea1.mRect, aArea2.mRect); }

private:
  nsIntRect mRect;
};

}

#endif // mozilla_TableArea_h_
