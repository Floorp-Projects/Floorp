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
 */
#include <stdlib.h>
#include <time.h>
#include "gtkmozembed.h"
#include "gtkmozembed_internal.h"
#include "nsIWebBrowser.h"
#include "nsCWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "GtkMozEmbedChrome.h"
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
class GtkMozEmbedPrivate;

class GtkMozEmbedListenerImpl : public GtkEmbedListener
{
public:
  GtkMozEmbedListenerImpl();
  virtual ~GtkMozEmbedListenerImpl();
  void     Init(GtkMozEmbed *aEmbed);
  nsresult NewBrowser(PRUint32 aChromeFlags,
		      nsIDocShellTreeItem **_retval);
  void     Destroy(void);
  void     Visibility(PRBool aVisibility);
  void     Message(GtkEmbedListenerMessageType aType,
		   const char *aMessage);
  PRBool   StartOpen(const char *aURI);
  void     SizeTo(PRInt32 width, PRInt32 height);
private:
  GtkMozEmbed *mEmbed;
  GtkMozEmbedPrivate *mEmbedPrivate;
};

// this class is a progress listener for the main content area, once
// it has been loaded.

class GtkMozEmbedContentProgress : public nsIWebProgressListener
{
public:
  GtkMozEmbedContentProgress();
  virtual ~GtkMozEmbedContentProgress();
  
  void Init(GtkMozEmbed *aEmbed);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIWEBPROGRESSLISTENER

private:
  GtkMozEmbed *mEmbed;
  GtkMozEmbedPrivate *mEmbedPrivate;
};

// this class is a progress listener for the chrome area
class GtkMozEmbedChromeProgress : public nsIWebProgressListener
{
public:
  GtkMozEmbedChromeProgress();
  virtual ~GtkMozEmbedChromeProgress();
  
  void Init(GtkMozEmbed *aEmbed);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIWEBPROGRESSLISTENER

private:
  GtkMozEmbed *mEmbed;
  GtkMozEmbedPrivate *mEmbedPrivate;
};

class GtkMozEmbedChromeEventListener : public nsIDOMKeyListener,
				       public nsIDOMMouseListener
{
public:
  GtkMozEmbedChromeEventListener();
  virtual ~GtkMozEmbedChromeEventListener();

  void Init (GtkMozEmbed *aEmbed);
  
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
  GtkMozEmbed *mEmbed;
  GtkMozEmbedPrivate *mEmbedPrivate;
};

class GtkMozEmbedPrivate
{
public:
  GtkMozEmbedPrivate();
  ~GtkMozEmbedPrivate();

