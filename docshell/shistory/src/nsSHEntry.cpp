/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Radha Kulkarni <radha@netscape.com>
 */

// Local Includes
#include "nsSHEntry.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIDocShellLoadInfo.h"

PRUint32 gEntryID=0;
//*****************************************************************************
//***    nsSHEntry: Object Management
//*****************************************************************************

nsSHEntry::nsSHEntry() 
{
   NS_INIT_REFCNT();
   mParent = nsnull;
   mID = gEntryID++;
}

nsSHEntry::~nsSHEntry() 
{
  // Release the references to any child entries...
  PRInt32 i, childCount = mChildren.Count();
  for (i=0; i<childCount; i++) {
    nsISHEntry* child;

    child = (nsISHEntry*) mChildren.ElementAt(i);
    NS_IF_RELEASE(child);
  }

  mChildren.Clear();
}

//*****************************************************************************
//    nsSHEntry: nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsSHEntry)
NS_IMPL_RELEASE(nsSHEntry)

NS_INTERFACE_MAP_BEGIN(nsSHEntry)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISHEntry)
  NS_INTERFACE_MAP_ENTRY(nsISHContainer)
  NS_INTERFACE_MAP_ENTRY(nsISHEntry)
  NS_INTERFACE_MAP_ENTRY(nsIHistoryEntry)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    nsSHEntry: nsISHEntry
//*****************************************************************************

