/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Brian Edmond <briane@qnx.com>
 */

#include <nsIDocShell.h>
#include <nsIURI.h>
#include <nsIWebProgress.h>
#include <nsIURIFixup.h>
#include "nsReadableUtils.h"
#include "nsIWidget.h"

// for NS_APPSHELL_CID
#include <nsWidgetsCID.h>

// for do_GetInterface
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
// for do_CreateInstance
#include <nsIComponentManager.h>

#include <nsIPrintSettings.h>
#include "nsPrintSettingsImpl.h"

// for initializing our window watcher service
#include <nsIWindowWatcher.h>

#include <nsILocalFile.h>
#include <nsEmbedAPI.h>
#include <nsString.h>

// all of the crap that we need for event listeners
// and when chrome windows finish loading
#include <nsIDOMWindow.h>
#include <nsPIDOMWindow.h>
#include <nsIDOMWindowInternal.h>
#include <nsIChromeEventHandler.h>
#include <nsIContentViewer.h>
#include <nsIContentViewerEdit.h>
#include <nsIWebBrowserSetup.h>
#include "nsIWebBrowserPrint.h"
#include "nsIClipboardCommands.h"
#include "docshell/nsCDefaultURIFixup.h"
#include "nsGfxCIID.h"

// for the focus hacking we need to do
#include <nsIFocusController.h>

#include "nsIWebBrowserPrint.h"

// all of our local includes
#include "EmbedPrivate.h"
#include "EmbedWindow.h"
#include "EmbedProgress.h"
#include "EmbedContentListener.h"
#include "EmbedEventListener.h"
#include "EmbedWindowCreator.h"
#include "EmbedStream.h"
#include "EmbedPrintListener.h"

#include "PtMozilla.h"

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

nsIAppShell *EmbedPrivate::sAppShell    = nsnull;
nsIPref     *EmbedPrivate::sPrefs       = nsnull;
nsVoidArray *EmbedPrivate::sWindowList  = nsnull;

EmbedPrivate::EmbedPrivate(void)
{
  mOwningWidget     = nsnull;
  mWindow           = nsnull;
  mProgress         = nsnull;
  mContentListener  = nsnull;
  mEventListener    = nsnull;
  mStream           = nsnull;
  mChromeMask       = 0;
  mIsChrome         = PR_FALSE;
  mChromeLoaded     = PR_FALSE;
  mListenersAttached = PR_FALSE;
  mMozWindowWidget  = 0;

	if (!sWindowList) {
  		sWindowList = new nsVoidArray();
	}
	sWindowList->AppendElement(this);
}

EmbedPrivate::~EmbedPrivate()
{
}

nsresult
EmbedPrivate::Init(PtWidget_t *aOwningWidget)
{
	// are we being re-initialized?
	if (mOwningWidget)
		return NS_OK;

	// hang on with a reference to the owning widget
	mOwningWidget = aOwningWidget;

	// Create our embed window, and create an owning reference to it and
	// initialize it.  It is assumed that this window will be destroyed
	// when we go out of scope.
	mWindow = new EmbedWindow();
	mWindowGuard = NS_STATIC_CAST(nsIWebBrowserChrome *, mWindow);
	mWindow->Init(this);

	// Create our progress listener object, make an owning reference,
	// and initialize it.  It is assumed that this progress listener
	// will be destroyed when we go out of scope.
	mProgress = new EmbedProgress();
	mProgressGuard = NS_STATIC_CAST(nsIWebProgressListener *,
					   mProgress);
	mProgress->Init(this);

	// Create our content listener object, initialize it and attach it.
	// It is assumed that this will be destroyed when we go out of
	// scope.
	mContentListener = new EmbedContentListener();
	mContentListenerGuard = mContentListener;
	mContentListener->Init(this);

	// Create our key listener object and initialize it.  It is assumed
	// that this will be destroyed before we go out of scope.
	mEventListener = new EmbedEventListener();
	mEventListenerGuard =
	NS_STATIC_CAST(nsISupports *, NS_STATIC_CAST(nsIDOMKeyListener *,
						 mEventListener));
	mEventListener->Init(this);

	// Create our print listener object, make an owning reference,
	// and initialize it.  It is assumed that this print listener
	// will be destroyed when we go out of scope.
	mPrint = new EmbedPrintListener();
	mPrintGuard = NS_STATIC_CAST(nsIWebProgressListener *, mPrint);
	mPrint->Init(this);

	// has the window creator service been set up?
	static int initialized = PR_FALSE;
	// Set up our window creator ( only once )
	if (!initialized) 
	{
		// We set this flag here instead of on success.  If it failed we
		// don't want to keep trying and leaking window creator objects.
		initialized = PR_TRUE;

		// create our local object
		EmbedWindowCreator *creator = new EmbedWindowCreator();
		nsCOMPtr<nsIWindowCreator> windowCreator;
		windowCreator = NS_STATIC_CAST(nsIWindowCreator *, creator);

		// Attach it via the watcher service
		nsCOMPtr<nsIWindowWatcher> watcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
		if (watcher)
      		watcher->SetWindowCreator(windowCreator);
  }

	if (!sPrefs)
	{
		// get prefs
		nsresult rv;
		nsCOMPtr<nsIPref> pref(do_GetService(NS_PREF_CONTRACTID, &rv));
		if (pref)
		{
			sPrefs = pref.get();
			NS_ADDREF(sPrefs);
			//sPrefs->ResetPrefs();
			sPrefs->ReadUserPrefs(nsnull);
		}
	}

  return NS_OK;
}