  nsresult    Init                (GtkMozEmbed *aEmbed);
  nsresult    Realize             (GtkWidget *aWidget);
  nsresult    Resize              (GtkWidget *aWidget);
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
  nsCOMPtr<nsIGtkEmbed>             mEmbed;
  GtkMozEmbedListenerImpl           mListener;
  GdkWindow                        *mMozWindow;
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

GtkMozEmbedPrivate::GtkMozEmbedPrivate(void) : mMozWindow(0), 
  mChromeLoaded(PR_FALSE), mContentLoaded(PR_FALSE),
  mEventListenerAttached(PR_FALSE)
{
}

GtkMozEmbedPrivate::~GtkMozEmbedPrivate(void)
{
}

nsresult
GtkMozEmbedPrivate::Init(GtkMozEmbed *aEmbed)
{

  // create an nsIWebBrowser object
  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  NS_ENSURE_TRUE(mWebBrowser, NS_ERROR_FAILURE);

  // create our glue widget
  GtkMozEmbedChrome *chrome = new GtkMozEmbedChrome();
  NS_ENSURE_TRUE(chrome, NS_ERROR_FAILURE);

  mEmbed = do_QueryInterface(NS_STATIC_CAST(nsISupports *,
					    NS_STATIC_CAST(nsIGtkEmbed *,
							   chrome)));
  NS_ENSURE_TRUE(mEmbed, NS_ERROR_FAILURE);

  // hide it
  aEmbed->data = this;

  // create our content and chrome progress listener objects
  GtkMozEmbedContentProgress *newContentProgress =
    new GtkMozEmbedContentProgress();
  GtkMozEmbedChromeProgress *newChromeProgress =
    new GtkMozEmbedChromeProgress();

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
  GtkMozEmbedChromeEventListener *newListener = 
    new GtkMozEmbedChromeEventListener();

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
  mEmbed->Init(GTK_WIDGET(aEmbed));

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
GtkMozEmbedPrivate::Realize(GtkWidget *aWidget)
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
  webBrowserBaseWindow->InitWindow(aWidget, NULL, 0, 0,
				   aWidget->allocation.width,
				   aWidget->allocation.height);
  webBrowserBaseWindow->Create();

  // save the id of the mozilla window for later if we need it.
  nsCOMPtr<nsIWidget> mozWidget;
  webBrowserBaseWindow->GetMainWidget(getter_AddRefs(mozWidget));
  // get the native drawing area
  GdkWindow *tmp_window = (GdkWindow *)
    mozWidget->GetNativeData(NS_NATIVE_WINDOW);
  // and, thanks to superwin we actually need the parent of that.
  tmp_window = gdk_window_get_parent(tmp_window);
  mMozWindow = tmp_window;

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
GtkMozEmbedPrivate::Resize(GtkWidget *aWidget)
{
  // set the size of the base window
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow =
    do_QueryInterface(mWebBrowser);
  webBrowserBaseWindow->SetPositionAndSize(0, 0, 
					   aWidget->allocation.width,
					   aWidget->allocation.height,
					   PR_TRUE);
  nsCOMPtr<nsIBaseWindow> embedBaseWindow = do_QueryInterface(mEmbed);
  embedBaseWindow->SetPositionAndSize(0, 0,
				      aWidget->allocation.width, 
				      aWidget->allocation.height,
				      PR_TRUE);
  return NS_OK;
}

nsresult
GtkMozEmbedPrivate::LoadChrome(void)
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

  mChromeLocation.AssignWithConversion("chrome://embedding/browser/content/simple-shell.xul");
  mChromeNav->LoadURI(mChromeLocation.GetUnicode());
  
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
GtkMozEmbedPrivate::GetCurrentURI(nsCString &aString)
{
  aString.Assign(mCurrentURI);
}

void
GtkMozEmbedPrivate::SetCurrentURI(nsCString &aString)
{
  mCurrentURI.Assign(aString);
}

nsresult
GtkMozEmbedPrivate::OnChromeStateChange(nsIWebProgress *aWebProgress,
					nsIRequest *aRequest,
					PRInt32 aStateFlags,
					PRUint32 aStatus)
{
  // XXX add shortcut here for content + chrome already loaded

  if ((aStateFlags & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT) && 
      (aStateFlags & GTK_MOZ_EMBED_FLAG_STOP))
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
	mContentNav->LoadURI(tmpString.GetUnicode());
      }
    }
  }
  return NS_OK;
}

void
GtkMozEmbedPrivate::Destroy(void)
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
GtkMozEmbedPrivate::RequestToURIString  (nsIRequest *aRequest, char **aString)
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
GtkMozEmbedPrivate::AddEventListener(void)
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
GtkMozEmbedPrivate::RemoveEventListener(void)
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


/* signals */

enum {
  LINK_MESSAGE,
  JS_STATUS,
  LOCATION,
  TITLE,
  PROGRESS,
  PROGRESS_ALL,
  NET_STATE,
  NET_STATE_ALL,
  NET_START,
  NET_STOP,
  NEW_WINDOW,
  VISIBILITY,
  DESTROY_BROWSER,
  OPEN_URI,
  SIZE_TO,
  DOM_KEY_DOWN,
  DOM_KEY_PRESS,
  DOM_KEY_UP,
  DOM_MOUSE_DOWN,
  DOM_MOUSE_UP,
  DOM_MOUSE_CLICK,
  DOM_MOUSE_DBL_CLICK,
  DOM_MOUSE_OVER,
  DOM_MOUSE_OUT,
  LAST_SIGNAL
};

static guint moz_embed_signals[LAST_SIGNAL] = { 0 };
static GdkWindow *offscreen_window = 0;

static char *component_path = 0;
static gint  num_widgets = 0;
nsCOMPtr <nsIAppShell> gAppShell;

/* class and instance initialization */

static void
gtk_moz_embed_class_init(GtkMozEmbedClass *klass);

static void
gtk_moz_embed_init(GtkMozEmbed *embed);

/* GtkWidget methods */

static void
gtk_moz_embed_realize(GtkWidget *widget);

static void
gtk_moz_embed_unrealize(GtkWidget *widget);

static void
gtk_moz_embed_size_allocate(GtkWidget *widget, GtkAllocation *allocation);

static void
gtk_moz_embed_map(GtkWidget *widget);

static void
gtk_moz_embed_unmap(GtkWidget *widget);

/* GtkObject methods */
static void
gtk_moz_embed_destroy(GtkObject *object);

/* startup and shutdown xpcom */

static gboolean
gtk_moz_embed_startup_xpcom(void);

static void
gtk_moz_embed_shutdown_xpcom(void);

void
gtk_moz_embed_create_offscreen_window(void);

