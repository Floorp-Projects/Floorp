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
 *   Ramiro Estrugo <ramiro@eazel.com>
 *	 Brian Edmond <briane@qnx.com>
 */
#include <stdlib.h>
#include <time.h>
#include "PtMozilla.h"
#include "nsIWebBrowser.h"
#include "nsCWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "PhMozEmbedChrome.h"
#include "nsIComponentManager.h"
#include "nsIWebNavigation.h"
#include "nsString.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsEmbedAPI.h"
#include "nsVoidArray.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShell.h"
#include "nsISHistory.h"
#include "jsapi.h"
#include "nsIJSContextStack.h"
#include "nsAppShellCIDs.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"
#include "nsIDOMDocument.h"

// freakin X headers
#ifdef Success
#undef Success
#endif

#ifdef KeyPress
#undef KeyPress
#endif

#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

// forward declaration for our class
class PhMozEmbedPrivate;

class PhMozEmbedListenerImpl : public PhEmbedListener
{
public:
  PhMozEmbedListenerImpl();
  virtual ~PhMozEmbedListenerImpl();
  void     Init(PhMozEmbed *aEmbed);
  nsresult NewBrowser(PRUint32 aChromeFlags,
		      nsIDocShellTreeItem **_retval);
  void     Destroy(void);
  void     Visibility(PRBool aVisibility);
  void     Message(PhEmbedListenerMessageType aType,
		   const char *aMessage);
  PRBool   StartOpen(const char *aURI);
  void     SizeTo(PRInt32 width, PRInt32 height);
private:
  PhMozEmbed *mEmbed;
  PhMozEmbedPrivate *mEmbedPrivate;
};

// this class is a progress listener for the main content area, once
// it has been loaded.

class PhMozEmbedContentProgress : public nsIWebProgressListener
{
public:
  PhMozEmbedContentProgress();
  virtual ~PhMozEmbedContentProgress();
  
  void Init(PhMozEmbed *aEmbed);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIWEBPROGRESSLISTENER

private:
  PhMozEmbed *mEmbed;
  PhMozEmbedPrivate *mEmbedPrivate;
};

// this class is a progress listener for the chrome area
class PhMozEmbedChromeProgress : public nsIWebProgressListener
{
public:
  PhMozEmbedChromeProgress();
  virtual ~PhMozEmbedChromeProgress();
  
  void Init(PhMozEmbed *aEmbed);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIWEBPROGRESSLISTENER

private:
  PhMozEmbed *mEmbed;
  PhMozEmbedPrivate *mEmbedPrivate;
};

class PhMozEmbedChromeEventListener : public nsIDOMKeyListener,
				       public nsIDOMMouseListener
{
public:
  PhMozEmbedChromeEventListener();
  virtual ~PhMozEmbedChromeEventListener();

  void Init (PhMozEmbed *aEmbed);
  
  NS_DECL_ISUPPORTS

  // nsIDOMEventListener

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMKeyListener
  
  NS_IMETHOD KeyDown(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aDOMEvent);

  // nsIDOMMouseListener

  NS_IMETHOD MouseDown(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aDOMEvent);

private:
  PhMozEmbed *mEmbed;
  PhMozEmbedPrivate *mEmbedPrivate;
};

class PhMozEmbedPrivate
{
public:
  PhMozEmbedPrivate();
  ~PhMozEmbedPrivate();

  nsresult    Init                (PhMozEmbed *aEmbed);
  nsresult    Realize             (PtWidget_t *aWidget);
  nsresult    Resize              (PtWidget_t *aWidget);
  nsresult    LoadChrome          (void);
  void        SetCurrentURI       (nsCString &aString);
  void        GetCurrentURI       (nsCString &aString);
  nsresult    OnChromeStateChange (nsIWebProgress *aWebProgress,
				   nsIRequest *aRequest,
				   PRInt32 aStateFlags,
				   PRUint32 aStatus);
  void        Destroy             (void);
  static void RequestToURIString  (nsIRequest *aRequest, char **aString);
  nsresult AddEventListener(void);
  nsresult RemoveEventListener(void);

  nsCOMPtr<nsIWebBrowser>           mWebBrowser;
  nsCOMPtr<nsIPhEmbed>             mEmbed;
  PhMozEmbedListenerImpl           mListener;
  PtWidget_t                        *mMozWindow;
  nsString                          mChromeLocation;
  PRBool                            mChromeLoaded;
  PRBool                            mContentLoaded;
  nsCOMPtr<nsIWebProgressListener>  mChromeProgress;
  nsCOMPtr<nsIWebProgressListener>  mContentProgress;
  nsCOMPtr<nsIWebNavigation>        mContentNav;
  nsCOMPtr<nsIWebNavigation>        mChromeNav;
  nsCOMPtr<nsISHistory>             mSessionHistory;
  nsCOMPtr<nsIDOMEventListener>     mEventListener;
  PRBool                            mEventListenerAttached;

private:
  nsCString		            mCurrentURI;
};

// nsEventQueueStack
// a little utility object to push an event queue and pop it when it
// goes out of scope. should probably be in a file of utility functions.
class nsEventQueueStack
{
public:
   nsEventQueueStack();
   ~nsEventQueueStack();

   nsresult Success();

protected:
   nsCOMPtr<nsIEventQueueService>   mService;
   nsCOMPtr<nsIEventQueue>          mQueue;
};

PhMozEmbedPrivate::PhMozEmbedPrivate(void) : mMozWindow(0), 
  mChromeLoaded(PR_FALSE), mContentLoaded(PR_FALSE),
  mEventListenerAttached(PR_FALSE)
{
}

PhMozEmbedPrivate::~PhMozEmbedPrivate(void)
{
}

