/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMsgWindow.h"
#include "nsReadableUtils.h"
#include "nsIURILoader.h"
#include "nsCURILoader.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsTransactionManagerCID.h"
#include "nsIComponentManager.h"
#include "nsILoadGroup.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsPIDOMWindow.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsICharsetAlias.h"
#include "nsIChannel.h"
#include "nsIRequestObserver.h"
#include "netCore.h"

#include "plbase64.h"
#include "nsMsgI18N.h"
#include "nsIWebNavigation.h"
#include "nsISupportsObsolete.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

static NS_DEFINE_CID(kTransactionManagerCID, NS_TRANSACTIONMANAGER_CID);

NS_IMPL_THREADSAFE_ISUPPORTS3(nsMsgWindow,
                              nsIMsgWindow,
                              nsIURIContentListener,
                              nsISupportsWeakReference)

nsMsgWindow::nsMsgWindow()
{
  mCharsetOverride = PR_FALSE;
  m_stopped = PR_FALSE;
}

nsMsgWindow::~nsMsgWindow()
{
  CloseWindow();
}

nsresult nsMsgWindow::Init()
{
  // register ourselves as a content listener with the uri dispatcher service
  nsresult rv;
  nsCOMPtr<nsIURILoader> dispatcher = 
           do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = dispatcher->RegisterContentListener(this);
  if (NS_FAILED(rv))
    return rv;

  // create Undo/Redo Transaction Manager
  mTransactionManager = do_CreateInstance(kTransactionManagerCID, &rv);
  if (NS_SUCCEEDED(rv))
    mTransactionManager->SetMaxTransactionCount(-1);

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
         rootAsNode->FindChildWithName(NS_LITERAL_STRING("messagepane").get(),
                                       PR_TRUE, PR_FALSE, nsnull, nsnull,
                                       getter_AddRefs(msgDocShellItem));

      docShell = do_QueryInterface(msgDocShellItem);
      // we don't own mMessageWindowDocShell so don't try to keep a reference to it!
      mMessageWindowDocShellWeak = do_GetWeakReference(docShell);
    }
  }

  *aDocShell = docShell;
  NS_IF_ADDREF(*aDocShell);
}

