/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vi:tw=2:ts=2:et:sw=2:
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com> 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#ifdef XP_OS2_VACPP
// XXX every other file that pulls in _os2.h has no problem with HTMX there;
// this one does; the problem may lie with the order of the headers below,
// which is why this fix is here instead of in _os2.h
typedef unsigned long HMTX;
#endif
#include "nsDocShell.h"
#include "nsIWebShell.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIClipboardCommands.h"
#include "nsILinkHandler.h"
#include "nsIStreamListener.h"
#include "nsIPrompt.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "nsIDNSService.h"
#include "nsIRefreshURI.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIProgressEventSink.h"
#include "nsDOMEvent.h"
#include "nsIPresContext.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "plevent.h"
#include "prprf.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"
//#include "nsPluginsCID.h"
#include "nsIPluginManager.h"
#include "nsIPref.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsIContent.h"
#include "prlog.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIWebShellServices.h"
#include "nsIGlobalHistory.h"
#include "prmem.h"
#include "nsXPIDLString.h"
#include "nsDOMError.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIURIContentListener.h"
#include "nsIDOMDocument.h"
#include "nsTimer.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsCURILoader.h"
#include "nsIDOMWindow.h"
#include "nsEscape.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsISocketTransportService.h"
#include "nsILayoutHistoryState.h"

#include "nsIHTTPChannel.h" // add this to the ick include list...we need it to QI for post data interface
#include "nsHTTPEnums.h"


#include "nsILocaleService.h"
#include "nsIStringBundle.h"
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kPlatformCharsetCID,  NS_PLATFORMCHARSET_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID,  NS_ICHARSETCONVERTERMANAGER_CID);


#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIProtocolHandler.h"

//XXX for nsIPostData; this is wrong; we shouldn't see the nsIDocument type
#include "nsIDocument.h"

#ifdef DEBUG
#undef NOISY_LINKS
#undef NOISY_WEBSHELL_LEAKS
#else
#undef NOISY_LINKS
#undef NOISY_WEBSHELL_LEAKS
#endif

#define NOISY_WEBSHELL_LEAKS
#ifdef NOISY_WEBSHELL_LEAKS
#undef DETECT_WEBSHELL_LEAKS
#define DETECT_WEBSHELL_LEAKS
#endif

