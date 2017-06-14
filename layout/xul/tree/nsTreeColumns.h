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
#include "nsQueryObject.h"
#include "nsWrapperCache.h"
#include "nsString.h"

class nsTreeBodyFrame;
class nsTreeColumns;
class nsIFrame;
class nsIContent;
struct nsRect;

namespace mozilla {
class ErrorResult;
namespace dom {
class Element;
class TreeBoxObject;
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
class nsTreeColumn final : public nsITreeColumn
                         , public nsWrapperCache
{
public:
  nsTreeColumn(nsTreeColumns* aColumns, nsIContent* aContent);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TREECOLUMN_IMPL_CID)

  static already_AddRefed<nsTreeColumn> From(nsITreeColumn* aColumn)
  {
    RefPtr<nsTreeColumn> col = do_QueryObject(aColumn);
    return col.forget();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsTreeColumn)
  NS_DECL_NSITREECOLUMN

  // WebIDL
  nsIContent* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  mozilla::dom::Element* GetElement(mozilla::ErrorResult& aRv);

  nsTreeColumns* GetColumns() const { return mColumns; }

  int32_t GetX(mozilla::ErrorResult& aRv);
  int32_t GetWidth(mozilla::ErrorResult& aRv);

  // GetId is fine
  int32_t Index() const { return mIndex; }

  bool Primary() const { return mIsPrimary; }
  bool Cycler() const { return mIsCycler; }
  bool Editable() const { return mIsEditable; }
  bool Selectable() const { return mIsSelectable; }
  int16_t Type() const { return mType; }

  nsTreeColumn* GetNext() const { return mNext; }
  nsTreeColumn* GetPrevious() const { return mPrevious; }

  void Invalidate(mozilla::ErrorResult& aRv);

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

public:
  nsIAtom* GetAtom() { return mAtom; }
  int32_t GetIndex() { return mIndex; }

protected:
  bool IsPrimary() { return mIsPrimary; }
  bool IsCycler() { return mIsCycler; }
  bool IsEditable() { return mIsEditable; }
  bool IsSelectable() { return mIsSelectable; }
  bool Overflow() { return mOverflow; }

  int16_t GetType() { return mType; }

  int8_t GetCropStyle() { return mCropStyle; }
  int32_t GetTextAlignment() { return mTextAlignment; }

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

  RefPtr<nsTreeColumn> mNext;
  nsTreeColumn* mPrevious;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsTreeColumn, NS_TREECOLUMN_IMPL_CID)

class nsTreeColumns final : public nsITreeColumns
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
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  mozilla::dom::TreeBoxObject* GetTree() const;
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
  nsTreeColumn* GetNamedColumn(const nsAString& aId);
  void GetSupportedNames(nsTArray<nsString>& aNames);

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
  RefPtr<nsTreeColumn> mFirstColumn;
};

#endif // nsTreeColumns_h__
