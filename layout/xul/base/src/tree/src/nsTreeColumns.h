/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Hyatt <hyatt@mozilla.org> (Original Author)
 *   Jan Varga <varga@ku.sk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  PRBool IsLastVisible(nsTreeBodyFrame* aBodyFrame);

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

  PRBool IsPrimary() { return mIsPrimary; }
  PRBool IsCycler() { return mIsCycler; }
  PRBool IsEditable() { return mIsEditable; }
  PRBool IsSelectable() { return mIsSelectable; }
  PRBool Overflow() { return mOverflow; }

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

  PRPackedBool mIsPrimary;
  PRPackedBool mIsCycler;
  PRPackedBool mIsEditable;
  PRPackedBool mIsSelectable;
  PRPackedBool mOverflow;

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