#ifdef NS_DEBUG
/**
 * Note: the log module is created during initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule = PR_NewLogModule("webshell");
#endif

#define WEB_TRACE_CALLS        0x1
#define WEB_TRACE_HISTORY      0x2

#define WEB_LOG_TEST(_lm,_bit) (PRIntn((_lm)->level) & (_bit))

#ifdef NS_DEBUG
#define WEB_TRACE(_bit,_args)            \
  PR_BEGIN_MACRO                         \
    if (WEB_LOG_TEST(gLogModule,_bit)) { \
      PR_LogPrint _args;                 \
    }                                    \
  PR_END_MACRO
#else
#define WEB_TRACE(_bit,_args)
#endif

static NS_DEFINE_CID(kGlobalHistoryCID, NS_GLOBALHISTORY_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

//----------------------------------------------------------------------

typedef enum {
   eCharsetReloadInit,
   eCharsetReloadRequested,
   eCharsetReloadStopOrigional
} eCharsetReloadState;

class nsWebShell : public nsDocShell,
                   public nsIWebShell,
                   public nsIWebShellContainer,
                   public nsIWebShellServices,
                   public nsILinkHandler,
                   public nsIDocumentLoaderObserver,
                   public nsIProgressEventSink, // should go away (nsIDocLoaderObs)
                   public nsIRefreshURI,
                   public nsIURIContentListener,
                   public nsIClipboardCommands
{
public:
  nsWebShell();
  virtual ~nsWebShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIURICONTENTLISTENER

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR

  NS_DECL_NSIDOCUMENTLOADEROBSERVER

  NS_IMETHOD SetupNewViewer(nsIContentViewer* aViewer);

  // nsIContentViewerContainer
  NS_IMETHOD Embed(nsIContentViewer* aDocViewer,
                   const char* aCommand,
                   nsISupports* aExtraInfo);

  // nsIWebShell
  NS_IMETHOD SetContainer(nsIWebShellContainer* aContainer);
  NS_IMETHOD GetContainer(nsIWebShellContainer*& aResult);
  NS_IMETHOD GetTopLevelWindow(nsIWebShellContainer** aWebShellWindow);
  NS_IMETHOD GetRootWebShell(nsIWebShell*& aResult);
  NS_IMETHOD SetParent(nsIWebShell* aParent);
  NS_IMETHOD GetParent(nsIWebShell*& aParent);
  NS_IMETHOD GetReferrer(nsIURI **aReferrer);

  // Document load api's
  NS_IMETHOD GetDocumentLoader(nsIDocumentLoader*& aResult);

  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     const char* aCommand,
                     nsIInputStream* aPostDataStream=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                     nsISupports * aHistoryState=nsnull,
                     const PRUnichar* aReferrer=nsnull,
                     const char * aWindowTarget = nsnull);

  NS_IMETHOD LoadURI(nsIURI * aUri,
                     const char * aCommand,
                     nsIInputStream* aPostDataStream=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                     nsISupports * aHistoryState=nsnull,
                     const PRUnichar* aReferrer=nsnull,
                     const char * aWindowTarget = nsnull);

  NS_IMETHOD SessionHistoryInternalLoadURL(const PRUnichar *aURLSpec,
   nsLoadFlags aType, nsISupports * aHistoryState, const PRUnichar* aReferrer);


  NS_IMETHOD GetCanGoBack(PRBool* aCanGoBack);
  NS_IMETHOD GetCanGoForward(PRBool* aCanGoForward);
  NS_IMETHOD GoBack();
  NS_IMETHOD GoForward();
  NS_IMETHOD LoadURI(const PRUnichar* aURI);
  NS_IMETHOD InternalLoad(nsIURI* aURI, nsIURI* aReferrer,
      const char* aWindowTarget, nsIInputStream* aPostData, loadType aLoadType);

  NS_IMETHOD Stop(void);

  void SetReferrer(const PRUnichar* aReferrer);

  // History api's
  NS_IMETHOD GoTo(PRInt32 aHistoryIndex);
  NS_IMETHOD GetHistoryLength(PRInt32& aResult);
  NS_IMETHOD GetHistoryIndex(PRInt32& aResult);
  NS_IMETHOD GetURL(PRInt32 aHistoryIndex, const PRUnichar** aURLResult);

  // nsIWebShellContainer
  NS_IMETHOD SetHistoryState(nsISupports* aLayoutHistoryState);
  NS_IMETHOD FireUnloadEvent(void);

  // nsIWebShellServices
  NS_IMETHOD LoadDocument(const char* aURL,
                          const char* aCharset= nsnull ,
                          nsCharsetSource aSource = kCharsetUninitialized);
  NS_IMETHOD ReloadDocument(const char* aCharset= nsnull ,
                            nsCharsetSource aSource = kCharsetUninitialized);
  NS_IMETHOD StopDocumentLoad(void);
  NS_IMETHOD SetRendering(PRBool aRender);

  // nsILinkHandler
  NS_IMETHOD OnLinkClick(nsIContent* aContent,
                         nsLinkVerb aVerb,
                         const PRUnichar* aURLSpec,
                         const PRUnichar* aTargetSpec,
                         nsIInputStream* aPostDataStream = 0);
  NS_IMETHOD OnOverLink(nsIContent* aContent,
                        const PRUnichar* aURLSpec,
                        const PRUnichar* aTargetSpec);
  NS_IMETHOD GetLinkState(const PRUnichar* aURLSpec, nsLinkState& aState);

  // nsIRefreshURL interface methods...
  NS_IMETHOD RefreshURI(nsIURI* aURI, PRInt32 aMillis, PRBool aRepeat);
  NS_IMETHOD CancelRefreshURITimers(void);

  // nsIProgressEventSink
  NS_DECL_NSIPROGRESSEVENTSINK

  // nsIClipboardCommands
  NS_IMETHOD CanCutSelection  (PRBool* aResult);
  NS_IMETHOD CanCopySelection (PRBool* aResult);
  NS_IMETHOD CanPasteSelection(PRBool* aResult);

  NS_IMETHOD CutSelection  (void);
  NS_IMETHOD CopySelection (void);
  NS_IMETHOD PasteSelection(void);

  NS_IMETHOD SelectAll(void);
  NS_IMETHOD SelectNone(void);

  NS_IMETHOD FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

  // nsIBaseWindow
  NS_IMETHOD Create();
  NS_IMETHOD Destroy();
  NS_IMETHOD SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy,
      PRBool fRepaint);
  NS_IMETHOD GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx,
      PRInt32* cy);

  // nsIDocShell
  NS_IMETHOD SetDocument(nsIDOMDocument *aDOMDoc, nsIDOMElement *aRootNode);

  // nsWebShell
  nsIEventQueue* GetEventQueue(void);
  void HandleLinkClickEvent(nsIContent *aContent,
                            nsLinkVerb aVerb,
                            const PRUnichar* aURLSpec,
                            const PRUnichar* aTargetSpec,
                            nsIInputStream* aPostDataStream = 0);

  void ShowHistory();

  static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);

  nsresult CreatePluginHost(PRBool aAllowPlugins);
  nsresult DestroyPluginHost(void);

  NS_IMETHOD SetSessionHistory(nsISessionHistory * aSHist);
  NS_IMETHOD GetSessionHistory(nsISessionHistory *& aResult);
  NS_IMETHOD SetIsInSHist(PRBool aIsFrame);
  NS_IMETHOD GetIsInSHist(PRBool& aIsFrame);
  NS_IMETHOD SetURL(const PRUnichar* aURL);

protected:
  void GetRootWebShellEvenIfChrome(nsIWebShell** aResult);
  void InitFrameData();
  nsresult CheckForTrailingSlash(nsIURI* aURL);
  nsresult InitDialogVars(void);

  nsIEventQueue* mThreadEventQueue;

  nsIWebShellContainer* mContainer;
  nsIDocumentLoader* mDocLoader;

  nsCOMPtr<nsIPrompt> mPrompter;
  nsCOMPtr<nsIStringBundle> mStringBundle;

  nsString mDefaultCharacterSet;


  nsVoidArray mHistory;
  PRInt32 mHistoryIndex;

  PRBool mFiredUnloadEvent;

  nsIGlobalHistory* mHistoryService;
  nsISessionHistory * mSHist;

  nsRect   mBounds;

  PRPackedBool mIsInSHist;
  PRPackedBool mFailedToLoadHistoryService;

  nsVoidArray mRefreshments;

  eCharsetReloadState mCharsetReloadState;

  nsISupports* mHistoryState; // Weak reference.  Session history owns this.

  nsresult FireUnloadForChildren();
  nsresult DoLoadURL(nsIURI * aUri, 
                     const char* aCommand,
                     nsIInputStream* aPostDataStream,
                     nsLoadFlags aType,
                     const PRUnichar* aReferrer,
                     const char * aWindowTarget,
                     PRBool aKickOffLoad = PR_TRUE);

  nsresult PrepareToLoadURI(nsIURI * aUri, 
                            nsIInputStream * aPostStream,
                            PRBool aModifyHistory,
                            nsLoadFlags aType,
                            nsISupports * aHistoryState,
                            const PRUnichar * aReferrer);

  nsresult CreateViewer(nsIChannel* aChannel,
                        const char* aContentType,
                        const char* aCommand,
                        nsIStreamListener** aResult);
  static nsIPluginHost    *mPluginHost;
  static nsIPluginManager *mPluginManager;
  static PRUint32          mPluginInitCnt;
  PRBool mProcessedEndDocumentLoad;

  MOZ_TIMER_DECLARE(mTotalTime)

#ifdef DETECT_WEBSHELL_LEAKS
private:
  // We're counting the number of |nsWebShells| to help find leaks
  static unsigned long gNumberOfWebShells;

public:
  static unsigned long TotalWebShellsInExistence() { return gNumberOfWebShells; }
#endif
};

#ifdef DETECT_WEBSHELL_LEAKS
unsigned long nsWebShell::gNumberOfWebShells = 0;

extern "C" NS_WEB
unsigned long
NS_TotalWebShellsInExistence()
{
  return nsWebShell::TotalWebShellsInExistence();
}
#endif

//----------------------------------------------------------------------

// Class IID's
static NS_DEFINE_IID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kChildCID,               NS_CHILD_CID);
static NS_DEFINE_IID(kDocLoaderServiceCID,    NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kWebShellCID,            NS_WEB_SHELL_CID);

// IID's
static NS_DEFINE_IID(kIContentViewerContainerIID,
                     NS_ICONTENT_VIEWER_CONTAINER_IID);
static NS_DEFINE_IID(kIProgressEventSinkIID,  NS_IPROGRESSEVENTSINK_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID,     NS_IDOCUMENTLOADER_IID);
static NS_DEFINE_IID(kIFactoryIID,            NS_IFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kRefreshURIIID,          NS_IREFRESHURI_IID);

static NS_DEFINE_IID(kIWidgetIID,             NS_IWIDGET_IID);
static NS_DEFINE_IID(kIPluginManagerIID,      NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginHostIID,         NS_IPLUGINHOST_IID);
static NS_DEFINE_IID(kCPluginManagerCID,      NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kITimerCallbackIID,      NS_ITIMERCALLBACK_IID);
static NS_DEFINE_IID(kIWebShellContainerIID,  NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIClipboardCommandsIID,  NS_ICLIPBOARDCOMMANDS_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kISessionHistoryIID,     NS_ISESSIONHISTORY_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID,    NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_CID(kCDOMRangeCID,           NS_RANGE_CID);
// XXX not sure
static NS_DEFINE_IID(kILinkHandlerIID,        NS_ILINKHANDLER_IID);

nsIPluginHost *nsWebShell::mPluginHost = nsnull;
nsIPluginManager *nsWebShell::mPluginManager = nsnull;
PRUint32 nsWebShell::mPluginInitCnt = 0;

nsresult nsWebShell::CreatePluginHost(PRBool aAllowPlugins)
{
  nsresult rv = NS_OK;

  if ((PR_TRUE == aAllowPlugins) && (0 == mPluginInitCnt))
  {
    if (nsnull == mPluginManager)
    {
      // use the service manager to obtain the plugin manager.
      rv = nsServiceManager::GetService(kCPluginManagerCID, kIPluginManagerIID,
                                        (nsISupports**)&mPluginManager);
      if (NS_OK == rv)
      {
        if (NS_OK == mPluginManager->QueryInterface(kIPluginHostIID,
                                                    (void **)&mPluginHost))
        {
          mPluginHost->Init();
        }
      }
    }
  }

  mPluginInitCnt++;

  return rv;
}

nsresult nsWebShell::DestroyPluginHost(void)
{
  mPluginInitCnt--;

  NS_ASSERTION(!(mPluginInitCnt < 0), "underflow in plugin host destruction");

  if (0 == mPluginInitCnt)
  {
    if (nsnull != mPluginHost) {
      mPluginHost->Destroy();
      mPluginHost->Release();
      mPluginHost = NULL;
    }

    // use the service manager to release the plugin manager.
    if (nsnull != mPluginManager) {
      nsServiceManager::ReleaseService(kCPluginManagerCID, mPluginManager);
      mPluginManager = NULL;
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------

// Note: operator new zeros our memory
nsWebShell::nsWebShell() : nsDocShell()
{
#ifdef DETECT_WEBSHELL_LEAKS
  // We're counting the number of |nsWebShells| to help find leaks
  ++gNumberOfWebShells;
#endif
#ifdef NOISY_WEBSHELL_LEAKS
  printf("WEBSHELL+ = %ld\n", gNumberOfWebShells);
#endif

  NS_INIT_REFCNT();
  mHistoryIndex = -1;
  mThreadEventQueue = nsnull;
  InitFrameData();
  mItemType = typeContent;
  mSHist = nsnull;
  mIsInSHist = PR_FALSE;
  mFailedToLoadHistoryService = PR_FALSE;
  mDefaultCharacterSet = "";
  mProcessedEndDocumentLoad = PR_FALSE;
  mCharsetReloadState = eCharsetReloadInit;
  mHistoryService = nsnull;
  mHistoryState = nsnull;
  mFiredUnloadEvent = PR_FALSE;
  mBounds.SetRect(0, 0, 0, 0);
}

nsWebShell::~nsWebShell()
{
  if (nsnull != mHistoryService) {
    nsServiceManager::ReleaseService(kGlobalHistoryCID, mHistoryService);
    mHistoryService = nsnull;
  }

  // Stop any pending document loads and destroy the loader...
  if (nsnull != mDocLoader) {
    mDocLoader->Stop();
    mDocLoader->RemoveObserver((nsIDocumentLoaderObserver*)this);
    mDocLoader->SetContainer(nsnull);
    NS_RELEASE(mDocLoader);
  }
  // Cancel any timers that were set for this loader.
  CancelRefreshURITimers();

  ++mRefCnt; // following releases can cause this destructor to be called
             // recursively if the refcount is allowed to remain 0

  NS_IF_RELEASE(mSHist);
  NS_IF_RELEASE(mThreadEventQueue);
  mContentViewer=nsnull;
  mDeviceContext=nsnull;
  NS_IF_RELEASE(mContainer);

  if (mScriptGlobal) {
    mScriptGlobal->SetDocShell(nsnull);
    mScriptGlobal = nsnull;
  }
  if (mScriptContext) {
    mScriptContext->SetOwner(nsnull);
    mScriptContext = nsnull;
  }

  InitFrameData();

  // Free up history memory
  PRInt32 i, n = mHistory.Count();
  for (i = 0; i < n; i++) {
    nsString* s = (nsString*) mHistory.ElementAt(i);
    delete s;
  }


  DestroyPluginHost();

#ifdef DETECT_WEBSHELL_LEAKS
  // We're counting the number of |nsWebShells| to help find leaks
  --gNumberOfWebShells;
#endif
#ifdef NOISY_WEBSHELL_LEAKS
  printf("WEBSHELL- = %ld\n", gNumberOfWebShells);
#endif
}

void nsWebShell::InitFrameData()
{
  SetMarginWidth(-1);    
  SetMarginHeight(-1);
}

nsresult
nsWebShell::FireUnloadForChildren()
{
  nsresult rv = NS_OK;

  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIDocShell* shell = (nsIDocShell*) mChildren.ElementAt(i);
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(shell));
    rv = webShell->FireUnloadEvent();
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::FireUnloadEvent()
{
  nsresult rv = NS_OK;

  if (mScriptGlobal) {
    nsIDocumentViewer* docViewer;
    if (mContentViewer && NS_SUCCEEDED(mContentViewer->QueryInterface(kIDocumentViewerIID, (void**)&docViewer))) {
      nsIPresContext *presContext;
      if (NS_SUCCEEDED(docViewer->GetPresContext(presContext))) {
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_PAGE_UNLOAD;
        rv = mScriptGlobal->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

        NS_RELEASE(presContext);
      }
      NS_RELEASE(docViewer);
    }
  }

  //Fire child unloads now while our data is intact.
  rv = FireUnloadForChildren();

  mFiredUnloadEvent = PR_TRUE;

  return rv;
}

NS_IMPL_ADDREF_INHERITED(nsWebShell, nsDocShell)
NS_IMPL_RELEASE_INHERITED(nsWebShell, nsDocShell)

NS_INTERFACE_MAP_BEGIN(nsWebShell)
#if 0 // inherits from nsDocShell:
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebShell)
#endif
   NS_INTERFACE_MAP_ENTRY(nsIWebShell)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellServices)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIContentViewerContainer, nsIWebShell)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
   NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellContainer)
   NS_INTERFACE_MAP_ENTRY(nsILinkHandler)
   NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
   NS_INTERFACE_MAP_ENTRY(nsIClipboardCommands)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
#if 0 // inherits from nsDocShell:
   NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIDocShell)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeNode)
   NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
   NS_INTERFACE_MAP_ENTRY(nsIScrollable)
#endif
NS_INTERFACE_MAP_END_INHERITING(nsDocShell)

NS_IMETHODIMP
nsWebShell::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   NS_ENSURE_ARG_POINTER(aInstancePtr);
   nsresult rv = NS_OK;
   *aInstancePtr = nsnull;

   if(aIID.Equals(NS_GET_IID(nsILinkHandler)))
      {
      *aInstancePtr = NS_STATIC_CAST(nsILinkHandler*, this);
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
      }
   else if(aIID.Equals(NS_GET_IID(nsIScriptGlobalObjectOwner)))
      {
      *aInstancePtr = NS_STATIC_CAST(nsIScriptGlobalObjectOwner*, this);
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
      }
   else if (aIID.Equals(NS_GET_IID(nsIURIContentListener)))
   {
      *aInstancePtr = NS_STATIC_CAST(nsIURIContentListener*, this);
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
   }
   else if(aIID.Equals(NS_GET_IID(nsIScriptGlobalObject)))
      {
      NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), NS_ERROR_FAILURE);
      *aInstancePtr = mScriptGlobal;
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
      }
   else if(aIID.Equals(NS_GET_IID(nsIDOMWindow)))
      {
      NS_ENSURE_SUCCESS(EnsureScriptEnvironment(), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(mScriptGlobal->QueryInterface(NS_GET_IID(nsIDOMWindow),
         aInstancePtr), NS_ERROR_FAILURE);
      return NS_OK;
      }
   else if(mPluginManager) //XXX this seems a little wrong. MMP
      rv = mPluginManager->QueryInterface(aIID, aInstancePtr); 

   if (!*aInstancePtr || NS_FAILED(rv))
     return nsDocShell::GetInterface(aIID,aInstancePtr);
   else
     return rv;
}

NS_IMETHODIMP
nsWebShell::SetupNewViewer(nsIContentViewer* aViewer)
{
   NS_ENSURE_SUCCESS(nsDocShell::SetupNewViewer(aViewer), NS_ERROR_FAILURE);
    // If the history state has been set by session history,
    // set it on the pres shell now that we have a content
    // viewer.
   if(mContentViewer && mHistoryState)
      {
      nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(mContentViewer));
      if(docv)
         {
         nsCOMPtr<nsIPresShell> shell;
         docv->GetPresShell(*getter_AddRefs(shell));
         if(shell)
            shell->SetHistoryState((nsILayoutHistoryState*)mHistoryState);
         }
      }
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::Embed(nsIContentViewer* aContentViewer,
                  const char* aCommand,
                  nsISupports* aExtraInfo)
{
   return SetupNewViewer(aContentViewer);
}

NS_IMETHODIMP
nsWebShell::SetContainer(nsIWebShellContainer* aContainer)
{
  NS_IF_RELEASE(mContainer);
  mContainer = aContainer;
  NS_IF_ADDREF(mContainer);

  // uri dispatching change.....if you set a container for a webshell
  // and that container is a content listener itself....then use
  // it as our parent container. 
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURIContentListener> contentListener = do_QueryInterface(mContainer, &rv);
  if (NS_SUCCEEDED(rv) && contentListener)
    SetParentContentListener(contentListener);

  // if the container is getting set to null, then our parent must be going away
  // so clear out our knowledge of the content listener represented by the container
  if (!aContainer)
    SetParentContentListener(nsnull);

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetContainer(nsIWebShellContainer*& aResult)
{
  aResult = mContainer;
  NS_IF_ADDREF(mContainer);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetTopLevelWindow(nsIWebShellContainer** aTopLevelWindow)
{
   NS_ENSURE_ARG_POINTER(aTopLevelWindow);
   *aTopLevelWindow = nsnull;

   nsCOMPtr<nsIWebShell> rootWebShell;

   GetRootWebShellEvenIfChrome(getter_AddRefs(rootWebShell));
   if(!rootWebShell)
      return NS_OK;
   
   nsCOMPtr<nsIWebShellContainer> rootContainer;
   rootWebShell->GetContainer(*aTopLevelWindow);

   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetSessionHistory(nsISessionHistory* aSHist)
{
  NS_IF_RELEASE(mSHist);
  mSHist = aSHist;
  NS_IF_ADDREF(aSHist);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetSessionHistory(nsISessionHistory *& aResult)
{
  aResult = mSHist;
  NS_IF_ADDREF(mSHist);
  return NS_OK;
}

nsEventStatus PR_CALLBACK
nsWebShell::HandleEvent(nsGUIEvent *aEvent)
{
  return nsEventStatus_eIgnore;
}

NS_IMETHODIMP
nsWebShell::GetRootWebShell(nsIWebShell*& aResult)
{
   nsCOMPtr<nsIDocShellTreeItem> top;
   GetSameTypeRootTreeItem(getter_AddRefs(top));
   nsCOMPtr<nsIWebShell> topAsWebShell(do_QueryInterface(top));
   aResult = topAsWebShell;
   NS_IF_ADDREF(aResult);
   return NS_OK;
}

void
nsWebShell::GetRootWebShellEvenIfChrome(nsIWebShell** aResult)
{
   nsCOMPtr<nsIDocShellTreeItem> top;
   GetRootTreeItem(getter_AddRefs(top));
   nsCOMPtr<nsIWebShell> topAsWebShell(do_QueryInterface(top));
   *aResult = topAsWebShell;
   NS_IF_ADDREF(*aResult);
}

NS_IMETHODIMP
nsWebShell::SetParent(nsIWebShell* aParent)
{
   nsCOMPtr<nsIDocShellTreeItem> parentAsTreeItem(do_QueryInterface(aParent));

   mParent = parentAsTreeItem.get();
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetParent(nsIWebShell*& aParent)
{
   nsCOMPtr<nsIDocShellTreeItem> parent;
   NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);

   if(parent)
      parent->QueryInterface(NS_GET_IID(nsIWebShell), (void**)&aParent);
   else
      aParent = nsnull;
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetReferrer(nsIURI **aReferrer)
{
   *aReferrer = mReferrerURI;
   NS_IF_ADDREF(*aReferrer);
   return NS_OK;
}

void
nsWebShell::SetReferrer(const PRUnichar* aReferrer)
{
   NS_NewURI(getter_AddRefs(mReferrerURI), aReferrer, nsnull);
}

NS_IMETHODIMP
nsWebShell::SetURL(const PRUnichar* aURL)
{
  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), aURL, nsnull), 
                    NS_ERROR_FAILURE);
  SetCurrentURI(uri);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetIsInSHist(PRBool& aResult)
{
  aResult = mIsInSHist;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetIsInSHist(PRBool aIsInSHist)
{
  mIsInSHist = aIsInSHist;
  return NS_OK;
}

/**
 * Document Load methods
 */
