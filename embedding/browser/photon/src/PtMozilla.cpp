#include <stdlib.h>
#include <time.h>
#include "nsIWebBrowser.h"
#include "nsCWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "MozEmbedChrome.h"
#include "nsIComponentManager.h"
#include "nsIWebNavigation.h"
#include "nsString.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsEmbedAPI.h"
#include "nsVoidArray.h"

#include <Pt.h>
#include "PtMozilla.h"
#include <photon/PtProto.h>

PtWidgetClass_t *PtCreateMozillaClass( void );
#ifndef _PHSLIB
	PtWidgetClassRef_t __PtMozilla = { NULL, PtCreateMozillaClass };
	PtWidgetClassRef_t *PtMozilla = &__PtMozilla; 
#endif

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);


static PRBool NS_SetupRegistryCalled = PR_FALSE;
static PRBool ThreadQueueSetup       = PR_FALSE;

extern "C" void NS_SetupRegistry();


int
mozilla_handle_event_queue(int fd, void *data, unsigned mode)
{
	nsIEventQueue *eventQueue = (nsIEventQueue *)data;
	eventQueue->ProcessPendingEvents();
}

// connect function
static void mozilla_init( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
	int rv;
	char *component_path = NULL;

	// --------- Mozilla related stuff ------------

#if 0
	// check to see if NS_SetupRegistry has been called
	if (!NS_SetupRegistryCalled)
	{
		NS_SetupRegistry();
		NS_SetupRegistryCalled = PR_TRUE;
	}
#else
	// init our embedding
	nsCOMPtr<nsILocalFile> binDir;
	NS_NewLocalFile(component_path, PR_TRUE, getter_AddRefs(binDir));
	
	NS_InitEmbedding(binDir, nsnull);
#endif

	// check to see if we have to set up our thread event queue
	if (!ThreadQueueSetup)
	{
		nsIEventQueueService* eventQService;
		rv = nsServiceManager::GetService(kEventQueueServiceCID, NS_GET_IID(nsIEventQueueService), (nsISupports **)&eventQService);
		if (NS_OK == rv)
		{
			nsIEventQueue *eventQueue;
			// create the event queue
			rv = eventQService->CreateThreadEventQueue();
			rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQueue);

			PtAppAddFd(NULL, eventQueue->GetEventQueueSelectFD(), Pt_FD_READ|Pt_FD_NOPOLL, mozilla_handle_event_queue, eventQueue);
			NS_RELEASE(eventQService);
			NS_RELEASE(eventQueue);
		}
		ThreadQueueSetup = PR_TRUE;
	}

	PtSuperClassConnect(PtContainer, widget);
}

static void
mozilla_embed_handle_title_change(PtWidget_t *widget)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;	
	char *retval = NULL;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaTitleCb_t t;

	if (moz->title_cb)
	{
		memset(&cbinfo, 0, sizeof(cbinfo));
		cbinfo.cbdata = &t;
		cbinfo.reason = Pt_CB_MOZ_TITLE;
		cb = moz->title_cb;

		moz->embed_private->embed->GetTitleChar(&retval);
		t.title = retval;
		PtInvokeCallbackList(cb, widget, &cbinfo);
	}
}


static void
mozilla_embed_handle_location_change(PtWidget_t *widget)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
	char *retval = NULL;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaUrlCb_t u;

	if (moz->url_cb)
	{
		memset(&cbinfo, 0, sizeof(cbinfo));
		cbinfo.cbdata = &u;
		cbinfo.reason = Pt_CB_MOZ_URL;
		cb = moz->url_cb;

		moz->embed_private->embed->GetLocation(&retval);
		u.url = retval;
		PtInvokeCallbackList(cb, widget, &cbinfo);
	}
}

static void
mozilla_embed_handle_link_change(PtWidget_t *widget)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
	char *retval = NULL;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaStatusCb_t s;

	if (moz->status_cb)
	{
		memset(&cbinfo, 0, sizeof(cbinfo));
		cbinfo.cbdata = &s;
		cbinfo.reason = Pt_CB_MOZ_STATUS;
		cb = moz->status_cb;

		moz->embed_private->embed->GetLinkMessage(&retval);
		s.message = retval;
		PtInvokeCallbackList(cb, widget, &cbinfo);
	}
}

