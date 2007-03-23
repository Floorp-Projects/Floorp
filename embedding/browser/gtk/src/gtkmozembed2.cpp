/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
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
 * Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Ramiro Estrugo <ramiro@eazel.com>
 *   Oleg Romashin <romaxa@gmail.com>
 *   Antonio Gomes <tonikitoo@gmail.com>
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
#include "EmbedContextMenuInfo.h"
#include "EmbedEventListener.h"
#include "EmbedDownloadMgr.h"

#include "nsIWebBrowserPersist.h"
#include "nsCWebBrowserPersist.h"
#include "nsIDOMDocument.h"
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsIFileURL.h"
#include "nsIURI.h"
#include "nsILocalFile.h"
#include "nsIFile.h"
// so we can do our get_nsIWebBrowser later...
#include "nsIWebBrowser.h"

#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsIX509Cert.h"
#include "nsISecureBrowserUI.h"

// for strings
#ifdef MOZILLA_INTERNAL_API
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#else
#include "nsStringAPI.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#endif

#ifdef MOZ_WIDGET_GTK2

#include "gtkmozembedmarshal.h"

#define NEW_TOOLKIT_STRING(x) g_strdup(NS_ConvertUTF16toUTF8(x).get())
#define GET_OBJECT_CLASS_TYPE(x) G_OBJECT_CLASS_TYPE(x)

#endif /* MOZ_WIDGET_GTK2 */

#ifdef MOZ_WIDGET_GTK

// so we can get callbacks from the mozarea
#include <gtkmozarea.h>

// so we get the right marshaler for gtk 1.2
#define gtkmozembed_VOID__INT_UINT \
  gtk_marshal_NONE__INT_INT
#define gtkmozembed_VOID__STRING_INT_INT \
  gtk_marshal_NONE__POINTER_INT_INT
#define gtkmozembed_VOID__STRING_INT_UINT \
  gtk_marshal_NONE__POINTER_INT_INT
#define gtkmozembed_VOID__POINTER_INT_POINTER \
  gtk_marshal_NONE__POINTER_INT_POINTER
#define gtkmozembed_BOOL__STRING \
  gtk_marshal_BOOL__POINTER
#define gtkmozembed_VOID__STRING_STRING \
  gtk_marshal_NONE__STRING_STRING
#define gtkmozembed_VOID__STRING_STRING_STRING_POINTER \
  gtk_marshal_NONE__STRING_STRING_STRING_POINTER
#define gtkmozembed_BOOL__STRING_STRING \
  gtk_marshal_BOOL__STRING_STRING
#define gtkmozembed_BOOL__STRING_STRING_STRING_POINTER \
  gtk_marshal_BOOL__STRING_STRING_STRING_POINTER
#define gtkmozembed_INT__STRING_STRING_UINT_STRING_STRING_STRING_STRING_POINTER \
  gtk_marshal_INT__STRING_STRING_UINT_STRING_STRING_STRING_STRING_POINTER
#define gtkmozembed_BOOL__STRING_STRING_POINTER_STRING_POINTER \
  gtk_marshal_BOOL__STRING_STRING_POINTER_STRING_POINTER

#define gtkmozembed_BOOL__STRING_STRING_POINTER_INT \
  gtk_marshal_BOOL__STRING_STRING_POINTER_INT
#define G_SIGNAL_TYPE_STATIC_SCOPE 0

#define NEW_TOOLKIT_STRING(x) g_strdup(NS_LossyConvertUTF16toASCII(x).get())
#define GET_OBJECT_CLASS_TYPE(x) (GTK_OBJECT_CLASS(x)->type)

#endif /* MOZ_WIDGET_GTK */

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

#ifdef MOZ_WIDGET_GTK
// signal handlers for tracking the focus in and and focus out events
// on the toplevel window.

static void
handle_toplevel_focus_in(GtkMozArea    *aArea,
                         GtkMozEmbed   *aEmbed);

static void
handle_toplevel_focus_out(GtkMozArea    *aArea,
                          GtkMozEmbed   *aEmbed);

#endif /* MOZ_WIDGET_GTK */

// globals for this type of widget

static GtkBinClass *embed_parent_class;

