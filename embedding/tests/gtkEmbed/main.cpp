/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


//
// What follows is a hacked version the the sample hello
// world gtk app found: http://www.gtk.org/tutorial/gtk_tut-2.html
//


#include <gtk/gtk.h>
 
#include "gtkEmbed.h"

#include "nsCOMPtr.h"
#include "nsISimpleEnumerator.h"
#include "nsEmbedAPI.h"
#include "WebBrowserChrome.h"
#include "WindowCreator.h"
#include "PromptService.h"

#include <nsIEventQueueService.h>
#include <nsIServiceManager.h>
#include <nsIWindowWatcher.h>
#include <nsIPref.h>

// For getenv()
#include "prenv.h"  

// for profiles
#include <nsMPFileLocProvider.h>

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kPromptServiceCID, NS_PROMPTSERVICE_CID);

static gint  io_identifier = 0;
static char *sWatcherContractID = "@mozilla.org/embedcomp/window-watcher;1";

static nsIPref *sPrefs = nsnull;

typedef struct
{
  GtkWidget *window;
  GtkWidget *urlEntry;
  GtkWidget *box;
  GtkWidget *view;
  GtkWidget *memUsageDumpBtn;
  nsIWebBrowserChrome *chrome;
} MyBrowser;


void destroy( GtkWidget *widget, MyBrowser*   data )
{
	PRBool dontQuit;

	printf("Destroying created widgets\n");
	gtk_widget_destroy( data->window );
	gtk_widget_destroy( data->urlEntry );
	gtk_widget_destroy( data->box );
	gtk_widget_destroy( data->memUsageDumpBtn );
	//  gtk_widget_destroy( data->view );
	free (data);
	
	dontQuit = PR_FALSE;
	nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(sWatcherContractID));
	if (wwatch) {
		nsCOMPtr<nsISimpleEnumerator> list;
		wwatch->GetWindowEnumerator(getter_AddRefs(list));
		if (list)
			list->HasMoreElements(&dontQuit);
	}
	if (!dontQuit)
		gtk_main_quit();
}

gint delete_event( GtkWidget *widget, GdkEvent  *event, MyBrowser* data )
{
	printf("Releasing chrome...");
	NS_RELEASE(data->chrome);
	printf("done.\n");
	return(FALSE);
}


void
dump_mem_clicked_cb(GtkButton *button, nsIWebBrowserChrome *browser)
{
	g_print("dumping memory usage:\n");
	system("ps -eo \"%c %z\" | grep gtkEmbed");

#ifdef NS_TRACE_MALLOC
	// dump detailed information, too
	NS_TraceMallocDumpAllocations("allocations.log");
#endif
}


void
url_activate_cb( GtkEditable *widget, nsIWebBrowserChrome *browser)
{
	gchar *text = gtk_editable_get_chars(widget, 0, -1);
	g_print("loading url %s\n", text);
	if (browser)
		{
			nsCOMPtr<nsIWebBrowser> webBrowser;
			browser->GetWebBrowser(getter_AddRefs(webBrowser));
			nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(webBrowser));
			if (webNav)
				webNav->LoadURI(NS_ConvertASCIItoUCS2(text).get(), nsIWebNavigation::LOAD_FLAGS_NONE);
			
		}       
    g_free(text);
}

static void
handle_event_queue(gpointer data, gint source, GdkInputCondition condition)
{
	nsIEventQueue *eventQueue = (nsIEventQueue *)data;
	eventQueue->ProcessPendingEvents();
}


/* InitializeWindowCreator creates and hands off an object with a callback
   to a window creation function. This will be used by Gecko C++ code
   (never JS) to create new windows when no previous window is handy
   to begin with. This is done in a few exceptional cases, like PSM code.
   Failure to set this callback will only disable the ability to create
   new windows under these circumstances. */
