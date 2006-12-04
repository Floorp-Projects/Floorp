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
#include "gtkmozembed_common.h"
#include "gtkmozembedprivate.h"
#include "gtkmozembed_internal.h"
#include "EmbedPrivate.h"
#include "EmbedWindow.h"

#ifdef MOZ_GTKPASSWORD_INTERFACE
#include "EmbedPasswordMgr.h"
#include "nsIPassword.h"
#endif

#include "EmbedGlobalHistory.h"
//#include "EmbedDownloadMgr.h"
// so we can do our get_nsIWebBrowser later...
#include <nsIWebBrowser.h>
#include <nsIComponentManager.h>
#include <nsIServiceManager.h>
#include "nsIPref.h"
#include <nsICookieManager.h>
#include <nsIPermissionManager.h>
#include <nsNetCID.h>
#include <nsICookie.h>
#include <nsIX509Cert.h>
// for strings
#ifdef MOZILLA_INTERNAL_API
#include <nsXPIDLString.h>
#include <nsReadableUtils.h>
#else
#include <nsStringAPI.h>
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#endif
// for plugins
#include <nsIDOMNavigator.h>
#include <nsIDOMPluginArray.h>
#include <nsIDOMPlugin.h>
#include <plugin/nsIPluginHost.h>
#include <nsString.h>
#include <nsIDOMMimeType.h>

//for security
#include <nsIWebProgressListener.h>

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

// CID used to get the plugin manager
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);

// class and instance initialization

static void
gtk_moz_embed_common_class_init(GtkMozEmbedCommonClass *klass);

static void
gtk_moz_embed_common_init(GtkMozEmbedCommon *embed);

static GtkObjectClass *common_parent_class = NULL;

// GtkObject methods

static void
gtk_moz_embed_common_destroy(GtkObject *object);

guint moz_embed_common_signals[COMMON_LAST_SIGNAL] = { 0 };

// GtkObject + class-related functions
GtkType
gtk_moz_embed_common_get_type(void)
{
  static GtkType moz_embed_common_type = 0;
  if (!moz_embed_common_type)
  {
    static const GtkTypeInfo moz_embed_common_info =
    {
      "GtkMozEmbedCommon",
      sizeof(GtkMozEmbedCommon),
      sizeof(GtkMozEmbedCommonClass),
      (GtkClassInitFunc)gtk_moz_embed_common_class_init,
      (GtkObjectInitFunc)gtk_moz_embed_common_init,
      0,
      0,
      0
    };
    moz_embed_common_type = gtk_type_unique(GTK_TYPE_BIN, &moz_embed_common_info);
  }
  return moz_embed_common_type;
}

static void
gtk_moz_embed_common_class_init(GtkMozEmbedCommonClass *klass)
{
  GtkObjectClass     *object_class;
  object_class    = GTK_OBJECT_CLASS(klass);
  common_parent_class = (GtkObjectClass *)gtk_type_class(gtk_object_get_type());
  object_class->destroy = gtk_moz_embed_common_destroy;
  moz_embed_common_signals[COMMON_CERT_ERROR] =
    gtk_signal_new("certificate-error",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass,
                                     certificate_error),
                   gtkmozembed_BOOL__POINTER_UINT,
                   G_TYPE_BOOLEAN,
                   2,
                   GTK_TYPE_POINTER,
                   GTK_TYPE_UINT);
  moz_embed_common_signals[COMMON_SELECT_LOGIN] =
    gtk_signal_new("select-login",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass,
                                     select_login),
                   gtk_marshal_INT__POINTER,
                   G_TYPE_INT,
                   1,
                   G_TYPE_POINTER);
  moz_embed_common_signals[COMMON_REMEMBER_LOGIN] =
    gtk_signal_new("remember-login",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass,
                                     remember_login),
                   gtkmozembed_INT__VOID,
                   G_TYPE_INT, 0);
  // set up our signals
  moz_embed_common_signals[COMMON_ASK_COOKIE] =
    gtk_signal_new("ask-cookie",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass, ask_cookie),
                   gtkmozembed_VOID__POINTER_INT_STRING_STRING_STRING_STRING_STRING_BOOLEAN_INT,
                   G_TYPE_NONE, 9,
                   GTK_TYPE_POINTER,
                   GTK_TYPE_INT,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_BOOL,
                   GTK_TYPE_INT);
