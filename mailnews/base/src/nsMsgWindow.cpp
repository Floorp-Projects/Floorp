/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsMsgWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsTransactionManagerCID.h"
#include "nsIComponentManager.h"

static NS_DEFINE_CID(kTransactionManagerCID, NS_TRANSACTIONMANAGER_CID);
static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);

NS_IMPL_ISUPPORTS(nsMsgWindow, nsCOMTypeInfo<nsIMsgWindow>::GetIID())

nsMsgWindow::nsMsgWindow()
{
	NS_INIT_REFCNT();
	mRootWebShell = nsnull;
    Init();
}

nsMsgWindow::~nsMsgWindow()
{

}

nsresult nsMsgWindow::Init()
{
    nsresult rv = NS_OK;
    // create Undo/Redo Transaction Manager
    NS_WITH_SERVICE (nsIComponentManager, compMgr, kComponentManagerCID, &rv);

    if (NS_SUCCEEDED(rv))
    {
        rv = compMgr->CreateInstance(kTransactionManagerCID, nsnull,
                       nsCOMTypeInfo<nsITransactionManager>::GetIID(),
                                     getter_AddRefs(mTransactionManager));
      if (NS_SUCCEEDED(rv))
        mTransactionManager->SetMaxTransactionCount(-1);
    }
    return rv;
}

NS_IMETHODIMP nsMsgWindow::GetStatusFeedback(nsIMsgStatusFeedback * *aStatusFeedback)
{
	if(!aStatusFeedback)
		return NS_ERROR_NULL_POINTER;

	*aStatusFeedback = mStatusFeedback;
	NS_IF_ADDREF(*aStatusFeedback);
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetStatusFeedback(nsIMsgStatusFeedback * aStatusFeedback)
{
	mStatusFeedback = aStatusFeedback;
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetTransactionManager(nsITransactionManager * *aTransactionManager)
{
	if(!aTransactionManager)
		return NS_ERROR_NULL_POINTER;

	*aTransactionManager = mTransactionManager;
	NS_IF_ADDREF(*aTransactionManager);
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetTransactionManager(nsITransactionManager * aTransactionManager)
{
	mTransactionManager = aTransactionManager;
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetMessageView(nsIMessageView * *aMessageView)
{
	if(!aMessageView)
		return NS_ERROR_NULL_POINTER;

	*aMessageView = mMessageView;
	NS_IF_ADDREF(*aMessageView);
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetMessageView(nsIMessageView * aMessageView)
{
	mMessageView = aMessageView;
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetOpenFolder(nsIMsgFolder * *aOpenFolder)
{
	if(!aOpenFolder)
		return NS_ERROR_NULL_POINTER;

	*aOpenFolder = mOpenFolder;
	NS_IF_ADDREF(*aOpenFolder);
	return NS_OK;

}

NS_IMETHODIMP nsMsgWindow::SetOpenFolder(nsIMsgFolder * aOpenFolder)
{
	mOpenFolder = aOpenFolder;
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetRootWebShell(nsIWebShell * *aWebShell)
{
	if(!aWebShell)
		return NS_ERROR_NULL_POINTER;

	*aWebShell = mRootWebShell;
	NS_IF_ADDREF(*aWebShell);
	return NS_OK;

}

NS_IMETHODIMP nsMsgWindow::SetRootWebShell(nsIWebShell * aWebShell)
{
	mRootWebShell = aWebShell;
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetDOMWindow(nsIDOMWindow *aWindow)
{
	if (!aWindow)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;

	nsCOMPtr<nsIScriptGlobalObject>
		globalScript(do_QueryInterface(aWindow));
	nsCOMPtr<nsIWebShell> webshell, rootWebshell;
	if (globalScript)
		globalScript->GetWebShell(getter_AddRefs(webshell));
	if (webshell)
	{
		webshell->GetRootWebShell(mRootWebShell);
		nsIWebShell *root = mRootWebShell;
		NS_RELEASE(root); // don't hold reference
	}
	return rv;
}

NS_IMETHODIMP nsMsgWindow::StopUrls()
{
	if (mRootWebShell)
		return mRootWebShell->Stop();
	return NS_ERROR_NULL_POINTER;
}