nsresult InitializeWindowCreator()
{
  // create an nsWindowCreator and give it to the WindowWatcher service
	WindowCreator *creatorCallback = new WindowCreator();
	if (creatorCallback) {
		nsCOMPtr<nsIWindowCreator> windowCreator(dont_QueryInterface(NS_STATIC_CAST(nsIWindowCreator *, creatorCallback)));
		if (windowCreator) {
			nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(sWatcherContractID));
			if (wwatch) {
				wwatch->SetWindowCreator(windowCreator);
				return NS_OK;
			}
		}
	}
	return NS_ERROR_FAILURE;
}

nsresult CreateBrowserWindow(PRUint32 aChromeFlags,
                             nsIWebBrowserChrome *aParent,
                             nsIWebBrowserChrome **aNewWindow)
{
    WebBrowserChrome * chrome = new WebBrowserChrome();
    if (!chrome)
        return NS_ERROR_FAILURE;

    CallQueryInterface(NS_STATIC_CAST(nsIWebBrowserChrome *, chrome), aNewWindow);

    // chrome->SetChromeFlags(aChromeFlags);
  
    nsCOMPtr<nsIWebBrowser> newBrowser;
    chrome->CreateBrowser(-1, -1, -1, -1, getter_AddRefs(newBrowser));
    if (!newBrowser)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(newBrowser);
    
    baseWindow->SetPositionAndSize(0, 
                                   0, 
                                   430, 
                                   430,
                                   PR_TRUE);

    baseWindow->SetVisibility(PR_TRUE);

    GtkWidget *widget;
    baseWindow->GetParentNativeWindow((void**)&widget);

    gtk_widget_show (widget); // should this function do this?
    return NS_OK;
}   


nsresult OpenWebPage(char* url)
{
    nsresult                      rv;
    nsCOMPtr<nsIWebBrowserChrome> chrome;

    rv = CreateBrowserWindow(nsIWebBrowserChrome::CHROME_ALL, nsnull,
                             getter_AddRefs(chrome));

    if (NS_SUCCEEDED(rv)) {
		// Start loading a page
		nsCOMPtr<nsIWebBrowser> newBrowser;
		chrome->GetWebBrowser(getter_AddRefs(newBrowser));
		nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(newBrowser));
		return webNav->LoadURI(NS_ConvertASCIItoUCS2(url).get(), nsIWebNavigation::LOAD_FLAGS_NONE);
    }
    return rv;
}   


nativeWindow CreateNativeWindow(nsIWebBrowserChrome* chrome)
{
	MyBrowser* browser = (MyBrowser*) malloc (sizeof(MyBrowser) );
	if (!browser)
		return nsnull;
	
	browser->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	browser->urlEntry = gtk_entry_new();
	browser->box = gtk_vbox_new(FALSE, 0);
	browser->view = gtk_event_box_new();
	browser->memUsageDumpBtn = gtk_button_new_with_label("Dump Memory Usage");
	browser->chrome = chrome;  // take ownership!
	NS_ADDREF(chrome);         // no, really take it! (released in delete_event)
	
	if (!browser->window || !browser->urlEntry || !browser->box || 
		!browser->view   || !browser->memUsageDumpBtn)
		return nsnull;
	
	/* Put the box into the main window. */
	gtk_container_add (GTK_CONTAINER (browser->window),browser->box);
	
	gtk_box_pack_start(GTK_BOX(browser->box), browser->urlEntry, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(browser->box), browser->view,     FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(browser->box), browser->memUsageDumpBtn,     FALSE, FALSE, 5);
	
	gtk_signal_connect (GTK_OBJECT (browser->window), 
						"destroy",
						GTK_SIGNAL_FUNC (destroy), 
						browser);
	
	gtk_signal_connect (GTK_OBJECT (browser->window), 
						"delete_event",
						GTK_SIGNAL_FUNC (delete_event), 
						browser);
	
	gtk_signal_connect(GTK_OBJECT(browser->urlEntry),
					   "activate",
					   GTK_SIGNAL_FUNC(url_activate_cb), 
					   chrome);
	
	gtk_signal_connect(GTK_OBJECT(browser->memUsageDumpBtn),
					   "clicked",
					   GTK_SIGNAL_FUNC(dump_mem_clicked_cb), 
					   chrome);
	
	gtk_widget_set_usize(browser->view, 430, 430);
	
	gtk_window_set_title (GTK_WINDOW (browser->window), "Embedding is Fun!");
	gtk_window_set_default_size (GTK_WINDOW(browser->window), 450, 450);
	
	gtk_container_set_border_width (GTK_CONTAINER (browser->window), 10);
	
	gtk_widget_realize (browser->window);
	
	gtk_widget_show (browser->memUsageDumpBtn);
	gtk_widget_show (browser->urlEntry);
	gtk_widget_show (browser->box);
	gtk_widget_show (browser->view);
	gtk_widget_show (browser->window);
	
	return browser->view;
}


