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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
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

// nsMsgPrintEngine.cpp: provides a DocShell container for use 
// in printing

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsIComponentManager.h"

#include "nsISupports.h"

#include "nsIURI.h"
#include "nsEscape.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsIWebProgress.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIMsgMailSession.h"
#include "nsMsgPrintEngine.h"
#include "nsMsgBaseCID.h"
#include "nsIDocumentLoader.h"
#include "nsIWebShellWindow.h"
#include "nsIWidget.h"
#include "nsIWebShellWindow.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

// For PLEvents
#include "plevent.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIWebNavigation.h"
#include "nsIChannel.h"
#include "nsIContentViewerFile.h"
#include "nsIPrintingPromptService.h"

static const char* kPrintingPromptService = "@mozilla.org/embedcomp/printingprompt-service;1";

/////////////////////////////////////////////////////////////////////////
// nsMsgPrintEngine implementation
/////////////////////////////////////////////////////////////////////////

nsMsgPrintEngine::nsMsgPrintEngine() :
  mIsDoingPrintPreview(PR_FALSE),
  mMsgInx(nsIMsgPrintEngine::MNAB_START)
{
  mCurrentlyPrintingURI = -1;
}


nsMsgPrintEngine::~nsMsgPrintEngine()
{
}

// Implement AddRef and Release
NS_IMPL_ISUPPORTS4(nsMsgPrintEngine,
                         nsIMsgPrintEngine, 
                         nsIWebProgressListener, 
                         nsIObserver,
                         nsISupportsWeakReference)