nsresult
PhMozEmbedPrivate::Init(PhMozEmbed *aEmbed)
{
  // create an nsIWebBrowser object
  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  NS_ENSURE_TRUE(mWebBrowser, NS_ERROR_FAILURE);

  // create our glue widget
  PhMozEmbedChrome *chrome = new PhMozEmbedChrome();
  NS_ENSURE_TRUE(chrome, NS_ERROR_FAILURE);

  mEmbed = do_QueryInterface(NS_STATIC_CAST(nsISupports *,
					    NS_STATIC_CAST(nsIPhEmbed *,
							   chrome)));
  NS_ENSURE_TRUE(mEmbed, NS_ERROR_FAILURE);

// briane
#if 0
  // hide it
  aEmbed->data = this;
#endif

  // create our content and chrome progress listener objects
  PhMozEmbedContentProgress *newContentProgress =
    new PhMozEmbedContentProgress();
  PhMozEmbedChromeProgress *newChromeProgress =
    new PhMozEmbedChromeProgress();

  // we own it...
  NS_ADDREF(newContentProgress);
  NS_ADDREF(newChromeProgress);
  newContentProgress->Init(aEmbed);
  newChromeProgress->Init(aEmbed);
  mContentProgress = do_QueryInterface(newContentProgress);
  mChromeProgress = do_QueryInterface(newChromeProgress);
  // and the ref in the private struct is the owning ref
  NS_RELEASE(newChromeProgress);
  NS_RELEASE(newContentProgress);

  // create our key listener handler
  PhMozEmbedChromeEventListener *newListener = 
    new PhMozEmbedChromeEventListener();

  // we own it...
  NS_ADDREF(newListener);
  newListener->Init(aEmbed);
  mEventListener = do_QueryInterface(NS_STATIC_CAST(nsISupports *,
				     NS_STATIC_CAST(nsIDOMKeyListener *,
						    newListener)));
  // the ref in the private struct is the owning ref
  NS_RELEASE(newListener);

  // get our hands on the browser chrome
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_QueryInterface(mEmbed);
  NS_ENSURE_TRUE(browserChrome, NS_ERROR_FAILURE);
  // set the container window
  mWebBrowser->SetContainerWindow(browserChrome);
  // set the widget as the owner of the object
  mEmbed->Init(aEmbed);

  // set up the callback object so that it knows how to call back to
  // this widget.
  mListener.Init(aEmbed);
  // set our listener for the chrome
  mEmbed->SetEmbedListener(&mListener);

  // so the embedding widget can find it's owning nsIWebBrowser object
  browserChrome->SetWebBrowser(mWebBrowser);

  // hang on to a copy of the navigation for the web browser
  mChromeNav = do_QueryInterface(mWebBrowser);

  // create our session history object for the inner content area
  mSessionHistory = do_CreateInstance(NS_SHISTORY_CONTRACTID);

  return NS_OK;
}

nsresult
PhMozEmbedPrivate::Realize(PtWidget_t *aWidget)
{
  // before we create the window, we have to set it up to handle
  // chrome content.
  nsCOMPtr<nsIDocShellTreeItem> browserAsItem = do_QueryInterface(mWebBrowser);
  browserAsItem->SetItemType(nsIDocShellTreeItem::typeChromeWrapper);

  // set ourselves up as the tree owner
  nsCOMPtr<nsIDocShellTreeOwner> browserAsOwner;
  browserAsOwner = do_QueryInterface(mEmbed);
  browserAsItem->SetTreeOwner(browserAsOwner.get());

  // now that we're realized, set up the nsIWebBrowser and nsIBaseWindow stuff
  // init our window
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow =
    do_QueryInterface(mWebBrowser);
  NS_ENSURE_TRUE(webBrowserBaseWindow, NS_ERROR_FAILURE);

  PhDim_t dim;
  PtWidgetDim(aWidget, &dim);
  webBrowserBaseWindow->InitWindow(aWidget, NULL, 0, 0,
  					dim.w, dim.h);
  webBrowserBaseWindow->Create();

// briane
#if 0
  // save the id of the mozilla window for later if we need it.
  nsCOMPtr<nsIWidget> mozWidget;
  webBrowserBaseWindow->GetMainWidget(getter_AddRefs(mozWidget));
  // get the native drawing area
  GdkWindow *tmp_window = (GdkWindow *)
    mozWidget->GetNativeData(NS_NATIVE_WINDOW);
  // and, thanks to superwin we actually need the parent of that.
  tmp_window = gdk_window_get_parent(tmp_window);
  mMozWindow = tmp_window;
#endif

  // set our webBrowser object as the content listener object
  nsCOMPtr<nsIURIContentListener> uriListener;
  uriListener = do_QueryInterface(mEmbed);
  NS_ENSURE_TRUE(uriListener, NS_ERROR_FAILURE);
  mWebBrowser->SetParentURIContentListener(uriListener);

  // get the nsIWebProgress object from the chrome docshell
  nsCOMPtr <nsIDocShell> docShell = do_GetInterface(mWebBrowser);
  nsCOMPtr <nsIWebProgress> webProgress;
  webProgress = do_GetInterface(docShell);
  // add our chrome listener object
  webProgress->AddProgressListener(mChromeProgress);

  nsresult rv;
  rv = LoadChrome();

  return rv;
}

nsresult
PhMozEmbedPrivate::Resize(PtWidget_t *aWidget)
{
	PhDim_t dim;
	PtWidgetDim(aWidget, &dim);

  // set the size of the base window
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow =
    do_QueryInterface(mWebBrowser);
  webBrowserBaseWindow->SetPositionAndSize(0, 0, 
  					dim.w, dim.h, PR_TRUE);
  nsCOMPtr<nsIBaseWindow> embedBaseWindow = do_QueryInterface(mEmbed);
  embedBaseWindow->SetPositionAndSize(0, 0,
  					dim.w, dim.h, PR_TRUE);
  return NS_OK;
}

