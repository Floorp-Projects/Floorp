/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Epstein <depstein@netscape.com>
 *   Ashish Bhatt <ashishbhatt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// nsIObserServ.cpp: implementation of the CnsIObserServ class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "nsIObserServ.h"
#include "QaUtils.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CnsIObserServ::CnsIObserServ()

{
	mRefCnt = 1 ;
}


CnsIObserServ::~CnsIObserServ()
{

}

ObserverElement ObserverTable[] = {
	{"profile-before-change", PR_TRUE},
    {"profile-after-change", PR_FALSE},
	{"profile-do-change", PR_TRUE},
	{"text/xml", PR_FALSE},
	{"application/xml", PR_TRUE},
	{"htmlparser", PR_FALSE},
	{"xmlparser", PR_TRUE},	
	{"memory-pressure", PR_FALSE},
	{"quit-application", PR_TRUE},
	{"login-failed", PR_FALSE}	
}; 


void CnsIObserServ::OnStartTests(UINT nMenuID)

{
	// Calls  all or indivdual test cases on the basis of the 
	// option selected from menu.

	switch(nMenuID)
	{

	case ID_INTERFACES_NSIOBSERVERSERVICE_RUNALLTESTS :
		RunAllTests();
		break;

	case ID_INTERFACES_NSIOBSERVERSERVICE_ADDOBSERVERS :
		AddObserversTest(2);
		break;

	case ID_INTERFACES_NSIOBSERVERSERVICE_ENUMERATEOBSERVERS :
		QAOutput("Adding observers first", 2);
		AddObserversTest(1);		
		EnumerateObserversTest(1);
		break;

	case ID_INTERFACES_NSIOBSERVERSERVICE_NOTIFYOBSERVERS :
		NotifyObserversTest(1);
		break;

	case ID_INTERFACES_NSIOBSERVERSERVICE_REMOVEOBSERVERS :
		QAOutput("Adding observers first.", 2);
		AddObserversTest(1);
		RemoveObserversTest(1);
		break;

	default :
		AfxMessageBox("Menu handler not added for this menu item");
		break;
	}
}

void CnsIObserServ::RunAllTests()
{
	AddObserversTest(1);
	EnumerateObserversTest(1);
	NotifyObserversTest(1);
	RemoveObserversTest(1);
}

void CnsIObserServ::AddObserversTest(int displayType)
{
	int i;

	nsCOMPtr<nsIObserverService>observerService(do_GetService("@mozilla.org/observer-service;1",&rv));
	RvTestResult(rv, "nsIObserverService object test", displayType);
	RvTestResultDlg(rv, "nsIObserverService object test", true);

	QAOutput("\n nsIObserverService::AddObserversTest().");
	if (!observerService) 
	{
		QAOutput("Can't get nsIObserverService object. Tests fail.");
		return;
	}

	for (i=0; i<10; i++)
	{
		rv = observerService->AddObserver(this, ObserverTable[i].theTopic, 
									      ObserverTable[i].theOwnsWeak);
		FormatAndPrintOutput("The observer to be added = ", ObserverTable[i].theTopic, 1);	
		RvTestResult(rv, "AddObservers() test", displayType);
		RvTestResultDlg(rv, "AddObservers() test");
	}
}

void CnsIObserServ::RemoveObserversTest(int displayType)
{
	int i;

	nsCOMPtr<nsIObserverService>observerService(do_GetService("@mozilla.org/observer-service;1",&rv));

	QAOutput("\n nsIObserverService::RemoveObserversTest().");
	if (!observerService) 
	{
		QAOutput("Can't get nsIObserverService object. Tests fail.");
		return;
	}

	for (i=0; i<10; i++)
	{
		rv = observerService->RemoveObserver(this, ObserverTable[i].theTopic);
		FormatAndPrintOutput("The observer to be removed = ", ObserverTable[i].theTopic, 1);	
		RvTestResult(rv, "RemoveObservers() test", displayType);
		RvTestResultDlg(rv, "RemoveObservers() test");
	}
}