void
gtk_moz_embed_destroy_offscreen_window(void);

static GtkBinClass *parent_class;

GtkType
gtk_moz_embed_get_type(void)
{
  static GtkType moz_embed_type = 0;
  if (!moz_embed_type)
  {
    static const GtkTypeInfo moz_embed_info =
    {
      "GtkMozEmbed",
      sizeof(GtkMozEmbed),
      sizeof(GtkMozEmbedClass),
      (GtkClassInitFunc)gtk_moz_embed_class_init,
      (GtkObjectInitFunc)gtk_moz_embed_init,
      0,
      0,
      0
    };
    moz_embed_type = gtk_type_unique(GTK_TYPE_BIN, &moz_embed_info);
  }
  return moz_embed_type;
}

static void
gtk_moz_embed_class_init(GtkMozEmbedClass *klass)
{
  GtkContainerClass  *container_class;
  GtkBinClass        *bin_class;
  GtkWidgetClass     *widget_class;
  GtkObjectClass     *object_class;
  
  container_class = GTK_CONTAINER_CLASS(klass);
  bin_class       = GTK_BIN_CLASS(klass);
  widget_class    = GTK_WIDGET_CLASS(klass);
  object_class    = (GtkObjectClass *)klass;

  parent_class = (GtkBinClass *)gtk_type_class(gtk_bin_get_type());

  widget_class->realize = gtk_moz_embed_realize;
  widget_class->unrealize = gtk_moz_embed_unrealize;
  widget_class->size_allocate = gtk_moz_embed_size_allocate;
  widget_class->map = gtk_moz_embed_map;
  widget_class->unmap = gtk_moz_embed_unmap;

  object_class->destroy = gtk_moz_embed_destroy;


  
  // set up our signals

  moz_embed_signals[LINK_MESSAGE] = 
    gtk_signal_new ("link_message",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET(GtkMozEmbedClass, link_message),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  moz_embed_signals[JS_STATUS] =
    gtk_signal_new ("js_status",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET(GtkMozEmbedClass, js_status),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  moz_embed_signals[LOCATION] =
    gtk_signal_new ("location",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET(GtkMozEmbedClass, location),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  moz_embed_signals[TITLE] = 
    gtk_signal_new("title",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, title),
		   gtk_marshal_NONE__NONE,
		   GTK_TYPE_NONE, 0);
  moz_embed_signals[PROGRESS] =
    gtk_signal_new("progress",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, progress),
		   gtk_marshal_NONE__INT_INT,
		   GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);
  moz_embed_signals[PROGRESS_ALL] = 
    gtk_signal_new("progress_all",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, progress_all),
		   gtk_marshal_NONE__POINTER_INT_INT,
		   GTK_TYPE_NONE, 3, GTK_TYPE_POINTER,
		   GTK_TYPE_INT, GTK_TYPE_INT);
  moz_embed_signals[NET_STATE] =
    gtk_signal_new("net_state",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, net_state),
		   gtk_marshal_NONE__INT_INT,
		   GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_UINT);
  moz_embed_signals[NET_STATE_ALL] =
    gtk_signal_new("net_state_all",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, net_state_all),
		   gtk_marshal_NONE__POINTER_INT_INT,
		   GTK_TYPE_NONE, 3, GTK_TYPE_POINTER,
		   GTK_TYPE_INT, GTK_TYPE_UINT);
  moz_embed_signals[NET_START] =
    gtk_signal_new("net_start",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, net_start),
		   gtk_marshal_NONE__NONE,
		   GTK_TYPE_NONE, 0);
  moz_embed_signals[NET_STOP] =
    gtk_signal_new("net_stop",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, net_stop),
		   gtk_marshal_NONE__NONE,
		   GTK_TYPE_NONE, 0);
  moz_embed_signals[NEW_WINDOW] =
    gtk_signal_new("new_window",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, new_window),
		   gtk_marshal_NONE__POINTER_UINT,
		   GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_UINT);
  moz_embed_signals[VISIBILITY] =
    gtk_signal_new("visibility",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, visibility),
		   gtk_marshal_NONE__BOOL,
		   GTK_TYPE_NONE, 1, GTK_TYPE_BOOL);
  moz_embed_signals[DESTROY_BROWSER] =
    gtk_signal_new("destroy_browser",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, destroy_brsr),
		   gtk_marshal_NONE__NONE,
		   GTK_TYPE_NONE, 0);
  moz_embed_signals[OPEN_URI] = 
    gtk_signal_new("open_uri",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, open_uri),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[SIZE_TO] =
    gtk_signal_new("size_to",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, size_to),
		   gtk_marshal_NONE__INT_INT,
		   GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);
  moz_embed_signals[DOM_KEY_DOWN] =
    gtk_signal_new("dom_key_down",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_key_down),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_KEY_PRESS] =
    gtk_signal_new("dom_key_press",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_key_press),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_KEY_UP] =
    gtk_signal_new("dom_key_up",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_key_up),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_DOWN] =
    gtk_signal_new("dom_mouse_down",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_down),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_UP] =
    gtk_signal_new("dom_mouse_up",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_up),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_CLICK] =
    gtk_signal_new("dom_mouse_click",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_click),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_DBL_CLICK] =
    gtk_signal_new("dom_mouse_dbl_click",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_dbl_click),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_OVER] =
    gtk_signal_new("dom_mouse_over",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_over),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_OUT] =
    gtk_signal_new("dom_mouse_out",
		   GTK_RUN_LAST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_out),
		   gtk_marshal_BOOL__POINTER,
		   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);

  gtk_object_class_add_signals(object_class, moz_embed_signals, LAST_SIGNAL);

}

