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
class nsTableCellFrame;
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
  * @see nsTableFrame::BuildCellMap
  * @see nsTableFrame::GrowCellMap
  * @see nsTableFrame::BuildCellIntoMap
  * 
  */
class CellData
{
public:
  nsTableCellFrame *mCell;
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
  static const char *kCaptionTagString;
  static const char *kRowGroupBodyTagString;
  static const char *kRowGroupHeadTagString;
  static const char *kRowGroupFootTagString;
  static const char *kRowTagString;
  static const char *kColGroupTagString;
  static const char *kColTagString;
  static const char *kDataCellTagString;
  static const char *kHeaderCellTagString;

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


/* overrides from nsHTMLContainer */

  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  NS_IMETHOD AppendChild(nsIContent* aKid, PRBool aNotify);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);


  static void GetTableBorder(nsIHTMLContent* aContent,
                             nsIStyleContext* aContext,
                             nsIPresContext* aPresContext,
                             PRBool aForCell);

protected:
  /** destructor 
    * deletes mCellMap, if allocated.
    */
  virtual ~nsTablePart();

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

private:

  static nsIAtom *kDefaultTag;
};

#endif    // nsTablePart_h___
