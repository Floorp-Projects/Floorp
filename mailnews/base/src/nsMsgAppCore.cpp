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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "prsystem.h"

#include "nsIDOMMsgAppCore.h"
#include "nsMsgAppCore.h"
#include "nsIScriptObjectOwner.h"
#include "nsAppCoresCIDs.h"
#include "nsIDOMBaseAppCore.h"
#include "nsIDOMAppCoresManager.h"
#include "nsJSMsgAppCore.h"

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
#include "nsIMailboxService.h"
#include "nsINntpService.h"
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

#include "nsINetService.h"
#include "nsCopyMessageStreamListener.h"
#include "nsICopyMessageListener.h"

#include "nsIMessageView.h"

static NS_DEFINE_IID(kIDOMAppCoresManagerIID, NS_IDOMAPPCORESMANAGER_IID);
static NS_DEFINE_IID(kAppCoresManagerCID,  NS_APPCORESMANAGER_CID);

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_CID(kCMailboxServiceCID, NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kCNntpServiceCID, NS_NNTPSERVICE_CID);
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

class nsMsgAppCore : public nsIDOMMsgAppCore,
                     public nsIScriptObjectOwner
{
  
public:
  nsMsgAppCore();
  virtual ~nsMsgAppCore();

  NS_DECL_ISUPPORTS
  NS_DECL_IDOMBASEAPPCORE

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  // nsIMsgAppCore
  NS_IMETHOD Open3PaneWindow();
  NS_IMETHOD GetNewMail();
  NS_IMETHOD SetWindow(nsIDOMWindow* aWin);
  NS_IMETHOD OpenURL(const char * url);
  NS_IMETHOD DeleteMessage(nsIDOMXULTreeElement *tree, nsIDOMXULElement *srcFolderElement, nsIDOMNodeList *nodeList);
  NS_IMETHOD CopyMessages(nsIDOMXULElement *srcFolderElement, nsIDOMXULElement *folderElement, nsIDOMNodeList *nodeList,
						  PRBool isMove);
  NS_IMETHOD GetRDFResourceForMessage(nsIDOMXULTreeElement *tree,
                                      nsIDOMNodeList *nodeList, nsISupports
                                      **aSupport); 
  NS_IMETHOD Exit();
  NS_IMETHOD ViewAllMessages(nsIRDFCompositeDataSource *databsae);
  NS_IMETHOD ViewUnreadMessages(nsIRDFCompositeDataSource *databsae);
	NS_IMETHOD ViewAllThreadMessages(nsIRDFCompositeDataSource *database);
	NS_IMETHOD NewFolder(nsIRDFCompositeDataSource *database, nsIDOMXULElement *parentFolderElement,
						const char *name);



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
	if(!messageView)
		return NS_ERROR_NULL_POINTER;

	nsIRDFService* gRDFService = nsnull;
	nsIRDFDataSource *view, *datasource;
	nsresult rv;
	rv = nsServiceManager::GetService(kRDFServiceCID,
												nsIRDFService::GetIID(),
												(nsISupports**) &gRDFService);
	if(NS_SUCCEEDED(rv))
	{
		rv = gRDFService->GetDataSource("rdf:mail-messageview", &view);
		rv = NS_SUCCEEDED(rv) && gRDFService->GetDataSource("rdf:mailnews", &datasource);
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
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
nsresult nsMsgAppCore::SetDocumentCharset(class nsString const & aCharset) 
{
	nsresult res = NS_OK;
  // This changes a charset of messenger's .xul.
	if (nsnull != mWindow) 
	{
		nsIDOMDocument* domDoc;
		res = mWindow->GetDocument(&domDoc);
		if (NS_SUCCEEDED(res) && nsnull != domDoc) 
		{
			nsIDocument * doc;
			res = domDoc->QueryInterface(nsIDocument::GetIID(), (void**)&doc);
			if (NS_SUCCEEDED(res) && nsnull != doc) 
			{
				nsString *aNewCharset = new nsString(aCharset);
				if (nsnull != aNewCharset) 
				{
					doc->SetDocumentCharacterSet(aNewCharset);
				}
				
				NS_RELEASE(doc);
			}
			
			NS_RELEASE(domDoc);
		}
	}
  // This changes a charset of nsIDocument for the message view.
  if (nsnull != mWebShell) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    if (NS_SUCCEEDED(res = mWebShell->GetContentViewer(getter_AddRefs(contentViewer)))) {
      nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(contentViewer, &res));
      if (NS_SUCCEEDED(res)) {
        // Get the document object
        nsCOMPtr<nsIDocument> doc;
        if (NS_SUCCEEDED(res = docViewer->GetDocument(*getter_AddRefs(doc)))) {
          nsString *aNewCharset = new nsString(aCharset);
          if (nsnull != aNewCharset) 
          {
            doc->SetDocumentCharacterSet(aNewCharset);
          }
        }
      }
    }
  }
	return res;
}

