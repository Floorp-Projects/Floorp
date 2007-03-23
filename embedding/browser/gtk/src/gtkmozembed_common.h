/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
#ifndef gtkmozembed_common_h
#define gtkmozembed_common_h
#include "gtkmozembed.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include <stddef.h>
#include <gtk/gtk.h>
#ifdef MOZILLA_CLIENT
#include "nscore.h"
#else // MOZILLA_CLIENT
#ifndef nscore_h__
/* Because this header may be included from files which not part of the mozilla
   build system, define macros from nscore.h */
#if (__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
#define NS_HIDDEN __attribute__((visibility("hidden")))
#else
#define NS_HIDDEN
#endif
#define NS_FROZENCALL
#define NS_EXPORT_(type) type
#define NS_IMPORT_(type) type
#endif // nscore_h__
#endif // MOZILLA_CLIENT
#ifdef XPCOM_GLUE
#define GTKMOZEMBED_API(type, name, params) \
  typedef type (NS_FROZENCALL * name##Type) params; \
  extern name##Type name NS_HIDDEN;
#else // XPCOM_GLUE
#ifdef _IMPL_GTKMOZEMBED
#define GTKMOZEMBED_API(type, name, params) NS_EXPORT_(type) name params;
#else
#define GTKMOZEMBED_API(type,name, params) NS_IMPORT_(type) name params;
#endif
#endif // XPCOM_GLUE
#define GTK_TYPE_MOZ_EMBED_COMMON             (gtk_moz_embed_common_get_type())
#define GTK_MOZ_EMBED_COMMON(obj)             GTK_CHECK_CAST((obj), GTK_TYPE_MOZ_EMBED_COMMON, GtkMozEmbedCommon)
#define GTK_MOZ_EMBED_COMMON_CLASS(klass)     GTK_CHECK_CLASS_CAST((klass), GTK_TYPE_MOZ_EMBED_COMMON, GtkMozEmbedCommonClass)
#define GTK_IS_MOZ_EMBED_COMMON(obj)          GTK_CHECK_TYPE((obj), GTK_TYPE_MOZ_EMBED_COMMON)
#define GTK_IS_MOZ_EMBED_COMMON_CLASS(klass)  GTK_CHECK_CLASS_TYPE((klass), GTK_TYPE_MOZ_EMBED_COMMON)
typedef struct _GtkMozEmbedCommon      GtkMozEmbedCommon;
typedef struct _GtkMozEmbedCommonClass GtkMozEmbedCommonClass;
struct _GtkMozEmbedCommon
{
  GtkBin object;
  void   *data;
};
struct _GtkMozEmbedCommonClass
{
  GtkBinClass parent_class;
  gboolean (* certificate_error)        (GObject *, GObject*, guint);
  gint     (* select_login)             (GObject *, GList*);
  gint     (* remember_login)           (GObject *);
  void     (* ask_cookie)               (GObject * , gint , const gchar * , const gchar * , const gchar * ,
                                         const gchar *, const gchar * , gboolean , gint , GObject *);
//  void (* ask_cookie)                 (GObject * , gpointer , gint , const gchar * , const gchar * , const gchar * ,
//                                       const gchar *, const gchar * , gboolean , gint , GObject *);
//  gint (* modal_dialog)               (GObject *, const gchar *, const gchar *, gint, gint, gint, gint);
//  gboolean (* certificate_dialog)     (GObject * , GObject *);
//  void (*certificate_password_dialog) (GObject *, const gchar *, const gchar *, gchar **);
//  void (*certificate_details)         (GObject *, gpointer );
//  gint (*on_submit)                   (GObject *);
//  gint (*select_match)                (GObject *, gpointer);
//  void (*history_added)               (GObject *, const gchar *, GObject*);
};
typedef enum
{
  GTK_MOZ_EMBED_CERT_VERIFIED_OK           = 0x0000,
  GTK_MOZ_EMBED_CERT_UNTRUSTED             = 0x0001,
  GTK_MOZ_EMBED_CERT_NOT_VERIFIED_UNKNOWN  = 0x0002,
  GTK_MOZ_EMBED_CERT_EXPIRED               = 0x0004,
  GTK_MOZ_EMBED_CERT_REVOKED               = 0x0008,
  GTK_MOZ_EMBED_UNKNOWN_CERT               = 0x0010,
  GTK_MOZ_EMBED_CERT_ISSUER_UNTRUSTED      = 0x0020,
  GTK_MOZ_EMBED_CERT_ISSUER_UNKNOWN        = 0x0040,
  GTK_MOZ_EMBED_CERT_INVALID_CA            = 0x0080
} GtkMozEmbedCertificateType;
typedef enum
{
  GTK_MOZ_EMBED_LOGIN_REMEMBER_FOR_THIS_SITE,
  GTK_MOZ_EMBED_LOGIN_REMEMBER_FOR_THIS_SERVER,
  GTK_MOZ_EMBED_LOGIN_NOT_NOW,
  GTK_MOZ_EMBED_LOGIN_NEVER_FOR_SITE,
  GTK_MOZ_EMBED_LOGIN_NEVER_FOR_SERVER
} GtkMozEmbedLoginType;
/*  GTK_MOZ_EMBED_CERT_USAGE_NOT_ALLOWED,
  GTK_MOZ_EMBED_CA_CERT,
  GTK_MOZ_EMBED_USER_CERT,
  GTK_MOZ_EMBED_EMAIL_CERT,
  GTK_MOZ_EMBED_SERVER_CERT
*/

/** GtkMozEmbedSecurityMode.
  * Enumerates security modes.
  */
typedef enum
{
  GTK_MOZ_EMBED_NO_SECURITY = 0,
  GTK_MOZ_EMBED_LOW_SECURITY,
  GTK_MOZ_EMBED_MEDIUM_SECURITY,
  GTK_MOZ_EMBED_HIGH_SECURITY,
  GTK_MOZ_EMBED_UNKNOWN_SECURITY
} GtkMozEmbedSecurityMode;

typedef struct _GtkMozCookieList GtkMozCookieList;
struct _GtkMozCookieList
{
    gchar *domain; /** < The domain's name */
    gchar *name;   /** < The cookie's name */
    gchar *value;  /** < The cookie's value */
    gchar *path;   /** < The cookie's path */
};
typedef struct _GtkMozEmbedCookie GtkMozEmbedCookie;
struct _GtkMozEmbedCookie
{
    gboolean remember_decision;
    gboolean accept;
};
/** @struct GtkMozPlugin.
 * Defines a Mozilla Plugin.
 */
typedef struct _GtkMozPlugin GtkMozPlugin;
struct _GtkMozPlugin
{
    gchar *title;  /** < Plugin title */
    gchar *path;   /** < Plugin path */
    gchar *type;   /** < Plugin type */
    gboolean isDisabled; /** < is plugin enabled */
};

typedef struct _GtkMozLogin GtkMozLogin;
struct _GtkMozLogin
{
    const gchar *user; /** < Plugin title */
    const gchar *pass; /** < Plugin path */
    const gchar *host; /** < Plugin type */
    guint index;
};

GTKMOZEMBED_API(GtkType,    gtk_moz_embed_common_get_type,          (void))
GTKMOZEMBED_API(GtkWidget*, gtk_moz_embed_common_new,               (void))
GTKMOZEMBED_API(gboolean,   gtk_moz_embed_common_set_pref,          (GtkType type, gchar*, gpointer))
GTKMOZEMBED_API(gboolean,   gtk_moz_embed_common_get_pref,          (GtkType type, gchar*, gpointer))
GTKMOZEMBED_API(gboolean,   gtk_moz_embed_common_save_prefs,        (void))
GTKMOZEMBED_API(gboolean,   gtk_moz_embed_common_login,             (GtkWidget *embed, const gchar* username))
GTKMOZEMBED_API(gboolean,   gtk_moz_embed_common_remove_passwords,  (const gchar *host, const gchar *user, gint index))
GTKMOZEMBED_API(gint,       gtk_moz_embed_common_get_logins,        (const char* uri, GList **list))
GTKMOZEMBED_API(gint,       gtk_moz_embed_common_get_history_list,  (GtkMozHistoryItem **GtkHI))
GTKMOZEMBED_API(gint,       gtk_moz_embed_common_remove_history,    (gchar *url, gint time))
GTKMOZEMBED_API(GSList*,    gtk_moz_embed_common_get_cookie_list,   (void))
GTKMOZEMBED_API(gint,       gtk_moz_embed_common_delete_all_cookies,(GSList *deletedCookies))
GTKMOZEMBED_API(unsigned char*, gtk_moz_embed_common_nsx509_to_raw, (void *nsIX509Ptr, guint *len))
GTKMOZEMBED_API(gint,       gtk_moz_embed_common_get_plugins_list,  (GList **pluginArray))
GTKMOZEMBED_API(void,       gtk_moz_embed_common_reload_plugins,    (void))
GTKMOZEMBED_API(guint,      gtk_moz_embed_common_get_security_mode, (guint sec_state))
GTKMOZEMBED_API(gint,       gtk_moz_embed_common_clear_cache,       (void))
GTKMOZEMBED_API(gboolean,   gtk_moz_embed_common_observe,           (const gchar*, gpointer, const gchar*, gunichar*))


/*typedef struct _GtkMozEmbedCertContext GtkMozEmbedCertContext;
struct _GtkMozEmbedCertContext
{
    GObject * cert;
    guint message;
    GtkWidget *parent;
};*/
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* gtkmozembed_common_h */