NS_IMETHODIMP nsSHEntry::GetURI(nsIURI** aURI)
{
   NS_ENSURE_ARG_POINTER(aURI);
  
   *aURI = mURI;
   NS_IF_ADDREF(*aURI);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetURI(nsIURI* aURI)
{
   mURI = aURI;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetReferrerURI(nsIURI **aReferrerURI)
{
   NS_ENSURE_ARG_POINTER(aReferrerURI);
   *aReferrerURI = mReferrerURI;
   NS_IF_ADDREF(*aReferrerURI);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetReferrerURI(nsIURI *aReferrerURI)
{
   mReferrerURI = aReferrerURI;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetDocument(nsIDOMDocument* aDocument)
{
   mDocument = aDocument;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetDocument(nsIDOMDocument** aResult)
{
	NS_ENSURE_ARG_POINTER(aResult);

   *aResult = mDocument;
   NS_IF_ADDREF(*aResult);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);

   // Check for empty title...
   if ( mTitle.IsEmpty() && mURI ) {
      // Default title is the URL.
      nsXPIDLCString spec;
      if ( NS_SUCCEEDED( mURI->GetSpec( getter_Copies( spec ) ) ) ) {
          mTitle.AssignWithConversion( spec ); 
      }
   }

   *aTitle = ToNewUnicode(mTitle);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetTitle(const PRUnichar* aTitle)
{
   mTitle = aTitle;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetPostData(nsIInputStream** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);

   *aResult = mPostData;
   NS_IF_ADDREF(*aResult);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetPostData(nsIInputStream* aPostData)
{
   mPostData = aPostData;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetLayoutHistoryState(nsILayoutHistoryState** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);
   
   *aResult = mLayoutHistoryState;
   NS_IF_ADDREF(*aResult);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetLayoutHistoryState(nsILayoutHistoryState* aState)
{
   mLayoutHistoryState = aState;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetLoadType(PRUint32 * aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);
   
   *aResult = mLoadType;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetLoadType(PRUint32  aLoadType)
{
   mLoadType = aLoadType;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetID(PRUint32 * aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);
   
   *aResult = mID;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetID(PRUint32  aID)
{
   mID = aID;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetIsSubFrame(PRBool * aFlag)
{
   NS_ENSURE_ARG_POINTER(aFlag);
   
   *aFlag = mIsFrameNavigation;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetIsSubFrame(PRBool  aFlag)
{
   mIsFrameNavigation = aFlag;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetCacheKey(nsISupports** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);

   *aResult = mCacheKey;
   NS_IF_ADDREF(*aResult);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetCacheKey(nsISupports* aCacheKey)
{
   mCacheKey = aCacheKey;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetSaveLayoutStateFlag(PRBool * aFlag)
{
   NS_ENSURE_ARG_POINTER(aFlag);
   
   *aFlag = mSaveLayoutState;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetSaveLayoutStateFlag(PRBool  aFlag)
{
   mSaveLayoutState = aFlag;
   return NS_OK;
}


NS_IMETHODIMP nsSHEntry::GetExpirationStatus(PRBool * aFlag)
{
   NS_ENSURE_ARG_POINTER(aFlag);
   
   *aFlag = mExpired;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetExpirationStatus(PRBool  aFlag)
{
   mExpired = aFlag;
   return NS_OK;
}

nsresult
nsSHEntry::Create(nsIURI * aURI, const PRUnichar * aTitle, nsIDOMDocument * aDOMDocument,
			         nsIInputStream * aInputStream, nsILayoutHistoryState * aHistoryLayoutState,
               nsISupports * aCacheKey)
{
  SetURI(aURI);
	SetTitle(aTitle);
	SetDocument(aDOMDocument);
	SetPostData(aInputStream);
	SetLayoutHistoryState(aHistoryLayoutState);
	SetCacheKey(aCacheKey);

  // Set the LoadType by default to loadHistory during creation
	SetLoadType((PRInt32)nsIDocShellLoadInfo::loadHistory);

  // By default all entries are set false for subframe flag. 
  // nsDocShell::CloneAndReplace() which creates entries for
  // all subframe navigations, sets the flag to true.
  SetIsSubFrame(PR_FALSE);

  // By default we save HistoryLayoutState
  SetSaveLayoutStateFlag(PR_TRUE);

  //By default the page is not expired
  SetExpirationStatus(PR_FALSE);

	return NS_OK;
	
}

NS_IMETHODIMP
nsSHEntry::GetParent(nsISHEntry ** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mParent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


NS_IMETHODIMP
nsSHEntry::SetParent(nsISHEntry * aParent)
{
	/* parent not Addrefed on purpose to avoid cyclic reference
	 * Null parent is OK
	 */
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP 
nsSHEntry::GetChildCount(PRInt32 * aCount)
{
	NS_ENSURE_ARG_POINTER(aCount);
    *aCount = mChildren.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AddChild(nsISHEntry * aChild, PRInt32 aOffset)
{
    NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);

	NS_ENSURE_SUCCESS(aChild->SetParent(this), NS_ERROR_FAILURE);
	PRInt32 childCount = mChildren.Count();
    //
    // Bug 52670: Ensure children are added in order.
    //
    //  Later frames in the child list may load faster and get appended
    //  before earlier frames, causing session history to be scrambled.
    //  By growing the list here, they are added to the right position.
    //
    //  Assert that aOffset will not be so high as to grow us a lot.
    //
    NS_ASSERTION(aOffset < (childCount + 1023), "Large frames array!\n");

    // This implicitly extends the array to include aOffset
    mChildren.ReplaceElementAt(aChild, aOffset);

	NS_ADDREF(aChild);

    return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::RemoveChild(nsISHEntry * aChild)
{
    NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);
	PRBool childRemoved = mChildren.RemoveElement((void *)aChild);
	if (childRemoved) {
	  aChild->SetParent(nsnull);
	  NS_RELEASE(aChild);
	}
    return NS_OK;
}


NS_IMETHODIMP
nsSHEntry::GetChildAt(PRInt32 aIndex, nsISHEntry ** aResult)
{
	NS_ENSURE_ARG_POINTER(aResult);
    if (PRUint32(aIndex) >= PRUint32(mChildren.Count())) {
      *aResult = nsnull;
	}
    else {
      *aResult = (nsISHEntry*) mChildren.ElementAt(aIndex);
      NS_IF_ADDREF(*aResult);
	}
    return NS_OK;
}

