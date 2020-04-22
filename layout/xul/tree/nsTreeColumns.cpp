/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsTreeColumns.h"
#include "nsTreeUtils.h"
#include "mozilla/ComputedStyle.h"
#include "nsContentUtils.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TreeColumnBinding.h"
#include "mozilla/dom/TreeColumnsBinding.h"
#include "mozilla/dom/XULTreeElement.h"

using namespace mozilla;
using namespace mozilla::dom;

// Column class that caches all the info about our column.
nsTreeColumn::nsTreeColumn(nsTreeColumns* aColumns, dom::Element* aElement)
    : mContent(aElement), mColumns(aColumns), mIndex(0), mPrevious(nullptr) {
  NS_ASSERTION(aElement && aElement->NodeInfo()->Equals(nsGkAtoms::treecol,
                                                        kNameSpaceID_XUL),
               "nsTreeColumn's content must be a <xul:treecol>");

  Invalidate(IgnoreErrors());
}

nsTreeColumn::~nsTreeColumn() {
  if (mNext) {
    mNext->SetPrevious(nullptr);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsTreeColumn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsTreeColumn)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContent)
  if (tmp->mNext) {
    tmp->mNext->SetPrevious(nullptr);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mNext)
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsTreeColumn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(nsTreeColumn)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTreeColumn)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTreeColumn)

// QueryInterface implementation for nsTreeColumn
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTreeColumn)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(nsTreeColumn)
NS_INTERFACE_MAP_END

nsIFrame* nsTreeColumn::GetFrame() { return mContent->GetPrimaryFrame(); }

bool nsTreeColumn::IsLastVisible(nsTreeBodyFrame* aBodyFrame) {
  NS_ASSERTION(GetFrame(), "should have checked for this already");

  // cyclers are fixed width, don't adjust them
  if (IsCycler()) return false;

  // we're certainly not the last visible if we're not visible
  if (GetFrame()->GetRect().width == 0) return false;

  // try to find a visible successor
  for (nsTreeColumn* next = GetNext(); next; next = next->GetNext()) {
    nsIFrame* frame = next->GetFrame();
    if (frame && frame->GetRect().width > 0) return false;
  }
  return true;
}

nsresult nsTreeColumn::GetRect(nsTreeBodyFrame* aBodyFrame, nscoord aY,
                               nscoord aHeight, nsRect* aResult) {
  nsIFrame* frame = GetFrame();
  if (!frame) {
    *aResult = nsRect();
    return NS_ERROR_FAILURE;
  }

  bool isRTL = aBodyFrame->StyleVisibility()->mDirection == StyleDirection::Rtl;
  *aResult = frame->GetRect();
  aResult->y = aY;
  aResult->height = aHeight;
  if (isRTL)
    aResult->x += aBodyFrame->mAdjustWidth;
  else if (IsLastVisible(aBodyFrame))
    aResult->width += aBodyFrame->mAdjustWidth;
  return NS_OK;
}

nsresult nsTreeColumn::GetXInTwips(nsTreeBodyFrame* aBodyFrame,
                                   nscoord* aResult) {
  nsIFrame* frame = GetFrame();
  if (!frame) {
    *aResult = 0;
    return NS_ERROR_FAILURE;
  }
  *aResult = frame->GetRect().x;
  return NS_OK;
}

nsresult nsTreeColumn::GetWidthInTwips(nsTreeBodyFrame* aBodyFrame,
                                       nscoord* aResult) {
  nsIFrame* frame = GetFrame();
  if (!frame) {
    *aResult = 0;
    return NS_ERROR_FAILURE;
  }
  *aResult = frame->GetRect().width;
  if (IsLastVisible(aBodyFrame)) *aResult += aBodyFrame->mAdjustWidth;
  return NS_OK;
}

void nsTreeColumn::GetId(nsAString& aId) const { aId = GetId(); }

