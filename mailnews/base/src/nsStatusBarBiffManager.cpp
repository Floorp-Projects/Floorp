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
 */

#include "nsStatusBarBiffManager.h"
#include "nsMsgBiffManager.h"
#include "nsCRT.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsIObserverService.h"
#include "nsIWindowMediator.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMElement.h"
#include "nsIMsgMailSession.h"
#include "nsMsgFolderDataSource.h"
#include "MailNewsTypes.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...

static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_IID(kIWindowMediatorIID,   NS_IWINDOWMEDIATOR_IID);
static NS_DEFINE_CID(kWindowMediatorCID,    NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID(kMsgMailSessionCID,    NS_MSGMAILSESSION_CID);

// QueryInterface, AddRef, and Release
//
NS_IMPL_ISUPPORTS1(nsStatusBarBiffManager, nsIFolderListener)

nsIAtom * nsStatusBarBiffManager::kBiffStateAtom = nsnull;

nsStatusBarBiffManager::nsStatusBarBiffManager()
: mInitialized(PR_FALSE), mCurrentBiffState(nsIMsgFolder::nsMsgBiffState_NoMail)
{
	NS_INIT_ISUPPORTS();
}

nsStatusBarBiffManager::~nsStatusBarBiffManager()
{
    NS_RELEASE(kBiffStateAtom);
}

nsresult nsStatusBarBiffManager::Init()
{
	if (mInitialized)
		return NS_ERROR_ALREADY_INITIALIZED;

	nsresult rv;

    kBiffStateAtom               = NS_NewAtom("BiffState");

	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->AddFolderListener(this);

	mInitialized = PR_TRUE;
	return NS_OK;
}

nsresult nsStatusBarBiffManager::Shutdown()
{
	return NS_OK;
}

nsresult nsStatusBarBiffManager::PerformStatusBarBiff(PRUint32 newBiffFlag)
{

	// See nsMsgStatusFeedback
	nsresult rv;
	NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
	nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

	// why use DOM window enumerator instead of XUL window...????
	if (NS_SUCCEEDED(windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator))))
	{ 
		PRBool more;
	
		windowEnumerator->HasMoreElements(&more);

		while(more)
		{
			nsCOMPtr<nsISupports> nextWindow = nsnull;
			windowEnumerator->GetNext(getter_AddRefs(nextWindow));
			nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(nextWindow));
			NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

			nsCOMPtr<nsIDOMDocument> domDocument;
			domWindow->GetDocument(getter_AddRefs(domDocument));

			if(domDocument)
			{
				nsCOMPtr<nsIDOMElement> domElement;
				domDocument->GetElementById(NS_ConvertASCIItoUCS2("mini-mail"), getter_AddRefs(domElement));

				if (domElement) {
					if (newBiffFlag == nsIMsgFolder::nsMsgBiffState_NewMail) {
						domElement->SetAttribute(NS_ConvertASCIItoUCS2("BiffState"), NS_ConvertASCIItoUCS2("NewMail"));
					}
					else if (newBiffFlag == nsIMsgFolder::nsMsgBiffState_NoMail){
						domElement->RemoveAttribute(NS_ConvertASCIItoUCS2("BiffState"));
					}
				}
			}
			
			windowEnumerator->HasMoreElements(&more);
		}

	}

	return NS_OK;
}

// nsIFolderListener methods....
NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemAdded(nsISupports *parentItem, nsISupports *item, const char *viewString)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemRemoved(nsISupports *parentItem, nsISupports *item, const char *viewString)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemPropertyChanged(nsISupports *item, nsIAtom *property, const char *oldValue, const char *newValue)
{
	return NS_OK;
}

NS_IMETHODIMP
nsStatusBarBiffManager::OnItemIntPropertyChanged(nsISupports *item, nsIAtom *property, PRInt32 oldValue, PRInt32 newValue)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemBoolPropertyChanged(nsISupports *item, nsIAtom *property, PRBool oldValue, PRBool newValue)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemUnicharPropertyChanged(nsISupports *item, nsIAtom *property, const PRUnichar *oldValue, const PRUnichar *newValue)
{
	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemPropertyFlagChanged(nsISupports *item, nsIAtom *property, PRUint32 oldFlag, PRUint32 newFlag)
{
	if (kBiffStateAtom == property)
	{
		if (mCurrentBiffState != newFlag) {
			PerformStatusBarBiff(newFlag);
			mCurrentBiffState = newFlag;
		}
	}

	return NS_OK;
}

NS_IMETHODIMP 
nsStatusBarBiffManager::OnItemEvent(nsIFolder *item, nsIAtom *event)
{
	return NS_OK;
}
 
