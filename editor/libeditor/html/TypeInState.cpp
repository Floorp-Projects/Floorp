/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "TypeInState.h"
#include "nsEditor.h"

/********************************************************************
 *                     XPCOM cruft 
 *******************************************************************/

NS_IMPL_ISUPPORTS1(TypeInState, nsISelectionListener)

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
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  
  PRBool isCollapsed = PR_FALSE;
  nsresult result = aSelection->GetIsCollapsed(&isCollapsed);

  if (NS_FAILED(result)) return result;

  if (isCollapsed)
  {
    result = nsEditor::GetStartNodeAndOffset(aSelection, address_of(mLastSelectionContainer), &mLastSelectionOffset);
  }
  return result;
}


NS_IMETHODIMP TypeInState::NotifySelectionChanged(nsIDOMDocument *, nsISelection *aSelection, short)
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

  if (aSelection)
  {
    PRBool isCollapsed = PR_FALSE;
    nsresult result = aSelection->GetIsCollapsed(&isCollapsed);

    if (NS_FAILED(result)) return result;

    if (isCollapsed)
    {
      nsCOMPtr<nsIDOMNode> selNode;
      PRInt32 selOffset = 0;

      result = nsEditor::GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);

      if (NS_FAILED(result)) return result;

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
  PRInt32 count;
  PropItem *propItemPtr;
  
  while ((count = mClearedArray.Count()))
  {
    // go backwards to keep nsVoidArray from memmoving everything each time
    count--; // nsVoidArray is zero based
    propItemPtr = (PropItem*)mClearedArray.ElementAt(count);
    mClearedArray.RemoveElementAt(count);
    if (propItemPtr) delete propItemPtr;
  }
  while ((count = mSetArray.Count()))
  {
    // go backwards to keep nsVoidArray from memmoving everything each time
    count--; // nsVoidArray is zero based
    propItemPtr = (PropItem*)mSetArray.ElementAt(count);
    mSetArray.RemoveElementAt(count);
    if (propItemPtr) delete propItemPtr;
  }
}


nsresult TypeInState::SetProp(nsIAtom *aProp)
{
  return SetProp(aProp,EmptyString(),EmptyString());
}

nsresult TypeInState::SetProp(nsIAtom *aProp, const nsString &aAttr)
{
  return SetProp(aProp,aAttr,EmptyString());
}

nsresult TypeInState::SetProp(nsIAtom *aProp, const nsString &aAttr, const nsString &aValue)
{
  // special case for big/small, these nest
  if (nsEditProperty::big == aProp)
  {
    mRelativeFontSize++;
    return NS_OK;
  }
  if (nsEditProperty::small == aProp)
  {
    mRelativeFontSize--;
    return NS_OK;
  }

  PRInt32 index;
  PropItem *item;

  if (IsPropSet(aProp,aAttr,nsnull,index))
  {
    // if it's already set, update the value
    item = (PropItem*)mSetArray[index];
    item->value = aValue;
  }
  else 
  {
    // make a new propitem
    item = new PropItem(aProp,aAttr,aValue);
    if (!item) return NS_ERROR_OUT_OF_MEMORY;
    
    // add it to the list of set properties
    mSetArray.AppendElement((void*)item);
    
    // remove it from the list of cleared properties, if we have a match
    RemovePropFromClearedList(aProp,aAttr);  
  }
    
  return NS_OK;
}


nsresult TypeInState::ClearAllProps()
{
  // null prop means "all" props
  return ClearProp(nsnull,EmptyString());
}

nsresult TypeInState::ClearProp(nsIAtom *aProp)
{
  return ClearProp(aProp,EmptyString());
}

nsresult TypeInState::ClearProp(nsIAtom *aProp, const nsString &aAttr)
{
  // if it's already cleared we are done
  if (IsPropCleared(aProp,aAttr)) return NS_OK;
  
  // make a new propitem
  PropItem *item = new PropItem(aProp,aAttr,nsAutoString());
  if (!item) return NS_ERROR_OUT_OF_MEMORY;
  
  // remove it from the list of set properties, if we have a match
  RemovePropFromSetList(aProp,aAttr);
  
  // add it to the list of cleared properties
  mClearedArray.AppendElement((void*)item);
  
  return NS_OK;
}


/***************************************************************************
 *    TakeClearProperty: hands back next property item on the clear list.
 *                       caller assumes ownership of PropItem and must delete it.
 */  
nsresult TypeInState::TakeClearProperty(PropItem **outPropItem)
{
  if (!outPropItem) return NS_ERROR_NULL_POINTER;
  *outPropItem = nsnull;
  PRInt32 count = mClearedArray.Count();
  if (count) // go backwards to keep nsVoidArray from memmoving everything each time
  {
    count--; // nsVoidArray is zero based
    *outPropItem = (PropItem*)mClearedArray[count];
    mClearedArray.RemoveElementAt(count);
  }
  return NS_OK;
}

/***************************************************************************
 *    TakeSetProperty: hands back next poroperty item on the set list.
 *                     caller assumes ownership of PropItem and must delete it.
 */  
nsresult TypeInState::TakeSetProperty(PropItem **outPropItem)
{
  if (!outPropItem) return NS_ERROR_NULL_POINTER;
  *outPropItem = nsnull;
  PRInt32 count = mSetArray.Count();
  if (count) // go backwards to keep nsVoidArray from memmoving everything each time
  {
    count--; // nsVoidArray is zero based
    *outPropItem = (PropItem*)mSetArray[count];
    mSetArray.RemoveElementAt(count);
  }
  return NS_OK;
}