nsresult
PhMozEmbedPrivate::LoadChrome(void)
{
  // First push a nested event queue for event processing from netlib
  // onto our UI thread queue stack.  This has to happen before we
  // realize the widget so that any thread events happen on a thread
  // event queue that isn't already in use in case we are being
  // called from an event handler - say from necko in an onload
  // handler.
  nsEventQueueStack queuePusher;
  NS_ENSURE_SUCCESS(queuePusher.Success(), NS_ERROR_FAILURE);

  nsCOMPtr<nsIAppShell> subShell(do_CreateInstance(kAppShellCID));
  NS_ENSURE_TRUE(subShell, NS_ERROR_FAILURE);

  mChromeLocation.AssignWithConversion("chrome://embed/content/simple-shell.xul");
  mChromeNav->LoadURI(mChromeLocation.GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
  
  subShell->Create(0, nsnull);
  subShell->Spinup();

  // Push nsnull onto the JSContext stack before we dispatch a native event.
  nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
  if(stack && NS_SUCCEEDED(stack->Push(nsnull)))
  {
    // until both the content and chrome areas are loaded, don't
    // return.
    nsresult looprv = NS_OK;
    while (NS_SUCCEEDED(looprv) && !mContentLoaded)
    {
      // technically this is just g_main_iteration() but in the hopes
      // of making this XP in the future...
      void      *data;
      PRBool    isRealEvent;
      
      looprv = subShell->GetNativeEvent(isRealEvent, data);
      subShell->DispatchNativeEvent(isRealEvent, data);
    }
    JSContext *cx;
    stack->Pop(&cx);
    NS_ASSERTION(cx == nsnull, "JSContextStack mismatch");

  }
    
  // and spindown...
  subShell->Spindown();

  return NS_OK;
}

void
PhMozEmbedPrivate::GetCurrentURI(nsCString &aString)
{
  aString.Assign(mCurrentURI);
}

void
PhMozEmbedPrivate::SetCurrentURI(nsCString &aString)
{
  mCurrentURI.Assign(aString);
}

nsresult
PhMozEmbedPrivate::OnChromeStateChange(nsIWebProgress *aWebProgress,
					nsIRequest *aRequest,
					PRInt32 aStateFlags,
					PRUint32 aStatus)
{
  // XXX add shortcut here for content + chrome already loaded

  if ((aStateFlags & Pt_MOZ_FLAG_IS_DOCUMENT) && 
      (aStateFlags & Pt_MOZ_FLAG_STOP))
  {
    nsXPIDLCString uriString;
    nsCString      chromeString;
    // get the original URI for this
    nsCOMPtr <nsIChannel> channel = do_QueryInterface(aRequest);
    nsCOMPtr <nsIURI>     origURI;
    channel->GetOriginalURI(getter_AddRefs(origURI));
    origURI->GetSpec(getter_Copies(uriString));
    chromeString.AssignWithConversion(mChromeLocation);
    // check to see if this was our chrome that was finished loading.
    if (chromeString.Equals(uriString))
    {
      if (!mChromeLoaded)
	mChromeLoaded = PR_TRUE;
    }
    // if the chrome has been loaded but the content hasn't been
    // loaded yet, try to load it.
    if (mChromeLoaded && !mContentLoaded)
    {
      
      // get the browser as an item
      nsCOMPtr<nsIDocShellTreeItem> browserAsItem;
      browserAsItem = do_QueryInterface(mWebBrowser);
      if (!browserAsItem)
      {
	NS_ASSERTION(0, "failed to get browser as item");
	return NS_OK;
      }
      // get the owner for that item
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      browserAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
      if (!treeOwner)
      {
	NS_ASSERTION(0, "failed to get tree owner");
	return NS_OK;
      }
      // get the primary content shell as an item
      nsCOMPtr<nsIDocShellTreeItem> contentItem;
      treeOwner->GetPrimaryContentShell(getter_AddRefs(contentItem));
      if (!contentItem)
      {
	// apparently our primary content area hasn't been loaded.
	// tis ok
	printf("Warning: Failed to find primary content shell!  I will try again later.\n");
	return NS_OK;
      }
      // if we made it this far, our primary content shell has been
      // loaded.  Mark our flag.
      mContentLoaded = PR_TRUE;
      // attach our chrome object as the tree owner for the content
      // docShell.  this is so that we get JS and link over
      // notifications.
      contentItem->SetTreeOwner(treeOwner);
      // get the docshell for that item
      nsCOMPtr <nsIDocShell> docShell;
      docShell = do_QueryInterface(contentItem);
      if (!docShell)
      {
	NS_ASSERTION(0, "failed to get docshell");
	return NS_OK;
      }
      // get the nsIWebProgress object for that docshell
      nsCOMPtr <nsIWebProgress> webProgress;
      webProgress = do_GetInterface(docShell);
      if (!webProgress)
      {
	NS_ASSERTION(0, "Failed to get webProgress object from content area docShell");
	return NS_OK;
      }
      // attach the mContentProgress object to that docshell
      webProgress->AddProgressListener(mContentProgress);
      // get the navigation for the content area
      mContentNav = do_QueryInterface(docShell);
      if (!mContentNav)
      {
	NS_ASSERTION(0, "Failed to get mContentNav object from content area docShell");
	return NS_OK;
      }
      // attach our session history object to the content area
      mContentNav->SetSessionHistory(mSessionHistory);
      // now that the content is loaded, attach our key listener to
      // the chrome document

      AddEventListener();

      // check to see whether or not we should load a new uri.
      if (mCurrentURI.Length() > 0)
      {
	nsString tmpString;
	tmpString.AssignWithConversion(mCurrentURI);
	mContentNav->LoadURI(tmpString.GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
      }
    }
  }
  return NS_OK;
}

void
PhMozEmbedPrivate::Destroy(void)
{
  // remove ourselves as the parent URI content listener
  mWebBrowser->SetParentURIContentListener(nsnull);
  
  // remove ourselves as the progress listener for the chrome object
  nsCOMPtr <nsIDocShell> docShell = do_GetInterface(mWebBrowser);
  nsCOMPtr <nsIWebProgress> webProgress;
  webProgress = do_GetInterface(docShell);
  webProgress->RemoveProgressListener(mChromeProgress);

  // remove our key press listener
  RemoveEventListener();

  // get the content item
  // get the browser as an item
  nsCOMPtr<nsIDocShellTreeItem> browserAsItem;
  browserAsItem = do_QueryInterface(mWebBrowser);
  // get the owner for that item
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  browserAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ASSERTION(treeOwner, "failed to get tree owner");
  // get the primary content shell as an item
  nsCOMPtr<nsIDocShellTreeItem> contentItem;
  treeOwner->GetPrimaryContentShell(getter_AddRefs(contentItem));
  NS_ASSERTION(contentItem, "failed to get content item");
  // remove our chrome object as the tree owner for the content
  // docShell.
  contentItem->SetTreeOwner(nsnull);
  // get the nsIWebProgress object for that docshell
  webProgress = do_GetInterface(docShell);
  // remove the mContentProgress object from that docshell
  webProgress->RemoveProgressListener(mContentProgress);
  
  // now that we have removed ourselves as the tree owner for the
  // content item, remove ourselves as the tree owner for the
  // browser
  browserAsItem = do_QueryInterface(mWebBrowser);
  browserAsItem->SetTreeOwner(nsnull);
  
  // destroy the native windows
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = 
    do_QueryInterface(mWebBrowser);
  webBrowserBaseWindow->Destroy();
  
  mMozWindow = 0;
  mWebBrowser = nsnull;
  mEmbed = nsnull;
}

/* static */
void
PhMozEmbedPrivate::RequestToURIString  (nsIRequest *aRequest, char **aString)
{
  // is it a channel
  nsCOMPtr<nsIChannel> channel;
  channel = do_QueryInterface(aRequest);
  if (!channel)
    return;
  
  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));
  if (!uri)
    return;
  
  nsXPIDLCString uriString;
  uri->GetSpec(getter_Copies(uriString));
  if (!uriString)
    return;

  *aString = nsCRT::strdup((const char *)uriString);
}

