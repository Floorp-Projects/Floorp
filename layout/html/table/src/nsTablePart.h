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
#ifndef nsTablePart_h___
#define nsTablePart_h___


// nsTablePart.h
#include "nsHTMLContainer.h"

// forward declarations
class nsTableCell;
class nsCellMap;
class nsTableRowGroup;
class nsTableColGroup;
class nsTableCol;
class nsTableCaption;

/** Data stored by nsCellMap to rationalize rowspan and colspan cells.
  * if mCell is null then mRealCell will be the rowspan/colspan source
  * in addition, if fOverlap is non-null then it will point to the
  * other cell that overlaps this position
  * @see nsCellMap
  * @see nsTablePart::BuildCellMap
  * @see nsTablePart::GrowCellMap
  * @see nsTablePart::BuildCellIntoMap
  * 
  */
class CellData
{
public:
  nsTableCell *mCell;
  CellData *mRealCell;
  CellData *mOverlap;

  CellData();

  ~CellData();
};

/**
  * Table Content Model.
  * We build a full rationalized content model, so the structure of a table
  * will always be regular and predictable.
  * A Table can contain RowGroups (THEAD, TBODY, TFOOT), ColumnGroups (COLGROUP), 
  * and Captions (CAPTION).  Any other table content (TH, TD, COL) belongs in
  * one of these containers, and if the container doesn't explicitly exist in 
  * the source, an implicit container will get created.
  *
  * @author  sclark
  */
class nsTablePart : public nsHTMLContainer {

public:

  /** known HTML tags */
  static const char *nsTablePart::kCaptionTagString;
  static const char *nsTablePart::kRowGroupBodyTagString;
  static const char *nsTablePart::kRowGroupHeadTagString;
  static const char *nsTablePart::kRowGroupFootTagString;
  static const char *nsTablePart::kRowTagString;
  static const char *nsTablePart::kColGroupTagString;
  static const char *nsTablePart::kColTagString;
  static const char *nsTablePart::kDataCellTagString;
  static const char *nsTablePart::kHeaderCellTagString;

  /** constructor 
    * @param aTag the HTML tag that caused this content node to be instantiated
    */
  nsTablePart(nsIAtom* aTag);

  /** constructor 
    * @param aTag         the HTML tag that caused this content node to be instantiated
    * @param aColumnCount the number of columns in this table
    */
  nsTablePart(nsIAtom* aTag, PRInt32 aColumnCount);

  // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);

/* public Table methods */

  /** ResetCellMap is called when the cell structure of the table is changed.
    * Call with caution, only when changing the structure of the table such as 
    * inserting or removing rows, changing the rowspan or colspan attribute of a cell, etc.
    */
  virtual void ResetCellMap ();

  /** ResetColumns is called when the column structure of the table is changed.
    * Call with caution, only when adding or removing columns, changing 
    * column attributes, changing the rowspan or colspan attribute of a cell, etc.
    */
  virtual void ResetColumns ();

  /** sum the columns represented by all nsTableColGroup objects. 
    * if the cell map says there are more columns than this, 
    * add extra implicit columns to the content tree.
    */ 
  virtual void EnsureColumns ();

  /** return the number of columns as specified by the input. 
    * has 2 side effects:<br>
    * calls SetStartColumnIndex on each nsTableColumn<br>
    * sets mSpecifiedColCount.<br>
    */
  virtual PRInt32 GetSpecifiedColumnCount ();

  /** returns the number of rows in this table.
    * if mCellMap has been created, it is asked for the number of rows.<br>
    * otherwise, the content is enumerated and the rows are counted.
    */
  virtual PRInt32 GetRowCount();

  /** returns the actual number of columns in this table.<br>
    * as a side effect, will call BuildCellMap to constuct mCellMap if needed.
    */
  virtual PRInt32 GetMaxColumns();


/* overrides from nsHTMLContainer */

  virtual PRBool InsertChildAt(nsIContent* aKid, PRInt32 aIndex);
  virtual PRBool ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex);
  virtual PRBool AppendChild(nsIContent* aKid);
  virtual PRBool RemoveChildAt(PRInt32 aIndex);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  /** called when the input stream knows that the input has been completely consumed.
    * this is a hook for future optimizations.
    */
  virtual void NotifyContentComplete();

  static void GetTableBorder(nsIHTMLContent* aContent,
                             nsIStyleContext* aContext,
                             nsIPresContext* aPresContext,
                             PRBool aForCell);

protected:
  /** destructor 
    * deletes mCellMap, if allocated.
    */
  virtual ~nsTablePart();

  /** return the row span of a cell, taking into account row span magic at the bottom
    * of a table.
    * @param aRowIndex  the first row that contains the cell
    * @param aCell      the content object representing the cell
    * @return  the row span, correcting for row spans that extend beyond the bottom
    *          of the table.
    */
  virtual PRInt32  GetEffectiveRowSpan(PRInt32 aRowIndex, nsTableCell *aCell);

  /** build as much of the CellMap as possible from the info we have so far 
    */
  virtual void BuildCellMap ();

  /** called whenever the number of columns changes, to increase the storage in mCellMap 
    */
  virtual void GrowCellMap(PRInt32 aColCount);

  /** called every time we discover we have a new cell to add to the table.
    * This could be because we got actual cell content, because of rowspan/colspan attributes, etc.
    * This method changes mCellMap as necessary to account for the new cell.
    *
    * @param aCell     the content object created for the cell
    * @param aRowIndex the row into which the cell is to be inserted
    * @param aColIndex the col into which the cell is to be inserted
    */
  virtual void BuildCellIntoMap (nsTableCell *aCell, PRInt32 aRowIndex, PRInt32 aColIndex);

  /** returns the index of the first child after aStartIndex that is a row group 
    */
  virtual PRInt32 NextRowGroup (PRInt32 aStartIndex);

  /** obsolete! */
  virtual void ReorderChildren();

  /** append aContent to my child list 
    * @return PR_TRUE on success, PR_FALSE if aContent could not be appended
    */
  virtual PRBool AppendRowGroup(nsTableRowGroup *aContent);

  /** append aContent to my child list 
    * @return PR_TRUE on success, PR_FALSE if aContent could not be appended
    */
  virtual PRBool AppendColGroup(nsTableColGroup *aContent);

  /** append aContent to my child list 
    * @return PR_TRUE on success, PR_FALSE if aContent could not be appended
    */
  virtual PRBool AppendColumn(nsTableCol *aContent);

  /** append aContent to my child list
    * @return PR_TRUE on success, PR_FALSE if aContent could not be appended
    */
  virtual PRBool AppendCaption(nsTableCaption *aContent);


public:
  virtual void        DumpCellMap() const; 
  virtual nsCellMap*  GetCellMap() const;

private:

  PRInt32 mColCount;
  PRInt32 mSpecifiedColCount;
  nsCellMap* mCellMap;
  static nsIAtom *kDefaultTag;
};

#endif    // nsTablePart_h___
