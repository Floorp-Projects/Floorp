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
#include "gtkmozembed_download.h"
#include "gtkmozembedprivate.h"
#include "gtkmozembed_internal.h"
#include "EmbedPrivate.h"
#include "EmbedWindow.h"
#include "EmbedDownloadMgr.h"
// so we can do our get_nsIWebBrowser later...
#include "nsIWebBrowser.h"

// for strings
#ifdef MOZILLA_INTERNAL_API
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#else
#include "nsStringAPI.h"
#endif

#ifdef MOZ_WIDGET_GTK2
#include "gtkmozembedmarshal.h"
#define NEW_TOOLKIT_STRING(x) g_strdup(NS_ConvertUTF16toUTF8(x).get())
#define GET_TOOLKIT_STRING(x) NS_ConvertUTF16toUTF8(x).get()
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
#define gtkmozembed_VOID__INT_INT_BOOLEAN \
  gtk_marshal_NONE__INT_INT_BOOLEAN
#define G_SIGNAL_TYPE_STATIC_SCOPE 0
#define NEW_TOOLKIT_STRING(x) g_strdup(NS_LossyConvertUTF16toASCII(x).get())
#define GET_TOOLKIT_STRING(x) NS_LossyConvertUTF16toASCII(x).get()
#define GET_OBJECT_CLASS_TYPE(x) (GTK_OBJECT_CLASS(x)->type)
#endif /* MOZ_WIDGET_GTK */

static void gtk_moz_embed_download_set_latest_object(GtkObject *o);
static GtkObject *latest_download_object = nsnull;

// class and instance initialization
guint moz_embed_download_signals[DOWNLOAD_LAST_SIGNAL] = { 0 };
static void gtk_moz_embed_download_class_init(GtkMozEmbedDownloadClass *klass);
static void gtk_moz_embed_download_init(GtkMozEmbedDownload *embed);
static void gtk_moz_embed_download_destroy(GtkObject *object);
GtkObject * gtk_moz_embed_download_new(void);

// GtkObject + class-related functions
GtkType
gtk_moz_embed_download_get_type(void)
{
  static GtkType moz_embed_download_type = 0;
  if (!moz_embed_download_type)
  {
    static const GtkTypeInfo moz_embed_download_info =
    {
      "GtkMozEmbedDownload",
      sizeof(GtkMozEmbedDownload),
      sizeof(GtkMozEmbedDownloadClass),
      (GtkClassInitFunc)gtk_moz_embed_download_class_init,
      (GtkObjectInitFunc)gtk_moz_embed_download_init,
      0,
      0,
      0
    };
    moz_embed_download_type = gtk_type_unique(GTK_TYPE_OBJECT, &moz_embed_download_info);
  }
  return moz_embed_download_type;
}

static void
gtk_moz_embed_download_class_init(GtkMozEmbedDownloadClass *klass)
{
  GtkObjectClass     *object_class;
  object_class    = GTK_OBJECT_CLASS(klass);
  object_class->destroy = gtk_moz_embed_download_destroy;

  // set up our signals
  moz_embed_download_signals[DOWNLOAD_STARTED_SIGNAL] =
    gtk_signal_new("started",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedDownloadClass, started),
                   gtk_marshal_NONE__POINTER,
                   GTK_TYPE_NONE,
                   1,
                   G_TYPE_POINTER);

  moz_embed_download_signals[DOWNLOAD_COMPLETED_SIGNAL] =
    gtk_signal_new("completed",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedDownloadClass, completed),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);

  moz_embed_download_signals[DOWNLOAD_FAILED_SIGNAL] =
    gtk_signal_new("error",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedDownloadClass, error),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);

  moz_embed_download_signals[DOWNLOAD_DESTROYED_SIGNAL] =
    gtk_signal_new("aborted",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedDownloadClass, aborted),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);

  moz_embed_download_signals[DOWNLOAD_PROGRESS_SIGNAL] =
    gtk_signal_new("progress",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedDownloadClass, progress),
                   gtkmozembed_VOID__ULONG_ULONG_ULONG,
                   GTK_TYPE_NONE,
                   3,
                   G_TYPE_ULONG,
                   G_TYPE_ULONG,
                   G_TYPE_ULONG);