NS_IMETHODIMP
nsWebShell::GetDocumentLoader(nsIDocumentLoader*& aResult)
{
  aResult = mDocLoader;
  NS_IF_ADDREF(mDocLoader);
  return (nsnull != mDocLoader) ? NS_OK : NS_ERROR_FAILURE;
}

static PRBool EqualBaseURLs(nsIURI* url1, nsIURI* url2)
{
   nsXPIDLCString spec1;
   nsXPIDLCString spec2;
   char *  anchor1 = nsnull, * anchor2=nsnull;
   PRBool rv = PR_FALSE;
  
   if (url1 && url2) {
     // XXX We need to make these strcmps case insensitive.
     url1->GetSpec(getter_Copies(spec1));
     url2->GetSpec(getter_Copies(spec2));
 
     /* Don't look at the ref-part */
     anchor1 = PL_strrchr(spec1, '#');
     anchor2 = PL_strrchr(spec2, '#');
 
     if (anchor1)
         *anchor1 = '\0';
     if (anchor2)
         *anchor2 = '\0';
 
     if (0 == PL_strcmp(spec1,spec2)) {
       rv = PR_TRUE;
     }
   }   // url1 && url2
  return rv;
}

nsresult
nsWebShell::DoLoadURL(nsIURI * aUri,
                      const char* aCommand,
                      nsIInputStream* aPostDataStream,
                      nsLoadFlags aType,
                      const PRUnichar* aReferrer,
                      const char * aWindowTarget,
                      PRBool aKickOffLoad)
{
  if (!aUri)
    return NS_ERROR_NULL_POINTER;

  nsXPIDLCString urlSpec;
  nsresult rv = NS_OK;
  rv = aUri->GetSpec(getter_Copies(urlSpec));
  if (NS_FAILED(rv)) return rv;

  // If it's a normal reload that uses the cache, look at the destination anchor
  // and see if it's an element within the current document
  // We don't have a reload loadtype yet in necko. So, check for just history
  // loadtype
  if ((aType == nsISessionHistory::LOAD_HISTORY || aType == nsIChannel::LOAD_NORMAL) && (nsnull != mContentViewer) &&
      (nsnull == aPostDataStream))
   {
    nsCOMPtr<nsIDocumentViewer> docViewer;
    if (NS_SUCCEEDED(mContentViewer->QueryInterface(kIDocumentViewerIID,
                                                    getter_AddRefs(docViewer))))
      {
      // Get the document object
      nsCOMPtr<nsIDocument> doc;
      docViewer->GetDocument(*getter_AddRefs(doc));

      // Get the URL for the document
      nsCOMPtr<nsIURI>  docURL = getter_AddRefs(doc->GetDocumentURL());

      if (aUri && docURL && EqualBaseURLs(docURL, aUri))
         {
         // See if there's a destination anchor
         nsXPIDLCString ref;
         nsCOMPtr<nsIURL> aUrl = do_QueryInterface(aUri);
         if (aUrl)
            rv = aUrl->GetRef(getter_Copies(ref));

         nsCOMPtr<nsIPresShell> presShell;
         rv = docViewer->GetPresShell(*getter_AddRefs(presShell));

         if (NS_SUCCEEDED(rv) && presShell)
            {
            /* Pass OnStartDocument notifications to the docloaderobserver
            * so that urlbar, forward/back buttons will
            * behave properly when going to named anchors
            */
            nsIInterfaceRequestor * interfaceRequestor = NS_STATIC_CAST(nsIInterfaceRequestor *, this);
            nsCOMPtr<nsIChannel> dummyChannel;
            // creating a channel is expensive...don't create it unless we know we have to
            // so move the creation down into each of the if clauses...
            if (nsnull != (const char *) ref)
               {
               rv = NS_OpenURI(getter_AddRefs(dummyChannel), aUri, nsnull, nsnull,
                               interfaceRequestor);
               if (NS_FAILED(rv)) return rv;
               if (!mProcessedEndDocumentLoad)
                  rv = OnStartDocumentLoad(mDocLoader, aUri, "load");
               // Go to the anchor in the current document
               rv = presShell->GoToAnchor(nsAutoString(ref));                      
               // Set the URL & referrer if the anchor was successfully visited
               if (NS_SUCCEEDED(rv))
                  {
                  SetCurrentURI(aUri);
                  SetReferrer(aReferrer);
                  }
               // Pass on status of scrolling/anchor visit to docloaderobserver
               if (!mProcessedEndDocumentLoad)
                  {
                  rv = OnEndDocumentLoad(mDocLoader, dummyChannel, rv);
                  }
               return rv;       
               }
            else if (aType == nsISessionHistory::LOAD_HISTORY)
               {
               rv = NS_OpenURI(getter_AddRefs(dummyChannel), aUri, nsnull, nsnull,
                               interfaceRequestor);
               if (NS_FAILED(rv)) return rv;
               rv = OnStartDocumentLoad(mDocLoader, aUri, "load");
               // Go to the top of the current document
               nsCOMPtr<nsIViewManager> viewMgr;
               rv = presShell->GetViewManager(getter_AddRefs(viewMgr));
               if (NS_SUCCEEDED(rv) && viewMgr)
                  {
                  nsIScrollableView* view;
                  rv = viewMgr->GetRootScrollableView(&view);
                  if (NS_SUCCEEDED(rv) && view)
                  rv = view->ScrollTo(0, 0, NS_VMREFRESH_IMMEDIATE);
                  if(NS_SUCCEEDED(rv))
                     {
                     SetCurrentURI(aUri);
                     SetReferrer(aReferrer);
                     }
                  mProcessedEndDocumentLoad = PR_FALSE;
                  // Pass on status of scrolling/anchor visit to docloaderobserver
                  rv = OnEndDocumentLoad(mDocLoader, dummyChannel, rv);
                  return rv;       
                  }
               }
#if 0              
            mProcessedEndDocumentLoad = PR_FALSE;
            // Pass on status of scrolling/anchor visit to docloaderobserver
            rv = OnEndDocumentLoad(mDocLoader, dummyChannel, rv);
            return rv;      
#endif /* 0 */
            }  // NS_SUCCEEDED(rv) && presShell
         } // EqualBaseURLs(docURL, url)
      }
   }

  // Stop loading the current document (if any...).  This call may result in
  // firing an EndLoadURL notification for the old document...
  if (aKickOffLoad)
    StopLoad();
  
  mProcessedEndDocumentLoad = PR_FALSE;

 /* WebShell was primarily passing the buck when it came to streamObserver.
  * So, pass on the observer which is already a streamObserver to DocLoder.
  */
  if (aKickOffLoad) {

#ifdef MOZ_PERF_METRICS
    {
       char* url;
       nsresult rv = NS_OK;
       rv = aUri->GetSpec(&url);
       if (NS_SUCCEEDED(rv)) {
         MOZ_TIMER_LOG(("*** Timing layout processes on url: '%s', webshell: %p\n", url, this));
         delete [] url;
       }
    }
  
    MOZ_TIMER_DEBUGLOG(("Reset and start: nsWebShell::DoLoadURL(), this=%p\n", this));
    MOZ_TIMER_RESET(mTotalTime);
    MOZ_TIMER_START(mTotalTime);
#endif

    // Create a referrer URI
    nsCOMPtr<nsIIOService> pNetService(do_GetService(kIOServiceCID));
    nsCOMPtr<nsIURI> referrer;
    if(aReferrer)
      {
      nsAutoString tempReferrer(aReferrer);
      char* referrerStr = tempReferrer.ToNewCString();
      pNetService->NewURI(referrerStr, nsnull, getter_AddRefs(referrer));
      Recycle(referrerStr);
      }

    // now let's pass the channel into the uri loader
    nsURILoadCommand loadCmd = nsIURILoader::viewNormal;
    if(nsCRT::strcasecmp(aCommand, "view-link-click") == 0)
       loadCmd = nsIURILoader::viewUserClick;

    NS_ENSURE_SUCCESS(DoURILoad(aUri, referrer, loadCmd, aWindowTarget, 
      aPostDataStream), NS_ERROR_FAILURE);
    return NS_OK; 
  }

  return rv;
}