static void
gtk_moz_embed_init(GtkMozEmbed *embed)
{
  // before we do anything else we need to fire up XPCOM
  if (num_widgets == 0)
  {
    gboolean retval;
    retval = gtk_moz_embed_startup_xpcom();
    g_return_if_fail(retval == TRUE);
      
  }
  // increment the number of widgets
  num_widgets++;

  GtkMozEmbedPrivate *embed_private;
  // create our private struct
  embed_private = new GtkMozEmbedPrivate();

  // initialize it
  nsresult rv;
  rv = embed_private->Init(embed);
  g_return_if_fail(NS_SUCCEEDED(rv));

}

GtkWidget *
gtk_moz_embed_new(void)
{
  return GTK_WIDGET(gtk_type_new(gtk_moz_embed_get_type()));
}

// These two push and pop functions allow you to cause xpcom to be
// started up before widgets are created and shut it down after the
// last widget is destroyed.

extern void
gtk_moz_embed_push_startup     (void)
{
  if (num_widgets == 0)
  {
    gboolean retval;
    retval = gtk_moz_embed_startup_xpcom();
    g_return_if_fail(retval == TRUE);
  }
  num_widgets++;
}

extern void
gtk_moz_embed_pop_startup      (void)
{
  num_widgets--;

  // see if we need to shutdown XPCOM
  if (num_widgets == 0)
    gtk_moz_embed_shutdown_xpcom();
}

void
gtk_moz_embed_set_comp_path    (char *aPath)
{
  // free the old one if we have to
  if (component_path)
    g_free(component_path);
  if (aPath) 
    component_path = g_strdup(aPath);
}

void
gtk_moz_embed_load_url(GtkMozEmbed *embed, const char *url)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  nsCString newURI;
  newURI.Assign(url);
  embed_private->SetCurrentURI(newURI);

  // If the widget isn't realized, just return.
  if (!GTK_WIDGET_REALIZED(embed))
    return;

  nsString uriString;
  uriString.AssignWithConversion(newURI);
  if (embed_private->mContentNav)
    embed_private->mContentNav->LoadURI(uriString.GetUnicode());
}

void
gtk_moz_embed_stop_load (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;

  if (embed_private->mContentNav)
    embed_private->mContentNav->Stop();
}

gboolean
gtk_moz_embed_can_go_back      (GtkMozEmbed *embed)
{
  PRBool retval = PR_FALSE;
  GtkMozEmbedPrivate *embed_private;

  g_return_val_if_fail ((embed != NULL), FALSE);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), FALSE);

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  if (embed_private->mContentNav)
    embed_private->mContentNav->GetCanGoBack(&retval);
  return retval;
}

gboolean
gtk_moz_embed_can_go_forward   (GtkMozEmbed *embed)
{
  PRBool retval = PR_FALSE;
  GtkMozEmbedPrivate *embed_private;

  g_return_val_if_fail ((embed != NULL), FALSE);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), FALSE);

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  if (embed_private->mContentNav)
    embed_private->mContentNav->GetCanGoForward(&retval);
  return retval;
}

void
gtk_moz_embed_go_back          (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  if (embed_private->mContentNav)
    embed_private->mContentNav->GoBack();
}

void
gtk_moz_embed_go_forward       (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  if (embed_private->mContentNav)
    embed_private->mContentNav->GoForward();
}

void
gtk_moz_embed_render_data      (GtkMozEmbed *embed, 
				const char *data, guint32 len,
				const char *base_uri, const char *mime_type)
{
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;

  embed_private->mEmbed->OpenStream(base_uri, mime_type);
  embed_private->mEmbed->AppendToStream(data, len);
  embed_private->mEmbed->CloseStream();
}

