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

#include "nsCoord.h"
#include "nscore.h"
#include "nsTableContent.h"
#include "nsTableRow.h"

/**
 * nsTableCell is the content object that represents table cells 
 * (HTML tags TD and TH). This class cannot be reused
 * outside of an nsTableRow.  It assumes that its parent is an nsTableRow, and 
 * it has a single nsBodyPart child.
 * 
 * @see nsTablePart
 * @see nsTableRow
 * @see nsBodyPart
 *
 * @author  sclark
 */
class nsTableCell : public nsTableContent
{

private:

  /** parent pointer */
  nsTableRow * mRow;

  /** the number of rows spanned by this cell */
  int          mRowSpan;
    
  /** the number of columns spanned by this cell */
  int          mColSpan;

  /** the starting column for this cell */
  int          mColIndex;

public:

  /** constructor
    * @param aImplicit  PR_TRUE if there is no actual input tag corresponding to
    *                   this cell.
    */
  nsTableCell (PRBool aImplicit);

  /** constructor
    * @param aTag  the HTML tag causing this cell to get constructed.
    */
  nsTableCell (nsIAtom* aTag);

  /** constructor
    * @param aTag     the HTML tag causing this caption to get constructed.
    * @param aRowSpan the number of rows spanned
    * @param aColSpan the number of columns spanned
    */
  nsTableCell (nsIAtom* aTag, int aRowSpan, int aColSpan);

  /** destructor, not responsible for any memory destruction itself */
  virtual ~nsTableCell();

    // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();


  /** returns nsITableContent::kTableCellType */
  virtual int GetType();

  /** @see nsIHTMLContent::CreateFrame */
  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);

 

  /** @return the number of rows spanned by this cell.  Always >= 1 */
  virtual int GetRowSpan ();

  /** set the number of rows spanned.  Must be >= 1 */
  virtual void SetRowSpan (int aRowSpan);

  /** @return the number of columns spanned by this cell.  Always >= 1 */
  virtual int GetColSpan ();

  /** set the number of columns spanned.  Must be >= 1 */
  virtual void SetColSpan (int aColSpan);

  /** return a pointer to this cell's parent, the row containing the cell */
  virtual nsTableRow * GetRow ();

  /** set this cell's parent.
    * Note: Since mRow is the parent of the table cell,
    * reference counting should not be done on 
    * this variable when setting the row.
    * see /ns/raptor/doc/MemoryModel.html
    */
  virtual void SetRow (nsTableRow * aRow);

  /** @return the starting column for this cell.  Always >= 1 */
  virtual int GetColIndex ();

  /** set the starting column for this cell.  Always >= 1 */
  virtual void SetColIndex (int aColIndex);

  virtual void ResetCellMap ();

protected:
  virtual nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                          nsHTMLValue& aValue,
                                          nsString& aResult) const;
};

#endif