//**************************************************************************
//    TakeRelativeFontSize: hands back relative font value, which is then
//                          cleared out.
nsresult TypeInState::TakeRelativeFontSize(PRInt32 *outRelSize)
{
  if (!outRelSize) return NS_ERROR_NULL_POINTER;
  *outRelSize = mRelativeFontSize;
  mRelativeFontSize = 0;
  return NS_OK;
}

nsresult TypeInState::GetTypingState(PRBool &isSet, PRBool &theSetting, nsIAtom *aProp)
{
  return GetTypingState(isSet, theSetting, aProp, EmptyString(), nsnull);
}

nsresult TypeInState::GetTypingState(PRBool &isSet, 
                                     PRBool &theSetting, 
                                     nsIAtom *aProp, 
                                     const nsString &aAttr)
{
  return GetTypingState(isSet, theSetting, aProp, aAttr, nsnull);
}


nsresult TypeInState::GetTypingState(PRBool &isSet, 
                                     PRBool &theSetting, 
                                     nsIAtom *aProp,
                                     const nsString &aAttr, 
                                     nsString *aValue)
{
  if (IsPropSet(aProp, aAttr, aValue))
  {
    isSet = PR_TRUE;
    theSetting = PR_TRUE;
  }
  else if (IsPropCleared(aProp, aAttr))
  {
    isSet = PR_TRUE;
    theSetting = PR_FALSE;
  }
  else
  {
    isSet = PR_FALSE;
  }
  return NS_OK;
}



/********************************************************************
 *                   protected methods
 *******************************************************************/
 
nsresult TypeInState::RemovePropFromSetList(nsIAtom *aProp, 
                                            const nsString &aAttr)
{
  PRInt32 index;
  PropItem *item;
  if (!aProp)
  {
    // clear _all_ props
    mRelativeFontSize=0;
    while ((index = mSetArray.Count()))
    {
      // go backwards to keep nsVoidArray from memmoving everything each time
      index--; // nsVoidArray is zero based
      item = (PropItem*)mSetArray.ElementAt(index);
      mSetArray.RemoveElementAt(index);
      if (item) delete item;
    }
  }
  else if (FindPropInList(aProp, aAttr, nsnull, mSetArray, index))
  {
    item = (PropItem*)mSetArray.ElementAt(index);
    mSetArray.RemoveElementAt(index);
    if (item) delete item;
  }
  return NS_OK;
}


nsresult TypeInState::RemovePropFromClearedList(nsIAtom *aProp, 
                                            const nsString &aAttr)
{
  PRInt32 index;
  if (FindPropInList(aProp, aAttr, nsnull, mClearedArray, index))
  {
    PropItem *item = (PropItem*)mClearedArray.ElementAt(index);
    mClearedArray.RemoveElementAt(index);
    if (item) delete item;
  }
  return NS_OK;
}


PRBool TypeInState::IsPropSet(nsIAtom *aProp, 
                              const nsString &aAttr,
                              nsString* outValue)
{
  PRInt32 i;
  return IsPropSet(aProp, aAttr, outValue, i);
}


PRBool TypeInState::IsPropSet(nsIAtom *aProp, 
                              const nsString &aAttr,
                              nsString *outValue,
                              PRInt32 &outIndex)
{
  // linear search.  list should be short.
  PRInt32 i, count = mSetArray.Count();
  for (i=0; i<count; i++)
  {
    PropItem *item = (PropItem*)mSetArray[i];
    if ( (item->tag == aProp) &&
         (item->attr == aAttr) )
    {
      if (outValue) *outValue = item->value;
      outIndex = i;
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


PRBool TypeInState::IsPropCleared(nsIAtom *aProp, 
                                  const nsString &aAttr)
{
  PRInt32 i;
  return IsPropCleared(aProp, aAttr, i);
}


PRBool TypeInState::IsPropCleared(nsIAtom *aProp, 
                                  const nsString &aAttr,
                                  PRInt32 &outIndex)
{
  if (FindPropInList(aProp, aAttr, nsnull, mClearedArray, outIndex))
    return PR_TRUE;
  if (FindPropInList(0, nsAutoString(), nsnull, mClearedArray, outIndex))
  {
    // special case for all props cleared
    outIndex = -1;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool TypeInState::FindPropInList(nsIAtom *aProp, 
                                   const nsAString &aAttr,
                                   nsAString *outValue,
                                   nsVoidArray &aList,
                                   PRInt32 &outIndex)
{
  // linear search.  list should be short.
  PRInt32 i, count = aList.Count();
  for (i=0; i<count; i++)
  {
    PropItem *item = (PropItem*)aList[i];
    if ( (item->tag == aProp) &&
         (item->attr == aAttr) ) 
    {
      if (outValue) *outValue = item->value;
      outIndex = i;
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}



/********************************************************************
 *    PropItem: helper struct for TypeInState
 *******************************************************************/

PropItem::PropItem(nsIAtom *aTag, const nsAString &aAttr, const nsAString &aValue) :
 tag(aTag)
,attr(aAttr)
,value(aValue)
{
}

PropItem::~PropItem()
{
}
