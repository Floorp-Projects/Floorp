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
#include "gtkmozembed.h"
#include "nsIWebBrowser.h"
#include "nsCWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "GtkMozEmbedChrome.h"
#include "nsIComponentManager.h"
#include "nsIWebNavigation.h"
#include "nsString.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kWebBrowserCID, NS_WEBBROWSER_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

class GtkMozEmbedPrivate
{
public:
  nsCOMPtr<nsIWebBrowser>  webBrowser;
  nsCOMPtr<nsIGtkEmbed>    embed;
  GdkSuperWin             *superwin;
};

static void
gtk_moz_embed_class_init(GtkMozEmbedClass *klass);

static void
gtk_moz_embed_init(GtkMozEmbed *embed);

/* GtkWidget methods */

static void
gtk_moz_embed_realize(GtkWidget *widget);

static void
gtk_moz_embed_size_allocate(GtkWidget *widget, GtkAllocation *allocation);

/* GtkObject methods */
static void
gtk_moz_embed_destroy(GtkObject *object);

/* signal handlers */

static void
gtk_moz_embed_handle_show(GtkWidget *widget, gpointer user_data);

/* event queue callback */

static void
gtk_moz_embed_handle_event_queue(gpointer data, gint source, GdkInputCondition condition);

static GtkWidgetClass *parent_class;

static PRBool NS_SetupRegistryCalled = PR_FALSE;
static PRBool ThreadQueueSetup       = PR_FALSE;

extern "C" void NS_SetupRegistry();

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
    moz_embed_type = gtk_type_unique(GTK_TYPE_WIDGET, &moz_embed_info);
  }
  return moz_embed_type;
}

static void
gtk_moz_embed_class_init(GtkMozEmbedClass *klass)
{
  GtkWidgetClass *widget_class;
  GtkObjectClass *object_class;
  
  widget_class = GTK_WIDGET_CLASS(klass);
  object_class = GTK_OBJECT_CLASS(klass);

  parent_class = (GtkWidgetClass *)gtk_type_class(gtk_widget_get_type());

  widget_class->realize = gtk_moz_embed_realize;
  widget_class->size_allocate = gtk_moz_embed_size_allocate;

  object_class->destroy = gtk_moz_embed_destroy;

  // check to see if NS_SetupRegistry has been called
  if (!NS_SetupRegistryCalled)
  {
    NS_SetupRegistry();
    NS_SetupRegistryCalled = PR_TRUE;
  }
  // check to see if we have to set up our thread event queue
  if (!ThreadQueueSetup)
  {
    nsIEventQueueService* eventQService;
    nsresult rv;
    rv = nsServiceManager::GetService(kEventQueueServiceCID,
				      NS_GET_IID(nsIEventQueueService),
				      (nsISupports **)&eventQService);
    if (NS_OK == rv)
    {
      // create the event queue
      rv = eventQService->CreateThreadEventQueue();
      if (!NS_SUCCEEDED(rv))
	g_print("Warning: Could not create the event queue for the the thread");

      nsIEventQueue *eventQueue;
      rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQueue);
      if (!NS_SUCCEEDED(rv))
	g_print("Warning: Could not create the thread event queue\n");

      gdk_input_add(eventQueue->GetEventQueueSelectFD(),
		    GDK_INPUT_READ,
		    gtk_moz_embed_handle_event_queue,
		    eventQueue);
      NS_RELEASE(eventQService);
    }
    ThreadQueueSetup = PR_TRUE;
    
  }
}

