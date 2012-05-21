/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "TypeInState.h"
#include "nsEditor.h"

/********************************************************************
 *                     XPCOM cruft 
 *******************************************************************/

NS_IMPL_CYCLE_COLLECTION_1(TypeInState, mLastSelectionContainer)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TypeInState)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TypeInState)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TypeInState)
  NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/********************************************************************
 *                   public methods
 *******************************************************************/
 
TypeInState::TypeInState() :
 mSetArray()
,mClearedArray()
,mRelativeFontSize(0)
,mLastSelectionOffset(0)
{
  Reset();
}

TypeInState::~TypeInState()
{
  // Call Reset() to release any data that may be in
  // mClearedArray and mSetArray.

  Reset();
}

nsresult TypeInState::UpdateSelState(nsISelection *aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  return nsEditor::GetStartNodeAndOffset(aSelection, getter_AddRefs(mLastSelectionContainer), &mLastSelectionOffset);
}


NS_IMETHODIMP TypeInState::NotifySelectionChanged(nsIDOMDocument *, nsISelection *aSelection, PRInt16)
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

  if (aSelection) {
    PRInt32 rangeCount = 0;
    nsresult result = aSelection->GetRangeCount(&rangeCount);
    NS_ENSURE_SUCCESS(result, result);

    if (aSelection->Collapsed() && rangeCount) {
      nsCOMPtr<nsIDOMNode> selNode;
      PRInt32 selOffset = 0;

      result = nsEditor::GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);

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
      mLastSelectionContainer = nsnull;
      mLastSelectionOffset = 0;
    }
  }

  Reset(); 
  return NS_OK;
}

void TypeInState::Reset()
{
  for(PRUint32 i = 0, n = mClearedArray.Length(); i < n; i++) {
    delete mClearedArray[i];
  }
  mClearedArray.Clear();
  for(PRUint32 i = 0, n = mSetArray.Length(); i < n; i++) {
    delete mSetArray[i];
  }
  mSetArray.Clear();
}


void
TypeInState::SetProp(nsIAtom* aProp, const nsAString& aAttr,
                     const nsAString& aValue)
{
  // special case for big/small, these nest
  if (nsEditProperty::big == aProp) {
    mRelativeFontSize++;
    return;
  }
  if (nsEditProperty::small == aProp) {
    mRelativeFontSize--;
    return;
  }

  PRInt32 index;
  if (IsPropSet(aProp, aAttr, NULL, index)) {
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
  ClearProp(nsnull, EmptyString());
}

void
TypeInState::ClearProp(nsIAtom* aProp, const nsAString& aAttr)
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


/***************************************************************************
 *    TakeClearProperty: hands back next property item on the clear list.
 *                       caller assumes ownership of PropItem and must delete it.
 */  
PropItem*
TypeInState::TakeClearProperty()
{
  PRUint32 count = mClearedArray.Length();
  if (!count) {
    return NULL;
  }

  --count; // indices are zero based
  PropItem* propItem = mClearedArray[count];
  mClearedArray.RemoveElementAt(count);
  return propItem;
}

/***************************************************************************
 *    TakeSetProperty: hands back next poroperty item on the set list.
 *                     caller assumes ownership of PropItem and must delete it.
 */  
PropItem*
TypeInState::TakeSetProperty()
{
  PRUint32 count = mSetArray.Length();
  if (!count) {
    return NULL;
  }
  count--; // indices are zero based
  PropItem* propItem = mSetArray[count];
  mSetArray.RemoveElementAt(count);
  return propItem;
}

//**************************************************************************
//    TakeRelativeFontSize: hands back relative font value, which is then
//                          cleared out.
PRInt32
TypeInState::TakeRelativeFontSize()
{
  PRInt32 relSize = mRelativeFontSize;
  mRelativeFontSize = 0;
  return relSize;
}

nsresult TypeInState::GetTypingState(bool &isSet, bool &theSetting, nsIAtom *aProp)
{
  return GetTypingState(isSet, theSetting, aProp, EmptyString(), nsnull);
}

nsresult TypeInState::GetTypingState(bool &isSet, 
                                     bool &theSetting, 
                                     nsIAtom *aProp,
                                     const nsString &aAttr, 
                                     nsString *aValue)
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
  return NS_OK;
}



/********************************************************************
 *                   protected methods
 *******************************************************************/
 
nsresult TypeInState::RemovePropFromSetList(nsIAtom* aProp,
                                            const nsAString& aAttr)
{
  PRInt32 index;
  if (!aProp)
  {
    // clear _all_ props
    for(PRUint32 i = 0, n = mSetArray.Length(); i < n; i++) {
      delete mSetArray[i];
    }
    mSetArray.Clear();
    mRelativeFontSize=0;
  }
  else if (FindPropInList(aProp, aAttr, nsnull, mSetArray, index))
  {
    delete mSetArray[index];
    mSetArray.RemoveElementAt(index);
  }
  return NS_OK;
}


nsresult TypeInState::RemovePropFromClearedList(nsIAtom* aProp,
                                                const nsAString& aAttr)
{
  PRInt32 index;
  if (FindPropInList(aProp, aAttr, nsnull, mClearedArray, index))
  {
    delete mClearedArray[index];
    mClearedArray.RemoveElementAt(index);
  }
  return NS_OK;
}


bool TypeInState::IsPropSet(nsIAtom *aProp,
                            const nsAString& aAttr,
                            nsAString* outValue)
{
  PRInt32 i;
  return IsPropSet(aProp, aAttr, outValue, i);
}


bool TypeInState::IsPropSet(nsIAtom* aProp,
                            const nsAString& aAttr,
                            nsAString* outValue,
                            PRInt32& outIndex)
{
  // linear search.  list should be short.
  PRUint32 i, count = mSetArray.Length();
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


bool TypeInState::IsPropCleared(nsIAtom* aProp,
                                const nsAString& aAttr)
{
  PRInt32 i;
  return IsPropCleared(aProp, aAttr, i);
}


bool TypeInState::IsPropCleared(nsIAtom* aProp,
                                const nsAString& aAttr,
                                PRInt32& outIndex)
{
  if (FindPropInList(aProp, aAttr, nsnull, mClearedArray, outIndex))
    return true;
  if (FindPropInList(0, EmptyString(), nsnull, mClearedArray, outIndex))
  {
    // special case for all props cleared
    outIndex = -1;
    return true;
  }
  return false;
}

bool TypeInState::FindPropInList(nsIAtom *aProp, 
                                   const nsAString &aAttr,
                                   nsAString *outValue,
                                   nsTArray<PropItem*> &aList,
                                   PRInt32 &outIndex)
{
  // linear search.  list should be short.
  PRUint32 i, count = aList.Length();
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
 *    PropItem: helper struct for TypeInState
 *******************************************************************/

PropItem::PropItem() : 
 tag(nsnull)
,attr()
,value()
{
  MOZ_COUNT_CTOR(PropItem);
}

PropItem::PropItem(nsIAtom *aTag, const nsAString &aAttr, const nsAString &aValue) :
 tag(aTag)
,attr(aAttr)
,value(aValue)
{
  MOZ_COUNT_CTOR(PropItem);
}

PropItem::~PropItem()
{
  MOZ_COUNT_DTOR(PropItem);
}
