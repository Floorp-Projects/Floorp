/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsITableCellLayout_h__
#define nsITableCellLayout_h__

#include "nsQueryFrame.h"

#define MAX_ROWSPAN 65534 // the cellmap can not handle more.
#define MAX_COLSPAN 1000 // limit as IE and opera do.  If this ever changes,
                         // change COL_SPAN_OFFSET/COL_SPAN_SHIFT accordingly.

/**
 * nsITableCellLayout
 * interface for layout objects that act like table cells.
 *
 * @author  sclark
 */
class nsITableCellLayout
{
public:

  NS_DECL_QUERYFRAME_TARGET(nsITableCellLayout)

  /** return the mapped cell's row and column indexes (starting at 0 for each) */
  NS_IMETHOD GetCellIndexes(int32_t &aRowIndex, int32_t &aColIndex)=0;

  /** return the mapped cell's row index (starting at 0 for the first row) */
  virtual nsresult GetRowIndex(int32_t &aRowIndex) const = 0;
  
  /** return the mapped cell's column index (starting at 0 for the first column) */
  virtual nsresult GetColIndex(int32_t &aColIndex) const = 0;
};

#endif



