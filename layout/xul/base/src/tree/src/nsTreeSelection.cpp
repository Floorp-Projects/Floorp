/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsTreeSelection.h"
#include "nsIBoxObject.h"
#include "nsITreeBoxObject.h"
#include "nsITreeView.h"
#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsDOMClassInfoID.h"
#include "nsIContent.h"
#include "nsGUIEvent.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsAsyncDOMEvent.h"
#include "nsEventDispatcher.h"
#include "nsAutoPtr.h"

// A helper class for managing our ranges of selection.
struct nsTreeRange
{
  nsTreeSelection* mSelection;

  nsTreeRange* mPrev;
  nsTreeRange* mNext;

  PRInt32 mMin;
  PRInt32 mMax;

  nsTreeRange(nsTreeSelection* aSel, PRInt32 aSingleVal)
    :mSelection(aSel), mPrev(nullptr), mNext(nullptr), mMin(aSingleVal), mMax(aSingleVal) {}
  nsTreeRange(nsTreeSelection* aSel, PRInt32 aMin, PRInt32 aMax) 
    :mSelection(aSel), mPrev(nullptr), mNext(nullptr), mMin(aMin), mMax(aMax) {}

  ~nsTreeRange() { delete mNext; }

  void Connect(nsTreeRange* aPrev = nullptr, nsTreeRange* aNext = nullptr) {
    if (aPrev)
      aPrev->mNext = this;
    else
      mSelection->mFirstRange = this;

    if (aNext)
      aNext->mPrev = this;

    mPrev = aPrev;
    mNext = aNext;
  }

  nsresult RemoveRange(PRInt32 aStart, PRInt32 aEnd) {
    // This should so be a loop... sigh...
    // We start past the range to remove, so no more to remove
    if (aEnd < mMin)
      return NS_OK;
    // We are the last range to be affected
    if (aEnd < mMax) {
      if (aStart <= mMin) {
        // Just chop the start of the range off
        mMin = aEnd + 1;
      } else {
        // We need to split the range
        nsTreeRange* range = new nsTreeRange(mSelection, aEnd + 1, mMax);
        if (!range)
          return NS_ERROR_OUT_OF_MEMORY;

        mMax = aStart - 1;
        range->Connect(this, mNext);
      }
      return NS_OK;
    }
    nsTreeRange* next = mNext;
    if (aStart <= mMin) {
      // The remove includes us, remove ourselves from the list
      if (mPrev)
        mPrev->mNext = next;
      else
        mSelection->mFirstRange = next;

      if (next)
        next->mPrev = mPrev;
      mPrev = mNext = nullptr;
      delete this;
    } else if (aStart <= mMax) {
      // Just chop the end of the range off
      mMax = aStart - 1;
    }
    return next ? next->RemoveRange(aStart, aEnd) : NS_OK;
  }

  nsresult Remove(PRInt32 aIndex) {
    if (aIndex >= mMin && aIndex <= mMax) {
      // We have found the range that contains us.
      if (mMin == mMax) {
        // Delete the whole range.
        if (mPrev)
          mPrev->mNext = mNext;
        if (mNext)
          mNext->mPrev = mPrev;
        nsTreeRange* first = mSelection->mFirstRange;
        if (first == this)
          mSelection->mFirstRange = mNext;
        mNext = mPrev = nullptr;
        delete this;
      }
      else if (aIndex == mMin)
        mMin++;
      else if (aIndex == mMax)
        mMax--;
      else {
        // We have to break this range.
        nsTreeRange* newRange = new nsTreeRange(mSelection, aIndex + 1, mMax);
        if (!newRange)
          return NS_ERROR_OUT_OF_MEMORY;

        newRange->Connect(this, mNext);
        mMax = aIndex - 1;
      }
    }
    else if (mNext)
      return mNext->Remove(aIndex);

    return NS_OK;
  }

