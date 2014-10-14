/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TreeBoxObject_h
#define mozilla_dom_TreeBoxObject_h

#include "mozilla/dom/BoxObject.h"
#include "nsITreeView.h"
#include "nsITreeBoxObject.h"

class nsTreeBodyFrame;
class nsTreeColumns;

namespace mozilla {
namespace dom {

struct TreeCellInfo;
class DOMRect;

class TreeBoxObject MOZ_FINAL : public BoxObject,
                                public nsITreeBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TreeBoxObject, BoxObject)
  NS_DECL_NSITREEBOXOBJECT

  TreeBoxObject();

  nsTreeBodyFrame* GetTreeBodyFrame(bool aFlushLayout = false);
  nsTreeBodyFrame* GetCachedTreeBodyFrame() { return mTreeBody; }

  //NS_PIBOXOBJECT interfaces
  virtual void Clear() MOZ_OVERRIDE;
  virtual void ClearCachedValues() MOZ_OVERRIDE;

  // WebIDL
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  already_AddRefed<nsTreeColumns> GetColumns();

  already_AddRefed<nsITreeView> GetView();

  void SetView(nsITreeView* arg, ErrorResult& aRv);

  bool Focused();

  already_AddRefed<Element> GetTreeBody();

  int32_t RowHeight();

  int32_t RowWidth();

  int32_t HorizontalPosition();

  already_AddRefed<nsIScriptableRegion> SelectionRegion();

  int32_t GetFirstVisibleRow();

  int32_t GetLastVisibleRow();

  int32_t GetPageLength();

  int32_t GetRowAt(int32_t x, int32_t y);

  void GetCellAt(int32_t x, int32_t y, TreeCellInfo& aRetVal, ErrorResult& aRv);

  already_AddRefed<DOMRect> GetCoordsForCellItem(int32_t row,
                                                 nsITreeColumn* col,
                                                 const nsAString& element,
                                                 ErrorResult& aRv);

  bool IsCellCropped(int32_t row, nsITreeColumn* col, ErrorResult& aRv);

  // Deprecated APIs from old IDL
  void GetCellAt(JSContext* cx,
                 int32_t x, int32_t y,
                 JS::Handle<JSObject*> rowOut,
                 JS::Handle<JSObject*> colOut,
                 JS::Handle<JSObject*> childEltOut,
                 ErrorResult& aRv);

  void GetCoordsForCellItem(JSContext* cx,
                            int32_t row,
                            nsITreeColumn* col,
                            const nsAString& element,
                            JS::Handle<JSObject*> xOut,
                            JS::Handle<JSObject*> yOut,
                            JS::Handle<JSObject*> widthOut,
                            JS::Handle<JSObject*> heightOut,
                            ErrorResult& aRv);

  // Same signature (except for nsresult return type) as the XPIDL impls
  // void Invalidate();
  // void BeginUpdateBatch();
  // void EndUpdateBatch();
  // void ClearStyleAndImageCaches();
  // void SetFocused(bool arg);
  // void EnsureRowIsVisible(int32_t index);
  // void EnsureCellIsVisible(int32_t row, nsITreeColumn* col);
  // void ScrollToRow(int32_t index);
  // void ScrollByLines(int32_t numLines);
  // void ScrollByPages(int32_t numPages);
  // void ScrollToCell(int32_t row, nsITreeColumn* col);
  // void ScrollToColumn(nsITreeColumn* col);
  // void ScrollToHorizontalPosition(int32_t horizontalPosition);
  // void InvalidateColumn(nsITreeColumn* col);
  // void InvalidateRow(int32_t index);
  // void InvalidateCell(int32_t row, nsITreeColumn* col);
  // void InvalidateRange(int32_t startIndex, int32_t endIndex);
  // void InvalidateColumnRange(int32_t startIndex, int32_t endIndex, nsITreeColumn* col);
  // void RowCountChanged(int32_t index, int32_t count);

protected:
  nsTreeBodyFrame* mTreeBody;
  nsCOMPtr<nsITreeView> mView;

private:
  ~TreeBoxObject();
};

} // namespace dom
} // namespace mozilla

#endif