/*  
  moz_embed_common_signals[COMMON_CERT_DIALOG] =
    gtk_signal_new("certificate-dialog",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass,
                                     certificate_dialog),
                   gtk_marshal_BOOL__POINTER,
                   G_TYPE_BOOLEAN, 1, GTK_TYPE_POINTER);
  moz_embed_common_signals[COMMON_CERT_PASSWD_DIALOG] =
    gtk_signal_new("certificate-password-dialog",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass,
                                     certificate_password_dialog),
                   gtkmozembed_VOID__STRING_STRING_POINTER,
                   G_TYPE_NONE,
                   3,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                   GTK_TYPE_POINTER);
  moz_embed_common_signals[COMMON_CERT_DETAILS_DIALOG] =
    gtk_signal_new("certificate-details",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass,
                                     certificate_details),
                   gtk_marshal_VOID__POINTER,
                   G_TYPE_NONE,
                   1,
                   GTK_TYPE_POINTER);
  moz_embed_common_signals[COMMON_HISTORY_ADDED] =
    gtk_signal_new("global-history-item-added",
                   GTK_RUN_FIRST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass,
                                     history_added),
                   gtk_marshal_VOID__STRING,
                   G_TYPE_NONE,
                   1,
                   GTK_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
  moz_embed_common_signals[COMMON_ON_SUBMIT_SIGNAL] =
    gtk_signal_new("on-submit",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass,
                                     on_submit),
                   gtkmozembed_INT__VOID,
                   G_TYPE_INT, 0);
  moz_embed_common_signals[COMMON_MODAL_DIALOG] =
    gtk_signal_new("modal_dialog",
                   GTK_RUN_LAST,
                   GET_OBJECT_CLASS_TYPE(klass),
                   GTK_SIGNAL_OFFSET(GtkMozEmbedCommonClass,
                      modal_dialog),
                   gtkmozembed_INT__STRING_STRING_INT_INT_INT_INT,
                   G_TYPE_INT,
                   6,
                   G_TYPE_STRING, G_TYPE_STRING, 
                   G_TYPE_INT, G_TYPE_INT,
                   G_TYPE_INT, G_TYPE_INT);
  */
#ifdef MOZ_WIDGET_GTK
    gtk_object_class_add_signals(object_class, moz_embed_common_signals,
                                 COMMON_LAST_SIGNAL);
#endif /* MOZ_WIDGET_GTK */
}

static void
gtk_moz_embed_common_init(GtkMozEmbedCommon *common)
{
  // this is a placeholder for later in case we need to stash data at
  // a later data and maintain backwards compatibility.
  common->data = nsnull;
  EmbedCommon *priv = EmbedCommon::GetInstance();
  priv->mCommon = common;
  common->data = priv;
  EmbedGlobalHistory::GetInstance();
}

static void
gtk_moz_embed_common_destroy(GtkObject *object)
{
  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_MOZ_EMBED_COMMON(object));
  GtkMozEmbedCommon  *embed = nsnull;
  EmbedCommon *commonPrivate = nsnull;
  embed = GTK_MOZ_EMBED_COMMON(object);
  commonPrivate = (EmbedCommon *)embed->data;
  if (commonPrivate) {
    delete commonPrivate;
    embed->data = NULL;
  }
}

GtkWidget *
gtk_moz_embed_common_new(void)
{
  GtkWidget *widget = (GtkWidget*) gtk_type_new(gtk_moz_embed_common_get_type());
  gtk_widget_set_name (widget, "gtkmozembedcommon");
  return (GtkWidget *) widget;
}