guint moz_embed_signals[EMBED_LAST_SIGNAL] = { 0 };

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
  GtkWidgetClass     *widget_class;
  GtkObjectClass     *object_class;

  container_class = GTK_CONTAINER_CLASS(klass);
  widget_class    = GTK_WIDGET_CLASS(klass);
  object_class    = GTK_OBJECT_CLASS(klass);

  embed_parent_class = (GtkBinClass *)gtk_type_class(gtk_bin_get_type());

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
    gtk_signal_new("link_message",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, link_message),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);
  moz_embed_signals[JS_STATUS] =
    gtk_signal_new("js_status",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, js_status),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);
  moz_embed_signals[LOCATION] =
    gtk_signal_new("location",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, location),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);
  moz_embed_signals[TITLE] =
    gtk_signal_new("title",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, title),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);
  moz_embed_signals[PROGRESS] =
    gtk_signal_new("progress",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, progress),
                   gtk_marshal_NONE__INT_INT,
                   GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);
  moz_embed_signals[PROGRESS_ALL] =
    gtk_signal_new("progress_all",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, progress_all),
                   gtkmozembed_VOID__STRING_INT_INT,
                   GTK_TYPE_NONE, 3,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_INT, GTK_TYPE_INT);
  moz_embed_signals[NET_STATE] =
    gtk_signal_new("net_state",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, net_state),
                   gtkmozembed_VOID__INT_UINT,
                   GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_UINT);
  moz_embed_signals[NET_STATE_ALL] =
    gtk_signal_new("net_state_all",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, net_state_all),
                   gtkmozembed_VOID__STRING_INT_UINT,
                   GTK_TYPE_NONE, 3,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_INT, GTK_TYPE_UINT);
  moz_embed_signals[NET_START] =
    gtk_signal_new("net_start",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, net_start),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);
  moz_embed_signals[NET_STOP] =
    gtk_signal_new("net_stop",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, net_stop),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);
  moz_embed_signals[NEW_WINDOW] =
    gtk_signal_new("new_window",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, new_window),
                   gtk_marshal_NONE__POINTER_UINT,
                   GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_UINT);
  moz_embed_signals[VISIBILITY] =
    gtk_signal_new("visibility",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, visibility),
                   gtk_marshal_NONE__BOOL,
                   GTK_TYPE_NONE, 1, GTK_TYPE_BOOL);
  moz_embed_signals[DESTROY_BROWSER] =
    gtk_signal_new("destroy_browser",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, destroy_brsr),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);
  moz_embed_signals[OPEN_URI] =
    gtk_signal_new("open_uri",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, open_uri),
                   gtkmozembed_BOOL__STRING,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_STRING |
                   G_SIGNAL_TYPE_STATIC_SCOPE);
  moz_embed_signals[SIZE_TO] =
    gtk_signal_new("size_to",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, size_to),
                   gtk_marshal_NONE__INT_INT,
                   GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);
  moz_embed_signals[DOM_KEY_DOWN] =
    gtk_signal_new("dom_key_down",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_key_down),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_KEY_PRESS] =
    gtk_signal_new("dom_key_press",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_key_press),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_KEY_UP] =
    gtk_signal_new("dom_key_up",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_key_up),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_DOWN] =
    gtk_signal_new("dom_mouse_down",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_down),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_UP] =
    gtk_signal_new("dom_mouse_up",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_up),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_CLICK] =
    gtk_signal_new("dom_mouse_click",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_click),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_DBL_CLICK] =
    gtk_signal_new("dom_mouse_dbl_click",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_dbl_click),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_OVER] =
    gtk_signal_new("dom_mouse_over",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_over),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_OUT] =
    gtk_signal_new("dom_mouse_out",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_out),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
/*  moz_embed_signals[DOM_MOUSE_MOVE] =
    gtk_signal_new("dom_mouse_move",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_move),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);*/
  moz_embed_signals[DOM_MOUSE_SCROLL] =
    gtk_signal_new("dom_mouse_scroll",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_scroll),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_MOUSE_LONG_PRESS] =
    gtk_signal_new("dom_mouse_long_press",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_mouse_long_press),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_FOCUS] =
    gtk_signal_new("dom_focus",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_focus),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_BLUR] =
    gtk_signal_new("dom_blur",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_blur),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[SECURITY_CHANGE] =
    gtk_signal_new("security_change",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, security_change),
                   gtk_marshal_NONE__POINTER_UINT,
                   GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_UINT);
  moz_embed_signals[STATUS_CHANGE] =
    gtk_signal_new("status_change",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, status_change),
                   gtkmozembed_VOID__POINTER_INT_POINTER,
                   GTK_TYPE_NONE, 3,
                   GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_ACTIVATE] =
    gtk_signal_new("dom_activate",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_activate),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_FOCUS_IN] =
    gtk_signal_new("dom_focus_in",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_focus_in),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
  moz_embed_signals[DOM_FOCUS_OUT] =
    gtk_signal_new("dom_focus_out",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, dom_focus_out),
                   gtk_marshal_BOOL__POINTER,
                   GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
