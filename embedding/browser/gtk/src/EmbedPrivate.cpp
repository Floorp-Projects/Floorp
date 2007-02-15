/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
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
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIDocShell.h"
#include "nsIWebProgress.h"
#include "nsIWidget.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsIWebBrowserStream.h"
#include "nsIWebBrowserFocus.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"

// for do_GetInterface
#include "nsIInterfaceRequestor.h"
// for do_CreateInstance
#include "nsIComponentManager.h"

// for initializing our window watcher service
#include "nsIWindowWatcher.h"

#include "nsILocalFile.h"

#include "nsXULAppAPI.h"

// all of the crap that we need for event listeners
// and when chrome windows finish loading
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindowInternal.h"

// For seting scrollbar visibilty
#include "nsIDOMBarProp.h"

// for the focus hacking we need to do
#include "nsIFocusController.h"

#include "nsIFormControl.h"
// for the clipboard actions we need to do
#include "nsIClipboardCommands.h"

#ifndef MOZ_ENABLE_LIBXUL
// for profiles
#include "nsProfileDirServiceProvider.h"
#include "nsEmbedAPI.h"
// for NS_APPSHELL_CID
#include "nsWidgetsCID.h"
#endif

// app component registration
#include "nsIGenericFactory.h"
#include "nsIComponentRegistrar.h"

// all of our local includes
#include "EmbedPrivate.h"
#include "EmbedWindow.h"
#include "EmbedProgress.h"
#include "EmbedContentListener.h"
#include "EmbedEventListener.h"
#include "EmbedWindowCreator.h"
#ifdef MOZ_WIDGET_GTK2
#include "GtkPromptService.h"
#include "nsICookiePromptService.h"
#include "EmbedCertificates.h"
#include "EmbedDownloadMgr.h"
#ifdef MOZ_GTKPASSWORD_INTERFACE
#include "EmbedPasswordMgr.h"
#endif
#include "EmbedGlobalHistory.h"
#include "EmbedFilePicker.h"
#else
#include "nsNativeCharsetUtils.h"
#endif

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsIAccessibilityService.h"
#include "nsIAccessible.h"
#include "nsIDOMDocument.h"
#endif
#include "nsIDocument.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIDOMNSDocument.h"
#include "nsIEditingSession.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsEditorCID.h"

#include "nsEmbedCID.h"

//#include "nsICache.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
//#include "nsICacheListener.h"
static NS_DEFINE_CID(kCacheServiceCID,           NS_CACHESERVICE_CID);
static nsICacheService* sCacheService;

#ifdef MOZ_WIDGET_GTK2
static EmbedCommon* sEmbedCommon = nsnull;

/* static */
EmbedCommon*
EmbedCommon::GetInstance()
{
  if (!sEmbedCommon)
  {
    sEmbedCommon = new EmbedCommon();
    if (!sEmbedCommon)
      return nsnull;
    if (NS_FAILED(sEmbedCommon->Init()))
    {
      return nsnull;
    }
  }
  return sEmbedCommon;
}

/* static */
void
EmbedCommon::DeleteInstance()
{
  if (sEmbedCommon)
  {
    delete sEmbedCommon;
    sEmbedCommon = nsnull;

    EmbedGlobalHistory::DeleteInstance();

  }
}

nsresult
EmbedCommon::Init (void)
{
    mCommon = NULL;
    return NS_OK;
}
#endif

PRUint32     EmbedPrivate::sWidgetCount = 0;
char        *EmbedPrivate::sPath        = nsnull;
char        *EmbedPrivate::sCompPath    = nsnull;
nsVoidArray *EmbedPrivate::sWindowList  = nsnull;
#ifdef MOZ_ENABLE_LIBXUL
nsILocalFile *EmbedPrivate::sProfileDir  = nsnull;
nsISupports  *EmbedPrivate::sProfileLock = nsnull;
#else
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
char        *EmbedPrivate::sProfileDirS  = nsnull;
char        *EmbedPrivate::sProfileName = nsnull;
nsIPref     *EmbedPrivate::sPrefs       = nsnull;
nsIAppShell *EmbedPrivate::sAppShell    = nsnull;
nsProfileDirServiceProvider *EmbedPrivate::sProfileDirServiceProvider = nsnull;
#endif
GtkWidget   *EmbedPrivate::sOffscreenWindow = 0;
GtkWidget   *EmbedPrivate::sOffscreenFixed  = 0;

nsIDirectoryServiceProvider *EmbedPrivate::sAppFileLocProvider = nsnull;

GtkMozEmbed*
EmbedCommon::GetAnyLiveWidget()
{
  if (!EmbedPrivate::sWidgetCount || !EmbedPrivate::sWindowList)
    return nsnull;

  // Get the number of browser windows.
  PRInt32 count = EmbedPrivate::sWindowList->Count();
  // This function doesn't get called very often at all (only when
  // creating a new window) so it's OK to walk the list of open
  // windows.
  //FIXME need to choose right window
  GtkMozEmbed *ret = nsnull;
  for (int i = 0; i < count; i++) {
    EmbedPrivate *tmpPrivate = NS_STATIC_CAST(EmbedPrivate *,
                                              EmbedPrivate::sWindowList->ElementAt(i));
    ret = tmpPrivate->mOwningWidget;
  }
  return ret;
}

class GTKEmbedDirectoryProvider : public nsIDirectoryServiceProvider2
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2
};

static const GTKEmbedDirectoryProvider kDirectoryProvider;

NS_IMPL_QUERY_INTERFACE2(GTKEmbedDirectoryProvider,
                         nsIDirectoryServiceProvider,
                         nsIDirectoryServiceProvider2)

NS_IMETHODIMP_(nsrefcnt)
GTKEmbedDirectoryProvider::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(nsrefcnt)
GTKEmbedDirectoryProvider::Release()
{
  return 1;
}

