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

#include "nsIDOMXULTreeElement.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsMsgRDFUtils.h"

#include "nsINetService.h"
#include "nsCopyMessageStreamListener.h"
#include "nsICopyMessageListener.h"

#include "nsIMessageView.h"

#include "nsMsgUtils.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);
static NS_DEFINE_CID(kRDFServiceCID,	NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID); 
static NS_DEFINE_IID(kAppShellServiceCID,        NS_APPSHELL_SERVICE_CID);

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

class nsMessenger : public nsIMessenger
{
  
public:
  nsMessenger();
  virtual ~nsMessenger();

  NS_DECL_ISUPPORTS
  
  // nsIMessenger
  NS_IMETHOD Open3PaneWindow();
  NS_IMETHOD GetNewMessages(nsIRDFCompositeDataSource *db, nsIDOMXULElement *folderElement);
  NS_IMETHOD SetWindow(nsIDOMWindow* aWin);
  NS_IMETHOD OpenURL(const char * url);
  NS_IMETHOD DeleteMessages(nsIDOMXULTreeElement *tree, nsIDOMXULElement *srcFolderElement, nsIDOMNodeList *nodeList);
  NS_IMETHOD DeleteFolders(nsIRDFCompositeDataSource *db, nsIDOMXULElement *parentFolder, nsIDOMXULElement *folder);

  NS_IMETHOD CopyMessages(nsIDOMXULElement *srcFolderElement, nsIDOMXULElement *folderElement, nsIDOMNodeList *nodeList,
						  PRBool isMove);
  NS_IMETHOD GetRDFResourceForMessage(nsIDOMXULTreeElement *tree,
                                      nsIDOMNodeList *nodeList, nsISupports
                                      **aSupport); 
  NS_IMETHOD Exit();
  NS_IMETHOD Close();
  NS_IMETHOD OnUnload();
  NS_IMETHOD ViewAllMessages(nsIRDFCompositeDataSource *databsae);
  NS_IMETHOD ViewUnreadMessages(nsIRDFCompositeDataSource *databsae);
  NS_IMETHOD ViewAllThreadMessages(nsIRDFCompositeDataSource *database);
  NS_IMETHOD MarkMessagesRead(nsIRDFCompositeDataSource *database, nsIDOMNodeList *messages, PRBool markRead);

  NS_IMETHOD NewFolder(nsIRDFCompositeDataSource *database, nsIDOMXULElement *parentFolderElement,
						const char *name);
  NS_IMETHOD AccountManager(nsIDOMWindow *parent);

protected:
	nsresult DoDelete(nsIRDFCompositeDataSource* db, nsISupportsArray *srcArray, nsISupportsArray *deletedArray);
	nsresult DoCommand(nsIRDFCompositeDataSource *db, char * command, nsISupportsArray *srcArray, 
					   nsISupportsArray *arguments);
private:
  
  nsString mId;
  void *mScriptObject;

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
	nsIDOMXULTreeElement *xulElement;
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

