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
#include "nsString2.h"
#include "nsIMsgHeaderParser.h"

static NS_DEFINE_CID(kHeaderParserCID, NS_MSGHEADERPARSER_CID);

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
}

nsAbAutoCompleteSession::~nsAbAutoCompleteSession()
{}


typedef struct
{
	char * userName;
	char * emailAddress;
} nsAbStubEntry;

nsAbStubEntry SearchNameCompletionEntryTable[] =
{
    {"Scott MacGregor",		"mscott@netscape.com"},
    {"Seth Spitzer",		"sspitzer@netscape.com"},
    {"Scott Putterman",		"scottip@netscape.com"},
    {"mailnewsstaff",		"mailnewsstaff@netscape.com"},
    {"David Bienvenu",		"bievenu@netscape.com"},
    {"Jeff Tsai",		"jefft@netscape.com"},	
    {"Alec Flett",		"alecf@netscape.com"},
    {"Candice Huang",		"chuang@netscape.com"},
    {"Jean-Francois Ducarroz",	"ducarroz@netscape.com"},
    {"Esther Goes",		"esther@netscape.com"},
    {"Rich Pizzaro",		"rhp@netscape.com"},
    {"Par Pandit",		"ppandit@netscape.com"},
    {"mailnewsqa",		"mailnewsqa@netscape.com"},	
    {"Ninoschka Baca",		"nbaca@netscape.com"},
    {"Lisa Chiang",		"lchiang@netscape.com"},
    {"Fenella Gor ",		"fenella@netscape.com"},
    {"Peter Mock",		"pmock@netscape.com"},	
    {"Suresh Kasinathan",	"suresh@netscape.com"},
    {"Paul Hangas",		"hangas@netscape.com"},
    {"David Hyatt",		"hyatt@netscape.com"},
    {"Chris Waterson",		"waterson@netscape.com"},
    {"Phil Peterson",		"phil@netscape.com"},
    {"Syd Logan",		"syd@netscape.com"},
    {"Dave Rothschild",		"daver@netscape.com"}
};

const PRInt32 numStubEntries = 24;


NS_IMETHODIMP nsAbAutoCompleteSession::AutoComplete(const PRUnichar *aDocId, const PRUnichar *aSearchString, nsIAutoCompleteListener *aResultListener)
{
	// mscott - right now I'm not even going to bother to make this synchronous...
	// I'll beef it up with some test data later but we want to see if this idea works for right now...

	nsresult rv = NS_OK;
	if (aResultListener)
	{
		PRUint32 searchStringLen = nsCRT::strlen(aSearchString);
		PRBool matchFound = PR_FALSE;
		for (PRInt32 index = 0; index < numStubEntries && !matchFound; index++)
		{
			if (nsCRT::strncasecmp(aSearchString, SearchNameCompletionEntryTable[index].userName, searchStringLen) == 0
				|| nsCRT::strncasecmp(aSearchString, SearchNameCompletionEntryTable[index].emailAddress,searchStringLen) == 0)
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
					parser->MakeFullAddress(nsnull, SearchNameCompletionEntryTable[index].userName, 
											SearchNameCompletionEntryTable[index].emailAddress, &fullAddress);
				nsString2 searchResult(fullAddress);
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