NS_IMETHODIMP
GTKEmbedDirectoryProvider::GetFile(const char *aKey, PRBool *aPersist,
                                   nsIFile* *aResult)
{
  nsresult rv;
  if (EmbedPrivate::sAppFileLocProvider) {
    rv = EmbedPrivate::sAppFileLocProvider->GetFile(aKey, aPersist,
                                                    aResult);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  if (!strcmp(aKey, NS_APP_USER_PROFILE_50_DIR)) {
#ifdef MOZ_ENABLE_LIBXUL
    if (EmbedPrivate::sProfileDir) {
      *aPersist = PR_TRUE;
      return EmbedPrivate::sProfileDir->Clone(aResult);
    }
#else
    nsCOMPtr<nsILocalFile> profDir =
      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return rv;
    rv = profDir->InitWithNativePath(nsDependentCString(EmbedPrivate::sProfileDirS));
    if (NS_FAILED(rv))
      return rv;
    NS_ADDREF(*aResult = profDir);
    return NS_OK;
#endif
  }

  *aResult = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GTKEmbedDirectoryProvider::GetFiles(const char *aKey,
                                    nsISimpleEnumerator* *aResult)
{
  nsCOMPtr<nsIDirectoryServiceProvider2>
    dp2(do_QueryInterface(EmbedPrivate::sAppFileLocProvider));

  if (!dp2)
    return NS_ERROR_FAILURE;

  return dp2->GetFiles(aKey, aResult);
}

#ifdef MOZ_WIDGET_GTK2
NS_GENERIC_FACTORY_CONSTRUCTOR(GtkPromptService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(EmbedCertificates, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedDownloadMgr)
#ifdef MOZ_GTKPASSWORD_INTERFACE
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(EmbedPasswordMgr, EmbedPasswordMgr::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedSignonPrompt)
#endif
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(EmbedGlobalHistory, EmbedGlobalHistory::GetInstance)
NS_GENERIC_FACTORY_CONSTRUCTOR(EmbedFilePicker)

static const nsModuleComponentInfo defaultAppComps[] = {
#ifdef MOZ_GTKPASSWORD_INTERFACE
  {
    EMBED_PASSWORDMANAGER_DESCRIPTION,
    EMBED_PASSWORDMANAGER_CID,
    NS_PASSWORDMANAGER_CONTRACTID,
    EmbedPasswordMgrConstructor,
    EmbedPasswordMgr::Register,
    EmbedPasswordMgr::Unregister
  },
  { EMBED_PASSWORDMANAGER_DESCRIPTION,
    EMBED_PASSWORDMANAGER_CID,
    NS_PWMGR_AUTHPROMPTFACTORY,
    EmbedPasswordMgrConstructor,
    EmbedPasswordMgr::Register,
    EmbedPasswordMgr::Unregister},

  {
    EMBED_PASSWORDMANAGER_DESCRIPTION,
    NS_SINGLE_SIGNON_PROMPT_CID,
    "@mozilla.org/wallet/single-sign-on-prompt;1",
    EmbedSignonPromptConstructor
  },
#endif
  { "Prompt Service",
    NS_PROMPTSERVICE_CID,
    NS_COOKIEPROMPTSERVICE_CONTRACTID,
    GtkPromptServiceConstructor
  },
  {
    "Prompt Service",
    NS_PROMPTSERVICE_CID,
    "@mozilla.org/embedcomp/prompt-service;1",
    GtkPromptServiceConstructor
  },
  {
    EMBED_CERTIFICATES_DESCRIPTION,
    EMBED_CERTIFICATES_CID,
    NS_TOKENPASSWORDSDIALOG_CONTRACTID,
    EmbedCertificatesConstructor
  },
  {
    EMBED_CERTIFICATES_DESCRIPTION,
    EMBED_CERTIFICATES_CID,
    NS_BADCERTLISTENER_CONTRACTID,
    EmbedCertificatesConstructor
  },
#ifdef BAD_CERT_LISTENER2
  {
    EMBED_CERTIFICATES_DESCRIPTION,
    EMBED_CERTIFICATES_CID,
    NS_BADCERTLISTENER2_CONTRACTID,
    EmbedCertificatesConstructor
  },
#endif
  {
    EMBED_CERTIFICATES_DESCRIPTION,
    EMBED_CERTIFICATES_CID,
    NS_CERTIFICATEDIALOGS_CONTRACTID,
    EmbedCertificatesConstructor
  },
  {
    EMBED_CERTIFICATES_DESCRIPTION,
    EMBED_CERTIFICATES_CID,
    NS_CLIENTAUTHDIALOGS_CONTRACTID,
    EmbedCertificatesConstructor
  },
  {
    EMBED_CERTIFICATES_DESCRIPTION,
    EMBED_CERTIFICATES_CID,
    NS_CERTPICKDIALOGS_CONTRACTID,
    EmbedCertificatesConstructor
  },
  {
    EMBED_CERTIFICATES_DESCRIPTION,
    EMBED_CERTIFICATES_CID,
    NS_TOKENDIALOGS_CONTRACTID,
    EmbedCertificatesConstructor
  },
  {
    EMBED_CERTIFICATES_DESCRIPTION,
    EMBED_CERTIFICATES_CID,
    NS_DOMCRYPTODIALOGS_CONTRACTID,
    EmbedCertificatesConstructor
  },
  {
    EMBED_CERTIFICATES_DESCRIPTION,
    EMBED_CERTIFICATES_CID,
    NS_GENERATINGKEYPAIRINFODIALOGS_CONTRACTID,
    EmbedCertificatesConstructor
  },
// content handler component info
  {
    EMBED_DOWNLOADMGR_DESCRIPTION,
    EMBED_DOWNLOADMGR_CID,
    NS_IHELPERAPPLAUNCHERDLG_CONTRACTID,
    EmbedDownloadMgrConstructor
  },
  // global history
  {
    "Global History",
    NS_EMBEDGLOBALHISTORY_CID,
    NS_GLOBALHISTORY2_CONTRACTID,
    EmbedGlobalHistoryConstructor
  },
  {
    EMBED_FILEPICKER_CLASSNAME,
    EMBED_FILEPICKER_CID,
    EMBED_FILEPICKER_CONTRACTID,
    EmbedFilePickerConstructor
  },
};

const nsModuleComponentInfo *EmbedPrivate::sAppComps = defaultAppComps;
int   EmbedPrivate::sNumAppComps = sizeof(defaultAppComps) / sizeof(nsModuleComponentInfo);

#else

const nsModuleComponentInfo *EmbedPrivate::sAppComps = nsnull;
int   EmbedPrivate::sNumAppComps = 0;

#endif

EmbedPrivate::EmbedPrivate(void)
{
  mOwningWidget     = nsnull;
  mWindow           = nsnull;
  mProgress         = nsnull;
  mContentListener  = nsnull;
  mEventListener    = nsnull;
  mChromeMask       = nsIWebBrowserChrome::CHROME_ALL;
  mIsChrome         = PR_FALSE;
  mChromeLoaded     = PR_FALSE;
  mLoadFinished     = PR_TRUE;
  mListenersAttached = PR_FALSE;
  mMozWindowWidget  = 0;
  mIsDestroyed      = PR_FALSE;
  mDoResizeEmbed    = PR_TRUE;
  mOpenBlock        = PR_FALSE;
  mNeedFav          = PR_FALSE;

  PushStartup();
  if (!sWindowList) {
    sWindowList = new nsVoidArray();
  }
  sWindowList->AppendElement(this);
}

EmbedPrivate::~EmbedPrivate()
{
  sWindowList->RemoveElement(this);
  PopStartup();
}

nsresult
EmbedPrivate::Init(GtkMozEmbed *aOwningWidget)
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
  mContentListenerGuard = NS_ISUPPORTS_CAST(nsIURIContentListener*, mContentListener);
  mContentListener->Init(this);

  // Create our key listener object and initialize it.  It is assumed
  // that this will be destroyed before we go out of scope.
  mEventListener = new EmbedEventListener();
  mEventListenerGuard =
    NS_ISUPPORTS_CAST(nsIDOMKeyListener *, mEventListener);
  mEventListener->Init(this);

  // has the window creator service been set up?
  static int initialized = PR_FALSE;
  // Set up our window creator (only once)
  if (!initialized) {
    // We set this flag here instead of on success.  If it failed we
    // don't want to keep trying and leaking window creator objects.
    initialized = PR_TRUE;

    // create our local object
    EmbedWindowCreator *creator = new EmbedWindowCreator(&mOpenBlock);
    nsCOMPtr<nsIWindowCreator> windowCreator;
    windowCreator = NS_STATIC_CAST(nsIWindowCreator *, creator);

    // Attach it via the watcher service
    nsCOMPtr<nsIWindowWatcher> watcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    if (watcher)
      watcher->SetWindowCreator(windowCreator);
  }
  return NS_OK;
}

nsresult
EmbedPrivate::Realize(PRBool *aAlreadyRealized)
{

  *aAlreadyRealized = PR_FALSE;

  // create the offscreen window if we have to
  EnsureOffscreenWindow();

  // Have we ever been initialized before?  If so then just reparent
  // from the offscreen window.
  if (mMozWindowWidget) {
    gtk_widget_reparent(mMozWindowWidget, GTK_WIDGET(mOwningWidget));
    *aAlreadyRealized = PR_TRUE;
    return NS_OK;
  }

  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // get a handle on the navigation object
  mNavigation = do_QueryInterface(webBrowser);

  // Create our session history object and tell the navigation object
  // to use it.  We need to do this before we create the web browser
  // window.
  mSessionHistory = do_CreateInstance(NS_SHISTORY_CONTRACTID);
  if (mSessionHistory)
    mNavigation->SetSessionHistory(mSessionHistory);

  // create the window
  mWindow->CreateWindow();

  // bind the progress listener to the browser object
  nsCOMPtr<nsISupportsWeakReference> supportsWeak;
  supportsWeak = do_QueryInterface(mProgressGuard);
  nsCOMPtr<nsIWeakReference> weakRef;
  supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
  webBrowser->AddWebBrowserListener(
    weakRef,
    NS_GET_IID(nsIWebProgressListener));

  // set ourselves as the parent uri content listener
  nsCOMPtr<nsIURIContentListener> uriListener;
  uriListener = do_QueryInterface(mContentListenerGuard);
  webBrowser->SetParentURIContentListener(uriListener);

  // save the window id of the newly created window
  nsCOMPtr<nsIWidget> mozWidget;
  mWindow->mBaseWindow->GetMainWidget(getter_AddRefs(mozWidget));
  // get the native drawing area
  GdkWindow *tmp_window =
    NS_STATIC_CAST(GdkWindow *,
      mozWidget->GetNativeData(NS_NATIVE_WINDOW));
  // and, thanks to superwin we actually need the parent of that.
  tmp_window = gdk_window_get_parent(tmp_window);
  // save the widget ID - it should be the mozarea of the window.
  gpointer data = nsnull;
  gdk_window_get_user_data(tmp_window, &data);
  mMozWindowWidget = NS_STATIC_CAST(GtkWidget *, data);

  // Apply the current chrome mask
  ApplyChromeMask();

  return NS_OK;
}

void
EmbedPrivate::Unrealize(void)
{
  // reparent to our offscreen window
  gtk_widget_reparent(mMozWindowWidget, sOffscreenFixed);
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

#include "nsIDOMScreen.h"
void
EmbedPrivate::Resize(PRUint32 aWidth, PRUint32 aHeight)
{
  if (mDoResizeEmbed) {
    mWindow->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION |
                           nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER,
                           0, 0, aWidth, aHeight);
  } else {
#ifdef MOZ_WIDGET_GTK2
    PRInt32 X, Y, W, H;
    mWindow->GetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION, &X, &Y, &W, &H);
    if (Y < 0) {
      mWindow->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION, 0, 0, nsnull, nsnull);
      return;
    }
    EmbedContextMenuInfo * ctx_menu = mEventListener->GetContextInfo();
    gint x, y, width, height, depth;
    gdk_window_get_geometry(gtk_widget_get_parent_window(GTK_WIDGET(mOwningWidget)),
                            &x,
                            &y,
                            &width,
                            &height,
                            &depth);
    if (ctx_menu) {
      if (height < ctx_menu->mFormRect.y + ctx_menu->mFormRect.height) {
        PRInt32 sub = ctx_menu->mFormRect.y - height + ctx_menu->mFormRect.height;
        PRInt32 diff = ctx_menu->mFormRect.y - sub;
//        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!height: %i, Form.y: %i, Form.Height: %i, sub: %i, diff: %i\n", height, ctx_menu->mFormRect.y, ctx_menu->mFormRect.height, sub, diff);
        if (sub > 0 && diff >= 0)
          mWindow->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION, 0, -sub, nsnull, nsnull);
      }
    }
#endif
  }
}