void
gtk_moz_embed_open_stream (GtkMozEmbed *embed,
			   const char *base_uri, const char *mime_type)
{
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  embed_private->mEmbed->OpenStream(base_uri, mime_type);
}

void
gtk_moz_embed_append_data (GtkMozEmbed *embed, const char *data, guint32 len)
{
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  embed_private->mEmbed->AppendToStream(data, len);
}

void
gtk_moz_embed_close_stream     (GtkMozEmbed *embed)
{
 GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
 
  embed_private->mEmbed->CloseStream();
}

void
gtk_moz_embed_reload           (GtkMozEmbed *embed, gint32 flags)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;

  if (embed_private->mContentNav)
    embed_private->mContentNav->Reload(flags);
}

void
gtk_moz_embed_set_chrome_mask (GtkMozEmbed *embed, guint32 flags)
{
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  nsCOMPtr<nsIWebBrowserChrome> browserChrome =
    do_QueryInterface(embed_private->mEmbed);
  g_return_if_fail(browserChrome);
  browserChrome->SetChromeFlags(flags);
}

guint32
gtk_moz_embed_get_chrome_mask  (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;
  PRUint32 curFlags = 0;
  
  g_return_val_if_fail ((embed != NULL), 0);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), 0);

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = 
    do_QueryInterface(embed_private->mEmbed);
  g_return_val_if_fail(browserChrome, 0);
  if (browserChrome->GetChromeFlags(&curFlags) == NS_OK)
    return curFlags;
  else
    return 0;
}

char *
gtk_moz_embed_get_link_message (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;
  char *retval = NULL;

  g_return_val_if_fail ((embed != NULL), NULL);
  g_return_val_if_fail ((GTK_IS_MOZ_EMBED(embed)), NULL);

  embed_private = (GtkMozEmbedPrivate *)embed->data;

  embed_private->mEmbed->GetLinkMessage(&retval);

  return retval;
}

char  *
gtk_moz_embed_get_js_status (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;
  char *retval = NULL;
  
  g_return_val_if_fail ((embed != NULL), NULL);
  g_return_val_if_fail ((GTK_IS_MOZ_EMBED(embed)), NULL);
  
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  embed_private->mEmbed->GetJSStatus(&retval);

  return retval;
}

char *
gtk_moz_embed_get_title (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;
  char *retval = NULL;
  
  g_return_val_if_fail ((embed != NULL), NULL);
  g_return_val_if_fail ((GTK_IS_MOZ_EMBED(embed)), NULL);
  
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  embed_private->mEmbed->GetTitleChar(&retval);

  return retval;
}

char *
gtk_moz_embed_get_location     (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;
  char *retval = NULL;
  
  g_return_val_if_fail ((embed != NULL), NULL);
  g_return_val_if_fail ((GTK_IS_MOZ_EMBED(embed)), NULL);
  
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  nsCString currentURI;
  embed_private->GetCurrentURI(currentURI);
  retval = nsCRT::strdup(currentURI.GetBuffer());

  return retval;
}

void
gtk_moz_embed_get_nsIWebBrowser  (GtkMozEmbed *embed, nsIWebBrowser **retval)
{
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));
  g_return_if_fail (retval != NULL);
  
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  *retval = embed_private->mWebBrowser.get();
  NS_IF_ADDREF(*retval);
}

void
gtk_moz_embed_realize(GtkWidget *widget)
{
  GtkMozEmbed        *embed;
  GtkMozEmbedPrivate *embed_private;
  GdkWindowAttr       attributes;
  gint                attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(widget));

  // check to see if we have to create the offscreen window
  if (!offscreen_window) {
    gtk_moz_embed_create_offscreen_window();
  }

  embed = GTK_MOZ_EMBED(widget);
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, embed);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  // if mozWindow is set then we've already been realized once.
  // reparent the window to our widget->window and return.
  if (embed_private->mMozWindow)
  {
    gdk_window_reparent(embed_private->mMozWindow, widget->window,
			widget->allocation.x, widget->allocation.y);
    return;
  }

  nsresult rv;
  rv = embed_private->Realize(widget);
  g_return_if_fail(NS_SUCCEEDED(rv));
}

void
gtk_moz_embed_unrealize(GtkWidget *widget)
{
  GtkMozEmbed        *embed;
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
  
  gdk_window_reparent(embed_private->mMozWindow,
		      offscreen_window, 0, 0);
}

void
gtk_moz_embed_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
  GtkMozEmbed        *embed;
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(widget));
  g_return_if_fail (allocation != NULL);

  embed = GTK_MOZ_EMBED(widget);
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  widget->allocation = *allocation;