//
// nsMsgAppCore
//
nsMsgAppCore::nsMsgAppCore() : m_folderPath("")
{
	NS_INIT_REFCNT();
	mScriptObject = nsnull;
	mWebShell = nsnull; 
	mWindow = nsnull;

	InitializeFolderRoot();
}

nsMsgAppCore::~nsMsgAppCore()
{
	// remove ourselves from the app cores manager...
	// if we were able to inherit directly from nsBaseAppCore then it would do this for
	// us automatically

	nsIDOMAppCoresManager * appCoreManager;
	nsresult rv = nsServiceManager::GetService(kAppCoresManagerCID, kIDOMAppCoresManagerIID,
											   (nsISupports**)&appCoreManager);
	if (NS_SUCCEEDED(rv) && appCoreManager)
	{
		appCoreManager->Remove((nsIDOMBaseAppCore *) this);
		nsServiceManager::ReleaseService(kAppCoresManagerCID, appCoreManager);
	}
}

//
// nsISupports
//
NS_IMPL_ADDREF(nsMsgAppCore);
NS_IMPL_RELEASE(nsMsgAppCore);

NS_IMETHODIMP
nsMsgAppCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
      return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIScriptObjectOwnerIID)) {
      *aInstancePtr = (void*) ((nsIScriptObjectOwner*)this);
      AddRef();
      return NS_OK;
  }
  if ( aIID.Equals(nsIDOMBaseAppCore::GetIID())) {
      *aInstancePtr = (void*) ((nsIDOMBaseAppCore*)this);
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(nsIDOMMsgAppCore::GetIID()) ) {
      *aInstancePtr = (void*) (nsIDOMMsgAppCore*)this;
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(kISupportsIID) ) {
      *aInstancePtr = (void*)(nsISupports*) (nsIScriptObjectOwner *) this;
      AddRef();
      return NS_OK;
  }

  return NS_NOINTERFACE;
}

