/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XULTreeElement_h__
#define XULTreeElement_h__

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsString.h"
#include "nsXULElement.h"
#include "nsITreeView.h"

class nsTreeBodyFrame;
class nsTreeColumn;
class nsTreeColumns;

namespace mozilla {
namespace dom {

struct TreeCellInfo;
class DOMRect;
enum class CallerType : uint32_t;

class XULTreeElement final : public nsXULElement {
 public:
  explicit XULTreeElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : nsXULElement(std::move(aNodeInfo)),
        mCachedFirstVisibleRow(0),
        mTreeBody(nullptr) {}

  NS_IMPL_FROMNODE_WITH_TAG(XULTreeElement, kNameSpaceID_XUL, tree)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XULTreeElement, nsXULElement)

  nsTreeBodyFrame* GetTreeBodyFrame(FlushType = FlushType::Frames);
  nsTreeBodyFrame* GetCachedTreeBodyFrame() { return mTreeBody; }

  already_AddRefed<nsTreeColumns> GetColumns(FlushType = FlushType::Frames);

  already_AddRefed<nsITreeView> GetView(CallerType /* unused */) {
    return GetView();
  }
  already_AddRefed<nsITreeView> GetView();

  void SetView(nsITreeView* arg, CallerType aCallerType, ErrorResult& aRv);

  bool Focused();

  already_AddRefed<Element> GetTreeBody();

  int32_t RowHeight();

  int32_t RowWidth();

  int32_t HorizontalPosition();

  void EnsureCellIsVisible(int32_t row, nsTreeColumn* col, ErrorResult& aRv);

  void ScrollToRow(int32_t aRow);

  void ScrollByLines(int32_t aNumLines);

  void ScrollByPages(int32_t aNumPages);

  int32_t GetFirstVisibleRow();

  int32_t GetLastVisibleRow();

  int32_t GetPageLength();

  int32_t GetRowAt(int32_t x, int32_t y);

  void GetCellAt(int32_t x, int32_t y, TreeCellInfo& aRetVal, ErrorResult& aRv);

  nsIntRect GetCoordsForCellItem(int32_t aRow, nsTreeColumn* aCol,
                                 const nsAString& aElement, nsresult& rv);
  already_AddRefed<DOMRect> GetCoordsForCellItem(int32_t row, nsTreeColumn& col,
                                                 const nsAString& element,
                                                 ErrorResult& aRv);

  bool IsCellCropped(int32_t row, nsTreeColumn* col, ErrorResult& aRv);

  void RemoveImageCacheEntry(int32_t row, nsTreeColumn& col, ErrorResult& aRv);

  void SetFocused(bool aFocused);
  void EnsureRowIsVisible(int32_t index);
  void Invalidate(void);
  void InvalidateColumn(nsTreeColumn* col);
  void InvalidateRow(int32_t index);
  void InvalidateCell(int32_t row, nsTreeColumn* col);
  void InvalidateRange(int32_t startIndex, int32_t endIndex);
  void RowCountChanged(int32_t index, int32_t count);
  void BeginUpdateBatch(void);
  void EndUpdateBatch(void);
  void ClearStyleAndImageCaches(void);

  virtual void UnbindFromTree(bool aNullParent) override;
  virtual void DestroyContent() override;

  void BodyDestroyed(int32_t aFirstVisibleRow) {
    mTreeBody = nullptr;
    mCachedFirstVisibleRow = aFirstVisibleRow;
  }

  int32_t GetCachedTopVisibleRow() { return mCachedFirstVisibleRow; }

 protected:
  int32_t mCachedFirstVisibleRow;

  nsTreeBodyFrame* mTreeBody;
  nsCOMPtr<nsITreeView> mView;

  virtual ~XULTreeElement() = default;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace dom
}  // namespace mozilla

#endif
