/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsAbAutoCompleteSession.h"
#include "nsString.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsIAbDirectory.h"
#include "nsIAbCard.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsMsgBaseCID.h"
#include "nsMsgI18N.h"
#include "nsIMsgIdentity.h"
#include "nsIPref.h"
#include "prmem.h"


static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAutoCompleteResultsCID, NS_AUTOCOMPLETERESULTS_CID);
static NS_DEFINE_CID(kAutoCompleteItemCID, NS_AUTOCOMPLETEITEM_CID);


NS_IMPL_ISUPPORTS2(nsAbAutoCompleteSession, nsIAbAutoCompleteSession, nsIAutoCompleteSession)

nsAbAutoCompleteSession::nsAbAutoCompleteSession()
{
	NS_INIT_REFCNT();
    mParser = do_GetService(kHeaderParserCID);
}


nsAbAutoCompleteSession::~nsAbAutoCompleteSession()
{
}


void nsAbAutoCompleteSession::ResetMatchTypeConters()
{
    PRInt32 i;
    for (i = 0; i < LAST_MATCH_TYPE; mMatchTypeConters[i++] = 0)
        ;
}

PRBool nsAbAutoCompleteSession::ItsADuplicate(PRUnichar* fullAddrStr, nsIAutoCompleteResults* results)
{
    nsresult rv;

    nsCOMPtr<nsISupportsArray> array;
    rv = results->GetItems(getter_AddRefs(array));
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIEnumerator> enumerator;
        rv = array->Enumerate(getter_AddRefs(enumerator));
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsISupports> item;
            nsCOMPtr<nsIAutoCompleteItem> resultItem;
            nsAutoString valueStr;

            for (rv = enumerator->First(); NS_SUCCEEDED(rv); rv = enumerator->Next())
            {
                rv = enumerator->CurrentItem(getter_AddRefs(item));
                if (NS_SUCCEEDED(rv) && item)
                {
                    resultItem = do_QueryInterface(item, &rv);
                    if (NS_SUCCEEDED(rv))
                    {
                        rv = resultItem->GetValue(valueStr);
                        if (NS_SUCCEEDED(rv) && !valueStr.IsEmpty())
                        {
                          if (nsCRT::strcasecmp(fullAddrStr, valueStr.get())==0)
                            return PR_TRUE;
                        }
                    }
                }
            }
        }
    }
    
    return PR_FALSE;
}