NS_IMETHODIMP nsWebShell::GetCanGoBack(PRBool* aCanGoBack)
{
#ifdef DOCSHELL_LOAD
   return nsDocShell::GetCanGoBack(aCanGoBack);
#else  /*!DOCSHELL_LOAD*/
   *aCanGoBack = (mHistoryIndex - 1) > - 1 ? PR_TRUE : PR_FALSE;
   return NS_OK;
#endif /*!DOCSHELL_LOAD*/
}

NS_IMETHODIMP nsWebShell::GetCanGoForward(PRBool* aCanGoForward)
{
#ifdef DOCSHELL_LOAD
   return nsDocShell::GetCanGoForward(aCanGoForward);
#else  /*!DOCSHELL_LOAD*/
   *aCanGoForward = mHistoryIndex  < mHistory.Count() - 1 ? PR_TRUE : PR_FALSE;
   return NS_OK;
#endif /*!DOCSHELL_LOAD*/
}

NS_IMETHODIMP nsWebShell::GoBack()
{
#ifdef DOCSHELL_LOAD
   return nsDocShell::GoBack();
#else  /*!DOCSHELL_LOAD*/
   NS_ENSURE_SUCCESS(GoTo(mHistoryIndex - 1), NS_ERROR_FAILURE);
   return NS_OK;
#endif /*!DOCSHELL_LOAD*/
}

NS_IMETHODIMP nsWebShell::GoForward()
{
#ifdef DOCSHELL_LOAD
   return nsDocShell::GoForward();
#else  /*!DOCSHELL_LOAD*/
   NS_ENSURE_SUCCESS(GoTo(mHistoryIndex + 1), NS_ERROR_FAILURE);
   return NS_OK;
#endif /*!DOCSHELL_LOAD*/
}

NS_IMETHODIMP nsWebShell::LoadURI(const PRUnichar* aURI)
{
#ifdef DOCSHELL_LOAD
   return nsDocShell::LoadURI(aURI);
#else  /*!DOCSHELL_LOAD*/
   return LoadURL(aURI, "view");
#endif /*!DOCSHELL_LOAD*/
}

NS_IMETHODIMP nsWebShell::InternalLoad(nsIURI* aURI, nsIURI* aReferrer,
   const char* aWindowTarget, nsIInputStream* aPostData, loadType aLoadType)
{
#ifdef DOCSHELL_LOAD
   return nsDocShell::InternalLoad(aURI, aReferrer, aWindowTarget, aPostData, 
      aLoadType);
#else /*!DOCSHELL_LOAD*/
   PRBool updateHistory = PR_TRUE;
   switch(aLoadType)
      {
      case loadHistory:
      case loadReloadNormal:
      case loadReloadBypassCache:
      case loadReloadBypassProxy:
      case loadRelaodBypassProxyAndCache:
         updateHistory = PR_FALSE;
         break;

      default:
         NS_ERROR("Need to update case");
         // Fall through to a normal type of load.
      case loadNormalReplace:
      case loadNormal:
      case loadLink:
         updateHistory = PR_TRUE;
         break;
      } 

   nsXPIDLCString referrer;
   if(aReferrer)
      aReferrer->GetSpec(getter_Copies(referrer));

   return LoadURI(aURI, "view", nsnull, updateHistory, nsIChannel::LOAD_NORMAL,
      nsnull, nsAutoString(referrer).GetUnicode(), aWindowTarget);
#endif /*!DOCSHELL_LOAD*/
}

// nsIURIContentListener support
NS_IMETHODIMP nsWebShell::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);
   return mContentListener->OnStartURIOpen(aURI, aWindowTarget, aAbortOpen);
}

NS_IMETHODIMP
nsWebShell::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
   // Farm this off to our content listener
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);
   return mContentListener->GetProtocolHandler(aURI, aProtocolHandler);
}

NS_IMETHODIMP nsWebShell::IsPreferred(const char * aContentType,
                                      nsURILoadCommand aCommand,
                                      const char * aWindowTarget,
                                      char ** aDesiredContentType,
                                      PRBool * aCanHandleContent)
{
  NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);
  return mContentListener->IsPreferred(aContentType, aCommand, aWindowTarget, aDesiredContentType, 
                                       aCanHandleContent);
}

NS_IMETHODIMP nsWebShell::CanHandleContent(const char * aContentType,
                                           nsURILoadCommand aCommand,
                                           const char * aWindowTarget,
                                           char ** aDesiredContentType,
                                           PRBool * aCanHandleContent)

{
  NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);
  return mContentListener->CanHandleContent(aContentType, aCommand, aWindowTarget, aDesiredContentType, 
                                       aCanHandleContent);
} 

