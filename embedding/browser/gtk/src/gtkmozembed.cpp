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

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

class GtkMozEmbedPrivate
{
public:
  nsCOMPtr<nsIWebBrowser>     webBrowser;
  nsCOMPtr<nsIWebNavigation>  navigation;
  nsCOMPtr<nsIGtkEmbed>       embed;
  nsCString		      mInitialURL;
};

/* signals */

enum {
  LINK_MESSAGE,
  JS_STATUS,
  LOCATION,
  TITLE,
  PROGRESS,
  NET_STATE,
  NET_START,
  NET_STOP,
  NEW_WINDOW,
  VISIBILITY,
  DESTROY_BROWSER,
  OPEN_URI,
  LAST_SIGNAL
};

static guint moz_embed_signals[LAST_SIGNAL] = { 0 };

static char *component_path = NULL;
static gint  num_widgets = 0;
static gint  io_identifier = 0;

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

/* GtkObject methods */
static void
gtk_moz_embed_destroy(GtkObject *object);

/* signal handlers */

static void
gtk_moz_embed_handle_show(GtkWidget *widget, gpointer user_data);

/* event queue callback */

static void
gtk_moz_embed_handle_event_queue(gpointer data, gint source, GdkInputCondition condition);

/* callbacks from various changes in the window */
static void
gtk_moz_embed_handle_link_change(GtkMozEmbed *embed);

static void
gtk_moz_embed_handle_js_status_change(GtkMozEmbed *embed);

static void
gtk_moz_embed_handle_location_change(GtkMozEmbed *embed);

static void
gtk_moz_embed_handle_title_change(GtkMozEmbed *embed);

static void
gtk_moz_embed_handle_progress(GtkMozEmbed *embed, gint32 curprogress, gint32 maxprogress);

static void
gtk_moz_embed_handle_net(GtkMozEmbed *embed, gint flags, guint status);

static nsresult
gtk_moz_embed_handle_new_window(PRUint32 chromeMask, nsIWebBrowser **_retval, void *aData);

static void
gtk_moz_embed_handle_visibility(PRBool aVisibility, void *aData);

static void
gtk_moz_embed_handle_destroy(void *aData);

static PRBool
gtk_moz_embed_handle_open_uri(const char *aURI, void *aData);

static gboolean
gtk_moz_embed_startup_xpcom(void);

static void
gtk_moz_embed_shutdown_xpcom(void);

static GtkBinClass *parent_class;

// we use this for adding callbacks to the C++ code that doesn't know
// anything about the GtkMozEmbed class
typedef void (*generic_cb_with_data) (void *);

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
  moz_embed_signals[NET_STATE] =
    gtk_signal_new("net_state",
		   GTK_RUN_FIRST,
		   object_class->type,
		   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, net_state),
		   gtk_marshal_NONE__INT_INT,
		   GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_UINT);
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
  // create an nsIWebBrowser object
  embed_private->webBrowser = do_CreateInstance(NS_WEBBROWSER_PROGID);
  g_return_if_fail(embed_private->webBrowser);
  // create our glue widget
  GtkMozEmbedChrome *chrome = new GtkMozEmbedChrome();
  g_return_if_fail(chrome);
  embed_private->embed = do_QueryInterface((nsISupports *)(nsIGtkEmbed *) chrome);
  g_return_if_fail(embed_private->embed);
  // hide it
  embed->data = embed_private;
  // this is how we hook into when show() is called on the widget
  gtk_signal_connect(GTK_OBJECT(embed), "show",
		     GTK_SIGNAL_FUNC(gtk_moz_embed_handle_show), NULL);
  // get our hands on the browser chrome
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_QueryInterface(embed_private->embed);
  g_return_if_fail(browserChrome);
  // set the toplevel window
  embed_private->webBrowser->SetTopLevelWindow(browserChrome);
  // set the widget as the owner of the object
  embed_private->embed->Init(GTK_WIDGET(embed));

  // so the embedding widget can find it's owning nsIWebBrowser object
  browserChrome->SetWebBrowser(embed_private->webBrowser);

  // track the window changes
  embed_private->embed->SetLinkChangeCallback((generic_cb_with_data)gtk_moz_embed_handle_link_change,
					      embed);
  embed_private->embed->SetJSStatusChangeCallback((generic_cb_with_data)gtk_moz_embed_handle_js_status_change,
						  embed);
  embed_private->embed->SetLocationChangeCallback((generic_cb_with_data)gtk_moz_embed_handle_location_change,
						  embed);
  embed_private->embed->SetTitleChangeCallback((generic_cb_with_data)gtk_moz_embed_handle_title_change,
					       embed);
  embed_private->embed->SetProgressCallback((void (*)(void *, gint, gint))gtk_moz_embed_handle_progress,
					    embed);
  embed_private->embed->SetNetCallback((void (*)(void *, gint, guint))gtk_moz_embed_handle_net,
				       embed);
  embed_private->embed->SetNewBrowserCallback(gtk_moz_embed_handle_new_window, embed);
  embed_private->embed->SetVisibilityCallback(gtk_moz_embed_handle_visibility, embed);
  embed_private->embed->SetDestroyCallback(gtk_moz_embed_handle_destroy, embed);
  embed_private->embed->SetStartOpenCallback(gtk_moz_embed_handle_open_uri, embed);
  // get our hands on a copy of the nsIWebNavigation interface for later
  embed_private->navigation = do_QueryInterface(embed_private->webBrowser);
  g_return_if_fail(embed_private->navigation);

}

