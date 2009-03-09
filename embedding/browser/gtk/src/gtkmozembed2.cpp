/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Ramiro Estrugo <ramiro@eazel.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>

#include "gtkmozembed.h"
#include "gtkmozembedprivate.h"
#include "gtkmozembed_internal.h"

#include "EmbedPrivate.h"
#include "EmbedWindow.h"

// so we can do our get_nsIWebBrowser later...
#include "nsIWebBrowser.h"

#include "gtkmozembedmarshal.h"

#define NEW_TOOLKIT_STRING(x) g_strdup(NS_ConvertUTF16toUTF8(x).get())
#define GET_OBJECT_CLASS_TYPE(x) G_OBJECT_CLASS_TYPE(x)

class nsIDirectoryServiceProvider;

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

#ifdef MOZ_ACCESSIBILITY_ATK
static AtkObject* gtk_moz_embed_get_accessible (GtkWidget *widget);
#endif

static gint
handle_child_focus_in(GtkWidget     *aWidget,
		      GdkEventFocus *aGdkFocusEvent,
		      GtkMozEmbed   *aEmbed);

static gint
handle_child_focus_out(GtkWidget     *aWidget,
		       GdkEventFocus *aGdkFocusEvent,
		       GtkMozEmbed   *aEmbed);

// globals for this type of widget

static GtkBinClass *embed_parent_class;

guint moz_embed_signals[EMBED_LAST_SIGNAL] = { 0 };

// GtkObject + class-related functions

GType
gtk_moz_embed_get_type(void)
{
  static GType moz_embed_type = 0;
  if (moz_embed_type == 0)
  {
    const GTypeInfo our_info =
    {
      sizeof(GtkMozEmbedClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc)gtk_moz_embed_class_init,
      NULL,
      NULL, /* class_data */
      sizeof(GtkMozEmbed),
      0, /* n_preallocs */
      (GInstanceInitFunc)gtk_moz_embed_init,
    };

    moz_embed_type = g_type_register_static(GTK_TYPE_BIN,
                                            "GtkMozEmbed",
                                            &our_info,
                                            (GTypeFlags)0);
  }

  return moz_embed_type;
}

