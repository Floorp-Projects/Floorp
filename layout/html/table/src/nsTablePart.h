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

/**
  * if mCell is null then mRealCell will be the rowspan/colspan source
  * in addition, if fOverlap is non-null then it will point to the
  * other cell that overlaps this position
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
  * A Table can contain RowGroups, ColumnGroups, and Captions.
  */
class nsTablePart : public nsHTMLContainer {

public:

  static const char *nsTablePart::kCaptionTagString;
  static const char *nsTablePart::kRowGroupBodyTagString;
  static const char *nsTablePart::kRowGroupHeadTagString;
  static const char *nsTablePart::kRowGroupFootTagString;
  static const char *nsTablePart::kRowTagString;
  static const char *nsTablePart::kColGroupTagString;
  static const char *nsTablePart::kColTagString;
  static const char *nsTablePart::kDataCellTagString;
  static const char *nsTablePart::kHeaderCellTagString;

  nsTablePart(nsIAtom* aTag);
  nsTablePart(nsIAtom* aTag, int aColumnCount);

  // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  /* public Table methods */
  virtual void ResetCellMap ();
  virtual void ResetColumns ();
  virtual void EnsureColumns ();
  virtual int GetSpecifiedColumnCount ();
  virtual int GetRowCount();
  virtual int GetMaxColumns();

  /* overrides from nsHTMLContainer */
  virtual PRBool InsertChildAt(nsIContent* aKid, PRInt32 aIndex);
  virtual PRBool ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex);
  virtual PRBool AppendChild(nsIContent* aKid);
  virtual PRBool RemoveChildAt(PRInt32 aIndex);

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual void NotifyContentComplete();

protected:
  virtual ~nsTablePart();
  virtual int  GetEffectiveRowSpan(int aRowIndex, nsTableCell *aCell);
  virtual void BuildCellMap ();
  virtual void GrowCellMap(int aColCount);
  virtual void BuildCellIntoMap (nsTableCell *aCell, int aRowIndex, int aColIndex);
  virtual int  NextRowGroup (int aStartIndex);
  virtual void ReorderChildren();
  virtual PRBool AppendRowGroup(nsTableRowGroup *aContent);
  virtual PRBool AppendColGroup(nsTableColGroup *aContent);
  virtual PRBool AppendColumn(nsTableCol *aContent);
  virtual PRBool AppendCaption(nsTableCaption *aContent);

public:
  virtual void        DumpCellMap() const; 
  virtual nsCellMap*  GetCellMap() const;

private:

  int mColCount;
  int mSpecifiedColCount;
  nsCellMap* mCellMap;
  static nsIAtom *kDefaultTag;
};

#endif    // nsTablePart_h___
