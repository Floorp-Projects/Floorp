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
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsImapStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"

/* This is the next generation string retrieval call */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define IMAP_MSGS_URL       "chrome://messenger/locale/imapMsgs.properties"

extern "C" 
PRUnichar *
IMAPGetStringByID(PRInt32 stringID)
{
	nsresult    res;
	char*       propertyURL = NULL;
	nsString	resultString; resultString.AssignWithConversion("???");

	propertyURL = IMAP_MSGS_URL;

	NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
	if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
	{
		nsILocale   *locale = nsnull;

		nsIStringBundle* sBundle = nsnull;
		res = sBundleService->CreateBundle(propertyURL, locale, &sBundle);

		if (NS_SUCCEEDED(res))
		{
			PRUnichar *ptrv = nsnull;
			res = sBundle->GetStringFromID(stringID, &ptrv);

			NS_RELEASE(sBundle);

			if (NS_FAILED(res)) 
			{
				resultString.AssignWithConversion("[StringID");
				resultString.AppendInt(stringID, 10);
				resultString.AppendWithConversion("?]");
				return resultString.ToNewUnicode();
			}

			return ptrv;
		}
	}

	return resultString.ToNewUnicode();
}

