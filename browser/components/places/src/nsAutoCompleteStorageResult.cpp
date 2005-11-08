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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAutoCompleteStorageResult.h"

#include "nsCOMPtr.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(nsAutoCompleteStorageMatch, nsIAutoCompleteStorageMatch)

NS_IMETHODIMP
nsAutoCompleteStorageMatch::GetValue(nsAString& aValue)
{
  aValue = mValue;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageMatch::SetValue(const nsAString& aValue)
{
  mValue = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageMatch::GetComment(nsAString& aComment)
{
  aComment = mComment;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageMatch::SetComment(const nsAString& aComment)
{
  mComment = aComment;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageMatch::GetPrivatePriority(PRInt32* aPrivatePriority)
{
  *aPrivatePriority = mPrivatePriority;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageMatch::SetPrivatePriority(PRInt32 aPrivatePriority)
{
  mPrivatePriority = aPrivatePriority;
  return NS_OK;
}


NS_IMPL_ISUPPORTS3(nsAutoCompleteStorageResult,
                   nsIAutoCompleteResult,
                   nsIAutoCompleteBaseResult,
                   nsIAutoCompleteStorageResult)

nsAutoCompleteStorageResult::nsAutoCompleteStorageResult()
{
}

nsAutoCompleteStorageResult::~nsAutoCompleteStorageResult()
{
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::AppendMatch(nsIAutoCompleteStorageMatch *entry)
{
  mResults.AppendObject(entry);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::GetMatchAt(PRUint32 matchIndex,
                                        nsIAutoCompleteStorageMatch **_retval)
{
  NS_ENSURE_TRUE(matchIndex < (PRUint32)mResults.Count(),
                 NS_ERROR_ILLEGAL_VALUE);
  NS_ADDREF(*_retval = mResults.ObjectAt(matchIndex));
  return NS_OK;
}

// nsIAutoCompleteResult

NS_IMETHODIMP
nsAutoCompleteStorageResult::GetSearchString(nsAString &aSearchString)
{
  aSearchString = mSearchString;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::GetSearchResult(PRUint16 *aSearchResult)
{
  *aSearchResult = mSearchResult;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::GetDefaultIndex(PRInt32 *aDefaultIndex)
{
  *aDefaultIndex = mDefaultIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::GetErrorDescription(nsAString & aErrorDescription)
{
  aErrorDescription = mErrorDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::GetMatchCount(PRUint32 *aMatchCount)
{
  *aMatchCount = mResults.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::GetValueAt(PRInt32 aIndex, nsAString & _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && aIndex < mResults.Count(),
                 NS_ERROR_ILLEGAL_VALUE);

  nsIAutoCompleteStorageMatch* match = mResults.ObjectAt(aIndex);
  if (!match)
    return NS_ERROR_FAILURE;

  return match->GetValue(_retval);
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::GetCommentAt(PRInt32 aIndex, nsAString & _retval)
{
  NS_ENSURE_TRUE(aIndex >= 0 && aIndex < mResults.Count(),
                 NS_ERROR_ILLEGAL_VALUE);

  nsIAutoCompleteStorageMatch* match = mResults.ObjectAt(aIndex);
  if (!match)
    return NS_ERROR_FAILURE;

  return match->GetComment(_retval);
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::GetStyleAt(PRInt32 aIndex, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIAutoCompleteBaseResult

NS_IMETHODIMP
nsAutoCompleteStorageResult::SetSearchString(const nsAString &aSearchString)
{
  mSearchString.Assign(aSearchString);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::SetErrorDescription(const nsAString &aErrorDescription)
{
  mErrorDescription.Assign(aErrorDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::SetDefaultIndex(PRInt32 aDefaultIndex)
{
  mDefaultIndex = aDefaultIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::SetSearchResult(PRUint32 aSearchResult)
{
  mSearchResult = aSearchResult;
  return NS_OK;
}

NS_IMETHODIMP
nsAutoCompleteStorageResult::RemoveValueAt(PRInt32 aRowIndex,
                                           PRBool aRemoveFromDb)
{
  return NS_OK;
}