static void
mozilla_embed_handle_progress(PtWidget_t *widget, int32 curprogress, int32 maxprogress)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
	PtCallbackList_t *cb;
	PtCallbackInfo_t cbinfo;
	PtMozillaProgressCb_t p;

	if (moz->progress_cb)
	{
		memset(&cbinfo, 0, sizeof(cbinfo));
		cbinfo.cbdata = &p;
		cbinfo.reason = Pt_CB_MOZ_PROGRESS;
		cb = moz->progress_cb;
		p.max = maxprogress;
		p.cur = curprogress;
		PtInvokeCallbackList(cb, widget, &cbinfo);
	}
}

static void
mozilla_embed_handle_net(PtWidget_t *widget,int32 flags, _uint32 status)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
	PtCallbackList_t *cb = NULL;
	PtCallbackInfo_t cbinfo;

	memset(&cbinfo, 0, sizeof(cbinfo));
	// if we've got the start flag, emit the signal
	if ((flags & MOZ_EMBED_FLAG_IS_NETWORK) && (flags & MOZ_EMBED_FLAG_START))
	{
		if (moz->start_cb)
		{
			cbinfo.reason = Pt_CB_MOZ_START;
			cb = moz->start_cb;
		}
		fprintf(stderr, "start\n");
	}
	// and for stop, too
	if ((flags & MOZ_EMBED_FLAG_IS_NETWORK) && (flags & MOZ_EMBED_FLAG_STOP))
	{
		if (moz->complete_cb)
		{
			cbinfo.reason = Pt_CB_MOZ_COMPLETE;
			cb = moz->complete_cb;
		}
		fprintf(stderr, "stop\n");
	}
	fprintf(stderr, "other %d\n", flags);
	if (cb)
		PtInvokeCallbackList(cb, widget, &cbinfo);
}

static void mozilla_defaults( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
	PtBasicWidget_t *basic = (PtBasicWidget_t *)widget;
	nsCOMPtr<nsIWebBrowserChrome> browserChrome;
	nsresult rv;
	char *component_path = NULL;

	rv = NS_InitEmbedding(component_path);
	if (NS_FAILED(rv))
	  return;
	// set up the thread event queue
	nsIEventQueueService* eventQService;
	rv = nsServiceManager::GetService(kEventQueueServiceCID, NS_GET_IID(nsIEventQueueService), (nsISupports **)&eventQService);
	if (NS_OK == rv)
	{
	  	// get our hands on the thread event queue
	  	nsIEventQueue *eventQueue;
	  	rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQueue);
	  	if (NS_FAILED(rv))
			return;

		PtAppAddFd(NULL, eventQueue->GetEventQueueSelectFD(), Pt_FD_READ|Pt_FD_NOPOLL, mozilla_handle_event_queue, eventQueue);
	  	NS_RELEASE(eventQService);
	  	NS_RELEASE(eventQueue);
	}

	basic->flags = Pt_ALL_OUTLINES | Pt_ALL_BEVELS | Pt_FLAT_FILL;
	widget->resize_flags &= ~Pt_RESIZE_XY_BITS; // fixed size.
	widget->anchor_flags = Pt_TOP_ANCHORED_TOP | Pt_LEFT_ANCHORED_LEFT | \
			Pt_BOTTOM_ANCHORED_TOP | Pt_RIGHT_ANCHORED_LEFT | Pt_ANCHORS_INVALID;

	strcpy(moz->url, "www.mozilla.org");
	moz->embed_private = new MozEmbedPrivate();
	// create an nsIWebBrowser object
	moz->embed_private->webBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
	// create our glue widget
	moz->chrome = new MozEmbedChrome();
	moz->embed_private->embed = do_QueryInterface((nsISupports *)(nsIPhEmbed *) moz->chrome);

	// get our hands on the browser chrome
	browserChrome = do_QueryInterface(moz->embed_private->embed);
	// set the toplevel window
	moz->embed_private->webBrowser->SetTopLevelWindow(browserChrome);
	// set the widget as the owner of the object
	moz->embed_private->embed->Init(widget);

	// so the embedding widget can find it's owning nsIWebBrowser object
	browserChrome->SetWebBrowser(moz->embed_private->webBrowser);

	// track the window changes
	moz->embed_private->embed->SetLinkChangeCallback((void (*)(void *))mozilla_embed_handle_link_change, widget);
	moz->embed_private->embed->SetTitleChangeCallback((void (*)(void *))mozilla_embed_handle_title_change, widget);
	moz->embed_private->embed->SetProgressCallback((void (*)(void *, int, int))mozilla_embed_handle_progress, widget);
	moz->embed_private->embed->SetNetCallback((void (*)(void *, int, uint))mozilla_embed_handle_net, widget);
	moz->embed_private->embed->SetLocationChangeCallback((void (*)(void *))mozilla_embed_handle_location_change, widget);

	// get our hands on a copy of the nsIWebNavigation interface for later
	moz->embed_private->navigation = do_QueryInterface(moz->embed_private->webBrowser);

	browserChrome->SetChromeMask(1);
}