static void
gtk_moz_embed_class_init(GtkMozEmbedClass *klass)
{
  GtkContainerClass  *container_class;
  GtkWidgetClass     *widget_class;
  GtkObjectClass     *object_class;

  container_class = GTK_CONTAINER_CLASS(klass);
  widget_class    = GTK_WIDGET_CLASS(klass);
  object_class    = GTK_OBJECT_CLASS(klass);

  embed_parent_class = (GtkBinClass *)g_type_class_peek(gtk_bin_get_type());

  widget_class->realize = gtk_moz_embed_realize;
  widget_class->unrealize = gtk_moz_embed_unrealize;
  widget_class->size_allocate = gtk_moz_embed_size_allocate;
  widget_class->map = gtk_moz_embed_map;
  widget_class->unmap = gtk_moz_embed_unmap;

#ifdef MOZ_ACCESSIBILITY_ATK
  widget_class->get_accessible = gtk_moz_embed_get_accessible;
#endif

  object_class->destroy = gtk_moz_embed_destroy;

  // set up our signals

  moz_embed_signals[LINK_MESSAGE] =
    g_signal_new("link_message",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, link_message),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
  moz_embed_signals[JS_STATUS] =
    g_signal_new("js_status",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, js_status),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
  moz_embed_signals[LOCATION] =
    g_signal_new("location",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, location),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
  moz_embed_signals[TITLE] =
    g_signal_new("title",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, title),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
  moz_embed_signals[PROGRESS] =
    g_signal_new("progress",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, progress),
                 NULL, NULL,
                 gtkmozembed_VOID__INT_INT,
                 G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
  moz_embed_signals[PROGRESS_ALL] =
    g_signal_new("progress_all",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, progress_all),
                 NULL, NULL,
                 gtkmozembed_VOID__STRING_INT_INT,
                 G_TYPE_NONE, 3,
                 G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                 G_TYPE_INT, G_TYPE_INT);
  moz_embed_signals[NET_STATE] =
    g_signal_new("net_state",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, net_state),
                 NULL, NULL,
                 gtkmozembed_VOID__INT_UINT,
                 G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_UINT);
  moz_embed_signals[NET_STATE_ALL] =
    g_signal_new("net_state_all",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, net_state_all),
                 NULL, NULL,
                 gtkmozembed_VOID__STRING_INT_UINT,
                 G_TYPE_NONE, 3,
                 G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                 G_TYPE_INT, G_TYPE_UINT);
  moz_embed_signals[NET_START] =
    g_signal_new("net_start",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, net_start),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
  moz_embed_signals[NET_STOP] =
    g_signal_new("net_stop",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, net_stop),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
  moz_embed_signals[NEW_WINDOW] =
    g_signal_new("new_window",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, new_window),
                 NULL, NULL,
                 gtkmozembed_VOID__POINTER_UINT,
                 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_UINT);
  moz_embed_signals[VISIBILITY] =
    g_signal_new("visibility",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, visibility),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__BOOLEAN,
                 G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
  moz_embed_signals[DESTROY_BROWSER] =
    g_signal_new("destroy_browser",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, destroy_brsr),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
  moz_embed_signals[OPEN_URI] =
    g_signal_new("open_uri",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, open_uri),
                 NULL, NULL,
                 gtkmozembed_BOOL__STRING,
                 G_TYPE_BOOLEAN, 1, G_TYPE_STRING |
                 G_SIGNAL_TYPE_STATIC_SCOPE);
  moz_embed_signals[SIZE_TO] =
    g_signal_new("size_to",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, size_to),
                 NULL, NULL,
                 gtkmozembed_VOID__INT_INT,
                 G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
  moz_embed_signals[DOM_KEY_DOWN] =
    g_signal_new("dom_key_down",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_key_down),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_KEY_PRESS] =
    g_signal_new("dom_key_press",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_key_press),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_KEY_UP] =
    g_signal_new("dom_key_up",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_key_up),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_DOWN] =
    g_signal_new("dom_mouse_down",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_mouse_down),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_UP] =
    g_signal_new("dom_mouse_up",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_mouse_up),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_CLICK] =
    g_signal_new("dom_mouse_click",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_mouse_click),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_DBL_CLICK] =
    g_signal_new("dom_mouse_dbl_click",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_mouse_dbl_click),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_OVER] =
    g_signal_new("dom_mouse_over",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_mouse_over),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_OUT] =
    g_signal_new("dom_mouse_out",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_mouse_out),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[SECURITY_CHANGE] =
    g_signal_new("security_change",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, security_change),
                 NULL, NULL,
                 gtkmozembed_VOID__POINTER_UINT,
                 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_UINT);
  moz_embed_signals[STATUS_CHANGE] =
    g_signal_new("status_change",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, status_change),
                 NULL, NULL,
                 gtkmozembed_VOID__POINTER_INT_POINTER,
                 G_TYPE_NONE, 3,
                 G_TYPE_POINTER, G_TYPE_INT, G_TYPE_POINTER);
  moz_embed_signals[DOM_ACTIVATE] =
    g_signal_new("dom_activate",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_activate),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_FOCUS_IN] =
    g_signal_new("dom_focus_in",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_focus_in),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
  moz_embed_signals[DOM_FOCUS_OUT] =
    g_signal_new("dom_focus_out",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkMozEmbedClass, dom_focus_out),
                 NULL, NULL,
                 gtkmozembed_BOOL__POINTER,
                 G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
}

static void
gtk_moz_embed_init(GtkMozEmbed *embed)
{
  EmbedPrivate *priv = new EmbedPrivate();
  embed->data = priv;
  gtk_widget_set_name(GTK_WIDGET(embed), "gtkmozembed");

  GTK_WIDGET_UNSET_FLAGS(GTK_WIDGET(embed), GTK_NO_WINDOW);
}