void nsTreeColumn::Invalidate(ErrorResult& aRv) {
  nsIFrame* frame = GetFrame();
  if (NS_WARN_IF(!frame)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Fetch the Id.
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, mId);

  // If we have an Id, cache the Id as an atom.
  if (!mId.IsEmpty()) {
    mAtom = NS_Atomize(mId);
  }

  // Cache our index.
  nsTreeUtils::GetColumnIndex(mContent, &mIndex);

  const nsStyleVisibility* vis = frame->StyleVisibility();

  // Cache our text alignment policy.
  const nsStyleText* textStyle = frame->StyleText();

  mTextAlignment = textStyle->mTextAlign;
  // START or END alignment sometimes means RIGHT
  if ((mTextAlignment == StyleTextAlign::Start &&
       vis->mDirection == StyleDirection::Rtl) ||
      (mTextAlignment == StyleTextAlign::End &&
       vis->mDirection == StyleDirection::Ltr)) {
    mTextAlignment = StyleTextAlign::Right;
  } else if (mTextAlignment == StyleTextAlign::Start ||
             mTextAlignment == StyleTextAlign::End) {
    mTextAlignment = StyleTextAlign::Left;
  }

  // Figure out if we're the primary column (that has to have indentation
  // and twisties drawn.
  mIsPrimary = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::primary,
                                     nsGkAtoms::_true, eCaseMatters);

  // Figure out if we're a cycling column (one that doesn't cause a selection
  // to happen).
  mIsCycler = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::cycler,
                                    nsGkAtoms::_true, eCaseMatters);

  mIsEditable = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::editable,
                                      nsGkAtoms::_true, eCaseMatters);

  mOverflow = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::overflow,
                                    nsGkAtoms::_true, eCaseMatters);

  // Figure out our column type. Default type is text.
  mType = TreeColumn_Binding::TYPE_TEXT;
  static Element::AttrValuesArray typestrings[] = {nsGkAtoms::checkbox,
                                                   nullptr};
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type,
                                    typestrings, eCaseMatters)) {
    case 0:
      mType = TreeColumn_Binding::TYPE_CHECKBOX;
      break;
  }

  // Fetch the crop style.
  mCropStyle = 0;
  static Element::AttrValuesArray cropstrings[] = {
      nsGkAtoms::center, nsGkAtoms::left, nsGkAtoms::start, nullptr};
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::crop,
                                    cropstrings, eCaseMatters)) {
    case 0:
      mCropStyle = 1;
      break;
    case 1:
    case 2:
      mCropStyle = 2;
      break;
  }
}

nsIContent* nsTreeColumn::GetParentObject() const { return mContent; }

/* virtual */
JSObject* nsTreeColumn::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return dom::TreeColumn_Binding::Wrap(aCx, this, aGivenProto);
}

Element* nsTreeColumn::Element() { return mContent; }

int32_t nsTreeColumn::GetX(mozilla::ErrorResult& aRv) {
  nsIFrame* frame = GetFrame();
  if (NS_WARN_IF(!frame)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return 0;
  }

  return nsPresContext::AppUnitsToIntCSSPixels(frame->GetRect().x);
}

int32_t nsTreeColumn::GetWidth(mozilla::ErrorResult& aRv) {
  nsIFrame* frame = GetFrame();
  if (NS_WARN_IF(!frame)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return 0;
  }

  return nsPresContext::AppUnitsToIntCSSPixels(frame->GetRect().width);
}

already_AddRefed<nsTreeColumn> nsTreeColumn::GetPreviousColumn() {
  nsIFrame* frame = GetFrame();
  while (frame) {
    frame = frame->GetPrevSibling();
    if (frame && frame->GetContent()->IsElement()) {
      RefPtr<nsTreeColumn> column =
          mColumns->GetColumnFor(frame->GetContent()->AsElement());
      if (column) {
        return column.forget();
      }
    }
  }

  return nullptr;
}

nsTreeColumns::nsTreeColumns(nsTreeBodyFrame* aTree) : mTree(aTree) {}

nsTreeColumns::~nsTreeColumns() { nsTreeColumns::InvalidateColumns(); }

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsTreeColumns)

// QueryInterface implementation for nsTreeColumns
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTreeColumns)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTreeColumns)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTreeColumns)

nsIContent* nsTreeColumns::GetParentObject() const {
  return mTree ? mTree->GetBaseElement() : nullptr;
}

/* virtual */
JSObject* nsTreeColumns::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return dom::TreeColumns_Binding::Wrap(aCx, this, aGivenProto);
}

XULTreeElement* nsTreeColumns::GetTree() const {
  if (!mTree) {
    return nullptr;
  }

  return XULTreeElement::FromNodeOrNull(mTree->GetBaseElement());
}

uint32_t nsTreeColumns::Count() {
  EnsureColumns();
  uint32_t count = 0;
  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    ++count;
  }
  return count;
}

nsTreeColumn* nsTreeColumns::GetLastColumn() {
  EnsureColumns();
  nsTreeColumn* currCol = mFirstColumn;
  while (currCol) {
    nsTreeColumn* next = currCol->GetNext();
    if (!next) {
      return currCol;
    }
    currCol = next;
  }
  return nullptr;
}

