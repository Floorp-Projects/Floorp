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

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// forward declaration for our class
class GtkMozEmbedPrivate;

class GtkMozEmbedListenerImpl : public GtkEmbedListener
{
public:
  GtkMozEmbedListenerImpl();
  virtual ~GtkMozEmbedListenerImpl();
  void     Init(GtkMozEmbed *aEmbed);
  nsresult NewBrowser(PRUint32 chromeMask, 
		      nsIWebBrowser **_retval);
  void     Destroy(void);
  void     Visibility(PRBool aVisibility);
  void     Message(GtkEmbedListenerMessageType aType,
		   const char *aMessage);
  void     ProgressChange(nsIWebProgress *aProgress,
			  nsIRequest *aRequest,
			  PRInt32 curSelfProgress,
			  PRInt32 maxSelfProgress,
			  PRInt32 curTotalProgress,
			  PRInt32 maxTotalProgress);
  void     Net(nsIWebProgress *aProgress,
	       nsIRequest *aRequest,
	       PRInt32 aFlags, nsresult aStatus);
  PRBool   StartOpen(const char *aURI);
  void     SizeTo(PRInt32 width, PRInt32 height);
private:
  GtkMozEmbed *mEmbed;
  GtkMozEmbedPrivate *mEmbedPrivate;
};

class GtkMozEmbedPrivate
{
public:
  GtkMozEmbedPrivate();
  nsCOMPtr<nsIWebBrowser>     webBrowser;
  nsCOMPtr<nsIWebNavigation>  navigation;
  nsCOMPtr<nsIGtkEmbed>       embed;
  nsCString		      currentURI;
  GtkMozEmbedListenerImpl     listener;
  GdkWindow                  *mozWindow;
};

GtkMozEmbedPrivate::GtkMozEmbedPrivate() : mozWindow(0)
{
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
  LAST_SIGNAL
};

static guint moz_embed_signals[LAST_SIGNAL] = { 0 };
static GdkWindow *offscreen_window = 0;

static char *component_path = 0;
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

static void
gtk_moz_embed_unmap(GtkWidget *widget);

/* GtkObject methods */
static void
gtk_moz_embed_destroy(GtkObject *object);

/* event queue callback */

static void
gtk_moz_embed_handle_event_queue(gpointer data, gint source,
				 GdkInputCondition condition);

/* startup and shutdown xpcom */

static gboolean
gtk_moz_embed_startup_xpcom(void);

static void
gtk_moz_embed_shutdown_xpcom(void);

/* utility functions */
void
gtk_moz_embed_convert_request_to_uri_string(char **aString, 
					    nsIRequest *aRequest);

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
  embed_private->embed = 
    do_QueryInterface((nsISupports *)(nsIGtkEmbed *) chrome);
  g_return_if_fail(embed_private->embed);
  // hide it
  embed->data = embed_private;

  // get our hands on the browser chrome
  nsCOMPtr<nsIWebBrowserChrome> browserChrome =
    do_QueryInterface(embed_private->embed);
  g_return_if_fail(browserChrome);
  // set the toplevel window
  embed_private->webBrowser->SetTopLevelWindow(browserChrome);
  // set the widget as the owner of the object
  embed_private->embed->Init(GTK_WIDGET(embed));

  // set up the callback object so that it knows how to call back to
  // this widget.
  embed_private->listener.Init(embed);
  // set our listener for the chrome
  embed_private->embed->SetEmbedListener(&embed_private->listener);

  // so the embedding widget can find it's owning nsIWebBrowser object
  browserChrome->SetWebBrowser(embed_private->webBrowser);

  // get our hands on a copy of the nsIWebNavigation interface for later
  embed_private->navigation = do_QueryInterface(embed_private->webBrowser);
  g_return_if_fail(embed_private->navigation);
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
  embed_private->currentURI.Assign(url);

  // If the widget isn't realized, just return.
  if (!GTK_WIDGET_REALIZED(embed))
    return;

  nsString uriString;
  uriString.AssignWithConversion(embed_private->currentURI);
  embed_private->navigation->LoadURI(uriString.GetUnicode());
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
gtk_moz_embed_open_stream (GtkMozEmbed *embed,
			   const char *base_uri, const char *mime_type)
{
  GtkMozEmbedPrivate *embed_private;
  
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embed_private = (GtkMozEmbedPrivate *)embed->data;
  
  embed_private->embed->OpenStream(base_uri, mime_type);
}