GtkWidget *
gtk_moz_embed_new(void)
{
  return GTK_WIDGET(gtk_type_new(gtk_moz_embed_get_type()));
}

void
gtk_moz_embed_set_comp_path    (char *aPath)
{
  // free the old one if we have to
  if (component_path)
    g_free(component_path);
  if (aPath) 
  {
    component_path = g_strdup(aPath);
  }
}

void
gtk_moz_embed_load_url(GtkMozEmbed *embed, const char *url)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;

  // If the widget aint realized, save the url for later
  if (!GTK_WIDGET_REALIZED(embed))
  {
    embed_private->mInitialURL = url;
    return;
  }

  nsString URLString;
  URLString.AssignWithConversion(url);
  embed_private->navigation->LoadURI(URLString.GetUnicode());
}

void
gtk_moz_embed_stop_load (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;

  embed_private->navigation->Stop();
}

gboolean
gtk_moz_embed_can_go_back      (GtkMozEmbed *embed)
{
  PRBool retval = PR_FALSE;
  GtkMozEmbedPrivate *embed_private;

  g_return_val_if_fail ((embed != NULL), FALSE);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), FALSE);

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  embed_private->navigation->GetCanGoBack(&retval);
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
  
  embed_private->navigation->GetCanGoForward(&retval);
  return retval;
}

void
gtk_moz_embed_go_back          (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  embed_private->navigation->GoBack();
}

void
gtk_moz_embed_go_forward       (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  embed_private->navigation->GoForward();
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

  embed_private->embed->OpenStream(base_uri, mime_type);
  embed_private->embed->AppendToStream(data, len);
  embed_private->embed->CloseStream();
}

void
gtk_moz_embed_open_stream      (GtkMozEmbed *embed,
				const char *base_uri, const char *mime_type)
{
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  embed_private->embed->OpenStream(base_uri, mime_type);
}

void
gtk_moz_embed_append_data      (GtkMozEmbed *embed, const char *data, guint32 len)
{
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  embed_private->embed->AppendToStream(data, len);
}

void
gtk_moz_embed_close_stream     (GtkMozEmbed *embed)
{
 GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
 
  embed_private->embed->CloseStream();
}

void
gtk_moz_embed_reload           (GtkMozEmbed *embed, gint32 flags)
{
  GtkMozEmbedPrivate *embed_private;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  embed_private->navigation->Reload(flags);
}

void
gtk_moz_embed_set_chrome_mask (GtkMozEmbed *embed, guint32 flags)
{
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_QueryInterface(embed_private->embed);
  g_return_if_fail(browserChrome);
  browserChrome->SetChromeMask(flags);
}

