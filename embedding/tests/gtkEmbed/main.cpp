//
// What follows is a hacked version the the sample hello
// world gtk app found: http://www.gtk.org/tutorial/gtk_tut-2.html
//


#include <gtk/gtk.h>
 
#include "nsEmbedAPI.h"
#include "WebBrowser.h"

#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static gint  io_identifier = 0;

static void
handle_event_queue(gpointer data, gint source, GdkInputCondition condition)
{
  nsIEventQueue *eventQueue = (nsIEventQueue *)data;
  eventQueue->ProcessPendingEvents();
}

     int main( int   argc,
               char *argv[] )
     {
         nsresult rv;
         NS_InitEmbedding(nsnull, nsnull);

         // set up the thread event queue
         nsIEventQueueService* eventQService;
         rv = nsServiceManager::GetService(kEventQueueServiceCID,
	          			   NS_GET_IID(nsIEventQueueService),
		         		   (nsISupports **)&eventQService);
         if (NS_OK == rv)
         {
            // get our hands on the thread event queue
            nsIEventQueue *eventQueue;
            rv = eventQService->GetThreadEventQueue( NS_CURRENT_THREAD, 
                                                     &eventQueue);
            if (NS_FAILED(rv))
            return FALSE;
    
            io_identifier = gdk_input_add( eventQueue->GetEventQueueSelectFD(),
				           GDK_INPUT_READ,
				           handle_event_queue,
				           eventQueue);
            NS_RELEASE(eventQService);
            NS_RELEASE(eventQueue);
         }






         GtkWidget *window;
         
         /* This is called in all GTK applications. Arguments are parsed
          * from the command line and are returned to the application. */
         gtk_init(&argc, &argv);
         
         /* create a new window */
         window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
         
         gtk_window_set_title (GTK_WINDOW (window), "Embedding is Fun!");
         gtk_container_set_border_width (GTK_CONTAINER (window), 10);


         gtk_widget_realize (window);

         WebBrowser *browser = new WebBrowser();
         if (! browser)
             return -1;

         browser->Init(window,nsnull);
         browser->SetPositionAndSize(0, 320, 400, 400);
         browser->GoTo("http://people.netscape.com");
       
         gtk_widget_show (window); 
 
         /* All GTK applications must have a gtk_main(). Control ends here
          * and waits for an event to occur (like a key press or
          * mouse event). */
         gtk_main ();
         
         return(0);
     }
