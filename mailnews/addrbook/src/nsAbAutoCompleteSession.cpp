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

nsresult NS_NewAbAutoCompleteSession(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (aInstancePtrResult)
	{
		nsAbAutoCompleteSession * abSession = new nsAbAutoCompleteSession(); 
		if (abSession)
			return abSession->QueryInterface(nsCOMTypeInfo<nsIAbAutoCompleteSession>::GetIID(), aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

NS_IMPL_ISUPPORTS(nsAbAutoCompleteSession, nsCOMTypeInfo<nsIAbAutoCompleteSession>::GetIID())

nsAbAutoCompleteSession::nsAbAutoCompleteSession()
{
	NS_INIT_REFCNT;
}

nsAbAutoCompleteSession::~nsAbAutoCompleteSession()
{}

NS_IMETHODIMP nsAbAutoCompleteSession::AutoComplete(const PRUnichar *aSearchString, nsIAutoCompleteListener *aResultListener)
{
	// mscott - right now I'm not even going to bother to make this synchronous...
	// I'll beef it up with some test data later but we want to see if this idea works for right now...

	nsresult rv = NS_OK;
	nsString2 searchResult("Scott MacGregor <mscott@netscape.com>");
	if (aResultListener)
		rv = aResultListener->OnAutoCompleteResult(aSearchString, searchResult.GetUnicode());
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}
