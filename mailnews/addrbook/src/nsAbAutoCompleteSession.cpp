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

static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsresult NS_NewAbAutoCompleteSession(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (aInstancePtrResult)
	{
		nsAbAutoCompleteSession * abSession = new nsAbAutoCompleteSession(); 
		if (abSession)
			return abSession->QueryInterface(NS_GET_IID(nsIAbAutoCompleteSession), aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

NS_IMPL_ISUPPORTS(nsAbAutoCompleteSession, NS_GET_IID(nsIAbAutoCompleteSession))

nsAbAutoCompleteSession::nsAbAutoCompleteSession()
{
	NS_INIT_REFCNT();
    m_numEntries = 0;
    m_tableInitialized = PR_FALSE;
}

nsresult nsAbAutoCompleteSession::PopulateTableWithAB(nsIEnumerator * aABCards)
{
    nsresult rv = NS_OK;
    if (!aABCards)
        return rv;

    rv = aABCards->First();
    while (NS_SUCCEEDED(rv) && m_numEntries < MAX_ENTRIES) 
    {
        m_searchNameCompletionEntryTable[m_numEntries].userName = nsnull;
        m_searchNameCompletionEntryTable[m_numEntries].emailAddress = nsnull;
        m_searchNameCompletionEntryTable[m_numEntries].nickName = nsnull;
    
        nsCOMPtr<nsISupports> i;
        rv = aABCards->CurrentItem(getter_AddRefs(i));
        if (NS_FAILED(rv)) break;
    
        nsCOMPtr<nsIAbCard> card(do_QueryInterface(i, &rv));
        if (NS_FAILED(rv)) break;
    
	    /* card holds unicode string, convert to utf8 String for autocomplete*/
	    nsXPIDLString pUnicodeStr;
	    PRInt32 unicharLength = 0;
	    rv=card->GetDisplayName(getter_Copies(pUnicodeStr));
        if (NS_FAILED(rv)) break; 

	    unicharLength = nsCRT::strlen(pUnicodeStr);
	    INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&m_searchNameCompletionEntryTable[m_numEntries].userName);
    
        rv=card->GetPrimaryEmail(getter_Copies(pUnicodeStr));
        if (NS_FAILED(rv)) break;

	    unicharLength = nsCRT::strlen(pUnicodeStr);
	    INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&m_searchNameCompletionEntryTable[m_numEntries].emailAddress);
    
        rv=card->GetNickName(getter_Copies(pUnicodeStr));
        if (NS_FAILED(rv)) break;

		unicharLength = nsCRT::strlen(pUnicodeStr);
	    INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&m_searchNameCompletionEntryTable[m_numEntries].nickName);
    
        rv = aABCards->Next();
        m_numEntries++;
        m_tableInitialized = PR_TRUE;
    
        if (m_numEntries == MAX_ENTRIES)
            break;
    }

    return NS_OK;
}

nsresult nsAbAutoCompleteSession::InitializeTable()
{
#ifdef DEBUG_seth
  fprintf(stderr,"initializing autocomplete table\n");
#endif
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr <nsIRDFResource> resource;
  // this should not be hardcoded to abook.mab
  rv = rdfService->GetResource("abdirectory://abook.mab", getter_AddRefs(resource));
  if (NS_FAILED(rv)) return rv;
  
  // query interface 
  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(resource, &rv));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIEnumerator> cards;
  rv = directory->GetChildCards(getter_AddRefs(cards));
  if (NS_FAILED(rv)) return rv;

  rv = PopulateTableWithAB(cards);
  if (NS_FAILED(rv)) return rv;

  // now if we have any left over space, populate the table with the history address book

  rv = rdfService->GetResource("abdirectory://history.mab", getter_AddRefs(resource));
  if (NS_FAILED(rv)) return rv;
  
  directory = do_QueryInterface(resource, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = directory->GetChildCards(getter_AddRefs(cards));
  if (NS_FAILED(rv)) return rv;

  rv = PopulateTableWithAB(cards);
  if (NS_FAILED(rv)) return rv;

  return NS_OK; // always return success??
}

nsAbAutoCompleteSession::~nsAbAutoCompleteSession()
{
	PRInt32 i;
	for (i=0;i<m_numEntries;i++) {
      PR_FREEIF(m_searchNameCompletionEntryTable[i].userName);     
      m_searchNameCompletionEntryTable[i].userName = nsnull;
      PR_FREEIF(m_searchNameCompletionEntryTable[i].emailAddress);
      m_searchNameCompletionEntryTable[i].emailAddress = nsnull;
      PR_FREEIF(m_searchNameCompletionEntryTable[i].nickName);
      m_searchNameCompletionEntryTable[i].nickName = nsnull;
	}
}

