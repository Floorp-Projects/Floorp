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
#include "nsAppShellCIDs.h"
#include "nsMsgRDFUtils.h"

#include "nsINetService.h"
#include "nsICopyMsgStreamListener.h"
#include "nsICopyMessageListener.h"

#include "nsIMessageView.h"

#include "nsMsgUtils.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"

#include "nsIComponentManager.h"
#include "nsTransactionManagerCID.h"
#include "nsITransactionManager.h"

#include "nsIMsgSendLater.h" 
#include "nsMsgCompCID.h"
#include "nsIMsgSendLaterListener.h"

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);
static NS_DEFINE_CID(kRDFServiceCID,	NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID); 
static NS_DEFINE_IID(kAppShellServiceCID,        NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kTransactionManagerCID, NS_TRANSACTIONMANAGER_CID);
static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kMsgSendLaterCID, NS_MSGSENDLATER_CID); 
static NS_DEFINE_CID(kCopyMessageStreamListenerCID, NS_COPYMESSAGESTREAMLISTENER_CID); 

class nsMessenger : public nsIMessenger
{
  
public:
  nsMessenger();
  virtual ~nsMessenger();

  NS_DECL_ISUPPORTS
  
  // nsIMessenger
  NS_IMETHOD GetTransactionManager(nsITransactionManager * *aTxnMgr);
  NS_IMETHOD Open3PaneWindow();
  NS_IMETHOD GetNewMessages(nsIRDFCompositeDataSource *db, nsIDOMXULElement *folderElement);
  NS_IMETHOD SetWindow(nsIDOMWindow* aWin);
  NS_IMETHOD OpenURL(const char * url);
  NS_IMETHOD DeleteMessages(nsIDOMXULElement *tree, nsIDOMXULElement *srcFolderElement, nsIDOMNodeList *nodeList);
  NS_IMETHOD DeleteFolders(nsIRDFCompositeDataSource *db, nsIDOMXULElement *parentFolder, nsIDOMXULElement *folder);

  NS_IMETHOD CopyMessages(nsIRDFCompositeDataSource *database, nsIDOMXULElement *srcFolderElement,
						  nsIDOMXULElement *dstFolderElement, nsIDOMNodeList *messages, PRBool isMove);
  NS_IMETHOD GetRDFResourceForMessage(nsIDOMXULElement *tree,
                                      nsIDOMNodeList *nodeList, nsISupports
                                      **aSupport); 
  NS_IMETHOD Exit();
  NS_IMETHOD Close();
  NS_IMETHOD OnUnload();
  NS_IMETHOD ViewAllMessages(nsIRDFCompositeDataSource *databsae);
  NS_IMETHOD ViewUnreadMessages(nsIRDFCompositeDataSource *databsae);
  NS_IMETHOD ViewAllThreadMessages(nsIRDFCompositeDataSource *database);
  NS_IMETHOD MarkMessagesRead(nsIRDFCompositeDataSource *database, nsIDOMNodeList *messages, PRBool markRead);
  NS_IMETHOD MarkMessageRead(nsIRDFCompositeDataSource *database, nsIDOMXULElement *message, PRBool markRead);
  NS_IMETHOD MarkAllMessagesRead(nsIRDFCompositeDataSource *database, nsIDOMXULElement *folder);

  NS_IMETHOD NewFolder(nsIRDFCompositeDataSource *database, nsIDOMXULElement *parentFolderElement,
						const char *name);
  NS_IMETHOD Undo();
  NS_IMETHOD Redo();
  NS_IMETHOD SendUnsentMessages();
  NS_IMETHOD SetDocumentCharset(const PRUnichar *characterSet);

protected:
	nsresult DoDelete(nsIRDFCompositeDataSource* db, nsISupportsArray *srcArray, nsISupportsArray *deletedArray);
	nsresult DoCommand(nsIRDFCompositeDataSource *db, char * command, nsISupportsArray *srcArray, 
					   nsISupportsArray *arguments);
	nsresult DoMarkMessagesRead(nsIRDFCompositeDataSource *database, nsISupportsArray *resourceArray, PRBool markRead);
private:
  
  nsString mId;
  void *mScriptObject;
  nsCOMPtr<nsITransactionManager> mTxnMgr;