void
gtk_moz_embed_append_data (GtkMozEmbed *embed, const char *data, guint32 len)
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
  nsCOMPtr<nsIWebBrowserChrome> browserChrome =
    do_QueryInterface(embed_private->embed);
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
  nsCOMPtr<nsIWebBrowserChrome> browserChrome = 
    do_QueryInterface(embed_private->embed);
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
  if (embed_private->mozWindow)
  {
    gdk_window_reparent(embed_private->mozWindow, widget->window,
			widget->allocation.x, widget->allocation.y);
    return;
  }

  // now that we're realized, set up the nsIWebBrowser and nsIBaseWindow stuff
  // init our window
  nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow =
    do_QueryInterface(embed_private->webBrowser);
  g_return_if_fail(webBrowserBaseWindow);
  webBrowserBaseWindow->InitWindow(widget, NULL, 0, 0,
				   widget->allocation.width,
				   widget->allocation.height);
  webBrowserBaseWindow->Create();
  // save the id of the mozilla window for later if we need it.
  nsCOMPtr<nsIWidget> mozWindow;
  webBrowserBaseWindow->GetMainWidget(getter_AddRefs(mozWindow));
  // get the native drawing area
  GdkWindow *tmp_window = (GdkWindow *)
    mozWindow->GetNativeData(NS_NATIVE_WINDOW);
  // and, thanks to superwin we actually need the parent of that.
  tmp_window = gdk_window_get_parent(tmp_window);
  embed_private->mozWindow = tmp_window;
  // set our webBrowser object as the content listener object
  nsCOMPtr<nsIURIContentListener> uriListener;
  uriListener = do_QueryInterface(embed_private->embed);
  g_return_if_fail(uriListener);
  embed_private->webBrowser->SetParentURIContentListener(uriListener);

  if (embed_private->currentURI.Length() > 0)
  {
    // we have to make a copy here since load_url will replace the
    // string that we would be passing in and replace itself with a
    // null.  nasty.
    char * initialURI = 
      g_strdup((const char *) embed_private->currentURI);
       gtk_moz_embed_load_url (GTK_MOZ_EMBED(widget), initialURI);
    g_free(initialURI);
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

  GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
  
  gdk_window_reparent(embed_private->mozWindow,
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
    // set the size of the base window
    nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow =
      do_QueryInterface(embed_private->webBrowser);
    webBrowserBaseWindow->SetPositionAndSize(0, 0, allocation->width,
					     allocation->height, PR_TRUE);
    nsCOMPtr<nsIBaseWindow> embedBaseWindow =
      do_QueryInterface(embed_private->embed);
    embedBaseWindow->SetPositionAndSize(0, 0, allocation->width, 
					allocation->height, PR_TRUE);
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
    do_QueryInterface(embed_private->webBrowser);
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
    nsCOMPtr<nsIBaseWindow> webBrowserBaseWindow = 
      do_QueryInterface(embed_private->webBrowser);
    g_return_if_fail(webBrowserBaseWindow);
    webBrowserBaseWindow->Destroy();
    embed_private->mozWindow = 0;
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
    if (offscreen_window)
      gtk_moz_embed_destroy_offscreen_window();
    gtk_moz_embed_shutdown_xpcom();
  }
}