		if(NS_SUCCEEDED(rv = node->QueryInterface(nsIDOMXULElement::GetIID(), (void**)&xulElement)))
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
	if(NS_SUCCEEDED(view->QueryInterface(nsIRDFCompositeDataSource::GetIID(), (void**)&viewCompositeDataSource)))
	{
		viewCompositeDataSource->AddDataSource(datasource);
		NS_IF_RELEASE(viewCompositeDataSource);
	}
	rv = view->QueryInterface(nsIMessageView::GetIID(), (void**)messageView);

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
NS_IMPL_ISUPPORTS(nsMessenger, nsIMessenger::GetIID())

//
// nsIMsgAppCore
//
NS_IMETHODIMP    
nsMessenger::Open3PaneWindow()
{
	char *  urlstr=nsnull;
	nsresult rv = NS_OK;
	
	urlstr = "resource:/res/samples/messenger.html";
	NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  
	nsIURL* url = nsnull;
	NS_WITH_SERVICE(nsINetService, pNetService, kNetServiceCID, &rv);
	if (NS_SUCCEEDED(rv) && pNetService) {
		rv = pNetService->CreateURL(&url, urlstr);
		if (NS_FAILED(rv))
			goto done;
	}
	else
		goto done;


	nsIWebShellWindow* newWindow;
	appShell->CreateTopLevelWindow(nsnull,      // parent
                                   url,
                                   PR_TRUE,
                                   newWindow,   // result widget
                                   nsnull,      // observer
                                   nsnull,      // callbacks
                                   200,         // width
                                   200);        // height
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
    
    char * folderRoot;
    if (NS_SUCCEEDED(rv))
        rv = server->GetLocalPath(&folderRoot);
    
    if (NS_SUCCEEDED(rv)) {
        // everyone should have a inbox so let's
        // tack that folder name on to the root path...
        m_folderPath = folderRoot;
        m_folderPath += "Inbox";
    } // if we have a folder root for the current server
    
}

NS_IMETHODIMP
nsMessenger::OpenURL(const char * url)
{
	if (url)
	{
#ifdef DEBUG_sspitzer
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
		rv = db->DoCommand(srcArray, commandResource, argumentArray);
	}

	return rv;

}

NS_IMETHODIMP
nsMessenger::DeleteMessages(nsIDOMXULTreeElement *tree, nsIDOMXULElement *srcFolderElement, nsIDOMNodeList *nodeList)
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
nsMessenger::CopyMessages(nsIDOMXULElement *srcFolderElement, nsIDOMXULElement *dstFolderElement,
						   nsIDOMNodeList *nodeList, PRBool isMove)
{
	nsresult rv;

	if(!srcFolderElement || !dstFolderElement || !nodeList)
		return NS_ERROR_NULL_POINTER;

	nsIRDFResource *srcResource, *dstResource;
	nsICopyMessageListener *dstFolder;
	nsIMsgFolder *srcFolder;
	nsISupportsArray *resourceArray;

	if(NS_FAILED(rv = dstFolderElement->GetResource(&dstResource)))
		return rv;

	if(NS_FAILED(rv = dstResource->QueryInterface(nsICopyMessageListener::GetIID(), (void**)&dstFolder)))
		return rv;

	if(NS_FAILED(rv = srcFolderElement->GetResource(&srcResource)))
		return rv;

	if(NS_FAILED(rv = srcResource->QueryInterface(nsIMsgFolder::GetIID(), (void**)&srcFolder)))
		return rv;

	if(NS_FAILED(rv =ConvertDOMListToResourceArray(nodeList, &resourceArray)))
		return rv;

	//Call the mailbox service to copy first message.  In the future we should call CopyMessages.
	//And even more in the future we need to distinguish between the different types of URI's, i.e.
	//local, imap, and news, and call the appropriate copy function.

	PRUint32 cnt;
    rv = resourceArray->Count(&cnt);
    if (NS_SUCCEEDED(rv) && cnt > 0)
	{
		nsIRDFResource * firstMessage = (nsIRDFResource*)resourceArray->ElementAt(0);
		char *uri;
		firstMessage->GetValue(&uri);
		nsCopyMessageStreamListener* copyStreamListener = new nsCopyMessageStreamListener(srcFolder, dstFolder, nsnull);

		nsIMsgMessageService * messageService = nsnull;
		rv = GetMessageServiceFromURI(uri, &messageService);

		if (NS_SUCCEEDED(rv) && messageService)
		{
			nsIURL * url = nsnull;
			messageService->CopyMessage(uri, copyStreamListener, isMove, nsnull, &url);
			ReleaseMessageServiceFromURI(uri, messageService);
		}

	}

	NS_RELEASE(srcResource);
	NS_RELEASE(srcFolder);
	NS_RELEASE(dstResource);
	NS_RELEASE(dstFolder);
	NS_RELEASE(resourceArray);
	return rv;
}

NS_IMETHODIMP
nsMessenger::GetRDFResourceForMessage(nsIDOMXULTreeElement *tree,
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
        rv = aItem->QueryInterface(nsIMessage::GetIID(), (void**)aSupport);
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
nsMessenger::MarkMessagesRead(nsIRDFCompositeDataSource *database, nsIDOMNodeList *messages, PRBool markRead)
{
	nsresult rv;

	if(!database || !messages)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsISupportsArray> resourceArray, argumentArray;


	rv =ConvertDOMListToResourceArray(messages, getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return rv;

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
nsMessenger::AccountManager(nsIDOMWindow *parent)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURL> url;
  rv = NS_NewURL(getter_AddRefs(url),
                 "chrome://messenger/content/AccountManager.xul");
  if (NS_FAILED(rv)) return rv;

  nsIXULWindowCallbacks *cb = nsnull;
  nsIWebShellWindow* newWindow;
  rv = appShell->CreateDialogWindow(nsnull, // parent
                                    url, // UI url
                                    PR_TRUE, 
                                    newWindow,
                                    nsnull, // stream observer
                                    cb, // callbacks
                                    504, 436 ); // width, height

  if (NS_SUCCEEDED(rv) && newWindow)
    rv = newWindow->ShowModal();
  
  return rv;
}
