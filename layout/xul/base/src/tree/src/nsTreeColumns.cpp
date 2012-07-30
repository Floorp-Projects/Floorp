/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIDOMElement.h"
#include "nsIBoxObject.h"
#include "nsTreeColumns.h"
#include "nsTreeUtils.h"
#include "nsStyleContext.h"
#include "nsDOMClassInfoID.h"
#include "nsINodeInfo.h"
#include "nsContentUtils.h"
#include "nsTreeBodyFrame.h"

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
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContent)
  if (tmp->mNext) {
    tmp->mNext->SetPrevious(nullptr);
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNext)
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsTreeColumn)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTreeColumn)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTreeColumn)

DOMCI_DATA(TreeColumn, nsTreeColumn)

// QueryInterface implementation for nsTreeColumn
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTreeColumn)
  NS_INTERFACE_MAP_ENTRY(nsITreeColumn)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TreeColumn)
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

  bool isRTL = aBodyFrame->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
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
nsTreeColumn::GetX(PRInt32* aX)
{
  nsIFrame* frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  *aX = nsPresContext::AppUnitsToIntCSSPixels(frame->GetRect().x);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetWidth(PRInt32* aWidth)
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
nsTreeColumn::GetIdConst(const PRUnichar** aIdConst)
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
nsTreeColumn::GetIndex(PRInt32* aIndex)
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
nsTreeColumn::GetType(PRInt16* aType)
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

  const nsStyleVisibility* vis = frame->GetStyleVisibility();

  // Cache our text alignment policy.
  const nsStyleText* textStyle = frame->GetStyleText();

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


nsTreeColumns::nsTreeColumns(nsITreeBoxObject* aTree)
  : mTree(aTree),
    mFirstColumn(nullptr)
{
}

nsTreeColumns::~nsTreeColumns()
{
  nsTreeColumns::InvalidateColumns();
}

DOMCI_DATA(TreeColumns, nsTreeColumns)

// QueryInterface implementation for nsTreeColumns
NS_INTERFACE_MAP_BEGIN(nsTreeColumns)
  NS_INTERFACE_MAP_ENTRY(nsITreeColumns)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TreeColumns)
NS_INTERFACE_MAP_END
                                                                                
NS_IMPL_ADDREF(nsTreeColumns)
NS_IMPL_RELEASE(nsTreeColumns)

NS_IMETHODIMP
nsTreeColumns::GetTree(nsITreeBoxObject** _retval)
{
  NS_IF_ADDREF(*_retval = mTree);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetCount(PRInt32* _retval)
{
  EnsureColumns();
  *_retval = 0;
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    ++(*_retval);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetLength(PRInt32* _retval)
{
  return GetCount(_retval);
}

NS_IMETHODIMP
nsTreeColumns::GetFirstColumn(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetFirstColumn());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetLastColumn(nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nullptr;
  nsTreeColumn* currCol = mFirstColumn;
  while (currCol) {
    nsTreeColumn* next = currCol->GetNext();
    if (!next) {
      NS_ADDREF(*_retval = currCol);
      break;
    }
    currCol = next;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetPrimaryColumn(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetPrimaryColumn());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetSortedColumn(nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nullptr;
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->mContent &&
        nsContentUtils::HasNonEmptyAttr(currCol->mContent, kNameSpaceID_None,
                                        nsGkAtoms::sortDirection)) {
      NS_ADDREF(*_retval = currCol);
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetKeyColumn(nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nullptr;

  nsTreeColumn* first;
  nsTreeColumn* primary;
  nsTreeColumn* sorted;
  first = primary = sorted = nullptr;

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
    *_retval = sorted;
  else if (primary)
    *_retval = primary;
  else
    *_retval = first;

  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetColumnFor(nsIDOMElement* aElement, nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nullptr;
  nsCOMPtr<nsIContent> element = do_QueryInterface(aElement);
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->mContent == element) {
      NS_ADDREF(*_retval = currCol);
      break;
    }
  }

  return NS_OK;
}

nsITreeColumn*
nsTreeColumns::GetNamedColumn(const nsAString& aId)
{
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetId().Equals(aId)) {
      return currCol;
    }
  }
  return nullptr;
}

NS_IMETHODIMP
nsTreeColumns::GetNamedColumn(const nsAString& aId, nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetNamedColumn(aId));
  return NS_OK;
}

nsITreeColumn*
nsTreeColumns::GetColumnAt(PRInt32 aIndex)
{
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetIndex() == aIndex) {
      return currCol;
    }
  }
  return nullptr;
}

NS_IMETHODIMP
nsTreeColumns::GetColumnAt(PRInt32 aIndex, nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetColumnAt(aIndex));
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

  nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mTree);
  nsCOMPtr<nsIDOMElement> element;
  boxObject->GetElement(getter_AddRefs(element));
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);

  // Strong ref, since we'll be setting attributes
  nsCOMPtr<nsIContent> colsContent =
    nsTreeUtils::GetImmediateChild(content, nsGkAtoms::treecols);
  if (!colsContent)
    return NS_OK;

  PRUint32 numChildren = colsContent->GetChildCount();
  for (PRUint32 i = 0; i < numChildren; ++i) {
    nsIContent *child = colsContent->GetChildAt(i);
    nsAutoString ordinal;
    ordinal.AppendInt(i);
    child->SetAttr(kNameSpaceID_None, nsGkAtoms::ordinal, ordinal, true);
  }

  nsTreeColumns::InvalidateColumns();

  mTree->Invalidate();

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
    nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mTree);
    nsCOMPtr<nsIDOMElement> treeElement;
    boxObject->GetElement(getter_AddRefs(treeElement));
    nsCOMPtr<nsIContent> treeContent = do_QueryInterface(treeElement);

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