GtkWidget *
gtk_moz_embed_new(void)
{
  return GTK_WIDGET(g_object_new(GTK_TYPE_MOZ_EMBED, NULL));
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

    // Destroy the widget only if it's been Init()ed.
    if(embedPrivate->mMozWindowWidget != 0) {
      embedPrivate->Destroy();
    }

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

  // initialize the window
  nsresult rv;
  rv = embedPrivate->Init(embed);
  g_return_if_fail(NS_SUCCEEDED(rv));
  
  PRBool alreadyRealized = PR_FALSE;
  rv = embedPrivate->Realize(&alreadyRealized);
  g_return_if_fail(NS_SUCCEEDED(rv));

  // if we're already realized we don't need to hook up to anything below
  if (alreadyRealized)
    return;

  if (embedPrivate->mURI.Length())
    embedPrivate->LoadCurrentURI();

  // connect to the focus out event for the child
  GtkWidget *child_widget = GTK_BIN(widget)->child;
  g_signal_connect_object(G_OBJECT(child_widget),
                          "focus_out_event",
                          G_CALLBACK(handle_child_focus_out),
                          embed,
                          G_CONNECT_AFTER);
  g_signal_connect_object(G_OBJECT(child_widget),
                          "focus_in_event",
                          G_CALLBACK(handle_child_focus_in),
                          embed,
                          G_CONNECT_AFTER);
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

  if (GTK_WIDGET_CLASS(embed_parent_class)->unrealize)
    (* GTK_WIDGET_CLASS(embed_parent_class)->unrealize)(widget);
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

#ifdef MOZ_ACCESSIBILITY_ATK
static AtkObject*
gtk_moz_embed_get_accessible (GtkWidget *widget)
{
  g_return_val_if_fail(widget != NULL, NULL);
  g_return_val_if_fail(GTK_IS_MOZ_EMBED(widget), NULL);

  GtkMozEmbed  *embed = GTK_MOZ_EMBED(widget);;
  EmbedPrivate *embedPrivate = (EmbedPrivate *)embed->data;
  return static_cast<AtkObject *>(
                        embedPrivate->GetAtkObjectForCurrentDocument());
}
#endif /* MOZ_ACCESSIBILITY_ATK */

static gint
handle_child_focus_in(GtkWidget     *aWidget,
		      GdkEventFocus *aGdkFocusEvent,
		      GtkMozEmbed   *aEmbed)
{
  EmbedPrivate *embedPrivate;

  embedPrivate = (EmbedPrivate *)aEmbed->data;

  embedPrivate->ChildFocusIn();

  return FALSE;
}

static gint
handle_child_focus_out(GtkWidget     *aWidget,
		       GdkEventFocus *aGdkFocusEvent,
		       GtkMozEmbed   *aEmbed)
{
  EmbedPrivate *embedPrivate;

  embedPrivate = (EmbedPrivate *)aEmbed->data;

  embedPrivate->ChildFocusOut();
 
  return FALSE;
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
gtk_moz_embed_set_path(const char *aPath)
{
  EmbedPrivate::SetPath(aPath);
}

void
gtk_moz_embed_set_comp_path(const char *aPath)
{
  EmbedPrivate::SetCompPath(aPath);
}

void
gtk_moz_embed_set_app_components(const nsModuleComponentInfo* aComps,
                                 int aNumComponents)
{
  EmbedPrivate::SetAppComponents(aComps, aNumComponents);
}

void
gtk_moz_embed_set_profile_path(const char *aDir, const char *aName)
{
  EmbedPrivate::SetProfilePath(aDir, aName);
}

void
gtk_moz_embed_set_directory_service_provider(nsIDirectoryServiceProvider *appFileLocProvider) {
  EmbedPrivate::SetDirectoryServiceProvider(appFileLocProvider);
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
    embedPrivate->mNavigation->Stop(nsIWebNavigation::STOP_ALL);
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
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  embedPrivate->OpenStream(base_uri, mime_type);
  embedPrivate->AppendToStream((const PRUint8*)data, len);
  embedPrivate->CloseStream();
}

void
gtk_moz_embed_open_stream(GtkMozEmbed *embed, const char *base_uri,
			  const char *mime_type)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));
  g_return_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)));

  embedPrivate = (EmbedPrivate *)embed->data;

  embedPrivate->OpenStream(base_uri, mime_type);
}