#ifdef MOZ_WIDGET_GTK2
  moz_embed_signals[ALERT] =
    gtk_signal_new("alert",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, alert),
                   gtkmozembed_VOID__STRING_STRING,
                   GTK_TYPE_NONE, 2,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
  moz_embed_signals[ALERT_CHECK] =
    gtk_signal_new("alert_check",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, alert_check),
                   gtkmozembed_VOID__STRING_STRING_STRING_POINTER,
                   GTK_TYPE_NONE, 4,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_POINTER);
  moz_embed_signals[CONFIRM] =
    gtk_signal_new("confirm",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, confirm),
                   gtkmozembed_BOOL__STRING_STRING,
                   GTK_TYPE_BOOL, 2,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING);
  moz_embed_signals[CONFIRM_CHECK] =
    gtk_signal_new("confirm_check",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, confirm_check),
                   gtkmozembed_BOOL__STRING_STRING_STRING_POINTER,
                   GTK_TYPE_BOOL, 4,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_POINTER);
  moz_embed_signals[CONFIRM_EX] =
    gtk_signal_new("confirm_ex",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, confirm_ex),
                   gtkmozembed_INT__STRING_STRING_UINT_STRING_STRING_STRING_STRING_POINTER,
                   GTK_TYPE_INT, 8,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_UINT,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_POINTER);
  moz_embed_signals[PROMPT] =
    gtk_signal_new("prompt",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, prompt),
                   gtkmozembed_BOOL__STRING_STRING_POINTER_STRING_POINTER,
                   GTK_TYPE_BOOL, 5,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_POINTER,
                   GTK_TYPE_STRING,
                   GTK_TYPE_POINTER);
  moz_embed_signals[PROMPT_AUTH] =
    gtk_signal_new("prompt_auth",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, prompt_auth),
                   gtkmozembed_BOOL__STRING_STRING_POINTER_POINTER_STRING_POINTER,
                   GTK_TYPE_BOOL, 6,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_POINTER,
                   GTK_TYPE_POINTER,
                   GTK_TYPE_STRING,
                   GTK_TYPE_POINTER);
  moz_embed_signals[SELECT] =
    gtk_signal_new("select",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, select),
                   gtkmozembed_BOOL__STRING_STRING_POINTER_INT,
                   GTK_TYPE_BOOL, 4,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_POINTER,
                   GTK_TYPE_INT);
  moz_embed_signals[DOWNLOAD_REQUEST] =
    gtk_signal_new("download_request",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, download_request),
                   gtkmozembed_VOID__STRING_STRING_STRING_ULONG_INT,
                   GTK_TYPE_NONE,
                   5,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_INT,
                   GTK_TYPE_INT
                   );
  moz_embed_signals[UPLOAD_DIALOG] =
    gtk_signal_new("upload_dialog",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, upload_dialog),
                   gtkmozembed_BOOL__STRING_STRING_POINTER,
                   GTK_TYPE_BOOL,
                   3,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING,
                   GTK_TYPE_POINTER);
  moz_embed_signals[ICON_CHANGED] =
    gtk_signal_new("icon_changed",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, icon_changed),
                   gtkmozembed_VOID__POINTER,
                   GTK_TYPE_NONE,
                   1,
                   GTK_TYPE_POINTER);
  moz_embed_signals[MAILTO] =
    gtk_signal_new("mailto",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, mailto),
                   gtk_marshal_VOID__STRING,
                   GTK_TYPE_NONE,
                   1,
                   GTK_TYPE_STRING);

  moz_embed_signals[NETWORK_ERROR] =
    gtk_signal_new("network_error",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, network_error),
                   gtkmozembed_VOID__INT_STRING_STRING,
                   GTK_TYPE_NONE,
                   3,
                   GTK_TYPE_INT,
                   GTK_TYPE_STRING,
                   GTK_TYPE_STRING);

  moz_embed_signals[RSS_REQUEST] =
    gtk_signal_new("rss_request",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedClass, rss_request),
                   gtkmozembed_VOID__STRING_STRING,
                   GTK_TYPE_NONE,
                   2,
                   GTK_TYPE_STRING, GTK_TYPE_STRING);

#endif

#ifdef MOZ_WIDGET_GTK
  gtk_object_class_add_signals(object_class, moz_embed_signals,
                               EMBED_LAST_SIGNAL);
#endif /* MOZ_WIDGET_GTK */

}

