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
#ifndef nsTableRow_h__
#define nsTableRow_h__

#include "nscore.h"
#include "nsTableContent.h"
#include "nsTableRowGroup.h"


/**
 * nsTableRow is the content object that represents table rows 
 * (HTML tag TR). This class cannot be reused
 * outside of an nsTableRowGroup.  It assumes that its parent is an nsTableRowGroup, and 
 * its children are nsTableCells.
 * 
 * @see nsTablePart
 * @see nsTableRowGroup
 * @see nsTableCell
 *
 * @author  sclark
 */
class nsTableRow : public nsTableContent
{

private:
  /** parent pointer */
  nsTableRowGroup *  mRowGroup;

  /** the index of the row this content object represents */
  PRInt32            mRowIndex;

public:

  /** constructor
    * @param aTag  the HTML tag causing this row to get constructed.
    */
  nsTableRow (nsIAtom* aTag);

  /** constructor
    * @param aTag  the HTML tag causing this row to get constructed.
    * @param aImplicit  PR_TRUE if there is no actual input tag corresponding to
    *                   this row.
    */
  nsTableRow (nsIAtom* aTag, PRBool aImplicit);

  /** destructor, not responsible for any memory destruction itself */
  virtual ~nsTableRow();

  // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  /** returns nsITableContent::kTableRowType */
  virtual int GetType();

  /** @see nsIHTMLContent::CreateFrame */
  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  /** return the row group that contains me (my parent) */
  virtual nsTableRowGroup *GetRowGroup ();


  /** Set my parent row group.<br>
    * NOTE: Since mRowGroup is the parent of the table row,
    * reference counting should not be done on 
    * this variable when setting the row.
    * see /ns/raptor/doc/MemoryModel.html
    **/
  virtual void SetRowGroup (nsTableRowGroup * aRowGroup);

  /** return this row's starting row index */
  virtual PRInt32 GetRowIndex ();

  /** set this row's starting row index */
  virtual void SetRowIndex (int aRowIndex);

  /** return the number of columns represented by the cells in this row */
  virtual PRInt32 GetMaxColumns();

  /** notify the containing nsTablePart that cell information has changed */
  virtual void ResetCellMap ();

  /* ----------- nsTableContent overrides ----------- */

  /** can only append objects that are cells (implement nsITableContent and are .
    * of type nsITableContent::kTableCellType.)
    * @see nsIContent::AppendChild
    */
  virtual PRBool AppendChild (nsIContent * aContent);

  /** can only insert objects that are cells (implement nsITableContent and are .
    * of type nsITableContent::kTableCellType.)
    * @see nsIContent::InsertChildAt
    */
  virtual PRBool InsertChildAt (nsIContent * aContent, int aIndex);

  /** can only replace child objects with objects that are cells 
    * (implement nsITableContent and are * of type nsITableContent::kTableCellType.)
    * @param aContent the object to insert, must be a cell
    * @param aIndex   the index of the object to replace.  Must be in the range
    *                 0<=aIndex<ChildCount().
    * @see nsIContent::ReplaceChildAt
    */
  virtual PRBool ReplaceChildAt (nsIContent * aContent, int aIndex);

  /** @see nsIContent::InsertChildAt */
  virtual PRBool RemoveChildAt (int aIndex);


};

#endif