// nsIWebProgressListener implementation
NS_IMETHODIMP
nsMsgPrintEngine::OnStateChange(nsIWebProgress* aWebProgress, 
                   nsIRequest *aRequest, 
                   PRUint32 progressStateFlags, 
                   nsresult aStatus)
{
  nsresult rv = NS_OK;

  // top-level document load data
  if (progressStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT) {
    if (progressStateFlags & nsIWebProgressListener::STATE_START) {
      // Tell the user we are loading...
      PRUnichar *msg = GetString(NS_LITERAL_STRING("LoadingMessageToPrint").get());
      SetStatusMessage( msg );
      CRTFREEIF(msg)
    }

    if (progressStateFlags & nsIWebProgressListener::STATE_STOP) {
      nsCOMPtr<nsIDocumentLoader> docLoader(do_QueryInterface(aWebProgress));
      if (docLoader) 
      {
        // Check to see if the document DOMWin that is finished loading is the same
        // one as the mail msg that we started to load.
        // We only want to print when the entire msg and all of its attachments
        // have finished loading.
        // The mail msg doc is the last one to receive the STATE_STOP notification
        nsCOMPtr<nsISupports> container;
        docLoader->GetContainer(getter_AddRefs(container));
        nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(container));
        if (domWindow.get() != mMsgDOMWin.get()) {
          return NS_OK;
        }
      }
      nsCOMPtr<nsIWebProgressListener> wpl(do_QueryInterface(mPrintPromptService));
      if (wpl) {
        wpl->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP|nsIWebProgressListener::STATE_IS_DOCUMENT, nsnull);
        mPrintProgressListener = nsnull;
        mPrintProgress         = nsnull;
        mPrintProgressParams   = nsnull;
      }

      PRBool isPrintingCancelled = PR_FALSE;
      if (mPrintSettings)
      {
        mPrintSettings->GetIsCancelled(&isPrintingCancelled);
      }
      if (!isPrintingCancelled) {
        // if aWebProgress is a documentloader than the notification from
        // loading the documents. If it is NULL (or not a DocLoader) then it 
        // it coming from Printing
        if (docLoader) {
          // Now, fire off the print operation!
          rv = NS_ERROR_FAILURE;

          // Tell the user the message is loaded...
          PRUnichar *msg = GetString(NS_LITERAL_STRING("MessageLoaded").get());
          SetStatusMessage( msg );
          if (msg) nsCRT::free(msg);

          NS_ASSERTION(mDocShell,"can't print, there is no docshell");
          if ( (!mDocShell) || (!aRequest) ) 
          {
            return StartNextPrintOperation();
          }
          nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(aRequest);
          if (!aChannel) return NS_ERROR_FAILURE;

          // Make sure this isn't just "about:blank" finishing....
          nsCOMPtr<nsIURI> originalURI = nsnull;
          if (NS_SUCCEEDED(aChannel->GetOriginalURI(getter_AddRefs(originalURI))) && originalURI)
          {
            nsCAutoString spec;

            if (NS_SUCCEEDED(originalURI->GetSpec(spec)))
            {      
              if (spec.Equals("about:blank"))
              {
                return StartNextPrintOperation();
              }
            }
          }

          // If something bad happens here (menaing we can fire the PLEvent, highly unlikely)
          // we will still ask the msg to print, but if the user "cancels" out of the 
          // print dialog the hidden print window will not be "closed"
          if (!FirePrintEvent()) 
          {
            PrintMsgWindow();
          }
        } else {
          FireStartNextEvent();
          rv = NS_OK;
        }
      } 
      else 
      {
        mWindow->Close();
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsMsgPrintEngine::OnProgressChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRInt32 aCurSelfProgress,
                                     PRInt32 aMaxSelfProgress,
                                     PRInt32 aCurTotalProgress,
                                     PRInt32 aMaxTotalProgress)
{
    return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::OnLocationChange(nsIWebProgress* aWebProgress,
                      nsIRequest* aRequest,
                      nsIURI *location)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}


NS_IMETHODIMP
nsMsgPrintEngine::OnStatusChange(nsIWebProgress* aWebProgress,
                    nsIRequest* aRequest,
                    nsresult aStatus,
                    const PRUnichar* aMessage)
{
    return NS_OK;
}


NS_IMETHODIMP
nsMsgPrintEngine::OnSecurityChange(nsIWebProgress *aWebProgress, 
                      nsIRequest *aRequest, 
                      PRUint32 state)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP    
nsMsgPrintEngine::SetWindow(nsIDOMWindowInternal *aWin)
{
	if (!aWin)
  {
    // It isn't an error to pass in null for aWin, in fact it means we are shutting
    // down and we should start cleaning things up...
		return NS_OK;
  }

  mWindow = aWin;

  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  NS_ENSURE_TRUE(globalObj, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
    do_QueryInterface(globalObj->GetDocShell());
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> rootAsItem;
  docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(rootAsItem));

  nsCOMPtr<nsIDocShellTreeNode> rootAsNode(do_QueryInterface(rootAsItem));
  NS_ENSURE_TRUE(rootAsNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> childItem;
  rootAsNode->FindChildWithName(NS_LITERAL_STRING("content").get(), PR_TRUE, PR_FALSE, nsnull,
    getter_AddRefs(childItem));

  mDocShell = do_QueryInterface(childItem);

  if(mDocShell)
    SetupObserver();

  return NS_OK;
}

/* void setParentWindow (in nsIDOMWindowInternal ptr); */
NS_IMETHODIMP nsMsgPrintEngine::SetParentWindow(nsIDOMWindowInternal *ptr)
{
  mParentWindow = ptr;
  return NS_OK;
}


NS_IMETHODIMP
nsMsgPrintEngine::ShowWindow(PRBool aShow)
{
  nsresult rv;

  NS_ENSURE_TRUE(mWindow, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr <nsIScriptGlobalObject> globalScript = do_QueryInterface(mWindow, &rv);

  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIWebShell> webShell =
    do_QueryInterface(globalScript->GetDocShell(), &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIWebShellContainer> webShellContainer;
  rv = webShell->GetContainer(*getter_AddRefs(webShellContainer));
  NS_ENSURE_SUCCESS(rv,rv);
  
  if (webShellContainer) {
    nsCOMPtr <nsIWebShellWindow> webShellWindow = do_QueryInterface(webShellContainer, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIDocShellTreeItem>  treeItem(do_QueryInterface(webShell, &rv));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    rv = treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
    NS_ENSURE_SUCCESS(rv,rv);

    // disable (enable) the window
    nsCOMPtr<nsIBaseWindow> baseWindow;
    baseWindow = do_QueryInterface(treeOwner, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = baseWindow->SetEnabled(aShow);
    NS_ENSURE_SUCCESS(rv,rv);

    // hide or show the window
    rv = webShellWindow->Show(aShow);
  }
  return rv;
}

NS_IMETHODIMP
nsMsgPrintEngine::AddPrintURI(const PRUnichar *aMsgURI)
{
  mURIArray.AppendString(nsDependentString(aMsgURI));
  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::SetPrintURICount(PRInt32 aCount)
{
  mURICount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::StartPrintOperation(nsIPrintSettings* aPS)
{
  NS_ENSURE_ARG_POINTER(aPS);
  mPrintSettings = aPS;

  // Load the about:blank on the tail end...
  nsresult rv = AddPrintURI(NS_LITERAL_STRING("about:blank").get()); 
  if (NS_FAILED(rv)) return rv; 
  return StartNextPrintOperation();
}

//----------------------------------------------------------------------
// Set up to use the "pluggable" Print Progress Dialog
nsresult
nsMsgPrintEngine::ShowProgressDialog(PRBool aIsForPrinting, PRBool& aDoNotify)
{
  nsresult rv;

  // default to not notifying, that if something here goes wrong
  // or we aren't going to show the progress dialog we can straight into 
  // reflowing the doc for printing.
  aDoNotify = PR_FALSE;

  // Assume we can't do progress and then see if we can
  PRBool showProgressDialog = PR_FALSE;

  // if it is already being shown then don't bother to find out if it should be
  // so skip this and leave mShowProgressDialog set to FALSE
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) 
  {
    prefBranch->GetBoolPref("print.show_print_progress", &showProgressDialog);
  }

  // Turning off the showing of Print Progress in Prefs overrides
  // whether the calling PS desire to have it on or off, so only check PS if 
  // prefs says it's ok to be on.
  if (showProgressDialog) 
  {
    mPrintSettings->GetShowPrintProgress(&showProgressDialog);
  }

  // Now open the service to get the progress dialog
  // If we don't get a service, that's ok, then just don't show progress
  if (showProgressDialog) {
    if (!mPrintPromptService) 
    {
      mPrintPromptService = do_GetService(kPrintingPromptService);
    }
    if (mPrintPromptService) 
    {
      nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(mParentWindow));
      if (!domWin) 
      {
        domWin = mWindow;
      }

      rv = mPrintPromptService->ShowProgress(domWin, mWebBrowserPrint, mPrintSettings, this, aIsForPrinting,
                                            getter_AddRefs(mPrintProgressListener), 
                                            getter_AddRefs(mPrintProgressParams), 
                                            &aDoNotify);
      if (NS_SUCCEEDED(rv)) {

        showProgressDialog = mPrintProgressListener != nsnull && mPrintProgressParams != nsnull;

        if (showProgressDialog) 
        {
          nsIWebProgressListener* wpl = NS_STATIC_CAST(nsIWebProgressListener*, mPrintProgressListener.get());
          NS_ASSERTION(wpl, "nsIWebProgressListener is NULL!");
          NS_ADDREF(wpl);
          PRUnichar *msg = nsnull;
          if (mIsDoingPrintPreview) {
            GetString(NS_LITERAL_STRING("LoadingMailMsgForPrintPreview").get());
          } else {
            GetString(NS_LITERAL_STRING("LoadingMailMsgForPrint").get());
          }
          if (msg) 
          {
            mPrintProgressParams->SetDocTitle(msg);
            nsCRT::free(msg);
          }
        }
      }
    }
  }
  return rv;
}


NS_IMETHODIMP
nsMsgPrintEngine::StartNextPrintOperation()
{
  nsresult      rv;

  // Only do this the first time through...
  if (mCurrentlyPrintingURI == -1)
    InitializeDisplayCharset();

  mCurrentlyPrintingURI++;

  // First, check if we are at the end of this stuff!
  if ( mCurrentlyPrintingURI >= mURIArray.Count() )
  {
    // This is the end...dum, dum, dum....my only friend...the end
    mWindow->Close();

    // Tell the user we are done...
    PRUnichar *msg = GetString(NS_LITERAL_STRING("PrintingComplete").get());
    SetStatusMessage( msg );
    CRTFREEIF(msg)
    
    return NS_OK;
  }

  if (!mDocShell)
    return StartNextPrintOperation();

  nsString *uri = mURIArray.StringAt(mCurrentlyPrintingURI);
  rv = FireThatLoadOperationStartup(uri);
  if (NS_FAILED(rv))
    return StartNextPrintOperation();
  else
    return rv;
}

NS_IMETHODIMP    
nsMsgPrintEngine::SetStatusFeedback(nsIMsgStatusFeedback *aFeedback)
{
	mFeedback = aFeedback;
	return NS_OK;
}

#define DATA_URL_PREFIX     "data:"
#define DATA_URL_PREFIX_LEN 5

#define ADDBOOK_URL_PREFIX     "addbook:"
#define ADDBOOK_URL_PREFIX_LEN 8

NS_IMETHODIMP
nsMsgPrintEngine::FireThatLoadOperationStartup(nsString *uri)
{
  if (uri) 
  {
    mLoadURI = *uri;
  } 
  else 
  {
    mLoadURI.SetLength(0);
  }

  PRBool   notify = PR_FALSE;
  nsresult rv     = NS_ERROR_FAILURE;
  // Don't show dialog if we are out of URLs
  //if ( mCurrentlyPrintingURI < mURIArray.Count() && !mIsDoingPrintPreview)
  if ( mCurrentlyPrintingURI < mURIArray.Count())
  {
    rv = ShowProgressDialog(!mIsDoingPrintPreview, notify);
  }
  if (NS_FAILED(rv) || !notify) 
  {
    return FireThatLoadOperation(uri);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgPrintEngine::FireThatLoadOperation(nsString *uri)
{
  nsresult rv = NS_OK;

  char  *tString = ToNewCString(*uri);
  if (!tString)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr <nsIMsgMessageService> messageService;

  // if this is a data: url, skip it, because 
  // we've already got something we can print
  // and we know it is not a message.
  //
  // if this an about:blank url, skip it, because
  // ...
  //
  // if this is an addbook: url, skip it, because
  // we know that isn't a message.
  //
  // if this is a message part (or .eml file on disk)
  // skip it, because we don't want to print the parent message
  // we want to print the part.
  // example:  imap://sspitzer@nsmail-1:143/fetch%3EUID%3E/INBOX%3E180958?part=1.1.2&type=x-message-display&filename=test"
  if (strncmp(tString, DATA_URL_PREFIX, DATA_URL_PREFIX_LEN) && 
      strncmp(tString, ADDBOOK_URL_PREFIX, ADDBOOK_URL_PREFIX_LEN) && 
      strcmp(tString, "about:blank") &&
      !strstr(tString, "type=x-message-display")) {
    rv = GetMessageServiceFromURI(tString, getter_AddRefs(messageService));
  }

  if (NS_SUCCEEDED(rv) && messageService)
  {
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mDocShell));
    rv = messageService->DisplayMessageForPrinting(tString, webShell, nsnull, nsnull, nsnull);
  }
  //If it's not something we know about, then just load try loading it directly.
  else
  {
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
    if (webNav)
      rv = webNav->LoadURI(uri->get(),                        // URI string
                           nsIWebNavigation::LOAD_FLAGS_NONE, // Load flags
                           nsnull,                            // Referring URI
                           nsnull,                            // Post data
                           nsnull);                           // Extra headers
  }

  if (tString) nsCRT::free(tString);
  return rv;
}

void
nsMsgPrintEngine::InitializeDisplayCharset()
{
  // libmime always converts to UTF-8 (both HTML and XML)
  if (mDocShell) 
  {
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) 
    {
      nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);
      if (muDV) 
      {
        muDV->SetForceCharacterSet(NS_LITERAL_CSTRING("UTF-8"));
      }
    }
  }
}

void
nsMsgPrintEngine::SetupObserver()
{
  if (!mDocShell)
    return;

  if (mDocShell)
  {
    nsCOMPtr<nsIWebProgress> progress(do_GetInterface(mDocShell));
    NS_ASSERTION(progress, "we were expecting a nsIWebProgress");
    if (progress) 
    {
      (void) progress->AddProgressListener((nsIWebProgressListener *)this,
                                        nsIWebProgress::NOTIFY_STATE_DOCUMENT);
    }

    // Cache a pointer to the mail message's DOMWindow 
    // so later we know when we can print when the 
    // document "loaded" msgs com thru via the Progress listener
    mMsgDOMWin = do_GetInterface(mDocShell);
  }
}

nsresult
nsMsgPrintEngine::SetStatusMessage(PRUnichar *aMsgString)
{
  if ( (!mFeedback) || (!aMsgString) )
    return NS_OK;

  mFeedback->ShowStatusString(aMsgString);
  return NS_OK;
}

#define MESSENGER_STRING_URL       "chrome://messenger/locale/messenger.properties"

PRUnichar *
nsMsgPrintEngine::GetString(const PRUnichar *aStringName)
{
	nsresult    res = NS_OK;
  PRUnichar   *ptrv = nsnull;

	if (!mStringBundle)
	{
		static const char propertyURL[] = MESSENGER_STRING_URL;

		nsCOMPtr<nsIStringBundleService> sBundleService = 
		         do_GetService(NS_STRINGBUNDLE_CONTRACTID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			res = sBundleService->CreateBundle(propertyURL, getter_AddRefs(mStringBundle));
		}
	}

	if (mStringBundle)
		res = mStringBundle->GetStringFromName(aStringName, &ptrv);

  if ( NS_SUCCEEDED(res) && (ptrv) )
    return ptrv;
  else
    return nsCRT::strdup(aStringName);
}

//-----------------------------------------------------------
void
nsMsgPrintEngine::PrintMsgWindow()
{
  const char* kMsgKeys[] = {"PrintingMessage",  "PrintPreviewMessage",
                            "PrintingCard",     "PrintPreviewCard",
                            "PrintingAddrBook", "PrintPreviewAddrBook"};

  mDocShell->GetContentViewer(getter_AddRefs(mContentViewer));  
  if (mContentViewer) 
  {
    mWebBrowserPrint = do_QueryInterface(mContentViewer);
    if (mWebBrowserPrint) 
    {
      if (!mPrintSettings) 
      {
        mWebBrowserPrint->GetGlobalPrintSettings(getter_AddRefs(mPrintSettings));
      }
      
      // fix for bug #118887 and bug #176016
      // don't show the actual url when printing mail messages or addressbook cards.
      // for mail, it can review the salt.  for addrbook, it's a data:// url, which
      // means nothing to the end user.
      // needs to be " " and not "" or nsnull, otherwise, we'll still print the url
      mPrintSettings->SetDocURL(NS_LITERAL_STRING(" ").get());

      nsresult rv = NS_ERROR_FAILURE;
      if (mIsDoingPrintPreview) 
      {
        if (mStartupPPObs) {
          rv = mStartupPPObs->Observe(nsnull, nsnull, nsnull);
        }
      } 
      else 
      {
        mPrintSettings->SetPrintSilent(mCurrentlyPrintingURI != 0);
        nsCOMPtr<nsIContentViewerFile> contentViewerFile(do_QueryInterface(mWebBrowserPrint));
        if (contentViewerFile && mParentWindow) 
        {
          rv = contentViewerFile->PrintWithParent(mParentWindow, mPrintSettings, (nsIWebProgressListener *)this);
        } 
        else 
        {
          rv = mWebBrowserPrint->Print(mPrintSettings, (nsIWebProgressListener *)this);
        }
      }

      if (NS_FAILED(rv))
      {
        mWebBrowserPrint = nsnull;
        mContentViewer = nsnull;
        PRBool isPrintingCancelled = PR_FALSE;
        if (mPrintSettings)
        {
          mPrintSettings->GetIsCancelled(&isPrintingCancelled);
        }
        if (!isPrintingCancelled) 
        {
          StartNextPrintOperation();
        } 
        else 
        {
          mWindow->Close();
        }
      }
      else
      {
        // Tell the user we started printing...
        PRUnichar *msg = GetString(NS_ConvertASCIItoUCS2(kMsgKeys[mMsgInx]).get());
        SetStatusMessage( msg );
        CRTFREEIF(msg)
      }
    }
  }
}

//---------------------------------------------------------------
//-- PLEvent Notification
//---------------------------------------------------------------
//-----------------------------------------------------------
PRBool
FireEvent(nsMsgPrintEngine* aMPE, PLHandleEventProc handler, PLDestroyEventProc destructor)
{
  static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

  nsCOMPtr<nsIEventQueueService> event_service = do_GetService(kEventQueueServiceCID);

  if (!event_service) 
  {
    NS_WARNING("Failed to get event queue service");
    return PR_FALSE;
  }

  nsCOMPtr<nsIEventQueue> event_queue;

  event_service->GetThreadEventQueue(NS_CURRENT_THREAD,
                                     getter_AddRefs(event_queue));

  if (!event_queue) 
  {
    NS_WARNING("Failed to get event queue from service");
    return PR_FALSE;
  }

  PLEvent *event = new PLEvent;

  if (!event) 
  {
    NS_WARNING("Out of memory?");
    return PR_FALSE;
  }

  PL_InitEvent(event, aMPE, handler, destructor);

  // The event owns the msgPrintEngine pointer now.
  NS_ADDREF(aMPE);

  if (NS_FAILED(event_queue->PostEvent(event)))
  {
    NS_WARNING("Failed to post event");
    PL_DestroyEvent(event);
    return PR_FALSE;
  }

  return PR_TRUE;
}

void PR_CALLBACK HandlePLEventPrintMsgWindow(PLEvent* aEvent)
{
  nsMsgPrintEngine *msgPrintEngine = (nsMsgPrintEngine*)PL_GetEventOwner(aEvent);

  NS_ASSERTION(msgPrintEngine, "The event owner is null.");
  if (msgPrintEngine) 
  {
    msgPrintEngine->PrintMsgWindow();
  }
}

//------------------------------------------------------------------------
void PR_CALLBACK DestroyPLEventPrintMsgWindow(PLEvent* aEvent)
{
  nsMsgPrintEngine *msgPrintEngine = (nsMsgPrintEngine*)PL_GetEventOwner(aEvent);
  NS_IF_RELEASE(msgPrintEngine);

  delete aEvent;
}

//-----------------------------------------------------------
PRBool
nsMsgPrintEngine::FirePrintEvent()
{
  return FireEvent(this, (PLHandleEventProc)::HandlePLEventPrintMsgWindow, 
                         (PLDestroyEventProc)::DestroyPLEventPrintMsgWindow);
}

void PR_CALLBACK HandlePLEventStartNext(PLEvent* aEvent)
{
  nsMsgPrintEngine *msgPrintEngine = (nsMsgPrintEngine*)PL_GetEventOwner(aEvent);

  NS_ASSERTION(msgPrintEngine, "The event owner is null.");
  if (msgPrintEngine) 
  {
    msgPrintEngine->StartNextPrintOperation();
  }
}

//------------------------------------------------------------------------
void PR_CALLBACK DestroyPLEventStartNext(PLEvent* aEvent)
{
  nsMsgPrintEngine *msgPrintEngine = (nsMsgPrintEngine*)PL_GetEventOwner(aEvent);
  NS_IF_RELEASE(msgPrintEngine);

  delete aEvent;
}

//-----------------------------------------------------------
PRBool
nsMsgPrintEngine::FireStartNextEvent()
{
  return FireEvent(this, (PLHandleEventProc)::HandlePLEventStartNext, 
                         (PLDestroyEventProc)::DestroyPLEventStartNext);
}

/* void setStartupPPObserver (in nsIObserver startupPPObs); */
NS_IMETHODIMP nsMsgPrintEngine::SetStartupPPObserver(nsIObserver *startupPPObs)
{
  mStartupPPObs = startupPPObs;
  return NS_OK;
}

/* attribute boolean doPrintPreview; */
NS_IMETHODIMP nsMsgPrintEngine::GetDoPrintPreview(PRBool *aDoPrintPreview)
{
  NS_ENSURE_ARG_POINTER(aDoPrintPreview);
  *aDoPrintPreview = mIsDoingPrintPreview;
  return NS_OK;
}
NS_IMETHODIMP nsMsgPrintEngine::SetDoPrintPreview(PRBool aDoPrintPreview)
{
  mIsDoingPrintPreview = aDoPrintPreview;
  return NS_OK;
}

/* readonly attribute nsIWebBrowserPrint webBrowserPrint; */
NS_IMETHODIMP nsMsgPrintEngine::GetWebBrowserPrint(nsIWebBrowserPrint * *aWebBrowserPrint)
{
  NS_ENSURE_ARG_POINTER(aWebBrowserPrint);
  *aWebBrowserPrint = nsnull;

  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
  mDocShell->GetContentViewer(getter_AddRefs(mContentViewer));  
  NS_ENSURE_TRUE(mContentViewer, NS_ERROR_FAILURE);
  mWebBrowserPrint = do_QueryInterface(mContentViewer);
  NS_ENSURE_TRUE(mWebBrowserPrint, NS_ERROR_FAILURE);

  NS_ADDREF(*aWebBrowserPrint = mWebBrowserPrint);

  return NS_OK;
}

/* void setMsgType (in long aMsgType); */
NS_IMETHODIMP nsMsgPrintEngine::SetMsgType(PRInt32 aMsgType)
{
  if (mMsgInx >= nsIMsgPrintEngine::MNAB_START && mMsgInx < nsIMsgPrintEngine::MNAB_END) 
  {
    mMsgInx = aMsgType;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*=============== nsIObserver Interface ======================*/
NS_IMETHODIMP nsMsgPrintEngine::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  return FireThatLoadOperation(&mLoadURI);
}