nsIPref *
EmbedPrivate::GetPrefs()
{
	return (sPrefs);
}

nsresult
EmbedPrivate::Setup()
{
	// Get the nsIWebBrowser object for our embedded window.
	nsCOMPtr<nsIWebBrowser> webBrowser;
	mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

	// Configure what the web browser can and cannot do
	PRBool aAllowPlugins = PR_TRUE;
	nsCOMPtr<nsIWebBrowserSetup> webBrowserAsSetup(do_QueryInterface(webBrowser));
	webBrowserAsSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS, aAllowPlugins);

	// get a handle on the navigation object
	mNavigation = do_QueryInterface(webBrowser);

	mFixup = do_GetService(NS_URIFIXUP_CONTRACTID);

	// Create our session history object and tell the navigation object
	// to use it.  We need to do this before we create the web browser
	// window.
	mSessionHistory = do_CreateInstance(NS_SHISTORY_CONTRACTID);
	mNavigation->SetSessionHistory(mSessionHistory);

	// create the window
	mWindow->CreateWindow();

	// bind the progress listener to the browser object
	nsCOMPtr<nsISupportsWeakReference> supportsWeak;
	supportsWeak = do_QueryInterface(mProgressGuard);
	nsCOMPtr<nsIWeakReference> weakRef;
	supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
	webBrowser->AddWebBrowserListener(weakRef, nsIWebProgressListener::GetIID());

	// set ourselves as the parent uri content listener
	nsCOMPtr<nsIURIContentListener> uriListener;
	uriListener = do_QueryInterface(mContentListenerGuard);
	webBrowser->SetParentURIContentListener(uriListener);

	nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(webBrowser));
	if (print)
		print->GetGlobalPrintSettings(getter_AddRefs(m_PrintSettings));

	return NS_OK;
}

void
EmbedPrivate::Show(void)
{
  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // and set the visibility on the thing
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(webBrowser);
  baseWindow->SetVisibility(PR_TRUE);
}

void
EmbedPrivate::Hide(void)
{
  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // and set the visibility on the thing
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(webBrowser);
  baseWindow->SetVisibility(PR_FALSE);
}

void
EmbedPrivate::Position(PRUint32 aX, PRUint32 aY)
{
	mWindow->mBaseWindow->SetPosition(aX, aY);
}

void
EmbedPrivate::Size(PRUint32 aWidth, PRUint32 aHeight)
{
	mWindow->mBaseWindow->SetSize(aWidth, aHeight, PR_TRUE);
}



void
EmbedPrivate::Destroy(void)
{
  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // Release our progress listener
  nsCOMPtr<nsISupportsWeakReference> supportsWeak;
  supportsWeak = do_QueryInterface(mProgressGuard);
  nsCOMPtr<nsIWeakReference> weakRef;
  supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
  webBrowser->RemoveWebBrowserListener(weakRef,
				       nsIWebProgressListener::GetIID());
  weakRef = nsnull;
  supportsWeak = nsnull;

  // Release our content listener
  webBrowser->SetParentURIContentListener(nsnull);
  mContentListenerGuard = nsnull;
  mContentListener = nsnull;

  // Now that we have removed the listener, release our progress
  // object
  mProgressGuard = nsnull;
  mProgress = nsnull;

  // detach our event listeners and release the event receiver
  DetachListeners();
  if (mEventReceiver)
    mEventReceiver = nsnull;

	// remove this from the window list
  sWindowList->RemoveElement(this);
  
  // destroy our child window
  mWindow->ReleaseChildren();

  // release navigation
  mNavigation = nsnull;
  mFixup = nsnull;

  //m_PrintSettings = nsnull;

  // release session history
  mSessionHistory = nsnull;

  mOwningWidget = nsnull;

  mMozWindowWidget = 0;
}

