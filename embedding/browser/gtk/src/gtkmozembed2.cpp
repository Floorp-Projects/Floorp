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


#include "gtkmozembed.h"
#include "gtkmozembedprivate.h"

#include "EmbedPrivate.h"
#include "EmbedWindow.h"

// so we can do our get_nsIWebBrowser later...
#include <nsIWebBrowser.h>

// class and instance initialization

static void
gtk_moz_embed_class_init(GtkMozEmbedClass *klass);

static void
gtk_moz_embed_init(GtkMozEmbed *embed);

// GtkObject methods

static void
gtk_moz_embed_destroy(GtkObject *object);

// GtkWidget methods

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

// globals for this type of widget

static GtkBinClass *parent_class;

guint moz_embed_signals[LAST_SIGNAL] = { 0 };

// GtkObject + class-related functions

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
		   GTK_TYPE_NONE, 3, GTK_TYPE_STRING,
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
		   GTK_TYPE_NONE, 3, GTK_TYPE_STRING,
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
		   GTK_TYPE_BOOL, 1, GTK_TYPE_STRING);
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
  EmbedPrivate *priv = new EmbedPrivate();
  embed->data = priv;
}

GtkWidget *
gtk_moz_embed_new(void)
{
  return GTK_WIDGET(gtk_type_new(gtk_moz_embed_get_type()));
}

// GtkObject methods

static void
gtk_moz_embed_destroy(GtkObject *object)
{
  GtkMozEmbed  *embed;
  EmbedPrivate *embedPrivate;
  
  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(object));

  embed = GTK_MOZ_EMBED(object);
  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate) {
    delete embedPrivate;
    embed->data = NULL;
  }
}

// GtkWidget methods

static void
gtk_moz_embed_realize(GtkWidget *widget)
{
  GtkMozEmbed    *embed;
  EmbedPrivate   *embedPrivate;
  GdkWindowAttr   attributes;
  gint            attributes_mask;
  
  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);
  embedPrivate = (EmbedPrivate *)embed->data;

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

  // initialize the widget now that we have a window
  nsresult rv;
  rv = embedPrivate->Init(embed);
  g_return_if_fail(NS_SUCCEEDED(rv));
  rv = embedPrivate->Realize();
  g_return_if_fail(NS_SUCCEEDED(rv));

  if (embedPrivate->mURI.Length())
    embedPrivate->LoadCurrentURI();
}

static void
gtk_moz_embed_unrealize(GtkWidget *widget)
{
  GtkMozEmbed  *embed;
  EmbedPrivate *embedPrivate;
  
  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);
  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate) {
    embedPrivate->Unrealize();
  }
}

static void
gtk_moz_embed_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
  GtkMozEmbed  *embed;
  EmbedPrivate *embedPrivate;
  
  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);
  embedPrivate = (EmbedPrivate *)embed->data;
  
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED(widget))
  {
    gdk_window_move_resize(widget->window,
			   allocation->x, allocation->y,
			   allocation->width, allocation->height);
    embedPrivate->Resize(allocation->width, allocation->height);
  }
}

static void
gtk_moz_embed_map(GtkWidget *widget)
{
  GtkMozEmbed  *embed;
  EmbedPrivate *embedPrivate;
  
  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);
  embedPrivate = (EmbedPrivate *)embed->data;

  GTK_WIDGET_SET_FLAGS(widget, GTK_MAPPED);

  embedPrivate->Show();

  gdk_window_show(widget->window);
  
}

static void
gtk_moz_embed_unmap(GtkWidget *widget)
{
  GtkMozEmbed  *embed;
  EmbedPrivate *embedPrivate;
  
  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(widget));

  embed = GTK_MOZ_EMBED(widget);
  embedPrivate = (EmbedPrivate *)embed->data;

  GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED);

  gdk_window_hide(widget->window);

  embedPrivate->Hide();

}

// Widget methods

void
gtk_moz_embed_push_startup(void)
{
  EmbedPrivate::PushStartup();
}

void
gtk_moz_embed_pop_startup(void)
{
  EmbedPrivate::PopStartup();
}