void gtk_moz_embed_append_data(GtkMozEmbed *embed, const char *data,
			       guint32 len)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));
  g_return_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)));

  embedPrivate = (EmbedPrivate *)embed->data;
  embedPrivate->AppendToStream((const PRUint8*)data, len);
}

void
gtk_moz_embed_close_stream(GtkMozEmbed *embed)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));
  g_return_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)));

  embedPrivate = (EmbedPrivate *)embed->data;
  embedPrivate->CloseStream();
}

char *
gtk_moz_embed_get_link_message(GtkMozEmbed *embed)
{
  char *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), (char *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (char *)NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mWindow)
    retval = NEW_TOOLKIT_STRING(embedPrivate->mWindow->mLinkMessage);

  return retval;
}

char *
gtk_moz_embed_get_js_status(GtkMozEmbed *embed)
{
  char *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), (char *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (char *)NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mWindow)
    retval = NEW_TOOLKIT_STRING(embedPrivate->mWindow->mJSStatus);

  return retval;
}

char *
gtk_moz_embed_get_title(GtkMozEmbed *embed)
{
  char *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), (char *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (char *)NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mWindow)
    retval = NEW_TOOLKIT_STRING(embedPrivate->mWindow->mTitle);

  return retval;
}

char *
gtk_moz_embed_get_location(GtkMozEmbed *embed)
{
  char *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), (char *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (char *)NULL);

  embedPrivate = (EmbedPrivate *)embed->data;
  
  if (!embedPrivate->mURI.IsEmpty())
    retval = g_strdup(embedPrivate->mURI.get());

  return retval;
}

void
gtk_moz_embed_reload(GtkMozEmbed *embed, gint32 flags)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  PRUint32 reloadFlags = 0;
  
  // map the external API to the internal web navigation API.
  switch (flags) {
  case GTK_MOZ_EMBED_FLAG_RELOADNORMAL:
    reloadFlags = 0;
    break;
  case GTK_MOZ_EMBED_FLAG_RELOADBYPASSCACHE:
    reloadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE;
    break;
  case GTK_MOZ_EMBED_FLAG_RELOADBYPASSPROXY:
    reloadFlags = nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY;
    break;
  case GTK_MOZ_EMBED_FLAG_RELOADBYPASSPROXYANDCACHE:
    reloadFlags = (nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY |
		   nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE);
    break;
  case GTK_MOZ_EMBED_FLAG_RELOADCHARSETCHANGE:
    reloadFlags = nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE;
    break;
  default:
    reloadFlags = 0;
    break;
  }

  embedPrivate->Reload(reloadFlags);
}

void
gtk_moz_embed_set_chrome_mask(GtkMozEmbed *embed, guint32 flags)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  embedPrivate->SetChromeMask(flags);
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