nsresult
PhMozEmbedPrivate::AddEventListener(void)
{
  // get the DOM document
  nsCOMPtr <nsIDOMDocument> domDoc;
  mChromeNav->GetDocument(getter_AddRefs(domDoc));
  if (!domDoc)
  {
    NS_ASSERTION(0, "Failed to get docuement from mChromeNav\n");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMEventReceiver> eventReceiver;
  // get the nsIDOMEventReceiver
  eventReceiver = do_QueryInterface(domDoc);
  if (!eventReceiver)
  {
    NS_ASSERTION(0, "failed to get event receiver\n");
    return NS_ERROR_FAILURE;
  }
  // add ourselves as a key listener
  nsresult rv = NS_OK;
  rv = eventReceiver->AddEventListenerByIID(mEventListener,
					    NS_GET_IID(nsIDOMKeyListener));
  if (NS_FAILED(rv))
  {
    NS_ASSERTION(0, "failed to add event receiver\n");
    return NS_ERROR_FAILURE;
  }
  rv = eventReceiver->AddEventListenerByIID(mEventListener,
					    NS_GET_IID(nsIDOMMouseListener));
  if (NS_FAILED(rv))
  {
    NS_ASSERTION(0, "failed to add event receiver\n");
    return NS_ERROR_FAILURE;
  }
  // set our flag
  mEventListenerAttached = PR_TRUE;
  return NS_OK;
}

nsresult
PhMozEmbedPrivate::RemoveEventListener(void)
{
  // check to see if it's attached
  if (!mEventListenerAttached)
    return NS_OK;

  // get the DOM document
  nsCOMPtr <nsIDOMDocument> domDoc;
  mChromeNav->GetDocument(getter_AddRefs(domDoc));
  if (!domDoc)
  {
    NS_ASSERTION(0, "Failed to get docuement from mChromeNav\n");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMEventReceiver> eventReceiver;
  // get the nsIDOMEventReceiver
  eventReceiver = do_QueryInterface(domDoc);
  if (!eventReceiver)
  {
    NS_ASSERTION(0, "failed to get event receiver\n");
    return NS_ERROR_FAILURE;
  }
  // add ourselves as a key listener
  nsresult rv = NS_OK;
  rv = eventReceiver->RemoveEventListenerByIID(mEventListener,
					       NS_GET_IID(nsIDOMKeyListener));
  if (NS_FAILED(rv))
  {
    NS_ASSERTION(0, "failed to remove event receiver\n");
    return NS_ERROR_FAILURE;
  }
  rv = eventReceiver->RemoveEventListenerByIID(mEventListener,
					       NS_GET_IID(nsIDOMMouseListener));
  if (NS_FAILED(rv))
  {
    NS_ASSERTION(0, "failed to remove event receiver\n");
    return NS_ERROR_FAILURE;
  }
  // set our flag
  mEventListenerAttached = PR_FALSE;
  return NS_OK;
}


// this is a class that implements the callbacks from the chrome
// object

PhMozEmbedListenerImpl::PhMozEmbedListenerImpl(void)
{
  mEmbed = nsnull;
  mEmbedPrivate = nsnull;
}

PhMozEmbedListenerImpl::~PhMozEmbedListenerImpl(void)
{
}

void
PhMozEmbedListenerImpl::Init(PhMozEmbed *aEmbed)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)aEmbed;
  	mEmbed = aEmbed;
  	mEmbedPrivate = (PhMozEmbedPrivate *)moz->embed_private;
}