gboolean
gtk_moz_embed_common_set_pref(GtkType type, gchar *name, gpointer value)
{
  g_return_val_if_fail (name != NULL, FALSE);

  nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID);

  if (pref) {
    nsresult rv = NS_ERROR_FAILURE;
    switch (type) {
    case GTK_TYPE_BOOL:
      {
        /* I doubt this cast pair is correct */
        rv = pref->SetBoolPref (name, !!*(int*)value);
        break;
      }
    case GTK_TYPE_INT:
      {
        /* I doubt this cast pair is correct */
        rv = pref->SetIntPref (name, *(int*)value);
        break;
      }
    case GTK_TYPE_STRING:
      {
        g_return_val_if_fail (value, FALSE);
        rv = pref->SetCharPref (name, (gchar*)value);
        break;
      }
    default:
      break;
    }
    return ( NS_SUCCEEDED (rv) ? TRUE : FALSE );
  }
  return (FALSE);
}

gboolean
gtk_moz_embed_common_get_pref(GtkType type, gchar *name, gpointer value)
{
  g_return_val_if_fail (name != NULL, FALSE);

  nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID);

  nsresult rv = NS_ERROR_FAILURE;
  if (pref){
    switch (type) {
    case GTK_TYPE_BOOL:
      {
        rv = pref->GetBoolPref (name, (gboolean*)value);
        break;
      }
    case GTK_TYPE_INT:
      {
        rv = pref->GetIntPref (name, (gint*)value);
        break;
      }
    case GTK_TYPE_STRING:
      {
        rv = pref->GetCharPref (name, (gchar**)value);
        break;
      }
    default:
      break;
    }
    return ( NS_SUCCEEDED (rv) ? TRUE : FALSE );
  }
  return (FALSE);
}

gboolean
gtk_moz_embed_common_save_prefs()
{
  nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREF_CONTRACTID);
  g_return_val_if_fail (prefService != nsnull, FALSE);
  if (prefService == nsnull)
    return FALSE;
  nsresult rv = prefService->SavePrefFile (nsnull);
  return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}

#ifdef MOZ_GTKPASSWORD_INTERFACE
static GList *
gtk_moz_embed_common_get_logins(const char* uri, nsIPasswordManager *passwordManager)
{
  GList *logins = NULL;
  nsCOMPtr<nsISimpleEnumerator> passwordEnumerator;
  nsresult result = passwordManager->GetEnumerator(getter_AddRefs(passwordEnumerator));
  PRBool enumResult;
  for (passwordEnumerator->HasMoreElements(&enumResult) ;
       enumResult == PR_TRUE ;
       passwordEnumerator->HasMoreElements(&enumResult))
  {
    nsCOMPtr<nsIPassword> nsPassword;
    result = passwordEnumerator->GetNext
      (getter_AddRefs(nsPassword));
    if (NS_FAILED(result)) {
      /* this almost certainly leaks logins */
      return NULL;
    }
    nsCString transfer;
    nsPassword->GetHost (transfer);
    if (uri && !g_str_has_prefix(uri, transfer.get()))
      continue;
    nsString unicodeName;
    nsPassword->GetUser (unicodeName);
    logins = g_list_append(logins, ToNewUTF8String(unicodeName));
  }
  return logins;
}
#endif

gboolean
gtk_moz_embed_common_login(GtkWidget *embed)
{
  gint retval = -1;
#ifdef MOZ_GTKPASSWORD_INTERFACE
  EmbedPasswordMgr *passwordManager = EmbedPasswordMgr::GetInstance();
  GList * list = gtk_moz_embed_common_get_logins(gtk_moz_embed_get_location(GTK_MOZ_EMBED(embed)), passwordManager);
  gtk_signal_emit(
      GTK_OBJECT(GTK_MOZ_EMBED(embed)->common),
      moz_embed_common_signals[COMMON_SELECT_LOGIN],
      list,
      &retval);
  if (retval != -1) {
    passwordManager->InsertLogin((const gchar*)g_list_nth_data(list, retval));
  }
  g_list_free(list);
#endif
  return retval != -1;
}

gboolean
gtk_moz_embed_common_remove_passwords(const gchar *host, const gchar *user)
{
#ifdef MOZ_GTKPASSWORD_INTERFACE
  EmbedPasswordMgr *passwordManager = EmbedPasswordMgr::GetInstance();
  passwordManager->RemovePasswords(host, user);
#endif
  return TRUE;
}

gboolean
gtk_moz_embed_common_remove_passwords_by_index (gint index)
{
#ifdef MOZ_GTKPASSWORD_INTERFACE
  EmbedPasswordMgr *passwordManager = EmbedPasswordMgr::GetInstance();
  passwordManager->RemovePasswordsByIndex(index);
#endif
  return TRUE;
}