NS_IMETHODIMP
nsAbAutoCompleteSession::AutoComplete(nsIMsgIdentity *aIdentity,
                                      nsISupports *aParam,
                                      const PRUnichar *aSearchString,
                                      nsIAbAutoCompleteListener *aResultListener)
{
	// mscott - right now I'm not even going to bother to make this synchronous...
	// I'll beef it up with some test data later but we want to see if this idea works for right now...
    
	nsresult rv = NS_OK;
    if (!m_tableInitialized) {
      rv = InitializeTable();
      if (NS_FAILED(rv)) return rv;
    }

    nsXPIDLCString domain;
    if (aIdentity) {
      nsXPIDLCString email;
      rv = aIdentity->GetEmail(getter_Copies(email));
      if (NS_SUCCEEDED(rv) && email) {

        char *domainStart = PL_strchr(email, '@');
        if (domainStart)
          domain = domainStart; // makes a copy
      }
    }
    
    if (nsCRT::strlen(aSearchString) == 0)
    	return NS_OK;

	if (aResultListener)
	{
		// get a mime header parser to generate a valid address
		nsCOMPtr<nsIMsgHeaderParser> parser = do_GetService(kHeaderParserCID);

		PRUint32 searchStringLen = nsCRT::strlen(aSearchString);
		PRInt32 nIndex;
		PRUnichar* matchResult = nsnull;

		for (nIndex = 0; nIndex < m_numEntries; nIndex++)
		{
			/* First look for a Nickname exact match... */
			if (nsCRT::strcasecmp(aSearchString, m_searchNameCompletionEntryTable[nIndex].nickName) == 0)
			{
				PRUnichar* searchResult = BuildSearchResult(nIndex, parser);
				if (searchResult)
				{
					rv = aResultListener->OnAutoCompleteResult(aParam, aSearchString, searchResult);
					nsCRT::free(searchResult);
					return rv;
				}
			}
			/* ... look for a username or email address match */
			if (!matchResult)
				if (nsCRT::strncasecmp(aSearchString, m_searchNameCompletionEntryTable[nIndex].userName, searchStringLen) == 0
					|| nsCRT::strncasecmp(aSearchString, m_searchNameCompletionEntryTable[nIndex].emailAddress,searchStringLen) == 0)
				{
					matchResult = BuildSearchResult(nIndex, parser);
				}
		}
		
		/* no exact nickname match, maybe we got another match? */
		if (matchResult)
		{
			rv = aResultListener->OnAutoCompleteResult(aParam, aSearchString, matchResult);
			nsCRT::free(matchResult);
			return rv;
		}

		/*no match at all,  do we have several names? */
		nsAutoString searchResult(aSearchString);
		char * cSearchString = searchResult.ToNewUTF8String();
		nsresult res;
		PRUint32 numAddresses;
		char	*names;
		char	*addresses;

		if (parser)
		{
			res = parser->ParseHeaderAddresses (nsnull, cSearchString, &names, &addresses, &numAddresses);
			if (NS_SUCCEEDED(res))
			{
				searchResult = "";
				PRUint32 i;
				char* pName = names;
				char* pAddr = addresses;
				nsAutoString fullAddress;
				for (i = 0; i < numAddresses; i ++)
				{
					if (! searchResult.IsEmpty())
						searchResult += ',';

					char * strFullAddr = nsnull;
					PRUnichar* uStr;
					PRInt32 uStrLen;
					parser->MakeFullAddress(nsnull, pName, pAddr, &strFullAddr);
					INTL_ConvertToUnicode(strFullAddr, nsCRT::strlen(strFullAddr), (void**)&uStr, &uStrLen);

					fullAddress = uStr;
					if (fullAddress.FindChar('@') < 0) /* no domain? */
						fullAddress += domain;		/* then add the default domain */
						
					searchResult += fullAddress;

					nsCRT::free(strFullAddr);
					nsCRT::free(uStr);

					pName += nsCRT::strlen(pName) + 1;
					pAddr += nsCRT::strlen(pAddr) + 1;
				}
			}
			PR_FREEIF(cSearchString);
		}

		return aResultListener->OnAutoCompleteResult(aParam, aSearchString, searchResult.GetUnicode());
	}
	else
		return NS_ERROR_NULL_POINTER;

}


PRUnichar* nsAbAutoCompleteSession::BuildSearchResult(PRUint32 nIndex, nsIMsgHeaderParser* parser)
{
	char * fullAddress = nsnull;
	if (parser)
		parser->MakeFullAddress(nsnull, m_searchNameCompletionEntryTable[nIndex].userName, 
								m_searchNameCompletionEntryTable[nIndex].emailAddress, &fullAddress);

	if (! fullAddress || ! *fullAddress)
		return nsnull;	/* something goes wrong with this entry, maybe the email address is missing.*/
	
	/* We need to convert back the result from UTF-8 to Unicode */
	PRUnichar* searchResult;
	PRInt32 searchResultLen;
	INTL_ConvertToUnicode(fullAddress, nsCRT::strlen(fullAddress), (void**)&searchResult, &searchResultLen);
	PR_Free(fullAddress);

	return searchResult;
}