nsresult
PhMozEmbedListenerImpl::NewBrowser(PRUint32 chromeMask,
				    nsIDocShellTreeItem **_retval)
{
  	PhMozEmbed *newEmbed = NULL;
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaNewWindowCb_t nwin;

	if (!moz->new_window_cb)
		return NS_OK;

	memset(&cbinfo, 0, sizeof(cbinfo));
	memset(&nwin, 0, sizeof(PtMozillaNewWindowCb_t));
	cbinfo.cbdata = &nwin;
	nwin.window_flags = chromeMask;
	cbinfo.reason = Pt_CB_MOZ_NEW_WINDOW;
	cb = moz->new_window_cb;
	PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);

	if (nwin.widget)  
	{
		//newEmbed = nwin.widget;
		moz = (PtMozillaWidget_t *) nwin.widget;
		PhMozEmbedPrivate *embed_private = moz->embed_private;

		// now that the content area has been loaded, get the
		// nsIDocShellTreeItem for the content area
		// get the browser as an item
		nsCOMPtr<nsIDocShellTreeItem> browserAsItem;
		browserAsItem = do_QueryInterface(embed_private->mWebBrowser);
		// get the owner for that item
		nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
		browserAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
		NS_ASSERTION(treeOwner, "failed to get tree owner");
		// get the primary content shell as an item
		return treeOwner->GetPrimaryContentShell(_retval);
	}

  return NS_OK;
}

void
PhMozEmbedListenerImpl::Destroy(void)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;

	if (!moz->destroy_cb)
		return;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_DESTROY;
	cb = moz->destroy_cb;
	PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);
}

void
PhMozEmbedListenerImpl::Visibility(PRBool aVisibility)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;

	if (!moz->destroy_cb)
		return;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_VISIBILITY;
	cb = moz->visibility_cb;
	PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);
}

void
PhMozEmbedListenerImpl::Message(PhEmbedListenerMessageType aType,
				 const char *aMessage)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaStatusCb_t status;

	if (!moz->status_cb)
		return;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.cbdata = &status;
	cbinfo.reason = Pt_CB_MOZ_STATUS;
	cb = moz->status_cb;

	switch (aType) 
	{
		case MessageLink:
			status.type = Pt_MOZ_STATUS_LINK;
			break;
		case MessageJSStatus:
			status.type = Pt_MOZ_STATUS_JS;
			break;
		case MessageTitle:
			status.type = Pt_MOZ_STATUS_TITLE;
			break;
		default:
			return;
	}
	status.message = (char *)aMessage;
	PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);
}

PRBool
PhMozEmbedListenerImpl::StartOpen(const char *aURI)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaUrlCb_t url;

  	// if our chrome hasn't been loaded then it's all OK.
  	if (!mEmbedPrivate->mChromeLoaded)
    	return PR_FALSE;
  	int return_val = PR_FALSE;

  	if (!moz->open_cb)
  		return return_val;

	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.cbdata = &url;
	cbinfo.reason = Pt_CB_MOZ_OPEN;
	cb = moz->open_cb;
	url.url = (char *)aURI;
	if (PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo) == Pt_END)
		return_val = PR_TRUE;

  	return return_val;
}

void
PhMozEmbedListenerImpl::SizeTo(PRInt32 width, PRInt32 height)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaResizeCb_t resize;

	if (!moz->resize_cb)
		return;

	memset(&cbinfo, 0, sizeof(cbinfo));
	resize.dim.w = width;
	resize.dim.h = height;
	cbinfo.cbdata = &resize;
	cbinfo.reason = Pt_CB_MOZ_RESIZE;
	cb = moz->new_window_cb;
	PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);
}

PhMozEmbedContentProgress::PhMozEmbedContentProgress(void)
{
  NS_INIT_REFCNT();
  mEmbed = nsnull;
  mEmbedPrivate = nsnull;
}

PhMozEmbedContentProgress::~PhMozEmbedContentProgress(void)
{
}

NS_IMPL_ADDREF(PhMozEmbedContentProgress)
NS_IMPL_RELEASE(PhMozEmbedContentProgress)
NS_INTERFACE_MAP_BEGIN(PhMozEmbedContentProgress)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END

void
PhMozEmbedContentProgress::Init(PhMozEmbed *aEmbed)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)aEmbed;
	mEmbed = aEmbed;
	mEmbedPrivate = (PhMozEmbedPrivate *)moz->embed_private;
}

NS_IMETHODIMP
PhMozEmbedContentProgress::OnStateChange(nsIWebProgress *aWebProgress,
					  nsIRequest *aRequest,
					  PRInt32 aStateFlags,
					  PRUint32 aStatus)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
	PtCallbackList_t *cb = NULL;
	PtCallbackInfo_t cbinfo;
	PtMozillaNetStateCb_t	state;

	nsXPIDLCString uriString;
	PhMozEmbedPrivate::RequestToURIString(aRequest, getter_Copies(uriString));
	nsCString currentURI;
	mEmbedPrivate->GetCurrentURI(currentURI);

	memset(&cbinfo, 0, sizeof(cbinfo));
	if ((aStateFlags & Pt_MOZ_FLAG_IS_DOCUMENT) &&
	    (aStateFlags & Pt_MOZ_FLAG_START))
	{
		cbinfo.reason = Pt_CB_MOZ_START;
		if (cb = moz->start_cb)
			PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);
	}

	if (cb = moz->net_state_cb)
	{
		cbinfo.reason = Pt_CB_MOZ_NET_STATE;
		cbinfo.cbdata = &state;
		state.flags = aStateFlags;
		state.status = aStatus;
			PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);
		cbinfo.cbdata = NULL;
	}

	if ((aStateFlags & Pt_MOZ_FLAG_IS_DOCUMENT) &&
	    (aStateFlags & Pt_MOZ_FLAG_STOP))
	{
		cbinfo.reason = Pt_CB_MOZ_COMPLETE;
		if (cb = moz->complete_cb)
			PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);
	}

  	return NS_OK;
}

NS_IMETHODIMP
PhMozEmbedContentProgress::OnProgressChange(nsIWebProgress *aWebProgress, 
					     nsIRequest *aRequest, 
					     PRInt32 aCurSelfProgress,
					     PRInt32 aMaxSelfProgress,
					     PRInt32 aCurTotalProgress, 
					     PRInt32 aMaxTotalProgress)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaProgressCb_t	prog;

	if (!moz->progress_cb)
		return NS_OK;

	nsXPIDLCString uriString;
	PhMozEmbedPrivate::RequestToURIString(aRequest, getter_Copies(uriString));
	nsCString currentURI;
	mEmbedPrivate->GetCurrentURI(currentURI);

	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_PROGRESS;
	cbinfo.cbdata = &prog;
	cb = moz->progress_cb;
  	if (currentURI.Equals(uriString)) 
  		prog.type = Pt_MOZ_PROGRESS;
  	else
  		prog.type = Pt_MOZ_PROGRESS_ALL;
  	prog.cur = aCurTotalProgress;
  	prog.max = aMaxTotalProgress;

	PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);
  
  	return NS_OK;
}

