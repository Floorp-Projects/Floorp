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
#ifndef nsTableCell_h__
#define nsTableCell_h__

#include "nscore.h"
#include "nsTableContent.h"
#include "nsTableRow.h"

/**
 * nsTableCell
 * datastructure to maintain information about a single table column
 *
 * @author  sclark
 */
class nsTableCell : public nsTableContent
{

private:

  nsTableRow * mRow;
  int          mRowSpan, mColSpan;
  int          mColIndex;

public:

  nsTableCell (PRBool aImplicit);

  nsTableCell (nsIAtom* aTag);

  nsTableCell (nsIAtom* aTag, int aRowSpan, int aColSpan);

  virtual ~nsTableCell();

    // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();



  virtual int GetType();

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);

  virtual int GetRowSpan ();

  virtual void SetRowSpan (int aRowSpan);

  virtual int GetColSpan ();

  virtual void SetColSpan (int aColSpan);

  virtual nsTableRow * GetRow ();

  /** 
  * Since mRow is the parent of the table cell,
  * reference counting should not be done on 
  * this variable when setting the row.
  * see /ns/raptor/doc/MemoryModel.html
  **/
  virtual void SetRow (nsTableRow * aRow);

  virtual int GetColIndex ();

  virtual void SetColIndex (int aColIndex);

  virtual void ResetCellMap ();

};

#endif
