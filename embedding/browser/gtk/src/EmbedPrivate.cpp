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
 */

#include <nsIDocShell.h>
#include <nsIWebProgress.h>

// for NS_APPSHELL_CID
#include <nsWidgetsCID.h>

// for do_GetInterface
#include <nsIInterfaceRequestor.h>
// for do_CreateInstance
#include <nsIComponentManager.h>

// for initializing our window watcher service
#include <nsIWindowWatcher.h>

#include <nsILocalFile.h>
#include <nsEmbedAPI.h>

// all of the crap that we need for event listeners
#include <nsIDOMWindow.h>
#include <nsPIDOMWindow.h>
#include <nsIDOMWindowInternal.h>
#include <nsIChromeEventHandler.h>

// for profiles
#include <nsMPFileLocProvider.h>

// all of our local includes
#include "EmbedPrivate.h"
#include "EmbedWindow.h"
#include "EmbedProgress.h"
#include "EmbedContentListener.h"
#include "EmbedEventListener.h"
#include "EmbedWindowCreator.h"

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

static char *sWatcherContractID = "@mozilla.org/embedcomp/window-watcher;1";

PRUint32     EmbedPrivate::sWidgetCount = 0;
char        *EmbedPrivate::sCompPath    = nsnull;
nsIAppShell *EmbedPrivate::sAppShell    = nsnull;
PRBool       EmbedPrivate::sCreatorInit = PR_FALSE;
nsVoidArray *EmbedPrivate::sWindowList  = nsnull;
char        *EmbedPrivate::sProfileDir  = nsnull;
char        *EmbedPrivate::sProfileName = nsnull;
nsIPref     *EmbedPrivate::sPrefs       = nsnull;

EmbedPrivate::EmbedPrivate(void)
{
  mOwningWidget     = nsnull;
  mWindow           = nsnull;
  mProgress         = nsnull;
  mContentListener  = nsnull;
  mEventListener    = nsnull;
  mListenersAttached = PR_FALSE;
  PushStartup();
  if (!sWindowList) {
    sWindowList = new nsVoidArray();
  }
  sWindowList->AppendElement(this);
}

EmbedPrivate::~EmbedPrivate()
{
  sWindowList->RemoveElement(this);
  DetachListeners();
  if (mEventReceiver)
    mEventReceiver = nsnull;
  PopStartup();
}

nsresult
EmbedPrivate::Init(GtkMozEmbed *aOwningWidget)
{
  // hang on with a reference to the owning widget
  mOwningWidget = aOwningWidget;

  // Create our embed window, and create an owning reference to it and
  // initialize it.
  mWindow = new EmbedWindow();
  mWindowGuard = NS_STATIC_CAST(nsIWebBrowserChrome *, mWindow);
  mWindow->Init(this);

  // Create our progress listener object, make an owning reference,
  // and initialize it.
  mProgress = new EmbedProgress();
  mProgressGuard = NS_STATIC_CAST(nsIWebProgressListener *,
				       mProgress);
  mProgress->Init(this);

  // Create our content listener object, initialize it and attach it.
  mContentListener = new EmbedContentListener();
  mContentListenerGuard = mContentListener;
  mContentListener->Init(this);

  // Create our key listener object and initialize it.
  mEventListener = new EmbedEventListener();
  mEventListenerGuard =
    NS_STATIC_CAST(nsISupports *, NS_STATIC_CAST(nsIDOMKeyListener *,
						 mEventListener));
  mEventListener->Init(this);

  // Set up our window creator ( only once )
  if (!sCreatorInit) {
    // We set this flag here instead of on success.  If it failed we
    // don't want to keep trying and leaking window creator objects.
    sCreatorInit = PR_TRUE;

    // create our local object
    EmbedWindowCreator *creator = new EmbedWindowCreator();
    nsCOMPtr<nsIWindowCreator> windowCreator;
    windowCreator = NS_STATIC_CAST(nsIWindowCreator *, creator);

    // Attach it via the watcher service
    nsCOMPtr<nsIWindowWatcher> watcher = do_GetService(sWatcherContractID);
    if (watcher)
      watcher->SetWindowCreator(windowCreator);
  }
  return NS_OK;
}

nsresult
EmbedPrivate::Realize(void)
{
  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

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
  webBrowser->AddWebBrowserListener(weakRef,
				    nsIWebProgressListener::GetIID());

  // set ourselves as the parent uri content listener
  nsCOMPtr<nsIURIContentListener> uriListener;
  uriListener = do_QueryInterface(mContentListenerGuard);
  webBrowser->SetParentURIContentListener(uriListener);


  return NS_OK;
}