NS_IMETHODIMP 
nsWebShell::DoContent(const char * aContentType,
                      nsURILoadCommand aCommand,
                      const char * aWindowTarget,
                      nsIChannel * aOpenedChannel,
                      nsIStreamListener ** aContentHandler,
                      PRBool * aAbortProcess)
{
  NS_ENSURE_ARG(aOpenedChannel);
  nsresult rv = NS_OK;
  if (aAbortProcess)
    *aAbortProcess = PR_FALSE;

  // determine if the channel has just been retargeted to us...
  nsLoadFlags loadAttribs = 0;
  aOpenedChannel->GetLoadAttributes(&loadAttribs);
  // first, run any uri preparation stuff that we would have run normally
  // had we gone through OpenURI
  nsCOMPtr<nsIURI> aUri;
  aOpenedChannel->GetURI(getter_AddRefs(aUri));
  if (loadAttribs & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
  {
    PrepareToLoadURI(aUri, nsnull, PR_TRUE, nsIChannel::LOAD_NORMAL, nsnull, nsnull);
    // mscott: when I called DoLoadURL I found that we ran into problems because
    // we currently don't have channel retargeting yet. Basically, what happens is that
    // DoLoadURL calls StopBeforeRequestingURL and this cancels the current load group
    // however since we can't retarget yet, we were basically canceling our very
    // own load group!!! So the request would get canceled out from under us...
    // after retargeting we may be able to safely call DoLoadURL. 
    DoLoadURL(aUri, "view", nsnull, nsIChannel::LOAD_NORMAL, nsnull, nsnull, PR_FALSE);
    SetFocus(); // force focus to get set on the retargeted window...
  }

  OnLoadingSite(aOpenedChannel);

   return CreateContentViewer(aContentType, aOpenedChannel, aContentHandler); 
}

nsresult nsWebShell::PrepareToLoadURI(nsIURI * aUri, 
                                      nsIInputStream * aPostStream,
                                      PRBool aModifyHistory,
                                      nsLoadFlags aType,
                                      nsISupports * aHistoryState,
                                      const PRUnichar * aReferrer)
{
  nsresult rv;
  CancelRefreshURITimers();
  nsXPIDLCString scheme, CUriSpec;

  if (!aUri) return NS_ERROR_NULL_POINTER;

  rv = aUri->GetScheme(getter_Copies(scheme));
  if (NS_FAILED(rv)) return rv;
  rv = aUri->GetSpec(getter_Copies(CUriSpec));
  if (NS_FAILED(rv)) return rv;

  nsAutoString uriSpec(CUriSpec);

  nsXPIDLCString spec;
  rv = aUri->GetSpec(getter_Copies(spec));
  if (NS_FAILED(rv)) return rv;

  nsString* url = new nsString(uriSpec);
  if (aModifyHistory) {
    // Discard part of history that is no longer reachable
    PRInt32 i, n = mHistory.Count();
    i = mHistoryIndex + 1;
    while (--n >= i) {
      nsString* u = (nsString*) mHistory.ElementAt(n);
      delete u;
      mHistory.RemoveElementAt(n);
    }

    // Tack on new url
    mHistory.AppendElement(url);
    mHistoryIndex++;
  }
  else {
    
    // Replace the current history index with this URL
    nsString* u = (nsString*) mHistory.ElementAt(mHistoryIndex);
    if (nsnull != u) {
      delete u;
    }
    mHistory.ReplaceElementAt(url, mHistoryIndex);
  }
  ShowHistory();

  return rv;
}


NS_IMETHODIMP 
nsWebShell::LoadURI(nsIURI * aUri,
                    const char * aCommand,
                    nsIInputStream* aPostDataStream,
                    PRBool aModifyHistory,
                    nsLoadFlags aType,
                    nsISupports * aHistoryState,
                    const PRUnichar* aReferrer,
                    const char * aWindowTarget)
{
  nsresult rv = PrepareToLoadURI(aUri, aPostDataStream,
                                 aModifyHistory, aType,
                                 aHistoryState, aReferrer);
  if (NS_SUCCEEDED(rv))
    rv =  DoLoadURL(aUri, aCommand, aPostDataStream, aType, aReferrer, aWindowTarget);
  return rv;
}


NS_IMETHODIMP
nsWebShell::LoadURL(const PRUnichar *aURLSpec,
                    const char* aCommand,
                    nsIInputStream* aPostDataStream,
                    PRBool aModifyHistory,
                    nsLoadFlags aType,
                    nsISupports * aHistoryState,
                    const PRUnichar* aReferrer,
                    const char * aWindowTarget)
{
   nsresult rv = NS_OK;
   PRBool keywordsEnabled = PR_FALSE;

   /* 
     TODO This doesnt belong here... The app should be doing all this
     URL play. The webshell should not have a clue about whats "mailto" 
     If you insist that this should be here, then put in URL parsing 
     optimizations here. -Gagan
   */

   nsAutoString urlStr(aURLSpec);
   // first things first. try to create a uri out of the string.
   nsCOMPtr<nsIURI> uri;
   nsXPIDLCString  spec;
   if(0==urlStr.Find("file:",0))
      {
      // if this is file url, we need to  convert the URI
      // from Unicode to the FS charset
      nsCAutoString inFSCharset;
      rv = ConvertStringURIToFileCharset(urlStr, inFSCharset);
      NS_ASSERTION(NS_SUCCEEDED(rv), "convertURLToFielCharset failed");
      if(NS_SUCCEEDED(rv)) 
         rv = NS_NewURI(getter_AddRefs(uri), inFSCharset.GetBuffer(), nsnull);
      } 
   else 
      rv = NS_NewURI(getter_AddRefs(uri), urlStr, nsnull);
   if(NS_FAILED(rv))
      {
      // no dice.
      nsAutoString urlSpec;
      urlStr.Trim(" ");

      // see if we've got a file url.
      ConvertFileToStringURI(urlStr, urlSpec);
      nsCAutoString inFSCharset;
      rv = ConvertStringURIToFileCharset(urlSpec, inFSCharset);
      NS_ASSERTION(NS_SUCCEEDED(rv), "convertURLToFielCharset failed");
      if(NS_SUCCEEDED(rv)) 
         rv = NS_NewURI(getter_AddRefs(uri), inFSCharset.GetBuffer(), nsnull);

      if(NS_FAILED(rv))
         {
         rv = CreateFixupURI(urlStr.GetUnicode(), getter_AddRefs(uri));

         if(NS_ERROR_UNKNOWN_PROTOCOL == rv)
            {
            PRInt32 colon=urlStr.FindChar(':');
            // we weren't able to find a protocol handler
            rv = InitDialogVars();
            if (NS_FAILED(rv)) 
               return rv;

            nsXPIDLString messageStr;
            nsAutoString name("protocolNotFound");
            rv = mStringBundle->GetStringFromName(name.GetUnicode(), getter_Copies(messageStr));
            if (NS_FAILED(rv)) 
               return rv;

            NS_ASSERTION(colon != -1, "we shouldn't have gotten this far if we didn't have a colon");

            // extract the scheme
            nsAutoString scheme;
            urlSpec.Left(scheme, colon);

            nsAutoString dnsMsg(scheme);
            dnsMsg.Append(' ');
            dnsMsg.Append(messageStr);

            (void)mPrompter->Alert(dnsMsg.GetUnicode());
            } // end unknown protocol
         }
      }

   if(!uri)
      return rv;

   rv = uri->GetSpec(getter_Copies(spec));
   if(NS_FAILED(rv))
      return rv;

   // Get hold of Root webshell
   nsCOMPtr<nsIWebShell>  root;
   nsCOMPtr<nsISessionHistory> shist;
   PRBool  isLoadingHistory=PR_FALSE; // Is SH currently loading an entry from history?
   rv = GetRootWebShell(*getter_AddRefs(root));
   // Get hold of session History
   if (NS_SUCCEEDED(rv) && root)
      {    
      root->GetSessionHistory(*getter_AddRefs(shist));
      }
   if (shist)
      shist->GetLoadingFlag(&isLoadingHistory);


   /* 
   * Save the history state for the current index iff this loadurl() request
   * is not from SH. When the request comes from SH, aModifyHistory will
   * be false and nsSessionHistory.cpp takes of this.
   */
   if (shist)
      {
      PRInt32  indix;
      shist->GetCurrentIndex(&indix);
      if(indix >= 0 && (aModifyHistory))
         {
         nsCOMPtr<nsILayoutHistoryState>  historyState;
         //XXX For now don't do it when we have frames
         if(mChildren.Count())
            {
            nsCOMPtr<nsIPresShell> presShell;
            GetPresShell(getter_AddRefs(presShell));
            if(presShell)
               rv = presShell->CaptureHistoryState(getter_AddRefs(historyState));
            if(NS_SUCCEEDED(rv) && historyState)
               shist->SetHistoryObjectForIndex(indix, historyState);
            }
         }
      }
   /* Set the History state object for the current page in the
   * presentation shell. If it is a new page being visited,
   * aHistoryState is null. If the load is coming from
   * session History, it will be set to the cached history object by
   * session History.
   */
   SetHistoryState(aHistoryState);

   /* If the current Uri is the same as the one to be loaded
    * don't add it to session history. This avoids multiple
   * entries for the same url in the  go menu 
   */
   nsCOMPtr<nsIURI> currentURI;
   nsresult res = GetCurrentURI(getter_AddRefs(currentURI));
   if(NS_SUCCEEDED(res) && currentURI)
      {
      nsXPIDLCString currentUriSpec;
      rv = currentURI->GetSpec(getter_Copies(currentUriSpec));
      if(NS_FAILED(rv))
         return rv;
      nsAutoString currentURIString(currentUriSpec);
      if(currentURIString.Equals(spec))
         {
         /* The url to be loaded is the same as the 
	     * url already in the page. Don't add it to session history
	     */
	     aModifyHistory = PR_FALSE;
         }
      }


   /* Add the page to session history */
   if(aModifyHistory && shist)
      {
      PRInt32  ret;
      nsCAutoString referrer(aReferrer);
      ret = shist->Add(spec, referrer, this);
      }

   nsCOMPtr<nsIWebShell> parent;
   res = GetParent(*getter_AddRefs(parent));
   nsCOMPtr<nsIURI> newURI;

   if ((isLoadingHistory))
      {
      /* if LoadURL() got called from SH, AND If we are going "Back/Forward" 
      * to a frame page,SH  will change the current uri to the right value
      * for smoother redraw. 
      */
      res = GetCurrentURI(getter_AddRefs(newURI));
      }
   else
      {
      /* If the call is not from SH, use the url passed by the caller
      * so that things like JS will work right. This is for bug # 1646.
      * May regress in other situations.
      * What a hack
      */
      nsAutoString urlstr = (const char *) spec;
      res = NS_NewURI(getter_AddRefs(newURI), urlstr, nsnull);
      }


   if(NS_SUCCEEDED(res))
      {
      // now that we have a uri, call the REAL LoadURI method which requires a nsIURI.
      return LoadURI(newURI, aCommand, aPostDataStream, aModifyHistory, aType, aHistoryState, aReferrer, aWindowTarget);
      }
   return rv;
}


NS_IMETHODIMP nsWebShell::Stop(void)
{
  // Cancel any timers that were set for this loader.
  CancelRefreshURITimers();

  return nsDocShell::Stop();
}

//----------------------------------------

NS_IMETHODIMP nsWebShell::SessionHistoryInternalLoadURL(const PRUnichar *aURLSpec,
   nsLoadFlags aType, nsISupports * aHistoryState, const PRUnichar* aReferrer)
{
   return LoadURL(aURLSpec, "view", nsnull, PR_FALSE, aType, aHistoryState, aReferrer);
}

// History methods

NS_IMETHODIMP
nsWebShell::GoTo(PRInt32 aHistoryIndex)
{
  nsresult rv = NS_ERROR_ILLEGAL_VALUE;
  if ((aHistoryIndex >= 0) &&
      (aHistoryIndex < mHistory.Count())) {
    nsString* s = (nsString*) mHistory.ElementAt(aHistoryIndex);

#ifdef DEBUG
    printf("Goto %d\n", aHistoryIndex);
#endif
    mHistoryIndex = aHistoryIndex;
    ShowHistory();

    nsAutoString urlSpec(*s);

    // convert the uri spec into a url and then pass it to DoLoadURL
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), urlSpec, nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = DoLoadURL(uri,       // URL string
                   "view",        // Command
                   nsnull,        // Post Data
                   nsISessionHistory::LOAD_HISTORY,  // the reload type
                   nsnull,        // referrer
                   nsnull,        // window target
                   PR_TRUE);      // kick off load?
  }
  return rv;

}

