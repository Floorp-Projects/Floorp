/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsMsgComposeStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsIIOService.h"
#include "nsIURI.h"

/* This is the next generation string retrieval call */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define COMPOSE_BE_URL       "chrome://messengercompose/locale/composeMsgs.properties"

extern "C" 
PRUnichar *
ComposeGetStringByID(PRInt32 stringID)
{
	nsresult    res;
	char*       propertyURL = NULL;
	nsString	resultString = "???";

	propertyURL = COMPOSE_BE_URL;

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
				resultString = "[StringID";
				resultString.Append(stringID, 10);
				resultString += "?]";
				return resultString.ToNewUnicode();
			}

			return ptrv;
		}
	}

	return resultString.ToNewUnicode();
}