gint
gtk_moz_embed_common_get_number_of_saved_passwords ()
{
  gint saved_passwd_num = 0;
#ifdef MOZ_GTKPASSWORD_INTERFACE
  EmbedPasswordMgr *passwordManager = EmbedPasswordMgr::GetInstance();
  passwordManager->GetNumberOfSavedPassword ( &saved_passwd_num);
#endif
  return saved_passwd_num;
}

gint
gtk_moz_embed_common_get_history_list (GtkMozHistoryItem **GtkHI)
{
  gint count = 0;
  EmbedGlobalHistory *history = EmbedGlobalHistory::GetInstance();
  history->GetContentList(GtkHI, &count);
  return count;
}

gint 
gtk_moz_embed_common_clean_all_history () {
  nsresult rv;
  // The global history service
  nsCOMPtr<nsIGlobalHistory2> globalHistory(do_GetService("@mozilla.org/browser/global-history;2"));
  if (!globalHistory) return NS_ERROR_NULL_POINTER; 
  // The browser history interface
  nsCOMPtr<nsIObserver> myHistory = do_QueryInterface(globalHistory, &rv);
  if (!myHistory) return NS_ERROR_NULL_POINTER ;
  myHistory->Observe(nsnull, "RemoveAllPages", nsnull);
  return 1;
}

GSList*
gtk_moz_embed_common_get_cookie_list(void)
{
  GSList *cookies = NULL;
  nsresult result;
  nsCOMPtr<nsICookieManager> cookieManager =
    do_GetService(NS_COOKIEMANAGER_CONTRACTID);
  nsCOMPtr<nsISimpleEnumerator> cookieEnumerator;
  result = cookieManager->GetEnumerator(getter_AddRefs(cookieEnumerator));
  g_return_val_if_fail(NS_SUCCEEDED(result), NULL);
  PRBool enumResult;
  for (cookieEnumerator->HasMoreElements(&enumResult);
       enumResult == PR_TRUE;
       cookieEnumerator->HasMoreElements(&enumResult))
  {
    GtkMozCookieList *c;
    nsCOMPtr<nsICookie> nsCookie;
    result = cookieEnumerator->GetNext(getter_AddRefs(nsCookie));
    g_return_val_if_fail(NS_SUCCEEDED(result), NULL);
    c = g_new0(GtkMozCookieList, 1);
    nsCAutoString transfer;
    nsCookie->GetHost(transfer);
    c->domain = g_strdup(transfer.get());
    nsCookie->GetName(transfer);
    c->name = g_strdup(transfer.get());
    nsCookie->GetValue(transfer);
    c->value = g_strdup(transfer.get());
    nsCookie->GetPath(transfer);
    /* this almost certainly leaks g_strconcat */
    if (strchr(c->domain,'.'))
      c->path = g_strdup(g_strconcat("http://*",c->domain,"/",NULL));
    else
      c->path = g_strdup(g_strconcat("http://",c->domain,"/",NULL));
    cookies = g_slist_prepend(cookies, c);
  }
  cookies = g_slist_reverse(cookies);
  return cookies;
}

gint
gtk_moz_embed_common_delete_all_cookies (GSList *deletedCookies)
{
  if (!deletedCookies) 
    return 1;

  nsCOMPtr<nsICookieManager> cookieManager =
    do_GetService(NS_COOKIEMANAGER_CONTRACTID);

  if (!cookieManager) 
    return 1;
  cookieManager->RemoveAll();

  nsCOMPtr<nsIPermissionManager> permissionManager =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

  if (!permissionManager)
    return 1;

  permissionManager->RemoveAll ();

  g_slist_free (deletedCookies);
    return 0;//False in GWebStatus means OK, as opposed to gboolean in C
}

unsigned char *
gtk_moz_embed_common_nsx509_to_raw(void *nsIX509Ptr, guint *len)
{
  if (!nsIX509Ptr)
    return NULL;
  unsigned char *data;
  ((nsIX509Cert*)nsIX509Ptr)->GetRawDER(len, (PRUint8 **)&data);
  if (!data)
    return NULL;
  return data;
}

