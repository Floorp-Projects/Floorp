/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIDOMElement.h"
#include "nsIBoxObject.h"
#include "nsTreeColumns.h"
#include "nsTreeUtils.h"
#include "nsStyleContext.h"
#include "nsDOMClassInfoID.h"
#include "nsContentUtils.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TreeBoxObject.h"
#include "mozilla/dom/TreeColumnBinding.h"
#include "mozilla/dom/TreeColumnsBinding.h"

using namespace mozilla;

// Column class that caches all the info about our column.
nsTreeColumn::nsTreeColumn(nsTreeColumns* aColumns, nsIContent* aContent)
  : mContent(aContent),
    mColumns(aColumns),
    mPrevious(nullptr)
{
  NS_ASSERTION(aContent &&
               aContent->NodeInfo()->Equals(nsGkAtoms::treecol,
                                            kNameSpaceID_XUL),
               "nsTreeColumn's content must be a <xul:treecol>");

  Invalidate();
}

nsTreeColumn::~nsTreeColumn()
{
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(nsTreeColumn)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTreeColumn)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTreeColumn)

// QueryInterface implementation for nsTreeColumn
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTreeColumn)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsITreeColumn)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  if (aIID.Equals(NS_GET_IID(nsTreeColumn))) {
    AddRef();
    *aInstancePtr = this;
    return NS_OK;
  }
  else
NS_INTERFACE_MAP_END

nsIFrame*
nsTreeColumn::GetFrame()
{
  NS_ENSURE_TRUE(mContent, nullptr);

  return mContent->GetPrimaryFrame();
}

bool
nsTreeColumn::IsLastVisible(nsTreeBodyFrame* aBodyFrame)
{
  NS_ASSERTION(GetFrame(), "should have checked for this already");

  // cyclers are fixed width, don't adjust them
  if (IsCycler())
    return false;

  // we're certainly not the last visible if we're not visible
  if (GetFrame()->GetRect().width == 0)
    return false;

  // try to find a visible successor
  for (nsTreeColumn *next = GetNext(); next; next = next->GetNext()) {
    nsIFrame* frame = next->GetFrame();
    if (frame && frame->GetRect().width > 0)
      return false;
  }
  return true;
}

nsresult
nsTreeColumn::GetRect(nsTreeBodyFrame* aBodyFrame, nscoord aY, nscoord aHeight, nsRect* aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame) {
    *aResult = nsRect();
    return NS_ERROR_FAILURE;
  }

  bool isRTL = aBodyFrame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
  *aResult = frame->GetRect();
  aResult->y = aY;
  aResult->height = aHeight;
  if (isRTL)
    aResult->x += aBodyFrame->mAdjustWidth;
  else if (IsLastVisible(aBodyFrame))
    aResult->width += aBodyFrame->mAdjustWidth;
  return NS_OK;
}

nsresult
nsTreeColumn::GetXInTwips(nsTreeBodyFrame* aBodyFrame, nscoord* aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame) {
    *aResult = 0;
    return NS_ERROR_FAILURE;
  }
  *aResult = frame->GetRect().x;
  return NS_OK;
}

nsresult
nsTreeColumn::GetWidthInTwips(nsTreeBodyFrame* aBodyFrame, nscoord* aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame) {
    *aResult = 0;
    return NS_ERROR_FAILURE;
  }
  *aResult = frame->GetRect().width;
  if (IsLastVisible(aBodyFrame))
    *aResult += aBodyFrame->mAdjustWidth;
  return NS_OK;
}