void
EmbedPrivate::SetURI(const char *aURI)
{
  mURI.AssignWithConversion(aURI);
}

void
EmbedPrivate::LoadCurrentURI(void)
{
  if (mURI.Length())
	mNavigation->LoadURI(mURI.get(),                        // URI string
                     nsIWebNavigation::LOAD_FLAGS_NONE, // Load flags
                     nsnull,                            // Refering URI
                     nsnull,                            // Post data
                     nsnull);                           // extra headers
}

void
EmbedPrivate::Stop(void)
{
	if (mNavigation)
		mNavigation->Stop(nsIWebNavigation::STOP_ALL);
}

void
EmbedPrivate::Reload(int32_t flags)
{
	PRUint32 reloadFlags = 0;

	// map the external API to the internal web navigation API.
	switch (flags) 
	{
		case MOZ_EMBED_FLAG_RELOADNORMAL:
		  	reloadFlags = 0;
		  	break;
		case MOZ_EMBED_FLAG_RELOADBYPASSCACHE:
		  	reloadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;
		  	break;
		case MOZ_EMBED_FLAG_RELOADBYPASSPROXY:
		  	reloadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
		  	break;
		case MOZ_EMBED_FLAG_RELOADBYPASSPROXYANDCACHE:
		  	reloadFlags = (nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY |
				 nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE);
		  	break;
		case MOZ_EMBED_FLAG_RELOADCHARSETCHANGE:
		  	reloadFlags = nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE;
		  	break;
		default:
		  	reloadFlags = 0;
		  	break;
	}

	if (mNavigation)
  		mNavigation->Reload(reloadFlags);
}

void
EmbedPrivate::Back(void)
{
	if (mNavigation)
		mNavigation->GoBack();
}

void
EmbedPrivate::Forward(void)
{
	if (mNavigation)
		mNavigation->GoForward();
}

void
EmbedPrivate::ScrollUp(int amount)
{
  	nsCOMPtr<nsIWebBrowser> webBrowser;
  	mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

	nsCOMPtr<nsIDOMWindow> oDomWindow;
	nsresult rv = webBrowser->GetContentDOMWindow(getter_AddRefs(oDomWindow));

	if (oDomWindow)
		rv = oDomWindow->ScrollBy(0, -amount);
}
void
EmbedPrivate::ScrollDown(int amount)
{
  	nsCOMPtr<nsIWebBrowser> webBrowser;
  	mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

	nsCOMPtr<nsIDOMWindow> oDomWindow;
	nsresult rv = webBrowser->GetContentDOMWindow(getter_AddRefs(oDomWindow));

	if (oDomWindow)
		rv = oDomWindow->ScrollBy(0, amount);
}
void
EmbedPrivate::ScrollLeft(int amount)
{
  	nsCOMPtr<nsIWebBrowser> webBrowser;
  	mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

	nsCOMPtr<nsIDOMWindow> oDomWindow;
	nsresult rv = webBrowser->GetContentDOMWindow(getter_AddRefs(oDomWindow));

	if (oDomWindow)
		rv = oDomWindow->ScrollBy(-amount, 0);
}
void
EmbedPrivate::ScrollRight(int amount)
{
  	nsCOMPtr<nsIWebBrowser> webBrowser;
  	mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

	nsCOMPtr<nsIDOMWindow> oDomWindow;
	nsresult rv = webBrowser->GetContentDOMWindow(getter_AddRefs(oDomWindow));

	if (oDomWindow)
		rv = oDomWindow->ScrollBy(amount, 0);
}






PRBool
EmbedPrivate::CanGoBack()
{
	PRBool nsresult = PR_FALSE;

	if (mNavigation)
    	mNavigation->GetCanGoBack(&nsresult);

	return (nsresult);
}

PRBool
EmbedPrivate::CanGoForward()
{
	PRBool nsresult = PR_FALSE;

	if (mNavigation)
    	mNavigation->GetCanGoForward(&nsresult);

	return (nsresult);
}

void
EmbedPrivate::Cut()
{
	nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWindow->mWebBrowser));
	if (clipboard)
	    clipboard->CutSelection();
}

void
EmbedPrivate::Copy()
{
	nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWindow->mWebBrowser));
	if (clipboard)
	    clipboard->CopySelection();
}

