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
#ifndef nsITableCellLayout_h__
#define nsITableCellLayout_h__

#include "nsISupports.h"

// IID for the nsITableCellLayout interface 
// 8c921430-ba23-11d2-8f4b-006008159b0c
#define NS_ITABLECELLAYOUT_IID \
 { 0x8c921430, 0xba23, 0x11d2,{0x8f, 0xb4, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x0c}}

/**
 * nsITableCellLayout
 * interface for layout objects that act like table cells.
 *
 * @author  sclark
 */
class nsITableCellLayout : public nsISupports
{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_ITABLECELLAYOUT_IID; return iid; }

  /** return the mapped cell's row and column indexes (starting at 0 for each) */
  NS_IMETHOD GetCellIndexes(PRInt32 &aRowIndex, PRInt32 &aColIndex)=0;

  /** return the mapped cell's row index (starting at 0 for the first row) */
  virtual nsresult GetRowIndex(PRInt32 &aRowIndex) const = 0;
  
  /** return the mapped cell's column index (starting at 0 for the first column) */
  virtual nsresult GetColIndex(PRInt32 &aColIndex) const = 0;
};


#endif



