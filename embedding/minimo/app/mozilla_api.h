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
 *   Doug Turner <dougt@meer.net>  Branched from TestGtkEmbed.cpp
 *
 *   The 10LE Team (in alphabetical order)
 *   -------------------------------------
 *
 *    Ilias Biris       <ext-ilias.biris@indt.org.br> - Coordinator
 *    Afonso Costa      <afonso.costa@indt.org.br>
 *    Antonio Gomes     <antonio.gomes@indt.org.br>
 *    Diego Gonzalez    <diego.gonzalez@indt.org.br>
 *    Raoni Novellino   <raoni.novellino@indt.org.br>
 *    Andre Pedralho    <andre.pedralho@indt.org.br>
 *
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

#ifndef mozilla_api_h
#define mozilla_api_h

#include "minimo_types.h"

/* mozilla specific headers */
#include "prenv.h"
#include "nsMemory.h"
#include "gtkmozembed_internal.h"

/* Includes used by Find features */
#include "nsIWebBrowser.h"
#include "nsIWebBrowserFind.h"
#include "nsNetUtil.h"

/* Includes used by 'mozilla_preference_set', 'mozilla_preference_set_int' and 'mozilla_preference_set_boolean' */
#include "nsIPref.h"

/* Includes used by 'save_mozilla' */
#include "nsIPresShell.h"
#include "nsIDocShell.h"
#include "nsIDOMDocument.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWebNavigation.h"
#include "nsIDocShellTreeItem.h"
#include "nsILocalFile.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocument.h"

/* Includes used by 'menu context' */
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNSHTMLElement.h"

#define PREF_ID NS_PREF_CONTRACTID

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

#ifdef MOZ_JPROF
#include "jprof.h"
#endif

/* Mozilla API Methods - make possible the direct comunitation between Minimo Browser and the "Gecko Render Engine" */

PRUnichar *mozilla_locale_to_unicode (const gchar *locStr);
gboolean mozilla_find(GtkMozEmbed *b, const char *exp, PRBool IgnoreCase,
                      PRBool SearchBackWards, PRBool DoWrapFind,
                      PRBool SearchEntireWord, PRBool SearchInFrames,
                      PRBool DidFind);
gboolean mozilla_preference_set (const char *preference_name, const char *new_value);
gboolean mozilla_preference_set_boolean (const char *preference_name, gboolean new_boolean_value);
gboolean mozilla_preference_set_int (const char *preference_name, gint  new_int_value);
gboolean mozilla_save(GtkMozEmbed *b, gchar *file_name, gint all);
void mozilla_save_image(GtkMozEmbed *b,gchar *location, gchar *FullPath);
gchar *mozilla_get_link_message (GtkWidget *embed);
glong mozilla_get_context_menu_type(GtkMozEmbed *b, gpointer event,  gchar **img,  gchar **link, gchar **linktext);
gchar *mozilla_get_attribute (nsIDOMNode *node, gchar *attribute);
gboolean mozilla_save_prefs (void);

#define CONTEXT_NONE     0
#define CONTEXT_DEFAULT  1
#define CONTEXT_LINK     2
#define CONTEXT_IMAGE    4
#define CONTEXT_DOCUMENT 8
#define CONTEXT_INPUT    64
#define CONTEXT_OTHER    128
#define CONTEXT_XUL      256

#endif