void
mozilla_embed_load_url(PtWidget_t *widget, const char *url)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
	nsString URLString;

	URLString.AssignWithConversion(url);
	moz->embed_private->navigation->LoadURI(URLString.GetUnicode());
}

// realized function
static void mozilla_realized( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
	nsCOMPtr<nsIURIContentListener> uriListener;
	nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow;
	PRBool visibility;

	PtSuperClassRealized(PtContainer, widget);

	// init our window
	webBrowserBaseWindow = do_QueryInterface(moz->embed_private->webBrowser);
	if (!webBrowserBaseWindow)
		printf("no webBrowserBaseWindow\n");
	webBrowserBaseWindow->InitWindow(widget, NULL, 0, 0, widget->area.size.w, widget->area.size.h);
	webBrowserBaseWindow->Create();
	webBrowserBaseWindow->GetVisibility(&visibility);
	// set our webBrowser object as the content listener object
	uriListener = do_QueryInterface(moz->embed_private->embed);
	if (!uriListener)
		printf("no uriListener\n");
	moz->embed_private->webBrowser->SetParentURIContentListener(uriListener);

	// show it
	webBrowserBaseWindow->SetVisibility(PR_TRUE);

	// If an initial url was stored, load it
	if (moz->url[0])
		mozilla_embed_load_url(widget, moz->url);
}

// unrealized function
static void mozilla_unrealized( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
}

static void mozilla_modify( PtWidget_t *widget, PtArg_t const *argt )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;

	switch( argt->type ) 
	{
		case Pt_ARG_MOZ_GET_URL:
		{
			nsString URLString;
			char *url = (char *)argt->value;
			URLString.AssignWithConversion(url);
			moz->embed_private->navigation->LoadURI(URLString.GetUnicode());
			break;
		}
		case Pt_ARG_MOZ_NAVIGATE_PAGE:
			if (argt->len == WWW_DIRECTION_FWD)
				moz->embed_private->navigation->GoForward();
			else
				moz->embed_private->navigation->GoBack();
			break;
		case Pt_ARG_MOZ_STOP:
			moz->embed_private->navigation->Stop();
			break;
		case Pt_ARG_MOZ_RELOAD:
			moz->embed_private->navigation->Reload(0);
			break;
		default:
			return;
	}

	return;
}

// PtMozilla class creation function
PtWidgetClass_t *PtCreateMozillaClass( void )
{
	static const PtResourceRec_t resources[] =
	{
		{ Pt_ARG_MOZ_GET_URL,           mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_NAVIGATE_PAGE,     mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_RELOAD,            mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_STOP,              mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_CB_MOZ_STATUS,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, status_cb) },
		{ Pt_CB_MOZ_PROGRESS,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, progress_cb) },
		{ Pt_CB_MOZ_TITLE,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, title_cb) },
		{ Pt_CB_MOZ_START,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, start_cb) },
		{ Pt_CB_MOZ_COMPLETE,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, complete_cb) },
		{ Pt_CB_MOZ_URL,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, url_cb) },
	};

	static const PtArg_t args[] = 
	{
		{ Pt_SET_VERSION, 200},
		{ Pt_SET_STATE_LEN, sizeof( PtMozillaWidget_t ) },
		//{ Pt_SET_CONNECT_F, (long)mozilla_init },
		{ Pt_SET_DFLTS_F, (long)mozilla_defaults },
		{ Pt_SET_REALIZED_F, (long)mozilla_realized },
		{ Pt_SET_UNREALIZE_F, (long)mozilla_unrealized },
		{ Pt_SET_FLAGS, Pt_RECTANGULAR, Pt_RECTANGULAR },
		{ Pt_SET_RESOURCES, (long) resources },
		{ Pt_SET_NUM_RESOURCES, sizeof( resources )/sizeof( resources[0] ) },
		{ Pt_SET_DESCRIPTION, (long) "PtMozilla" },
	};

	return( PtMozilla->wclass = PtCreateWidgetClass(
		PtContainer, 0, sizeof( args )/sizeof( args[0] ), args ) );
}
