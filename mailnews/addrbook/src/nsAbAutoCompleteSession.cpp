/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsAbAutoCompleteSession.h"
#include "nsString.h"
#include "nsIMsgHeaderParser.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsIAbDirectory.h"
#include "nsIAbCard.h"

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
			return abSession->QueryInterface(nsCOMTypeInfo<nsIAutoCompleteSession>::GetIID(), aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

NS_IMPL_ISUPPORTS(nsAbAutoCompleteSession, nsCOMTypeInfo<nsIAutoCompleteSession>::GetIID())

nsAbAutoCompleteSession::nsAbAutoCompleteSession()
{
	NS_INIT_REFCNT();

    m_tableInitialized = PR_FALSE;
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
  rv = rdfService->GetResource("abdirectory://abook.mab", getter_AddRefs(resource));
  if (NS_FAILED(rv)) return rv;
  
  // query interface 
  nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(resource, &rv));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIEnumerator> cards;
  rv = directory->GetChildCards(getter_AddRefs(cards));
  if (NS_FAILED(rv)) return rv;
  
  m_numEntries = 0;
  rv = cards->First();
  while (NS_SUCCEEDED(rv)) {
    m_searchNameCompletionEntryTable[m_numEntries].userName = nsnull;
    m_searchNameCompletionEntryTable[m_numEntries].emailAddress = nsnull;
    
    nsCOMPtr<nsISupports> i;
    rv = cards->CurrentItem(getter_AddRefs(i));
    if (NS_FAILED(rv)) break;
    
    nsCOMPtr<nsIAbCard> card(do_QueryInterface(i, &rv));
    if (NS_FAILED(rv)) break;
    
    rv=card->GetDisplayName(&m_searchNameCompletionEntryTable[m_numEntries].userName);
    if (NS_FAILED(rv)) {
      m_searchNameCompletionEntryTable[m_numEntries].userName = nsnull;
      break;
    }
    
    rv=card->GetPrimaryEmail(&m_searchNameCompletionEntryTable[m_numEntries].emailAddress);
    if (NS_FAILED(rv)) {
      m_searchNameCompletionEntryTable[m_numEntries].emailAddress = nsnull;
      break;
    }
    
    rv = cards->Next();
    m_numEntries++;
    m_tableInitialized = PR_TRUE;
    
    if (m_numEntries == MAX_ENTRIES) {
      break;
    }
  }

  return NS_OK;
}

nsAbAutoCompleteSession::~nsAbAutoCompleteSession()
{
	PRInt32 i;
	for (i=0;i<m_numEntries;i++) {
      PR_FREEIF(m_searchNameCompletionEntryTable[m_numEntries].userName);     
      m_searchNameCompletionEntryTable[m_numEntries].userName = nsnull;
      PR_FREEIF(m_searchNameCompletionEntryTable[m_numEntries].emailAddress);
      m_searchNameCompletionEntryTable[m_numEntries].emailAddress = nsnull;
	}
}

NS_IMETHODIMP nsAbAutoCompleteSession::AutoComplete(const PRUnichar *aDocId, const PRUnichar *aSearchString, nsIAutoCompleteListener *aResultListener)
{
	// mscott - right now I'm not even going to bother to make this synchronous...
	// I'll beef it up with some test data later but we want to see if this idea works for right now...
    
	nsresult rv = NS_OK;
    if (!m_tableInitialized) {
      rv = InitializeTable();
      if (NS_FAILED(rv)) return rv;
    }

    if (m_numEntries == 0) {
      return NS_OK;
    }

	if (aResultListener)
	{
		PRUint32 searchStringLen = nsCRT::strlen(aSearchString);
		PRBool matchFound = PR_FALSE;
		PRInt32 nIndex;
		for (nIndex = 0; nIndex < m_numEntries && !matchFound; nIndex++)
		{
			if (nsCRT::strncasecmp(aSearchString, m_searchNameCompletionEntryTable[nIndex].userName, searchStringLen) == 0
				|| nsCRT::strncasecmp(aSearchString, m_searchNameCompletionEntryTable[nIndex].emailAddress,searchStringLen) == 0)
			{
				matchFound = PR_TRUE; // so we kick out of the loop

				// get a mime header parser to generate a valid address
				nsCOMPtr<nsIMsgHeaderParser> parser;
				nsComponentManager::CreateInstance(kHeaderParserCID,
													nsnull,
													nsCOMTypeInfo<nsIMsgHeaderParser>::GetIID(),
													getter_AddRefs(parser));

				char * fullAddress = nsnull;
				if (parser)
					parser->MakeFullAddress(nsnull, m_searchNameCompletionEntryTable[nIndex].userName, 
											m_searchNameCompletionEntryTable[nIndex].emailAddress, &fullAddress);
				nsString searchResult(fullAddress);
				// iterate over the table looking for a match
				rv = aResultListener->OnAutoCompleteResult(aDocId, aSearchString, searchResult.GetUnicode());
				break;
			}
		}

	}
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}