static void
gtk_moz_embed_init(GtkMozEmbed *embed)
{
  EmbedPrivate *priv = new EmbedPrivate();
  embed->data = priv;
  embed->common = NULL;
  gtk_widget_set_name(GTK_WIDGET(embed), "gtkmozembed");

#ifdef MOZ_WIDGET_GTK2
  GTK_WIDGET_UNSET_FLAGS(GTK_WIDGET(embed), GTK_NO_WINDOW);
#endif
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

    // Destroy the widget only if it's been Init()ed.
    if (embedPrivate->mMozWindowWidget != 0) {
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
  attributes.visual = gtk_widget_get_visual(widget);
  attributes.colormap = gtk_widget_get_colormap(widget);
  attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
           &attributes, attributes_mask);
  gdk_window_set_user_data(widget->window, embed);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

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

  if (!embedPrivate->mURI.IsEmpty())
    embedPrivate->LoadCurrentURI();

  // connect to the focus out event for the child
  GtkWidget *child_widget = GTK_BIN(widget)->child;
  gtk_signal_connect_while_alive(GTK_OBJECT(child_widget),
                                 "focus_out_event",
                                 GTK_SIGNAL_FUNC(handle_child_focus_out),
                                 embed,
                                 GTK_OBJECT(child_widget));
  gtk_signal_connect_while_alive(GTK_OBJECT(child_widget),
                                 "focus_in_event",
                                 GTK_SIGNAL_FUNC(handle_child_focus_in),
                                 embed,
                                 GTK_OBJECT(child_widget));

#ifdef MOZ_WIDGET_GTK
  // connect to the toplevel focus out events for the child
  GtkMozArea *mozarea = GTK_MOZAREA(child_widget);
  gtk_signal_connect_while_alive(GTK_OBJECT(mozarea),
                                 "toplevel_focus_in",
                                 GTK_SIGNAL_FUNC(handle_toplevel_focus_in),
                                 embed,
                                 GTK_OBJECT(mozarea));

  gtk_signal_connect_while_alive(GTK_OBJECT(mozarea),
                                 "toplevel_focus_out",
                                 GTK_SIGNAL_FUNC(handle_toplevel_focus_out),
                                 embed,
                                 GTK_OBJECT(mozarea));
#endif /* MOZ_WIDGET_GTK */
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
  return NS_STATIC_CAST(AtkObject *,
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

  return PR_FALSE;
}

static gint
handle_child_focus_out(GtkWidget     *aWidget,
                       GdkEventFocus *aGdkFocusEvent,
                       GtkMozEmbed   *aEmbed)
{
  EmbedPrivate *embedPrivate;

  embedPrivate = (EmbedPrivate *)aEmbed->data;

  embedPrivate->ChildFocusOut();

  return PR_FALSE;
}

#ifdef MOZ_WIDGET_GTK
void
handle_toplevel_focus_in(GtkMozArea    *aArea,
                         GtkMozEmbed   *aEmbed)
{
  EmbedPrivate   *embedPrivate;
  embedPrivate = (EmbedPrivate *)aEmbed->data;

  embedPrivate->TopLevelFocusIn();
}

void
handle_toplevel_focus_out(GtkMozArea    *aArea,
                          GtkMozEmbed   *aEmbed)
{
  EmbedPrivate   *embedPrivate;
  embedPrivate = (EmbedPrivate *)aEmbed->data;

  embedPrivate->TopLevelFocusOut();
}
#endif /* MOZ_WIDGET_GTK */

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
gtk_moz_embed_set_path(const gchar *aPath)
{
  EmbedPrivate::SetPath(aPath);
}

void
gtk_moz_embed_set_comp_path(const gchar *aPath)
{
  EmbedPrivate::SetCompPath(aPath);
}

void
gtk_moz_embed_set_app_components(const nsModuleComponentInfo* aComps,
                                 gint aNumComponents)
{
  EmbedPrivate::SetAppComponents(aComps, aNumComponents);
}

void
gtk_moz_embed_set_profile_path(const gchar *aDir, const gchar *aName)
{
  EmbedPrivate::SetProfilePath(aDir, aName);
}

void
gtk_moz_embed_set_directory_service_provider(nsIDirectoryServiceProvider *appFileLocProvider)
{
  EmbedPrivate::SetDirectoryServiceProvider(appFileLocProvider);
}

void
gtk_moz_embed_load_url(GtkMozEmbed *embed, const gchar *url)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail(embed != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  embedPrivate->SetURI(url);
  embedPrivate->mOpenBlock = PR_FALSE;

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
  return retval ? TRUE : FALSE;
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
  return retval ? TRUE : FALSE;
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
gtk_moz_embed_render_data(GtkMozEmbed *embed, const gchar *data,
                          guint32 len, const gchar *base_uri,
                          const gchar *mime_type)
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
gtk_moz_embed_open_stream(GtkMozEmbed *embed, const gchar *base_uri,
                          const gchar *mime_type)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));
  g_return_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)));

  embedPrivate = (EmbedPrivate *)embed->data;

  embedPrivate->OpenStream(base_uri, mime_type);
}

