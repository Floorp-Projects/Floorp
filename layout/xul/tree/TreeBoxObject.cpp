/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TreeBoxObject.h"
#include "nsCOMPtr.h"
#include "nsXULElement.h"
#include "nsIScriptableRegion.h"
#include "nsTreeContentView.h"
#include "nsITreeSelection.h"
#include "ChildIterator.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/dom/TreeBoxObjectBinding.h"
#include "nsITreeColumns.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ToJSValue.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(TreeBoxObject, BoxObject,
                                   mView)

NS_IMPL_ADDREF_INHERITED(TreeBoxObject, BoxObject)
NS_IMPL_RELEASE_INHERITED(TreeBoxObject, BoxObject)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TreeBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsITreeBoxObject)
NS_INTERFACE_MAP_END_INHERITING(BoxObject)

void
TreeBoxObject::Clear()
{
  ClearCachedValues();

  // Drop the view's ref to us.
  if (mView) {
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel)
      sel->SetTree(nullptr);
    mView->SetTree(nullptr); // Break the circular ref between the view and us.
  }
  mView = nullptr;

  BoxObject::Clear();
}


TreeBoxObject::TreeBoxObject()
  : mTreeBody(nullptr)
{
}

TreeBoxObject::~TreeBoxObject()
{
}

static nsIContent* FindBodyElement(nsIContent* aParent)
{
  mozilla::dom::FlattenedChildIterator iter(aParent);
  for (nsIContent* content = iter.GetNextChild(); content; content = iter.GetNextChild()) {
    mozilla::dom::NodeInfo *ni = content->NodeInfo();
    if (ni->Equals(nsGkAtoms::treechildren, kNameSpaceID_XUL)) {
      return content;
    } else if (ni->Equals(nsGkAtoms::tree, kNameSpaceID_XUL)) {
      // There are nesting tree elements. Only the innermost should
      // find the treechilren.
      return nullptr;
    } else if (content->IsElement() &&
               !ni->Equals(nsGkAtoms::_template, kNameSpaceID_XUL)) {
      nsIContent* result = FindBodyElement(content);
      if (result)
        return result;
    }
  }

  return nullptr;
}

nsTreeBodyFrame*
TreeBoxObject::GetTreeBodyFrame(bool aFlushLayout)
{
  // Make sure our frames are up to date, and layout as needed.  We
  // have to do this before checking for our cached mTreeBody, since
  // it might go away on style flush, and in any case if aFlushLayout
  // is true we need to make sure to flush no matter what.
  // XXXbz except that flushing style when we were not asked to flush
  // layout here breaks things.  See bug 585123.
  nsIFrame* frame = nullptr;
  if (aFlushLayout) {
    frame = GetFrame(aFlushLayout);
    if (!frame)
      return nullptr;
  }

  if (mTreeBody) {
    // Have one cached already.
    return mTreeBody;
  }

  if (!aFlushLayout) {
    frame = GetFrame(aFlushLayout);
    if (!frame)
      return nullptr;
  }

  // Iterate over our content model children looking for the body.
  nsCOMPtr<nsIContent> content = FindBodyElement(frame->GetContent());
  if (!content)
    return nullptr;

  frame = content->GetPrimaryFrame();
  if (!frame)
     return nullptr;

  // Make sure that the treebodyframe has a pointer to |this|.
  nsTreeBodyFrame *treeBody = do_QueryFrame(frame);
  NS_ENSURE_TRUE(treeBody && treeBody->GetTreeBoxObject() == this, nullptr);

  mTreeBody = treeBody;
  return mTreeBody;
}

NS_IMETHODIMP
TreeBoxObject::GetView(nsITreeView * *aView)
{
  if (!mTreeBody) {
    if (!GetTreeBodyFrame()) {
      // Don't return an uninitialised view
      *aView = nullptr;
      return NS_OK;
    }

    if (mView)
      // Our new frame needs to initialise itself
      return mTreeBody->GetView(aView);
  }
  if (!mView) {
    RefPtr<nsXULElement> xulele = nsXULElement::FromContentOrNull(mContent);
    if (xulele) {
      // No tree builder, create a tree content view.
      nsresult rv = NS_NewTreeContentView(getter_AddRefs(mView));
      NS_ENSURE_SUCCESS(rv, rv);

      // Initialise the frame and view
      mTreeBody->SetView(mView);
    }
  }
  NS_IF_ADDREF(*aView = mView);
  return NS_OK;
}