//
// nsIScriptObjectOwner
//
nsresult
nsMsgAppCore::GetScriptObject(nsIScriptContext *aContext, void **aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptMsgAppCore(aContext, 
                                   (nsISupports *)(nsIDOMMsgAppCore*)this, 
                                   nsnull, 
                                   &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}

nsresult
nsMsgAppCore::SetScriptObject(void* aScriptObject)
{
  
  return NS_OK;
} 

//
// nsIDOMBaseAppCore
//
nsresult
nsMsgAppCore::Init(const nsString& aId)
{
	mId = aId;
	
	// add ourselves to the app cores manager...
	// if we were able to inherit directly from nsBaseAppCore then it would do this for
	// us automatically

	nsIDOMAppCoresManager * appCoreManager;
	nsresult rv = nsServiceManager::GetService(kAppCoresManagerCID, kIDOMAppCoresManagerIID,
											   (nsISupports**)&appCoreManager);
	if (NS_SUCCEEDED(rv) && appCoreManager)
	{
		appCoreManager->Add((nsIDOMBaseAppCore *) this);
		nsServiceManager::ReleaseService(kAppCoresManagerCID, appCoreManager);
	}
	return NS_OK;
}


nsresult
nsMsgAppCore::GetId(nsString& aId)
{
  aId = mId;
  return NS_OK;
}

//
// nsIMsgAppCore
//
NS_IMETHODIMP    
nsMsgAppCore::Open3PaneWindow()
{
	static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

	nsIAppShellService* appShell;
	char *  urlstr=nsnull;
	nsresult rv;
	nsString controllerCID;

	urlstr = "resource:/res/samples/messenger.html";
	rv = nsServiceManager::GetService(kAppShellServiceCID,
									  nsIAppShellService::GetIID(),
									  (nsISupports**)&appShell);
  
	nsIURL* url = nsnull;
	nsINetService * pNetService;
	rv = nsServiceManager::GetService(kNetServiceCID, nsINetService::GetIID(), (nsISupports **)&pNetService);
	if (NS_SUCCEEDED(rv) && pNetService) {
		rv = pNetService->CreateURL(&url, urlstr);
		NS_RELEASE(pNetService);
		if (NS_FAILED(rv))
			goto done;
	}
	else
		goto done;


	nsIWebShellWindow* newWindow;
	controllerCID = "6B75BB61-BD41-11d2-9D31-00805F8ADDDE";
	appShell->CreateTopLevelWindow(nsnull,      // parent
                                   url,
                                   controllerCID,
                                   newWindow,   // result widget
                                   nsnull,      // observer
                                   nsnull,      // callbacks
                                   200,         // width
                                   200);        // height
	done:
	NS_RELEASE(url);
	if (nsnull != appShell) {
		nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
	}
	return NS_OK;
}

nsresult
nsMsgAppCore::GetNewMail()
{

    nsresult rv;
    NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    NS_WITH_SERVICE(nsIPop3Service, pop3Service, kCPop3ServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIMsgIncomingServer *server;
    rv = mailSession->GetCurrentServer(&server);
    if (NS_FAILED(rv)) return rv;

    nsIPop3IncomingServer *popServer;
    rv = server->QueryInterface(nsIPop3IncomingServer::GetIID(),
                                (void **)&popServer);
    if (NS_SUCCEEDED(rv)) {
        rv = pop3Service->GetNewMail(nsnull,popServer,nsnull);
        NS_RELEASE(popServer);
    }
    
    NS_RELEASE(server);
    
    return rv;
}
                              
extern "C"
nsresult
NS_NewMsgAppCore(const nsIID &aIID, void **aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;

  nsMsgAppCore *appcore = new nsMsgAppCore();
  if (appcore)
    return appcore->QueryInterface(aIID, (void **)aResult);
  else
	return NS_ERROR_NOT_INITIALIZED;
}


NS_IMETHODIMP    
nsMsgAppCore::SetWindow(nsIDOMWindow* aWin)
{
  nsAutoString  webShellName("browser.webwindow");
  mWindow = aWin;
  NS_ADDREF(aWin);

  /* rhp - Needed to access the webshell to drive message display */
  printf("nsMsgAppCore::SetWindow(): Getting the webShell of interest...\n");

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
    rootWebShell->FindChildWithName(webShellName, mWebShell);
#ifdef NS_DEBUG
    if (nsnull != mWebShell)
        printf("nsMsgAppCore::SetWindow(): Got the webShell %s.\n", webShellName.ToNewCString());
    else
        printf("nsMsgAppCore::SetWindow(): Failed to find webshell %s.\n", webShellName.ToNewCString());
#endif
    NS_RELEASE(rootWebShell);
  }

  NS_RELEASE(webShell);
	return NS_OK;
}


// this should really go through all the pop servers and initialize all
// folder roots
void nsMsgAppCore::InitializeFolderRoot()
{
    nsresult rv;
    
	// get the current identity from the mail session....
    NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kCMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return;

    nsIMsgIncomingServer* server = nsnull;
    rv = mailSession->GetCurrentServer(&server);
    if (NS_SUCCEEDED(rv) && server) {
        char * folderRoot = nsnull;
        nsIPop3IncomingServer *popServer;
        rv = server->QueryInterface(nsIPop3IncomingServer::GetIID(),
                                    (void **)&popServer);
        if (NS_SUCCEEDED(rv)) {
            popServer->GetRootFolderPath(&folderRoot);
            if (folderRoot) {
                // everyone should have a inbox so let's
                // tack that folder name on to the root path...
                m_folderPath = folderRoot;
                m_folderPath += "Inbox";
            } // if we have a folder root for the current identity
            NS_IF_RELEASE(popServer);
        }
        NS_IF_RELEASE(server);
    } // if we have an server
    
	return;
}

NS_IMETHODIMP
nsMsgAppCore::OpenURL(const char * url)
{
	// mscott, okay this is all temporary hack code to support the Demo menu item which allows us to load
	// some hard coded news and mailbox urls.....it will ALL eventually go away.......

	if (url)
	{
		// turn off news for now...
		if (PL_strncmp(url, "news:", 5) == 0) // is it a news url?
		{
			nsINntpService * nntpService = nsnull;
			nsresult rv = nsServiceManager::GetService(kCNntpServiceCID, nsINntpService::GetIID(), (nsISupports **) &nntpService);
			if (NS_SUCCEEDED(rv) && nntpService)
			{
				nntpService->RunNewsUrl(url, mWebShell, nsnull, nsnull);
				nsServiceManager::ReleaseService(kCNntpServiceCID, nntpService);
			}
		}
		if (PL_strncmp(url, "mailbox:", 8) == 0 || PL_strncmp(url, kMailboxMessageRootURI, PL_strlen(kMailboxMessageRootURI)) == 0)
		{
			PRUint32 msgIndex=0;
			nsFileSpec folderPath; 
			PRBool displayNumber;
			if(isdigit(url[8]))
			{
				// right now these urls are just mailbox:# where # is the ordinal number representing what message
				// we want to load...we have a whole syntax for mailbox urls which are used in the normal case but
				// we aren't using for this little demo menu....
				url += 8; // skip past mailbox: stuff...
				msgIndex = atol(url); // extract the index to use...
				folderPath = m_folderPath;
				displayNumber = PR_TRUE;
			}
			else
			{
				displayNumber = PR_FALSE;
			}
			nsIMailboxService * mailboxService = nsnull;
			nsresult rv = nsServiceManager::GetService(kCMailboxServiceCID, nsIMailboxService::GetIID(), (nsISupports **) &mailboxService);
			if (NS_SUCCEEDED(rv) && mailboxService)
			{
				if(displayNumber)
					mailboxService->DisplayMessageNumber(folderPath, msgIndex, mWebShell, nsnull, nsnull);
				else
					mailboxService->DisplayMessage(url, mWebShell, nsnull, nsnull);

				nsServiceManager::ReleaseService(kCMailboxServiceCID, mailboxService);
			}
		}

	}
	return NS_OK;
}

NS_IMETHODIMP
nsMsgAppCore::DeleteMessage(nsIDOMXULTreeElement *tree, nsIDOMXULElement *srcFolderElement, nsIDOMNodeList *nodeList)
{
	nsresult rv;
	nsIRDFCompositeDataSource *database;
	nsISupportsArray *resourceArray, *folderArray;
	nsIRDFResource *resource;
	nsIMsgFolder *srcFolder;

	if(NS_FAILED(rv = srcFolderElement->GetResource(&resource)))
		return rv;

	if(NS_FAILED(rv = resource->QueryInterface(nsIMsgFolder::GetIID(), (void**)&srcFolder)))
		return rv;

	if(NS_FAILED(rv = tree->GetDatabase(&database)))
		return rv;

	if(NS_FAILED(rv =ConvertDOMListToResourceArray(nodeList, &resourceArray)))
		return rv;

	if(NS_FAILED(NS_NewISupportsArray(&folderArray)))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	folderArray->AppendElement(srcFolder);
	
	nsIRDFService* gRDFService = nsnull;
	nsIRDFResource* deleteResource;
	rv = nsServiceManager::GetService(kRDFServiceCID,
												nsIRDFService::GetIID(),
												(nsISupports**) &gRDFService);
	if(NS_SUCCEEDED(rv))
	{
		if(NS_SUCCEEDED(rv = gRDFService->GetResource("http://home.netscape.com/NC-rdf#Delete", &deleteResource)))
		{
			rv = database->DoCommand(folderArray, deleteResource, resourceArray);
			NS_RELEASE(deleteResource);
		}
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
	}

	NS_RELEASE(database);
	NS_RELEASE(resourceArray);
	NS_RELEASE(resource);
	NS_RELEASE(srcFolder);
	NS_RELEASE(folderArray);
	return rv;
}

NS_IMETHODIMP
nsMsgAppCore::CopyMessages(nsIDOMXULElement *srcFolderElement, nsIDOMXULElement *dstFolderElement,
						   nsIDOMNodeList *nodeList, PRBool isMove)
{
	nsresult rv;
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

	if(resourceArray->Count() > 0)
	{
		nsIRDFResource * firstMessage = (nsIRDFResource*)resourceArray->ElementAt(0);
		char *uri;
		firstMessage->GetValue(&uri);
		nsCopyMessageStreamListener* copyStreamListener = new nsCopyMessageStreamListener(srcFolder, dstFolder, nsnull);

		nsIMailboxService * mailboxService = nsnull;
		nsresult rv = nsServiceManager::GetService(kCMailboxServiceCID, nsIMailboxService::GetIID(), (nsISupports **) &mailboxService);
		if (NS_SUCCEEDED(rv) && mailboxService)
		{
			nsIURL * url = nsnull;
			mailboxService->CopyMessage(uri, copyStreamListener, isMove, nsnull, &url);

			nsServiceManager::ReleaseService(kCMailboxServiceCID, mailboxService);
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
nsMsgAppCore::GetRDFResourceForMessage(nsIDOMXULTreeElement *tree,
                                       nsIDOMNodeList *nodeList, nsISupports
                                       **aSupport) 
{
      nsresult rv;
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
nsMsgAppCore::Exit()
{
  nsIAppShellService* appShell = nsnull;

  /*
   * Create the Application Shell instance...
   */
  nsresult rv = nsServiceManager::GetService(kAppShellServiceCID,
											nsIAppShellService::GetIID(),
                                             (nsISupports**)&appShell);
  if (NS_SUCCEEDED(rv)) {
    appShell->Shutdown();
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  } 
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAppCore::ViewAllMessages(nsIRDFCompositeDataSource *database)
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
nsMsgAppCore::ViewUnreadMessages(nsIRDFCompositeDataSource *database)
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
nsMsgAppCore::ViewAllThreadMessages(nsIRDFCompositeDataSource *database)
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
nsMsgAppCore::NewFolder(nsIRDFCompositeDataSource *database, nsIDOMXULElement *parentFolderElement,
						const char *name)
{
	nsresult rv;
	nsIRDFResource *resource;
	nsIMsgFolder *parentFolder;
	nsISupportsArray *nameArray, *folderArray;

	if(!parentFolderElement || !name)
		return NS_ERROR_NULL_POINTER;

	rv = parentFolderElement->GetResource(&resource);
	if(NS_FAILED(rv))
		return rv;

	rv = resource->QueryInterface(nsIMsgFolder::GetIID(), (void**)&parentFolder);
	if(NS_FAILED(rv))
		return rv;

	if(NS_FAILED(NS_NewISupportsArray(&nameArray)))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	if(NS_FAILED(NS_NewISupportsArray(&folderArray)))
		return NS_ERROR_OUT_OF_MEMORY;

	folderArray->AppendElement(parentFolder);

	nsIRDFService* gRDFService = nsnull;
	rv = nsServiceManager::GetService(kRDFServiceCID,
												nsIRDFService::GetIID(),
												(nsISupports**) &gRDFService);
	if(NS_SUCCEEDED(rv))
	{
		nsString nameStr = name;
		nsIRDFLiteral *nameLiteral;
		nsIRDFResource *newFolderResource;

	    gRDFService->GetLiteral(nameStr, &nameLiteral);
		nameArray->AppendElement(nameLiteral);
		if(NS_SUCCEEDED(rv = gRDFService->GetResource("http://home.netscape.com/NC-rdf#NewFolder", &newFolderResource)))
		{
			rv = database->DoCommand(folderArray, newFolderResource, nameArray);
			NS_RELEASE(newFolderResource);
		}
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
	}
	NS_IF_RELEASE(nameArray);
	NS_IF_RELEASE(resource);
	NS_IF_RELEASE(parentFolder);
	return rv;
}

//  to load the webshell!
//  mWebShell->LoadURL(nsAutoString("http://www.netscape.com"), 
//                      nsnull, PR_TRUE, nsURLReload, 0);