PRUnichar *
gtk_moz_embed_get_title_unichar (GtkMozEmbed *embed)
{
  PRUnichar *retval = nsnull;
  EmbedPrivate *embedPrivate;
                   
  g_return_val_if_fail ((embed != NULL), (PRUnichar *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (PRUnichar *)NULL);
  
  embedPrivate = (EmbedPrivate *)embed->data;
                   
  if (embedPrivate->mWindow)
    retval = ToNewUnicode(embedPrivate->mWindow->mTitle);
                   
  return retval;
}

PRUnichar *
gtk_moz_embed_get_js_status_unichar (GtkMozEmbed *embed)
{
  PRUnichar *retval = nsnull;
  EmbedPrivate *embedPrivate;
                   
  g_return_val_if_fail ((embed != NULL), (PRUnichar *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (PRUnichar *)NULL);
  
  embedPrivate = (EmbedPrivate *)embed->data;
                   
  if (embedPrivate->mWindow)
    retval = ToNewUnicode(embedPrivate->mWindow->mJSStatus);
                   
  return retval;
}

PRUnichar *
gtk_moz_embed_get_link_message_unichar (GtkMozEmbed *embed)
{
  PRUnichar *retval = nsnull;
  EmbedPrivate *embedPrivate;
                   
  g_return_val_if_fail ((embed != NULL), (PRUnichar *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (PRUnichar *)NULL);
  
  embedPrivate = (EmbedPrivate *)embed->data;
                   
  if (embedPrivate->mWindow)
    retval = ToNewUnicode(embedPrivate->mWindow->mLinkMessage);
                   
  return retval;
}

// class and instance initialization

static void
gtk_moz_embed_single_class_init(GtkMozEmbedSingleClass *klass);

static void
gtk_moz_embed_single_init(GtkMozEmbedSingle *embed);

GtkMozEmbedSingle *
gtk_moz_embed_single_new(void);

enum {
  NEW_WINDOW_ORPHAN,
  SINGLE_LAST_SIGNAL
};

guint moz_embed_single_signals[SINGLE_LAST_SIGNAL] = { 0 };

// GtkObject + class-related functions

GType
gtk_moz_embed_single_get_type(void)
{
  static GType moz_embed_single_type = 0;
  if (moz_embed_single_type == 0)
  {
    const GTypeInfo our_info =
    {
      sizeof(GtkMozEmbedClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc)gtk_moz_embed_single_class_init,
      NULL,
      NULL, /* class_data */
      sizeof(GtkMozEmbed),
      0, /* n_preallocs */
      (GInstanceInitFunc)gtk_moz_embed_single_init,
    };

    moz_embed_single_type = g_type_register_static(GTK_TYPE_OBJECT,
                                                   "GtkMozEmbedSingle",
                                                   &our_info,
                                                   (GTypeFlags)0);
  }

  return moz_embed_single_type;
}

static void
gtk_moz_embed_single_class_init(GtkMozEmbedSingleClass *klass)
{
  GtkObjectClass     *object_class;

  object_class    = GTK_OBJECT_CLASS(klass);

  // set up our signals

  moz_embed_single_signals[NEW_WINDOW_ORPHAN] =
    g_signal_new("new_window_orphan",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(GtkMozEmbedSingleClass, new_window_orphan),
                 NULL, NULL,
                 gtkmozembed_VOID__POINTER_UINT,
                 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_UINT);

}

static void
gtk_moz_embed_single_init(GtkMozEmbedSingle *embed)
{
  // this is a placeholder for later in case we need to stash data at
  // a later data and maintain backwards compatibility.
  embed->data = nsnull;
}

GtkMozEmbedSingle *
gtk_moz_embed_single_new(void)
{
  return (GtkMozEmbedSingle *)g_object_new(GTK_TYPE_MOZ_EMBED_SINGLE, NULL);
}

GtkMozEmbedSingle *
gtk_moz_embed_single_get(void)
{
  static GtkMozEmbedSingle *singleton_object = nsnull;
  if (!singleton_object)
  {
    singleton_object = gtk_moz_embed_single_new();
  }

  return singleton_object;
}

// our callback from the window creator service
void
gtk_moz_embed_single_create_window(GtkMozEmbed **aNewEmbed,
				   guint         aChromeFlags)
{
  GtkMozEmbedSingle *single = gtk_moz_embed_single_get();

  *aNewEmbed = nsnull;

  if (!single)
    return;

  g_signal_emit(G_OBJECT(single),
                moz_embed_single_signals[NEW_WINDOW_ORPHAN], 0,
                aNewEmbed, aChromeFlags);

}
