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
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsTransactionManagerCID.h"
#include "nsIComponentManager.h"
#include "nsIDocumentLoader.h"
#include "nsILoadGroup.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsPIDOMWindow.h"
#include "nsIPrompt.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"

// XXX Remove
#include "nsIWebShell.h"


static NS_DEFINE_CID(kTransactionManagerCID, NS_TRANSACTIONMANAGER_CID);
static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);

NS_IMPL_THREADSAFE_ISUPPORTS2(nsMsgWindow, nsIMsgWindow, nsIURIContentListener)

nsMsgWindow::nsMsgWindow()
{
  NS_INIT_ISUPPORTS();
}

nsMsgWindow::~nsMsgWindow()
{
  CloseWindow();
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

void nsMsgWindow::GetMessageWindowDocShell(nsIDocShell ** aDocShell)
{
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mMessageWindowDocShellWeak));
  if (!docShell)
  {
    // if we don't have a docshell, then we need to look up the message pane docshell
    nsCOMPtr<nsIDocShell> rootShell(do_QueryReferent(mRootDocShellWeak));
    if (rootShell)
    {
      nsCOMPtr<nsIDocShellTreeNode> rootAsNode(do_QueryInterface(rootShell));

      nsCOMPtr<nsIDocShellTreeItem> msgDocShellItem;
      if(rootAsNode)
         rootAsNode->FindChildWithName(NS_LITERAL_STRING("messagepane"), PR_TRUE, PR_FALSE,
                                       nsnull, getter_AddRefs(msgDocShellItem));

      docShell = do_QueryInterface(msgDocShellItem);
      // we don't own mMessageWindowDocShell so don't try to keep a reference to it!
      mMessageWindowDocShellWeak = getter_AddRefs(NS_GetWeakReference(docShell));
    }
  }

  *aDocShell = docShell;
  NS_IF_ADDREF(*aDocShell);
}

/* void SelectFolder (in string folderUri); */
NS_IMETHODIMP nsMsgWindow::SelectFolder(const char *folderUri)
{
	return mMsgWindowCommands->SelectFolder(folderUri);
}

/* void SelectMessage (in string messasgeUri); */
NS_IMETHODIMP nsMsgWindow::SelectMessage(const char *messageUri)
{
    return mMsgWindowCommands->SelectMessage(messageUri);
}