void gtk_moz_embed_append_data(GtkMozEmbed *embed,
                               const gchar *data,
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

gchar *
gtk_moz_embed_get_link_message(GtkMozEmbed *embed)
{
  gchar *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), (gchar *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (gchar *)NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mWindow)
    retval = NEW_TOOLKIT_STRING(embedPrivate->mWindow->mLinkMessage);

  return retval;
}

gchar *
gtk_moz_embed_get_js_status(GtkMozEmbed *embed)
{
  gchar *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), (gchar *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (gchar *)NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mWindow)
    retval = NEW_TOOLKIT_STRING(embedPrivate->mWindow->mJSStatus);

  return retval;
}

gchar *
gtk_moz_embed_get_title(GtkMozEmbed *embed)
{
  gchar *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), (gchar *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (gchar *)NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mWindow)
    retval = NEW_TOOLKIT_STRING(embedPrivate->mWindow->mTitle);

  return retval;
}

gchar *
gtk_moz_embed_get_location(GtkMozEmbed *embed)
{
  gchar *retval = nsnull;
  EmbedPrivate *embedPrivate;

  g_return_val_if_fail ((embed != NULL), (gchar *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (gchar *)NULL);

  embedPrivate = (EmbedPrivate *)embed->data;

  if (!embedPrivate->mURI.IsEmpty())
    retval = NEW_TOOLKIT_STRING(embedPrivate->mURI);

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
gtk_moz_embed_get_nsIWebBrowser(GtkMozEmbed *embed, nsIWebBrowser **retval)
{
  EmbedPrivate *embedPrivate;

  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  embedPrivate = (EmbedPrivate *)embed->data;

  if (embedPrivate->mWindow)
    embedPrivate->mWindow->GetWebBrowser(retval);
}

PRUnichar *
gtk_moz_embed_get_title_unichar(GtkMozEmbed *embed)
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
gtk_moz_embed_get_js_status_unichar(GtkMozEmbed *embed)
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
gtk_moz_embed_get_link_message_unichar(GtkMozEmbed *embed)
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

GtkType
gtk_moz_embed_single_get_type(void)
{
  static GtkType moz_embed_single_type = 0;
  if (!moz_embed_single_type)
  {
    static const GtkTypeInfo moz_embed_single_info =
    {
      "GtkMozEmbedSingle",
      sizeof(GtkMozEmbedSingle),
      sizeof(GtkMozEmbedSingleClass),
      (GtkClassInitFunc)gtk_moz_embed_single_class_init,
      (GtkObjectInitFunc)gtk_moz_embed_single_init,
      0,
      0,
      0
    };
    moz_embed_single_type = gtk_type_unique(GTK_TYPE_OBJECT,
                                            &moz_embed_single_info);
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
    gtk_signal_new("new_window_orphan",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedSingleClass,
                                     new_window_orphan),
                   gtk_marshal_NONE__POINTER_UINT,
                   GTK_TYPE_NONE,
                   2,
                   GTK_TYPE_POINTER, GTK_TYPE_UINT);

#ifdef MOZ_WIDGET_GTK
  gtk_object_class_add_signals(object_class, moz_embed_single_signals,
                               SINGLE_LAST_SIGNAL);
#endif /* MOZ_WIDGET_GTK */
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
  return (GtkMozEmbedSingle *)gtk_type_new(gtk_moz_embed_single_get_type());
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

  gtk_signal_emit(GTK_OBJECT(single),
                  moz_embed_single_signals[NEW_WINDOW_ORPHAN],
                  aNewEmbed, aChromeFlags);
}

gboolean
gtk_moz_embed_set_zoom_level(GtkMozEmbed *embed, gint zoom_level, gpointer context)
{
  g_return_val_if_fail (embed != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), FALSE);
  g_return_val_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)), FALSE);
  EmbedPrivate *embedPrivate;
  embedPrivate = (EmbedPrivate *) embed->data;
  nsresult rv = NS_OK;
  if (embedPrivate)
    rv = embedPrivate->SetZoom(zoom_level, (nsISupports*)context);
  return NS_SUCCEEDED(rv);
}