void
gtk_moz_embed_set_comp_path(char *aPath)
{
  EmbedPrivate::SetCompPath(aPath);
}

void
gtk_moz_embed_set_profile_path(char *aDir, char *aName)
{
  EmbedPrivate::SetProfilePath(aDir, aName);
}

void
gtk_moz_embed_load_url(GtkMozEmbed *embed, const char *url)
{
  EmbedPrivate *embedPrivate;
  
  g_return_if_fail(embed != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  embedPrivate->SetURI(url);

  // If the widget is realized, load the URI.  If it isn't then we
  // will load it later.
  if (GTK_WIDGET_REALIZED(embed))
    embedPrivate->LoadCurrentURI();
}

void
gtk_moz_embed_stop_load(GtkMozEmbed *embed)
{
  EmbedPrivate *embedPrivate;
  
  g_return_if_fail(embed != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mNavigation)
    embedPrivate->mNavigation->Stop();
}

gboolean
gtk_moz_embed_can_go_back(GtkMozEmbed *embed)
{
  PRBool retval = PR_FALSE;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), FALSE);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), FALSE);

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mNavigation)
    embedPrivate->mNavigation->GetCanGoBack(&retval);
  return retval;
}

gboolean
gtk_moz_embed_can_go_forward(GtkMozEmbed *embed)
{
  PRBool retval = PR_FALSE;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), FALSE);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), FALSE);

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mNavigation)
    embedPrivate->mNavigation->GetCanGoForward(&retval);
  return retval;
}

void
gtk_moz_embed_go_back(GtkMozEmbed *embed)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mNavigation)
    embedPrivate->mNavigation->GoBack();
}

void
gtk_moz_embed_go_forward(GtkMozEmbed *embed)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mNavigation)
    embedPrivate->mNavigation->GoForward();
}

void
gtk_moz_embed_render_data(GtkMozEmbed *embed, const char *data,
			  guint32 len, const char *base_uri,
			  const char *mime_type)
{
  // XXX
}

void
gtk_moz_embed_open_stream(GtkMozEmbed *embed, const char *base_uri,
			  const char *mime_type)
{
  // XXX
}

void gtk_moz_embed_append_data(GtkMozEmbed *embed, const char *data,
			       guint32 len)
{
  // XXX
}

void
gtk_moz_embed_close_stream(GtkMozEmbed *embed)
{
  // XXX
}

char *
gtk_moz_embed_get_link_message(GtkMozEmbed *embed)
{
  char *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  retval = embedPrivate->mWindow->mLinkMessage.ToNewCString();

  return retval;
}

char *
gtk_moz_embed_get_js_status(GtkMozEmbed *embed)
{
  char *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  retval = embedPrivate->mWindow->mJSStatus.ToNewCString();

  return retval;
}

char *
gtk_moz_embed_get_title(GtkMozEmbed *embed)
{
  char *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  retval = embedPrivate->mWindow->mTitle.ToNewCString();

  return retval;
}

char *
gtk_moz_embed_get_location(GtkMozEmbed *embed)
{
  char *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), NULL);

  embedPrivate = (EmbedPrivate *)embed->data;
  
  retval = embedPrivate->mURI.ToNewCString();

  return retval;
}

void
gtk_moz_embed_reload(GtkMozEmbed *embed, gint32 flags)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mNavigation)
    embedPrivate->mNavigation->Reload(flags);
}

void
gtk_moz_embed_set_chrome_mask(GtkMozEmbed *embed, guint32 flags)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  embedPrivate->mChromeMask = flags;
}

guint32
gtk_moz_embed_get_chrome_mask(GtkMozEmbed *embed)
{
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), 0);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), 0);

  embedPrivate = (EmbedPrivate *)embed->data;

  return embedPrivate->mChromeMask;
}

void
gtk_moz_embed_get_nsIWebBrowser  (GtkMozEmbed *embed, nsIWebBrowser **retval)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;
  
  if (embedPrivate->mWindow)
    embedPrivate->mWindow->GetWebBrowser(retval);
}
