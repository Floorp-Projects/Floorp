/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeColumns_h__
#define nsTreeColumns_h__

#include "nsITreeColumns.h"
#include "nsITreeBoxObject.h"
#include "nsIContent.h"
#include "nsIFrame.h"

class nsTreeBodyFrame;
class nsTreeColumns;

#define NS_TREECOLUMN_IMPL_CID                       \
{ /* 02cd1963-4b5d-4a6c-9223-814d3ade93a3 */         \
    0x02cd1963,                                      \
    0x4b5d,                                          \
    0x4a6c,                                          \
    {0x92, 0x23, 0x81, 0x4d, 0x3a, 0xde, 0x93, 0xa3} \
}

// This class is our column info.  We use it to iterate our columns and to obtain
// information about each column.
class nsTreeColumn : public nsITreeColumn {
public:
  nsTreeColumn(nsTreeColumns* aColumns, nsIContent* aContent);
  ~nsTreeColumn();

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TREECOLUMN_IMPL_CID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsTreeColumn)
  NS_DECL_NSITREECOLUMN

  friend class nsTreeBodyFrame;
  friend class nsTreeColumns;

protected:
  nsIFrame* GetFrame();
  nsIFrame* GetFrame(nsTreeBodyFrame* aBodyFrame);
  // Don't call this if GetWidthInTwips or GetRect fails
  bool IsLastVisible(nsTreeBodyFrame* aBodyFrame);

  /**
   * Returns a rect with x and width taken from the frame's rect and specified
   * y and height. May fail in case there's no frame for the column.
   */
  nsresult GetRect(nsTreeBodyFrame* aBodyFrame, nscoord aY, nscoord aHeight,
                   nsRect* aResult);

  nsresult GetXInTwips(nsTreeBodyFrame* aBodyFrame, nscoord* aResult);
  nsresult GetWidthInTwips(nsTreeBodyFrame* aBodyFrame, nscoord* aResult);

  void SetColumns(nsTreeColumns* aColumns) { mColumns = aColumns; }

  const nsAString& GetId() { return mId; }
  nsIAtom* GetAtom() { return mAtom; }

  PRInt32 GetIndex() { return mIndex; }

  bool IsPrimary() { return mIsPrimary; }
  bool IsCycler() { return mIsCycler; }
  bool IsEditable() { return mIsEditable; }
  bool IsSelectable() { return mIsSelectable; }
  bool Overflow() { return mOverflow; }

  PRInt16 GetType() { return mType; }

  PRInt8 GetCropStyle() { return mCropStyle; }
  PRInt32 GetTextAlignment() { return mTextAlignment; }

  nsTreeColumn* GetNext() { return mNext; }
  nsTreeColumn* GetPrevious() { return mPrevious; }
  void SetNext(nsTreeColumn* aNext) {
    NS_ASSERTION(!mNext, "already have a next sibling");
    mNext = aNext;
  }
  void SetPrevious(nsTreeColumn* aPrevious) { mPrevious = aPrevious; }

private:
  /**
   * Non-null nsIContent for the associated <treecol> element.
   */
  nsCOMPtr<nsIContent> mContent;

  nsTreeColumns* mColumns;

  nsString mId;
  nsCOMPtr<nsIAtom> mAtom;

  PRInt32 mIndex;

  bool mIsPrimary;
  bool mIsCycler;
  bool mIsEditable;
  bool mIsSelectable;
  bool mOverflow;

  PRInt16 mType;

  PRInt8 mCropStyle;
  PRInt8 mTextAlignment;

  nsRefPtr<nsTreeColumn> mNext;
  nsTreeColumn* mPrevious;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsTreeColumn, NS_TREECOLUMN_IMPL_CID)

class nsTreeColumns : public nsITreeColumns {
public:
  nsTreeColumns(nsITreeBoxObject* aTree);
  ~nsTreeColumns();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITREECOLUMNS

  nsITreeColumn* GetColumnAt(PRInt32 aIndex);
  nsITreeColumn* GetNamedColumn(const nsAString& aId);

  static nsTreeColumns* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsITreeColumns> columns_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsITreeColumns pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(columns_qi == static_cast<nsITreeColumns*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsTreeColumns*>(aSupports);
  }

  friend class nsTreeBodyFrame;
protected:
  void SetTree(nsITreeBoxObject* aTree) { mTree = aTree; }

  // Builds our cache of column info.
  void EnsureColumns();

  nsTreeColumn* GetFirstColumn() { EnsureColumns(); return mFirstColumn; }
  nsTreeColumn* GetPrimaryColumn();

private:
  nsITreeBoxObject* mTree;

  /**
   * The first column in the list of columns. All of the columns are supposed
   * to be "alive", i.e. have a frame. This is achieved by clearing the columns
   * list each time an nsTreeColFrame is destroyed.
   *
   * XXX this means that new nsTreeColumn objects are unnecessarily created
   *     for untouched columns.
   */
  nsTreeColumn* mFirstColumn;
};

#endif // nsTreeColumns_h__