already_AddRefed<nsITreeView>
TreeBoxObject::GetView(CallerType /* unused */)
{
  nsCOMPtr<nsITreeView> view;
  GetView(getter_AddRefs(view));
  return view.forget();
}

NS_IMETHODIMP
TreeBoxObject::SetView(nsITreeView* aView)
{
  ErrorResult rv;
  SetView(aView, CallerType::System, rv);
  return rv.StealNSResult();
}

void
TreeBoxObject::SetView(nsITreeView* aView, CallerType aCallerType,
                       ErrorResult& aRv)
{
  if (aCallerType != CallerType::System) {
    // Don't trust views coming from random places.
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  mView = aView;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    body->SetView(aView);
}

bool TreeBoxObject::Focused()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->GetFocused();
  return false;
}

NS_IMETHODIMP TreeBoxObject::GetFocused(bool* aFocused)
{
  *aFocused = Focused();
  return NS_OK;
}

NS_IMETHODIMP TreeBoxObject::SetFocused(bool aFocused)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->SetFocused(aFocused);
  return NS_OK;
}

NS_IMETHODIMP TreeBoxObject::GetTreeBody(nsIDOMElement** aElement)
{
  *aElement = nullptr;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->GetTreeBody(aElement);
  return NS_OK;
}

already_AddRefed<Element>
TreeBoxObject::GetTreeBody()
{
  nsCOMPtr<nsIDOMElement> el;
  GetTreeBody(getter_AddRefs(el));
  nsCOMPtr<Element> ret(do_QueryInterface(el));
  return ret.forget();
}

already_AddRefed<nsTreeColumns>
TreeBoxObject::GetColumns()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->Columns();
  return nullptr;
}

NS_IMETHODIMP TreeBoxObject::GetColumns(nsITreeColumns** aColumns)
{
  *aColumns = GetColumns().take();
  return NS_OK;
}

int32_t TreeBoxObject::RowHeight()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->RowHeight();
  return 0;
}

int32_t TreeBoxObject::RowWidth()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->RowWidth();
  return 0;
}

NS_IMETHODIMP TreeBoxObject::GetRowHeight(int32_t* aRowHeight)
{
  *aRowHeight = RowHeight();
  return NS_OK;
}

NS_IMETHODIMP TreeBoxObject::GetRowWidth(int32_t *aRowWidth)
{
  *aRowWidth = RowWidth();
  return NS_OK;
}

int32_t TreeBoxObject::GetFirstVisibleRow()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->FirstVisibleRow();
  return 0;
}

NS_IMETHODIMP TreeBoxObject::GetFirstVisibleRow(int32_t *aFirstVisibleRow)
{
  *aFirstVisibleRow = GetFirstVisibleRow();
  return NS_OK;
}

int32_t TreeBoxObject::GetLastVisibleRow()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->LastVisibleRow();
  return 0;
}

NS_IMETHODIMP TreeBoxObject::GetLastVisibleRow(int32_t *aLastVisibleRow)
{
  *aLastVisibleRow = GetLastVisibleRow();
  return NS_OK;
}

int32_t TreeBoxObject::HorizontalPosition()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->GetHorizontalPosition();
  return 0;
}

NS_IMETHODIMP TreeBoxObject::GetHorizontalPosition(int32_t *aHorizontalPosition)
{
  *aHorizontalPosition = HorizontalPosition();
  return NS_OK;
}

int32_t TreeBoxObject::GetPageLength()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->PageLength();
  return 0;
}

NS_IMETHODIMP TreeBoxObject::GetPageLength(int32_t *aPageLength)
{
  *aPageLength = GetPageLength();
  return NS_OK;
}