NS_IMETHODIMP
nsTreeColumn::GetElement(nsIDOMElement** aElement)
{
  if (mContent) {
    return CallQueryInterface(mContent, aElement);
  }
  *aElement = nullptr;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTreeColumn::GetColumns(nsITreeColumns** aColumns)
{
  NS_IF_ADDREF(*aColumns = mColumns);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetX(int32_t* aX)
{
  nsIFrame* frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  *aX = nsPresContext::AppUnitsToIntCSSPixels(frame->GetRect().x);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetWidth(int32_t* aWidth)
{
  nsIFrame* frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  *aWidth = nsPresContext::AppUnitsToIntCSSPixels(frame->GetRect().width);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetId(nsAString& aId)
{
  aId = GetId();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetIdConst(const char16_t** aIdConst)
{
  *aIdConst = mId.get();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetAtom(nsIAtom** aAtom)
{
  NS_IF_ADDREF(*aAtom = GetAtom());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetIndex(int32_t* aIndex)
{
  *aIndex = GetIndex();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetPrimary(bool* aPrimary)
{
  *aPrimary = IsPrimary();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetCycler(bool* aCycler)
{
  *aCycler = IsCycler();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetEditable(bool* aEditable)
{
  *aEditable = IsEditable();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetSelectable(bool* aSelectable)
{
  *aSelectable = IsSelectable();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetType(int16_t* aType)
{
  *aType = GetType();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetNext(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetNext());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetPrevious(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetPrevious());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::Invalidate()
{
  nsIFrame* frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // Fetch the Id.
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, mId);

  // If we have an Id, cache the Id as an atom.
  if (!mId.IsEmpty()) {
    mAtom = do_GetAtom(mId);
  }

  // Cache our index.
  nsTreeUtils::GetColumnIndex(mContent, &mIndex);

  const nsStyleVisibility* vis = frame->StyleVisibility();

  // Cache our text alignment policy.
  const nsStyleText* textStyle = frame->StyleText();

  mTextAlignment = textStyle->mTextAlign;
  // DEFAULT or END alignment sometimes means RIGHT
  if ((mTextAlignment == NS_STYLE_TEXT_ALIGN_DEFAULT &&
       vis->mDirection == NS_STYLE_DIRECTION_RTL) ||
      (mTextAlignment == NS_STYLE_TEXT_ALIGN_END &&
       vis->mDirection == NS_STYLE_DIRECTION_LTR)) {
    mTextAlignment = NS_STYLE_TEXT_ALIGN_RIGHT;
  } else if (mTextAlignment == NS_STYLE_TEXT_ALIGN_DEFAULT ||
             mTextAlignment == NS_STYLE_TEXT_ALIGN_END) {
    mTextAlignment = NS_STYLE_TEXT_ALIGN_LEFT;
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

  mIsSelectable = !mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::selectable,
                                         nsGkAtoms::_false, eCaseMatters);

  mOverflow = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::overflow,
                                    nsGkAtoms::_true, eCaseMatters);

  // Figure out our column type. Default type is text.
  mType = nsITreeColumn::TYPE_TEXT;
  static nsIContent::AttrValuesArray typestrings[] =
    {&nsGkAtoms::checkbox, &nsGkAtoms::progressmeter, nullptr};
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type,
                                    typestrings, eCaseMatters)) {
    case 0: mType = nsITreeColumn::TYPE_CHECKBOX; break;
    case 1: mType = nsITreeColumn::TYPE_PROGRESSMETER; break;
  }

  // Fetch the crop style.
  mCropStyle = 0;
  static nsIContent::AttrValuesArray cropstrings[] =
    {&nsGkAtoms::center, &nsGkAtoms::left, &nsGkAtoms::start, nullptr};
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

  return NS_OK;
}

nsIContent*
nsTreeColumn::GetParentObject() const
{
  return mContent;
}

/* virtual */ JSObject*
nsTreeColumn::WrapObject(JSContext* aCx)
{
  return dom::TreeColumnBinding::Wrap(aCx, this);
}

mozilla::dom::Element*
nsTreeColumn::GetElement(mozilla::ErrorResult& aRv)
{
  nsCOMPtr<nsIDOMElement> element;
  aRv = GetElement(getter_AddRefs(element));
  if (aRv.Failed()) {
    return nullptr;
  }
  nsCOMPtr<nsINode> node = do_QueryInterface(element);
  return node->AsElement();
}

int32_t
nsTreeColumn::GetX(mozilla::ErrorResult& aRv)
{
  int32_t x;
  aRv = GetX(&x);
  return x;
}

int32_t
nsTreeColumn::GetWidth(mozilla::ErrorResult& aRv)
{
  int32_t width;
  aRv = GetWidth(&width);
  return width;
}

void
nsTreeColumn::Invalidate(mozilla::ErrorResult& aRv)
{
  aRv = Invalidate();
}

nsTreeColumns::nsTreeColumns(nsTreeBodyFrame* aTree)
  : mTree(aTree),
    mFirstColumn(nullptr)
{
}

nsTreeColumns::~nsTreeColumns()
{
  nsTreeColumns::InvalidateColumns();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsTreeColumns)

// QueryInterface implementation for nsTreeColumns
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTreeColumns)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsITreeColumns)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
                                                                                
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTreeColumns)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTreeColumns)

nsIContent*
nsTreeColumns::GetParentObject() const
{
  return mTree ? mTree->GetBaseElement() : nullptr;
}

/* virtual */ JSObject*
nsTreeColumns::WrapObject(JSContext* aCx)
{
  return dom::TreeColumnsBinding::Wrap(aCx, this);
}

dom::TreeBoxObject*
nsTreeColumns::GetTree() const
{
  return mTree ? static_cast<mozilla::dom::TreeBoxObject*>(mTree->GetTreeBoxObject()) : nullptr;
}

NS_IMETHODIMP
nsTreeColumns::GetTree(nsITreeBoxObject** _retval)
{
  NS_IF_ADDREF(*_retval = GetTree());
  return NS_OK;
}

uint32_t
nsTreeColumns::Count()
{
  EnsureColumns();
  uint32_t count = 0;
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    ++count;
  }
  return count;
}

NS_IMETHODIMP
nsTreeColumns::GetCount(int32_t* _retval)
{
  *_retval = Count();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetLength(int32_t* _retval)
{
  *_retval = Length();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetFirstColumn(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetFirstColumn());
  return NS_OK;
}

nsTreeColumn*
nsTreeColumns::GetLastColumn()
{
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

NS_IMETHODIMP
nsTreeColumns::GetLastColumn(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetLastColumn());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetPrimaryColumn(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetPrimaryColumn());
  return NS_OK;
}

nsTreeColumn*
nsTreeColumns::GetSortedColumn()
{
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->mContent &&
        nsContentUtils::HasNonEmptyAttr(currCol->mContent, kNameSpaceID_None,
                                        nsGkAtoms::sortDirection)) {
      return currCol;
    }
  }
  return nullptr;
}

NS_IMETHODIMP
nsTreeColumns::GetSortedColumn(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetSortedColumn());
  return NS_OK;
}

nsTreeColumn*
nsTreeColumns::GetKeyColumn()
{
  EnsureColumns();

  nsTreeColumn* first = nullptr;
  nsTreeColumn* primary = nullptr;
  nsTreeColumn* sorted = nullptr;

  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    // Skip hidden columns.
    if (!currCol->mContent ||
        currCol->mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                                       nsGkAtoms::_true, eCaseMatters))
      continue;

    // Skip non-text column
    if (currCol->GetType() != nsITreeColumn::TYPE_TEXT)
      continue;

    if (!first)
      first = currCol;
    
    if (nsContentUtils::HasNonEmptyAttr(currCol->mContent, kNameSpaceID_None,
                                        nsGkAtoms::sortDirection)) {
      // Use sorted column as the key.
      sorted = currCol;
      break;
    }

    if (currCol->IsPrimary())
      if (!primary)
        primary = currCol;
  }

  if (sorted)
    return sorted;
  if (primary)
    return primary;
  return first;
}

