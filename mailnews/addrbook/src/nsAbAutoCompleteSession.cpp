/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"
#include "nsAbAutoCompleteSession.h"
#include "nsString.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsIAbDirectory.h"
#include "nsIAbCard.h"
#include "nsXPIDLString.h"
#include "nsMsgBaseCID.h"
#include "nsMsgI18N.h"
#include "nsIMsgIdentity.h"

/* The definition is nsAddressBook.cpp */
extern const char *kDirectoryDataSourceRoot;

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
            nsXPIDLString valueStr;

            for (rv = enumerator->First(); NS_SUCCEEDED(rv); rv = enumerator->Next())
            {
                rv = enumerator->CurrentItem(getter_AddRefs(item));
                if (NS_SUCCEEDED(rv) && item)
                {
                    resultItem = do_QueryInterface(item, &rv);
                    if (NS_SUCCEEDED(rv))
                    {
                        rv = resultItem->GetValue(getter_Copies(valueStr));
                        if (NS_SUCCEEDED(rv) && valueStr && ((const PRUnichar*)valueStr)[0] != 0)
                        {
                            if (nsCRT::strcasecmp(fullAddrStr, valueStr) == 0)
                                return PR_TRUE;
                        }
                    }
                }
            }
        }
    }
    
    return PR_FALSE;
}

void nsAbAutoCompleteSession::AddToResult(const PRUnichar* pNickNameStr, const PRUnichar* pNameStr, 
    const PRUnichar*pEmailStr, MatchType type, nsIAutoCompleteResults* results)
{
    nsresult rv;
    PRUnichar* fullAddrStr = nsnull;

    if (type == DEFAULT_MATCH)
    {
        if (mDefaultDomain[0] == 0)
            return;

        nsAutoString aStr = pNameStr;
        aStr += '@';
        aStr += mDefaultDomain;
        fullAddrStr = aStr.ToNewUnicode();
    }
    else
        if (mParser)
        {
            char * utf8Name = nsAutoString(pNameStr).ToNewUTF8String();
            char * utf8Email = nsAutoString(pEmailStr).ToNewUTF8String();   
    	    char * fullAddress = nsnull;

            mParser->MakeFullAddress(nsnull, utf8Name, utf8Email, &fullAddress);
    	    if (fullAddress && *fullAddress)
    	    {
            	/* We need to convert back the result from UTF-8 to Unicode */
            	PRInt32 strLen;
            	INTL_ConvertToUnicode(fullAddress, nsCRT::strlen(fullAddress), (void**)&fullAddrStr, &strLen);
            	PR_Free(fullAddress);
    	    }
            Recycle(utf8Name);
            Recycle(utf8Email);
        }
    
    if (!fullAddrStr)
    {
        //oops, parser problem! I will try to do my best...
        nsAutoString aStr = pNameStr;
        aStr += " <";
        aStr += pEmailStr;
        aStr += ">";
        fullAddrStr = aStr.ToNewUnicode();
    }
    
    if (! ItsADuplicate(fullAddrStr, results))
    {    
        nsCOMPtr<nsIAutoCompleteItem> newItem;
    	rv = nsComponentManager::CreateInstance(kAutoCompleteItemCID, nsnull, NS_GET_IID(nsIAutoCompleteItem), getter_AddRefs(newItem));
        if (NS_SUCCEEDED(rv))
        {
            nsAbAutoCompleteParam *param = new nsAbAutoCompleteParam(pNickNameStr, pNameStr, pEmailStr, type);
            NS_IF_ADDREF(param);
            newItem->SetParam(param);
            NS_IF_RELEASE(param);

            newItem->SetValue(fullAddrStr);
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


nsresult nsAbAutoCompleteSession::SearchCards(nsIAbDirectory* directory, const PRUnichar* searchStr, nsIAutoCompleteResults* results)
{
    nsresult rv;    
    nsCOMPtr<nsIEnumerator> cardsEnumerator;
    nsCOMPtr<nsIAbCard> card;
    
    PRUint32 searchStrLen = nsCRT::strlen(searchStr);

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
            	    nsXPIDLString pNameStr;
            	    nsXPIDLString pNickNameStr;

                    // Don't bother with card without an email address
             	    rv = card->GetPrimaryEmail(getter_Copies(pEmailStr));
                    if (NS_FAILED(rv))
                        continue;
                    if (!(const PRUnichar*)pEmailStr || ((const PRUnichar*)pEmailStr)[0] == 0)
                        continue;
                    
                    //Now, retrive the user name and nickname
             	    rv = card->GetDisplayName(getter_Copies(pNameStr));
             	    if (NS_FAILED(rv))
             	        continue;
             	    rv = card->GetNickName(getter_Copies(pNickNameStr));
             	    if (NS_FAILED(rv))
             	        continue;
                    
                    // First check for a Nickname exact match
        			if (nsCRT::strcasecmp(searchStr, (const PRUnichar*)pNickNameStr) == 0)
        			{
        			    AddToResult((const PRUnichar*)pNickNameStr, (const PRUnichar*)pNameStr, (const PRUnichar*)pEmailStr, NICKNAME_EXACT_MATCH, results);
        			    continue;
        			}

                    // Then check for a Name exact match
        			if (nsCRT::strcasecmp(searchStr, (const PRUnichar*)pNameStr) == 0)
        			{
        			    AddToResult((const PRUnichar*)pNickNameStr, (const PRUnichar*)pNameStr, (const PRUnichar*)pEmailStr, NAME_EXACT_MATCH, results);
        			    continue;
        			}

                    // Then check for a Email exact match
        			if (nsCRT::strcasecmp(searchStr, (const PRUnichar*)pEmailStr) == 0)
        			{
        			    AddToResult((const PRUnichar*)pNickNameStr, (const PRUnichar*)pNameStr, (const PRUnichar*)pEmailStr, EMAIL_EXACT_MATCH, results);
        			    continue;
        			}

                    // Then check for a NickName partial match
    			    if (nsCRT::strncasecmp(searchStr, (const PRUnichar*)pNickNameStr, searchStrLen) == 0)
    			    {
    			        AddToResult((const PRUnichar*)pNickNameStr, (const PRUnichar*)pNameStr, (const PRUnichar*)pEmailStr, NICKNAME_MATCH, results);
        			    continue;
    			    }

                    // Then check for a Name partial match
    			    if (nsCRT::strncasecmp(searchStr, (const PRUnichar*)pNameStr, searchStrLen) == 0)
    			    {
    			        AddToResult((const PRUnichar*)pNickNameStr, (const PRUnichar*)pNameStr, (const PRUnichar*)pEmailStr, NAME_MATCH, results);
        			    continue;
    			    }

                    // Then check for a Email partial match
    			    if (nsCRT::strncasecmp(searchStr, (const PRUnichar*)pEmailStr, searchStrLen) == 0)
    			    {
    			        AddToResult((const PRUnichar*)pNickNameStr, (const PRUnichar*)pNameStr, (const PRUnichar*)pEmailStr, EMAIL_MATCH, results);
        			    continue;
    			    }
	            }
            }
        }
    }

    return NS_OK;
}


