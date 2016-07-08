/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TypeInState.h"

#include <stddef.h>

#include "nsError.h"
#include "mozilla/EditorBase.h"
#include "mozilla/mozalloc.h"
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsDebug.h"
#include "nsGkAtoms.h"
#include "nsIDOMNode.h"
#include "nsISupportsBase.h"
#include "nsISupportsImpl.h"
#include "nsReadableUtils.h"
#include "nsStringFwd.h"

class nsIAtom;
class nsIDOMDocument;

namespace mozilla {

using namespace dom;

/********************************************************************
 * mozilla::TypeInState
 *******************************************************************/

NS_IMPL_CYCLE_COLLECTION(TypeInState, mLastSelectionContainer)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TypeInState)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TypeInState)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TypeInState)
  NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TypeInState::TypeInState()
  : mRelativeFontSize(0)
  , mLastSelectionOffset(0)
{
  Reset();
}

TypeInState::~TypeInState()
{
  // Call Reset() to release any data that may be in
  // mClearedArray and mSetArray.

  Reset();
}

nsresult
TypeInState::UpdateSelState(Selection* aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);

  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  return EditorBase::GetStartNodeAndOffset(
                       aSelection, getter_AddRefs(mLastSelectionContainer),
                       &mLastSelectionOffset);
}


NS_IMETHODIMP
TypeInState::NotifySelectionChanged(nsIDOMDocument* aDOMDocument,
                                    nsISelection* aSelection,
                                    int16_t aReason)
{
  // XXX: Selection currently generates bogus selection changed notifications
  // XXX: (bug 140303). It can notify us when the selection hasn't actually
  // XXX: changed, and it notifies us more than once for the same change.
  // XXX:
  // XXX: The following code attempts to work around the bogus notifications,
  // XXX: and should probably be removed once bug 140303 is fixed.
  // XXX:
  // XXX: This code temporarily fixes the problem where clicking the mouse in
  // XXX: the same location clears the type-in-state.
  RefPtr<Selection> selection =
    aSelection ? aSelection->AsSelection() : nullptr;

  if (aSelection) {
    int32_t rangeCount = selection->RangeCount();

    if (selection->Collapsed() && rangeCount) {
      nsCOMPtr<nsIDOMNode> selNode;
      int32_t selOffset = 0;

      nsresult result =
        EditorBase::GetStartNodeAndOffset(selection, getter_AddRefs(selNode),
                                          &selOffset);

      NS_ENSURE_SUCCESS(result, result);

      if (selNode && selNode == mLastSelectionContainer && selOffset == mLastSelectionOffset)
      {
        // We got a bogus selection changed notification!
        return NS_OK;
      }

      mLastSelectionContainer = selNode;
      mLastSelectionOffset = selOffset;
    }
    else
    {
      mLastSelectionContainer = nullptr;
      mLastSelectionOffset = 0;
    }
  }

  Reset();
  return NS_OK;
}

void
TypeInState::Reset()
{
  for(uint32_t i = 0, n = mClearedArray.Length(); i < n; i++) {
    delete mClearedArray[i];
  }
  mClearedArray.Clear();
  for(uint32_t i = 0, n = mSetArray.Length(); i < n; i++) {
    delete mSetArray[i];
  }
  mSetArray.Clear();
}


void
TypeInState::SetProp(nsIAtom* aProp,
                     const nsAString& aAttr,
                     const nsAString& aValue)
{
  // special case for big/small, these nest
  if (nsGkAtoms::big == aProp) {
    mRelativeFontSize++;
    return;
  }
  if (nsGkAtoms::small == aProp) {
    mRelativeFontSize--;
    return;
  }

  int32_t index;
  if (IsPropSet(aProp, aAttr, nullptr, index)) {
    // if it's already set, update the value
    mSetArray[index]->value = aValue;
    return;
  }

  // Make a new propitem and add it to the list of set properties.
  mSetArray.AppendElement(new PropItem(aProp, aAttr, aValue));

  // remove it from the list of cleared properties, if we have a match
  RemovePropFromClearedList(aProp, aAttr);
}


void
TypeInState::ClearAllProps()
{
  // null prop means "all" props
  ClearProp(nullptr, EmptyString());
}

void
TypeInState::ClearProp(nsIAtom* aProp,
                       const nsAString& aAttr)
{
  // if it's already cleared we are done
  if (IsPropCleared(aProp, aAttr)) {
    return;
  }

  // make a new propitem
  PropItem* item = new PropItem(aProp, aAttr, EmptyString());

  // remove it from the list of set properties, if we have a match
  RemovePropFromSetList(aProp, aAttr);

  // add it to the list of cleared properties
  mClearedArray.AppendElement(item);
}


/**
 * TakeClearProperty() hands back next property item on the clear list.
 * Caller assumes ownership of PropItem and must delete it.
 */
PropItem*
TypeInState::TakeClearProperty()
{
  uint32_t count = mClearedArray.Length();
  if (!count) {
    return nullptr;
  }

  --count; // indices are zero based
  PropItem* propItem = mClearedArray[count];
  mClearedArray.RemoveElementAt(count);
  return propItem;
}