#ifdef DEBUG_loser
  g_print("gtk_moz_embed_size allocate for %p to %d %d %d %d\n", widget, 
	  allocation->x, allocation->y, allocation->width, allocation->height);
#endif

  if (GTK_WIDGET_REALIZED(widget))
  {
    gdk_window_move_resize(widget->window,
			   allocation->x, allocation->y,
			   allocation->width, allocation->height);
    embed_private->Resize(widget);
  }
}

static void
gtk_moz_embed_map(GtkWidget *widget)
{
  GtkMozEmbed *embed;
  GtkMozEmbedPrivate *embed_private;
  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);

  GTK_WIDGET_SET_FLAGS(widget, GTK_MAPPED);

  embed_private = (GtkMozEmbedPrivate *)embed->data;

  // get our hands on the base window
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = 
    do_QueryInterface(embed_private->mWebBrowser);
  g_return_if_fail(webBrowserBaseWindow);
  // show it
  webBrowserBaseWindow->SetVisibility(PR_TRUE);
  // show the widget window
  gdk_window_show(widget->window);
}

static void
gtk_moz_embed_unmap(GtkWidget *widget)
{
  GtkMozEmbed *embed;
  GtkMozEmbedPrivate *embed_private;
  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);

  GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED);

  embed_private = (GtkMozEmbedPrivate *)embed->data;

  gdk_window_hide(widget->window);

}

void
gtk_moz_embed_destroy(GtkObject *object)
{
  GtkMozEmbed        *embed;
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(object));

  embed = GTK_MOZ_EMBED(object);
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  if (embed_private)
  {
    embed_private->Destroy();
    delete embed_private;
    embed->data = NULL;
  }

  num_widgets--;

  // see if we need to shutdown XPCOM
  if (num_widgets == 0)
  {
    if (offscreen_window)
      gtk_moz_embed_destroy_offscreen_window();
    gtk_moz_embed_shutdown_xpcom();
  }
}

gboolean
gtk_moz_embed_startup_xpcom(void)
{
  nsresult rv;
  // init our embedding
  nsCOMPtr<nsILocalFile> binDir;
  
  if (component_path) {
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

void
gtk_moz_embed_shutdown_xpcom(void)
{
  if (gAppShell)
  {
    gAppShell->Spindown();
    gAppShell = nsnull;
    // shut down XPCOM
    NS_TermEmbedding();
  }
}

void
gtk_moz_embed_create_offscreen_window(void)
{
  GdkWindowAttr attributes;

  attributes.window_type = GDK_WINDOW_TOPLEVEL;
  attributes.width = 1;
  attributes.height = 1;
  attributes.wclass = GDK_INPUT_OUTPUT;

  offscreen_window = gdk_window_new(NULL, &attributes, 0);
				    
}

void
gtk_moz_embed_destroy_offscreen_window(void)
{
  gdk_window_destroy(offscreen_window);
  offscreen_window = 0;
}

// this is a class that implements the callbacks from the chrome
// object

GtkMozEmbedListenerImpl::GtkMozEmbedListenerImpl(void)
{
  mEmbed = nsnull;
  mEmbedPrivate = nsnull;
}

GtkMozEmbedListenerImpl::~GtkMozEmbedListenerImpl(void)
{
}

void
GtkMozEmbedListenerImpl::Init(GtkMozEmbed *aEmbed)
{
  mEmbed = aEmbed;
  mEmbedPrivate = (GtkMozEmbedPrivate *)aEmbed->data;
}

nsresult
GtkMozEmbedListenerImpl::NewBrowser(PRUint32 chromeMask,
				    nsIDocShellTreeItem **_retval)
{
  GtkMozEmbed *newEmbed = NULL;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NEW_WINDOW],
		  &newEmbed, (guint)chromeMask);
  if (newEmbed)  {
   // We need to create a new top level window and then enter a nested
   // loop. Eventually the new window will be told that it has loaded,
   // at which time we know it is safe to spin out of the nested loop
   // and allow the opening code to proceed.

    // The window _must_ be realized before we pass it back to the
    // function that created it. Functions that create new windows
    // will do things like GetDocShell() and the widget has to be
    // realized before that can happen.
    gtk_widget_realize(GTK_WIDGET(newEmbed));
    GtkMozEmbedPrivate *embed_private = (GtkMozEmbedPrivate *)newEmbed->data;

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
GtkMozEmbedListenerImpl::Destroy(void)
{
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DESTROY_BROWSER]);
}

void
GtkMozEmbedListenerImpl::Visibility(PRBool aVisibility)
{
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[VISIBILITY],
		  aVisibility);
}


