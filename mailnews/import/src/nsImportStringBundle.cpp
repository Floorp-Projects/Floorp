/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */
#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsImportStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"

/* This is the next generation string retrieval call */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define IMPORT_MSGS_URL       "chrome://messenger/locale/importMsgs.properties"

nsIStringBundle *nsImportStringBundle::GetStringBundle( void)
{
	nsresult			rv;
	char*				propertyURL = IMPORT_MSGS_URL;
	nsIStringBundle*	sBundle = nsnull;


	NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &rv); 
	if (NS_SUCCEEDED(rv) && (nsnull != sBundleService)) {
		nsILocale   *		locale = nsnull;
		rv = sBundleService->CreateBundle(propertyURL, locale, &sBundle);
	}

	return( sBundle);
}

void nsImportStringBundle::GetStringByID( PRInt32 stringID, nsString& result, nsIStringBundle *pBundle)
{
	
	PRUnichar *ptrv = GetStringByID( stringID, pBundle);	
	result = ptrv;
	FreeString( ptrv);
}

PRUnichar *nsImportStringBundle::GetStringByID(PRInt32 stringID, nsIStringBundle *pBundle)
{
	PRBool	mine = PR_FALSE;
	if (!pBundle) {
		mine = PR_TRUE;
		pBundle = GetStringBundle();
	}
	
	if (pBundle) {
		PRUnichar *ptrv = nsnull;
		nsresult rv = pBundle->GetStringFromID(stringID, &ptrv);
		
		if (mine) {
			NS_RELEASE(pBundle);
		}
		
		if (NS_SUCCEEDED( rv) && ptrv)
			return( ptrv);
	}

	nsString resultString( "[StringID ");
	resultString.Append(stringID, 10);
	resultString += "?]";

	return( resultString.ToNewUnicode());
}