gboolean
gtk_moz_embed_get_zoom_level(GtkMozEmbed *embed, gint *zoom_level, gpointer context)
{
  g_return_val_if_fail (embed != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), FALSE);
  g_return_val_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)), FALSE);
  EmbedPrivate *embedPrivate;
  embedPrivate = (EmbedPrivate *) embed->data;
  nsresult rv = NS_OK;
  if (embedPrivate)
    rv = embedPrivate->GetZoom(zoom_level, (nsISupports*)context);
  return NS_SUCCEEDED(rv);
}

gboolean
gtk_moz_embed_find_text(GtkMozEmbed *embed, const gchar *string,
                        gboolean reverse, gboolean whole_word,
                        gboolean case_sensitive, gboolean restart, gint target)
{
  EmbedPrivate *embedPrivate;
  g_return_val_if_fail (embed != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), FALSE);
  g_return_val_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)), FALSE);
  embedPrivate = (EmbedPrivate *)embed->data;
  if (embedPrivate->mWindow)
    return embedPrivate->FindText(string, reverse, whole_word, case_sensitive, restart);
  return FALSE;
}

gboolean
gtk_moz_embed_clipboard(GtkMozEmbed *embed, guint action, gint target)
{
  EmbedPrivate *embedPrivate;
  g_return_val_if_fail (embed != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), FALSE);
  g_return_val_if_fail (GTK_WIDGET_REALIZED(GTK_WIDGET(embed)), FALSE);
  embedPrivate = (EmbedPrivate *)embed->data;
  return embedPrivate->ClipBoardAction((GtkMozEmbedClipboard)action) ? TRUE : FALSE;
}

void
gtk_moz_embed_notify_plugins(GtkMozEmbed *embed, guint)
{
  return;
}

gchar *
gtk_moz_embed_get_encoding(GtkMozEmbed *embed, gint frame_number)
{
  gchar *retval = nsnull;
  EmbedPrivate *embedPrivate;
  g_return_val_if_fail ((embed != NULL), (gchar *)NULL);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), (gchar *)NULL);
  embedPrivate = (EmbedPrivate *)embed->data;
  if (embedPrivate->mWindow)
    retval = embedPrivate->GetEncoding();
  return retval;
}

void
gtk_moz_embed_set_encoding(GtkMozEmbed *embed, const gchar *encoding_text, gint frame_number)
{
  EmbedPrivate *embedPrivate;
  g_return_if_fail(embed != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED(embed));
  embedPrivate = (EmbedPrivate *)embed->data;
  if (embedPrivate->mWindow)
    embedPrivate->SetEncoding(encoding_text);
  return;
}

guint
gtk_moz_embed_get_context_info(GtkMozEmbed *embed, gpointer event, gpointer *node,
                               gint *x, gint *y, gint *docindex,
                               const gchar **url, const gchar **objurl, const gchar **docurl)
{
#ifdef MOZ_WIDGET_GTK2
  EmbedPrivate *embedPrivate;
  g_return_val_if_fail(embed != NULL, GTK_MOZ_EMBED_CTX_NONE);
  g_return_val_if_fail(GTK_IS_MOZ_EMBED(embed), GTK_MOZ_EMBED_CTX_NONE);
  embedPrivate = (EmbedPrivate *)embed->data;

  if (!event) {
    nsIWebBrowser *webBrowser = nsnull;
    gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(embed), &webBrowser);
    if (!webBrowser) return GTK_MOZ_EMBED_CTX_NONE;

    nsCOMPtr<nsIDOMWindow> DOMWindow;
    webBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));
    if (!DOMWindow) return GTK_MOZ_EMBED_CTX_NONE;

    nsCOMPtr<nsIDOMDocument> doc;
    DOMWindow->GetDocument(getter_AddRefs(doc));
    if (!doc) return GTK_MOZ_EMBED_CTX_NONE;

    nsCOMPtr<nsIDOMNode> docnode = do_QueryInterface(doc);
    *node = docnode;
    return GTK_MOZ_EMBED_CTX_DOCUMENT;
  }

  if (embedPrivate->mEventListener) {
    EmbedContextMenuInfo * ctx_menu = embedPrivate->mEventListener->GetContextInfo();
    if (!ctx_menu)
      return 0;
    ctx_menu->UpdateContextData(event);
    *x = ctx_menu->mX;
    *y = ctx_menu->mY;
    *docindex = ctx_menu->mCtxFrameNum;
    if (ctx_menu->mEmbedCtxType & GTK_MOZ_EMBED_CTX_LINK && !*url) {
      *url = ToNewUTF8String(ctx_menu->mCtxHref);
    }
    if (ctx_menu->mEmbedCtxType & GTK_MOZ_EMBED_CTX_IMAGE) {
      *objurl = ToNewUTF8String(ctx_menu->mCtxImgHref);
    }
    *docurl = ToNewUTF8String(ctx_menu->mCtxURI);
    *node = ctx_menu->mEventNode;
    return ctx_menu->mEmbedCtxType;
  }