void
GtkMozEmbedListenerImpl::Message(GtkEmbedListenerMessageType aType,
				 const char *aMessage)
{
  switch (aType) {
  case MessageLink:
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[LINK_MESSAGE]);
    break;
  case MessageJSStatus:
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[JS_STATUS]);
    break;
  case MessageTitle:
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[TITLE]);
    break;
  default:
    break;
  }
}

PRBool
GtkMozEmbedListenerImpl::StartOpen(const char *aURI)
{
  // if our chrome hasn't been loaded then it's all OK.
  if (!mEmbedPrivate->mChromeLoaded)
    return PR_FALSE;
  gint return_val = PR_FALSE;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[OPEN_URI], 
		  aURI, &return_val);
  return return_val;
}

void
GtkMozEmbedListenerImpl::SizeTo(PRInt32 width, PRInt32 height)
{
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[SIZE_TO], width,
		  height);
}

GtkMozEmbedContentProgress::GtkMozEmbedContentProgress(void)
{
  NS_INIT_REFCNT();
  mEmbed = nsnull;
  mEmbedPrivate = nsnull;
}

GtkMozEmbedContentProgress::~GtkMozEmbedContentProgress(void)
{
}

NS_IMPL_ADDREF(GtkMozEmbedContentProgress)
NS_IMPL_RELEASE(GtkMozEmbedContentProgress)
NS_INTERFACE_MAP_BEGIN(GtkMozEmbedContentProgress)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END

void
GtkMozEmbedContentProgress::Init(GtkMozEmbed *aEmbed)
{
  mEmbed = aEmbed;
  mEmbedPrivate = (GtkMozEmbedPrivate *)aEmbed->data;
}

NS_IMETHODIMP
GtkMozEmbedContentProgress::OnStateChange(nsIWebProgress *aWebProgress,
					  nsIRequest *aRequest,
					  PRInt32 aStateFlags,
					  PRUint32 aStatus)
{
    // if we've got the start flag, emit the signal
  if ((aStateFlags & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT) && 
      (aStateFlags & GTK_MOZ_EMBED_FLAG_START))
  {
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NET_START]);
  }

  nsXPIDLCString uriString;
  GtkMozEmbedPrivate::RequestToURIString(aRequest, getter_Copies(uriString));
  nsCString currentURI;
  mEmbedPrivate->GetCurrentURI(currentURI);
  if (currentURI.Equals(uriString))
  {
    // for people who know what they are doing
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NET_STATE],
		    aStateFlags, aStatus);
  }
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NET_STATE_ALL],
		  (const char *)uriString, aStateFlags, aStatus);
  // and for stop, too
  if ((aStateFlags & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT) && 
      (aStateFlags & GTK_MOZ_EMBED_FLAG_STOP))
  {
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NET_STOP]);
  }

  return NS_OK;
}

NS_IMETHODIMP
GtkMozEmbedContentProgress::OnProgressChange(nsIWebProgress *aWebProgress, 
					     nsIRequest *aRequest, 
					     PRInt32 aCurSelfProgress,
					     PRInt32 aMaxSelfProgress,
					     PRInt32 aCurTotalProgress, 
					     PRInt32 aMaxTotalProgress)
{
  nsXPIDLCString uriString;
  GtkMozEmbedPrivate::RequestToURIString(aRequest, getter_Copies(uriString));
  nsCString currentURI;
  mEmbedPrivate->GetCurrentURI(currentURI);
  if (currentURI.Equals(uriString)) {
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[PROGRESS],
		    aCurTotalProgress, aMaxTotalProgress);
  }
  
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[PROGRESS_ALL],
		  (const char *)uriString,
		  aCurTotalProgress, aMaxTotalProgress);
  return NS_OK;
}

NS_IMETHODIMP
GtkMozEmbedContentProgress::OnLocationChange(nsIWebProgress *aWebProgress,
					     nsIRequest *aRequest,
					     nsIURI *aLocation)
{
  nsXPIDLCString newURI;
  NS_ENSURE_ARG_POINTER(aLocation);
  aLocation->GetSpec(getter_Copies(newURI));
  // let the chrome listener know that the location has changed
  nsCString newURIString;
  newURIString.Assign(newURI);
  mEmbedPrivate->SetCurrentURI(newURIString);
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[LOCATION]);
  return NS_OK;
}

NS_IMETHODIMP
GtkMozEmbedContentProgress::OnStatusChange(nsIWebProgress *aWebProgress,
					   nsIRequest *aRequest,
					   nsresult aStatus, 
					   const PRUnichar *aMessage)
{
  return NS_OK;
}




