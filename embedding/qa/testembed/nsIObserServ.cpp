/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
	{"session-logout", PR_FALSE}	
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
		AddObserversTest();
		break;
	case ID_INTERFACES_NSIOBSERVERSERVICE_ENUMERATEOBSERVERS : 
		EnumerateObserversTest();
		break;
	case ID_INTERFACES_NSIOBSERVERSERVICE_NOTIFYOBSERVERS :
		NotifyObserversTest();
		break;
	case ID_INTERFACES_NSIOBSERVERSERVICE_REMOVEOBSERVERS :
		RemoveObserversTest();
		break;
	default :
		AfxMessageBox("Not added menu handler for this menu item");
		break;
	}
}
void CnsIObserServ::RunAllTests()
{
	AddObserversTest();
	EnumerateObserversTest();
	NotifyObserversTest();
	RemoveObserversTest();
}

void CnsIObserServ::AddObserversTest()
{
	int i;

	nsCOMPtr<nsIObserverService>observerService(do_GetService("@mozilla.org/observer-service;1",&rv));

	QAOutput("nsIObserverService::AddObserversTest().");
	if (!observerService) 
	{
		QAOutput("Can't get nsIObserverService object. Tests fail.");
		return;
	}

	observerService->AddObserver(this, "text/xml", PR_TRUE);

	for (i=0; i<10; i++)
	{
		rv = observerService->AddObserver(this, ObserverTable[i].theTopic, 
									 ObserverTable[i].theOwnsWeak);
		RvTestResult(rv, "AddObservers() test", 2);
	}

}

void CnsIObserServ::RemoveObserversTest()
{
	int i;

	nsCOMPtr<nsIObserverService>observerService(do_GetService("@mozilla.org/observer-service;1",&rv));

	QAOutput("nsIObserverService::RemoveObserversTest().");
	if (!observerService) 
	{
		QAOutput("Can't get nsIObserverService object. Tests fail.");
		return;
	}

	AddObserversTest();

	for (i=0; i<10; i++)
	{
		rv = observerService->RemoveObserver(this, ObserverTable[i].theTopic);
		RvTestResult(rv, "RemoveObservers() test", 2);
	}
}

void CnsIObserServ::NotifyObserversTest()
{
	QAOutput("nsIObserverService::NotifyObserversTest().");
}


void CnsIObserServ::EnumerateObserversTest()
{
	PRInt32 i=0;
	nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1",&rv));
	nsCOMPtr<nsISimpleEnumerator> simpleEnum;

	QAOutput("nsIObserverService::EnumerateObserversTest().");
	if (!observerService) 
	{
		QAOutput("Can't get nsIObserverService object. Tests fail.");
		return;
	}

	AddObserversTest();

	for (i=0; i<10; i++)
	{
		// need to handle Simple Enumerator
		rv = observerService->EnumerateObservers(ObserverTable[i].theTopic, 
												 getter_AddRefs(simpleEnum));

		RvTestResult(rv, "EnumerateObserversTest() test", 2);
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

			rv = observer->Observe(observer, ObserverTable[i].theTopic, 0);
			RvTestResult(rv, "Observer() test", 2);	
			
			// compare 'this' with observer object
	//		if (this ==(CnsIObserServ *)observer)


			if( this == NS_REINTERPRET_CAST(CnsIObserServ*,NS_REINTERPRET_CAST(void*, observer.get())))
				QAOutput("match. Test passes.");
			else
				QAOutput("don't match. Test fails.");
		}
	}
}


NS_IMPL_THREADSAFE_ISUPPORTS2(CnsIObserServ,  nsIObserver,  nsISupportsWeakReference);


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
	else if (nsCRT::strcmp(aTopic, "session-logout") == 0)
    {
 		QAOutput("Observed 'session-logout'.");
    }

    return rv;
}
