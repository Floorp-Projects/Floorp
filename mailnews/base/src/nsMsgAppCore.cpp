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

#include "nsIDOMMsgAppCore.h"
#include "nsMsgAppCore.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMBaseAppCore.h"
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
#include "nsIMsgIdentity.h"
#include "nsIMailboxService.h"
#include "nsFileSpec.h"

#include "nsIMessage.h"
#include "nsIPop3Service.h"

#include "nsNNTPProtocol.h" // mscott - hopefully this dependency should only be temporary...

#include "nsIDOMXULTreeElement.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_CID(kCMailboxServiceCID, NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);
static NS_DEFINE_CID(kRDFServiceCID,	NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);

NS_BEGIN_EXTERN_C

nsresult NS_MailNewsLoadUrl(const nsString& urlString, nsISupports * aConsumer);

NS_END_EXTERN_C

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
  NS_IMETHOD DeleteMessage(nsIDOMXULTreeElement *tree, nsIDOMNodeList *nodeList);
  NS_IMETHOD GetRDFResourceForMessage(nsIDOMXULTreeElement *tree,
                                      nsIDOMNodeList *nodeList, nsISupports
                                      **aSupport); 

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
	printf("Init\n");
  mId = aId;
  return NS_OK;
}


nsresult
nsMsgAppCore::GetId(nsString& aId)
{
	printf("GetID\n");
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
	nsIURL* url;
	nsIWidget* newWindow;
  
	rv = NS_NewURL(&url, urlstr);
	if (NS_FAILED(rv)) {
	  goto done;
	}

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
  printf("nsMsgAppCore::GetNewMail()\n");
  // get the pop3 service and ask it to fetch new mail....
  nsIPop3Service * pop3Service = nsnull;
  nsresult rv = nsServiceManager::GetService(kCPop3ServiceCID, nsIPop3Service::GetIID(),
                                      (nsISupports **) &pop3Service);
  if (NS_SUCCEEDED(rv) && pop3Service)
	  pop3Service->GetNewMail(nsnull,nsnull);

  nsServiceManager::ReleaseService(kCPop3ServiceCID, pop3Service);

  return NS_OK;
}
                              
extern "C"
nsresult
NS_NewMsgAppCore(nsIDOMMsgAppCore **aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;

  nsMsgAppCore *appcore = new nsMsgAppCore();
  if (appcore) {
    return appcore->QueryInterface(nsIDOMMsgAppCore::GetIID(),
                                   (void **)aResult);

  }
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

void nsMsgAppCore::InitializeFolderRoot()
{
	// get the current identity from the mail session....
	nsIMsgMailSession * mailSession = nsnull;
	nsresult rv = nsServiceManager::GetService(kCMsgMailSessionCID,
	    							  nsIMsgMailSession::GetIID(),
                                      (nsISupports **) &mailSession);
	if (NS_SUCCEEDED(rv) && mailSession)
	{
		nsIMsgIdentity * identity = nsnull;
		rv = mailSession->GetCurrentIdentity(&identity);
		if (NS_SUCCEEDED(rv) && identity)
		{
			const char * folderRoot = nsnull;
			identity->GetRootFolderPath(&folderRoot);
			if (folderRoot)
			{
				// everyone should have a inbox so let's tack that folder name on to the root path...
				// mscott: this only works on windows...add 
				char * fullPath = PR_smprintf("%s\\%s", folderRoot, "Inbox");
				if (fullPath)
				{
					m_folderPath = fullPath;
					PR_Free(fullPath);
				}
			} // if we have a folder root for the current identity

			NS_IF_RELEASE(identity);
		} // if we have an identity

		// now release the mail service because we are done with it
		nsServiceManager::ReleaseService(kCMsgMailSessionCID, mailSession);
	} // if we have a mail session

	return;
}

NS_IMETHODIMP
nsMsgAppCore::OpenURL(const char * url)
{
	// mscott, okay this is all temporary hack code to support the Demo menu item which allows us to load
	// some hard coded news and mailbox urls.....it will ALL eventually go away.......

	if (url)
	{
		if (PL_strncmp(url, "news:", 5) == 0) // is it a news url?
			NS_MailNewsLoadUrl(url, mWebShell); 
		else if (PL_strncmp(url, "mailbox:", 8) == 0)
		{
			PRUint32 msgIndex;
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
				nsString folderURI;
				nsParseLocalMessageURI(url, folderURI, &msgIndex);
				char *rootURI = folderURI.ToNewCString();
				nsURI2Path(kMailboxRootURI, rootURI, folderPath);
				displayNumber = PR_FALSE;
			}
			nsIMailboxService * mailboxService = nsnull;
			nsresult rv = nsServiceManager::GetService(kCMailboxServiceCID, nsIMailboxService::GetIID(), (nsISupports **) &mailboxService);
			if (NS_SUCCEEDED(rv) && mailboxService)
			{
				nsIURL * url = nsnull;
				if(displayNumber)
					mailboxService->DisplayMessageNumber(folderPath, msgIndex, mWebShell, nsnull, nsnull);
				else
					mailboxService->DisplayMessage(folderPath, msgIndex, nsnull, mWebShell, nsnull, nsnull);

				nsServiceManager::ReleaseService(kCMailboxServiceCID, mailboxService);
			}
		}

	}
	return NS_OK;
}

NS_IMETHODIMP
nsMsgAppCore::DeleteMessage(nsIDOMXULTreeElement *tree, nsIDOMNodeList *nodeList)
{
	nsresult rv;
	nsIRDFCompositeDataSource *database;
	nsISupportsArray *resourceArray;


	if(NS_FAILED(rv = tree->GetDatabase(&database)))
		return rv;

	if(NS_FAILED(rv =ConvertDOMListToResourceArray(nodeList, &resourceArray)))
		return rv;

	nsIRDFService* gRDFService = nsnull;
	nsIRDFResource* deleteResource;
	rv = nsServiceManager::GetService(kRDFServiceCID,
												nsIRDFService::GetIID(),
												(nsISupports**) &gRDFService);
	if(NS_SUCCEEDED(rv))
	{
		if(NS_SUCCEEDED(rv = gRDFService->GetResource("http://home.netscape.com/NC-rdf#Delete", &deleteResource)))
		{
			rv = database->DoCommand(resourceArray, deleteResource, nsnull);
			NS_RELEASE(deleteResource);
		}
		nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
	}

	NS_RELEASE(database);
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


//  to load the webshell!
//  mWebShell->LoadURL(nsAutoString("http://www.netscape.com"), 
//                      nsnull, PR_TRUE, nsURLReload, 0);
