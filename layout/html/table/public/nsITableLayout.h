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
#ifndef nsITableLayout_h__
#define nsITableLayout_h__

#include "nsISupports.h"
class nsIDOMElement;

// IID for the nsITableLayout interface 
// A9222E6B-437E-11d3-B227-004095E27A10
#define NS_ITABLELAYOUT_IID \
 { 0xa9222e6b, 0x437e, 0x11d3, { 0xb2, 0x27, 0x0, 0x40, 0x95, 0xe2, 0x7a, 0x10 }}


#define NS_TABLELAYOUT_CELL_NOT_FOUND \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 3)


/**
 * nsITableLayout
 * interface for layout objects that act like tables.
 * initially, we use this to get cell info 
 *
 * @author  sclark
 */
class nsITableLayout : public nsISupports
{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_ITABLELAYOUT_IID; return iid; }

  /** return all the relevant layout information about a cell.
   *  @param aRowIndex       a row which the cell intersects
   *  @param aColIndex       a col which the cell intersects
   *  @param aCell           [OUT] the content representing the cell at (aRowIndex, aColIndex)
   *  @param aStartRowIndex  [OUT] the row in which aCell starts
   *  @param aStartColIndex  [OUT] the col in which aCell starts
   *  @param aRowSpan        [OUT] the number of rows aCell spans
   *  @param aColSpan        [OUT] the number of cols aCell spans
   *  @param aIsSelected     [OUT] PR_TRUE if the frame that maps aCell is selected
   *                               in the presentation shell that owns this.
   */
  NS_IMETHOD GetCellDataAt(PRInt32 aRowIndex, PRInt32 aColIndex, 
                           nsIDOMElement* &aCell,   //out params
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                           PRInt32& aRowSpan, PRInt32& aColSpan,
                           PRBool& aIsSelected)=0;
};

#endif
