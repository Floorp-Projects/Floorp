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
 * nsTableRow
 * datastructure to maintain information about a single table row
 *
 * @author  sclark
 */
class nsTableRow : public nsTableContent
{

private:

  nsTableRowGroup *  mRowGroup;
  PRInt32            mRowIndex;

public:

  nsTableRow (nsIAtom* aTag);

  nsTableRow (nsIAtom* aTag, PRBool aImplicit);

  virtual ~nsTableRow();

  // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual int GetType();

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual nsTableRowGroup *GetRowGroup ();


  /** 
  * Since mRowGroup is the parent of the table row,
  * reference counting should not be done on 
  * this variable when setting the row.
  * see /ns/raptor/doc/MemoryModel.html
  **/
  virtual void SetRowGroup (nsTableRowGroup * aRowGroup);


  virtual PRInt32 GetRowIndex ();

  virtual void SetRowIndex (int aRowIndex);

  virtual PRInt32 GetMaxColumns();

  virtual void ResetCellMap ();

  /* ----------- nsTableContent overrides ----------- */

  virtual PRBool AppendChild (nsIContent * aContent);

  virtual PRBool InsertChildAt (nsIContent * aContent, int aIndex);

  virtual PRBool ReplaceChildAt (nsIContent * aContent, int aIndex);

  virtual PRBool RemoveChildAt (int aIndex);


};

#endif