NS_IMETHODIMP
PhMozEmbedContentProgress::OnLocationChange(nsIWebProgress *aWebProgress,
					     nsIRequest *aRequest,
					     nsIURI *aLocation)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaUrlCb_t	url;

  	nsXPIDLCString newURI;
  	NS_ENSURE_ARG_POINTER(aLocation);
  	aLocation->GetSpec(getter_Copies(newURI));
  	// let the chrome listener know that the location has changed
  	nsCString newURIString;
  	newURIString.Assign(newURI);
  	mEmbedPrivate->SetCurrentURI(newURIString);

  	if (!moz->url_cb)
  		return NS_OK;

	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.cbdata = &url;
	cbinfo.reason = Pt_CB_MOZ_URL;
	cb = moz->url_cb;
	url.url = (char *)newURIString;
	PtInvokeCallbackList(cb, (PtWidget_t *)mEmbed, &cbinfo);

  	return NS_OK;
}

NS_IMETHODIMP
PhMozEmbedContentProgress::OnStatusChange(nsIWebProgress *aWebProgress,
					   nsIRequest *aRequest,
					   nsresult aStatus, 
					   const PRUnichar *aMessage)
{
  return NS_OK;
}




NS_IMETHODIMP 
PhMozEmbedContentProgress::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                             nsIRequest *aRequest, 
                                             PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


PhMozEmbedChromeProgress::PhMozEmbedChromeProgress(void)
{
  NS_INIT_REFCNT();
  mEmbed = nsnull;
  mEmbedPrivate = nsnull;
}

PhMozEmbedChromeProgress::~PhMozEmbedChromeProgress(void)
{
}

NS_IMPL_ADDREF(PhMozEmbedChromeProgress)
NS_IMPL_RELEASE(PhMozEmbedChromeProgress)
NS_INTERFACE_MAP_BEGIN(PhMozEmbedChromeProgress)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END

void
PhMozEmbedChromeProgress::Init(PhMozEmbed *aEmbed)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)aEmbed;
	mEmbed = aEmbed;
	mEmbedPrivate = (PhMozEmbedPrivate *)moz->embed_private;
}

NS_IMETHODIMP
PhMozEmbedChromeProgress::OnStateChange(nsIWebProgress *aWebProgress,
					 nsIRequest *aRequest,
					 PRInt32 aStateFlags,
					 PRUint32 aStatus)
{
  return mEmbedPrivate->OnChromeStateChange(aWebProgress,
					    aRequest,
					    aStateFlags,
					    aStatus);
}

NS_IMETHODIMP
PhMozEmbedChromeProgress::OnProgressChange(nsIWebProgress *aWebProgress, 
					     nsIRequest *aRequest, 
					     PRInt32 aCurSelfProgress,
					     PRInt32 aMaxSelfProgress,
					     PRInt32 aCurTotalProgress, 
					     PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
PhMozEmbedChromeProgress::OnLocationChange(nsIWebProgress *aWebProgress,
					    nsIRequest *aRequest,
					    nsIURI *aLocation)
{
  return NS_OK;
}

NS_IMETHODIMP
PhMozEmbedChromeProgress::OnStatusChange(nsIWebProgress *aWebProgress,
					   nsIRequest *aRequest,
					   nsresult aStatus, 
					   const PRUnichar *aMessage)
{
  return NS_OK;
}


NS_IMETHODIMP 
PhMozEmbedChromeProgress::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                            nsIRequest *aRequest, 
                                            PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

PhMozEmbedChromeEventListener::PhMozEmbedChromeEventListener(void)
{
  NS_INIT_REFCNT();
  mEmbed = nsnull;
  mEmbedPrivate = nsnull;
}

PhMozEmbedChromeEventListener::~PhMozEmbedChromeEventListener(void)
{
}

NS_IMPL_ADDREF(PhMozEmbedChromeEventListener)
NS_IMPL_RELEASE(PhMozEmbedChromeEventListener)
NS_INTERFACE_MAP_BEGIN(PhMozEmbedChromeEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
NS_INTERFACE_MAP_END

void
PhMozEmbedChromeEventListener::Init(PhMozEmbed *aEmbed)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)aEmbed;
	mEmbed = aEmbed;
	mEmbedPrivate = (PhMozEmbedPrivate *)moz->embed_private;
}

// All of the event listeners below return NS_OK to indicate that the
// event should not be consumed in the default case.

NS_IMETHODIMP
PhMozEmbedChromeEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

int _invoke_event_cb(PtMozillaWidget_t *moz, int type)
{
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaEventCb_t	event;

	if (!moz->event_cb)
		return (0);
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_EVENT;
	cbinfo.cbdata = &event;
	event.type = type;
	cb = moz->event_cb;
	PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);

	return (0);
}

NS_IMETHODIMP
PhMozEmbedChromeEventListener::KeyDown(nsIDOMEvent* aDOMEvent)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
  	nsCOMPtr <nsIDOMKeyEvent> keyEvent;
  	keyEvent = do_QueryInterface(aDOMEvent);
  	if (!keyEvent || !moz->event_cb)
    	return NS_OK;
  	// return NS_OK to this function to mark this event as not
  	// consumed...
  	int return_val = NS_OK;

	_invoke_event_cb(moz, Ph_EV_KEY);

  	return return_val;
}