NS_IMETHODIMP
nsTreeColumns::GetKeyColumn(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetKeyColumn());
  return NS_OK;
}

nsTreeColumn*
nsTreeColumns::GetColumnFor(dom::Element* aElement)
{
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->mContent == aElement) {
      return currCol;
    }
  }
  return nullptr;
}

NS_IMETHODIMP
nsTreeColumns::GetColumnFor(nsIDOMElement* aElement, nsITreeColumn** _retval)
{
  nsCOMPtr<dom::Element> element = do_QueryInterface(aElement);
  NS_ADDREF(*_retval = GetColumnFor(element));
  return NS_OK;
}

nsTreeColumn*
nsTreeColumns::NamedGetter(const nsAString& aId, bool& aFound)
{
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetId().Equals(aId)) {
      aFound = true;
      return currCol;
    }
  }
  aFound = false;
  return nullptr;
}

bool
nsTreeColumns::NameIsEnumerable(const nsAString& aName)
{
  return true;
}

nsTreeColumn*
nsTreeColumns::GetNamedColumn(const nsAString& aId)
{
  bool dummy;
  return NamedGetter(aId, dummy);
}

NS_IMETHODIMP
nsTreeColumns::GetNamedColumn(const nsAString& aId, nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetNamedColumn(aId));
  return NS_OK;
}

