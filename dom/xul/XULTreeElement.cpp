/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsTreeContentView.h"
#include "nsITreeSelection.h"
#include "ChildIterator.h"
#include "nsError.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/XULTreeElement.h"
#include "mozilla/dom/XULTreeElementBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(XULTreeElement, nsXULElement)
NS_IMPL_CYCLE_COLLECTION_INHERITED(XULTreeElement, nsXULElement, mView)

JSObject* XULTreeElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return XULTreeElement_Binding::Wrap(aCx, this, aGivenProto);
}

void XULTreeElement::UnbindFromTree(bool aDeep, bool aNullParent) {
  // Drop the view's ref to us.
  if (mView) {
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel) {
      sel->SetTree(nullptr);
    }
    mView->SetTree(nullptr);  // Break the circular ref between the view and us.
  }
  mView = nullptr;

  nsXULElement::UnbindFromTree(aDeep, aNullParent);
}

void XULTreeElement::DestroyContent() {
  // Drop the view's ref to us.
  if (mView) {
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel) {
      sel->SetTree(nullptr);
    }
    mView->SetTree(nullptr);  // Break the circular ref between the view and us.
  }
  mView = nullptr;

  nsXULElement::DestroyContent();
}

static nsIContent* FindBodyElement(nsIContent* aParent) {
  mozilla::dom::FlattenedChildIterator iter(aParent);
  for (nsIContent* content = iter.GetNextChild(); content;
       content = iter.GetNextChild()) {
    mozilla::dom::NodeInfo* ni = content->NodeInfo();
    if (ni->Equals(nsGkAtoms::treechildren, kNameSpaceID_XUL)) {
      return content;
    } else if (ni->Equals(nsGkAtoms::tree, kNameSpaceID_XUL)) {
      // There are nesting tree elements. Only the innermost should
      // find the treechilren.
      return nullptr;
    } else if (content->IsElement() &&
               !ni->Equals(nsGkAtoms::_template, kNameSpaceID_XUL)) {
      nsIContent* result = FindBodyElement(content);
      if (result) return result;
    }
  }

  return nullptr;
}

nsTreeBodyFrame* XULTreeElement::GetTreeBodyFrame(bool aFlushLayout) {
  nsCOMPtr<nsIContent> kungFuDeathGrip = this;  // keep a reference
  RefPtr<Document> doc = GetUncomposedDoc();

  // Make sure our frames are up to date, and layout as needed.  We
  // have to do this before checking for our cached mTreeBody, since
  // it might go away on style flush, and in any case if aFlushLayout
  // is true we need to make sure to flush no matter what.
  // XXXbz except that flushing style when we were not asked to flush
  // layout here breaks things.  See bug 585123.
  if (aFlushLayout && doc) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  if (mTreeBody) {
    // Have one cached already.
    return mTreeBody;
  }

  if (!aFlushLayout && doc) {
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  nsCOMPtr<nsIContent> tree = FindBodyElement(this);
  if (tree) {
    mTreeBody = do_QueryFrame(tree->GetPrimaryFrame());
  }

  return mTreeBody;
}

nsresult XULTreeElement::GetView(nsITreeView** aView) {
  if (!mTreeBody) {
    if (!GetTreeBodyFrame()) {
      // Don't return an uninitialised view
      *aView = nullptr;
      return NS_OK;
    }

    if (mView) {
      // Our new frame needs to initialise itself
      return mTreeBody->GetView(aView);
    }
  }
  if (!mView) {
    // No tree builder, create a tree content view.
    nsresult rv = NS_NewTreeContentView(getter_AddRefs(mView));
    NS_ENSURE_SUCCESS(rv, rv);

    // Initialise the frame and view
    mTreeBody->SetView(mView);
  }
  NS_IF_ADDREF(*aView = mView);
  return NS_OK;
}

already_AddRefed<nsITreeView> XULTreeElement::GetView(CallerType /* unused */) {
  nsCOMPtr<nsITreeView> view;
  GetView(getter_AddRefs(view));
  return view.forget();
}

nsresult XULTreeElement::SetView(nsITreeView* aView) {
  ErrorResult rv;
  SetView(aView, CallerType::System, rv);
  return rv.StealNSResult();
}

void XULTreeElement::SetView(nsITreeView* aView, CallerType aCallerType,
                             ErrorResult& aRv) {
  if (aCallerType != CallerType::System) {
    // Don't trust views coming from random places.
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  mView = aView;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->SetView(aView);
}

bool XULTreeElement::Focused() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->GetFocused();
  return false;
}

nsresult XULTreeElement::GetFocused(bool* aFocused) {
  *aFocused = Focused();
  return NS_OK;
}

void XULTreeElement::SetFocused(bool aFocused) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->SetFocused(aFocused);
}

nsresult XULTreeElement::GetTreeBody(Element** aElement) {
  *aElement = nullptr;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->GetTreeBody(aElement);
  return NS_OK;
}

already_AddRefed<Element> XULTreeElement::GetTreeBody() {
  RefPtr<Element> el;
  GetTreeBody(getter_AddRefs(el));
  return el.forget();
}

already_AddRefed<nsTreeColumns> XULTreeElement::GetColumns() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->Columns();
  return nullptr;
}

nsresult XULTreeElement::GetColumns(nsTreeColumns** aColumns) {
  *aColumns = GetColumns().take();
  return NS_OK;
}

int32_t XULTreeElement::RowHeight() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->RowHeight();
  return 0;
}

int32_t XULTreeElement::RowWidth() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->RowWidth();
  return 0;
}