NS_IMETHODIMP
PhMozEmbedChromeEventListener::KeyUp(nsIDOMEvent* aDOMEvent)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
  nsCOMPtr <nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aDOMEvent);
  if (!keyEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  int return_val = NS_OK;

  _invoke_event_cb(moz, Ph_EV_KEY);

  return return_val;
}

NS_IMETHODIMP
PhMozEmbedChromeEventListener::KeyPress(nsIDOMEvent* aDOMEvent)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
  nsCOMPtr <nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aDOMEvent);
  if (!keyEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  int return_val = NS_OK;

  _invoke_event_cb(moz, Ph_EV_KEY);

  return return_val;
}

NS_IMETHODIMP
PhMozEmbedChromeEventListener::MouseDown(nsIDOMEvent* aDOMEvent)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  int return_val = NS_OK;
  _invoke_event_cb(moz, Ph_EV_BUT_PRESS);
  return return_val;
}

NS_IMETHODIMP
PhMozEmbedChromeEventListener::MouseUp(nsIDOMEvent* aDOMEvent)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  int return_val = NS_OK;
  _invoke_event_cb(moz, Ph_EV_BUT_RELEASE);
  return return_val;
}

NS_IMETHODIMP
PhMozEmbedChromeEventListener::MouseClick(nsIDOMEvent* aDOMEvent)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  int return_val = NS_OK;
  _invoke_event_cb(moz, Ph_EV_BUT_PRESS);
  return return_val;
}

NS_IMETHODIMP
PhMozEmbedChromeEventListener::MouseDblClick(nsIDOMEvent* aDOMEvent)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  int return_val = NS_OK;
  _invoke_event_cb(moz, Ph_EV_BUT_PRESS);
  return return_val;
}

NS_IMETHODIMP
PhMozEmbedChromeEventListener::MouseOver(nsIDOMEvent* aDOMEvent)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  int return_val = NS_OK;
  _invoke_event_cb(moz, Ph_EV_PTR_MOTION_NOBUTTON);
  return return_val;
}

NS_IMETHODIMP
PhMozEmbedChromeEventListener::MouseOut(nsIDOMEvent* aDOMEvent)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)mEmbed;
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  int return_val = NS_OK;
  _invoke_event_cb(moz, Ph_EV_BOUNDARY);
  return return_val;
}

//*****************************************************************************
//*** nsEventQueueStack: Object Implementation
//*****************************************************************************   

nsEventQueueStack::nsEventQueueStack() : mQueue(nsnull)
{
   mService = do_GetService(kEventQueueServiceCID);

   if(mService)
      mService->PushThreadEventQueue(getter_AddRefs(mQueue));
}
nsEventQueueStack::~nsEventQueueStack()
{
   if(mQueue)
      mService->PopThreadEventQueue(mQueue);
   mService = nsnull;
}

nsresult nsEventQueueStack::Success()
{
   return mQueue ? NS_OK : NS_ERROR_FAILURE; 
}

////////////////////////////////////////////////////////////////////

PtWidgetClass_t *PtCreateMozillaClass( void );
#ifndef _PHSLIB
	PtWidgetClassRef_t __PtMozilla = { NULL, PtCreateMozillaClass };
	PtWidgetClassRef_t *PtMozilla = &__PtMozilla; 
#endif

static char *component_path = 0;
static int  num_widgets = 0;
nsCOMPtr <nsIAppShell> gAppShell;

int _mozilla_embed_startup_xpcom(void)
{
  nsresult rv;
  //init our embedding
  nsCOMPtr<nsILocalFile> binDir;
  
  if (component_path) 
  {
  	rv = NS_NewLocalFile(component_path, 1, getter_AddRefs(binDir));
  	if (NS_FAILED(rv))
      return FALSE;
  }
    
  rv = NS_InitEmbedding(binDir, nsnull);
  if (NS_FAILED(rv))
    return FALSE;

  gAppShell = do_CreateInstance(kAppShellCID);
  NS_ENSURE_TRUE(gAppShell, NS_ERROR_FAILURE);

  gAppShell->Create(0, nsnull);
  gAppShell->Spinup();

  return TRUE;
}

// defaults
static void mozilla_defaults( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	PtBasicWidget_t *basic = (PtBasicWidget_t *) widget;

  	// before we do anything else we need to fire up XPCOM
  	if (num_widgets == 0)
  	{
    	int retval;
    	retval = _mozilla_embed_startup_xpcom();
    	if (retval != TRUE)
    		printf("moz_embed_startup_xpcom(): FAILED\n");
  	}
  	// increment the number of widgets
  	num_widgets++;

 	// PhMozEmbedPrivate *embed_private;
  	// create our private struct
  	moz->embed_private = new PhMozEmbedPrivate();

  	// initialize it
  	nsresult rv;
  	rv = moz->embed_private->Init((PtWidget_t *)moz);
  	if (!NS_SUCCEEDED(rv))
  		printf("moz->embed_private->Init(moz): FAILED\n");

  	// widget related
	basic->flags = Pt_ALL_OUTLINES | Pt_ALL_BEVELS | Pt_FLAT_FILL;
	widget->resize_flags &= ~Pt_RESIZE_XY_BITS; // fixed size.
	widget->anchor_flags = Pt_TOP_ANCHORED_TOP | Pt_LEFT_ANCHORED_LEFT | \
			Pt_BOTTOM_ANCHORED_TOP | Pt_RIGHT_ANCHORED_LEFT | Pt_ANCHORS_INVALID;
	strcpy(moz->url, "www.qnx.com");
}

