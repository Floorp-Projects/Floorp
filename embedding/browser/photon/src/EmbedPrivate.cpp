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
#include <nsIDOMDocument.h>
#include <nsIDOMNodeList.h>
#include <nsISelection.h>
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsIWidget.h"

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

// for the focus hacking we need to do
#include <nsIFocusController.h>

#include "nsIWebBrowserPrint.h"
#include "nsIPrintOptions.h"

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

/* globals */
extern char *g_Print_Left_Header_String, *g_Print_Right_Header_String, *g_Print_Left_Footer_String, *g_Print_Right_Footer_String;

static const char sWatcherContractID[] = "@mozilla.org/embedcomp/window-watcher;1";

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


static void mozilla_set_default_pref( nsIPref *pref );

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
		nsCOMPtr<nsIWindowWatcher> watcher = do_GetService(sWatcherContractID);
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
			NS_ADDREF( sPrefs );
			extern int sProfileDirCreated;
			if( sProfileDirCreated ) mozilla_set_default_pref( pref );
			sPrefs->ReadUserPrefs( nsnull );
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
//	if (print)
//		print->GetNewPrintSettings(getter_AddRefs(m_PrintSettings));

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

#include <nsIWebShell.h>
void
EmbedPrivate::SelectAll()
{
/*
	nsCOMPtr<nsIClipboardCommands> clipboard(do_GetInterface(mWindow->mWebBrowser));
	if (clipboard)
	    clipboard->SelectAll();
*/

	nsCOMPtr<nsIDOMWindow> domWindow;
	mWindow->mWebBrowser->GetContentDOMWindow( getter_AddRefs( domWindow ) );
	if( !domWindow ) {
		NS_WARNING( "no dom window in content finished loading\n" );
		return;
		}

	nsCOMPtr<nsIDOMDocument> domDocument;
	domWindow->GetDocument( getter_AddRefs( domDocument ) );
	if( !domDocument ) {
		NS_WARNING( "no dom document\n" );
		return;
		}

	nsCOMPtr<nsIDOMNodeList> list;
	domDocument->GetElementsByTagName( NS_LITERAL_STRING( "body" ), getter_AddRefs( list ) );
	if( !list ) {
		NS_WARNING( "no list\n" );
		return;
		}

	nsCOMPtr<nsIDOMNode> node;
	list->Item( 0, getter_AddRefs( node ) );
	if( !node ) {
		NS_WARNING( "no node\n" );
		return;
		}

	nsCOMPtr<nsISelection> selection;
	domWindow->GetSelection( getter_AddRefs( selection ) );
	if( !selection ) {
		NS_WARNING( "no selection\n" );
		return;
		}

	selection->SelectAllChildren( node );

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
    nsCOMPtr<nsIWebBrowserPrint> browserAsPrint = do_GetInterface(mWindow->mWebBrowser);
    NS_ASSERTION(browserAsPrint, "No nsIWebBrowserPrint!");

    nsCOMPtr<nsIPrintSettings> printSettings;
    browserAsPrint->GetGlobalPrintSettings(getter_AddRefs(printSettings));
    if (printSettings) 
    {
    printSettings->SetPrintSilent(PR_TRUE);
		printSettings->SetEndPageRange((PRInt32) pc);

		nsAutoString format_left_footer;
		PrintHeaderFooter_FormatSpecialCodes( g_Print_Left_Footer_String, format_left_footer );
		nsAutoString format_right_footer;
		PrintHeaderFooter_FormatSpecialCodes( g_Print_Right_Footer_String, format_right_footer );
		nsAutoString format_left_header;
		PrintHeaderFooter_FormatSpecialCodes( g_Print_Left_Header_String, format_left_header );
		nsAutoString format_right_header;
		PrintHeaderFooter_FormatSpecialCodes( g_Print_Right_Header_String, format_right_header );

		printSettings->SetFooterStrLeft( format_left_footer.get() );
		printSettings->SetFooterStrRight( format_right_footer.get() );
		printSettings->SetHeaderStrLeft( format_left_header.get() );
		printSettings->SetHeaderStrRight( format_right_header.get() );
    }

		nsIPref *pref = GetPrefs();
		pref->SetBoolPref( "print.show_print_progress", PR_FALSE );

    browserAsPrint->Print(printSettings, mPrint);
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
EmbedPrivate::SaveAs(char *fname, char *dirname)
{	
	if (mWindow)
		return (mWindow->SaveAs(fname, dirname));
	return (1);
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



#if 0

/*
	not used - we handle focus in/out activate/deactivate in
	child_getting_focus/child_losing_focus methods
	of the PtMozilla widget class
*/


// handle focus in and focus out events
void
EmbedPrivate::TopLevelFocusIn(void)
{
  nsCOMPtr<nsPIDOMWindow> piWin;
  GetPIDOMWindow(getter_AddRefs(piWin));

  if (!piWin)
    return;

  nsIFocusController *focusController = piWin->GetRootFocusController();
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

  nsIFocusController *focusController = piWin->GetRootFocusController();
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
  nsIFocusController *focusController = piWin->GetRootFocusController();
  if (focusController)
    focusController->SetActive(PR_TRUE);

}

#endif


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

  mEventReceiver = do_QueryInterface(piWin->GetChromeEventHandler());
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
	*aPIWin = domWindowPrivate->GetPrivateRoot();
  
  if (*aPIWin) {
    NS_ADDREF(*aPIWin);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;

}


static void mozilla_set_default_pref( nsIPref *pref )
{
/*
	PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) widget;
	nsIPref *pref = moz->EmbedRef->GetPrefs();
*/

	/* translation set = Western European (ISO 8859-1) */
	pref->SetUnicharPref( "intl.charset.default", NS_ConvertASCIItoUCS2("iso8859-1").get());

	/* HTML Options */
	pref->SetUnicharPref( "browser.visited_color", NS_ConvertASCIItoUCS2("#008080").get() );
	pref->SetUnicharPref( "browser.anchor_color", NS_ConvertASCIItoUCS2("#0000ff").get() );
	pref->SetUnicharPref( "browser.display.foreground_color", NS_ConvertASCIItoUCS2("#000000").get() );
	pref->SetUnicharPref( "browser.display.background_color", NS_ConvertASCIItoUCS2("#ffffff").get() );

	pref->SetCharPref( "font.name.serif.x-western", "serif" );
	pref->SetCharPref( "font.name.sans-serif.x-western", "sans-serif" );
	pref->SetCharPref( "font.name.monospace.x-western", "monospace" );
	pref->SetCharPref( "font.name.cursive.x-western", "cursive" );
	pref->SetCharPref( "font.name.fantasy.x-western", "fantasy" );

	pref->SetBoolPref( "browser.display.use_document_colors", PR_TRUE );
	pref->SetBoolPref( "browser.underline_anchors", PR_TRUE );
	pref->SetIntPref( "font.size.variable.x-western", 14 );
	pref->SetIntPref( "font.size.fixed.x-western", 12 );
	pref->SetIntPref( "browser.history_expire_days", 4 );
	pref->SetIntPref( "browser.sessionhistory.max_entries", 50 );
//	pref->SetIntPref( "browser.cache.check_doc_frequency", 2 );
	pref->SetBoolPref( "browser.cache.disk.enable", PR_TRUE );
	pref->SetIntPref( "browser.cache.disk.capacity", 5000 );
	pref->SetIntPref( "network.http.connect.timeout", 2400 );
	pref->SetIntPref( "network.http.max-connections", 4 );
	pref->SetIntPref( "network.proxy.http_port", 80 );
	pref->SetIntPref( "network.proxy.ftp_port", 80 );
	pref->SetIntPref( "network.proxy.gopher_port", 80 );

	pref->SetCharPref( "general.skins.selectedSkin", "classic/1.0" );
	pref->SetIntPref( "browser.cache.memory.capacity", 100 ); /* 100k ( no cache ) */
	pref->SetCharPref( "user.print.print_frame", "print_frame_selected" );

	pref->SetCharPref( "print.print_headercenter", "" );
	pref->SetCharPref( "print.print_footercenter", "" );

	pref->SavePrefFile( nsnull );
}


//------------------------------------------------------------------------------
#define FORMAT_ESCAPE_CHARACTER                '&'
void EmbedPrivate::PrintHeaderFooter_FormatSpecialCodes(const char *original, nsString& aNewStr)
{
	/* Think of this as a sprintf-variant. */

	const char *szPattern = original;

	time_t aclock;
	struct tm *tm;

	char workBuffer[20], *sz;

	nsAutoString result;

	while ( *szPattern )
	{
		if (*szPattern != FORMAT_ESCAPE_CHARACTER)
		{
			workBuffer[0] = *szPattern;
			szPattern++;
			workBuffer[1] = 0;

			nsAutoString ss;
			ss.AssignWithConversion( workBuffer );
			result += ss;
		}
		else
		{
			szPattern++;				/* skip over '&' */
			switch (*szPattern)
			{
			case 'w':					/* window name */
			case 'W':
				szPattern++;			/* advance past "&w" */

				/* add the title */
				PRUnichar *uTitle;
				mWindow->GetTitle( &uTitle );
				result += uTitle;
				break;


			case 'u':					/* URL */
			case 'U':					/* TODO should this be ifdef'd for kiosk's */
				szPattern++;			/* advance past "&w" */

				/* add the URL */
				result += mURI.get();
				break;


			case 'd':		/* date -- american style "mmm dd yyyy" */
			case 'D':		/* date -- european style "dd mmm yyyy" */
				szPattern++;
				
				(void) time(&aclock);
				tm = localtime(&aclock);
				sz = asctime(tm);

				/*  ..........1.........2.... .   */
				/*  012345678901234567890123. .   */
				/* "Fri Oct 22 09:15:00 1993\n\0" */

				if (szPattern[1] == 'd')
				{
					workBuffer[0] = sz[4];	/* O */
					workBuffer[1] = sz[5];	/* c */
					workBuffer[2] = sz[6];	/* t */
					workBuffer[3] = sz[7];	/* _ */
					workBuffer[4] = sz[8];	/* 2 */
					workBuffer[5] = sz[9];	/* 2 */
				}
				else
				{
					workBuffer[0] = sz[8];	/* 2 */
					workBuffer[1] = sz[9];	/* 2 */
					workBuffer[2] = sz[7];	/* _ */
					workBuffer[3] = sz[4];	/* O */
					workBuffer[4] = sz[5];	/* c */
					workBuffer[5] = sz[6];	/* t */
				}
				workBuffer[6] = sz[10];	/* _ */
				workBuffer[7] = sz[20];	/* 1 */
				workBuffer[8] = sz[21];	/* 9 */
				workBuffer[9] = sz[22];	/* 9 */
				workBuffer[10] = sz[23];	/* 3 */
				workBuffer[11] = 0;

				/* add the content of workBuffer */
				{
				nsAutoString ss;
				ss.AssignWithConversion( workBuffer );
				result += ss;
				}

				break;


			case 't':					/* time "HH:MM am" (12 hour format) */
				szPattern++;

				(void) time(&aclock);
				tm = localtime(&aclock);

				strftime(workBuffer, sizeof(workBuffer), "%I:%M %p", tm);

				{
				/* add the content of workBuffer */
				nsAutoString ss;
				ss.AssignWithConversion( workBuffer );
				result += ss;
				}

				break;

				
			case 'T':					/* time "HH:MM" (24 hour format) */
				szPattern++;

				(void) time(&aclock);
				tm = localtime(&aclock);

				strftime(workBuffer, sizeof(workBuffer), "%H:%M", tm);

				/* add the content of workBuffer */
				{
				nsAutoString ss;
				ss.AssignWithConversion( workBuffer );
				result += ss;
				}
				break;

			case 'p':					/* current page number */
				szPattern++;

				{
				/* add the page number */
				const PRUnichar * uStr = NS_LITERAL_STRING( "&P" ).get();
				result += uStr;
				}
				break;

			case 'P': /* current of total number of pages */
				/* add the page number */
				{
				const PRUnichar * uStr = NS_LITERAL_STRING( "&PT" ).get();
				result += uStr;
				}
				break;

			case FORMAT_ESCAPE_CHARACTER:	/* && expands to a single & */

				workBuffer[0] = *szPattern;
				szPattern++;
				workBuffer[1] = 0;
				{
				nsAutoString ss;
				ss.AssignWithConversion( workBuffer );
				result += ss;
				}
				break;

			case '\0':					/* copy '&' to output */

				workBuffer[0] = FORMAT_ESCAPE_CHARACTER;
				workBuffer[1] = 0;
				{
				nsAutoString ss;
				ss.AssignWithConversion( workBuffer );
				result += ss;
				}
				break;
				
			default:					/* copy '&*' to output */
#if 0
				SM_STRNCPY(p,(const char *) &szPattern[-1],lenCopy);
#endif
				szPattern++;

				/* add the &szPattern[-1] */
				{
				nsAutoString ss;
				ss.AssignWithConversion( &szPattern[-1] );
				result += ss;
				}
				break;
			}
		}
	}

	aNewStr.Assign( result );
}