  nsresult Add(PRInt32 aIndex) {
    if (aIndex < mMin) {
      // We have found a spot to insert.
      if (aIndex + 1 == mMin)
        mMin = aIndex;
      else if (mPrev && mPrev->mMax+1 == aIndex)
        mPrev->mMax = aIndex;
      else {
        // We have to create a new range.
        nsTreeRange* newRange = new nsTreeRange(mSelection, aIndex);
        if (!newRange)
          return NS_ERROR_OUT_OF_MEMORY;

        newRange->Connect(mPrev, this);
      }
    }
    else if (mNext)
      mNext->Add(aIndex);
    else {
      // Insert on to the end.
      if (mMax+1 == aIndex)
        mMax = aIndex;
      else {
        // We have to create a new range.
        nsTreeRange* newRange = new nsTreeRange(mSelection, aIndex);
        if (!newRange)
          return NS_ERROR_OUT_OF_MEMORY;

        newRange->Connect(this, nullptr);
      }
    }
    return NS_OK;
  }

  bool Contains(PRInt32 aIndex) {
    if (aIndex >= mMin && aIndex <= mMax)
      return true;

    if (mNext)
      return mNext->Contains(aIndex);

    return false;
  }

  PRInt32 Count() {
    PRInt32 total = mMax - mMin + 1;
    if (mNext)
      total += mNext->Count();
    return total;
  }

  static void CollectRanges(nsTreeRange* aRange, nsTArray<PRInt32>& aRanges)
  {
    nsTreeRange* cur = aRange;
    while (cur) {
      aRanges.AppendElement(cur->mMin);
      aRanges.AppendElement(cur->mMax);
      cur = cur->mNext;
    }
  }
  
  static void InvalidateRanges(nsITreeBoxObject* aTree,
                               nsTArray<PRInt32>& aRanges)
  {
    if (aTree) {
      nsCOMPtr<nsITreeBoxObject> tree = aTree;
      for (PRUint32 i = 0; i < aRanges.Length(); i += 2) {
        aTree->InvalidateRange(aRanges[i], aRanges[i + 1]);
      }
    }
  }

  void Invalidate() {
    nsTArray<PRInt32> ranges;
    CollectRanges(this, ranges);
    InvalidateRanges(mSelection->mTree, ranges);
    
  }

  void RemoveAllBut(PRInt32 aIndex) {
    if (aIndex >= mMin && aIndex <= mMax) {

      // Invalidate everything in this list.
      nsTArray<PRInt32> ranges;
      CollectRanges(mSelection->mFirstRange, ranges);

      mMin = aIndex;
      mMax = aIndex;
      
      nsTreeRange* first = mSelection->mFirstRange;
      if (mPrev)
        mPrev->mNext = mNext;
      if (mNext)
        mNext->mPrev = mPrev;
      mNext = mPrev = nullptr;
      
      if (first != this) {
        delete mSelection->mFirstRange;
        mSelection->mFirstRange = this;
      }
      InvalidateRanges(mSelection->mTree, ranges);
    }
    else if (mNext)
      mNext->RemoveAllBut(aIndex);
  }

  void Insert(nsTreeRange* aRange) {
    if (mMin >= aRange->mMax)
      aRange->Connect(mPrev, this);
    else if (mNext)
      mNext->Insert(aRange);
    else 
      aRange->Connect(this, nullptr);
  }
};

nsTreeSelection::nsTreeSelection(nsITreeBoxObject* aTree)
  : mTree(aTree),
    mSuppressed(false),
    mCurrentIndex(-1),
    mShiftSelectPivot(-1),
    mFirstRange(nullptr)
{
}

nsTreeSelection::~nsTreeSelection()
{
  delete mFirstRange;
  if (mSelectTimer)
    mSelectTimer->Cancel();
}

NS_IMPL_CYCLE_COLLECTION_2(nsTreeSelection, mTree, mCurrentColumn)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTreeSelection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTreeSelection)

DOMCI_DATA(TreeSelection, nsTreeSelection)

// QueryInterface implementation for nsBoxObject
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTreeSelection)
  NS_INTERFACE_MAP_ENTRY(nsITreeSelection)
  NS_INTERFACE_MAP_ENTRY(nsINativeTreeSelection)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TreeSelection)
NS_INTERFACE_MAP_END