guint32
gtk_moz_embed_get_chrome_mask  (GtkMozEmbed *embed)
{
  GtkMozEmbedPrivate *embed_private;
  PRUint32 curMask = 0;
  
  g_return_val_if_fail ((embed != NULL), 0);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), 0);

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = do_QueryInterface(embed_private->embed);
  g_return_val_if_fail(browserChrome, 0);
  if (browserChrome->GetChromeMask(&curMask) == NS_OK)
    return curMask;
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

  embed_private->embed->GetLinkMessage(&retval);

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

  embed_private->embed->GetJSStatus(&retval);

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

  embed_private->embed->GetTitleChar(&retval);

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

  embed_private->embed->GetLocation(&retval);

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

  *retval = embed_private->webBrowser.get();
  NS_IF_ADDREF(*retval);
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

  // init our window
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = do_QueryInterface(embed_private->webBrowser);
  g_return_if_fail(webBrowserBaseWindow);
  webBrowserBaseWindow->InitWindow(widget, NULL, 0, 0,
				   widget->allocation.width, widget->allocation.height);
  webBrowserBaseWindow->Create();
  PRBool visibility;
  webBrowserBaseWindow->GetVisibility(&visibility);
  // set our webBrowser object as the content listener object
  nsCOMPtr<nsIURIContentListener> uriListener;
  uriListener = do_QueryInterface(embed_private->embed);
  g_return_if_fail(uriListener);
  embed_private->webBrowser->SetParentURIContentListener(uriListener);

  // If an initial url was stored, load it
  if (embed_private->mInitialURL.Length() > 0)
  {
	  const char * foo = (const char *) embed_private->mInitialURL;
	  gtk_moz_embed_load_url (GTK_MOZ_EMBED(widget), foo);
	  embed_private->mInitialURL = "";
  }
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

  if (embed_private)
  {
    embed_private->webBrowser = nsnull;
    embed_private->embed = nsnull;
    // XXX XXX delete all the members of the topLevelWindows
    // nsVoidArray and then delete the array
    delete embed_private;
    embed->data = NULL;
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

#ifdef DEBUG_blizzard
  g_print("gtk_moz_embed_size allocate for %p to %d %d %d %d\n", widget, 
	  allocation->x, allocation->y, allocation->width, allocation->height);
#endif

  if (GTK_WIDGET_REALIZED(widget))
  {
    gdk_window_move_resize(widget->window,
			   allocation->x, allocation->y,
			   allocation->width, allocation->height);
    // set the size of the base window
    nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = do_QueryInterface(embed_private->webBrowser);
    webBrowserBaseWindow->SetPositionAndSize(0, 0, allocation->width, allocation->height, PR_TRUE);
    nsCOMPtr<nsIBaseWindow> embedBaseWindow = do_QueryInterface(embed_private->embed);
    embedBaseWindow->SetPositionAndSize(0, 0, allocation->width, allocation->height, PR_TRUE);
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
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = do_QueryInterface(embed_private->webBrowser);
  g_return_if_fail(webBrowserBaseWindow);
  // show it
  webBrowserBaseWindow->SetVisibility(PR_TRUE);
  // show the widget window
  gdk_window_show(widget->window);
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
    nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = do_QueryInterface(embed_private->webBrowser);
    g_return_if_fail(webBrowserBaseWindow);
    webBrowserBaseWindow->Destroy();
    embed_private->webBrowser = nsnull;
    embed_private->embed = nsnull;
    // XXX XXX delete all the members of the topLevelWindows
    // nsVoidArray and then delete the array
    delete embed_private;
    embed->data = NULL;
  }

  num_widgets--;

  // see if we need to shutdown XPCOM
  if (num_widgets == 0)
  {
    gtk_moz_embed_shutdown_xpcom();
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
}

static void
gtk_moz_embed_handle_event_queue(gpointer data, gint source, GdkInputCondition condition)
{
  nsIEventQueue *eventQueue = (nsIEventQueue *)data;
  eventQueue->ProcessPendingEvents();
}

static void
gtk_moz_embed_handle_link_change(GtkMozEmbed *embed)
{
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[LINK_MESSAGE]);
}

static void
gtk_moz_embed_handle_js_status_change(GtkMozEmbed *embed)
{
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[JS_STATUS]);
}

static void
gtk_moz_embed_handle_location_change(GtkMozEmbed *embed)
{
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[LOCATION]);

}

static void
gtk_moz_embed_handle_title_change(GtkMozEmbed *embed)
{
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[TITLE]);
}