void
EmbedPrivate::Destroy(void)
{
  // This flag might have been set from
  // EmbedWindow::DestroyBrowserWindow() as well if someone used a
  // window.close() or something or some other script action to close
  // the window.  No harm setting it again.
  mIsDestroyed = PR_TRUE;

  // Get the nsIWebBrowser object for our embedded window.
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  // Release our progress listener
  nsCOMPtr<nsISupportsWeakReference> supportsWeak;
  supportsWeak = do_QueryInterface(mProgressGuard);
  nsCOMPtr<nsIWeakReference> weakRef;
  supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
  webBrowser->RemoveWebBrowserListener(weakRef,
                                       NS_GET_IID(nsIWebProgressListener));
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

  // destroy our child window
  mWindow->ReleaseChildren();

  // release navigation
  mNavigation = nsnull;

  // release session history
  mSessionHistory = nsnull;

  mOwningWidget = nsnull;

  mMozWindowWidget = 0;
  mNeedFav = PR_FALSE;
}

void
EmbedPrivate::SetURI(const char *aURI)
{
#ifdef MOZ_WIDGET_GTK
  // XXX: Even though NS_CopyNativeToUnicode is not designed for non-filenames,
  // we know that it will do "the right thing" on UNIX.
  NS_CopyNativeToUnicode(nsDependentCString(aURI), mURI);
#endif

#ifdef MOZ_WIDGET_GTK2
#ifdef MOZILLA_INTERNAL_API
  CopyUTF8toUTF16(aURI, mURI);
#else
  mURI.AssignLiteral(aURI);
#endif
#endif
}

void
EmbedPrivate::LoadCurrentURI(void)
{
  if (!mURI.IsEmpty()) {
    nsCOMPtr<nsPIDOMWindow> piWin;
    GetPIDOMWindow(getter_AddRefs(piWin));

    nsAutoPopupStatePusher popupStatePusher(piWin, openAllowed);

    mNavigation->LoadURI(mURI.get(),                        // URI string
                         nsIWebNavigation::LOAD_FLAGS_NONE, // Load flags
                         nsnull,                            // Referring URI
                         nsnull,                            // Post data
                         nsnull);                           // extra headers
  }
}

void
EmbedPrivate::Reload(PRUint32 reloadFlags)
{
  /* Use the session history if it is available, this
   * allows framesets to reload correctly */
  nsCOMPtr<nsIWebNavigation> wn;

  if (mSessionHistory) {
    wn = do_QueryInterface(mSessionHistory);
  }
  if (!wn)
    wn = mNavigation;

  if (wn)
    wn->Reload(reloadFlags);
}


void
EmbedPrivate::ApplyChromeMask()
{
   if (mWindow) {
      nsCOMPtr<nsIWebBrowser> webBrowser;
      mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

      nsCOMPtr<nsIDOMWindow> domWindow;
      webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
      if (domWindow) {

         nsCOMPtr<nsIDOMBarProp> scrollbars;
         domWindow->GetScrollbars(getter_AddRefs(scrollbars));
         if (scrollbars) {

            scrollbars->SetVisible
               (mChromeMask & nsIWebBrowserChrome::CHROME_SCROLLBARS ?
                PR_TRUE : PR_FALSE);
         }
      }
   }
}