NS_IMETHODIMP TreeBoxObject::GetSelectionRegion(nsIScriptableRegion **aRegion)
{
  *aRegion = nullptr;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->GetSelectionRegion(aRegion);
  return NS_OK;
}

already_AddRefed<nsIScriptableRegion>
TreeBoxObject::SelectionRegion()
{
  nsCOMPtr<nsIScriptableRegion> region;
  GetSelectionRegion(getter_AddRefs(region));
  return region.forget();
}

NS_IMETHODIMP
TreeBoxObject::EnsureRowIsVisible(int32_t aRow)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->EnsureRowIsVisible(aRow);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::EnsureCellIsVisible(int32_t aRow, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->EnsureCellIsVisible(aRow, aCol);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::ScrollToRow(int32_t aRow)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame(true);
  if (body)
    return body->ScrollToRow(aRow);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::ScrollByLines(int32_t aNumLines)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->ScrollByLines(aNumLines);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::ScrollByPages(int32_t aNumPages)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->ScrollByPages(aNumPages);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::ScrollToCell(int32_t aRow, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->ScrollToCell(aRow, aCol);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::ScrollToColumn(nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->ScrollToColumn(aCol);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::ScrollToHorizontalPosition(int32_t aHorizontalPosition)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->ScrollToHorizontalPosition(aHorizontalPosition);
  return NS_OK;
}

NS_IMETHODIMP TreeBoxObject::Invalidate()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->Invalidate();
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::InvalidateColumn(nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->InvalidateColumn(aCol);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::InvalidateRow(int32_t aIndex)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->InvalidateRow(aIndex);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::InvalidateCell(int32_t aRow, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->InvalidateCell(aRow, aCol);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::InvalidateRange(int32_t aStart, int32_t aEnd)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->InvalidateRange(aStart, aEnd);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::InvalidateColumnRange(int32_t aStart, int32_t aEnd, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->InvalidateColumnRange(aStart, aEnd, aCol);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::GetRowAt(int32_t x, int32_t y, int32_t *aRow)
{
  *aRow = 0;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->GetRowAt(x, y, aRow);
  return NS_OK;
}

int32_t
TreeBoxObject::GetRowAt(int32_t x, int32_t y)
{
  int32_t row;
  GetRowAt(x, y, &row);
  return row;
}

NS_IMETHODIMP
TreeBoxObject::GetCellAt(int32_t aX, int32_t aY, int32_t *aRow,
                         nsITreeColumn** aCol, nsAString& aChildElt)
{
  *aRow = 0;
  *aCol = nullptr;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) {
    nsAutoCString element;
    nsresult retval = body->GetCellAt(aX, aY, aRow, aCol, element);
    CopyUTF8toUTF16(element, aChildElt);
    return retval;
  }
  return NS_OK;
}

void
TreeBoxObject::GetCellAt(int32_t x, int32_t y, TreeCellInfo& aRetVal, ErrorResult& aRv)
{
  nsCOMPtr<nsITreeColumn> col;
  GetCellAt(x, y, &aRetVal.mRow, getter_AddRefs(col), aRetVal.mChildElt);
  aRetVal.mCol = col.forget().downcast<nsTreeColumn>();
}

void
TreeBoxObject::GetCellAt(JSContext* cx,
                         int32_t x, int32_t y,
                         JS::Handle<JSObject*> rowOut,
                         JS::Handle<JSObject*> colOut,
                         JS::Handle<JSObject*> childEltOut,
                         ErrorResult& aRv)
{
  int32_t row;
  nsITreeColumn* col;
  nsAutoString childElt;
  GetCellAt(x, y, &row, &col, childElt);

  JS::Rooted<JS::Value> v(cx);

  if (!ToJSValue(cx, row, &v) ||
      !JS_SetProperty(cx, rowOut, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
  if (!dom::WrapObject(cx, col, &v) ||
      !JS_SetProperty(cx, colOut, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
  if (!ToJSValue(cx, childElt, &v) ||
      !JS_SetProperty(cx, childEltOut, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
}

NS_IMETHODIMP
TreeBoxObject::GetCoordsForCellItem(int32_t aRow, nsITreeColumn* aCol, const nsAString& aElement,
                                      int32_t *aX, int32_t *aY, int32_t *aWidth, int32_t *aHeight)
{
  *aX = *aY = *aWidth = *aHeight = 0;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  NS_ConvertUTF16toUTF8 element(aElement);
  if (body)
    return body->GetCoordsForCellItem(aRow, aCol, element, aX, aY, aWidth, aHeight);
  return NS_OK;
}

already_AddRefed<DOMRect>
TreeBoxObject::GetCoordsForCellItem(int32_t row, nsTreeColumn& col, const nsAString& element, ErrorResult& aRv)
{
  int32_t x, y, w, h;
  GetCoordsForCellItem(row, &col, element, &x, &y, &w, &h);
  RefPtr<DOMRect> rect = new DOMRect(mContent, x, y, w, h);
  return rect.forget();
}

void
TreeBoxObject::GetCoordsForCellItem(JSContext* cx,
                                    int32_t row,
                                    nsTreeColumn& col,
                                    const nsAString& element,
                                    JS::Handle<JSObject*> xOut,
                                    JS::Handle<JSObject*> yOut,
                                    JS::Handle<JSObject*> widthOut,
                                    JS::Handle<JSObject*> heightOut,
                                    ErrorResult& aRv)
{
  int32_t x, y, w, h;
  GetCoordsForCellItem(row, &col, element, &x, &y, &w, &h);
  JS::Rooted<JS::Value> v(cx, JS::Int32Value(x));
  if (!JS_SetProperty(cx, xOut, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
  v.setInt32(y);
  if (!JS_SetProperty(cx, yOut, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
  v.setInt32(w);
  if (!JS_SetProperty(cx, widthOut, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
  v.setInt32(h);
  if (!JS_SetProperty(cx, heightOut, "value", v)) {
    aRv.Throw(NS_ERROR_XPC_CANT_SET_OUT_VAL);
    return;
  }
}

NS_IMETHODIMP
TreeBoxObject::IsCellCropped(int32_t aRow, nsITreeColumn* aCol, bool *aIsCropped)
{
  *aIsCropped = false;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->IsCellCropped(aRow, aCol, aIsCropped);
  return NS_OK;
}

bool
TreeBoxObject::IsCellCropped(int32_t row, nsITreeColumn* col, ErrorResult& aRv)
{
  bool ret;
  aRv = IsCellCropped(row, col, &ret);
  return ret;
}

NS_IMETHODIMP
TreeBoxObject::RowCountChanged(int32_t aIndex, int32_t aDelta)
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->RowCountChanged(aIndex, aDelta);
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::BeginUpdateBatch()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->BeginUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::EndUpdateBatch()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->EndUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::ClearStyleAndImageCaches()
{
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body)
    return body->ClearStyleAndImageCaches();
  return NS_OK;
}

NS_IMETHODIMP
TreeBoxObject::RemoveImageCacheEntry(int32_t aRowIndex, nsITreeColumn* aCol)
{
  NS_ENSURE_ARG(aCol);
  NS_ENSURE_TRUE(aRowIndex >= 0, NS_ERROR_INVALID_ARG);
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) {
    return body->RemoveImageCacheEntry(aRowIndex, aCol);
  }
  return NS_OK;
}

void
TreeBoxObject::RemoveImageCacheEntry(int32_t row, nsITreeColumn& col, ErrorResult& aRv)
{
  aRv = RemoveImageCacheEntry(row, &col);
}

void
TreeBoxObject::ClearCachedValues()
{
  mTreeBody = nullptr;
}

JSObject*
TreeBoxObject::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TreeBoxObjectBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

// Creation Routine ///////////////////////////////////////////////////////////////////////

using namespace mozilla::dom;

nsresult
NS_NewTreeBoxObject(nsIBoxObject** aResult)
{
  NS_ADDREF(*aResult = new TreeBoxObject());
  return NS_OK;
}