static void
gtk_moz_embed_handle_progress(GtkMozEmbed *embed, gint32 curprogress, gint32 maxprogress)
{
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));
  
  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[PROGRESS], curprogress, maxprogress);
}

static void
gtk_moz_embed_handle_net(GtkMozEmbed *embed, gint32 flags, guint32 status)
{
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  // if we've got the start flag, emit the signal
  if ((flags & GTK_MOZ_EMBED_FLAG_IS_NETWORK) && (flags & GTK_MOZ_EMBED_FLAG_START))
    gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[NET_START]);
  // for people who know what they are doing
  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[NET_STATE], flags, status);
  // and for stop, too
  if ((flags & GTK_MOZ_EMBED_FLAG_IS_NETWORK) && (flags & GTK_MOZ_EMBED_FLAG_STOP))
    gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[NET_STOP]);

}

nsresult
gtk_moz_embed_handle_new_window(PRUint32 chromeMask, nsIWebBrowser **_retval, void *aData)
{
  GtkMozEmbed *newEmbed = NULL;
  GtkMozEmbed *embed = (GtkMozEmbed *)aData;
  g_return_val_if_fail(GTK_IS_MOZ_EMBED(embed), NS_ERROR_FAILURE);
  g_print("handle_new_window\n");
  g_print("retval is %p chromemask is %d\n", &newEmbed, chromeMask);
  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[NEW_WINDOW], &newEmbed, (guint)chromeMask);
  if (newEmbed)
  {
    // The window _must_ be realized before we pass it back to the
    // function that created it. Functions that create new windows
    // will do things like GetDocShell() and the widget has to be
    // realized before that can happen.
    gtk_widget_realize(GTK_WIDGET(newEmbed));
    GtkMozEmbedPrivate *embed_private = (GtkMozEmbedPrivate *)newEmbed->data;
    nsIWebBrowser *webBrowser = embed_private->webBrowser.get();
    NS_ADDREF(webBrowser);
    *_retval = webBrowser;
    g_print("created new window.\n");
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

void
gtk_moz_embed_handle_visibility(PRBool aVisibility, void *aData)
{
  GtkMozEmbed *embed = (GtkMozEmbed *)aData;
  g_return_if_fail(GTK_IS_MOZ_EMBED(embed));
  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[VISIBILITY], aVisibility);
}

void
gtk_moz_embed_handle_destroy(void *aData)
{
  GtkMozEmbed *embed = (GtkMozEmbed *)aData;
  g_return_if_fail(GTK_IS_MOZ_EMBED(embed));
  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[DESTROY_BROWSER]);
}

PRBool
gtk_moz_embed_handle_open_uri(const char *aURI, void *aData)
{
  GtkMozEmbed *embed = (GtkMozEmbed *)aData;
  gint return_val = PR_FALSE;
  g_return_val_if_fail(GTK_IS_MOZ_EMBED(embed), PR_FALSE);
  gtk_signal_emit(GTK_OBJECT(embed), moz_embed_signals[OPEN_URI], aURI, &return_val);
  return return_val;
}

gboolean
gtk_moz_embed_startup_xpcom(void)
{
  nsresult rv;
  // init our embedding
  rv = NS_InitEmbedding(component_path);
  if (NS_FAILED(rv))
    return FALSE;
  // set up the thread event queue
  nsIEventQueueService* eventQService;
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
				    NS_GET_IID(nsIEventQueueService),
				    (nsISupports **)&eventQService);
  if (NS_OK == rv)
  {
    // get our hands on the thread event queue
    nsIEventQueue *eventQueue;
    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQueue);
    if (NS_FAILED(rv))
      return FALSE;
    
    io_identifier = gdk_input_add(eventQueue->GetEventQueueSelectFD(),
				  GDK_INPUT_READ,
				  gtk_moz_embed_handle_event_queue,
				  eventQueue);
    NS_RELEASE(eventQService);
    NS_RELEASE(eventQueue);
  }
  return TRUE;
}

void
gtk_moz_embed_shutdown_xpcom(void)
{
  nsresult rv;
  // remove the IO handler for the thread event queue
  gdk_input_remove(io_identifier);
  io_identifier = 0;
  // shut down XPCOM
  NS_TermEmbedding();
}

