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
   *  @param aStartRowIndex  [IN/OUT] the row in which aCell starts
   *  @param aStartColIndex  [IN/OUT] the col in which aCell starts
   *                         Initialize these with the "candidate" start indexes to use
   *                           for searching through the table when a cell isn't found 
   *                           because of "holes" in the cellmap
   *                           when ROWSPAN and/or COLSPAN > 1
   *  @param aRowSpan        [OUT] the value of the ROWSPAN attribute (may be 0 or actual number)
   *  @param aColSpan        [OUT] the value of the COLSPAN attribute (may be 0 or actual number)
   *  @param aActualRowSpan  [OUT] the actual number of rows aCell spans
   *  @param aActualColSpan  [OUT] the acutal number of cols aCell spans
   *  @param aIsSelected     [OUT] PR_TRUE if the frame that maps aCell is selected
   *                               in the presentation shell that owns this.
   */
  NS_IMETHOD GetCellDataAt(PRInt32 aRowIndex, PRInt32 aColIndex,
                           nsIDOMElement* &aCell,   //out params
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                           PRInt32& aRowSpan, PRInt32& aColSpan,
                           PRInt32& aActualRowSpan, PRInt32& aActualColSpan,
                           PRBool& aIsSelected)=0;

  /** Get the number of rows and column for a table from the frame's cellmap 
   *  Some rows may not have enough cells (the number returned is the maximum possible),
   *  which displays as a ragged-right edge table
   */
  NS_IMETHOD GetTableSize(PRInt32& aRowCount, PRInt32& aColCount)=0;
};

#endif