/**
 * TakeSetProperty() hands back next poroperty item on the set list.
 * Caller assumes ownership of PropItem and must delete it.
 */
PropItem*
TypeInState::TakeSetProperty()
{
  uint32_t count = mSetArray.Length();
  if (!count) {
    return nullptr;
  }
  count--; // indices are zero based
  PropItem* propItem = mSetArray[count];
  mSetArray.RemoveElementAt(count);
  return propItem;
}

/**
 * TakeRelativeFontSize() hands back relative font value, which is then
 * cleared out.
 */
int32_t
TypeInState::TakeRelativeFontSize()
{
  int32_t relSize = mRelativeFontSize;
  mRelativeFontSize = 0;
  return relSize;
}

void
TypeInState::GetTypingState(bool& isSet,
                            bool& theSetting,
                            nsIAtom* aProp)
{
  GetTypingState(isSet, theSetting, aProp, EmptyString(), nullptr);
}

void
TypeInState::GetTypingState(bool& isSet,
                            bool& theSetting,
                            nsIAtom* aProp,
                            const nsString& aAttr,
                            nsString* aValue)
{
  if (IsPropSet(aProp, aAttr, aValue))
  {
    isSet = true;
    theSetting = true;
  }
  else if (IsPropCleared(aProp, aAttr))
  {
    isSet = true;
    theSetting = false;
  }
  else
  {
    isSet = false;
  }
}

void
TypeInState::RemovePropFromSetList(nsIAtom* aProp,
                                   const nsAString& aAttr)
{
  int32_t index;
  if (!aProp)
  {
    // clear _all_ props
    for(uint32_t i = 0, n = mSetArray.Length(); i < n; i++) {
      delete mSetArray[i];
    }
    mSetArray.Clear();
    mRelativeFontSize=0;
  }
  else if (FindPropInList(aProp, aAttr, nullptr, mSetArray, index))
  {
    delete mSetArray[index];
    mSetArray.RemoveElementAt(index);
  }
}


void
TypeInState::RemovePropFromClearedList(nsIAtom* aProp,
                                       const nsAString& aAttr)
{
  int32_t index;
  if (FindPropInList(aProp, aAttr, nullptr, mClearedArray, index))
  {
    delete mClearedArray[index];
    mClearedArray.RemoveElementAt(index);
  }
}


bool
TypeInState::IsPropSet(nsIAtom* aProp,
                       const nsAString& aAttr,
                       nsAString* outValue)
{
  int32_t i;
  return IsPropSet(aProp, aAttr, outValue, i);
}


bool
TypeInState::IsPropSet(nsIAtom* aProp,
                       const nsAString& aAttr,
                       nsAString* outValue,
                       int32_t& outIndex)
{
  // linear search.  list should be short.
  uint32_t i, count = mSetArray.Length();
  for (i=0; i<count; i++)
  {
    PropItem *item = mSetArray[i];
    if ( (item->tag == aProp) &&
         (item->attr == aAttr) )
    {
      if (outValue) *outValue = item->value;
      outIndex = i;
      return true;
    }
  }
  return false;
}


bool
TypeInState::IsPropCleared(nsIAtom* aProp,
                           const nsAString& aAttr)
{
  int32_t i;
  return IsPropCleared(aProp, aAttr, i);
}


bool
TypeInState::IsPropCleared(nsIAtom* aProp,
                           const nsAString& aAttr,
                           int32_t& outIndex)
{
  if (FindPropInList(aProp, aAttr, nullptr, mClearedArray, outIndex))
    return true;
  if (FindPropInList(0, EmptyString(), nullptr, mClearedArray, outIndex))
  {
    // special case for all props cleared
    outIndex = -1;
    return true;
  }
  return false;
}

bool
TypeInState::FindPropInList(nsIAtom* aProp,
                            const nsAString& aAttr,
                            nsAString* outValue,
                            nsTArray<PropItem*>& aList,
                            int32_t& outIndex)
{
  // linear search.  list should be short.
  uint32_t i, count = aList.Length();
  for (i=0; i<count; i++)
  {
    PropItem *item = aList[i];
    if ( (item->tag == aProp) &&
         (item->attr == aAttr) )
    {
      if (outValue) *outValue = item->value;
      outIndex = i;
      return true;
    }
  }
  return false;
}

/********************************************************************
 * mozilla::PropItem: helper struct for mozilla::TypeInState
 *******************************************************************/

PropItem::PropItem()
  : tag(nullptr)
{
  MOZ_COUNT_CTOR(PropItem);
}

PropItem::PropItem(nsIAtom* aTag,
                   const nsAString& aAttr,
                   const nsAString &aValue)
  : tag(aTag)
  , attr(aAttr)
  , value(aValue)
{
  MOZ_COUNT_CTOR(PropItem);
}

PropItem::~PropItem()
{
  MOZ_COUNT_DTOR(PropItem);
}

} // namespace mozilla