static void _mozilla_embed_load_url(PtMozillaWidget_t *moz, char *url)
{
	nsCString newURI;
	newURI.Assign(url);
	moz->embed_private->SetCurrentURI(newURI);

  	// If the widget isn't realized, just return.
	if (!(PtWidgetFlags((PtWidget_t *)moz) & Pt_REALIZED))
		return;

	nsString uriString;
	uriString.AssignWithConversion(newURI);
	if (moz->embed_private->mContentNav)
		moz->embed_private->mContentNav->LoadURI(uriString.GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
}

// realized
static void mozilla_realized( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
  	PhMozEmbedPrivate *embed_private;
  	int                attributes_mask;

	PtSuperClassRealized(PtContainer, widget);

#if 0
  // check to see if we have to create the offscreen window
  if (!offscreen_window) {
    gtk_moz_embed_create_offscreen_window();
  }
#endif

  embed_private = (PhMozEmbedPrivate *)moz->embed_private;

  nsresult rv;
  rv = embed_private->Realize(widget);
  if (!NS_SUCCEEDED(rv))
  	printf("embed_private->Realize(widget): FAILED\n");
  
  // get our hands on the base window
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = 
    do_QueryInterface(embed_private->mWebBrowser);
  if (!webBrowserBaseWindow)
  	printf("webBrowserBaseWindow: FAILED\n");
  // show it
  webBrowserBaseWindow->SetVisibility(PR_TRUE);


  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_QueryInterface(embed_private->mEmbed);
  browserChrome->SetChromeFlags(Pt_MOZ_FLAG_ALLCHROME);

	// If an initial url was stored, load it
  if (moz->url[0])
		_mozilla_embed_load_url(moz, moz->url);
}


// unrealized function
static void mozilla_unrealized( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
}

static void mozilla_extent(PtWidget_t *widget)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	PtSuperClassExtent(PtContainer, widget);

	moz->embed_private->Resize(widget);
}


static void mozilla_modify( PtWidget_t *widget, PtArg_t const *argt )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;

	switch( argt->type ) 
	{
		case Pt_ARG_MOZ_GET_URL:
			_mozilla_embed_load_url(moz, (char *)(argt->value));
			break;
		case Pt_ARG_MOZ_NAVIGATE_PAGE:
			if (moz->embed_private->mContentNav)
			{
				if (argt->len == WWW_DIRECTION_FWD)
					moz->embed_private->mContentNav->GoForward();
				else
					moz->embed_private->mContentNav->GoBack();
			}
			break;
		case Pt_ARG_MOZ_STOP:
			if (moz->embed_private->mContentNav)
				moz->embed_private->mContentNav->Stop();
			break;
		case Pt_ARG_MOZ_RELOAD:
			if (moz->embed_private->mContentNav)
    			moz->embed_private->mContentNav->Reload(0);
			break;
		default:
			return;
	}

	return;
}

static int mozilla_get_info(PtWidget_t *widget, PtArg_t *argt)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	int retval, value = 0;

	switch (argt->type)
	{
		case Pt_ARG_MOZ_NAVIGATE_PAGE:
			moz->embed_private->mContentNav->GetCanGoBack(&retval);
			if (retval)
				moz->navigate_flags |= WWW_DIRECTION_BACK;
			moz->embed_private->mContentNav->GetCanGoForward(&retval);
			if (retval)
				moz->navigate_flags |= WWW_DIRECTION_FWD;
			*((int **)argt->value) = &moz->navigate_flags;
			break;
	}

	return (Pt_CONTINUE);
}

// PtMozilla class creation function
PtWidgetClass_t *PtCreateMozillaClass( void )
{
	static const PtResourceRec_t resources[] =
	{
		{ Pt_ARG_MOZ_GET_URL,           mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_NAVIGATE_PAGE,     mozilla_modify, mozilla_get_info },
		{ Pt_ARG_MOZ_RELOAD,            mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_STOP,              mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_CB_MOZ_STATUS,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, status_cb) },
		{ Pt_CB_MOZ_START,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, start_cb) },
		{ Pt_CB_MOZ_COMPLETE,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, complete_cb) },
		{ Pt_CB_MOZ_PROGRESS,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, progress_cb) },
		{ Pt_CB_MOZ_URL,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, url_cb) },
		{ Pt_CB_MOZ_EVENT,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, event_cb) },
		{ Pt_CB_MOZ_NET_STATE,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, net_state_cb) },
		{ Pt_CB_MOZ_NEW_WINDOW,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, new_window_cb) },
		{ Pt_CB_MOZ_RESIZE,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, resize_cb) },
		{ Pt_CB_MOZ_DESTROY,		NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, destroy_cb) },
		{ Pt_CB_MOZ_VISIBILITY,		NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, visibility_cb) },
		{ Pt_CB_MOZ_OPEN,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, open_cb) },
	};

	static const PtArg_t args[] = 
	{
		{ Pt_SET_VERSION, 200},
		{ Pt_SET_STATE_LEN, sizeof( PtMozillaWidget_t ) },
		{ Pt_SET_DFLTS_F, (long)mozilla_defaults },
		{ Pt_SET_REALIZED_F, (long)mozilla_realized },
		{ Pt_SET_UNREALIZE_F, (long)mozilla_unrealized },
		{ Pt_SET_EXTENT_F, (long)mozilla_extent },
		{ Pt_SET_FLAGS, Pt_RECTANGULAR, Pt_RECTANGULAR },
		{ Pt_SET_RESOURCES, (long) resources },
		{ Pt_SET_NUM_RESOURCES, sizeof( resources )/sizeof( resources[0] ) },
		{ Pt_SET_DESCRIPTION, (long) "PtMozilla" },
	};

	return( PtMozilla->wclass = PtCreateWidgetClass(
		PtContainer, 0, sizeof( args )/sizeof( args[0] ), args ) );
}

void
_moz_embed_open_stream (PtWidget_t *embed,
                           const char *base_uri, const char *mime_type)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) embed;
  PhMozEmbedPrivate *embed_private = moz->embed_private;

  embed_private->mEmbed->OpenStream(base_uri, mime_type);
}

void
_moz_embed_append_data (PtWidget_t *embed, const char *data, uint32 len)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) embed;
  PhMozEmbedPrivate *embed_private = moz->embed_private;

  embed_private->mEmbed->AppendToStream(data, len);
}

void
_moz_embed_close_stream     (PtWidget_t *embed)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) embed;
  PhMozEmbedPrivate *embed_private = moz->embed_private;

  embed_private->mEmbed->CloseStream();
}