nsresult XULTreeElement::GetRowHeight(int32_t* aRowHeight) {
  *aRowHeight = RowHeight();
  return NS_OK;
}

nsresult XULTreeElement::GetRowWidth(int32_t* aRowWidth) {
  *aRowWidth = RowWidth();
  return NS_OK;
}

int32_t XULTreeElement::GetFirstVisibleRow() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->FirstVisibleRow();
  return 0;
}

nsresult XULTreeElement::GetFirstVisibleRow(int32_t* aFirstVisibleRow) {
  *aFirstVisibleRow = GetFirstVisibleRow();
  return NS_OK;
}

int32_t XULTreeElement::GetLastVisibleRow() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->LastVisibleRow();
  return 0;
}

nsresult XULTreeElement::GetLastVisibleRow(int32_t* aLastVisibleRow) {
  *aLastVisibleRow = GetLastVisibleRow();
  return NS_OK;
}

int32_t XULTreeElement::HorizontalPosition() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->GetHorizontalPosition();
  return 0;
}

int32_t XULTreeElement::GetPageLength() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->PageLength();
  return 0;
}

void XULTreeElement::EnsureRowIsVisible(int32_t aRow) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->EnsureRowIsVisible(aRow);
}

void XULTreeElement::EnsureCellIsVisible(int32_t aRow, nsTreeColumn* aCol,
                                         ErrorResult& aRv) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) {
    nsresult rv = body->EnsureCellIsVisible(aRow, aCol);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
    }
  }
}

void XULTreeElement::ScrollToRow(int32_t aRow) {
  nsTreeBodyFrame* body = GetTreeBodyFrame(true);
  if (!body) {
    return;
  }

  body->ScrollToRow(aRow);
}

void XULTreeElement::ScrollByLines(int32_t aNumLines) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (!body) {
    return;
  }
  body->ScrollByLines(aNumLines);
}

void XULTreeElement::ScrollByPages(int32_t aNumPages) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->ScrollByPages(aNumPages);
}

void XULTreeElement::Invalidate() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->Invalidate();
}

void XULTreeElement::InvalidateColumn(nsTreeColumn* aCol) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->InvalidateColumn(aCol);
}

void XULTreeElement::InvalidateRow(int32_t aIndex) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->InvalidateRow(aIndex);
}

void XULTreeElement::InvalidateCell(int32_t aRow, nsTreeColumn* aCol) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->InvalidateCell(aRow, aCol);
}

void XULTreeElement::InvalidateRange(int32_t aStart, int32_t aEnd) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->InvalidateRange(aStart, aEnd);
}

int32_t XULTreeElement::GetRowAt(int32_t x, int32_t y) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (!body) {
    return 0;
  }
  return body->GetRowAt(x, y);
}

nsresult XULTreeElement::GetCellAt(int32_t aX, int32_t aY, int32_t* aRow,
                                   nsTreeColumn** aCol, nsAString& aChildElt) {
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

void XULTreeElement::GetCellAt(int32_t x, int32_t y, TreeCellInfo& aRetVal,
                               ErrorResult& aRv) {
  GetCellAt(x, y, &aRetVal.mRow, getter_AddRefs(aRetVal.mCol),
            aRetVal.mChildElt);
}

nsresult XULTreeElement::GetCoordsForCellItem(int32_t aRow, nsTreeColumn* aCol,
                                              const nsAString& aElement,
                                              int32_t* aX, int32_t* aY,
                                              int32_t* aWidth,
                                              int32_t* aHeight) {
  *aX = *aY = *aWidth = *aHeight = 0;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  NS_ConvertUTF16toUTF8 element(aElement);
  if (body)
    return body->GetCoordsForCellItem(aRow, aCol, element, aX, aY, aWidth,
                                      aHeight);
  return NS_OK;
}

already_AddRefed<DOMRect> XULTreeElement::GetCoordsForCellItem(
    int32_t row, nsTreeColumn& col, const nsAString& element,
    ErrorResult& aRv) {
  int32_t x, y, w, h;
  GetCoordsForCellItem(row, &col, element, &x, &y, &w, &h);
  RefPtr<DOMRect> rect = new DOMRect(this, x, y, w, h);
  return rect.forget();
}

nsresult XULTreeElement::IsCellCropped(int32_t aRow, nsTreeColumn* aCol,
                                       bool* aIsCropped) {
  *aIsCropped = false;
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) return body->IsCellCropped(aRow, aCol, aIsCropped);
  return NS_OK;
}

bool XULTreeElement::IsCellCropped(int32_t row, nsTreeColumn* col,
                                   ErrorResult& aRv) {
  bool ret;
  aRv = IsCellCropped(row, col, &ret);
  return ret;
}

void XULTreeElement::RowCountChanged(int32_t aIndex, int32_t aDelta) {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->RowCountChanged(aIndex, aDelta);
}

void XULTreeElement::BeginUpdateBatch() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->BeginUpdateBatch();
}

void XULTreeElement::EndUpdateBatch() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->EndUpdateBatch();
}

void XULTreeElement::ClearStyleAndImageCaches() {
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) body->ClearStyleAndImageCaches();
}

void XULTreeElement::RemoveImageCacheEntry(int32_t aRowIndex,
                                           nsTreeColumn& aCol,
                                           ErrorResult& aRv) {
  if (NS_WARN_IF(aRowIndex < 0)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  nsTreeBodyFrame* body = GetTreeBodyFrame();
  if (body) {
    body->RemoveImageCacheEntry(aRowIndex, &aCol);
  }
}

}  // namespace dom
}  // namespace mozilla
