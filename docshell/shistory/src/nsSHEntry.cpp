/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Radha Kulkarni <radha@netscape.com>
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

// Local Includes
#include "nsSHEntry.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIDocShellLoadInfo.h"

static PRUint32 gEntryID = 0;

//*****************************************************************************
//***    nsSHEntry: Object Management
//*****************************************************************************

nsSHEntry::nsSHEntry() 
  : mLoadType(0)
  , mID(gEntryID++)
  , mScrollPositionX(0)
  , mScrollPositionY(0)
  , mIsFrameNavigation(PR_FALSE)
  , mSaveLayoutState(PR_TRUE)
  , mExpired(PR_FALSE)
  , mParent(nsnull)
{
}

nsSHEntry::nsSHEntry(const nsSHEntry &other)
  : mURI(other.mURI)
  , mReferrerURI(other.mReferrerURI)
  // XXX why not copy mDocument?
  , mTitle(other.mTitle)
  , mPostData(other.mPostData)
  , mLayoutHistoryState(other.mLayoutHistoryState)
  , mLoadType(0)         // XXX why not copy?
  , mID(other.mID)
  , mScrollPositionX(0)  // XXX why not copy?
  , mScrollPositionY(0)  // XXX why not copy?
  , mIsFrameNavigation(other.mIsFrameNavigation)
  , mSaveLayoutState(other.mSaveLayoutState)
  , mExpired(other.mExpired)
  // XXX why not copy mContentType?
  , mCacheKey(other.mCacheKey)
  , mParent(other.mParent)
{
}

//*****************************************************************************
//    nsSHEntry: nsISupports
//*****************************************************************************

NS_IMPL_ISUPPORTS3(nsSHEntry, nsISHContainer, nsISHEntry, nsIHistoryEntry)

//*****************************************************************************
//    nsSHEntry: nsISHEntry
//*****************************************************************************

NS_IMETHODIMP nsSHEntry::SetScrollPosition(PRInt32 x, PRInt32 y)
{
  mScrollPositionX = x;
  mScrollPositionY = y;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetScrollPosition(PRInt32 *x, PRInt32 *y)
{
  *x = mScrollPositionX;
  *y = mScrollPositionY;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetURI(nsIURI** aURI)
{
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
  *aResult = mDocument;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetTitle(PRUnichar** aTitle)
{
  // Check for empty title...
  if (mTitle.IsEmpty() && mURI) {
    // Default title is the URL.
    nsCAutoString spec;
    if (NS_SUCCEEDED(mURI->GetSpec(spec)))
      AppendUTF8toUTF16(spec, mTitle);
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
  *aResult = mLayoutHistoryState;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetLayoutHistoryState(nsILayoutHistoryState* aState)
{
  NS_ENSURE_STATE(mSaveLayoutState || !aState);
  mLayoutHistoryState = aState;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetLoadType(PRUint32 * aResult)
{
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
  *aFlag = mSaveLayoutState;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetSaveLayoutStateFlag(PRBool  aFlag)
{
  mSaveLayoutState = aFlag;

  // if we are not allowed to hold layout history state, then make sure
  // that we are not holding it!
  if (!mSaveLayoutState)
    mLayoutHistoryState = nsnull;

  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetExpirationStatus(PRBool * aFlag)
{
  *aFlag = mExpired;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetExpirationStatus(PRBool  aFlag)
{
  mExpired = aFlag;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetContentType(nsACString& aContentType)
{
  aContentType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetContentType(const nsACString& aContentType)
{
  mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Create(nsIURI * aURI, const PRUnichar * aTitle,
                  nsIDOMDocument * aDOMDocument, nsIInputStream * aInputStream,
                  nsILayoutHistoryState * aHistoryLayoutState,
                  nsISupports * aCacheKey, const nsACString& aContentType)
{
  mURI = aURI;
  mTitle = aTitle;
  mDocument = aDOMDocument;
  mPostData = aInputStream;
  mCacheKey = aCacheKey;
  mContentType = aContentType;
    
  // Set the LoadType by default to loadHistory during creation
  mLoadType = (PRUint32) nsIDocShellLoadInfo::loadHistory;

  // By default all entries are set false for subframe flag. 
  // nsDocShell::CloneAndReplace() which creates entries for
  // all subframe navigations, sets the flag to true.
  mIsFrameNavigation = PR_FALSE;

  // By default we save HistoryLayoutState
  mSaveLayoutState = PR_TRUE;
  mLayoutHistoryState = aHistoryLayoutState;

  //By default the page is not expired
  mExpired = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Clone(nsISHEntry ** aResult)
{
  *aResult = new nsSHEntry(*this);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
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
   *
   * XXX this method should not be scriptable if this is the case!!
   */
  mParent = aParent;
  return NS_OK;
}

//*****************************************************************************
//    nsSHEntry: nsISHContainer
//*****************************************************************************

NS_IMETHODIMP 
nsSHEntry::GetChildCount(PRInt32 * aCount)
{
  *aCount = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::AddChild(nsISHEntry * aChild, PRInt32 aOffset)
{
  NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(aChild->SetParent(this), NS_ERROR_FAILURE);

  //
  // Bug 52670: Ensure children are added in order.
  //
  //  Later frames in the child list may load faster and get appended
  //  before earlier frames, causing session history to be scrambled.
  //  By growing the list here, they are added to the right position.
  //
  //  Assert that aOffset will not be so high as to grow us a lot.
  //
  NS_ASSERTION(aOffset < (mChildren.Count()+1023), "Large frames array!\n");

  // This implicitly extends the array to include aOffset
  mChildren.ReplaceObjectAt(aChild, aOffset);

  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::RemoveChild(nsISHEntry * aChild)
{
  NS_ENSURE_TRUE(aChild, NS_ERROR_FAILURE);
  PRBool childRemoved = mChildren.RemoveObject(aChild);
  if (childRemoved)
    aChild->SetParent(nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::GetChildAt(PRInt32 aIndex, nsISHEntry ** aResult)
{
  if (aIndex >= 0 && aIndex < mChildren.Count()) {
    *aResult = mChildren[aIndex];
    // yes, mChildren can have holes in it.  AddChild's offset parameter makes
    // that possible.
    NS_IF_ADDREF(*aResult);
  } else {
    *aResult = nsnull;
  }
  return NS_OK;
}