#endif
  return 0;
}

const gchar*
gtk_moz_embed_get_selection(GtkMozEmbed *embed)
{
  EmbedPrivate *embedPrivate;
  g_return_val_if_fail(embed != NULL, NULL);
  g_return_val_if_fail(GTK_IS_MOZ_EMBED(embed), NULL);
  embedPrivate = (EmbedPrivate *)embed->data;
#ifdef MOZ_WIDGET_GTK2
  if (embedPrivate->mEventListener) {
    EmbedContextMenuInfo * ctx_menu = embedPrivate->mEventListener->GetContextInfo();
    if (!ctx_menu)
      return NULL;
    return ctx_menu->GetSelectedText();
  }
#endif
  return NULL;
}
gboolean
gtk_moz_embed_insert_text(GtkMozEmbed *embed, const gchar *string, gpointer node)
{
  EmbedPrivate *embedPrivate;
  g_return_val_if_fail(embed != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_MOZ_EMBED(embed), FALSE);
  embedPrivate = (EmbedPrivate *)embed->data;
  if (!embedPrivate || !embedPrivate->mEventListener)
    return FALSE;
  if (!string && node) {
    embedPrivate->ScrollToSelectedNode((nsIDOMNode*)node);
    return TRUE;
  }
  if (string) {
    embedPrivate->InsertTextToNode((nsIDOMNode*)node, string);
    return TRUE;
  }
  return FALSE;
}


gboolean
gtk_moz_embed_save_target(GtkMozEmbed *aEmbed, gchar* aUrl,
                          gchar* aDestination, gint aSetting)
{
  //FIXME
  nsresult rv;

  g_return_val_if_fail (aEmbed != NULL, FALSE);
  nsIWebBrowser *webBrowser = nsnull;
  gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(aEmbed), &webBrowser);
  g_return_val_if_fail (webBrowser != NULL, FALSE);

  nsCOMPtr<nsIDOMWindow> DOMWindow;
  webBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));
  g_return_val_if_fail (DOMWindow != NULL, FALSE);

  nsCOMPtr<nsIDOMDocument> doc;
  DOMWindow->GetDocument(getter_AddRefs(doc));
  g_return_val_if_fail (doc != NULL, FALSE);

  nsCOMPtr<nsIWebBrowserPersist> persist =
    do_CreateInstance(NS_WEBBROWSERPERSIST_CONTRACTID);
  if (!persist)
    return FALSE;

  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (!ios)
    return FALSE;

  nsCOMPtr<nsIURI> uri;
  rv = ios->NewURI(nsDependentCString(aDestination), "", nsnull, getter_AddRefs(uri));
  if (!uri)
    return FALSE;

  if (aSetting == 0)
  {
    nsCOMPtr<nsIURI> uri_s;
    rv = ios->NewURI(nsDependentCString(aUrl), "", nsnull, getter_AddRefs(uri_s));
    rv = ios->NewURI(nsDependentCString(aDestination), "", nsnull, getter_AddRefs(uri));

    if (!uri_s)
      return FALSE;
    rv = persist->SaveURI(uri_s, nsnull, nsnull, nsnull, "", uri);  

    if (NS_SUCCEEDED(rv))
      return TRUE;

  } else if (aSetting == 1)
  {
    nsCOMPtr<nsIURI> contentFolder;
    rv = ios->NewURI(nsDependentCString(aDestination), "", nsnull, getter_AddRefs(uri));
    rv = ios->NewURI(nsDependentCString(aDestination), "", nsnull, getter_AddRefs(contentFolder));

    nsCString contentFolderPath;
    contentFolder->GetSpec(contentFolderPath);
    contentFolderPath.Append("_content");
    printf("GetNativePath=%s ", contentFolderPath.get());
    rv = ios->NewURI(contentFolderPath, "", nsnull, getter_AddRefs(contentFolder));

    if (NS_FAILED(rv))
      return FALSE;
    
    rv = persist->SaveDocument(doc, uri, contentFolder, nsnull, 0, 0);
    if (NS_SUCCEEDED(rv))
      return TRUE;
  } else if (aSetting == 2)
  {
    // FIXME: How should I handle this option G_WEBENGINE_SAVE_FRAMES ?
    return FALSE;
  }
  return FALSE;
}