void
EmbedPrivate::Paste()
{
	nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWindow->mWebBrowser));
	if (clipboard)
	    clipboard->Paste();
}

void
EmbedPrivate::SelectAll()
{
	nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWindow->mWebBrowser));
	if (clipboard)
	    clipboard->SelectAll();
}

void
EmbedPrivate::Clear()
{
	nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWindow->mWebBrowser));
	if (clipboard)
	    clipboard->SelectNone();
}

void
EmbedPrivate::Print(PpPrintContext_t *pc)
{
  nsCOMPtr<nsIDOMWindow> window;
  mWindow->mWebBrowser->GetContentDOMWindow(getter_AddRefs(window));
  nsCOMPtr<nsIWebBrowserPrint> print( do_GetInterface( mWindow->mWebBrowser ) );

  m_PrintSettings->SetEndPageRange((PRInt32) pc);
  print->Print(m_PrintSettings, mPrint);
}

nsresult
EmbedPrivate::OpenStream(const char *aBaseURI, const char *aContentType)
{
  nsresult rv;

  if (!mStream) {
    mStream = new EmbedStream();
    mStreamGuard = do_QueryInterface(mStream);
    mStream->InitOwner(this);
    rv = mStream->Init();
    if (NS_FAILED(rv))
      return rv;
  }

  rv = mStream->OpenStream(aBaseURI, aContentType);
  return rv;
}

int
EmbedPrivate::SaveAs(char *fname)
{	
	if (mWindow)
		return (mWindow->SaveAs(fname));
	return (1);
}

int
EmbedPrivate::SaveURI(char *aURI, char *fname)
{	
	if (mWindow && mFixup)
	{
		nsIURI* uri;

		nsCString   u(aURI);
		nsresult rv = mFixup->CreateFixupURI(ToNewUnicode(u), 0, &(uri));
		return (mWindow->SaveURI(uri, fname));
	}
	return (1);
}

void
EmbedPrivate::CancelSaveURI()
{	
	if (mWindow)
		mWindow->CancelSaveURI();
}

nsresult
EmbedPrivate::AppendToStream(const char *aData, PRInt32 aLen)
{
  if (!mStream)
    return NS_ERROR_FAILURE;

  // Attach listeners to this document since in some cases we don't
  // get updates for content added this way.
  ContentStateChange();

  return mStream->AppendToStream(aData, aLen);
}

nsresult
EmbedPrivate::CloseStream(void)
{
  nsresult rv;

  if (!mStream)
    return NS_ERROR_FAILURE;
  rv = mStream->CloseStream();

  // release
  mStream = 0;
  mStreamGuard = 0;

  return rv;
}

/* static */
EmbedPrivate *
EmbedPrivate::FindPrivateForBrowser(nsIWebBrowserChrome *aBrowser)
{
	if (!sWindowList)
	  return nsnull;

	// Get the number of browser windows.
	PRInt32 count = sWindowList->Count();
	// This function doesn't get called very often at all ( only when
	// creating a new window ) so it's OK to walk the list of open
	// windows.
	for (int i = 0; i < count; i++) 
	{
	  EmbedPrivate *tmpPrivate = NS_STATIC_CAST(EmbedPrivate *,
							sWindowList->ElementAt(i));
	  // get the browser object for that window
	  nsIWebBrowserChrome *chrome = NS_STATIC_CAST(nsIWebBrowserChrome *,
						   tmpPrivate->mWindow);
	  if (chrome == aBrowser)
		return tmpPrivate;
	}

  return nsnull;
}

void
EmbedPrivate::ContentStateChange(void)
{

  // we don't attach listeners to chrome
  if (mListenersAttached && !mIsChrome)
    return;

  GetListener();

  if (!mEventReceiver)
    return;
  
  AttachListeners();

}

void
EmbedPrivate::ContentFinishedLoading(void)
{
  if (mIsChrome) {
    // We're done loading.
    mChromeLoaded = PR_TRUE;

    // get the web browser
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    
    // get the content DOM window for that web browser
    nsCOMPtr<nsIDOMWindow> domWindow;
    webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (!domWindow) {
      NS_WARNING("no dom window in content finished loading\n");
      return;
    }
    
    // resize the content
    domWindow->SizeToContent();

    // and since we're done loading show the window, assuming that the
    // visibility flag has been set.
    PRBool visibility;
    mWindow->GetVisibility(&visibility);
    if (visibility)
      mWindow->SetVisibility(PR_TRUE);
  }
}