void
EmbedPrivate::SetChromeMask(PRUint32 aChromeMask)
{
  if (aChromeMask & GTK_MOZ_EMBED_FLAG_WINDOWRESIZEON) {
    mDoResizeEmbed = PR_TRUE;
    mChromeMask = aChromeMask;
  } else {
    mDoResizeEmbed = PR_FALSE;
    return;
  }
  mChromeMask = aChromeMask;

  ApplyChromeMask();
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
    nsCOMPtr<nsILocalFile> compDir;
    if (EmbedPrivate::sCompPath) {
      rv = NS_NewNativeLocalFile(nsDependentCString(EmbedPrivate::sCompPath), 1, getter_AddRefs(binDir));
      rv = NS_NewNativeLocalFile(nsDependentCString(EmbedPrivate::sCompPath), 1, getter_AddRefs(compDir));
      if (NS_FAILED(rv))
        return;
      PRBool exists;
      rv = compDir->AppendNative(nsDependentCString("components"));
      compDir->Exists(&exists);
      if (!exists)
        rv = compDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
      if (NS_FAILED(rv))
        return;
    } else
      NS_ASSERTION(EmbedPrivate::sCompPath, "Warning: Failed to init Component Path.\n");

#ifdef MOZ_ENABLE_LIBXUL

    const char *grePath = sPath;
    NS_ASSERTION(grePath, "Warning: Failed to init grePath.\n");

    if (!grePath)
      grePath = getenv("MOZILLA_FIVE_HOME");

    if (!grePath)
      return;

    nsCOMPtr<nsILocalFile> greDir;
    rv = NS_NewNativeLocalFile(nsDependentCString(grePath), PR_TRUE,
                               getter_AddRefs(greDir));
    if (NS_FAILED(rv))
      return;

    rv = XRE_InitEmbedding(greDir, binDir,
                           NS_CONST_CAST(GTKEmbedDirectoryProvider*,
                                         &kDirectoryProvider),
                           nsnull, nsnull);
    if (NS_FAILED(rv))
      return;

    if (EmbedPrivate::sProfileDir) {
      XRE_NotifyProfile();
    }

#else

    rv = NS_InitEmbedding(binDir, sAppFileLocProvider);
    if (NS_FAILED(rv))
      return;

    // we no longer need a reference to the DirectoryServiceProvider
    if (sAppFileLocProvider) {
      NS_RELEASE(sAppFileLocProvider);
      sAppFileLocProvider = nsnull;
    }

    rv = StartupProfile();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Warning: Failed to start up profiles.\n");

    nsCOMPtr<nsIAppShell> appShell;
    appShell = do_CreateInstance(kAppShellCID);
    if (!appShell) {
      NS_WARNING("Failed to create appshell in EmbedPrivate::PushStartup!\n");
      return;
    }
    sAppShell = appShell.get();
    NS_ADDREF(sAppShell);
#ifdef MOZILLA_1_8_BRANCH
    sAppShell->Create(0, nsnull);
    sAppShell->Spinup();
#endif
#endif

    rv = RegisterAppComponents();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Warning: Failed to register app components.\n");
  }
}

/* static */
void
EmbedPrivate::PopStartup(void)
{
  sWidgetCount--;
  if (sWidgetCount == 0) {
    NS_IF_RELEASE(sCacheService);

    // destroy the offscreen window
    DestroyOffscreenWindow();

#ifndef MOZ_ENABLE_LIBXUL
    // shut down the profiles
    ShutdownProfile();

    if (sAppShell) {
#ifdef MOZILLA_1_8_BRANCH
      // Shutdown the appshell service.
      sAppShell->Spindown();
#endif
      NS_RELEASE(sAppShell);
      sAppShell = 0;
    }

    // shut down XPCOM/Embedding
    NS_TermEmbedding();
#else

    // we no longer need a reference to the DirectoryServiceProvider
    if (EmbedPrivate::sAppFileLocProvider) {
      NS_RELEASE(EmbedPrivate::sAppFileLocProvider);
      EmbedPrivate::sAppFileLocProvider = nsnull;
    }

    // shut down XPCOM/Embedding
    XRE_TermEmbedding();
#endif
#ifdef MOZ_WIDGET_GTK2
    EmbedGlobalHistory::DeleteInstance();
#endif
  }
}

/* static */
void EmbedPrivate::SetPath(const char *aPath)
{
  if (sPath)
    free(sPath);
  if (aPath)
    sPath = strdup(aPath);
  else
    sPath = nsnull;
}

/* static */
void
EmbedPrivate::SetCompPath(const char *aPath)
{
  if (EmbedPrivate::sCompPath)
    free(EmbedPrivate::sCompPath);
  if (aPath)
    EmbedPrivate::sCompPath = strdup(aPath);
  else
    EmbedPrivate::sCompPath = nsnull;
}

/* static */
void
EmbedPrivate::SetAppComponents(const nsModuleComponentInfo* aComps,
                               int aNumComponents)
{
  sAppComps = aComps;
  sNumAppComps = aNumComponents;
}

/* static */
void
EmbedPrivate::SetProfilePath(const char *aDir, const char *aName)
{
#ifdef MOZ_ENABLE_LIBXUL
  if (EmbedPrivate::sProfileDir) {
    if (sWidgetCount) {
      NS_ERROR("Cannot change profile directory during run.");
      return;
    }
    NS_RELEASE(EmbedPrivate::sProfileDir);
    NS_RELEASE(EmbedPrivate::sProfileLock);
  }

  nsresult rv =
    NS_NewNativeLocalFile(nsDependentCString(aDir), PR_TRUE, &EmbedPrivate::sProfileDir);

  if (NS_SUCCEEDED(rv) && aName)
    rv = EmbedPrivate::sProfileDir->AppendNative(nsDependentCString(aName));
  if (NS_SUCCEEDED(rv)) {
    PRBool exists;
    rv = EmbedPrivate::sProfileDir->Exists(&exists);
    if (!exists) {
      rv = EmbedPrivate::sProfileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
    }
    rv = XRE_LockProfileDirectory(EmbedPrivate::sProfileDir, &EmbedPrivate::sProfileLock);
  }
  if (NS_SUCCEEDED(rv)) {
    if (sWidgetCount)
      XRE_NotifyProfile();

    return;
  }
  NS_WARNING("Failed to lock profile.");

  // Failed
  NS_IF_RELEASE(EmbedPrivate::sProfileDir);
  NS_IF_RELEASE(EmbedPrivate::sProfileLock);
#else
  if (sProfileDirS) {
    nsMemory::Free(sProfileDirS);
    sProfileDirS = nsnull;
  }

  if (sProfileName) {
    nsMemory::Free(sProfileName);
    sProfileName = nsnull;
  }

  if (aDir)
    sProfileDirS = (char *)nsMemory::Clone(aDir, strlen(aDir) + 1);

  if (aName)
    sProfileName = (char *)nsMemory::Clone(aName, strlen(aName) + 1);
#endif
}

void
EmbedPrivate::SetDirectoryServiceProvider(nsIDirectoryServiceProvider * appFileLocProvider)
{
  if (EmbedPrivate::sAppFileLocProvider)
    NS_RELEASE(EmbedPrivate::sAppFileLocProvider);

  if (appFileLocProvider) {
    EmbedPrivate::sAppFileLocProvider = appFileLocProvider;
    NS_ADDREF(EmbedPrivate::sAppFileLocProvider);
  }
}