nsresult nsAbAutoCompleteSession::SearchDirectory(nsString& fileName, const PRUnichar* searchStr, nsIAutoCompleteResults* results, PRBool searchSubDirectory)
{
    nsresult rv = NS_OK;
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr <nsIRDFResource> resource;
    char * strFileName = fileName.ToNewCString();
    rv = rdfService->GetResource(strFileName, getter_AddRefs(resource));
    Recycle(strFileName);
    if (NS_FAILED(rv)) return rv;

    // query interface 
    nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(resource, &rv));
    if (NS_FAILED(rv)) return rv;
    
    if (fileName != nsAutoString(kDirectoryDataSourceRoot))
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
			            DIR_Server *server = nsnull;
                        if (NS_SUCCEEDED(directory->GetServer(&server)) && server)
                        {
                            nsAutoString subFileName(fileName);
                            if (subFileName.Last() != '/')
                                subFileName += "/";
                            subFileName += server->fileName;
                            rv = SearchDirectory(subFileName, searchStr, results, PR_TRUE);
                        }
                    }
                }
            } while (NS_SUCCEEDED(subDirectories->Next()));
        }
    }
    return rv;
}

nsresult nsAbAutoCompleteSession::SearchPreviousResults(const PRUnichar *searchStr, nsIAutoCompleteResults *previousSearchResult)
{
    if (!previousSearchResult)
        return NS_ERROR_NULL_POINTER;
        
    nsXPIDLString prevSearchString;
    PRUint32 searchStrLen = nsCRT::strlen(searchStr);
    nsresult rv;

    rv = previousSearchResult->GetSearchString(getter_Copies(prevSearchString));
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    
    if (!(const PRUnichar*)prevSearchString || ((const PRUnichar*)prevSearchString)[0] == 0)
        return NS_ERROR_FAILURE;
    
    PRUint32 prevSearchStrLen = nsCRT::strlen(prevSearchString);
    if (searchStrLen < prevSearchStrLen || nsCRT::strncasecmp(searchStr, prevSearchString, prevSearchStrLen != 0))
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
	        rv = array->QueryElementAt(pos, nsIAutoCompleteItem::GetIID(), getter_AddRefs(resultItem));
	        if (NS_FAILED(rv))
                return NS_ERROR_FAILURE;
	        
            rv = resultItem->GetParam(getter_AddRefs(item));
	        if (NS_FAILED(rv) || !item)
                return NS_ERROR_FAILURE;
          
	        rv = item->QueryInterface(nsAbAutoCompleteParam::GetIID(), (void **)&param);
	        if (NS_FAILED(rv) || !param)
                return NS_ERROR_FAILURE;
            
		    if (nsCRT::strncasecmp(searchStr, param->mNickName, searchStrLen) == 0 ||
                nsCRT::strncasecmp(searchStr, param->mUserName, searchStrLen) == 0 ||
			    nsCRT::strncasecmp(searchStr, param->mEmailAddress, searchStrLen) == 0)
			{
			    NS_RELEASE(param);
			    continue;
			}
			
			//No match at all with the new search string. Thefore remove this result item from the results array
			if (array->RemoveElementAt(pos))
			{
			    pos --;
			    mMatchTypeConters[param->mType] --;
			    NS_RELEASE(param);
			}
			else
			{
			    NS_RELEASE(param);
			    return NS_ERROR_FAILURE;
			}
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
        
    if (uSearchString[0] == 0)
    {
        listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
        return NS_OK;
    }
    
    PRInt32 i;
    for (i = nsCRT::strlen(uSearchString) - 1; i >= 0; i --)
        if (uSearchString[i] == '@')
        {
            listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
            return NS_OK;
        }
    
    nsCOMPtr<nsIAutoCompleteResults> results;
    if (NS_FAILED(SearchPreviousResults(uSearchString, previousSearchResult)))
    {
        ResetMatchTypeConters();
        
    	rv = nsComponentManager::CreateInstance(kAutoCompleteResultsCID, nsnull, NS_GET_IID(nsIAutoCompleteResults), getter_AddRefs(results));
        if (NS_SUCCEEDED(rv))
        {
            nsAutoString root = kDirectoryDataSourceRoot;
            rv = SearchDirectory(root, uSearchString, results, PR_TRUE);
        }
    }
    else
        results = previousSearchResult;
                
    AutoCompleteStatus status = nsIAutoCompleteStatus::failed;
    if (NS_SUCCEEDED(rv) && results)
    {
        PRBool addedDefaultItem = PR_FALSE;

        results->SetSearchString(uSearchString);
        results->SetDefaultItemIndex(-1);
        if (mDefaultDomain[0] != 0)
        {
            PRUnichar emptyStr = 0;
            AddToResult(&emptyStr, uSearchString, &emptyStr, DEFAULT_MATCH, results);
            addedDefaultItem = PR_TRUE;
        }

        nsCOMPtr<nsISupportsArray> array;
        rv = results->GetItems(getter_AddRefs(array));
        if (NS_SUCCEEDED(rv))
        {
            PRUint32 nbrOfItems;
            rv = array->Count(&nbrOfItems);
            if (NS_SUCCEEDED(rv))
                if (nbrOfItems > 1)
                {
                    results->SetDefaultItemIndex(addedDefaultItem ? 1 : 0);
                    status = nsIAutoCompleteStatus::matchFound;
                }
                else
                    if (nbrOfItems == 1)
                    {
                        results->SetDefaultItemIndex(0);
                        status = nsIAutoCompleteStatus::matchFound;
                    }
                    else
                        status = nsIAutoCompleteStatus::noMatch;
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

    *aDefaultDomain = mDefaultDomain.ToNewUnicode();
    return NS_OK;
}
NS_IMETHODIMP nsAbAutoCompleteSession::SetDefaultDomain(const PRUnichar * aDefaultDomain)
{
    mDefaultDomain = aDefaultDomain;
    return NS_OK;
}

NS_IMPL_ISUPPORTS(nsAbAutoCompleteParam, nsAbAutoCompleteParam::GetIID());