gboolean
gtk_moz_embed_get_doc_info(GtkMozEmbed *embed, gpointer node, gint docindex,
                           const gchar**title, const gchar**location,
                           const gchar **file_type, guint *file_size, gint *width, gint *height)
{
  g_return_val_if_fail(embed != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_MOZ_EMBED(embed), FALSE);
  EmbedPrivate *embedPrivate;
  embedPrivate = (EmbedPrivate *)embed->data;

  if (!embedPrivate || !embedPrivate->mEventListener)
    return FALSE;

#ifdef MOZ_WIDGET_GTK2

  if (file_type) {
    embedPrivate->GetMIMEInfo(file_type, (nsIDOMNode*)node);
  }

  if (width && height) {
    nsString imgSrc;
    EmbedContextMenuInfo * ctx_menu = embedPrivate->mEventListener->GetContextInfo();
    if (ctx_menu)
      ctx_menu->CheckDomImageElement((nsIDOMNode*)node, imgSrc, width, height);
  }

  if (title) {
    EmbedContextMenuInfo * ctx_menu = embedPrivate->mEventListener->GetContextInfo();
    if (ctx_menu)
      *title = NEW_TOOLKIT_STRING(ctx_menu->GetCtxDocTitle());
  }

  if (file_size && location && *location != nsnull) {
    nsCOMPtr<nsICacheEntryDescriptor> descriptor;
    nsresult rv;
    rv = embedPrivate->GetCacheEntry("HTTP", *location, nsICache::ACCESS_READ, PR_FALSE, getter_AddRefs(descriptor));
    if (descriptor) {
      rv = descriptor->GetDataSize(file_size);
    }
  }
#endif

  return TRUE;
}

gint
gtk_moz_embed_get_shistory_list(GtkMozEmbed *embed, GtkMozHistoryItem **GtkHI,
                                guint type)
{
  g_return_val_if_fail ((embed != NULL), 0);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), 0);
  EmbedPrivate *embedPrivate;
  gint count = 0;

  embedPrivate = (EmbedPrivate *)embed->data;
  if (embedPrivate)
    embedPrivate->GetSHistoryList(GtkHI, (GtkMozEmbedSessionHistory)type, &count);
  return count;
}

gint
gtk_moz_embed_get_shistory_index(GtkMozEmbed *embed)
{
  g_return_val_if_fail ((embed != NULL), -1);
  g_return_val_if_fail (GTK_IS_MOZ_EMBED(embed), -1);

  PRInt32 curIndex;
  EmbedPrivate *embedPrivate;
  
  embedPrivate = (EmbedPrivate *)embed->data;
  if (embedPrivate->mSessionHistory)
    embedPrivate->mSessionHistory->GetIndex(&curIndex);

  return (gint)curIndex;
}

void
gtk_moz_embed_shistory_goto_index(GtkMozEmbed *embed, gint index)
{
  g_return_if_fail (embed != NULL);
  g_return_if_fail (GTK_IS_MOZ_EMBED(embed));

  EmbedPrivate *embedPrivate;

  embedPrivate = (EmbedPrivate *)embed->data;
  if (embedPrivate->mNavigation)
    embedPrivate->mNavigation->GotoIndex(index);
}

gboolean
gtk_moz_embed_get_server_cert(GtkMozEmbed *embed, gpointer *aCert, gpointer context)
{
  g_return_val_if_fail(embed != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_MOZ_EMBED(embed), FALSE);

  nsIWebBrowser *webBrowser = nsnull;
  gtk_moz_embed_get_nsIWebBrowser(GTK_MOZ_EMBED(embed), &webBrowser);
  if (!webBrowser) return FALSE;

  nsCOMPtr<nsIDocShell> docShell(do_GetInterface((nsISupports*)webBrowser));
  if (!docShell) return FALSE;

  nsCOMPtr<nsISecureBrowserUI> mSecureUI;
  docShell->GetSecurityUI(getter_AddRefs(mSecureUI));
  if (!mSecureUI) return FALSE;

  nsCOMPtr<nsISSLStatusProvider> mSecureProvider = do_QueryInterface(mSecureUI);
  if (!mSecureProvider) return FALSE;

  nsCOMPtr<nsISSLStatus> SSLStatus;
  mSecureProvider->GetSSLStatus(getter_AddRefs(SSLStatus));
  if (!SSLStatus) return FALSE;

  nsCOMPtr<nsIX509Cert> serverCert;
  SSLStatus->GetServerCert(getter_AddRefs(serverCert));
  if (!serverCert) return FALSE;

  *aCert = serverCert;
  return TRUE;
}
