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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIStringBundle.h"
#include "nsTextStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"
#include "nsIURI.h"

/* This is the next generation string retrieval call */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

nsIStringBundle *	nsTextStringBundle::m_pBundle = nsnull;

#define TEXT_MSGS_URL       "chrome://messenger/locale/textImportMsgs.properties"

nsIStringBundle *nsTextStringBundle::GetStringBundle( void)
{
	if (m_pBundle)
		return( m_pBundle);

	nsresult			rv;
	static const char	propertyURL[] = TEXT_MSGS_URL;
	nsIStringBundle*	sBundle = nsnull;


	nsCOMPtr<nsIStringBundleService> sBundleService = 
	         do_GetService(kStringBundleServiceCID, &rv); 
	if (NS_SUCCEEDED(rv) && (nsnull != sBundleService)) {
		rv = sBundleService->CreateBundle(propertyURL, &sBundle);
	}

	m_pBundle = sBundle;

	return( sBundle);
}

nsIStringBundle *nsTextStringBundle::GetStringBundleProxy( void)
{
	if (!m_pBundle)
		return( nsnull);

	nsIStringBundle *strProxy = nsnull;
	nsresult rv;
	// create a proxy object if we aren't on the same thread?
	nsCOMPtr<nsIProxyObjectManager> proxyMgr = 
	         do_GetService(kProxyObjectManagerCID, &rv);
	if (NS_SUCCEEDED(rv)) {
		rv = proxyMgr->GetProxyForObject( NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIStringBundle),
										m_pBundle, PROXY_SYNC | PROXY_ALWAYS, (void **) &strProxy);
	}

	return( strProxy);
}

void nsTextStringBundle::GetStringByID( PRInt32 stringID, nsString& result, nsIStringBundle *pBundle)
{
	
	PRUnichar *ptrv = GetStringByID( stringID, pBundle);	
	result = ptrv;
	FreeString( ptrv);
}

PRUnichar *nsTextStringBundle::GetStringByID(PRInt32 stringID, nsIStringBundle *pBundle)
{
	if (!pBundle) {
		pBundle = GetStringBundle();
	}
	
	if (pBundle) {
		PRUnichar *ptrv = nsnull;
		nsresult rv = pBundle->GetStringFromID(stringID, &ptrv);
				
		if (NS_SUCCEEDED( rv) && ptrv)
			return( ptrv);
	}

	nsString resultString; resultString.AssignWithConversion( "[StringID ");
	resultString.AppendInt(stringID, 10);
	resultString.AppendWithConversion("?]");

	return ToNewUnicode(resultString);
}

void nsTextStringBundle::Cleanup( void)
{
	if (m_pBundle)
		m_pBundle->Release();
	m_pBundle = nsnull;
}