NS_IMETHODIMP nsMsgWindow::CloseWindow()
{
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_PROGID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->UnRegisterContentListener(this);

  // make sure the status feedback object
  // knows the window is going away...

  if (mStatusFeedback)
    mStatusFeedback->CloseWindow(); 

  nsCOMPtr<nsIDocShell> rootShell(do_QueryReferent(mRootDocShellWeak));

	if(rootShell)
	{
		rootShell->SetParentURIContentListener(nsnull);
		mRootDocShellWeak = nsnull;
		mMessageWindowDocShellWeak = nsnull;
	}

  return NS_OK;
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

  nsCOMPtr<nsIDocShell> messageWindowDocShell; 
  GetMessageWindowDocShell(getter_AddRefs(messageWindowDocShell));

  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(messageWindowDocShell));
  nsCOMPtr<nsIWebProgressListener> webProgressListener(do_QueryInterface(mStatusFeedback));

	mStatusFeedback = aStatusFeedback;

  // register our status feedback object
  if (webProgress && mStatusFeedback && messageWindowDocShell)
  {
    webProgressListener = do_QueryInterface(mStatusFeedback);
    webProgress->AddProgressListener(webProgressListener);
  }

	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetMsgHeaderSink(nsIMsgHeaderSink * *aMsgHdrSink)
{
	if(!aMsgHdrSink)
		return NS_ERROR_NULL_POINTER;

	*aMsgHdrSink = mMsgHeaderSink;
	NS_IF_ADDREF(*aMsgHdrSink);
	return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetMsgHeaderSink(nsIMsgHeaderSink * aMsgHdrSink)
{
	mMsgHeaderSink = aMsgHdrSink;
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

NS_IMETHODIMP nsMsgWindow::GetRootDocShell(nsIDocShell * *aDocShell)
{
  if(!aDocShell)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDocShell> rootShell(do_QueryReferent(mRootDocShellWeak));

  if (rootShell == nsnull)
    *aDocShell = nsnull;
  else
    rootShell->QueryInterface(nsIDocShell::GetIID(), (void **) aDocShell);

  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetRootDocShell(nsIDocShell * aDocShell)
{
  // Query for the doc shell and release it
  mRootDocShellWeak = nsnull;
  if (aDocShell)
  {
    mRootDocShellWeak = getter_AddRefs(NS_GetWeakReference(aDocShell));
    aDocShell->SetParentURIContentListener(this);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetMailCharacterSet(PRUnichar * *aMailCharacterSet)
{
  if(!aMailCharacterSet)
    return NS_ERROR_NULL_POINTER;

  *aMailCharacterSet = mMailCharacterSet.ToNewUnicode();
  if (!(*aMailCharacterSet))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetMailCharacterSet(const PRUnichar * aMailCharacterSet)
{
  mMailCharacterSet.Assign(aMailCharacterSet);

  // Convert to a canonical charset name instead of using the charset name from the message header as is.
  // This is needed for charset menu item to have a check mark correctly.
  nsresult rv;
  nsCOMPtr<nsICharsetConverterManager2> ccm2 = do_GetService(NS_CHARSETCONVERTERMANAGER_PROGID, &rv);
  if (NS_SUCCEEDED(rv)) 
  {
    nsCOMPtr <nsIAtom> charsetAtom;
    rv = ccm2->GetCharsetAtom(mMailCharacterSet.GetUnicode(), getter_AddRefs(charsetAtom));
    if (NS_SUCCEEDED(rv)) 
      rv = charsetAtom->ToString(mMailCharacterSet);
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetDOMWindow(nsIDOMWindow *aWindow)
{
	if (!aWindow)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;

	nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(aWindow));
  nsCOMPtr<nsIDocShell> docShell;
	if (globalScript)
		globalScript->GetDocShell(getter_AddRefs(docShell));

   nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));

   if(docShellAsItem)
   {
      nsCOMPtr<nsIDocShellTreeItem> rootAsItem;
      docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(rootAsItem));

      nsCOMPtr<nsIDocShell> rootAsShell(do_QueryInterface(rootAsItem));
      if(rootAsShell)
         rootAsShell->SetParentURIContentListener(this);

      nsAutoString childName; childName.AssignWithConversion("messagepane");
      nsCOMPtr<nsIDocShellTreeNode> rootAsNode(do_QueryInterface(rootAsItem));
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(rootAsItem));
      mRootDocShellWeak = getter_AddRefs(NS_GetWeakReference(docShell));

      // force ourselves to figure out the message pane
      nsCOMPtr<nsIDocShell> messageWindowDocShell; 
      GetMessageWindowDocShell(getter_AddRefs(messageWindowDocShell));
      SetStatusFeedback(mStatusFeedback);
   }

	//Get nsIMsgWindowCommands object
   nsCOMPtr<nsISupports> xpConnectObj;
   nsCOMPtr<nsPIDOMWindow> piDOMWindow(do_QueryInterface(aWindow));
   if (piDOMWindow)
   {
      nsAutoString msgWindowCommandsWinId; 
		  msgWindowCommandsWinId.AssignWithConversion("MsgWindowCommands");
      piDOMWindow->GetObjectProperty(msgWindowCommandsWinId.GetUnicode(), getter_AddRefs(xpConnectObj));
      mMsgWindowCommands = do_QueryInterface(xpConnectObj);
    }

	return rv;
}

NS_IMETHODIMP nsMsgWindow::StopUrls()
{
  nsCOMPtr<nsIDocShell> docShell;
  GetRootDocShell(getter_AddRefs(docShell));
  if (docShell)
  {
    return docShell->StopLoad();
  }
  
  nsCOMPtr<nsIDocShell> rootShell(do_QueryReferent(mRootDocShellWeak));
  nsCOMPtr <nsIWebShell> rootWebShell(do_QueryInterface(rootShell));
	if (rootWebShell)
	{
		nsCOMPtr <nsIDocumentLoader> docLoader;
		nsCOMPtr <nsILoadGroup> loadGroup;

		rootWebShell->GetDocumentLoader(*getter_AddRefs(docLoader));
		if (docLoader)
		{
			docLoader->GetLoadGroup(getter_AddRefs(loadGroup));
			if (loadGroup)
				loadGroup->Cancel(NS_BINDING_ABORTED);
		}
		return NS_OK;
	}
	return NS_ERROR_NULL_POINTER;
}


// nsIURIContentListener support
NS_IMETHODIMP nsMsgWindow::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   return NS_OK;
}

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
    // forward the DoContent call to our docshell
    nsCOMPtr<nsIDocShell> messageWindowDocShell; 
    GetMessageWindowDocShell(getter_AddRefs(messageWindowDocShell));
    nsCOMPtr<nsIURIContentListener> ctnListener = do_QueryInterface(messageWindowDocShell);
    if (ctnListener)
    {
      // get the url for the channel...let's hope it is a mailnews url so we can set our msg hdr sink on it..
      // right now, this is the only way I can think of to force the msg hdr sink into the mime converter so it can
      // get too it later...
      nsCOMPtr<nsIURI> uri;
      aChannel->GetURI(getter_AddRefs(uri));
      if (uri)
      {
        nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl(do_QueryInterface(uri));
        if (mailnewsUrl)
          mailnewsUrl->SetMsgWindow(this);
      }
      return ctnListener->DoContent(aContentType, aCommand, aWindowTarget, aChannel, aContentHandler, aAbortProcess);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgWindow::IsPreferred(const char * aContentType,
                         nsURILoadCommand aCommand,
                         const char * aWindowTarget,
                         char ** aDesiredContentType,
                         PRBool * aCanHandleContent)
{
  *aCanHandleContent = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::CanHandleContent(const char * aContentType,
                                            nsURILoadCommand aCommand,
                                            const char * aWindowTarget,
                                            char ** aDesiredContentType,
                                            PRBool * aCanHandleContent)

{
  // the mail window knows nothing about the default content types
  // it's docshell can handle...ask the content area if it can handle
  // the content type...
  
  nsCOMPtr<nsIDocShell> messageWindowDocShell; 
  GetMessageWindowDocShell(getter_AddRefs(messageWindowDocShell));
  nsCOMPtr<nsIURIContentListener> ctnListener (do_GetInterface(messageWindowDocShell));
  if (ctnListener)
    return ctnListener->CanHandleContent(aContentType, aCommand, aWindowTarget, 
                                         aDesiredContentType, aCanHandleContent);
  else
    *aCanHandleContent = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetParentContentListener(nsIURIContentListener** aParent)
{
  *aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetParentContentListener(nsIURIContentListener* aParent)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetLoadCookie(nsISupports ** aLoadCookie)
{
  *aLoadCookie = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetLoadCookie(nsISupports * aLoadCookie)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetPromptDialog(nsIPrompt **aPrompt)
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG_POINTER(aPrompt);
  nsCOMPtr<nsIDocShell> rootShell(do_QueryReferent(mRootDocShellWeak));
  if (rootShell)
  {
      nsCOMPtr<nsIPrompt> dialog;
      dialog = do_GetInterface(rootShell, &rv);
      if (dialog)
      {
          *aPrompt = dialog;
          NS_ADDREF(*aPrompt);
      }
      return rv;
  }
  else
      return NS_ERROR_NULL_POINTER;
}