void 
nsAbAutoCompleteSession::AddToResult(const PRUnichar* pNickNameStr,
                                     const PRUnichar* pDisplayNameStr,
                                     const PRUnichar* pFirstNameStr,
                                     const PRUnichar* pLastNameStr,
                                     const PRUnichar* pEmailStr, 
                                     const PRUnichar* pNotesStr, 
                                     const PRUnichar* pDirName,
                                     PRBool bIsMailList, MatchType type,
                                     nsIAutoCompleteResults* results)
{
  nsresult rv;
  PRUnichar* fullAddrStr = nsnull;

  if (type == DEFAULT_MATCH)
  {
    if (mDefaultDomain[0] == 0)
      return;

    nsAutoString aStr(pDisplayNameStr);
    aStr.AppendWithConversion('@');
    aStr += mDefaultDomain;
    fullAddrStr = ToNewUnicode(aStr);
  }
  else
  {
    if (mParser)
    {
      nsXPIDLCString fullAddress;
      nsXPIDLCString utf8Email;
      if (bIsMailList)
      {
        if (pNotesStr && pNotesStr[0] != 0)
          utf8Email.Adopt(ToNewUTF8String(nsDependentString(pNotesStr)));
        else
          utf8Email.Adopt(ToNewUTF8String(nsDependentString(pDisplayNameStr)));
      }
      else
        utf8Email.Adopt(ToNewUTF8String(nsDependentString(pEmailStr)));

      mParser->MakeFullAddress(nsnull, NS_ConvertUCS2toUTF8(pDisplayNameStr).get(),
                               utf8Email, getter_Copies(fullAddress));
      if (!fullAddress.IsEmpty())
      {
        /* We need to convert back the result from UTF-8 to Unicode */
        INTL_ConvertToUnicode(fullAddress.get(), fullAddress.Length(), (void**)&fullAddrStr);
      }
    }
  
    if (!fullAddrStr)
    {
      //oops, parser problem! I will try to do my best...
      const PRUnichar * pStr = nsnull;
      if (bIsMailList)
      {
        if (pNotesStr && pNotesStr[0] != 0)
          pStr = pNotesStr;
        else
          pStr = pDisplayNameStr;
      }
      else
        pStr = pEmailStr;
      // check this so we do not get a bogus entry "someName <>"
      if (pStr && pStr[0] != 0) {
        nsAutoString aStr(pDisplayNameStr);
        aStr.AppendWithConversion(" <");
        aStr += pStr;
        aStr.AppendWithConversion(">");
        fullAddrStr = ToNewUnicode(aStr);
      }
      else
        fullAddrStr = nsnull;
    }
  }
    
  if (fullAddrStr && ! ItsADuplicate(fullAddrStr, results))
  {    
    nsCOMPtr<nsIAutoCompleteItem> newItem;
    rv = nsComponentManager::CreateInstance(kAutoCompleteItemCID, nsnull, NS_GET_IID(nsIAutoCompleteItem), getter_AddRefs(newItem));
    if (NS_SUCCEEDED(rv))
    {
      nsAbAutoCompleteParam *param = new nsAbAutoCompleteParam(pNickNameStr, pDisplayNameStr, pFirstNameStr, pLastNameStr, pEmailStr, pNotesStr, pDirName, bIsMailList, type);
      NS_IF_ADDREF(param);
      newItem->SetParam(param);
      NS_IF_RELEASE(param);

      // how to process the comment column, if at all.  this value
      // comes from "mail.autoComplete.commentColumn", or, if that
      // doesn't exist, defaults to 0
      //
      // 0 = none
      // 1 = name of addressbook this card came from
      // 2 = other per-addressbook format (currrently unused here)
      //
      if (mAutoCompleteCommentColumn == 1) {
        rv = newItem->SetComment(pDirName);
        if (NS_FAILED(rv)) {
          NS_WARNING("nsAbAutoCompleteSession::AddToResult():"
                     " newItem->SetComment() failed\n");
        }
      }

      // if this isn't a default match, set the class name so we can style 
      // this cell with the local addressbook icon (or whatever)
      //
      rv = newItem->SetClassName(type == DEFAULT_MATCH ? "default-match" :
                                 "local-abook");
      if (NS_FAILED(rv)) {
        NS_WARNING("nsAbAutoCompleteSession::AddToResult():"
                   " newItem->SetClassName() failed\n");
      }

      newItem->SetValue(nsDependentString(fullAddrStr));
      nsCOMPtr<nsISupportsArray> array;
      rv = results->GetItems(getter_AddRefs(array));
      if (NS_SUCCEEDED(rv))
      {
        PRInt32 insertPosition = 0;
        PRInt32 i;
        for (i = 0; i <= type; insertPosition += mMatchTypeConters[i++])
          ; 
        rv = array->InsertElementAt(newItem, insertPosition);
        if (NS_SUCCEEDED(rv))
          mMatchTypeConters[type] ++;
      }
    }
  }    
  PR_Free(fullAddrStr);
}