static void
gtk_moz_embed_init(GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;
  // create our private struct
  embed_private = new GtkMozEmbedPrivate();
  // create an nsIWebBrowser object
  embed_private->webBrowser = do_CreateInstance(kWebBrowserCID);
  // create our glue widget
  GtkMozEmbedChrome *chrome = new GtkMozEmbedChrome();
  embed_private->embed = do_QueryInterface((nsISupports *)(nsIGtkEmbed *) chrome);
  // we haven't created our superwin yet
  embed_private->superwin = NULL;
  // hide it
  embed->data = embed_private;
  // this is how we hook into when show() is called on the widget
  gtk_signal_connect(GTK_OBJECT(embed), "show",
		     GTK_SIGNAL_FUNC(gtk_moz_embed_handle_show), NULL);
  // set our tree owner here
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_QueryInterface(embed_private->embed);
  if (!browserChrome)
    g_print("Warning: Failed to QI embed window to nsIWebBrowserChrome\n");

  embed_private->webBrowser->SetTopLevelWindow(browserChrome);


}

GtkWidget *
gtk_moz_embed_new(void)
{
  return GTK_WIDGET(gtk_type_new(gtk_moz_embed_get_type()));
}

void
gtk_moz_embed_load_url(GtkWidget *widget, char *url)
{
  GtkMozEmbed        *embed;
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);
  embed_private = (GtkMozEmbedPrivate *)embed->data;
  nsCOMPtr<nsIWebNavigation> navigation = do_QueryInterface(embed_private->webBrowser);
  if (!navigation)
    g_print("Warning: failed to QI webBrowser to nsIWebNavigation\n");
  nsString URLString;
  URLString.SetString(url);
  navigation->LoadURI(URLString.GetUnicode());
}

void
gtk_moz_embed_realize(GtkWidget *widget)
{
  GtkMozEmbed        *embed;
  GtkMozEmbedPrivate *embed_private;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);

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

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, embed);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  // now that we're realized, set up the nsIWebBrowser and nsIBaseWindow stuff
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  // create our superwin
  embed_private->superwin = gdk_superwin_new(widget->window, 0, 0,
					     widget->allocation.width, widget->allocation.height);

  // check to see if we're supposed to be already visible
  if (GTK_WIDGET_VISIBLE(widget))
  {
    gdk_window_show(embed_private->superwin->bin_window);
    gdk_window_show(embed_private->superwin->shell_window);
    gdk_window_show(widget->window);
  }

  // init our window
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = do_QueryInterface(embed_private->webBrowser);
  webBrowserBaseWindow->InitWindow(embed_private->superwin, NULL, 0, 0,
				   widget->allocation.width, widget->allocation.height);
  webBrowserBaseWindow->Create();
  PRBool visibility;
  webBrowserBaseWindow->GetVisibility(&visibility);
  if (GTK_WIDGET_VISIBLE(widget))
  {
    webBrowserBaseWindow->SetVisibility(PR_TRUE);
  }

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
  if (GTK_WIDGET_REALIZED(widget))
  {
    gdk_window_move_resize(widget->window,
			   allocation->x, allocation->y,
			   allocation->width, allocation->height);
    gdk_superwin_resize(embed_private->superwin, allocation->width, allocation->height);
    // set the size of the base window
    nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = do_QueryInterface(embed_private->webBrowser);
    webBrowserBaseWindow->SetPositionAndSize(0, 0, allocation->width, allocation->height, PR_TRUE);
  }
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
    embed_private->webBrowser = nsnull;
    embed_private->embed = nsnull;
    gtk_object_unref(GTK_OBJECT(embed_private->superwin));
    delete embed_private;
    embed->data = NULL;
  }
}

static void
gtk_moz_embed_handle_show(GtkWidget *widget, gpointer user_data)
{
  GtkMozEmbed        *embed;
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);
  embed_private = (GtkMozEmbedPrivate *)embed->data;

  // sometimes we get the show signal before we are actually realized
  if (embed_private->superwin) 
  {
    gdk_window_show(embed_private->superwin->bin_window);
    gdk_window_show(embed_private->superwin->shell_window);
  }
}

static void
gtk_moz_embed_handle_event_queue(gpointer data, gint source, GdkInputCondition condition)
{
  nsIEventQueue *eventQueue = (nsIEventQueue *)data;
  eventQueue->ProcessPendingEvents();
}