void
nsTreeColumns::GetSupportedNames(unsigned, nsTArray<nsString>& aNames)
{
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    aNames.AppendElement(currCol->GetId());
  }
}


nsTreeColumn*
nsTreeColumns::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetIndex() == static_cast<int32_t>(aIndex)) {
      aFound = true;
      return currCol;
    }
  }
  aFound = false;
  return nullptr;
}

nsTreeColumn*
nsTreeColumns::GetColumnAt(uint32_t aIndex)
{
  bool dummy;
  return IndexedGetter(aIndex, dummy);
}

NS_IMETHODIMP
nsTreeColumns::GetColumnAt(int32_t aIndex, nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetColumnAt(static_cast<uint32_t>(aIndex)));
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::InvalidateColumns()
{
  for (nsTreeColumn* currCol = mFirstColumn; currCol;
       currCol = currCol->GetNext()) {
    currCol->SetColumns(nullptr);
  }
  NS_IF_RELEASE(mFirstColumn);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::RestoreNaturalOrder()
{
  if (!mTree)
    return NS_OK;

  nsIContent* content = mTree->GetBaseElement();

  // Strong ref, since we'll be setting attributes
  nsCOMPtr<nsIContent> colsContent =
    nsTreeUtils::GetImmediateChild(content, nsGkAtoms::treecols);
  if (!colsContent)
    return NS_OK;

  for (uint32_t i = 0; i < colsContent->GetChildCount(); ++i) {
    nsCOMPtr<nsIContent> child = colsContent->GetChildAt(i);
    nsAutoString ordinal;
    ordinal.AppendInt(i);
    child->SetAttr(kNameSpaceID_None, nsGkAtoms::ordinal, ordinal, true);
  }

  nsTreeColumns::InvalidateColumns();

  if (mTree) {
    mTree->Invalidate();
  }
  return NS_OK;
}

nsTreeColumn*
nsTreeColumns::GetPrimaryColumn()
{
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->IsPrimary()) {
      return currCol;
    }
  }
  return nullptr;
}

void
nsTreeColumns::EnsureColumns()
{
  if (mTree && !mFirstColumn) {
    nsIContent* treeContent = mTree->GetBaseElement();
    nsIContent* colsContent =
      nsTreeUtils::GetDescendantChild(treeContent, nsGkAtoms::treecols);
    if (!colsContent)
      return;

    nsIContent* colContent =
      nsTreeUtils::GetDescendantChild(colsContent, nsGkAtoms::treecol);
    if (!colContent)
      return;

    nsIFrame* colFrame = colContent->GetPrimaryFrame();
    if (!colFrame)
      return;

    colFrame = colFrame->GetParent();
    if (!colFrame)
      return;

    colFrame = colFrame->GetFirstPrincipalChild();
    if (!colFrame)
      return;

    // Now that we have the first visible column,
    // we can enumerate the columns in visible order
    nsTreeColumn* currCol = nullptr;
    while (colFrame) {
      nsIContent* colContent = colFrame->GetContent();

      if (colContent->NodeInfo()->Equals(nsGkAtoms::treecol,
                                         kNameSpaceID_XUL)) {
        // Create a new column structure.
        nsTreeColumn* col = new nsTreeColumn(this, colContent);
        if (!col)
          return;

        if (currCol) {
          currCol->SetNext(col);
          col->SetPrevious(currCol);
        }
        else {
          NS_ADDREF(mFirstColumn = col);
        }
        currCol = col;
      }

      colFrame = colFrame->GetNextSibling();
    }
  }
}
