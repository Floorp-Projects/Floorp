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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "prsystem.h"

#include "nsIMessenger.h"
#include "nsMessenger.h"

/* rhp - for access to webshell */
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocumentLoaderObserver.h"

#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsIPop3IncomingServer.h"
#include "nsIMsgMessageService.h"
#include "nsFileSpec.h"

#include "nsIMessage.h"
#include "nsIMsgFolder.h"
#include "nsIPop3Service.h"

#include "nsIDOMXULElement.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsIIOService.h"
#include "nsAppShellCIDs.h"
#include "nsMsgRDFUtils.h"

#include "nsICopyMsgStreamListener.h"
#include "nsICopyMessageListener.h"

#include "nsMsgUtils.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"

#include "nsIComponentManager.h"
#include "nsTransactionManagerCID.h"
#include "nsITransactionManager.h"

#include "nsIMsgSendLater.h" 
#include "nsMsgCompCID.h"
#include "nsIMsgSendLaterListener.h"
#include "nsIMsgDraft.h"

#include "nsMsgStatusFeedback.h"

#include "nsIContentViewer.h" 
#include "nsIPref.h"
#include "nsLayoutCID.h"
#include "nsIPresContext.h"
#include "nsIStringStream.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);
static NS_DEFINE_CID(kRDFServiceCID,	NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID); 
static NS_DEFINE_IID(kAppShellServiceCID,        NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kTransactionManagerCID, NS_TRANSACTIONMANAGER_CID);
static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kMsgSendLaterCID, NS_MSGSENDLATER_CID); 
static NS_DEFINE_CID(kCopyMessageStreamListenerCID, NS_COPYMESSAGESTREAMLISTENER_CID); 
static NS_DEFINE_CID(kMsgDraftCID, NS_MSGDRAFT_CID);
static NS_DEFINE_CID(kPrintPreviewContextCID, NS_PRINT_PREVIEW_CONTEXT_CID);
static NS_DEFINE_IID(kIPresContextIID, NS_IPRESCONTEXT_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

#if defined(DEBUG_seth_) || defined(DEBUG_sspitzer_)
#define DEBUG_MESSENGER
#endif

class nsMessenger : public nsIMessenger
{
  
public:
  nsMessenger();
  virtual ~nsMessenger();

	NS_DECL_ISUPPORTS
  
	NS_DECL_NSIMESSENGER

protected:
	nsresult DoDelete(nsIRDFCompositeDataSource* db, nsISupportsArray *srcArray, nsISupportsArray *deletedArray);
	nsresult DoCommand(nsIRDFCompositeDataSource *db, char * command, nsISupportsArray *srcArray, 
					   nsISupportsArray *arguments);
	nsresult DoMarkMessagesRead(nsIRDFCompositeDataSource *database, nsISupportsArray *resourceArray, PRBool markRead);
	nsresult DoMarkMessagesFlagged(nsIRDFCompositeDataSource *database, nsISupportsArray *resourceArray, PRBool markFlagged);
private:
  
  nsString mId;
  void *mScriptObject;
  nsCOMPtr<nsITransactionManager> mTxnMgr;

  /* rhp - need this to drive message display */
  nsIDOMWindow       *mWindow;
  nsIWebShell        *mWebShell;

  nsCOMPtr <nsIDocumentLoaderObserver> m_docLoaderObserver;
  // mscott: temporary variable used to support running urls through the 'Demo' menu....
  nsFileSpec m_folderPath; 
  void InitializeFolderRoot();
};

static nsresult ConvertDOMListToResourceArray(nsIDOMNodeList *nodeList, nsISupportsArray **resourceArray)
{
	nsresult rv = NS_OK;
	PRUint32 listLength;
	nsIDOMNode *node;
	nsIDOMXULElement *xulElement;
	nsIRDFResource *resource;

	if(!resourceArray)
		return NS_ERROR_NULL_POINTER;

	if(NS_FAILED(rv = nodeList->GetLength(&listLength)))
		return rv;

	if(NS_FAILED(NS_NewISupportsArray(resourceArray)))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	for(PRUint32 i = 0; i < listLength; i++)
	{
		if(NS_FAILED(nodeList->Item(i, &node)))
			return rv;

		if(NS_SUCCEEDED(rv = node->QueryInterface(nsCOMTypeInfo<nsIDOMXULElement>::GetIID(), (void**)&xulElement)))
		{
			if(NS_SUCCEEDED(rv = xulElement->GetResource(&resource)) && resource)
			{
				(*resourceArray)->AppendElement(resource);
				NS_RELEASE(resource);
			}
			NS_RELEASE(xulElement);
		}
		NS_RELEASE(node);
		
	}

	return rv;
}


//
// nsMessenger
//
nsMessenger::nsMessenger() : m_folderPath("")
{
	NS_INIT_REFCNT();
	mScriptObject = nsnull;
	mWebShell = nsnull; 
	mWindow = nsnull;

	InitializeFolderRoot();
}

nsMessenger::~nsMessenger()
{
    NS_IF_RELEASE(mWindow);
    NS_IF_RELEASE(mWebShell);
}

//
// nsISupports
//
NS_IMPL_ISUPPORTS(nsMessenger, nsCOMTypeInfo<nsIMessenger>::GetIID())

//
// nsIMsgAppCore
//
NS_IMETHODIMP    
nsMessenger::Open3PaneWindow()
{
	const char *  urlstr=nsnull;
	nsresult rv = NS_OK;
	
	nsCOMPtr<nsIWebShellWindow> newWindow;

	urlstr = "resource:/res/samples/messenger.html";
	NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  
	nsCOMPtr<nsIURI> url;
	NS_WITH_SERVICE(nsIIOService, pNetService, kIOServiceCID, &rv);

	if (NS_SUCCEEDED(rv) && pNetService) 
		rv = pNetService->NewURI(urlstr, nsnull, getter_AddRefs(url));


	if (NS_SUCCEEDED(rv))
		rv = appShell->CreateTopLevelWindow(nsnull,      // parent
                                   url,
                                   PR_TRUE,
                                   PR_TRUE,
                                   NS_CHROME_ALL_CHROME,
                                   nsnull,      // callbacks
                                   NS_SIZETOCONTENT,           // width
                                   NS_SIZETOCONTENT,           // height
                                   getter_AddRefs(newWindow)); // result widget
	return rv;
}

nsresult
nsMessenger::GetNewMessages(nsIRDFCompositeDataSource *db, nsIDOMXULElement *folderElement)
{
	nsresult rv;
	nsCOMPtr<nsIRDFResource> folderResource;
	nsCOMPtr<nsISupportsArray> folderArray;

	if(!folderElement || !db)
		return NS_ERROR_NULL_POINTER;

	rv = folderElement->GetResource(getter_AddRefs(folderResource));
	if(NS_FAILED(rv))
		return rv;

	if(NS_FAILED(NS_NewISupportsArray(getter_AddRefs(folderArray))))
		return NS_ERROR_OUT_OF_MEMORY;

	folderArray->AppendElement(folderResource);

	DoCommand(db, NC_RDF_GETNEWMESSAGES, folderArray, nsnull);

	return rv;
}

extern "C"
nsresult
NS_NewMessenger(const nsIID &aIID, void **aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;

  nsMessenger *appcore = new nsMessenger();
  if (appcore)
    return appcore->QueryInterface(aIID, (void **)aResult);
  else
	return NS_ERROR_NOT_INITIALIZED;
}


NS_IMETHODIMP    
nsMessenger::SetWindow(nsIDOMWindow *aWin, nsIMsgStatusFeedback *aStatusFeedback)
{
	if(!aWin)
		return NS_ERROR_NULL_POINTER;

  nsAutoString  webShellName("messagepane");
  NS_IF_RELEASE(mWindow);
  mWindow = aWin;
  NS_ADDREF(aWin);

#ifdef DEBUG
  /* rhp - Needed to access the webshell to drive message display */
  printf("nsMessenger::SetWindow(): Getting the webShell of interest...\n");
#endif

  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  if (!globalObj) 
  {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell *webShell = nsnull;
  nsIWebShell *rootWebShell = nsnull;

  globalObj->GetWebShell(&webShell);
  if (nsnull == webShell) 
  {
    return NS_ERROR_FAILURE;
  }

  webShell->GetRootWebShell(rootWebShell);
  if (nsnull != rootWebShell) 
  {
    nsresult rv = rootWebShell->FindChildWithName(webShellName.GetUnicode(), mWebShell);
#ifdef NS_DEBUG
    if (NS_SUCCEEDED(rv) && nsnull != mWebShell)
        printf("nsMessenger::SetWindow(): Got the webShell %s.\n", (const char *) nsAutoCString(webShellName));
    else
        printf("nsMessenger::SetWindow(): Failed to find webshell %s.\n", (const char *) nsAutoCString(webShellName));
#endif
	if (mWebShell)
	{
		if (aStatusFeedback)
		{
			m_docLoaderObserver = do_QueryInterface(aStatusFeedback);
			aStatusFeedback->SetWebShell(mWebShell, mWindow);
			mWebShell->SetDocLoaderObserver(m_docLoaderObserver);
            NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
            if(NS_SUCCEEDED(rv))
	            mailSession->SetTemporaryMsgStatusFeedback(aStatusFeedback);
		}
	}
    NS_RELEASE(rootWebShell);
  }

  NS_RELEASE(webShell);

  // libmime always converts to UTF-8 (both HTML and XML)
  if (nsnull != mWebShell) 
  {
	  nsAutoString aForceCharacterSet("UTF-8");
	  mWebShell->SetForceCharacterSet(aForceCharacterSet.GetUnicode());
  }

  return NS_OK;
}


// this should really go through all the pop servers and initialize all
// folder roots
void nsMessenger::InitializeFolderRoot()
{
    nsresult rv;
    
	// get the current identity from the mail session....
    NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return;

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = mailSession->GetCurrentServer(getter_AddRefs(server));
    
    char * folderRoot=nsnull;
    if (NS_SUCCEEDED(rv))
        rv = server->GetLocalPath(&folderRoot);
    
    if (NS_SUCCEEDED(rv)) {
        // everyone should have a inbox so let's
        // tack that folder name on to the root path...
        m_folderPath = folderRoot;
        m_folderPath += "Inbox";
    } // if we have a folder root for the current server
    if (folderRoot) PL_strfree(folderRoot);
    
    // create Undo/Redo Transaction Manager
    NS_WITH_SERVICE (nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      rv = compMgr->CreateInstance(kTransactionManagerCID, nsnull, 
                                   nsCOMTypeInfo<nsITransactionManager>::GetIID(),
                                   getter_AddRefs(mTxnMgr));
      if (NS_SUCCEEDED(rv))
        mTxnMgr->SetMaxTransactionCount(-1);
    }
}

NS_IMETHODIMP
nsMessenger::OpenURL(const char * url)
{
	if (url)
	{
#ifdef DEBUG_MESSENGER
		printf("nsMessenger::OpenURL(%s)\n",url);
#endif    
		nsIMsgMessageService * messageService = nsnull;
		nsresult rv = GetMessageServiceFromURI(url, &messageService);

		if (NS_SUCCEEDED(rv) && messageService)
		{
			messageService->DisplayMessage(url, mWebShell, nsnull, nsnull);
			ReleaseMessageServiceFromURI(url, messageService);
		}
		//If it's not something we know about, then just load the url.
		else
		{
			nsString urlStr(url);
			mWebShell->LoadURL(urlStr.GetUnicode());
		}
	}
	return NS_OK;
}

nsresult
nsMessenger::DoCommand(nsIRDFCompositeDataSource* db, char *command,
                       nsISupportsArray *srcArray, 
                       nsISupportsArray *argumentArray)
{

	nsresult rv;

    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> commandResource;
	rv = rdfService->GetResource(command, getter_AddRefs(commandResource));
	if(NS_SUCCEEDED(rv))
	{
        // ** jt - temporary solution for pickybacking the undo manager into
        // the nsISupportArray
        if (mTxnMgr)
            srcArray->InsertElementAt(mTxnMgr, 0);
		rv = db->DoCommand(srcArray, commandResource, argumentArray);
	}

	return rv;

}

NS_IMETHODIMP
nsMessenger::DeleteMessages(nsIDOMXULElement *tree, nsIDOMXULElement *srcFolderElement, nsIDOMNodeList *nodeList)
{
	nsresult rv;

	if(!tree || !srcFolderElement || !nodeList)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIRDFCompositeDataSource> database;
	nsCOMPtr<nsISupportsArray> resourceArray, folderArray;
	nsCOMPtr<nsIRDFResource> resource;

	rv = srcFolderElement->GetResource(getter_AddRefs(resource));

	if(NS_FAILED(rv))
		return rv;

	rv = tree->GetDatabase(getter_AddRefs(database));
	if(NS_FAILED(rv))
		return rv;

	rv =ConvertDOMListToResourceArray(nodeList, getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return rv;

	rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	folderArray->AppendElement(resource);
	
	rv = DoCommand(database, NC_RDF_DELETE, folderArray, resourceArray);

	return rv;
}

NS_IMETHODIMP nsMessenger::DeleteFolders(nsIRDFCompositeDataSource *db, nsIDOMXULElement *parentFolderElement,
							nsIDOMXULElement *folderElement)
{
	nsresult rv;

	if(!db || !parentFolderElement || !folderElement)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISupportsArray> parentArray, deletedArray;
	nsCOMPtr<nsIRDFResource> parentResource, deletedFolderResource;

	rv = parentFolderElement->GetResource(getter_AddRefs(parentResource));

	if(NS_FAILED(rv))
		return rv;

	rv = folderElement->GetResource(getter_AddRefs(deletedFolderResource));

	if(NS_FAILED(rv))
		return rv;

	rv = NS_NewISupportsArray(getter_AddRefs(parentArray));

	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	rv = NS_NewISupportsArray(getter_AddRefs(deletedArray));

	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	parentArray->AppendElement(parentResource);
	deletedArray->AppendElement(deletedFolderResource);

	rv = DoCommand(db, NC_RDF_DELETE, parentArray, deletedArray);

	return NS_OK;
}

NS_IMETHODIMP
nsMessenger::CopyMessages(nsIRDFCompositeDataSource *database, nsIDOMXULElement *srcFolderElement,
						  nsIDOMXULElement *dstFolderElement, nsIDOMNodeList *messages, PRBool isMove)
{
#if 1
	nsresult rv;

	if(!srcFolderElement || !dstFolderElement || !messages)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIRDFResource> srcResource, dstResource;
	nsCOMPtr<nsIMsgFolder> srcFolder;
	nsCOMPtr<nsISupportsArray> argumentArray;
	nsCOMPtr<nsISupportsArray> folderArray;

	rv = dstFolderElement->GetResource(getter_AddRefs(dstResource));
	if(NS_FAILED(rv))
		return rv;

	rv = srcFolderElement->GetResource(getter_AddRefs(srcResource));
	if(NS_FAILED(rv))
		return rv;

	srcFolder = do_QueryInterface(srcResource);
	if(!srcFolder)
		return NS_ERROR_NO_INTERFACE;

	rv =ConvertDOMListToResourceArray(messages, getter_AddRefs(argumentArray));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsISupports> srcFolderSupports(do_QueryInterface(srcFolder));
	if(srcFolderSupports)
		argumentArray->InsertElementAt(srcFolderSupports, 0);

	rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	folderArray->AppendElement(dstResource);
	
	rv = DoCommand(database, isMove ? (char *)NC_RDF_MOVE : (char *)NC_RDF_COPY, folderArray, argumentArray);
	return rv;

#else
	nsresult rv;

	if(!srcFolderElement || !dstFolderElement || !messages)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIRDFResource> srcResource, dstResource;
	nsCOMPtr<nsICopyMessageListener> dstFolder;
	nsCOMPtr<nsIMsgFolder> srcFolder;
	nsCOMPtr<nsISupportsArray> resourceArray;

	rv = dstFolderElement->GetResource(getter_AddRefs(dstResource));
	if(NS_FAILED(rv))
		return rv;

	dstFolder = do_QueryInterface(dstResource);
	if(!dstFolder)
		return NS_ERROR_NO_INTERFACE;

	rv = srcFolderElement->GetResource(getter_AddRefs(srcResource));
	if(NS_FAILED(rv))
		return rv;

	srcFolder = do_QueryInterface(srcResource);
	if(!srcFolder)
		return NS_ERROR_NO_INTERFACE;

	rv =ConvertDOMListToResourceArray(messages, getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return rv;

	//Call the mailbox service to copy first message.  In the future we should call CopyMessages.
	//And even more in the future we need to distinguish between the different types of URI's, i.e.
	//local, imap, and news, and call the appropriate copy function.

	PRUint32 cnt;
    rv = resourceArray->Count(&cnt);
    if (NS_SUCCEEDED(rv) && cnt > 0)
	{
		nsCOMPtr<nsISupports> msgSupports = getter_AddRefs(resourceArray->ElementAt(0));
		nsCOMPtr<nsIRDFResource> firstMessage(do_QueryInterface(msgSupports));
		char *uri;
		firstMessage->GetValue(&uri);
		nsCOMPtr<nsICopyMessageStreamListener> copyStreamListener; 
		rv = nsComponentManager::CreateInstance(kCopyMessageStreamListenerCID, NULL,
												nsCOMTypeInfo<nsICopyMessageStreamListener>::GetIID(),
												getter_AddRefs(copyStreamListener)); 
		if(NS_FAILED(rv))
			return rv;

		rv = copyStreamListener->Init(srcFolder, dstFolder, nsnull);
		if(NS_FAILED(rv))
			return rv;
		nsIMsgMessageService * messageService = nsnull;
		rv = GetMessageServiceFromURI(uri, &messageService);

		if (NS_SUCCEEDED(rv) && messageService)
		{
			nsIURI * url = nsnull;
			nsCOMPtr<nsIStreamListener> streamListener(do_QueryInterface(copyStreamListener));
			if(!streamListener)
				return NS_ERROR_NO_INTERFACE;
			messageService->CopyMessage(uri, streamListener, isMove, nsnull, &url);
			ReleaseMessageServiceFromURI(uri, messageService);
		}

	}

	return rv;
#endif
}

NS_IMETHODIMP
nsMessenger::GetRDFResourceForMessage(nsIDOMXULElement *tree,
                                       nsIDOMNodeList *nodeList, nsISupports
                                       **aSupport) 
{
      nsresult rv;
	  if(!tree || !nodeList)
		  return NS_ERROR_NULL_POINTER;

      nsISupportsArray *resourceArray;
    nsIBidirectionalEnumerator *aEnumerator = nsnull;
    *aSupport = nsnull;
    nsISupports *aItem = nsnull;

      if(NS_FAILED(rv =ConvertDOMListToResourceArray(nodeList, &resourceArray)))
              return rv;

    rv = NS_NewISupportsArrayEnumerator(resourceArray, &aEnumerator);
    if (NS_FAILED(rv)) return rv;

    rv = aEnumerator->First();
    while (rv == NS_OK)
    {
        rv = aEnumerator->CurrentItem(&aItem);
        if (rv != NS_OK) break;
        rv = aItem->QueryInterface(nsCOMTypeInfo<nsIMessage>::GetIID(), (void**)aSupport);
        aItem->Release();
        if (rv == NS_OK && *aSupport) break;
        rv = aEnumerator->Next();
    }

    aEnumerator->Release();
      NS_RELEASE(resourceArray);
      return rv;
}

NS_IMETHODIMP
nsMessenger::Exit()
{
	nsresult rv = NS_OK;
  /*
   * Create the Application Shell instance...
   */
  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
    appShell->Shutdown();
  return NS_OK;
}

NS_IMETHODIMP
nsMessenger::OnUnload()
{
    // ** clean up
    // *** jt - We seem to have one extra ref count. I have no idea where it
    // came from. This could be the global object we created in commandglue.js
    // which causes us to have one more ref count. Call Release() here
    // seems the right thing to do. This gurantees the nsMessenger instance
    // gets deleted after we close down the messenger window.
    
    // smfr the one extra refcount is the result of a bug 8555, which I have 
    // checked in a fix for. So I'm commenting out this extra release.
    //Release();
    return NS_OK;
}

NS_IMETHODIMP
nsMessenger::Close()
{
    nsresult rv = NS_OK;
    if (mWindow)
    {
        nsCOMPtr<nsIScriptGlobalObject>
            globalScript(do_QueryInterface(mWindow));
        nsCOMPtr<nsIWebShell> webshell, rootWebshell;
        if (globalScript)
            globalScript->GetWebShell(getter_AddRefs(webshell));
        if (webshell)
            webshell->GetRootWebShell(*getter_AddRefs(rootWebshell));
        if (rootWebshell) 
        {
            nsCOMPtr<nsIWebShellContainer> webshellContainer;
            nsCOMPtr<nsIWebShellWindow> webWindow;
            rootWebshell->GetContainer(*getter_AddRefs(webshellContainer));
            webWindow = do_QueryInterface(webshellContainer);
            if (webWindow) 
			{
				webWindow->Show(PR_FALSE);
                webWindow->Close();
			}
         }
    }

    return rv;
}


NS_IMETHODIMP
nsMessenger::MarkMessageRead(nsIRDFCompositeDataSource *database, nsIDOMXULElement *message, PRBool markRead)
{
	if(!database || !message)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	nsCOMPtr<nsIRDFResource> messageResource;
	rv = message->GetResource(getter_AddRefs(messageResource));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsISupportsArray> resourceArray;

	rv = NS_NewISupportsArray(getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;

	resourceArray->AppendElement(messageResource);

	rv = DoMarkMessagesRead(database, resourceArray, markRead);

	return rv;
}

NS_IMETHODIMP
nsMessenger::MarkMessagesRead(nsIRDFCompositeDataSource *database, nsIDOMNodeList *messages, PRBool markRead)
{
	nsresult rv;

	if(!database || !messages)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISupportsArray> resourceArray;


	rv =ConvertDOMListToResourceArray(messages, getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return rv;

	rv= DoMarkMessagesRead(database, resourceArray, markRead);


	return rv;
}

nsresult
nsMessenger::DoMarkMessagesRead(nsIRDFCompositeDataSource *database, nsISupportsArray *resourceArray, PRBool markRead)
{
	nsresult rv;
	nsCOMPtr<nsISupportsArray> argumentArray;

	rv = NS_NewISupportsArray(getter_AddRefs(argumentArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	if(markRead)
		rv = DoCommand(database, NC_RDF_MARKREAD, resourceArray, argumentArray);
	else
		rv = DoCommand(database, NC_RDF_MARKUNREAD, resourceArray,  argumentArray);

	return rv;

}

NS_IMETHODIMP
nsMessenger::MarkAllMessagesRead(nsIRDFCompositeDataSource *database, nsIDOMXULElement *folder)
{
	nsresult rv;
	nsCOMPtr<nsIRDFResource> folderResource;
	nsCOMPtr<nsISupportsArray> folderArray;

	if(!folder || !database)
		return NS_ERROR_NULL_POINTER;

	rv = folder->GetResource(getter_AddRefs(folderResource));
	if(NS_FAILED(rv))
		return rv;

	if(NS_FAILED(NS_NewISupportsArray(getter_AddRefs(folderArray))))
		return NS_ERROR_OUT_OF_MEMORY;

	folderArray->AppendElement(folderResource);

	DoCommand(database, NC_RDF_MARKALLMESSAGESREAD, folderArray, nsnull);

	return rv;
}

NS_IMETHODIMP
nsMessenger::MarkMessageFlagged(nsIRDFCompositeDataSource *database, nsIDOMXULElement *message, PRBool markFlagged)
{
	if(!database || !message)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	nsCOMPtr<nsIRDFResource> messageResource;
	rv = message->GetResource(getter_AddRefs(messageResource));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsISupportsArray> resourceArray;

	rv = NS_NewISupportsArray(getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;

	resourceArray->AppendElement(messageResource);

	rv = DoMarkMessagesFlagged(database, resourceArray, markFlagged);

	return rv;
}

NS_IMETHODIMP
nsMessenger::MarkMessagesFlagged(nsIRDFCompositeDataSource *database, nsIDOMNodeList *messages, PRBool markFlagged)
{
	nsresult rv;

	if(!database || !messages)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISupportsArray> resourceArray;


	rv =ConvertDOMListToResourceArray(messages, getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return rv;

	rv= DoMarkMessagesFlagged(database, resourceArray, markFlagged);


	return rv;
}

nsresult
nsMessenger::DoMarkMessagesFlagged(nsIRDFCompositeDataSource *database, nsISupportsArray *resourceArray, PRBool markFlagged)
{
	nsresult rv;
	nsCOMPtr<nsISupportsArray> argumentArray;

	rv = NS_NewISupportsArray(getter_AddRefs(argumentArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	if(markFlagged)
		rv = DoCommand(database, NC_RDF_MARKFLAGGED, resourceArray, argumentArray);
	else
		rv = DoCommand(database, NC_RDF_MARKUNFLAGGED, resourceArray,  argumentArray);

	return rv;

}

NS_IMETHODIMP
nsMessenger::NewFolder(nsIRDFCompositeDataSource *database, nsIDOMXULElement *parentFolderElement,
						const char *name)
{
	nsresult rv;
	nsCOMPtr<nsIRDFResource> folderResource;
	nsCOMPtr<nsISupportsArray> nameArray, folderArray;

	if(!parentFolderElement || !name)
		return NS_ERROR_NULL_POINTER;

	rv = parentFolderElement->GetResource(getter_AddRefs(folderResource));
	if(NS_FAILED(rv))
		return rv;

	rv = NS_NewISupportsArray(getter_AddRefs(nameArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;

	folderArray->AppendElement(folderResource);

    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if(NS_SUCCEEDED(rv))
	{
		nsString nameStr = name;
		nsCOMPtr<nsIRDFLiteral> nameLiteral;

		rdfService->GetLiteral(nameStr.GetUnicode(), getter_AddRefs(nameLiteral));
		nameArray->AppendElement(nameLiteral);
		rv = DoCommand(database, NC_RDF_NEWFOLDER, folderArray, nameArray);
	}
	return rv;
}

NS_IMETHODIMP
nsMessenger::RenameFolder(nsIRDFCompositeDataSource* db,
                          nsIDOMXULElement* folder,
                          const char* name)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  if (!db || !folder || !name || !*name) return rv;
  nsCOMPtr<nsISupports> streamSupport;
  rv = NS_NewCharInputStream(getter_AddRefs(streamSupport), name);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISupportsArray> folderArray;
    nsCOMPtr<nsISupportsArray> argsArray;
    nsCOMPtr<nsIRDFResource> folderResource;
    rv = folder->GetResource(getter_AddRefs(folderResource));
    if (NS_SUCCEEDED(rv))
    {
      rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
      if (NS_FAILED(rv)) return rv;
      folderArray->AppendElement(folderResource);
      rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
      if (NS_FAILED(rv)) return rv;
      argsArray->AppendElement(streamSupport);
      rv = DoCommand(db, NC_RDF_RENAME, folderArray, argsArray);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::CompactFolder(nsIRDFCompositeDataSource* db,
                           nsIDOMXULElement* folder)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  
  if (!db || !folder) return rv;
  nsCOMPtr<nsISupportsArray> folderArray;
  nsCOMPtr<nsIRDFResource> folderResource;

  rv = folder->GetResource(getter_AddRefs(folderResource));
  if (NS_SUCCEEDED(rv))
  {
    rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
    if (NS_FAILED(rv)) return rv;
    folderArray->AppendElement(folderResource);
    rv = DoCommand(db, NC_RDF_COMPACT, folderArray, nsnull);
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::EmptyTrash(nsIRDFCompositeDataSource* db,
                        nsIDOMXULElement* folder)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  
  if (!db || !folder) return rv;
  nsCOMPtr<nsISupportsArray> folderArray;
  nsCOMPtr<nsIRDFResource> folderResource;

  rv = folder->GetResource(getter_AddRefs(folderResource));
  if (NS_SUCCEEDED(rv))
  {
    rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
    if (NS_FAILED(rv)) return rv;
    folderArray->AppendElement(folderResource);
    rv = DoCommand(db, NC_RDF_EMPTYTRASH, folderArray, nsnull);
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::Undo()
{
  nsresult rv = NS_OK;
  if (mTxnMgr)
  {
    PRInt32 numTxn = 0;
    rv = mTxnMgr->GetNumberOfUndoItems(&numTxn);
    if (NS_SUCCEEDED(rv) && numTxn > 0)
      mTxnMgr->Undo();
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::Redo()
{
  nsresult rv = NS_OK;
  if (mTxnMgr)
  {
    PRInt32 numTxn = 0;
    rv = mTxnMgr->GetNumberOfRedoItems(&numTxn);
    if (NS_SUCCEEDED(rv) && numTxn > 0)
      mTxnMgr->Redo();
  }
  return rv;
}

NS_IMETHODIMP
nsMessenger::GetTransactionManager(nsITransactionManager* *aTxnMgr)
{
  if (!mTxnMgr || !aTxnMgr)
    return NS_ERROR_NULL_POINTER;

  *aTxnMgr = mTxnMgr;
  NS_ADDREF(*aTxnMgr);

  return NS_OK;
}

NS_IMETHODIMP nsMessenger::SetDocumentCharset(const PRUnichar *characterSet)
{
	// Set a default charset of the webshell. 
	if (nsnull != mWebShell) {
		mWebShell->SetDefaultCharacterSet(characterSet);
	}
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// This is the listener class for the send operation. 
////////////////////////////////////////////////////////////////////////////////////
class SendLaterListener: public nsIMsgSendLaterListener
{
public:
  SendLaterListener(void);
  virtual ~SendLaterListener(void);

  // nsISupports interface
  NS_DECL_ISUPPORTS

  /* void OnStartSending (in PRUint32 aTotalMessageCount); */
  NS_IMETHOD OnStartSending(PRUint32 aTotalMessageCount);

  /* void OnProgress (in PRUint32 aCurrentMessage, in PRUint32 aTotalMessage); */
  NS_IMETHOD OnProgress(PRUint32 aCurrentMessage, PRUint32 aTotalMessage);

  /* void OnStatus (in wstring aMsg); */
  NS_IMETHOD OnStatus(const PRUnichar *aMsg);

  /* void OnStopSending (in nsresult aStatus, in wstring aMsg, in PRUint32 aTotalTried, in PRUint32 aSuccessful); */
  NS_IMETHOD OnStopSending(nsresult aStatus, const PRUnichar *aMsg, PRUint32 aTotalTried, PRUint32 aSuccessful);
};

NS_IMPL_ISUPPORTS(SendLaterListener, nsCOMTypeInfo<nsIMsgSendLaterListener>::GetIID());

SendLaterListener::SendLaterListener()
{
  NS_INIT_REFCNT();
}

SendLaterListener::~SendLaterListener()
{
}

nsresult
SendLaterListener::OnStartSending(PRUint32 aTotalMessageCount)
{
  return NS_OK;
}

nsresult
SendLaterListener::OnProgress(PRUint32 aCurrentMessage, PRUint32 aTotalMessage)
{
  return NS_OK;
}

nsresult
SendLaterListener::OnStatus(const PRUnichar *aMsg)
{
  return NS_OK;
}

nsresult
SendLaterListener::OnStopSending(nsresult aStatus, const PRUnichar *aMsg, PRUint32 aTotalTried, 
                                 PRUint32 aSuccessful) 
{
#ifdef NS_DEBUG
  if (NS_SUCCEEDED(aStatus))
    printf("SendLaterListener::OnStopSending: Tried to send %d messages. %d successful.\n",
            aTotalTried, aSuccessful);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsMessenger::SendUnsentMessages()
{
	nsresult rv;
	nsCOMPtr<nsIMsgSendLater> pMsgSendLater; 
	rv = nsComponentManager::CreateInstance(kMsgSendLaterCID, NULL,nsCOMTypeInfo<nsIMsgSendLater>::GetIID(),
																					(void **)getter_AddRefs(pMsgSendLater)); 
	if (NS_SUCCEEDED(rv) && pMsgSendLater) 
	{ 
#ifdef DEBUG
		printf("We succesfully obtained a nsIMsgSendLater interface....\n"); 
#endif

    SendLaterListener *sendLaterListener = new SendLaterListener();
    if (!sendLaterListener)
        return NS_ERROR_FAILURE;

    NS_ADDREF(sendLaterListener);
    pMsgSendLater->AddListener(sendLaterListener);

    // temporary hack to get the current identity
    NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIMsgIdentity> identity;
    rv = mailSession->GetCurrentIdentity(getter_AddRefs(identity));
    if (NS_FAILED(rv)) return rv;
      
    pMsgSendLater->SendUnsentMessages(identity, nsnull); 
    NS_RELEASE(sendLaterListener);
	} 
	return NS_OK;
}

NS_IMETHODIMP
nsMessenger::LoadFirstDraft()
{
	nsresult              rv = NS_ERROR_FAILURE;
	nsCOMPtr<nsIMsgDraft> pMsgDraft; 

	rv = nsComponentManager::CreateInstance(kMsgDraftCID, NULL, nsCOMTypeInfo<nsIMsgDraft>::GetIID(),
																					(void **)getter_AddRefs(pMsgDraft)); 
	if (NS_SUCCEEDED(rv) && pMsgDraft) 
	{ 
#ifdef DEBUG
		printf("We succesfully obtained a nsIMsgDraft interface....\n");
#endif


    // This should really pass in a URI, but for now, just to test, we can pass in nsnull
    rv = pMsgDraft->OpenDraftMsg(nsnull, nsnull); 
  } 

  return rv;
}

NS_IMETHODIMP nsMessenger::DoPrint()
{
#ifdef DEBUG_MESSENGER
  printf("nsMessenger::DoPrint()\n");
#endif

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIContentViewer> viewer;

  NS_ASSERTION(mWebShell,"can't print, there is no webshell");
  if (!mWebShell) {
	return rv;
  }

  mWebShell->GetContentViewer(getter_AddRefs(viewer));

  if (viewer) {
    rv = viewer->Print();
  }
#ifdef DEBUG_MESSENGER
  else {
	printf("failed to get the viewer for printing\n");
  }
#endif
  
  return rv;
}

NS_IMETHODIMP nsMessenger::DoPrintPreview()
{
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
#ifdef DEBUG_MESSENGER
  printf("nsMessenger::DoPrintPreview() not implemented yet\n");
#endif
  return rv;  
}