void
EmbedPrivate::Unrealize(void)
{
  // XXX umm, yeah.  undo all of realize.
  mOwningWidget = 0;
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
EmbedPrivate::Resize(PRUint32 aWidth, PRUint32 aHeight)
{
  mWindow->SetPositionAndSize(0, 0, aWidth, aHeight, PR_FALSE);
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
    mNavigation->LoadURI(mURI.GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
}

/* static */
void
EmbedPrivate::PushStartup(void)
{
  // increment the number of widgets
  sWidgetCount++;
  
  // if this is the first widget, fire up xpcom
  if (sWidgetCount == 1) {
    nsresult rv;
    nsCOMPtr<nsILocalFile> binDir;
    
    if (sCompPath) {
      rv = NS_NewLocalFile(sCompPath, 1, getter_AddRefs(binDir));
      if (NS_FAILED(rv))
	return;
    }

    rv = NS_InitEmbedding(binDir, nsnull);
    if (NS_FAILED(rv))
      return;

    rv = StartupProfile();
    if (NS_FAILED(rv))
      NS_WARNING("Warning: Failed to start up profiles.\n");
    

    // XXX startup appshell service?
    // XXX create offscreen window for appshell service?
    // XXX remove X prop from offscreen window?
    
    nsCOMPtr<nsIAppShell> appShell;
    appShell = do_CreateInstance(kAppShellCID);
    if (!appShell) {
      NS_WARNING("Failed to create appshell in EmbedPrivate::PushStartup!\n");
      return;
    }
    sAppShell = appShell.get();
    NS_ADDREF(sAppShell);
    sAppShell->Create(0, nsnull);
    sAppShell->Spinup();
  }
}

/* static */
void
EmbedPrivate::PopStartup(void)
{
  sWidgetCount--;
  if (sWidgetCount == 0) {
    if (sPrefs) {
      sPrefs->ShutDown();
      NS_RELEASE(sPrefs);
      sPrefs = 0;
    }

    if (sAppShell) {
      // Shutdown the appshell service.
      sAppShell->Spindown();
      NS_RELEASE(sAppShell);
      sAppShell = 0;
    }

    // shut down XPCOM/Embedding
    NS_TermEmbedding();
  }
}

/* static */
void
EmbedPrivate::SetCompPath(char *aPath)
{
  if (sCompPath)
    free(sCompPath);
  if (aPath)
    sCompPath = strdup(aPath);
  else
    sCompPath = nsnull;
}

/* static */
void
EmbedPrivate::SetProfilePath(char *aDir, char *aName)
{
  if (sProfileDir) {
    free(sProfileDir);
    sProfileDir = nsnull;
  }

  if (sProfileName) {
    free(sProfileName);
    sProfileName = nsnull;
  }

  if (aDir)
    sProfileDir = strdup(aDir);
  
  if (aName)
    sProfileName = strdup(aName);
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
  for (int i = 0; i < count; i++) {
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
EmbedPrivate::ContentProgressChange(void)
{
  if (mListenersAttached)
    return;

  GetListener();

  if (!mEventReceiver)
    return;
  
  AttachListeners();

}

// Get the event listener for the chrome event handler.

void
EmbedPrivate::GetListener(void)
{
  if (mEventReceiver)
    return;

  // get the web browser
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // get the content DOM window for that web browser
  nsCOMPtr<nsIDOMWindow> domWindow;
  webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  if (!domWindow) {
    NS_WARNING("no dom window in content progress change\n");
    return;
  }

  // get the private DOM window
  nsCOMPtr<nsPIDOMWindow> domWindowPrivate = do_QueryInterface(domWindow);
  // and the root window for that DOM window
  nsCOMPtr<nsIDOMWindowInternal> rootWindow;
  domWindowPrivate->GetPrivateRoot(getter_AddRefs(rootWindow));
  
  nsCOMPtr<nsIChromeEventHandler> chromeHandler;
  nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));
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

/* static */
nsresult
EmbedPrivate::StartupProfile(void)
{
  // initialize profiles
  if (sProfileDir && sProfileName) {
    nsresult rv;
    nsCOMPtr<nsILocalFile> profileDir;
    PRBool exists = PR_FALSE;
    PRBool isDir = PR_FALSE;
    profileDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
    rv = profileDir->InitWithPath(sProfileDir);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    profileDir->Exists(&exists);
    profileDir->IsDirectory(&isDir);
    // if it exists and it isn't a directory then give up now.
    if (!exists) {
      rv = profileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
      if NS_FAILED(rv) {
	return NS_ERROR_FAILURE;
      }
    }
    else if (exists && !isDir) {
      return NS_ERROR_FAILURE;
    }
    // actually create the loc provider and initialize prefs.
    nsMPFileLocProvider *locProvider;
    // Set up the loc provider.  This has a really strange ownership
    // model.  When I initialize it it will register itself with the
    // directory service.  The directory service becomes the owning
    // reference.  So, when that service is shut down this object will
    // be destroyed.  It's not leaking here.
    locProvider = new nsMPFileLocProvider;
    rv = locProvider->Initialize(profileDir, sProfileName);

    // get prefs
    nsCOMPtr<nsIPref> pref;
    pref = do_GetService(NS_PREF_CONTRACTID);
    if (!pref)
      return NS_ERROR_FAILURE;
    sPrefs = pref.get();
    NS_ADDREF(sPrefs);
    sPrefs->ResetPrefs();
    sPrefs->ReadUserPrefs();
  }
  return NS_OK;
}