void
gtk_moz_embed_common_get_plugins_list (GtkMozPlugin **pluginArray, gint *num_plugins)
{
  nsresult rv; 
  nsCOMPtr<nsIPluginManager> pluginMan = 
    do_GetService(kPluginManagerCID, &rv);
  if (NS_FAILED(rv)) {
    g_print("Could not get the plugin manager\n");
    return;
  }
  pluginMan->ReloadPlugins(PR_TRUE);
  nsCOMPtr<nsIPluginHost> pluginHost = 
    do_GetService(kPluginManagerCID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }
  PRUint32 aLength;
  nsIDOMPlugin **aItem;
  pluginHost->GetPluginCount(&aLength);
  *num_plugins = aLength;
  gint size = aLength;
  aItem = new nsIDOMPlugin*[aLength];
  pluginHost->GetPlugins(aLength, aItem);
  *pluginArray = (GtkMozPlugin*) g_try_malloc(size*sizeof(GtkMozPlugin));
  for (int aIndex = 0; aIndex < (gint) aLength; aIndex++)
  { 
    nsAutoString aName;
    aItem[aIndex]->GetName(aName);
    NS_ConvertUTF16toUTF8 utf8ValueTitle(aName);
    (*pluginArray)[aIndex].title = g_strdup((gchar *)utf8ValueTitle.get());
    nsAutoString aFilename;
    aItem[aIndex]->GetFilename(aFilename);
    NS_ConvertUTF16toUTF8 utf8ValueFilename(aFilename);
    (*pluginArray)[aIndex].path =g_strdup((gchar *)utf8ValueFilename.get());
    nsCOMPtr<nsIDOMMimeType> mimeType;
    aItem[aIndex]->Item(aIndex, getter_AddRefs(mimeType));
    nsAutoString aDescription;
    mimeType->GetDescription(aDescription);
    NS_ConvertUTF16toUTF8 utf8ValueDescription(aDescription);
    (*pluginArray)[aIndex].type = g_strdup((gchar *)utf8ValueDescription.get());
  }
  return;
}

void
gtk_moz_embed_common_reload_plugins ()
{
  nsresult rv;
  nsCOMPtr<nsIPluginManager> pluginMan = 
    do_GetService(kPluginManagerCID, &rv);
  pluginMan->ReloadPlugins(PR_TRUE);
}

guint
gtk_moz_embed_common_get_security_mode (guint sec_state)
{
  GtkMozEmbedSecurityMode sec_mode;

  switch (sec_state) {
    case nsIWebProgressListener::STATE_IS_INSECURE:
      sec_mode = GTK_MOZ_EMBED_NO_SECURITY;
      //g_print("GTK_MOZ_EMBED_NO_SECURITY\n");
      break;
    case nsIWebProgressListener::STATE_IS_BROKEN:
      sec_mode = GTK_MOZ_EMBED_NO_SECURITY;
      //g_print("GTK_MOZ_EMBED_NO_SECURITY\n");
      break;
    case nsIWebProgressListener::STATE_IS_SECURE|
      nsIWebProgressListener::STATE_SECURE_HIGH:
      sec_mode = GTK_MOZ_EMBED_HIGH_SECURITY;
      //g_print("GTK_MOZ_EMBED_HIGH_SECURITY");
      break;
    case nsIWebProgressListener::STATE_IS_SECURE|
      nsIWebProgressListener::STATE_SECURE_MED:
      sec_mode = GTK_MOZ_EMBED_MEDIUM_SECURITY;
      //g_print("GTK_MOZ_EMBED_MEDIUM_SECURITY\n");
      break;
    case nsIWebProgressListener::STATE_IS_SECURE|
      nsIWebProgressListener::STATE_SECURE_LOW:
      sec_mode = GTK_MOZ_EMBED_LOW_SECURITY;
      //g_print("GTK_MOZ_EMBED_LOW_SECURITY\n");
      break;
    default:
      sec_mode = GTK_MOZ_EMBED_UNKNOWN_SECURITY;
      //g_print("GTK_MOZ_EMBED_UNKNOWN_SECURITY\n");
      break;
  }
  return sec_mode;
}