// handle focus in and focus out events
void
EmbedPrivate::TopLevelFocusIn(void)
{
  nsCOMPtr<nsPIDOMWindow> piWin;
  GetPIDOMWindow(getter_AddRefs(piWin));

  if (!piWin)
    return;

  nsCOMPtr<nsIFocusController> focusController;
  piWin->GetRootFocusController(getter_AddRefs(focusController));
  if (focusController)
    focusController->SetActive(PR_TRUE);
}

void
EmbedPrivate::TopLevelFocusOut(void)
{
  nsCOMPtr<nsPIDOMWindow> piWin;
  GetPIDOMWindow(getter_AddRefs(piWin));

  if (!piWin)
    return;

  nsCOMPtr<nsIFocusController> focusController;
  piWin->GetRootFocusController(getter_AddRefs(focusController));
  if (focusController)
    focusController->SetActive(PR_FALSE);
}

void
EmbedPrivate::ChildFocusIn(void)
{
  nsCOMPtr<nsPIDOMWindow> piWin;
  GetPIDOMWindow(getter_AddRefs(piWin));

  if (!piWin)
    return;

  piWin->Activate();
}

void
EmbedPrivate::ChildFocusOut(void)
{
  nsCOMPtr<nsPIDOMWindow> piWin;
  GetPIDOMWindow(getter_AddRefs(piWin));

  if (!piWin)
    return;

  piWin->Deactivate();

  // but the window is still active until the toplevel gets a focus
  // out
  nsCOMPtr<nsIFocusController> focusController;
  piWin->GetRootFocusController(getter_AddRefs(focusController));
  if (focusController)
    focusController->SetActive(PR_TRUE);

}

// Get the event listener for the chrome event handler.

void
EmbedPrivate::GetListener(void)
{
  if (mEventReceiver)
    return;

  nsCOMPtr<nsPIDOMWindow> piWin;
  GetPIDOMWindow(getter_AddRefs(piWin));

  if (!piWin)
    return;

  nsCOMPtr<nsIChromeEventHandler> chromeHandler;
  piWin->GetChromeEventHandler(getter_AddRefs(chromeHandler));

  mEventReceiver = do_QueryInterface(chromeHandler);
}

// attach key and mouse event listeners

void
EmbedPrivate::AttachListeners(void)
{
  if (!mEventReceiver || mListenersAttached)
    return;

  nsIDOMEventListener *eventListener =
    NS_STATIC_CAST(nsIDOMEventListener *,
		   NS_STATIC_CAST(nsIDOMKeyListener *, mEventListener));

  // add the key listener
  nsresult rv;
  rv = mEventReceiver->AddEventListenerByIID(eventListener,
					     NS_GET_IID(nsIDOMKeyListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add key listener\n");
    return;
  }

  rv = mEventReceiver->AddEventListenerByIID(eventListener,
					     NS_GET_IID(nsIDOMMouseListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add mouse listener\n");
    return;
  }

  // ok, all set.
  mListenersAttached = PR_TRUE;
}

void
EmbedPrivate::DetachListeners(void)
{
  if (!mListenersAttached || !mEventReceiver)
    return;

  nsIDOMEventListener *eventListener =
    NS_STATIC_CAST(nsIDOMEventListener *,
		   NS_STATIC_CAST(nsIDOMKeyListener *, mEventListener));

  nsresult rv;
  rv = mEventReceiver->RemoveEventListenerByIID(eventListener,
						NS_GET_IID(nsIDOMKeyListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove key listener\n");
    return;
  }

  rv =
    mEventReceiver->RemoveEventListenerByIID(eventListener,
					     NS_GET_IID(nsIDOMMouseListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove mouse listener\n");
    return;
  }


  mListenersAttached = PR_FALSE;
}

nsresult
EmbedPrivate::GetPIDOMWindow(nsPIDOMWindow **aPIWin)
{
  *aPIWin = nsnull;

  // get the web browser
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // get the content DOM window for that web browser
  nsCOMPtr<nsIDOMWindow> domWindow;
  webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  if (!domWindow)
    return NS_ERROR_FAILURE;

  // get the private DOM window
  nsCOMPtr<nsPIDOMWindow> domWindowPrivate = do_QueryInterface(domWindow);
  // and the root window for that DOM window
  nsCOMPtr<nsIDOMWindowInternal> rootWindow;
  domWindowPrivate->GetPrivateRoot(getter_AddRefs(rootWindow));
  
  nsCOMPtr<nsIChromeEventHandler> chromeHandler;
  nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));

  *aPIWin = piWin.get();

  if (*aPIWin) {
    NS_ADDREF(*aPIWin);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;

}