NS_IMETHODIMP
nsWebShell::GetHistoryLength(PRInt32& aResult)
{
  aResult = mHistory.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetHistoryIndex(PRInt32& aResult)
{
  aResult = mHistoryIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetURL(PRInt32 aHistoryIndex, const PRUnichar** aURLResult)
{
  nsresult rv = NS_ERROR_ILLEGAL_VALUE;

  // XXX Ownership rules for the string passed back from this
  // method are not XPCOM compliant. If they were correct, 
  // the caller would deallocate the string.
  if ((aHistoryIndex >= 0) &&
      (aHistoryIndex <= mHistory.Count() - 1)) {
    nsString* s = (nsString*) mHistory.ElementAt(aHistoryIndex);
    if (nsnull != s) {
      *aURLResult = s->GetUnicode();
    }
    rv = NS_OK;
  }
  return rv;
}

void
nsWebShell::ShowHistory()
{
#if defined(NS_DEBUG)
  if (WEB_LOG_TEST(gLogModule, WEB_TRACE_HISTORY)) {
    PRInt32 i, n = mHistory.Count();
    for (i = 0; i < n; i++) {
      if (i == mHistoryIndex) {
        printf("**");
      }
      else {
        printf("  ");
      }
      nsString* u = (nsString*) mHistory.ElementAt(i);
      fputs(*u, stdout);
      printf("\n");
    }
  }
#endif
}


//----------------------------------------

//----------------------------------------------------------------------

// WebShell container implementation

NS_IMETHODIMP
nsWebShell::SetHistoryState(nsISupports* aLayoutHistoryState)
{
  mHistoryState = aLayoutHistoryState;
  return NS_OK;
}

//----------------------------------------------------------------------
// Web Shell Services API

NS_IMETHODIMP
nsWebShell::LoadDocument(const char* aURL,
                         const char* aCharset,
                         nsCharsetSource aSource)
{
  // XXX hack. kee the aCharset and aSource wait to pick it up
  nsCOMPtr<nsIContentViewer> cv;
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv)), NS_ERROR_FAILURE);
  if (cv)
  {
    nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);  
    if (muDV)
    {
      nsCharsetSource hint;
      muDV->GetHintCharacterSetSource((PRInt32 *)(&hint));
      if( aSource > hint ) 
      {
        nsAutoString inputCharSet(aCharset);
        muDV->SetHintCharacterSet(inputCharSet.GetUnicode());
        muDV->SetHintCharacterSetSource((PRInt32)aSource);
        if(eCharsetReloadRequested != mCharsetReloadState) 
        {
          mCharsetReloadState = eCharsetReloadRequested;
          nsAutoString url(aURL);
          LoadURI(url.GetUnicode());
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::ReloadDocument(const char* aCharset,
                           nsCharsetSource aSource)
{

  // XXX hack. kee the aCharset and aSource wait to pick it up
  nsCOMPtr<nsIContentViewer> cv;
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv)), NS_ERROR_FAILURE);
  if (cv)
  {
    nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);  
    if (muDV)
    {
      nsCharsetSource hint;
      muDV->GetHintCharacterSetSource((PRInt32 *)(&hint));
      if( aSource > hint ) 
      {
        nsAutoString inputCharSet(aCharset);
         muDV->SetHintCharacterSet(inputCharSet.GetUnicode());
         muDV->SetHintCharacterSetSource((PRInt32)aSource);
         if(eCharsetReloadRequested != mCharsetReloadState) 
         {
            mCharsetReloadState = eCharsetReloadRequested;
            return Reload(reloadNormal);
         }
      }
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsWebShell::StopDocumentLoad(void)
{
  if(eCharsetReloadRequested != mCharsetReloadState) 
  {
     Stop();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetRendering(PRBool aRender)
{
  if(eCharsetReloadRequested != mCharsetReloadState) 
  {
    if (mContentViewer) {
       mContentViewer->SetEnableRendering(aRender);
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------

// WebShell link handling

struct OnLinkClickEvent : public PLEvent {
  OnLinkClickEvent(nsWebShell* aHandler, nsIContent* aContent,
                   nsLinkVerb aVerb, const PRUnichar* aURLSpec,
                   const PRUnichar* aTargetSpec, nsIInputStream* aPostDataStream = 0);
  ~OnLinkClickEvent();

  void HandleEvent() {
    mHandler->HandleLinkClickEvent(mContent, mVerb, mURLSpec->GetUnicode(),
                                   mTargetSpec->GetUnicode(), mPostDataStream);
  }

  nsWebShell*  mHandler;
  nsString*    mURLSpec;
  nsString*    mTargetSpec;
  nsIInputStream* mPostDataStream;
  nsIContent*     mContent;
  nsLinkVerb      mVerb;
};

static void PR_CALLBACK HandlePLEvent(OnLinkClickEvent* aEvent)
{
  aEvent->HandleEvent();
}

static void PR_CALLBACK DestroyPLEvent(OnLinkClickEvent* aEvent)
{
  delete aEvent;
}

OnLinkClickEvent::OnLinkClickEvent(nsWebShell* aHandler,
                                   nsIContent *aContent,
                                   nsLinkVerb aVerb,
                                   const PRUnichar* aURLSpec,
                                   const PRUnichar* aTargetSpec,
                                   nsIInputStream* aPostDataStream)
{
  nsIEventQueue* eventQueue;

  mHandler = aHandler;
  NS_ADDREF(aHandler);
  mURLSpec = new nsString(aURLSpec);
  mTargetSpec = new nsString(aTargetSpec);
  mPostDataStream = aPostDataStream;
  NS_IF_ADDREF(mPostDataStream);
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mVerb = aVerb;

  PL_InitEvent(this, nsnull,
               (PLHandleEventProc) ::HandlePLEvent,
               (PLDestroyEventProc) ::DestroyPLEvent);

  eventQueue = aHandler->GetEventQueue();
  eventQueue->PostEvent(this);
  NS_RELEASE(eventQueue);
}

OnLinkClickEvent::~OnLinkClickEvent()
{
  NS_IF_RELEASE(mContent);
  NS_IF_RELEASE(mHandler);
  NS_IF_RELEASE(mPostDataStream);
  if (nsnull != mURLSpec) delete mURLSpec;
  if (nsnull != mTargetSpec) delete mTargetSpec;

}

//----------------------------------------

NS_IMETHODIMP
nsWebShell::OnLinkClick(nsIContent* aContent,
                        nsLinkVerb aVerb,
                        const PRUnichar* aURLSpec,
                        const PRUnichar* aTargetSpec,
                        nsIInputStream* aPostDataStream)
{
  OnLinkClickEvent* ev;
  nsresult rv = NS_OK;

  ev = new OnLinkClickEvent(this, aContent, aVerb, aURLSpec,
                            aTargetSpec, aPostDataStream);
  if (nsnull == ev) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  return rv;
}

nsIEventQueue* nsWebShell::GetEventQueue(void)
{
  NS_PRECONDITION(nsnull != mThreadEventQueue, "EventQueue for thread is null");
  NS_ADDREF(mThreadEventQueue);
  return mThreadEventQueue;
}

void
nsWebShell::HandleLinkClickEvent(nsIContent *aContent,
                                 nsLinkVerb aVerb,
                                 const PRUnichar* aURLSpec,
                                 const PRUnichar* aTargetSpec,
                                 nsIInputStream* aPostDataStream)
{
  nsAutoString target(aTargetSpec);

  switch(aVerb) {
    case eLinkVerb_New:
      target.Assign("_blank");
      // Fall into replace case
    case eLinkVerb_Undefined:
      // Fall through, this seems like the most reasonable action
    case eLinkVerb_Replace:
      {
        // for now, just hack the verb to be view-link-clicked
        // and down in the load document code we'll detect this and
        // set the correct uri loader command
        nsXPIDLCString spec;
        mCurrentURI->GetSpec(getter_Copies(spec));
        nsAutoString specString(spec);
        LoadURL(aURLSpec, "view-link-click", aPostDataStream,
                            PR_TRUE, nsIChannel::LOAD_NORMAL, 
                            nsnull, specString.GetUnicode(), nsCAutoString(aTargetSpec));
      }
      break;
    case eLinkVerb_Embed:
      // XXX TODO Should be similar to the HTML IMG ALT attribute handling
      //          in NS 4.x
    default:
      NS_ABORT_IF_FALSE(0,"unexpected link verb");
  }
}

NS_IMETHODIMP
nsWebShell::OnOverLink(nsIContent* aContent,
                       const PRUnichar* aURLSpec,
                       const PRUnichar* aTargetSpec)
{
    nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(mTreeOwner));

   if(browserChrome)
      browserChrome->SetOverLink(aURLSpec);

   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetLinkState(const PRUnichar* aURLSpec, nsLinkState& aState)
{
  aState = eLinkState_Unvisited;

  nsresult rv;

  // XXX: GlobalHistory is going to be moved out of the webshell into a more appropriate place.
  if ((nsnull == mHistoryService) && !mFailedToLoadHistoryService) {
    rv = nsServiceManager::GetService(kGlobalHistoryCID,
                                      NS_GET_IID(nsIGlobalHistory),
                                      (nsISupports**) &mHistoryService);

    if (NS_FAILED(rv)) {
      mFailedToLoadHistoryService = PR_TRUE;
    }
  }

  if (mHistoryService) {
    // XXX aURLSpec should really be a char*, not a PRUnichar*.
    nsAutoString urlStr(aURLSpec);

    char buf[256];
    char* url = buf;

    if (urlStr.Length() >= PRInt32(sizeof buf)) {
      url = new char[urlStr.Length() + 1];
    }

    PRInt64 lastVisitDate;

    if (url) {
      urlStr.ToCString(url, urlStr.Length() + 1);

      rv = mHistoryService->GetLastVisitDate(url, &lastVisitDate);

      if (url != buf)
        delete[] url;
    }
    else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }

  //XXX: Moved to destructor  nsServiceManager::ReleaseService(kGlobalHistoryCID, mHistoryService);

    if (NS_FAILED(rv)) return rv;

    // a last-visit-date of zero means we've never seen it before; so
    // if it's not zero, we must've seen it.
    if (! LL_IS_ZERO(lastVisitDate))
      aState = eLinkState_Visited;

    // XXX how to tell if eLinkState_OutOfDate?
  }

  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsWebShell::OnStartDocumentLoad(nsIDocumentLoader* loader,
                                nsIURI* aURL,
                                const char* aCommand)
{
  nsIDocumentViewer* docViewer;
  nsresult rv = NS_ERROR_FAILURE;

  if ((mScriptGlobal) &&
      (loader == mDocLoader)) {
    if (nsnull != mContentViewer &&
        NS_OK == mContentViewer->QueryInterface(kIDocumentViewerIID, (void**)&docViewer)) {
      nsIPresContext *presContext;
      if (NS_OK == docViewer->GetPresContext(presContext)) {
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_PAGE_UNLOAD;
        rv = mScriptGlobal->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

        NS_RELEASE(presContext);
      }
      NS_RELEASE(docViewer);
    }
  }

  if (loader == mDocLoader) {
    nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;

    if (!mDocLoaderObserver && mParent) {
      /* If this is a frame (in which case it would have a parent && doesn't
       * have a documentloaderObserver, get it from the rootWebShell
       */
      nsCOMPtr<nsIDocShellTreeItem> rootItem;
      GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
      nsCOMPtr<nsIDocShell> rootDocShell(do_QueryInterface(rootItem));

      if (rootDocShell)
        rootDocShell->GetDocLoaderObserver(getter_AddRefs(dlObserver));
    }
    else
    {
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
    }
    /*
     * Fire the OnStartDocumentLoad of the webshell observer
     */
    if ((nsnull != mContainer) && (nsnull != dlObserver))
    {
       dlObserver->OnStartDocumentLoad(mDocLoader, aURL, aCommand);
    }
  }

  return rv;
}



NS_IMETHODIMP
nsWebShell::OnEndDocumentLoad(nsIDocumentLoader* loader,
                              nsIChannel* channel,
                              nsresult aStatus)
{
#ifdef MOZ_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Stop: nsWebShell::OnEndDocumentLoad(), this=%p\n", this));
  MOZ_TIMER_STOP(mTotalTime);
  MOZ_TIMER_LOG(("Total (Layout + Page Load) Time (webshell=%p): ", this));
  MOZ_TIMER_PRINT(mTotalTime);
#endif

  nsresult rv = NS_ERROR_FAILURE;
  if (!channel) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;
  
  // clean up reload state for meta charset
  if(eCharsetReloadRequested == mCharsetReloadState)
      mCharsetReloadState = eCharsetReloadStopOrigional;
  else 
      mCharsetReloadState = eCharsetReloadInit;

  /* one of many safeguards that prevent death and destruction if
     someone is so very very rude as to bring this window down
     during this load handler. */
  nsCOMPtr<nsIWebShell> kungFuDeathGrip(this);

//if (!mProcessedEndDocumentLoad) {
  if (loader == mDocLoader) {
    mProcessedEndDocumentLoad = PR_TRUE;

    if (mScriptGlobal && !mEODForCurrentDocument) {
      nsIDocumentViewer* docViewer;
      if (nsnull != mContentViewer &&
          NS_OK == mContentViewer->QueryInterface(kIDocumentViewerIID, (void**)&docViewer)) {
        nsIPresContext *presContext;
        if (NS_OK == docViewer->GetPresContext(presContext)) {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_EVENT;
          event.message = NS_PAGE_LOAD;
          rv = mScriptGlobal->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

          NS_RELEASE(presContext);
        }
        NS_RELEASE(docViewer);
      }
    }

    mEODForCurrentDocument = PR_TRUE;

    nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;

    if (!mDocLoaderObserver && mParent) {
      /* If this is a frame (in which case it would have a parent && doesn't
       * have a documentloaderObserver, get it from the rootWebShell
       */
      nsCOMPtr<nsIDocShellTreeItem> rootItem;
      GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
      nsCOMPtr<nsIDocShell> rootDocShell(do_QueryInterface(rootItem));

      if (rootDocShell)
        rootDocShell->GetDocLoaderObserver(getter_AddRefs(dlObserver));
    }
    else
    {
      /* Take care of the Trailing slash situation */
      if (mSHist)
        CheckForTrailingSlash(aURL);
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
    }

    /*
     * Fire the OnEndDocumentLoad of the DocLoaderobserver
     */
    if (dlObserver && (nsnull != aURL)) {
       dlObserver->OnEndDocumentLoad(mDocLoader, channel, aStatus);
    }

    if (mDocLoader == loader && NS_FAILED(aStatus)) {
        nsAutoString errorMsg;
        nsXPIDLCString host;
        rv = aURL->GetHost(getter_Copies(host));
        if (NS_FAILED(rv)) return rv;

        CBufDescriptor buf((const char *)host, PR_TRUE, PL_strlen(host) + 1);
        nsCAutoString hostStr(buf);
        PRInt32 dotLoc = hostStr.FindChar('.');


        if (aStatus == NS_ERROR_UNKNOWN_HOST
            || aStatus == NS_ERROR_CONNECTION_REFUSED
            || aStatus == NS_ERROR_NET_TIMEOUT) {
            // First see if we should throw it to the keyword server.
            NS_ASSERTION(mPrefs, "the webshell's pref service wasn't initialized");
            PRBool keywordsEnabled = PR_FALSE;
            rv = mPrefs->GetBoolPref("keyword.enabled", &keywordsEnabled);
            if (NS_FAILED(rv)) return rv;

            if (keywordsEnabled && (-1 == dotLoc)) {
                // only send non-qualified hosts to the keyword server
                nsAutoString keywordSpec("keyword:");
                keywordSpec.Append(host);
                return LoadURI(keywordSpec.GetUnicode());
            } // end keywordsEnabled
        }

        // Doc failed to load because the host was not found.
        if (aStatus == NS_ERROR_UNKNOWN_HOST) {
            // Try our www.*.com trick.
            nsCAutoString retryHost;
            nsXPIDLCString scheme;
            rv = aURL->GetScheme(getter_Copies(scheme));
            if (NS_FAILED(rv)) return rv;

            PRUint32 schemeLen = PL_strlen((const char*)scheme);
            CBufDescriptor schemeBuf((const char*)scheme, PR_TRUE, schemeLen+1, schemeLen);
            nsCAutoString schemeStr(schemeBuf);

            if (schemeStr.Find("http") == 0) {
                if (-1 == dotLoc) {
                    retryHost = "www.";
                    retryHost += hostStr;
                    retryHost += ".com";
                } else {
                    PRInt32 hostLen = hostStr.Length();
                    if ( ((hostLen - dotLoc) == 3) || ((hostLen - dotLoc) == 4) ) {
                        retryHost = "www.";
                        retryHost += hostStr;
                    }
                }
            }

            if (!retryHost.IsEmpty()) {
                rv = aURL->SetHost(retryHost.GetBuffer());
                if (NS_FAILED(rv)) return rv;
                nsXPIDLCString aSpec;
                rv = aURL->GetSpec(getter_Copies(aSpec));
                if (NS_FAILED(rv)) return rv;
                nsAutoString newURL(aSpec);
                // reload the url
                return LoadURI(newURL.GetUnicode());
            } // retry

            // throw a DNS failure dialog
            rv = InitDialogVars();
            if (NS_FAILED(rv)) return rv;

            nsXPIDLString messageStr;
            nsAutoString name("dnsNotFound");
            rv = mStringBundle->GetStringFromName(name.GetUnicode(), getter_Copies(messageStr));
            if (NS_FAILED(rv)) return rv;

            errorMsg.Assign(host);
            errorMsg.Append(' ');
            errorMsg.Append(messageStr);

            (void)mPrompter->Alert(errorMsg.GetUnicode());
        } else // unknown host

        // Doc failed to load because we couldn't connect to the server.
        if (aStatus == NS_ERROR_CONNECTION_REFUSED) {
            // throw a connection failure dialog
            PRInt32 port = -1;
            rv = aURL->GetPort(&port);
            if (NS_FAILED(rv)) return rv;
            
            rv = InitDialogVars();
            if (NS_FAILED(rv)) return rv;

            nsXPIDLString messageStr;
            nsAutoString name("connectionFailure");
            rv = mStringBundle->GetStringFromName(name.GetUnicode(), getter_Copies(messageStr));
            if (NS_FAILED(rv)) return rv;

            errorMsg.Assign(messageStr);
            errorMsg.Append(' ');
            errorMsg.Append(host);
            if (port > 0) {
                errorMsg.Append(':');
                errorMsg.Append(port);
            }
            errorMsg.Append('.');

            (void)mPrompter->Alert(errorMsg.GetUnicode());
        } else // end NS_ERROR_CONNECTION_REFUSED

        // Doc failed to load because the socket function timed out.
        if (aStatus == NS_ERROR_NET_TIMEOUT) {
            // throw a timeout dialog
            rv = InitDialogVars();
            if (NS_FAILED(rv)) return rv;

            nsXPIDLString messageStr;
            nsAutoString name("netTimeout");
            rv = mStringBundle->GetStringFromName(name.GetUnicode(), getter_Copies(messageStr));
            if (NS_FAILED(rv)) return rv;

            errorMsg.Assign(messageStr);
            errorMsg.Append(' ');
            errorMsg.Append(host);
            errorMsg.Append('.');

            (void)mPrompter->Alert(errorMsg.GetUnicode());            
        } // end NS_ERROR_NET_TIMEOUT
    } // end mDocLoader == loader
  } //!mProcessedEndDocumentLoad
  else {
    rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::OnStartURLLoad(nsIDocumentLoader* loader,
                           nsIChannel* channel)
{
  nsresult rv;

  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;


  // Stop loading of the earlier document completely when the document url
  // load starts.  Now we know that this url is valid and available.
  PRBool equals = PR_FALSE;
  if (NS_SUCCEEDED(aURL->Equals(mCurrentURI, &equals)) && equals)
    Stop();

  /*
   *Fire the OnStartDocumentLoad of the webshell observer
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
    mDocLoaderObserver->OnStartURLLoad(mDocLoader, channel);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnProgressURLLoad(nsIDocumentLoader* loader,
                              nsIChannel* channel,
                              PRUint32 aProgress,
                              PRUint32 aProgressMax)
{
  /*
   *Fire the OnStartDocumentLoad of the webshell observer and container...
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
     mDocLoaderObserver->OnProgressURLLoad(mDocLoader, channel, aProgress, aProgressMax);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnStatusURLLoad(nsIDocumentLoader* loader,
                            nsIChannel* channel,
                            nsString& aMsg)
{
  /*
   *Fire the OnStartDocumentLoad of the webshell observer and container...
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
     mDocLoaderObserver->OnStatusURLLoad(mDocLoader, channel, aMsg);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnEndURLLoad(nsIDocumentLoader* loader,
                         nsIChannel* channel,
                         nsresult aStatus)
{
#if 0
  const char* spec;
  aURL->GetSpec(&spec);
  printf("nsWebShell::OnEndURLLoad:%p: loader=%p url=%s status=%d\n", this, loader, spec, aStatus);
#endif
  /*
   *Fire the OnEndDocumentLoad of the webshell observer
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
      mDocLoaderObserver->OnEndURLLoad(mDocLoader, channel, aStatus);
  }
  return NS_OK;
}

/* For use with redirect/refresh url api */
class refreshData : public nsITimerCallback
{
public:
  refreshData();

  NS_DECL_ISUPPORTS

  // nsITimerCallback interface
  NS_IMETHOD_(void) Notify(nsITimer *timer);

  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsIURI> mURI;
  PRBool       mRepeat;
  PRInt32      mDelay;

protected:
  virtual ~refreshData();
};

refreshData::refreshData()
{
  NS_INIT_REFCNT();
}

refreshData::~refreshData()
{
}


NS_IMPL_ISUPPORTS(refreshData, kITimerCallbackIID);

NS_IMETHODIMP_(void) refreshData::Notify(nsITimer *aTimer)
{
   NS_ASSERTION(mDocShell, "DocShell is somehow null");

   if(mDocShell)
      mDocShell->LoadURI(mURI, nsnull);
  /*
   * LoadURL(...) will cancel all refresh timers... This causes the Timer and
   * its refreshData instance to be released...
   */
}


NS_IMETHODIMP nsWebShell::RefreshURI(nsIURI* aURI, PRInt32 aDelay, PRBool aRepeat)
{
   NS_ENSURE_ARG(aURI);

   refreshData* data = new refreshData();
   NS_ENSURE_TRUE(data, NS_ERROR_OUT_OF_MEMORY);

   nsCOMPtr<nsISupports> dataRef = data; // Get the ref count to 1

   data->mDocShell = this;
   data->mURI = aURI;
   data->mDelay = aDelay;
   data->mRepeat = aRepeat;

   nsITimer* timer = nsnull;

   NS_NewTimer(&timer);
   NS_ENSURE_TRUE(timer, NS_ERROR_FAILURE);

   NS_LOCK_INSTANCE();  // XXX What is this for?
   mRefreshments.AppendElement(timer);
   timer->Init(data, aDelay);
   NS_UNLOCK_INSTANCE();

   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::CancelRefreshURITimers(void)
{
  PRInt32 i;
  nsITimer* timer;

  /* Right now all we can do is cancel all the timers for this webshell. */
  NS_LOCK_INSTANCE();

  /* Walk the list backwards to avoid copying the array as it shrinks.. */
  for (i = mRefreshments.Count()-1; (0 <= i); i--) {
    timer=(nsITimer*)mRefreshments.ElementAt(i);
    /* Remove the entry from the list before releasing the timer.*/
    mRefreshments.RemoveElementAt(i);

    if (timer) {
      timer->Cancel();
      NS_RELEASE(timer);
    }
  }
  NS_UNLOCK_INSTANCE();

  return NS_OK;
}


//----------------------------------------------------------------------

/*
 * There are cases where netlib does things like add a trailing slash
 * to the url being retrieved.  We need to watch out for such
 * changes and update the currently loading url's entry in the history
 * list. UpdateHistoryEntry() does this.
 *
 * Assumptions:
 *
 *   1) aURL is the URL that was inserted into the history list in LoadURL()
 *   2) The load of aURL is in progress and this function is being called
 *      from one of the functions in nsIStreamListener implemented by nsWebShell.
 */
nsresult nsWebShell::CheckForTrailingSlash(nsIURI* aURL)
{

  PRInt32     curIndex=0;
  nsresult rv;

  /* Get current history index and url for it */
  rv = mSHist->GetCurrentIndex(&curIndex);

  /* Get the url that netlib passed us */
  char* spec;
  aURL->GetSpec(&spec);
 
  //Set it in session history
  if (NS_SUCCEEDED(rv) && !mTitle.IsEmpty()) {
    mSHist->SetTitleForIndex(curIndex, mTitle.GetUnicode());
    // Replace the top most history entry with the new url
    mSHist->SetURLForIndex(curIndex, spec);
  }
  nsCRT::free(spec);


  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsWebShell::CanCutSelection(PRBool* aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = PR_FALSE;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::CanCopySelection(PRBool* aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = PR_FALSE;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::CanPasteSelection(PRBool* aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = PR_FALSE;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::CutSelection(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::CopySelection(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::PasteSelection(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::SelectAll(void)
{
  nsresult rv;

  nsCOMPtr<nsIDocumentViewer> docViewer;
  rv = mContentViewer->QueryInterface(kIDocumentViewerIID,
                                      getter_AddRefs(docViewer));
  if (NS_FAILED(rv) || !docViewer) return rv;

  nsCOMPtr<nsIPresShell> presShell;
  rv = docViewer->GetPresShell(*getter_AddRefs(presShell));
  if (NS_FAILED(rv) || !presShell) return rv;

  nsCOMPtr<nsIDOMSelection> selection;
  rv = presShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv) || !selection) return rv;

  // Get the document object
  nsCOMPtr<nsIDocument> doc;
  rv = docViewer->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(rv) || !doc) return rv;


  nsCOMPtr<nsIDOMHTMLDocument> htmldoc;
  rv = doc->QueryInterface(kIDOMHTMLDocumentIID, (void**)&htmldoc);
  if (NS_FAILED(rv) || !htmldoc) return rv;

  nsCOMPtr<nsIDOMHTMLElement>bodyElement;
  rv = htmldoc->GetBody(getter_AddRefs(bodyElement));
  if (NS_FAILED(rv) || !bodyElement) return rv;

  nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
  if (!bodyNode) return NS_ERROR_NO_INTERFACE;

#if 0
  rv = selection->Collapse(bodyNode, 0);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMRange>range;
  rv = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(rv) || !range) return rv;
#endif

  rv = selection->ClearSelection();
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMRange> range;
  rv = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
                                          NS_GET_IID(nsIDOMRange),
                                          getter_AddRefs(range));

  rv = range->SelectNodeContents(bodyNode);
  if (NS_FAILED(rv)) return rv;

  rv = selection->AddRange(range);
  return rv;
}

NS_IMETHODIMP
nsWebShell::SelectNone(void)
{
  return NS_ERROR_FAILURE;
}


//----------------------------------------------------
NS_IMETHODIMP
nsWebShell::FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  return NS_ERROR_FAILURE;
}

// Methods from nsIProgressEventSink
NS_IMETHODIMP
nsWebShell::OnProgress(nsIChannel* channel, nsISupports* ctxt, 
    PRUint32 aProgress, 
    PRUint32 aProgressMax)
{
    if (nsnull != mDocLoaderObserver)
    {
        return mDocLoaderObserver->OnProgressURLLoad(
            mDocLoader,
            channel,
            aProgress,
            aProgressMax);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnStatus(nsIChannel* channel, nsISupports* ctxt, 
    const PRUnichar* aMsg)
{
    if (nsnull != mDocLoaderObserver)
    {
        nsAutoString temp(aMsg);
        nsresult rv =  mDocLoaderObserver->OnStatusURLLoad(
            mDocLoader,
            channel,
            temp);

        return rv;
    }
    return NS_OK;
}

#define DIALOG_STRING_URI "chrome://global/locale/appstrings.properties"

nsresult nsWebShell::InitDialogVars(void)
{
    nsresult rv = NS_OK;
    if (!mPrompter) {
        // Get an nsIPrompt so we can throw dialogs.
      mPrompter = do_GetInterface(NS_STATIC_CAST(nsIDocShell*, this));
    }

    if (!mStringBundle) {
        // Get a string bundle so we can retrieve dialog strings
        nsCOMPtr<nsILocale> locale;
        NS_WITH_SERVICE(nsILocaleService, localeServ, kLocaleServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = localeServ->GetSystemLocale(getter_AddRefs(locale));
        if (NS_FAILED(rv)) return rv;

        NS_WITH_SERVICE(nsIStringBundleService, service, kCStringBundleServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = service->CreateBundle(DIALOG_STRING_URI, locale, getter_AddRefs(mStringBundle));
    }
    return rv;
}

//*****************************************************************************
// nsWebShell::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsWebShell::Create()
{
     // Cache the PL_EventQueue of the current UI thread...
  //
  // Since this call must be made on the UI thread, we know the Event Queue
  // will be associated with the current thread...
  //
  nsCOMPtr<nsIEventQueueService> eventService(do_GetService(kEventQueueServiceCID));
  NS_ENSURE_TRUE(eventService, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(eventService->GetThreadEventQueue(NS_CURRENT_THREAD,
   &mThreadEventQueue), NS_ERROR_FAILURE);

  //XXX make sure plugins have started up. this really needs to
  //be associated with the nsIContentViewerContainer interfaces,
  //not the nsIWebShell interfaces. this is a hack. MMP
  CreatePluginHost(mAllowPlugins);

  WEB_TRACE(WEB_TRACE_CALLS,
            ("nsWebShell::Init: this=%p", this));

  // HACK....force the uri loader to give us a load cookie for this webshell...then get it's
  // doc loader and store it...as more of the docshell lands, we'll be able to get rid
  // of this hack...
  nsCOMPtr<nsIURILoader> uriLoader = do_GetService(NS_URI_LOADER_PROGID);
  uriLoader->GetDocumentLoaderForContext(NS_STATIC_CAST( nsISupports*, (nsIWebShell *) this), &mDocLoader);

  // Set the webshell as the default IContentViewerContainer for the loader...
  mDocLoader->SetContainer(NS_STATIC_CAST(nsIContentViewerContainer*, (nsIWebShell*)this));

  //Register ourselves as an observer for the new doc loader
  mDocLoader->AddObserver((nsIDocumentLoaderObserver*)this);

   return nsDocShell::Create();
}

NS_IMETHODIMP nsWebShell::Destroy()
{
  nsresult rv = NS_OK;

  if (!mFiredUnloadEvent) {
    //Fire unload event before we blow anything away.
    rv = FireUnloadEvent();
  }

  nsDocShell::Destroy();

  SetContainer(nsnull);

  return NS_OK;
}

NS_IMETHODIMP nsWebShell::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   mBounds.SetRect(x, y, cx, cy);
   return nsDocShell::SetPositionAndSize(x, y, cx, cy, fRepaint);   
}

NS_IMETHODIMP nsWebShell::GetPositionAndSize(PRInt32* x, PRInt32* y, 
   PRInt32* cx, PRInt32* cy)
{
   if(x)
      *x = mBounds.x;
   if(y)
      *y = mBounds.y;
   if(cx)
      *cx = mBounds.width;
   if(cy)
      *cy = mBounds.height;
      
   return NS_OK; 
}


//*****************************************************************************
// nsWebShell::nsIDocShell
//*****************************************************************************   

NS_IMETHODIMP nsWebShell::SetDocument(nsIDOMDocument *aDOMDoc, 
   nsIDOMElement *aRootNode)
{
  // The tricky part is bypassing the normal load process and just putting a document into
  // the webshell.  This is particularly nasty, since webshells don't normally even know
  // about their documents

  // (1) Create a document viewer 
  nsCOMPtr<nsIContentViewer> documentViewer;
  nsCOMPtr<nsIDocumentLoaderFactory> docFactory;
  static NS_DEFINE_CID(kLayoutDocumentLoaderFactoryCID, NS_LAYOUT_DOCUMENT_LOADER_FACTORY_CID);
  NS_ENSURE_SUCCESS(nsComponentManager::CreateInstance(kLayoutDocumentLoaderFactoryCID, nsnull, 
                                                       NS_GET_IID(nsIDocumentLoaderFactory),
                                                       (void**)getter_AddRefs(docFactory)),
                    NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDOMDoc);
  if (!doc) { return NS_ERROR_NULL_POINTER; }

  NS_ENSURE_SUCCESS(docFactory->CreateInstanceForDocument(NS_STATIC_CAST(nsIContentViewerContainer*, (nsIWebShell*)this),
                                                          doc,
                                                          "view",
                                                          getter_AddRefs(documentViewer)),
                    NS_ERROR_FAILURE); 

  // (2) Feed the webshell to the content viewer
  NS_ENSURE_SUCCESS(documentViewer->SetContainer((nsIWebShell*)this), NS_ERROR_FAILURE);

  // (3) Tell the content viewer container to embed the content viewer.
  //     (This step causes everything to be set up for an initial flow.)
  NS_ENSURE_SUCCESS(SetupNewViewer(documentViewer), NS_ERROR_FAILURE);

  // XXX: It would be great to get rid of this dummy channel!
  const nsAutoString uriString = "about:blank";
  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), uriString), NS_ERROR_FAILURE);
  if (!uri) { return NS_ERROR_OUT_OF_MEMORY; }

  nsCOMPtr<nsIChannel> dummyChannel;
  NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(dummyChannel), uri, nsnull), NS_ERROR_FAILURE);

  // (4) fire start document load notification
  nsCOMPtr<nsIStreamListener> outStreamListener;  // a valid pointer is required for the returned stream listener
    // XXX: warning: magic cookie!  should get string "view delayedContentLoad"
    //      from somewhere, maybe nsIHTMLDocument?
  NS_ENSURE_SUCCESS(doc->StartDocumentLoad("view delayedContentLoad", dummyChannel, nsnull, NS_STATIC_CAST(nsIContentViewerContainer*, (nsIWebShell*)this),
                                           getter_AddRefs(outStreamListener)), 
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(OnStartDocumentLoad(mDocLoader, uri, "load"), NS_ERROR_FAILURE);

  // (5) hook up the document and its content
  nsCOMPtr<nsIContent> rootContent = do_QueryInterface(aRootNode);
  if (!doc) { return NS_ERROR_OUT_OF_MEMORY; }
  NS_ENSURE_SUCCESS(rootContent->SetDocument(doc, PR_FALSE), NS_ERROR_FAILURE);
  doc->SetRootContent(rootContent);
  rootContent->SetDocument(doc, PR_TRUE);

  // (6) reflow the document
  PRInt32 i;
  PRInt32 ns = doc->GetNumberOfShells();
  for (i = 0; i < ns; i++) 
  {
    nsCOMPtr<nsIPresShell> shell(dont_AddRef(doc->GetShellAt(i)));
    if (shell) 
    {
      // Make shell an observer for next time
      NS_ENSURE_SUCCESS(shell->BeginObservingDocument(), NS_ERROR_FAILURE);

      // Resize-reflow this time
      nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(documentViewer);
      if (!docViewer) { return NS_ERROR_OUT_OF_MEMORY; }
      nsCOMPtr<nsIPresContext> presContext;
      NS_ENSURE_SUCCESS(docViewer->GetPresContext(*(getter_AddRefs(presContext))), NS_ERROR_FAILURE);
      if (!presContext) { return NS_ERROR_OUT_OF_MEMORY; }
      float p2t;
      presContext->GetScaledPixelsToTwips(&p2t);

      nsRect r;
      GetPositionAndSize(&r.x, &r.y, &r.width, &r.height);
      NS_ENSURE_SUCCESS(shell->InitialReflow(NSToCoordRound(r.width * p2t), NSToCoordRound(r.height * p2t)), NS_ERROR_FAILURE);

      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      NS_ENSURE_SUCCESS(shell->GetViewManager(getter_AddRefs(vm)), NS_ERROR_FAILURE);
      if (vm) 
      {
        PRBool enabled;
        documentViewer->GetEnableRendering(&enabled);
        if (enabled) {
          vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
        }
        NS_ENSURE_SUCCESS(vm->SetWindowDimensions(NSToCoordRound(r.width * p2t), 
                                                  NSToCoordRound(r.height * p2t)), 
                          NS_ERROR_FAILURE);
      }
    }
  }

  // (7) fire end document load notification
  mProcessedEndDocumentLoad = PR_FALSE;
  nsresult rv = NS_OK;
  NS_ENSURE_SUCCESS(OnEndDocumentLoad(mDocLoader, dummyChannel, rv), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);  // test the resulting out-param separately

  return NS_OK;
}

NS_IMETHODIMP nsWebShell::GetParentContentListener(nsIURIContentListener** aParent)
{
  return GetParentURIContentListener(aParent);
}

NS_IMETHODIMP nsWebShell::SetParentContentListener(nsIURIContentListener* aParent)
{
  return SetParentURIContentListener(aParent);
}

NS_IMETHODIMP nsWebShell::GetLoadCookie(nsISupports ** aLoadCookie)
{
  NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);
  return mContentListener->GetLoadCookie(aLoadCookie);
}

NS_IMETHODIMP nsWebShell::SetLoadCookie(nsISupports * aLoadCookie)
{
  NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);
  return mContentListener->SetLoadCookie(aLoadCookie);
}

//----------------------------------------------------------------------

// Factory code for creating nsWebShell's

class nsWebShellFactory : public nsIFactory
{
public:
  nsWebShellFactory();
  virtual ~nsWebShellFactory();

  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);
};

nsWebShellFactory::nsWebShellFactory()
{
   NS_INIT_REFCNT();
}

nsWebShellFactory::~nsWebShellFactory()
{
}

NS_IMPL_ADDREF(nsWebShellFactory);
NS_IMPL_RELEASE(nsWebShellFactory);

NS_INTERFACE_MAP_BEGIN(nsWebShellFactory)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
   NS_INTERFACE_MAP_ENTRY(nsIFactory)
NS_INTERFACE_MAP_END

nsresult
nsWebShellFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsWebShell *inst;

  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_NO_AGGREGATION(aOuter);
  *aResult = NULL;

  NS_NEWXPCOM(inst, nsWebShell);
  NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

nsresult
nsWebShellFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

extern "C" NS_WEB nsresult
NS_NewWebShellFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsWebShellFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
