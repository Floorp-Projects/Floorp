/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 *
 */

// nsMsgPrintEngine.cpp: provides a WebShell container for use 
// in printing

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsRepository.h"

#include "nsISupports.h"

#include "nsIURI.h"
#include "nsEscape.h"
#include "nsIWebShell.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIContentViewerFile.h"
#include "nsIContentViewer.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIMarkupDocumentViewer.h"

#include "nsMsgPrintEngine.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"

/////////////////////////////////////////////////////////////////////////
// nsMsgPrintEngine implementation
/////////////////////////////////////////////////////////////////////////

nsMsgPrintEngine::nsMsgPrintEngine() :
  mWebShell(nsnull)
{
  mCurrentlyPrintingURI = -1;
  mWindow = nsnull;
  NS_INIT_REFCNT();
}


nsMsgPrintEngine::~nsMsgPrintEngine()
{
  NS_IF_RELEASE(mWindow);
}

// Implement AddRef and Release
NS_IMPL_ADDREF(nsMsgPrintEngine)
NS_IMPL_RELEASE(nsMsgPrintEngine)
NS_IMPL_QUERY_INTERFACE2(nsMsgPrintEngine,nsIMsgPrintEngine, nsIDocumentLoaderObserver);

nsresult nsMsgPrintEngine::Init()
{
	return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::OnStartDocumentLoad(nsIDocumentLoader *aLoader, nsIURI *aURL, const char *aCommand)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::OnEndDocumentLoad(nsIDocumentLoader *loader, nsIChannel *aChannel, PRUint32 aStatus)
{
  // Now, fire off the print operation!
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIContentViewer> viewer;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
  NS_ASSERTION(docShell,"can't print, there is no webshell");
  if ( (!docShell) || (!aChannel) ) 
  {
    return StartNextPrintOperation();
  }

  // Make sure this isn't just "about:blank" finishing....
  nsIURI    *aOriginalURI = nsnull;
  if (NS_SUCCEEDED(aChannel->GetOriginalURI(&aOriginalURI)))
  {
    char  *spec = nsnull;

    if (NS_SUCCEEDED(aOriginalURI->GetSpec(&spec)) && spec)
    {      
      if (!nsCRT::strcasecmp(spec, "about:blank"))
      {
        return StartNextPrintOperation();
      }
    }
  }

  docShell->GetContentViewer(getter_AddRefs(viewer));  
  if (viewer) 
  {
    nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
    if (viewerFile) 
    {
      rv = viewerFile->Print(PR_FALSE, nsnull);
    }
  }

  return StartNextPrintOperation();
}

NS_IMETHODIMP
nsMsgPrintEngine::OnStartURLLoad(nsIDocumentLoader *aLoader, nsIChannel *channel)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::OnProgressURLLoad(nsIDocumentLoader *aLoader, nsIChannel *aChannel, PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::OnStatusURLLoad(nsIDocumentLoader *loader, nsIChannel *channel, nsString & aMsg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::OnEndURLLoad(nsIDocumentLoader *aLoader, nsIChannel *aChannel, PRUint32 aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP    
nsMsgPrintEngine::SetWindow(nsIDOMWindow *aWin)
{
	if (!aWin)
  {
    // It isn't an error to pass in null for aWin, in fact it means we are shutting
    // down and we should start cleaning things up...
		return NS_OK;
  }

  NS_IF_RELEASE(mWindow);
  mWindow = aWin;
  NS_ADDREF(aWin);

  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  NS_ENSURE_TRUE(globalObj, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> rootAsItem;
  docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(rootAsItem));

  nsAutoString childName("printengine");
  nsCOMPtr<nsIDocShellTreeNode> rootAsNode(do_QueryInterface(rootAsItem));
  NS_ENSURE_TRUE(rootAsNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> childItem;
  rootAsNode->FindChildWithName(childName.GetUnicode(), PR_TRUE, PR_FALSE, nsnull,
    getter_AddRefs(childItem));

  mWebShell = do_QueryInterface(childItem);

  if(mWebShell)
    SetupObserver();

  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::AddPrintURI(const PRUnichar *aMsgURI)
{
  mURIArray.AppendString(aMsgURI);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::SetPrintURICount(PRInt32 aCount)
{
  mURICount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::StartPrintOperation()
{
  return StartNextPrintOperation();
}

NS_IMETHODIMP
nsMsgPrintEngine::StartNextPrintOperation()
{
  nsresult      rv;

  // Only do this the first time through...
  if (mCurrentlyPrintingURI == -1)
    InitializeDisplayCharset();

  // SetupObserver();

  // First, check if we are at the end of this stuff!
  if ( (mCurrentlyPrintingURI+1) >= mURIArray.Count() )
  {
    // Release();
    return NS_OK;
  }

  mCurrentlyPrintingURI++;
  if (!mWebShell)
    return StartNextPrintOperation();

  // SetWindow(mWindow);
  nsString *uri = mURIArray.StringAt(mCurrentlyPrintingURI);
  rv = FireThatLoadOperation(uri);
  if (NS_FAILED(rv))
    return StartNextPrintOperation();
  else
    return rv;
}

NS_IMETHODIMP
nsMsgPrintEngine::FireThatLoadOperation(nsString *uri)
{
  nsresult      rv = NS_OK;

  char  *tString = uri->ToNewCString();
  if (!tString)
    return NS_ERROR_OUT_OF_MEMORY;

  nsIMsgMessageService * messageService = nsnull;
  rv = GetMessageServiceFromURI(tString, &messageService);
  
  if (NS_SUCCEEDED(rv) && messageService)
  {
    rv = messageService->DisplayMessageForPrinting(tString, mWebShell, nsnull, nsnull, nsnull);
    ReleaseMessageServiceFromURI(tString, messageService);
  }
  //If it's not something we know about, then just load try loading it directly.
  else
  {
    if (mWebShell)
      mWebShell->LoadURL(uri->GetUnicode(), nsnull, nsIChannel::LOAD_DOCUMENT_URI);
  }

  PR_FREEIF(tString);
  return rv;
}

void
nsMsgPrintEngine::InitializeDisplayCharset()
{
  // libmime always converts to UTF-8 (both HTML and XML)
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
  if (docShell) 
  {
    nsAutoString aForceCharacterSet("UTF-8");
    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) 
    {
      nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);
      if (muDV) 
      {
        muDV->SetForceCharacterSet(aForceCharacterSet.GetUnicode());
      }
    }
  }
}

void
nsMsgPrintEngine::SetupObserver()
{
  if (!mWebShell)
    return;

  nsCOMPtr<nsIDocumentLoaderObserver> observer = do_QueryInterface(this);
  if (observer)
  {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
    docShell->SetDocLoaderObserver(observer);
  } 
}