nsresult
EmbedPrivate::OpenStream(const char *aBaseURI, const char *aContentType)
{
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  nsCOMPtr<nsIWebBrowserStream> wbStream = do_QueryInterface(webBrowser);
  if (!wbStream) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aBaseURI);
  if (NS_FAILED(rv))
    return rv;

  rv = wbStream->OpenStream(uri, nsDependentCString(aContentType));
  return rv;
}

nsresult
EmbedPrivate::AppendToStream(const PRUint8 *aData, PRUint32 aLen)
{
  // Attach listeners to this document since in some cases we don't
  // get updates for content added this way.
  ContentStateChange();

  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  nsCOMPtr<nsIWebBrowserStream> wbStream = do_QueryInterface(webBrowser);
  if (!wbStream) return NS_ERROR_FAILURE;

  return wbStream->AppendToStream(aData, aLen);
}

nsresult
EmbedPrivate::CloseStream(void)
{
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  nsCOMPtr<nsIWebBrowserStream> wbStream = do_QueryInterface(webBrowser);
  if (!wbStream) return NS_ERROR_FAILURE;

  return wbStream->CloseStream();
}

/* static */
EmbedPrivate *
EmbedPrivate::FindPrivateForBrowser(nsIWebBrowserChrome *aBrowser)
{
  if (!sWindowList)
    return nsnull;

  // Get the number of browser windows.
  PRInt32 count = sWindowList->Count();
  // This function doesn't get called very often at all (only when
  // creating a new window) so it's OK to walk the list of open
  // windows.
  for (int i = 0; i < count; i++) {
    EmbedPrivate *tmpPrivate = NS_STATIC_CAST(EmbedPrivate *, sWindowList->ElementAt(i));
    // get the browser object for that window
    nsIWebBrowserChrome *chrome =
      NS_STATIC_CAST(nsIWebBrowserChrome *, tmpPrivate->mWindow);
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

#ifdef MOZ_WIDGET_GTK2
#ifdef MOZ_GTKPASSWORD_INTERFACE
  EmbedPasswordMgr *passwordManager = EmbedPasswordMgr::GetInstance();
  if (passwordManager)
    passwordManager->mFormAttachCount = PR_FALSE;
#endif
#endif
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

#ifdef MOZ_WIDGET_GTK2
#ifdef MOZ_GTKPASSWORD_INTERFACE
  EmbedPasswordMgr *passwordManager = EmbedPasswordMgr::GetInstance();
  if (passwordManager && passwordManager->mFormAttachCount) {

    GList *list_full = NULL, *users_list = NULL;
    gint retval = -1;

    if (gtk_moz_embed_common_get_logins(NS_ConvertUTF16toUTF8(mURI).get(), &list_full)) {

      GList *ptr = list_full;
      while(ptr) {
        GtkMozLogin * login = NS_STATIC_CAST(GtkMozLogin*, ptr->data);
        if (login && login->user) {
          users_list = g_list_append(users_list, NS_strdup(login->user));
          NS_Free((void*)login->user);
          if (login->pass)
            NS_Free((void*)login->pass);
          if (login->host)
            NS_Free((void*)login->host);
        }
        else
          break;
        ptr = ptr->next;
      }
      g_list_free(list_full);
      if (users_list)
        gtk_signal_emit(GTK_OBJECT(mOwningWidget->common),
                        moz_embed_common_signals[COMMON_SELECT_LOGIN],
                        users_list,
                        &retval);
      if (retval != -1) {
        passwordManager->InsertLogin((const gchar*)g_list_nth_data(users_list, retval));
      }
      g_list_free(users_list);
    }
    passwordManager->mFormAttachCount = PR_FALSE;
  }
#endif
#endif
}

#ifdef MOZ_WIDGET_GTK
// handle focus in and focus out events
void
EmbedPrivate::TopLevelFocusIn(void)
{
  if (mIsDestroyed)
    return;

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
  if (mIsDestroyed)
    return;

  nsCOMPtr<nsPIDOMWindow> piWin;
  GetPIDOMWindow(getter_AddRefs(piWin));

  if (!piWin)
    return;

  nsIFocusController *focusController = piWin->GetRootFocusController();
  if (focusController)
    focusController->SetActive(PR_FALSE);
}
#endif /* MOZ_WIDGET_GTK */

void
EmbedPrivate::ChildFocusIn(void)
{
  if (mIsDestroyed)
    return;

#ifdef MOZ_WIDGET_GTK2
  nsresult rv;
  nsCOMPtr<nsIWebBrowser> webBrowser;
  rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIWebBrowserFocus> webBrowserFocus(do_QueryInterface(webBrowser));
  if (!webBrowserFocus)
    return;

  webBrowserFocus->Activate();
#endif /* MOZ_WIDGET_GTK2 */

#ifdef MOZ_WIDGET_GTK
  nsCOMPtr<nsPIDOMWindow> piWin;
  GetPIDOMWindow(getter_AddRefs(piWin));

  if (!piWin)
    return;

  piWin->Activate();
#endif /* MOZ_WIDGET_GTK */
}

void
EmbedPrivate::ChildFocusOut(void)
{
  if (mIsDestroyed)
    return;

#ifdef MOZ_WIDGET_GTK2
  nsresult rv;
  nsCOMPtr<nsIWebBrowser> webBrowser;
  rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIWebBrowserFocus> webBrowserFocus(do_QueryInterface(webBrowser));
  if (!webBrowserFocus)
    return;

  webBrowserFocus->Deactivate();
#endif /* MOZ_WIDGET_GTK2 */

#ifdef MOZ_WIDGET_GTK
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
#endif /* MOZ_WIDGET_GTK */
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
  rv = mEventReceiver->AddEventListenerByIID(
         eventListener,
         NS_GET_IID(nsIDOMKeyListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add key listener\n");
    return;
  }

  rv = mEventReceiver->AddEventListenerByIID(
        eventListener,
        NS_GET_IID(nsIDOMMouseListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add mouse listener\n");
    return;
  }

  rv = mEventReceiver->AddEventListenerByIID(
         eventListener,
         NS_GET_IID(nsIDOMUIListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add UI listener\n");
    return;
  }

  rv = mEventReceiver->AddEventListenerByIID(
         eventListener,
         NS_GET_IID(nsIDOMMouseMotionListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add Mouse Motion listener\n");
    return;
  }
  rv = mEventReceiver->AddEventListener(NS_LITERAL_STRING("focus"), eventListener, PR_TRUE);
  rv = mEventReceiver->AddEventListener(NS_LITERAL_STRING("blur"), eventListener, PR_TRUE);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to add Mouse Motion listener\n");
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
  rv = mEventReceiver->RemoveEventListenerByIID(
         eventListener,
         NS_GET_IID(nsIDOMKeyListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove key listener\n");
    return;
  }

  rv =
    mEventReceiver->RemoveEventListenerByIID(
      eventListener,
      NS_GET_IID(nsIDOMMouseListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove mouse listener\n");
    return;
  }

  rv = mEventReceiver->RemoveEventListenerByIID(
         eventListener,
         NS_GET_IID(nsIDOMUIListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove UI listener\n");
    return;
  }

  rv = mEventReceiver->RemoveEventListenerByIID(
         eventListener,
         NS_GET_IID(nsIDOMMouseMotionListener));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to remove Mouse Motion listener\n");
    return;
  }
  rv = mEventReceiver->RemoveEventListener(NS_LITERAL_STRING("focus"), eventListener, PR_TRUE);
  rv = mEventReceiver->RemoveEventListener(NS_LITERAL_STRING("blur"), eventListener, PR_TRUE);
  mListenersAttached = PR_FALSE;
}

nsresult
EmbedPrivate::GetFocusController(nsIFocusController * *controller)
{
  nsresult rv;
  if (!controller) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsPIDOMWindow> piWin;
  rv = GetPIDOMWindow(getter_AddRefs(piWin));
  if (!piWin || NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  nsIFocusController *focusController = piWin->GetRootFocusController();
  if (!focusController)
    return NS_ERROR_FAILURE;

  *controller = focusController;
  return NS_OK;
}

nsresult
EmbedPrivate::GetPIDOMWindow(nsPIDOMWindow **aPIWin)
{
  *aPIWin = nsnull;

  nsresult rv;
  // get the web browser
  nsCOMPtr<nsIWebBrowser> webBrowser;

  rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  if (NS_FAILED(rv) || !webBrowser)
    return NS_ERROR_FAILURE;
  // get the content DOM window for that web browser
  nsCOMPtr<nsIDOMWindow> domWindow;
  rv = webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
  if (NS_FAILED(rv) || !domWindow)
    return NS_ERROR_FAILURE;

  // get the private DOM window
  nsCOMPtr<nsPIDOMWindow> domWindowPrivate = do_QueryInterface(domWindow, &rv);
  // and the root window for that DOM window
  if (NS_FAILED(rv) || !domWindowPrivate)
    return NS_ERROR_FAILURE;
  *aPIWin = domWindowPrivate->GetPrivateRoot();

  if (*aPIWin) {
    NS_ADDREF(*aPIWin);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;

}

#ifdef MOZ_ACCESSIBILITY_ATK
void *
EmbedPrivate::GetAtkObjectForCurrentDocument()
{
  if (!mNavigation)
    return nsnull;

  nsCOMPtr<nsIAccessibilityService> accService =
    do_GetService("@mozilla.org/accessibilityService;1");
  if (accService) {
    //get current document
    nsCOMPtr<nsIDOMDocument> domDoc;
    mNavigation->GetDocument(getter_AddRefs(domDoc));
    NS_ENSURE_TRUE(domDoc, nsnull);

    nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(domDoc));
    NS_ENSURE_TRUE(domNode, nsnull);

    nsCOMPtr<nsIAccessible> acc;
    accService->GetAccessibleFor(domNode, getter_AddRefs(acc));
    NS_ENSURE_TRUE(acc, nsnull);

    void *atkObj = nsnull;
    if (NS_SUCCEEDED(acc->GetNativeInterface(&atkObj)))
      return atkObj;
  }
  return nsnull;
}
#endif /* MOZ_ACCESSIBILITY_ATK */

#ifndef MOZ_ENABLE_LIBXUL
/* static */
nsresult
EmbedPrivate::StartupProfile(void)
{
  // initialize profiles
  if (sProfileDirS && sProfileName) {
    nsresult rv;
    nsCOMPtr<nsILocalFile> profileDir;
    NS_NewNativeLocalFile(nsDependentCString(sProfileDirS), PR_TRUE,
                          getter_AddRefs(profileDir));
    if (!profileDir)
      return NS_ERROR_FAILURE;
    rv = profileDir->AppendNative(nsDependentCString(sProfileName));
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsProfileDirServiceProvider> locProvider;
    NS_NewProfileDirServiceProvider(PR_TRUE, getter_AddRefs(locProvider));
    if (!locProvider)
      return NS_ERROR_FAILURE;
    rv = locProvider->Register();
    if (NS_FAILED(rv))
      return rv;
    rv = locProvider->SetProfileDir(profileDir);
    if (NS_FAILED(rv))
      return rv;
    // Keep a ref so we can shut it down.
    NS_ADDREF(sProfileDirServiceProvider = locProvider);
    // get prefs
    nsCOMPtr<nsIPref> pref;
    pref = do_GetService(NS_PREF_CONTRACTID);
    if (!pref)
      return NS_ERROR_FAILURE;
    sPrefs = pref.get();
    NS_ADDREF(sPrefs);
  }
  return NS_OK;
}

/* static */
void
EmbedPrivate::ShutdownProfile(void)
{
  if (sProfileDirServiceProvider) {
    sProfileDirServiceProvider->Shutdown();
    NS_RELEASE(sProfileDirServiceProvider);
  }
  NS_IF_RELEASE(sPrefs);
}
#endif

/* static */
nsresult
EmbedPrivate::RegisterAppComponents(void)
{
  nsCOMPtr<nsIComponentRegistrar> cr;
  nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(cr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIComponentManager> cm;
  rv = NS_GetComponentManager (getter_AddRefs (cm));
  NS_ENSURE_SUCCESS (rv, rv);

  for (int i = 0; i < sNumAppComps; ++i) {
    nsCOMPtr<nsIGenericFactory> componentFactory;
    rv = NS_NewGenericFactory(getter_AddRefs(componentFactory),
                              &(sAppComps[i]));
    if (NS_FAILED(rv)) {
      NS_WARNING("Unable to create factory for component");
      continue;  // don't abort registering other components
    }

    rv = cr->RegisterFactory(sAppComps[i].mCID, sAppComps[i].mDescription,
                             sAppComps[i].mContractID, componentFactory);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to register factory for component");

    // Call the registration hook of the component, if any
    if (sAppComps[i].mRegisterSelfProc) {
      rv = sAppComps[i].mRegisterSelfProc(cm, nsnull, nsnull, nsnull,
                                          &(sAppComps[i]));
      NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to self-register component");
    }
  }

  return rv;
}

/* static */
void
EmbedPrivate::EnsureOffscreenWindow(void)
{
  if (sOffscreenWindow)
    return;
  sOffscreenWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_realize(sOffscreenWindow);
  sOffscreenFixed = gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(sOffscreenWindow), sOffscreenFixed);
  gtk_widget_realize(sOffscreenFixed);
}

/* static */
void
EmbedPrivate::DestroyOffscreenWindow(void)
{
  if (!sOffscreenWindow)
    return;
  gtk_widget_destroy(sOffscreenWindow);
  sOffscreenWindow = 0;
}

/* static */
PRBool
EmbedPrivate::ClipBoardAction(GtkMozEmbedClipboard type)
{
  nsresult rv = NS_OK;
  PRBool canDo = PR_TRUE;
  nsCOMPtr<nsIWebBrowser> webBrowser;
  rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  if (NS_FAILED(rv))
    return PR_FALSE;
  nsCOMPtr<nsIClipboardCommands> clipboard (do_GetInterface(webBrowser));
  if (!clipboard)
    return PR_FALSE;
  switch (type) {
    case GTK_MOZ_EMBED_SELECT_ALL:
    {
      rv = clipboard->SelectAll();
      break;
    }
    case GTK_MOZ_EMBED_CAN_SELECT:
    {
      //FIXME
      break;
    }
    case GTK_MOZ_EMBED_CUT:
    {
      rv = clipboard->CutSelection();
      break;
    }
    case GTK_MOZ_EMBED_COPY:
    {
      rv = clipboard->CopySelection();
      break;
    }
    case GTK_MOZ_EMBED_PASTE:
    {
      rv = clipboard->Paste();
      break;
    }
    case GTK_MOZ_EMBED_CAN_CUT:
    {
      rv = clipboard->CanCutSelection (&canDo);
      break;
    }
    case GTK_MOZ_EMBED_CAN_PASTE:
    {
      rv = clipboard->CanPaste (&canDo);
      break;
    }
    case GTK_MOZ_EMBED_CAN_COPY:
    {
      rv = clipboard->CanCopySelection (&canDo);
      break;
    }
    default:
    break;
  }
  if (NS_FAILED(rv))
    return PR_FALSE;
  return canDo;
}

char*
EmbedPrivate::GetEncoding ()
{
  char *encoding;
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  nsCOMPtr<nsIDocCharset> docCharset = do_GetInterface (webBrowser);
  docCharset->GetCharset (&encoding);
  return encoding;
}

nsresult
EmbedPrivate::SetEncoding (const char *encoding)
{
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  nsCOMPtr<nsIContentViewer> contentViewer;
  GetContentViewer (webBrowser, getter_AddRefs(contentViewer));
  NS_ENSURE_TRUE (contentViewer, NS_ERROR_FAILURE);
  nsCOMPtr<nsIMarkupDocumentViewer> mDocViewer = do_QueryInterface(contentViewer);
  NS_ENSURE_TRUE (mDocViewer, NS_ERROR_FAILURE);
  nsAutoString mCharset;
#ifdef MOZILLA_INTERNAL_API
  mCharset.AssignWithConversion (encoding);
#else
  mCharset.AssignLiteral (encoding);
#endif
  return mDocViewer->SetForceCharacterSet(NS_LossyConvertUTF16toASCII(ToNewUnicode(mCharset)));
}

PRBool
EmbedPrivate::FindText(const char *exp, PRBool  reverse,
                       PRBool  whole_word, PRBool  case_sensitive,
                       PRBool  restart)
{
  PRUnichar *text;
  PRBool match;
  nsresult rv;
  nsCOMPtr<nsIWebBrowser> webBrowser;
  mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface (webBrowser));
  g_return_val_if_fail(finder != NULL, FALSE);
  text = LocaleToUnicode (exp);
  finder->SetSearchString (text);
  finder->SetFindBackwards (reverse);
  finder->SetWrapFind(TRUE); //DoWrapFind
  finder->SetEntireWord (whole_word);
  finder->SetSearchFrames(TRUE); //SearchInFrames
  finder->SetMatchCase (case_sensitive);
  rv = finder->FindNext (&match);
  NS_Free(text);
  if (NS_FAILED(rv))
    return FALSE;

  return match;
}

nsresult
EmbedPrivate::ScrollToSelectedNode(nsIDOMNode *aDOMNode)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (aDOMNode) {
    nsCOMPtr <nsIDOMNSHTMLElement> nodeElement = do_QueryInterface(aDOMNode, &rv);
    if (NS_SUCCEEDED(rv) && nodeElement) {
      nodeElement->ScrollIntoView(PR_FALSE);
    }
  }
  return rv;
}

nsresult
EmbedPrivate::InsertTextToNode(nsIDOMNode *aDOMNode, const char *string)
{
  nsIDOMNode *targetNode = nsnull;
  nsresult rv;

  EmbedContextMenuInfo * ctx_menu = mEventListener->GetContextInfo();
  if (ctx_menu && ctx_menu->mEventNode && (ctx_menu->mEmbedCtxType & GTK_MOZ_EMBED_CTX_INPUT)) {
    targetNode = ctx_menu->mEventNode;
  }

  if (!targetNode)
    return NS_ERROR_FAILURE;

  nsString nodeName;
  targetNode->GetNodeName(nodeName);
  PRInt32 selectionStart = 0, selectionEnd = 0, textLength = 0;
  nsString buffer;

  if (ctx_menu->mCtxFormType == NS_FORM_TEXTAREA) {
    nsCOMPtr <nsIDOMHTMLTextAreaElement> input;
    input = do_QueryInterface(targetNode, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool rdonly = PR_FALSE;
    input->GetReadOnly(&rdonly);
    if (rdonly)
      return NS_ERROR_FAILURE;

    nsCOMPtr <nsIDOMNSHTMLTextAreaElement> nsinput;
    nsinput = do_QueryInterface(targetNode, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsinput->GetTextLength(&textLength);
    if (textLength > 0) {
      NS_ENSURE_SUCCESS(rv, rv);
      rv = input->GetValue(buffer);
      nsinput->GetSelectionStart (&selectionStart);
      nsinput->GetSelectionEnd (&selectionEnd);

      if (selectionStart != selectionEnd)
        buffer.Cut(selectionStart, selectionEnd - selectionStart);
#ifdef MOZILLA_INTERNAL_API
      buffer.Insert(UTF8ToNewUnicode(nsDependentCString(string)), selectionStart);
#else
      nsString nsstr;
      nsstr.AssignLiteral(string);
      buffer.Insert(nsstr, selectionStart);
#endif
    } else {
#ifdef MOZILLA_INTERNAL_API
      CopyUTF8toUTF16(string, buffer);
#else
      buffer.AssignLiteral(string);
#endif
    }

    input->SetValue(buffer);
    int len = strlen(string);
    nsinput->SetSelectionRange(selectionStart + len, selectionStart + len);
  }
  else if (ctx_menu->mCtxFormType) {
    nsCOMPtr <nsIDOMHTMLInputElement> input;
    input = do_QueryInterface(targetNode, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool rdonly = PR_FALSE;
    input->GetReadOnly(&rdonly);
    if (rdonly)
      return NS_ERROR_FAILURE;

    nsCOMPtr <nsIDOMNSHTMLInputElement> nsinput;
    nsinput = do_QueryInterface(targetNode, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsinput->GetTextLength(&textLength);

    if (textLength > 0) {
      NS_ENSURE_SUCCESS(rv, rv);
      rv = input->GetValue(buffer);
      nsinput->GetSelectionStart (&selectionStart);
      nsinput->GetSelectionEnd (&selectionEnd);

      if (selectionStart != selectionEnd) {
        buffer.Cut(selectionStart, selectionEnd - selectionStart);
      }
#ifdef MOZILLA_INTERNAL_API
      buffer.Insert(UTF8ToNewUnicode(nsDependentCString(string)), selectionStart);
#else
      nsString nsstr;
      buffer.Insert(nsstr, selectionStart);
#endif
    } else {
#ifdef MOZILLA_INTERNAL_API
      CopyUTF8toUTF16(string, buffer);
#else
      buffer.AssignLiteral(string);
#endif
    }

    input->SetValue(buffer);
    int len = strlen(string);
    nsinput->SetSelectionRange(selectionStart + len, selectionStart + len);
  }
  else {
    nsIWebBrowser *retval = nsnull;
    mWindow->GetWebBrowser(&retval);
    nsCOMPtr<nsIEditingSession> editingSession = do_GetInterface(retval);
    if (!editingSession)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIEditor> theEditor;
    nsCOMPtr<nsPIDOMWindow> piWin;
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(ctx_menu->mCtxDocument);
    if (!doc)
      return NS_OK;
    piWin = doc->GetWindow();
    editingSession->GetEditorForWindow(piWin, getter_AddRefs(theEditor));
    if (!theEditor) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIHTMLEditor> htmlEditor;
    htmlEditor = do_QueryInterface(theEditor, &rv);
    if (!htmlEditor)
      return NS_ERROR_FAILURE;

#ifdef MOZILLA_INTERNAL_API
    CopyUTF8toUTF16(string, buffer);
#else
    buffer.AssignLiteral(string);
#endif
    htmlEditor->InsertHTML(buffer);
  }
  return NS_OK;
}

nsresult
EmbedPrivate::GetDOMWindowByNode(nsIDOMNode *aNode, nsIDOMWindow * *aDOMWindow)
{
  nsresult rv;
  nsCOMPtr <nsIDOMDocument> nodeDoc;
  rv = aNode->GetOwnerDocument(getter_AddRefs(nodeDoc));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWebBrowser> webBrowser;
  rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIDOMWindow> mainWindow;
  rv = webBrowser->GetContentDOMWindow(getter_AddRefs(mainWindow));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> mainDoc;
  rv = mainWindow->GetDocument (getter_AddRefs(mainDoc));
  if (mainDoc == nodeDoc) {
    *aDOMWindow = mainWindow;
    NS_IF_ADDREF(*aDOMWindow);
    return NS_OK;
  }

  nsCOMPtr <nsIDOMWindowCollection> frames;
  rv = mainWindow->GetFrames(getter_AddRefs (frames));
  NS_ENSURE_SUCCESS(rv, rv);
  PRUint32 frameCount = 0;
  rv = frames->GetLength (&frameCount);
  nsCOMPtr <nsIDOMWindow> curWindow;
  for (unsigned int i= 0; i < frameCount; i++) {
    rv = frames->Item(i, getter_AddRefs (curWindow));
    if (!curWindow)
      continue;
    nsCOMPtr <nsIDOMDocument> currentDoc;
    curWindow->GetDocument (getter_AddRefs(currentDoc));
    if (currentDoc == nodeDoc) {
      *aDOMWindow = curWindow;
      NS_IF_ADDREF(*aDOMWindow);
      break;
    }
  }
  return rv;
}

nsresult
EmbedPrivate::GetZoom (PRInt32 *aZoomLevel, nsISupports *aContext)
{

  NS_ENSURE_ARG_POINTER(aZoomLevel);

  nsresult rv;
  *aZoomLevel = 100;

  nsCOMPtr <nsIDOMWindow> DOMWindow;
  if (aContext) {
    nsCOMPtr <nsIDOMNode> node = do_QueryInterface(aContext, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetDOMWindowByNode(node, getter_AddRefs(DOMWindow));
  } else {
    nsCOMPtr<nsIWebBrowser> webBrowser;
    rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = webBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  float zoomLevelFloat;
  if (DOMWindow)
    rv = DOMWindow->GetTextZoom(&zoomLevelFloat);
  NS_ENSURE_SUCCESS(rv, rv);

  *aZoomLevel = (int)round (zoomLevelFloat * 100.);
  return rv;
}
nsresult
EmbedPrivate::SetZoom (PRInt32 aZoomLevel, nsISupports *aContext)
{
  nsresult rv;
  nsCOMPtr <nsIDOMWindow> DOMWindow;

  if (aContext) {
    nsCOMPtr <nsIDOMNode> node = do_QueryInterface(aContext, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetDOMWindowByNode(node, getter_AddRefs(DOMWindow));
  } else {
    nsCOMPtr<nsIWebBrowser> webBrowser;
    rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = webBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  float zoomLevelFloat;
  zoomLevelFloat = (float) aZoomLevel / 100.;

  if (DOMWindow)
    rv = DOMWindow->SetTextZoom(zoomLevelFloat);

  return rv;
}

nsresult
EmbedPrivate::HasFrames  (PRUint32 *numberOfFrames)
{
  // setting default value.
  *numberOfFrames = 0;
  nsCOMPtr<nsIWebBrowser> webBrowser;
  nsresult rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  // get main dom window
  nsCOMPtr <nsIDOMWindow> DOMWindow;
  rv = webBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  // get frames.
  nsCOMPtr <nsIDOMWindowCollection> frameCollection;
  rv = DOMWindow->GetFrames (getter_AddRefs (frameCollection));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  // comparing frames' zoom level
  rv = frameCollection->GetLength (numberOfFrames);
  return rv;
}

nsresult
EmbedPrivate::GetMIMEInfo (const char **aMime, nsIDOMNode *aDOMNode)
{
  NS_ENSURE_ARG_POINTER(aMime);
  nsresult rv;
#ifdef MOZ_ENABLE_GTK2
  if (aDOMNode && mEventListener) {
    EmbedContextMenuInfo * ctx = mEventListener->GetContextInfo();
    if (!ctx)
      return NS_ERROR_FAILURE;
    nsCOMPtr<imgIRequest> request;
    rv = ctx->GetImageRequest(getter_AddRefs(request), aDOMNode);
    if (request)
      rv = request->GetMimeType((char**)aMime);
    return rv;
  }
#endif

  nsCOMPtr<nsIWebBrowser> webBrowser;
  rv = mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

  nsCOMPtr<nsIDOMWindow> DOMWindow;
  rv = webBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));

  nsCOMPtr<nsIDOMDocument> doc;
  rv = DOMWindow->GetDocument (getter_AddRefs(doc));

  nsCOMPtr<nsIDOMNSDocument> nsDoc = do_QueryInterface(doc);

  nsString nsmime;
  if (nsDoc)
    rv = nsDoc->GetContentType(nsmime);
  if (!NS_FAILED(rv) && !nsmime.IsEmpty())
    *aMime = g_strdup((char*)NS_LossyConvertUTF16toASCII(nsmime).get());
  return rv;
}

nsresult
EmbedPrivate::GetCacheEntry(const char *aStorage,
                             const char *aKeyName,
                             PRUint32 aAccess,
                             PRBool aIsBlocking,
                             nsICacheEntryDescriptor **aDescriptor)
{
  nsCOMPtr<nsICacheSession> session;
  nsresult rv;

  if (!sCacheService) {
    nsCOMPtr<nsICacheService> cacheService
      = do_GetService("@mozilla.org/network/cache-service;1", &rv);
    if (NS_FAILED(rv) || !cacheService) {
      NS_WARNING("do_GetService(kCacheServiceCID) failed\n");
      return rv;
    }
    NS_ADDREF(sCacheService = cacheService);
  }

  rv = sCacheService->CreateSession("HTTP", 0, PR_TRUE,
                                    getter_AddRefs(session));

  if (NS_FAILED(rv)) {
    NS_WARNING("nsCacheService::CreateSession() failed\n");
    return rv;
  }
  rv = session->OpenCacheEntry(nsCString(aKeyName),
                               nsICache::ACCESS_READ,
                               PR_FALSE,
                               aDescriptor);

  if (rv != NS_ERROR_CACHE_KEY_NOT_FOUND)
    NS_WARNING("OpenCacheEntry(ACCESS_READ) returned error for non-existent entry\n");
  return rv;
}