NS_IMETHODIMP nsMsgWindow::CloseWindow()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURILoader> dispatcher = do_GetService(NS_URI_LOADER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->UnRegisterContentListener(this);

  mMsgWindowCommands = nsnull;
  mStatusFeedback = nsnull;

  StopUrls();

  nsCOMPtr<nsIDocShell> rootShell(do_QueryReferent(mRootDocShellWeak));
  if(rootShell)
  {
    nsCOMPtr<nsIURIContentListener> listener(do_GetInterface(rootShell));
    if (listener) {
      listener->SetParentContentListener(nsnull);
    }
    mRootDocShellWeak = nsnull;
    mMessageWindowDocShellWeak = nsnull;
  }


  // in case nsMsgWindow leaks, make sure other stuff doesn't leak.
  mTransactionManager = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetStatusFeedback(nsIMsgStatusFeedback * *aStatusFeedback)
{
  if(!aStatusFeedback)
    return NS_ERROR_NULL_POINTER;

  NS_IF_ADDREF(*aStatusFeedback = mStatusFeedback);
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetStatusFeedback(nsIMsgStatusFeedback * aStatusFeedback)
{
  mStatusFeedback = aStatusFeedback;
  nsCOMPtr<nsIDocShell> messageWindowDocShell; 
  GetMessageWindowDocShell(getter_AddRefs(messageWindowDocShell));

  // register our status feedback object as a web progress listener
  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(messageWindowDocShell));
  if (webProgress && mStatusFeedback && messageWindowDocShell)
  {
    nsCOMPtr<nsIWebProgressListener> webProgressListener = do_QueryInterface(mStatusFeedback);
    webProgress->AddProgressListener(webProgressListener, nsIWebProgress::NOTIFY_ALL);
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetWindowCommands(nsIMsgWindowCommands * aMsgWindowCommands)
{
  mMsgWindowCommands = aMsgWindowCommands;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetWindowCommands(nsIMsgWindowCommands **aMsgWindowCommands)
{
  NS_ENSURE_ARG_POINTER(aMsgWindowCommands);
  NS_IF_ADDREF(*aMsgWindowCommands = mMsgWindowCommands);
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
  NS_ENSURE_ARG_POINTER(aTransactionManager);
  NS_IF_ADDREF(*aTransactionManager = mTransactionManager);
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetTransactionManager(nsITransactionManager * aTransactionManager)
{
  mTransactionManager = aTransactionManager;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetOpenFolder(nsIMsgFolder * *aOpenFolder)
{
  NS_ENSURE_ARG_POINTER(aOpenFolder);
  NS_IF_ADDREF(*aOpenFolder = mOpenFolder);
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetOpenFolder(nsIMsgFolder * aOpenFolder)
{
  mOpenFolder = aOpenFolder;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetRootDocShell(nsIDocShell * *aDocShell)
{
  if (mRootDocShellWeak) {
    CallQueryReferent(mRootDocShellWeak.get(), aDocShell);
  } else {
    *aDocShell = nsnull;
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetRootDocShell(nsIDocShell * aDocShell)
{
  // Query for the doc shell and release it
  mRootDocShellWeak = nsnull;
  if (aDocShell)
  {
    mRootDocShellWeak = do_GetWeakReference(aDocShell);
    nsCOMPtr<nsIURIContentListener> listener(do_GetInterface(aDocShell));
    if (listener)
      listener->SetParentContentListener(this);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetMailCharacterSet(char * *aMailCharacterSet)
{
  if(!aMailCharacterSet)
    return NS_ERROR_NULL_POINTER;

  *aMailCharacterSet = ToNewCString(mMailCharacterSet);
  if (!(*aMailCharacterSet))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetMailCharacterSet(const char * aMailCharacterSet)
{
  mMailCharacterSet.Assign(aMailCharacterSet);

  // Convert to a canonical charset name instead of using the charset name from the message header as is.
  // This is needed for charset menu item to have a check mark correctly.
  nsresult rv;
  nsCOMPtr<nsICharsetAlias> calias =
      do_GetService(NS_CHARSETALIAS_CONTRACTID, &rv);
  
  if (NS_SUCCEEDED(rv)) 
      calias->GetPreferred(nsDependentCString(aMailCharacterSet),
                           mMailCharacterSet);

  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetCharsetOverride(PRBool *aCharsetOverride)
{
  NS_ENSURE_ARG_POINTER(aCharsetOverride);
  *aCharsetOverride = mCharsetOverride;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetCharsetOverride(PRBool aCharsetOverride)
{
  mCharsetOverride = aCharsetOverride;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::GetDomWindow(nsIDOMWindowInternal **aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  if (mDomWindow)
    CallQueryReferent(mDomWindow.get(), aWindow);
  else
    *aWindow = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::SetDomWindow(nsIDOMWindowInternal * aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  mDomWindow = do_GetWeakReference(aWindow);

  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));
  nsIDocShell *docShell = nsnull;
  if (win)
    docShell = win->GetDocShell();

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));

  if(docShellAsItem)
  {
    nsCOMPtr<nsIDocShellTreeItem> rootAsItem;
    docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(rootAsItem));

    nsCOMPtr<nsIDocShell> rootAsShell(do_QueryInterface(rootAsItem));
    SetRootDocShell(rootAsShell);

    // force ourselves to figure out the message pane
    nsCOMPtr<nsIDocShell> messageWindowDocShell; 
    GetMessageWindowDocShell(getter_AddRefs(messageWindowDocShell));
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::StopUrls()
{
  m_stopped = PR_TRUE;
  nsCOMPtr<nsIWebNavigation> webnav(do_QueryReferent(mRootDocShellWeak));
  if (webnav)
  {
    return webnav->Stop(nsIWebNavigation::STOP_NETWORK);
  }
  
  return NS_ERROR_NULL_POINTER;
}


// nsIURIContentListener support
NS_IMETHODIMP nsMsgWindow::OnStartURIOpen(nsIURI* aURI, PRBool* aAbortOpen)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::DoContent(const char *aContentType, PRBool aIsContentPreferred,
                                     nsIRequest *request, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
  if (aContentType)
  {
    // forward the DoContent call to our docshell
    nsCOMPtr<nsIDocShell> messageWindowDocShell; 
    GetMessageWindowDocShell(getter_AddRefs(messageWindowDocShell));
    nsCOMPtr<nsIURIContentListener> ctnListener = do_QueryInterface(messageWindowDocShell);
    if (ctnListener)
    {
        nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
        if (!aChannel) return NS_ERROR_FAILURE;

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
      return ctnListener->DoContent(aContentType, aIsContentPreferred, request, aContentHandler, aAbortProcess);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgWindow::IsPreferred(const char * aContentType,
                         char ** aDesiredContentType,
                         PRBool * aCanHandleContent)
{
  *aCanHandleContent = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgWindow::CanHandleContent(const char * aContentType,
                                            PRBool aIsContentPreferred,
                                            char ** aDesiredContentType,
                                            PRBool * aCanHandleContent)

{
  // the mail window knows nothing about the default content types
  // its docshell can handle...ask the content area if it can handle
  // the content type...
  
  nsCOMPtr<nsIDocShell> messageWindowDocShell; 
  GetMessageWindowDocShell(getter_AddRefs(messageWindowDocShell));
  nsCOMPtr<nsIURIContentListener> ctnListener (do_GetInterface(messageWindowDocShell));
  if (ctnListener)
    return ctnListener->CanHandleContent(aContentType, aIsContentPreferred,
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

NS_IMETHODIMP 
nsMsgWindow::DisplayHTMLInMessagePane(const PRUnichar *title, const PRUnichar *body, PRBool clearMsgHdr)
{
  nsresult rv;

  if (clearMsgHdr && mMsgWindowCommands)
    mMsgWindowCommands->ClearMsgPane();

  nsString htmlStr;
  htmlStr.Append(NS_LITERAL_STRING("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"></head><body>").get());
  htmlStr.Append(body);
  htmlStr.Append(NS_LITERAL_STRING("</body></html>").get());

  char *encodedHtml = PL_Base64Encode(NS_ConvertUTF16toUTF8(htmlStr).get(), 0, nsnull);
  if (!encodedHtml)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCString dataSpec;
  dataSpec = "data:text/html;base64,";
  dataSpec += encodedHtml;

  PR_FREEIF(encodedHtml);

  nsCOMPtr <nsIURI> uri = do_CreateInstance("@mozilla.org/network/simple-uri;1");
  if (!uri) return NS_ERROR_UNEXPECTED;

  rv = uri->SetSpec(dataSpec);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIDocShell> docShell;
  GetMessageWindowDocShell(getter_AddRefs(docShell));
  if (!docShell) return NS_ERROR_UNEXPECTED;

  rv = docShell->LoadURI(uri,nsnull,nsIWebNavigation::LOAD_FLAGS_NONE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

NS_IMPL_GETSET(nsMsgWindow, Stopped, PRBool, m_stopped)