PRBool nsAbAutoCompleteSession::CheckEntry(nsAbAutoCompleteSearchString* searchStr, const PRUnichar* nickName, const PRUnichar* displayName,
  const PRUnichar* firstName, const PRUnichar* lastName, const PRUnichar* emailAddress, MatchType* matchType)
{
  const PRUnichar * fullString;
  PRUint32 fullStringLen;
  
  if (searchStr->mFirstPartLen > 0 && searchStr->mSecondPartLen == 0)
  {
    fullString = searchStr->mFirstPart;
    fullStringLen = searchStr->mFirstPartLen;
  }
  else
  {
    fullString = searchStr->mFullString;
    fullStringLen = searchStr->mFullStringLen;
  }

  // First check for a Nickname exact match
  if (nickName && nsCRT::strcasecmp(fullString, nickName) == 0)
  {
    *matchType = NICKNAME_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a display Name exact match
  if (displayName && nsCRT::strcasecmp(fullString, displayName) == 0)
  {
    *matchType = NAME_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a fisrt Name exact match
  if (firstName && nsCRT::strcasecmp(fullString, firstName) == 0)
  {
    *matchType = NAME_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a last Name exact match
  if (lastName && nsCRT::strcasecmp(fullString, lastName) == 0)
  {
    *matchType = NAME_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a Email exact match
  if (emailAddress && nsCRT::strcasecmp(fullString, emailAddress) == 0)
  {
    *matchType = EMAIL_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a NickName partial match
  if (nickName && nsCRT::strncasecmp(fullString, nickName, fullStringLen) == 0)
  {
  	*matchType = NICKNAME_MATCH;
    return PR_TRUE;
  }

  // Then check for a display Name partial match
  if (displayName && nsCRT::strncasecmp(fullString, displayName, fullStringLen) == 0)
  {
  	*matchType = NAME_MATCH;
    return PR_TRUE;
  }

  // Then check for a first Name partial match
  if (firstName && nsCRT::strncasecmp(fullString, firstName, fullStringLen) == 0)
  {
  	*matchType = NAME_MATCH;
    return PR_TRUE;
  }

  // Then check for a last Name partial match
  if (lastName && nsCRT::strncasecmp(fullString, lastName, fullStringLen) == 0)
  {
  	*matchType = NAME_MATCH;
    return PR_TRUE;
  }

  // Then check for a Email partial match
  if (emailAddress && nsCRT::strncasecmp(fullString, emailAddress, fullStringLen) == 0)
  {
  	*matchType = EMAIL_MATCH;
    return PR_TRUE;
  }
  
  //If we have a muti-part search string, look for a partial match with first name and last name or reverse
  if (searchStr->mFirstPartLen && searchStr->mSecondPartLen)
  {
    if (((firstName && nsCRT::strncasecmp(searchStr->mFirstPart, firstName, searchStr->mFirstPartLen) == 0) &&
        (lastName && nsCRT::strncasecmp(searchStr->mSecondPart, lastName, searchStr->mSecondPartLen) == 0)) ||
        ((lastName && nsCRT::strncasecmp(searchStr->mFirstPart, lastName, searchStr->mFirstPartLen) == 0) &&
        (firstName && nsCRT::strncasecmp(searchStr->mSecondPart, firstName, searchStr->mSecondPartLen) == 0))
       )
    {
      *matchType = NAME_MATCH;
      return PR_TRUE;
    }
  }

	return PR_FALSE;
}

nsresult nsAbAutoCompleteSession::SearchCards(nsIAbDirectory* directory, nsAbAutoCompleteSearchString* searchStr, nsIAutoCompleteResults* results)
{
  nsresult rv;    
  nsCOMPtr<nsIEnumerator> cardsEnumerator;
  nsCOMPtr<nsIAbCard> card;
  
  rv = directory->GetChildCards(getter_AddRefs(cardsEnumerator));
  if (NS_SUCCEEDED(rv) && cardsEnumerator)
  {
		nsCOMPtr<nsISupports> item;
	  for (rv = cardsEnumerator->First(); NS_SUCCEEDED(rv); rv = cardsEnumerator->Next())
	  {
      rv = cardsEnumerator->CurrentItem(getter_AddRefs(item));
      if (NS_SUCCEEDED(rv))
      {
        card = do_QueryInterface(item, &rv);
	      if (NS_SUCCEEDED(rv))
	      {
          nsXPIDLString pEmailStr;
          nsXPIDLString pDisplayNameStr;
          nsXPIDLString pFirstNameStr;
          nsXPIDLString pLastNameStr;
          nsXPIDLString pNickNameStr;
          nsXPIDLString pNotesStr;
					PRBool bIsMailList;

					rv = card->GetIsMailList(&bIsMailList);
          if (NS_FAILED(rv))
            continue;
					if (bIsMailList)
          {
            rv = card->GetNotes(getter_Copies(pNotesStr));
						if (NS_FAILED(rv))
							continue;
          }
          else
					{
            rv = card->GetPrimaryEmail(getter_Copies(pEmailStr));
						if (NS_FAILED(rv))
							continue;
						// Don't bother with card without an email address
						if (!(const PRUnichar*)pEmailStr || ((const PRUnichar*)pEmailStr)[0] == 0)
							continue;
						//...and does it looks like a valid address?
						PRInt32 i;
						for (i = 0; ((const PRUnichar*)pEmailStr)[i] != 0 &&
								((const PRUnichar*)pEmailStr)[i] != '@'; i ++)
							;
						if (((const PRUnichar*)pEmailStr)[i] == 0)
							continue;
					}
            
            //Now, retrive the user name and nickname
          rv = card->GetDisplayName(getter_Copies(pDisplayNameStr));
          if (NS_FAILED(rv))
             	continue;
          rv = card->GetFirstName(getter_Copies(pFirstNameStr));
          if (NS_FAILED(rv))
             	continue;
          rv = card->GetLastName(getter_Copies(pLastNameStr));
          if (NS_FAILED(rv))
             	continue;
          rv = card->GetNickName(getter_Copies(pNickNameStr));
          if (NS_FAILED(rv))
             	continue;
            
					MatchType matchType;
 					if ( CheckEntry(searchStr, pNickNameStr.get(), 
                                    pDisplayNameStr.get(), 
                                    pFirstNameStr.get(), 
                                    pLastNameStr.get(), pEmailStr.get(), 
                                    &matchType)) {

                      nsXPIDLString pDirName;
                      if ( mAutoCompleteCommentColumn == 1 ) {
                        rv = directory->GetDirName(getter_Copies(pDirName));
                        if (NS_FAILED(rv)) {
                          continue;
                        }
                      }

                      AddToResult(pNickNameStr.get(), pDisplayNameStr.get(), 
                                  pFirstNameStr.get(), pLastNameStr.get(), 
                                  pEmailStr.get(), pNotesStr.get(), 
                                  pDirName.get(), bIsMailList, matchType, 
                                  results);
                    }
	      }
      }
    }
  }

  return NS_OK;
}


nsresult nsAbAutoCompleteSession::SearchDirectory(nsString& fileName, nsAbAutoCompleteSearchString* searchStr, nsIAutoCompleteResults* results, PRBool searchSubDirectory)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr <nsIRDFResource> resource;
    char * strFileName = ToNewCString(fileName);
    rv = rdfService->GetResource(strFileName, getter_AddRefs(resource));
    Recycle(strFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    // query interface 
    nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(resource, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (!fileName.EqualsWithConversion(kAllDirectoryRoot))
        rv = SearchCards(directory, searchStr, results);
    
    if (!searchSubDirectory)
        return rv;
  
    nsCOMPtr<nsIEnumerator> subDirectories;
    if (NS_SUCCEEDED(directory->GetChildNodes(getter_AddRefs(subDirectories))) && subDirectories)
    {
        nsCOMPtr<nsISupports> item;
        if (NS_SUCCEEDED(subDirectories->First()))
        {
		        do
            {
                if (NS_SUCCEEDED(subDirectories->CurrentItem(getter_AddRefs(item))))
                {
                    directory = do_QueryInterface(item, &rv);
                    if (NS_SUCCEEDED(rv))
                    {
		                    nsCOMPtr<nsIRDFResource> subResource(do_QueryInterface(item, &rv));
		                    if (NS_SUCCEEDED(rv))
		                    {
		                        nsXPIDLCString URI;
		                        subResource->GetValue(getter_Copies(URI));
		                        nsAutoString subURI;
		                        subURI.AssignWithConversion(URI);
		                        rv = SearchDirectory(subURI, searchStr, results, PR_TRUE);
		                    }
                    }
                }
            } while (NS_SUCCEEDED(subDirectories->Next()));
        }
    }
    return rv;
}

nsresult nsAbAutoCompleteSession::SearchPreviousResults(nsAbAutoCompleteSearchString *searchStr, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteResults* results)
{
    if (!previousSearchResult)
        return NS_ERROR_NULL_POINTER;
        
    nsXPIDLString prevSearchString;
    nsresult rv;

    rv = previousSearchResult->GetSearchString(getter_Copies(prevSearchString));
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (!(const PRUnichar*)prevSearchString || ((const PRUnichar*)prevSearchString)[0] == 0)
        return NS_ERROR_FAILURE;
    
    PRUint32 prevSearchStrLen = nsCRT::strlen(prevSearchString);
    if (searchStr->mFullStringLen < prevSearchStrLen || nsCRT::strncasecmp(searchStr->mFullString, prevSearchString, prevSearchStrLen != 0))
        return NS_ERROR_ABORT;

    nsCOMPtr<nsISupportsArray> array;
    rv = previousSearchResult->GetItems(getter_AddRefs(array));
    if (NS_SUCCEEDED(rv))
    {
        PRUint32 nbrOfItems;
        PRUint32 i;
        PRUint32 pos;
        
        rv = array->Count(&nbrOfItems);
        if (NS_FAILED(rv) || nbrOfItems <= 0)
            return NS_ERROR_FAILURE;
        
        nsCOMPtr<nsISupports> item;
        nsCOMPtr<nsIAutoCompleteItem> resultItem;
        nsAbAutoCompleteParam *param;

        for (i = 0, pos = 0; i < nbrOfItems; i ++, pos ++)
        {
	          rv = array->QueryElementAt(pos, NS_GET_IID(nsIAutoCompleteItem),
                                           getter_AddRefs(resultItem));
	          NS_ENSURE_SUCCESS(rv, rv);
	            
            rv = resultItem->GetParam(getter_AddRefs(item));
            NS_ENSURE_SUCCESS(rv, rv);
	          if (!item)
                return NS_ERROR_FAILURE;

            param = (nsAbAutoCompleteParam *)(void *)item;
            
			      MatchType matchType;
			      if (CheckEntry(searchStr, param->mNickName, param->mDisplayName,  param->mFirstName,  param->mLastName, param->mEmailAddress, &matchType))
                AddToResult(param->mNickName, param->mDisplayName, 
                            param->mFirstName, param->mLastName, 
                            param->mEmailAddress, param->mNotes, 
                            param->mDirName, param->mIsMailList, matchType,
                            results);
	      }
	      return NS_OK;
    }

    return NS_ERROR_ABORT;
}

NS_IMETHODIMP nsAbAutoCompleteSession::OnStartLookup(const PRUnichar *uSearchString, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteListener *listener)
{
    nsresult rv = NS_OK;
    
    if (!listener)
        return NS_ERROR_NULL_POINTER;
    
    PRBool enableAutocomplete = PR_TRUE;

    nsCOMPtr<nsIPref> pPref(do_GetService(kPrefCID, &rv)); 
    NS_ENSURE_SUCCESS(rv, rv);

    pPref->GetBoolPref("mail.enable_autocomplete", &enableAutocomplete);

    if (uSearchString[0] == 0 || enableAutocomplete == PR_FALSE)
    {
        listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
        return NS_OK;
    }

    // figure out what we're supposed to do about the comment column, and 
    // remember it for when the results start coming back
    //
    rv = pPref->GetIntPref("mail.autoComplete.commentColumn", 
                           &mAutoCompleteCommentColumn);
    if (NS_FAILED(rv)) {
      mAutoCompleteCommentColumn = 0;
    }

    PRInt32 i;
    for (i = nsCRT::strlen(uSearchString) - 1; i >= 0; i --)
        if (uSearchString[i] == '@')
        {
            listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
            return NS_OK;
        }
        
    nsAbAutoCompleteSearchString searchStrings(uSearchString);
    
	  ResetMatchTypeConters();       
    nsCOMPtr<nsIAutoCompleteResults> results;
    rv = nsComponentManager::CreateInstance(kAutoCompleteResultsCID, nsnull, NS_GET_IID(nsIAutoCompleteResults), getter_AddRefs(results));
    if (NS_SUCCEEDED(rv))
		  if (NS_FAILED(SearchPreviousResults(&searchStrings, previousSearchResult, results)))
		  {
			  nsAutoString root; root.AssignWithConversion(kAllDirectoryRoot);
			  rv = SearchDirectory(root, &searchStrings, results, PR_TRUE);
		  }
                
    AutoCompleteStatus status = nsIAutoCompleteStatus::failed;
    if (NS_SUCCEEDED(rv) && results)
    {
        PRBool addedDefaultItem = PR_FALSE;

        results->SetSearchString(uSearchString);
        results->SetDefaultItemIndex(-1);
        if (mDefaultDomain[0] != 0)
        {
            PRUnichar emptyStr = 0;
            AddToResult(&emptyStr, uSearchString, &emptyStr, &emptyStr, 
                        &emptyStr, &emptyStr, &emptyStr, PR_FALSE, 
                        DEFAULT_MATCH, results);
            addedDefaultItem = PR_TRUE;
        }

        nsCOMPtr<nsISupportsArray> array;
        rv = results->GetItems(getter_AddRefs(array));
        if (NS_SUCCEEDED(rv))
        {
          //If we have more than a match (without counting the default item), we don't
          //want to auto complete the user input therefore set the default item index to -1

          PRUint32 nbrOfItems;
          rv = array->Count(&nbrOfItems);
          if (NS_SUCCEEDED(rv))
            if (nbrOfItems == 0)
              status = nsIAutoCompleteStatus::noMatch;
            else
            {
              status = nsIAutoCompleteStatus::matchFound;
              if (addedDefaultItem)
              {
                // If we have at least one REAL match then make it the default item. If we don't have any matches,
                // just the default domain, then don't install a default item index on the widget.
                results->SetDefaultItemIndex(nbrOfItems > 1 ? 1 : -1);
              }
              else
                results->SetDefaultItemIndex(0);  
            }
        }
    }
    listener->OnAutoComplete(results, status);
    
    return NS_OK;
}

NS_IMETHODIMP nsAbAutoCompleteSession::OnStopLookup()
{
    return NS_OK;
}

NS_IMETHODIMP nsAbAutoCompleteSession::OnAutoComplete(const PRUnichar *searchString, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteListener *listener)
{
    return OnStartLookup(searchString, previousSearchResult, listener);
}

NS_IMETHODIMP nsAbAutoCompleteSession::GetDefaultDomain(PRUnichar * *aDefaultDomain)
{
    if (!aDefaultDomain)
        return NS_ERROR_NULL_POINTER;

    *aDefaultDomain = ToNewUnicode(mDefaultDomain);
    return NS_OK;
}

NS_IMETHODIMP nsAbAutoCompleteSession::SetDefaultDomain(const PRUnichar * aDefaultDomain)
{
    mDefaultDomain = aDefaultDomain;
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsAbAutoCompleteParam, nsISupports)

nsAbAutoCompleteSearchString::nsAbAutoCompleteSearchString(const PRUnichar *uSearchString)
{
  mFullString = nsCRT::strdup(uSearchString);
  mFullStringLen = nsCRT::strlen(mFullString);
  
  PRUint32 i;
  PRUnichar * aPtr;
  for (i = 0, aPtr = (PRUnichar*)mFullString; i < mFullStringLen; i ++, aPtr ++)
  {
    if (*aPtr == ' ')
    {
      mFirstPart = nsCRT::strndup(mFullString, i);
      mFirstPartLen = i;
      mSecondPart = nsCRT::strdup(++aPtr);
      mSecondPartLen = mFullStringLen - i - 1;
      return;
    }
  }
  
  /* If we did not find a space in the search string, initialize the first and second part as null */
  mFirstPart = nsnull;
  mFirstPartLen = 0;
  mSecondPart = nsnull;
  mSecondPartLen = 0;
}

nsAbAutoCompleteSearchString::~nsAbAutoCompleteSearchString()
{
  if (mFullString)
     nsCRT::free((PRUnichar*)mFullString);
  if (mFirstPart)
     nsCRT::free((PRUnichar*)mFirstPart);
  if (mSecondPart)
     nsCRT::free((PRUnichar*)mSecondPart);
}