nsTreeColumn* nsTreeColumns::GetSortedColumn() {
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    if (nsContentUtils::HasNonEmptyAttr(currCol->mContent, kNameSpaceID_None,
                                        nsGkAtoms::sortDirection)) {
      return currCol;
    }
  }
  return nullptr;
}

nsTreeColumn* nsTreeColumns::GetKeyColumn() {
  EnsureColumns();

  nsTreeColumn* first = nullptr;
  nsTreeColumn* primary = nullptr;
  nsTreeColumn* sorted = nullptr;

  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    // Skip hidden columns.
    if (currCol->mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                                       nsGkAtoms::_true, eCaseMatters))
      continue;

    // Skip non-text column
    if (currCol->GetType() != TreeColumn_Binding::TYPE_TEXT) continue;

    if (!first) first = currCol;

    if (nsContentUtils::HasNonEmptyAttr(currCol->mContent, kNameSpaceID_None,
                                        nsGkAtoms::sortDirection)) {
      // Use sorted column as the key.
      sorted = currCol;
      break;
    }

    if (currCol->IsPrimary())
      if (!primary) primary = currCol;
  }

  if (sorted) return sorted;
  if (primary) return primary;
  return first;
}

nsTreeColumn* nsTreeColumns::GetColumnFor(dom::Element* aElement) {
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    if (currCol->mContent == aElement) {
      return currCol;
    }
  }
  return nullptr;
}

nsTreeColumn* nsTreeColumns::NamedGetter(const nsAString& aId, bool& aFound) {
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    if (currCol->GetId().Equals(aId)) {
      aFound = true;
      return currCol;
    }
  }
  aFound = false;
  return nullptr;
}

nsTreeColumn* nsTreeColumns::GetNamedColumn(const nsAString& aId) {
  bool dummy;
  return NamedGetter(aId, dummy);
}

void nsTreeColumns::GetSupportedNames(nsTArray<nsString>& aNames) {
  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    aNames.AppendElement(currCol->GetId());
  }
}

nsTreeColumn* nsTreeColumns::IndexedGetter(uint32_t aIndex, bool& aFound) {
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    if (currCol->GetIndex() == static_cast<int32_t>(aIndex)) {
      aFound = true;
      return currCol;
    }
  }
  aFound = false;
  return nullptr;
}

nsTreeColumn* nsTreeColumns::GetColumnAt(uint32_t aIndex) {
  bool dummy;
  return IndexedGetter(aIndex, dummy);
}

void nsTreeColumns::InvalidateColumns() {
  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    currCol->SetColumns(nullptr);
  }
  mFirstColumn = nullptr;
}

nsTreeColumn* nsTreeColumns::GetPrimaryColumn() {
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    if (currCol->IsPrimary()) {
      return currCol;
    }
  }
  return nullptr;
}

void nsTreeColumns::EnsureColumns() {
  if (mTree && !mFirstColumn) {
    nsIContent* treeContent = mTree->GetBaseElement();
    if (!treeContent) return;

    nsIContent* colsContent =
        nsTreeUtils::GetDescendantChild(treeContent, nsGkAtoms::treecols);
    if (!colsContent) return;

    nsIContent* colContent =
        nsTreeUtils::GetDescendantChild(colsContent, nsGkAtoms::treecol);
    if (!colContent) return;

    nsIFrame* colFrame = colContent->GetPrimaryFrame();
    if (!colFrame) return;

    colFrame = colFrame->GetParent();
    if (!colFrame) return;

    colFrame = colFrame->PrincipalChildList().FirstChild();
    if (!colFrame) return;

    // Now that we have the first visible column,
    // we can enumerate the columns in visible order
    nsTreeColumn* currCol = nullptr;
    while (colFrame) {
      nsIContent* colContent = colFrame->GetContent();

      if (colContent->NodeInfo()->Equals(nsGkAtoms::treecol,
                                         kNameSpaceID_XUL)) {
        // Create a new column structure.
        nsTreeColumn* col = new nsTreeColumn(this, colContent->AsElement());
        if (!col) return;

        if (currCol) {
          currCol->SetNext(col);
          col->SetPrevious(currCol);
        } else {
          mFirstColumn = col;
        }
        currCol = col;
      }

      colFrame = colFrame->GetNextSibling();
    }
  }
}
