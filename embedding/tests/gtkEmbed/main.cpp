
//
// What follows is a hacked version the the sample hello
// world gtk app found: http://www.gtk.org/tutorial/gtk_tut-2.html
//


#include <gtk/gtk.h>
 
#include "nsCOMPtr.h"
#include "nsEmbedAPI.h"
#include "WebBrowserChrome.h"

#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static gint  io_identifier = 0;

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
  printf("Destroying created widgets\n");
  gtk_widget_destroy( data->window );
  gtk_widget_destroy( data->urlEntry );
  gtk_widget_destroy( data->box );
  gtk_widget_destroy( data->memUsageDumpBtn );
  //  gtk_widget_destroy( data->view );
  free (data);
  
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
	  webNav->LoadURI(NS_ConvertASCIItoUCS2(text).GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);

  }       
  g_free(text);
}

static void
handle_event_queue(gpointer data, gint source, GdkInputCondition condition)
{
  nsIEventQueue *eventQueue = (nsIEventQueue *)data;
  eventQueue->ProcessPendingEvents();
}


nsresult OpenWebPage(char* url)
{
    WebBrowserChrome * chrome = new WebBrowserChrome();
    if (!chrome)
        return NS_ERROR_FAILURE;
  
    NS_ADDREF(chrome); // native window will hold the addref.

    nsCOMPtr<nsIWebBrowser> newBrowser;
    chrome->CreateBrowserWindow(0, -1, -1, -1, -1, getter_AddRefs(newBrowser));
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

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(newBrowser));

    if (!webNav)
	  return NS_ERROR_FAILURE;

    return webNav->LoadURI(NS_ConvertASCIItoUCS2(url).GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
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



int main( int  argc,  char *argv[] )
{
  char *loadURLStr;
  if (argc > 1)
    loadURLStr = argv[1];
  else
    loadURLStr = "http://www.mozilla.org";

  NS_InitEmbedding(nsnull, nsnull);

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
  return(0);
}