  /* rhp - need this to drive message display */
  nsIDOMWindow       *mWindow;
  nsIWebShell        *mWebShell;

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
			if(NS_SUCCEEDED(rv = xulElement->GetResource(&resource)))
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

static nsresult AddView(nsIRDFCompositeDataSource *database, nsIMessageView **messageView)
{
	if(!messageView || !database)
		return NS_ERROR_NULL_POINTER;

	nsIRDFDataSource *view, *datasource;
	nsresult rv;
	NS_WITH_SERVICE(nsIRDFService, gRDFService, kRDFServiceCID, &rv);

	if(NS_SUCCEEDED(rv))
	{
		rv = gRDFService->GetDataSource("rdf:mail-messageview", &view);
		rv = NS_SUCCEEDED(rv) && gRDFService->GetDataSource("rdf:mailnewsfolders", &datasource);
	}

	if(!NS_SUCCEEDED(rv))
		return rv;

	database->RemoveDataSource(datasource);
	//This is a hack until I have the ability to save off my current view some place.
	//In case it's already been added, remove it.  We'll need to do the same for the
	//thread view.
	database->RemoveDataSource(view);
	database->AddDataSource(view); 

			//add the datasource
		//return the view as an nsIMessageView
	nsIRDFCompositeDataSource *viewCompositeDataSource;
	if(NS_SUCCEEDED(view->QueryInterface(nsCOMTypeInfo<nsIRDFCompositeDataSource>::GetIID(), (void**)&viewCompositeDataSource)))
	{
		viewCompositeDataSource->AddDataSource(datasource);
		NS_IF_RELEASE(viewCompositeDataSource);
	}
	rv = view->QueryInterface(nsCOMTypeInfo<nsIMessageView>::GetIID(), (void**)messageView);

	NS_IF_RELEASE(view);
	NS_IF_RELEASE(datasource);

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
	char *  urlstr=nsnull;
	nsresult rv = NS_OK;
	
	nsCOMPtr<nsIWebShellWindow> newWindow;

	urlstr = "resource:/res/samples/messenger.html";
	NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  
	nsIURI* url = nsnull;
	NS_WITH_SERVICE(nsINetService, pNetService, kNetServiceCID, &rv);
	if (NS_SUCCEEDED(rv) && pNetService) {
		rv = pNetService->CreateURL(&url, urlstr);
		if (NS_FAILED(rv))
			goto done;		// goto in C++ is evil!
	}
	else
		goto done;		// goto in C++ is evil!

	appShell->CreateTopLevelWindow(nsnull,      // parent
                                   url,
                                   PR_TRUE,
                                   NS_CHROME_ALL_CHROME,
                                   nsnull,      // callbacks
                                   NS_SIZETOCONTENT,           // width
                                   NS_SIZETOCONTENT,           // height
                                   getter_AddRefs(newWindow)); // result widget
	done:
	NS_IF_RELEASE(url);
	return NS_OK;
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
nsMessenger::SetWindow(nsIDOMWindow* aWin)
{
	if(!aWin)
		return NS_ERROR_NULL_POINTER;

  nsAutoString  webShellName("messagepane");
  NS_IF_RELEASE(mWindow);
  mWindow = aWin;
  NS_ADDREF(aWin);

  /* rhp - Needed to access the webshell to drive message display */
  printf("nsMessenger::SetWindow(): Getting the webShell of interest...\n");

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
#if defined(DEBUG_sspitzer) || defined(DEBUG_seth)
		printf("nsMessenger::OpenURL(%s)\n",url);
#endif    
		nsIMsgMessageService * messageService = nsnull;
		nsresult rv = GetMessageServiceFromURI(url, &messageService);

		if (NS_SUCCEEDED(rv) && messageService)
		{
			messageService->DisplayMessage(url, mWebShell, nsnull, nsnull);
			ReleaseMessageServiceFromURI(url, messageService);
		}

	}
	return NS_OK;
}

nsresult
nsMessenger::DoCommand(nsIRDFCompositeDataSource* db, char *command,
						nsISupportsArray *srcArray, nsISupportsArray *argumentArray)
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
	
	rv = DoCommand(database, isMove ? NC_RDF_MOVE : NC_RDF_COPY, folderArray, argumentArray);
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
nsMessenger::ViewAllMessages(nsIRDFCompositeDataSource *database)
{
	nsIMessageView *messageView;
	if(NS_SUCCEEDED(AddView(database, &messageView)))
	{
		messageView->SetShowAll();
		messageView->SetShowThreads(PR_FALSE);
		NS_IF_RELEASE(messageView);
	}

	return NS_OK;

}

NS_IMETHODIMP
nsMessenger::ViewUnreadMessages(nsIRDFCompositeDataSource *database)
{
	nsIMessageView *messageView;
	if(NS_SUCCEEDED(AddView(database, &messageView)))
	{
		messageView->SetShowUnread();
		messageView->SetShowThreads(PR_FALSE);
		NS_IF_RELEASE(messageView);
	}

	return NS_OK;

}

NS_IMETHODIMP
nsMessenger::ViewAllThreadMessages(nsIRDFCompositeDataSource *database)
{
	nsIMessageView *messageView;
	if(NS_SUCCEEDED(AddView(database, &messageView)))
	{
		messageView->SetShowAll();
		messageView->SetShowThreads(PR_TRUE);
		NS_IF_RELEASE(messageView);
	}

	return NS_OK;
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
		printf("We succesfully obtained a nsIMsgSendLater interface....\n"); 

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