// Initialize profiles, need this for SSL to work.
// Borrowed from TestGtkEmbed.cpp, EmbedPrivate.cpp.
nsresult InitProfile()
{
    nsresult rv;

	char *home_path = home_path = PR_GetEnv("HOME");
	char *sProfileDir = g_strdup_printf("%s/%s", home_path, ".gtkEmbed");
	char *sProfileName = g_strdup_printf("%s", "gtkEmbed");

	// initialize profiles
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
    sPrefs->ReadUserPrefs(nsnull);

	return NS_OK;
}

nsresult InitPromptService()
{
    nsresult rv;
    nsCOMPtr<nsIFactory> promptFactory;
    rv = NS_NewPromptServiceFactory(getter_AddRefs(promptFactory));
    if (NS_SUCCEEDED(rv))
		{
			nsComponentManager::RegisterFactory(kPromptServiceCID,
												"Prompt Service",
												"@mozilla.org/embedcomp/prompt-service;1",
												promptFactory,
												PR_TRUE); // replace existing
		}
    return NS_OK;
}


int main( int  argc,  char *argv[] )
{
#ifdef NS_TRACE_MALLOC
	argc = NS_TraceMallocStartupArgs(argc, argv);
#endif

	// Get startup URL.
	char *loadURLStr;
	if (argc > 1)
		loadURLStr = argv[1];
	else
		loadURLStr = "http://www.mozilla.org/projects/embedding";

	// Initialize Embedding APIs.
	NS_InitEmbedding(nsnull, nsnull);

	// Initialize profile, we need this for SSL to work.
	nsresult rv = InitProfile();
	if (NS_FAILED(rv))
		NS_WARNING("Warning: Failed to start up profiles.\n");

	// Initialise the prompt service
	rv = InitPromptService();
	if (NS_FAILED(rv))
		NS_WARNING("Warning: Failed to register prompt service.\n");


	InitializeWindowCreator();

	// set up the thread event queue
	nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID);
	if (!eventQService) return (-1);

	nsCOMPtr< nsIEventQueue> eventQueue;
	eventQService->GetThreadEventQueue( NS_CURRENT_THREAD, getter_AddRefs(eventQueue));
	if (!eventQueue) return (-1);

	io_identifier = gdk_input_add( eventQueue->GetEventQueueSelectFD(),
								   GDK_INPUT_READ,
								   handle_event_queue,
								   eventQueue);

	/* This is called in all GTK applications. Arguments are parsed
	 * from the command line and are returned to the application. */
	gtk_set_locale();
	gtk_init(&argc, &argv);


	if ( NS_FAILED( OpenWebPage(loadURLStr) ))
		return (-1);


	/* All GTK applications must have a gtk_main(). Control ends here
	 * and waits for an event to occur (like a key press or
	 * mouse event). */
	gtk_main ();
	
	gtk_input_remove(io_identifier);
	
	printf("Cleaning up embedding\n");
	NS_TermEmbedding();        
	
#ifdef NS_TRACE_MALLOC
	NS_TraceMallocShutdown();
#endif

	return(0);
}