static void
gtk_moz_embed_handle_event_queue(gpointer data, gint source,
				 GdkInputCondition condition)
{
  nsIEventQueue *eventQueue = (nsIEventQueue *)data;
  eventQueue->ProcessPendingEvents();
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
  nsCOMPtr <nsIEventQueueService> eventQService = 
    do_GetService(kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv))
    return FALSE;
  // get our hands on the thread event queue
  nsCOMPtr<nsIEventQueue> eventQueue;
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                         getter_AddRefs(eventQueue));
  if (NS_FAILED(rv))
    return FALSE;

  io_identifier = gdk_input_add(eventQueue->GetEventQueueSelectFD(),
				GDK_INPUT_READ,
				gtk_moz_embed_handle_event_queue,
				eventQueue);
  return TRUE;
}

void
gtk_moz_embed_shutdown_xpcom(void)
{
  if (io_identifier)
  {
    // remove the IO handler for the thread event queue
    gdk_input_remove(io_identifier);
    io_identifier = 0;
    // shut down XPCOM
    NS_TermEmbedding();
  }
}

// this function takes an nsIRequest object and tries to get the
// string for it.
void
gtk_moz_embed_convert_request_to_uri_string(char **aString,
					    nsIRequest *aRequest)
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

void
gtk_moz_embed_create_offscreen_window(void)
{
  GdkWindowAttr attributes;

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
				    nsIWebBrowser **_retval)
{
  GtkMozEmbed *newEmbed = NULL;
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NEW_WINDOW],
		  &newEmbed, (guint)chromeMask);
  if (newEmbed)  {
    // The window _must_ be realized before we pass it back to the
    // function that created it. Functions that create new windows
    // will do things like GetDocShell() and the widget has to be
    // realized before that can happen.
    gtk_widget_realize(GTK_WIDGET(newEmbed));
    GtkMozEmbedPrivate *embed_private = (GtkMozEmbedPrivate *)newEmbed->data;
    nsIWebBrowser *webBrowser = embed_private->webBrowser.get();
    NS_ADDREF(webBrowser);
    *_retval = webBrowser;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
  case MessageLocation:
    // update the currentURI for this
    mEmbedPrivate->currentURI = aMessage;
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[LOCATION]);
    break;
  case MessageTitle:
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[TITLE]);
    break;
  default:
    break;
  }
}


void
GtkMozEmbedListenerImpl::ProgressChange(nsIWebProgress *aProgress,
					nsIRequest *aRequest,
					PRInt32 curSelfProgress,
					PRInt32 maxSelfProgress,
					PRInt32 curTotalProgress,
					PRInt32 maxTotalProgress)
{
  nsXPIDLCString uriString;
  gtk_moz_embed_convert_request_to_uri_string(getter_Copies(uriString),
					      aRequest);

  if (mEmbedPrivate->currentURI.Equals(uriString)) {
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[PROGRESS],
		    curTotalProgress, maxTotalProgress);
  }
  
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[PROGRESS_ALL],
		  (const char *)uriString,
		  curTotalProgress, maxTotalProgress);
}

void
GtkMozEmbedListenerImpl::Net(nsIWebProgress *aProgress,
			     nsIRequest *aRequest,
			     PRInt32 aFlags, nsresult aStatus)
{
  // if we've got the start flag, emit the signal
  if ((aFlags & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT) && 
      (aFlags & GTK_MOZ_EMBED_FLAG_START))
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NET_START]);

  nsXPIDLCString uriString;
  gtk_moz_embed_convert_request_to_uri_string(getter_Copies(uriString),
					      aRequest);
  if (mEmbedPrivate->currentURI.Equals(uriString)) {
    // for people who know what they are doing
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NET_STATE],
		    aFlags, aStatus);
  }
  gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NET_STATE_ALL],
		  (const char *)uriString, aFlags, aStatus);
  // and for stop, too
  if ((aFlags & GTK_MOZ_EMBED_FLAG_IS_DOCUMENT) && 
      (aFlags & GTK_MOZ_EMBED_FLAG_STOP))
    gtk_signal_emit(GTK_OBJECT(mEmbed), moz_embed_signals[NET_STOP]);

}

PRBool
GtkMozEmbedListenerImpl::StartOpen(const char *aURI)
{
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