NS_IMETHODIMP 
GtkMozEmbedContentProgress::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                             nsIRequest *aRequest, 
                                             PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


GtkMozEmbedChromeProgress::GtkMozEmbedChromeProgress(void)
{
  NS_INIT_REFCNT();
  mEmbed = nsnull;
  mEmbedPrivate = nsnull;
}

GtkMozEmbedChromeProgress::~GtkMozEmbedChromeProgress(void)
{
}

NS_IMPL_ADDREF(GtkMozEmbedChromeProgress)
NS_IMPL_RELEASE(GtkMozEmbedChromeProgress)
NS_INTERFACE_MAP_BEGIN(GtkMozEmbedChromeProgress)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END

void
GtkMozEmbedChromeProgress::Init(GtkMozEmbed *aEmbed)
{
  mEmbed = aEmbed;
  mEmbedPrivate = (GtkMozEmbedPrivate *)aEmbed->data;
}

NS_IMETHODIMP
GtkMozEmbedChromeProgress::OnStateChange(nsIWebProgress *aWebProgress,
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
GtkMozEmbedChromeProgress::OnProgressChange(nsIWebProgress *aWebProgress, 
					     nsIRequest *aRequest, 
					     PRInt32 aCurSelfProgress,
					     PRInt32 aMaxSelfProgress,
					     PRInt32 aCurTotalProgress, 
					     PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
GtkMozEmbedChromeProgress::OnLocationChange(nsIWebProgress *aWebProgress,
					    nsIRequest *aRequest,
					    nsIURI *aLocation)
{
  return NS_OK;
}

NS_IMETHODIMP
GtkMozEmbedChromeProgress::OnStatusChange(nsIWebProgress *aWebProgress,
					   nsIRequest *aRequest,
					   nsresult aStatus, 
					   const PRUnichar *aMessage)
{
  return NS_OK;
}


NS_IMETHODIMP 
GtkMozEmbedChromeProgress::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                            nsIRequest *aRequest, 
                                            PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

GtkMozEmbedChromeEventListener::GtkMozEmbedChromeEventListener(void)
{
  NS_INIT_REFCNT();
  mEmbed = nsnull;
  mEmbedPrivate = nsnull;
}

GtkMozEmbedChromeEventListener::~GtkMozEmbedChromeEventListener(void)
{
}

NS_IMPL_ADDREF(GtkMozEmbedChromeEventListener)
NS_IMPL_RELEASE(GtkMozEmbedChromeEventListener)
NS_INTERFACE_MAP_BEGIN(GtkMozEmbedChromeEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
NS_INTERFACE_MAP_END

void
GtkMozEmbedChromeEventListener::Init(GtkMozEmbed *aEmbed)
{
  mEmbed = aEmbed;
  mEmbedPrivate = (GtkMozEmbedPrivate *)aEmbed->data;
}

// All of the event listeners below return NS_OK to indicate that the
// event should not be consumed in the default case.

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::KeyDown(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aDOMEvent);
  if (!keyEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  gint return_val = NS_OK;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DOM_KEY_DOWN],
		  (void *)keyEvent, &return_val);
  return return_val;
}

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::KeyUp(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aDOMEvent);
  if (!keyEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  gint return_val = NS_OK;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DOM_KEY_UP],
		  (void *)keyEvent, &return_val);
  return return_val;
}

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::KeyPress(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aDOMEvent);
  if (!keyEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  gint return_val = NS_OK;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DOM_KEY_PRESS],
		  (void *)keyEvent, &return_val);
  return return_val;
}

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::MouseDown(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  gint return_val = NS_OK;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DOM_MOUSE_DOWN],
		  (void *)mouseEvent, &return_val);
  return return_val;
}

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::MouseUp(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  gint return_val = NS_OK;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DOM_MOUSE_UP],
		  (void *)mouseEvent, &return_val);
  return return_val;
}

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::MouseClick(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  gint return_val = NS_OK;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DOM_MOUSE_CLICK],
		  (void *)mouseEvent, &return_val);
  return return_val;
}

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::MouseDblClick(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  gint return_val = NS_OK;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DOM_MOUSE_DBL_CLICK],
		  (void *)mouseEvent, &return_val);
  return return_val;
}

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::MouseOver(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  gint return_val = NS_OK;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DOM_MOUSE_OVER],
		  (void *)mouseEvent, &return_val);
  return return_val;
}

NS_IMETHODIMP
GtkMozEmbedChromeEventListener::MouseOut(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr <nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // return NS_OK to this function to mark this event as not
  // consumed...
  gint return_val = NS_OK;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[DOM_MOUSE_OUT],
		  (void *)mouseEvent, &return_val);
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
