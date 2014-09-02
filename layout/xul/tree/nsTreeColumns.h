/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeColumns_h__
#define nsTreeColumns_h__

#include "nsITreeColumns.h"
#include "nsITreeBoxObject.h"
#include "mozilla/Attributes.h"
#include "nsCoord.h"
#include "nsCycleCollectionParticipant.h"
#include "nsAutoPtr.h"
#include "nsWrapperCache.h"
#include "nsString.h"

class nsTreeBodyFrame;
class nsTreeColumns;
class nsIFrame;
class nsIContent;
struct nsRect;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

#define NS_TREECOLUMN_IMPL_CID                       \
{ /* 02cd1963-4b5d-4a6c-9223-814d3ade93a3 */         \
    0x02cd1963,                                      \
    0x4b5d,                                          \
    0x4a6c,                                          \
    {0x92, 0x23, 0x81, 0x4d, 0x3a, 0xde, 0x93, 0xa3} \
}

// This class is our column info.  We use it to iterate our columns and to obtain
// information about each column.
class nsTreeColumn MOZ_FINAL : public nsITreeColumn {
public:
  nsTreeColumn(nsTreeColumns* aColumns, nsIContent* aContent);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TREECOLUMN_IMPL_CID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsTreeColumn)
  NS_DECL_NSITREECOLUMN

  friend class nsTreeBodyFrame;
  friend class nsTreeColumns;

protected:
  ~nsTreeColumn();
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

  int32_t GetIndex() { return mIndex; }

  bool IsPrimary() { return mIsPrimary; }
  bool IsCycler() { return mIsCycler; }
  bool IsEditable() { return mIsEditable; }
  bool IsSelectable() { return mIsSelectable; }
  bool Overflow() { return mOverflow; }

  int16_t GetType() { return mType; }

  int8_t GetCropStyle() { return mCropStyle; }
  int32_t GetTextAlignment() { return mTextAlignment; }

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

  int32_t mIndex;

  bool mIsPrimary;
  bool mIsCycler;
  bool mIsEditable;
  bool mIsSelectable;
  bool mOverflow;

  int16_t mType;

  int8_t mCropStyle;
  int8_t mTextAlignment;

  nsRefPtr<nsTreeColumn> mNext;
  nsTreeColumn* mPrevious;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsTreeColumn, NS_TREECOLUMN_IMPL_CID)

class nsTreeColumns MOZ_FINAL : public nsITreeColumns
                              , public nsWrapperCache
{
private:
  ~nsTreeColumns();

public:
  explicit nsTreeColumns(nsTreeBodyFrame* aTree);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsTreeColumns)
  NS_DECL_NSITREECOLUMNS

  nsIContent* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  nsITreeBoxObject* GetTree() const;
  uint32_t Count();
  uint32_t Length()
  {
    return Count();
  }

  nsTreeColumn* GetFirstColumn() { EnsureColumns(); return mFirstColumn; }
  nsTreeColumn* GetLastColumn();

  nsTreeColumn* GetPrimaryColumn();
  nsTreeColumn* GetSortedColumn();
  nsTreeColumn* GetKeyColumn();

  nsTreeColumn* GetColumnFor(mozilla::dom::Element* aElement);

  nsTreeColumn* IndexedGetter(uint32_t aIndex, bool& aFound);
  nsTreeColumn* GetColumnAt(uint32_t aIndex);
  nsTreeColumn* NamedGetter(const nsAString& aId, bool& aFound);
  bool NameIsEnumerable(const nsAString& aName);
  nsTreeColumn* GetNamedColumn(const nsAString& aId);
  void GetSupportedNames(unsigned, nsTArray<nsString>& aNames);

  // Uses XPCOM InvalidateColumns().
  // Uses XPCOM RestoreNaturalOrder().

  friend class nsTreeBodyFrame;
protected:
  void SetTree(nsTreeBodyFrame* aTree) { mTree = aTree; }

  // Builds our cache of column info.
  void EnsureColumns();

private:
  nsTreeBodyFrame* mTree;

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
