/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsMsgWindow.h"
#include "nsIURILoader.h"
#include "nsCURILoader.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsTransactionManagerCID.h"
#include "nsIComponentManager.h"

static NS_DEFINE_CID(kTransactionManagerCID, NS_TRANSACTIONMANAGER_CID);
static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);

NS_IMPL_ISUPPORTS2(nsMsgWindow, nsIMsgWindow, nsIURIContentListener)

nsMsgWindow::nsMsgWindow()
{
	NS_INIT_ISUPPORTS();
	mRootWebShell = nsnull;
  mMessageWindowWebShell = nsnull;
  Init();
}

nsMsgWindow::~nsMsgWindow()
{
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_PROGID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->UnRegisterContentListener(this);
}

nsresult nsMsgWindow::Init()
{
  // register ourselves as a content listener with the uri dispatcher service
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_PROGID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->RegisterContentListener(this);

  // create Undo/Redo Transaction Manager
  NS_WITH_SERVICE (nsIComponentManager, compMgr, kComponentManagerCID, &rv);

  if (NS_SUCCEEDED(rv))
  {
      rv = compMgr->CreateInstance(kTransactionManagerCID, nsnull,
                                   NS_GET_IID(nsITransactionManager),
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
  if (mRootWebShell)
    mRootWebShell->SetParentURIContentListener(this);
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
    if (mRootWebShell)
      mRootWebShell->SetParentURIContentListener(this);


    nsAutoString  webShellName("messagepane");
    nsCOMPtr<nsIWebShell> msgWebShell;
    rv = mRootWebShell->FindChildWithName(webShellName.GetUnicode(), *getter_AddRefs(msgWebShell));
    // we don't own mMessageWindowWebShell so don't try to keep a reference to it!
    mMessageWindowWebShell = msgWebShell;
	}
	return rv;
}

NS_IMETHODIMP nsMsgWindow::StopUrls()
{
	if (mRootWebShell)
		return mRootWebShell->Stop();
	return NS_ERROR_NULL_POINTER;
}

// nsIURIContentListener support
NS_IMETHODIMP nsMsgWindow::GetProtocolHandler(nsIURI * /* aURI */, nsIProtocolHandler **aProtocolHandler)
{
   // we don't have any app specific protocol handlers we want to use so 
  // just use the system default by returning null.
  *aProtocolHandler = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::DoContent(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, 
                                     nsIChannel *aChannel, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
  if (aContentType)
  {
    // forward the DoContent call to our webshell
    nsCOMPtr<nsIURIContentListener> ctnListener = do_QueryInterface(mMessageWindowWebShell);
    if (ctnListener)
      return ctnListener->DoContent(aContentType, aCommand, aWindowTarget, aChannel, aContentHandler, aAbortProcess);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::CanHandleContent(const char * aContentType,
                                            nsURILoadCommand aCommand,
                                            const char * aWindowTarget,
                                            char ** aDesiredContentType,
                                            PRBool * aCanHandleContent)

{
  // the mail window is the primary content handler for the following types:
  // If we are asked to handle any of these types, we will always say Yes!
  // regardlesss of the uri load command.
  //    Incoming Type                     Preferred type
  //     message/rfc822                     text/xul
  //

  // the mail window can also show the following content types. However, it isn't
  // the primary content handler for these types. So we won't try to accept this content
  // unless the uri load command is viewNormal (which implies that we are trying to view a url inline)
  //    incoming Type                     Preferred type
  //      text/html                    
  //      text/xul
  //      image/gif
  //      image/jpeg
  //      image/png
  //      image/tiff


   *aCanHandleContent = PR_FALSE;

  // (1) list all content types we want to  be the primary handler for....
  // and suggest a desired content type if appropriate...
  if (aContentType && nsCRT::strcasecmp(aContentType, "message/rfc822") == 0)
  {
    // we can handle this content type...but we would prefer it to be 
    // as text/xul so we can display it...
    *aCanHandleContent = PR_TRUE;
    *aDesiredContentType = nsCRT::strdup("text/xul");
  }
  else
  {
    // (2) list all the content types we can handle IF we really have too....i.e.
    // if the load command is viewNormal. This includes text/xul, text/html right now.
    // I'm sure it will grow.
    
    // for right now, if the load command is viewNormal just say we can handle it...
    if (aCommand == nsIURILoader::viewNormal)
    {
       if (nsCRT::strcasecmp(aContentType, "multipart/x-mixed-replace") == 0)
       {
        *aDesiredContentType = nsCRT::strdup("text/html");
        *aCanHandleContent = PR_TRUE;
       }
       if (nsCRT::strcasecmp(aContentType,  "text/html") == 0
           || nsCRT::strcasecmp(aContentType, "text/xul") == 0
           || nsCRT::strcasecmp(aContentType, "text/rdf") == 0 
           || nsCRT::strcasecmp(aContentType, "text/xml") == 0
           || nsCRT::strcasecmp(aContentType, "text/css") == 0
           || nsCRT::strcasecmp(aContentType, "image/gif") == 0
           || nsCRT::strcasecmp(aContentType, "image/jpeg") == 0
           || nsCRT::strcasecmp(aContentType, "image/png") == 0
           || nsCRT::strcasecmp(aContentType, "image/tiff") == 0
           || nsCRT::strcasecmp(aContentType, "application/http-index-format") == 0)
           *aCanHandleContent = PR_TRUE;
    }
  }

  return NS_OK;
}