#ifdef MOZ_WIDGET_GTK
  gtk_object_class_add_signals(object_class, moz_embed_download_signals,
                               DOWNLOAD_LAST_SIGNAL);
#endif /* MOZ_WIDGET_GTK */
}

static void
gtk_moz_embed_download_init(GtkMozEmbedDownload *download)
{
  // this is a placeholder for later in case we need to stash data at
  // a later data and maintain backwards compatibility.
  download->data = nsnull;
  EmbedDownload *priv = new EmbedDownload();
  download->data = priv;
}

static void
gtk_moz_embed_download_destroy(GtkObject *object)
{
  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED_DOWNLOAD(object));

  GtkMozEmbedDownload  *embed;
  EmbedDownload *downloadPrivate;

  embed = GTK_MOZ_EMBED_DOWNLOAD(object);
  downloadPrivate = (EmbedDownload *)embed->data;

  if (downloadPrivate) {
    delete downloadPrivate;
    embed->data = NULL;
  }
}

GtkObject *
gtk_moz_embed_download_new(void)
{
  GtkObject *instance = (GtkObject *) gtk_type_new(gtk_moz_embed_download_get_type());
  gtk_moz_embed_download_set_latest_object(instance);

  return instance;
}

GtkObject *
gtk_moz_embed_download_get_latest_object(void)
{
  return latest_download_object;
}

static void
gtk_moz_embed_download_set_latest_object(GtkObject *obj)
{
  latest_download_object = obj;
  return ;
}

void
gtk_moz_embed_download_do_command(GtkMozEmbedDownload *item, guint command)
{
  EmbedDownload *download_priv = (EmbedDownload *) item->data;

  if (!download_priv)
    return;

  if (command == GTK_MOZ_EMBED_DOWNLOAD_CANCEL) {
    download_priv->launcher->Cancel(GTK_MOZ_EMBED_STATUS_FAILED_USERCANCELED);
    download_priv->launcher->SetWebProgressListener(nsnull);

    return;
  }

  if (command == GTK_MOZ_EMBED_DOWNLOAD_RESUME) {
    download_priv->request->Resume();
    download_priv->is_paused = FALSE;

    return;
  }

  if (command == GTK_MOZ_EMBED_DOWNLOAD_PAUSE) {
    if (download_priv->request) {
      download_priv->request->Suspend();
      download_priv->is_paused = TRUE;
    }

    return;
  }

  if (command == GTK_MOZ_EMBED_DOWNLOAD_RELOAD) {
    if (download_priv->gtkMozEmbedParentWidget) {}
  }
  // FIXME: missing GTK_MOZ_EMBED_DOWNLOAD_STORE and GTK_MOZ_EMBED_DOWNLOAD_RESTORE implementation.
}

gchar*
gtk_moz_embed_download_get_file_name(GtkMozEmbedDownload *item)
{
  EmbedDownload *download_priv = (EmbedDownload *) item->data;

  if (!download_priv)
    return nsnull;

  return (gchar *) download_priv->file_name;
}

gchar*
gtk_moz_embed_download_get_url(GtkMozEmbedDownload *item)
{
  EmbedDownload *download_priv = (EmbedDownload *) item->data;

  if (!download_priv)
    return nsnull;

  // FIXME : 'server' is storing the wrong value. See EmbedDownloadMgr.cpp l. 189.
  return (gchar *) download_priv->server;
}

glong
gtk_moz_embed_download_get_progress(GtkMozEmbedDownload *item)
{
  EmbedDownload *download_priv = (EmbedDownload *) item->data;

  if (!download_priv)
    return -1;

  return (glong) download_priv->downloaded_size;
}

glong
gtk_moz_embed_download_get_file_size(GtkMozEmbedDownload *item)
{
  EmbedDownload *download_priv = (EmbedDownload *) item->data;

  if (!download_priv)
    return -1;

  return (glong) download_priv->file_size;
}