NS_IMETHODIMP nsTreeSelection::GetTree(nsITreeBoxObject * *aTree)
{
  NS_IF_ADDREF(*aTree = mTree);
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::SetTree(nsITreeBoxObject * aTree)
{
  if (mSelectTimer) {
    mSelectTimer->Cancel();
    mSelectTimer = nullptr;
  }

  // Make sure aTree really implements nsITreeBoxObject and nsIBoxObject!
  nsCOMPtr<nsIBoxObject> bo = do_QueryInterface(aTree);
  mTree = do_QueryInterface(bo);
  NS_ENSURE_STATE(mTree == aTree);
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::GetSingle(bool* aSingle)
{
  if (!mTree)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mTree);

  nsCOMPtr<nsIDOMElement> element;
  boxObject->GetElement(getter_AddRefs(element));

  nsCOMPtr<nsIContent> content = do_QueryInterface(element);

  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::single, &nsGkAtoms::cell, &nsGkAtoms::text, nullptr};

  *aSingle = content->FindAttrValueIn(kNameSpaceID_None,
                                      nsGkAtoms::seltype,
                                      strings, eCaseMatters) >= 0;

  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::IsSelected(PRInt32 aIndex, bool* aResult)
{
  if (mFirstRange)
    *aResult = mFirstRange->Contains(aIndex);
  else
    *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::TimedSelect(PRInt32 aIndex, PRInt32 aMsec)
{
  bool suppressSelect = mSuppressed;

  if (aMsec != -1)
    mSuppressed = true;

  nsresult rv = Select(aIndex);
  if (NS_FAILED(rv))
    return rv;

  if (aMsec != -1) {
    mSuppressed = suppressSelect;
    if (!mSuppressed) {
      if (mSelectTimer)
        mSelectTimer->Cancel();

      mSelectTimer = do_CreateInstance("@mozilla.org/timer;1");
      mSelectTimer->InitWithFuncCallback(SelectCallback, this, aMsec, 
                                         nsITimer::TYPE_ONE_SHOT);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::Select(PRInt32 aIndex)
{
  mShiftSelectPivot = -1;

  nsresult rv = SetCurrentIndex(aIndex);
  if (NS_FAILED(rv))
    return rv;

  if (mFirstRange) {
    bool alreadySelected = mFirstRange->Contains(aIndex);

    if (alreadySelected) {
      PRInt32 count = mFirstRange->Count();
      if (count > 1) {
        // We need to deselect everything but our item.
        mFirstRange->RemoveAllBut(aIndex);
        FireOnSelectHandler();
      }
      return NS_OK;
    }
    else {
      // Clear out our selection.
      mFirstRange->Invalidate();
      delete mFirstRange;
    }
  }

  // Create our new selection.
  mFirstRange = new nsTreeRange(this, aIndex);
  if (!mFirstRange)
    return NS_ERROR_OUT_OF_MEMORY;

  mFirstRange->Invalidate();

  // Fire the select event
  FireOnSelectHandler();
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::ToggleSelect(PRInt32 aIndex)
{
  // There are six cases that can occur on a ToggleSelect with our
  // range code.
  // (1) A new range should be made for a selection.
  // (2) A single range is removed from the selection.
  // (3) The item is added to an existing range.
  // (4) The item is removed from an existing range.
  // (5) The addition of the item causes two ranges to be merged.
  // (6) The removal of the item causes two ranges to be split.
  mShiftSelectPivot = -1;
  nsresult rv = SetCurrentIndex(aIndex);
  if (NS_FAILED(rv))
    return rv;

  if (!mFirstRange)
    Select(aIndex);
  else {
    if (!mFirstRange->Contains(aIndex)) {
      bool single;
      rv = GetSingle(&single);
      if (NS_SUCCEEDED(rv) && !single)
        rv = mFirstRange->Add(aIndex);
    }
    else
      rv = mFirstRange->Remove(aIndex);
    if (NS_SUCCEEDED(rv)) {
      if (mTree)
        mTree->InvalidateRow(aIndex);

      FireOnSelectHandler();
    }
  }

  return rv;
}

NS_IMETHODIMP nsTreeSelection::RangedSelect(PRInt32 aStartIndex, PRInt32 aEndIndex, bool aAugment)
{
  bool single;
  nsresult rv = GetSingle(&single);
  if (NS_FAILED(rv))
    return rv;

  if ((mFirstRange || (aStartIndex != aEndIndex)) && single)
    return NS_OK;

  if (!aAugment) {
    // Clear our selection.
    if (mFirstRange) {
        mFirstRange->Invalidate();
        delete mFirstRange;
        mFirstRange = nullptr;
    }
  }

  if (aStartIndex == -1) {
    if (mShiftSelectPivot != -1)
      aStartIndex = mShiftSelectPivot;
    else if (mCurrentIndex != -1)
      aStartIndex = mCurrentIndex;
    else
      aStartIndex = aEndIndex;
  }

  mShiftSelectPivot = aStartIndex;
  rv = SetCurrentIndex(aEndIndex);
  if (NS_FAILED(rv))
    return rv;
  
  PRInt32 start = aStartIndex < aEndIndex ? aStartIndex : aEndIndex;
  PRInt32 end = aStartIndex < aEndIndex ? aEndIndex : aStartIndex;

  if (aAugment && mFirstRange) {
    // We need to remove all the items within our selected range from the selection,
    // and then we insert our new range into the list.
    nsresult rv = mFirstRange->RemoveRange(start, end);
    if (NS_FAILED(rv))
      return rv;
  }

  nsTreeRange* range = new nsTreeRange(this, start, end);
  if (!range)
    return NS_ERROR_OUT_OF_MEMORY;

  range->Invalidate();

  if (aAugment && mFirstRange)
    mFirstRange->Insert(range);
  else
    mFirstRange = range;

  FireOnSelectHandler();

  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::ClearRange(PRInt32 aStartIndex, PRInt32 aEndIndex)
{
  nsresult rv = SetCurrentIndex(aEndIndex);
  if (NS_FAILED(rv))
    return rv;

  if (mFirstRange) {
    PRInt32 start = aStartIndex < aEndIndex ? aStartIndex : aEndIndex;
    PRInt32 end = aStartIndex < aEndIndex ? aEndIndex : aStartIndex;

    mFirstRange->RemoveRange(start, end);

    if (mTree)
      mTree->InvalidateRange(start, end);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::ClearSelection()
{
  if (mFirstRange) {
    mFirstRange->Invalidate();
    delete mFirstRange;
    mFirstRange = nullptr;
  }
  mShiftSelectPivot = -1;

  FireOnSelectHandler();

  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::InvertSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTreeSelection::SelectAll()
{
  if (!mTree)
    return NS_OK;

  nsCOMPtr<nsITreeView> view;
  mTree->GetView(getter_AddRefs(view));
  if (!view)
    return NS_OK;

  PRInt32 rowCount;
  view->GetRowCount(&rowCount);
  bool single;
  nsresult rv = GetSingle(&single);
  if (NS_FAILED(rv))
    return rv;

  if (rowCount == 0 || (rowCount > 1 && single))
    return NS_OK;

  mShiftSelectPivot = -1;

  // Invalidate not necessary when clearing selection, since 
  // we're going to invalidate the world on the SelectAll.
  delete mFirstRange;

  mFirstRange = new nsTreeRange(this, 0, rowCount-1);
  mFirstRange->Invalidate();

  FireOnSelectHandler();

  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::GetRangeCount(PRInt32* aResult)
{
  PRInt32 count = 0;
  nsTreeRange* curr = mFirstRange;
  while (curr) {
    count++;
    curr = curr->mNext;
  }

  *aResult = count;
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::GetRangeAt(PRInt32 aIndex, PRInt32* aMin, PRInt32* aMax)
{
  *aMin = *aMax = -1;
  PRInt32 i = -1;
  nsTreeRange* curr = mFirstRange;
  while (curr) {
    i++;
    if (i == aIndex) {
      *aMin = curr->mMin;
      *aMax = curr->mMax;
      break;
    }
    curr = curr->mNext;
  }

  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::GetCount(PRInt32 *count)
{
  if (mFirstRange)
    *count = mFirstRange->Count();
  else // No range available, so there's no selected row.
    *count = 0;
  
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::GetSelectEventsSuppressed(bool *aSelectEventsSuppressed)
{
  *aSelectEventsSuppressed = mSuppressed;
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::SetSelectEventsSuppressed(bool aSelectEventsSuppressed)
{
  mSuppressed = aSelectEventsSuppressed;
  if (!mSuppressed)
    FireOnSelectHandler();
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::GetCurrentIndex(PRInt32 *aCurrentIndex)
{
  *aCurrentIndex = mCurrentIndex;
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::SetCurrentIndex(PRInt32 aIndex)
{
  if (!mTree) {
    return NS_ERROR_UNEXPECTED;
  }
  if (mCurrentIndex == aIndex) {
    return NS_OK;
  }
  if (mCurrentIndex != -1 && mTree)
    mTree->InvalidateRow(mCurrentIndex);
  
  mCurrentIndex = aIndex;
  if (!mTree)
    return NS_OK;
  
  if (aIndex != -1)
    mTree->InvalidateRow(aIndex);

  // Fire DOMMenuItemActive or DOMMenuItemInactive event for tree.
  nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mTree);
  NS_ASSERTION(boxObject, "no box object!");
  if (!boxObject)
    return NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIDOMElement> treeElt;
  boxObject->GetElement(getter_AddRefs(treeElt));

  nsCOMPtr<nsINode> treeDOMNode(do_QueryInterface(treeElt));
  NS_ENSURE_STATE(treeDOMNode);

  NS_NAMED_LITERAL_STRING(DOMMenuItemActive, "DOMMenuItemActive");
  NS_NAMED_LITERAL_STRING(DOMMenuItemInactive, "DOMMenuItemInactive");

  nsRefPtr<nsAsyncDOMEvent> event =
    new nsAsyncDOMEvent(treeDOMNode,
                     (aIndex != -1 ? DOMMenuItemActive : DOMMenuItemInactive),
                      true, false);
  return event->PostDOMEvent();
}

NS_IMETHODIMP nsTreeSelection::GetCurrentColumn(nsITreeColumn** aCurrentColumn)
{
  NS_IF_ADDREF(*aCurrentColumn = mCurrentColumn);
  return NS_OK;
}

NS_IMETHODIMP nsTreeSelection::SetCurrentColumn(nsITreeColumn* aCurrentColumn)
{
  if (!mTree) {
    return NS_ERROR_UNEXPECTED;
  }
  if (mCurrentColumn == aCurrentColumn) {
    return NS_OK;
  }

  if (mCurrentColumn) {
    if (mFirstRange)
      mTree->InvalidateCell(mFirstRange->mMin, mCurrentColumn);
    if (mCurrentIndex != -1)
      mTree->InvalidateCell(mCurrentIndex, mCurrentColumn);
  }
  
  mCurrentColumn = aCurrentColumn;
  
  if (mCurrentColumn) {
    if (mFirstRange)
      mTree->InvalidateCell(mFirstRange->mMin, mCurrentColumn);
    if (mCurrentIndex != -1)
      mTree->InvalidateCell(mCurrentIndex, mCurrentColumn);
  }

  return NS_OK;
}

#define ADD_NEW_RANGE(macro_range, macro_selection, macro_start, macro_end) \
  { \
    PRInt32 start = macro_start; \
    PRInt32 end = macro_end; \
    if (start > end) { \
      end = start; \
    } \
    nsTreeRange* macro_new_range = new nsTreeRange(macro_selection, start, end); \
    if (macro_range) \
      macro_range->Insert(macro_new_range); \
    else \
      macro_range = macro_new_range; \
  }

NS_IMETHODIMP
nsTreeSelection::AdjustSelection(PRInt32 aIndex, PRInt32 aCount)
{
  NS_ASSERTION(aCount != 0, "adjusting by zero");
  if (!aCount) return NS_OK;

  // adjust mShiftSelectPivot, if necessary
  if ((mShiftSelectPivot != 1) && (aIndex <= mShiftSelectPivot)) {
    // if we are deleting and the delete includes the shift select pivot, reset it
    if (aCount < 0 && (mShiftSelectPivot <= (aIndex -aCount -1))) {
        mShiftSelectPivot = -1;
    }
    else {
        mShiftSelectPivot += aCount;
    }
  }

  // adjust mCurrentIndex, if necessary
  if ((mCurrentIndex != -1) && (aIndex <= mCurrentIndex)) {
    // if we are deleting and the delete includes the current index, reset it
    if (aCount < 0 && (mCurrentIndex <= (aIndex -aCount -1))) {
        mCurrentIndex = -1;
    }
    else {
        mCurrentIndex += aCount;
    }
  }

  // no selection, so nothing to do.
  if (!mFirstRange) return NS_OK;

  bool selChanged = false;
  nsTreeRange* oldFirstRange = mFirstRange;
  nsTreeRange* curr = mFirstRange;
  mFirstRange = nullptr;
  while (curr) {
    if (aCount > 0) {
      // inserting
      if (aIndex > curr->mMax) {
        // adjustment happens after the range, so no change
        ADD_NEW_RANGE(mFirstRange, this, curr->mMin, curr->mMax);
      }
      else if (aIndex <= curr->mMin) {  
        // adjustment happens before the start of the range, so shift down
        ADD_NEW_RANGE(mFirstRange, this, curr->mMin + aCount, curr->mMax + aCount);
        selChanged = true;
      }
      else {
        // adjustment happen inside the range.
        // break apart the range and create two ranges
        ADD_NEW_RANGE(mFirstRange, this, curr->mMin, aIndex - 1);
        ADD_NEW_RANGE(mFirstRange, this, aIndex + aCount, curr->mMax + aCount);
        selChanged = true;
      }
    }
    else {
      // deleting
      if (aIndex > curr->mMax) {
        // adjustment happens after the range, so no change
        ADD_NEW_RANGE(mFirstRange, this, curr->mMin, curr->mMax);
      }
      else {
        // remember, aCount is negative
        selChanged = true;
        PRInt32 lastIndexOfAdjustment = aIndex - aCount - 1;
        if (aIndex <= curr->mMin) {
          if (lastIndexOfAdjustment < curr->mMin) {
            // adjustment happens before the start of the range, so shift up
            ADD_NEW_RANGE(mFirstRange, this, curr->mMin + aCount, curr->mMax + aCount);
          }
          else if (lastIndexOfAdjustment >= curr->mMax) {
            // adjustment contains the range.  remove the range by not adding it to the newRange
          }
          else {
            // adjustment starts before the range, and ends in the middle of it, so trim the range
            ADD_NEW_RANGE(mFirstRange, this, aIndex, curr->mMax + aCount)
          }
        }
        else if (lastIndexOfAdjustment >= curr->mMax) {
         // adjustment starts in the middle of the current range, and contains the end of the range, so trim the range
         ADD_NEW_RANGE(mFirstRange, this, curr->mMin, aIndex - 1)
        }
        else {
          // range contains the adjustment, so shorten the range
          ADD_NEW_RANGE(mFirstRange, this, curr->mMin, curr->mMax + aCount)
        }
      }
    }
    curr = curr->mNext;
  }

  delete oldFirstRange;

  // Fire the select event
  if (selChanged)
    FireOnSelectHandler();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeSelection::InvalidateSelection()
{
  if (mFirstRange)
    mFirstRange->Invalidate();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeSelection::GetShiftSelectPivot(PRInt32* aIndex)
{
  *aIndex = mShiftSelectPivot;
  return NS_OK;
}


nsresult
nsTreeSelection::FireOnSelectHandler()
{
  if (mSuppressed || !mTree)
    return NS_OK;

  nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mTree);
  NS_ASSERTION(boxObject, "no box object!");
  if (!boxObject)
     return NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIDOMElement> elt;
  boxObject->GetElement(getter_AddRefs(elt));
  NS_ENSURE_STATE(elt);

  nsCOMPtr<nsINode> node(do_QueryInterface(elt));
  NS_ENSURE_STATE(node);

  nsRefPtr<nsAsyncDOMEvent> event =
    new nsAsyncDOMEvent(node, NS_LITERAL_STRING("select"), true, false);
  event->RunDOMEventWhenSafe();
  return NS_OK;
}

void
nsTreeSelection::SelectCallback(nsITimer *aTimer, void *aClosure)
{
  nsRefPtr<nsTreeSelection> self = static_cast<nsTreeSelection*>(aClosure);
  if (self) {
    self->FireOnSelectHandler();
    aTimer->Cancel();
    self->mSelectTimer = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewTreeSelection(nsITreeBoxObject* aTree, nsITreeSelection** aResult)
{
  *aResult = new nsTreeSelection(aTree);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