void CnsIObserServ::NotifyObserversTest(int displayType)
{
	PRInt32 i;
	nsCOMPtr<nsIObserverService>observerService(do_GetService("@mozilla.org/observer-service;1",&rv));

	QAOutput("\n nsIObserverService::NotifyObserversTest().");

	if (!observerService) 
	{
		QAOutput("Can't get nsIObserverService object. Tests fail.");
		return;
	}

	for (i=0; i<10; i++)
	{
		FormatAndPrintOutput("The notified observer = ", ObserverTable[i].theTopic, 1);
		rv = observerService->NotifyObservers(nsnull, ObserverTable[i].theTopic, 0);
		RvTestResult(rv, "NotifyObservers() test", displayType);
		RvTestResultDlg(rv, "NotifyObservers() test");
	}
}


void CnsIObserServ::EnumerateObserversTest(int displayType)
{
	PRInt32 i=0;
	nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1",&rv));
	nsCOMPtr<nsISimpleEnumerator> simpleEnum;

	QAOutput("\n nsIObserverService::EnumerateObserversTest().");
	if (!observerService) 
	{
		QAOutput("Can't get nsIObserverService object. Tests fail.");
		return;
	}

	for (i=0; i<10; i++)
	{
		// need to handle Simple Enumerator

		rv = observerService->EnumerateObservers(ObserverTable[i].theTopic, 
												 getter_AddRefs(simpleEnum));

		RvTestResult(rv, "EnumerateObserversTest() test", displayType);
		RvTestResultDlg(rv, "EnumerateObserversTest() test");
		if (!simpleEnum)
		{
			QAOutput("Didn't get SimpleEnumerator object. Tests fail.");
			return;
		}

		nsCOMPtr<nsIObserver> observer;
		PRBool theLoop = PR_TRUE;
		PRBool bLoop = PR_TRUE;

		while( NS_SUCCEEDED(simpleEnum->HasMoreElements(&theLoop)) && bLoop) 
		{
			simpleEnum->GetNext(getter_AddRefs(observer));

			if (!observer)
			{
				QAOutput("Didn't get the observer object. Tests fail.");
				return;
			}
			rv = observer->Observe(observer, ObserverTable[i].theTopic, 0);
			RvTestResult(rv, "nsIObserver() test", 1);	
			RvTestResultDlg(rv, "nsIObserver() test");
			
			// compare 'this' with observer object
			if( this == NS_REINTERPRET_CAST(CnsIObserServ*,NS_REINTERPRET_CAST(void*, observer.get())))
			{
				QAOutput("observers match. Test passes.");
				bLoop = PR_FALSE;
			}
			else
				QAOutput("observers don't match. Test fails.");
		}
	}
}


NS_IMPL_THREADSAFE_ISUPPORTS2(CnsIObserServ,  nsIObserver,  nsISupportsWeakReference)


NS_IMETHODIMP CnsIObserServ::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
    nsresult rv = NS_OK;
    
    if (nsCRT::strcmp(aTopic, "profile-before-change") == 0)
    {
 		QAOutput("Observed 'profile-before-change'.");
    }
    else if (nsCRT::strcmp(aTopic, "profile-after-change") == 0)
    {
 		QAOutput("Observed 'profile-after-change'.");
    }
	else if (nsCRT::strcmp(aTopic, "profile-do-change") == 0)
    {
 		QAOutput("Observed 'profile-do-change'.");
    }
    else if (nsCRT::strcmp(aTopic, "text/xml") == 0)
    {
 		QAOutput("Observed 'text/xml'.");
    }
	else if (nsCRT::strcmp(aTopic, "application/xml") == 0)
    {
 		QAOutput("Observed 'application/xml'.");
    }
    else if (nsCRT::strcmp(aTopic, "htmlparser") == 0)
    {
 		QAOutput("Observed 'htmlparser'.");
    }
	else if (nsCRT::strcmp(aTopic, "xmlparser") == 0)
    {
 		QAOutput("Observed 'xmlparser'.");
    }
	else if (nsCRT::strcmp(aTopic, "memory-pressure") == 0)
    {
 		QAOutput("Observed 'memory-pressure'.");
    }
	else if (nsCRT::strcmp(aTopic, "quit-application") == 0)
    {
 		QAOutput("Observed 'quit-application'.");
    }
	else if (nsCRT::strcmp(aTopic, "login-failed") == 0)
    {
 		QAOutput("Observed 'login-failed'.");
    }

    return rv;
}
