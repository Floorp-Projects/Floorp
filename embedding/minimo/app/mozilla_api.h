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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <gdk/gdkkeysyms.h>

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

#define PREF_ID NS_PREF_CONTRACTID

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

#ifdef MOZ_JPROF
#include "jprof.h"
#endif

/* Mozilla API Methods - make possible the direct comunitation between Minimo Browser and the "Gecko Render Engine" */

static PRUnichar *mozilla_locale_to_unicode (const gchar *locStr);
static gboolean mozilla_find(GtkMozEmbed *b, const char *exp, PRBool IgnoreCase,
                             PRBool SearchBackWards, PRBool DoWrapFind,
                             PRBool SearchEntireWord, PRBool SearchInFrames,
                             PRBool DidFind);
static gboolean mozilla_preference_set (const char *preference_name, const char *new_value);
static gboolean mozilla_preference_set_boolean (const char *preference_name, gboolean new_boolean_value);
static gboolean mozilla_preference_set_int (const char *preference_name, gint  new_int_value);
static gboolean mozilla_save(GtkMozEmbed *b, gchar *file_name, gint all);
static gchar *mozilla_get_link_message (GtkWidget *embed);


static PangoFontDescription* getOrCreateDefaultMinimoFont();

/*
 * mozilla_locale_to_unicode: Decodes a string encoded for the current
 * locale into unicode. Used for getting text entered in a GTK+ entry
 * into a form which mozilla can use.
 *
 */
static PRUnichar *mozilla_locale_to_unicode (const gchar *locStr)
{
  
  if (locStr == NULL)
  {
    return (NULL);
  }
  nsAutoString autoStr;
  autoStr.AssignWithConversion (locStr);       
  PRUnichar *uniStr = ToNewUnicode(autoStr);
  return ( uniStr );
}

/*
 * mozilla_find: Look for a text in the current shown webpage
 *
 */
static gboolean 
mozilla_find(GtkMozEmbed *b, const char *exp, PRBool IgnoreCase,
             PRBool SearchBackWards, PRBool DoWrapFind,
             PRBool SearchEntireWord, PRBool SearchInFrames,
             PRBool DidFind)
{
  PRUnichar *search_string;
  g_return_val_if_fail(b != NULL, FALSE);
  nsIWebBrowser *wb = nsnull;
  gtk_moz_embed_get_nsIWebBrowser( b, &wb);
  nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(wb));
  search_string = mozilla_locale_to_unicode(exp);
  finder->SetSearchString(search_string);
  
  finder->SetFindBackwards(SearchBackWards);
  finder->SetWrapFind(DoWrapFind);
  finder->SetEntireWord(SearchEntireWord);
  finder->SetSearchFrames(SearchInFrames);
  finder->SetMatchCase(IgnoreCase);
  finder->FindNext(&DidFind);
  
  g_free(search_string);
  return ( DidFind );
}

/*
 * mozilla_preference_set: Method used to set up some specific text-based parameters of the gecko render engine
 *
 */
static gboolean
mozilla_preference_set (const char *preference_name, const char *new_value)
{
  g_return_val_if_fail (preference_name != NULL, FALSE);
  g_return_val_if_fail (new_value != NULL, FALSE);
  
  nsCOMPtr<nsIPref> pref = do_CreateInstance(PREF_ID);
  
  if (pref)
  {
    nsresult rv = pref->SetCharPref (preference_name, new_value);
    return ( NS_SUCCEEDED (rv) ? TRUE : FALSE );
  }
  return (FALSE);
}

/*
 * mozilla_preference_set: Method used to set up some specific boolean-based parameters of the gecko render engine
 *
 */
static gboolean
mozilla_preference_set_boolean (const char *preference_name, gboolean new_boolean_value)
{
  
  g_return_val_if_fail (preference_name != NULL, FALSE);
  
  nsCOMPtr<nsIPref> pref = do_CreateInstance(PREF_ID);
  
  if (pref)
  {
    nsresult rv = pref->SetBoolPref (preference_name,  new_boolean_value ? PR_TRUE : PR_FALSE);
    return ( NS_SUCCEEDED (rv) ? TRUE : FALSE );
  }
  
  return (FALSE);
}	

/*
 * mozilla_preference_set: Method used to set up some specific integer-based parameters of the gecko render engine
 *
 */
static gboolean
mozilla_preference_set_int (const char *preference_name, gint  new_int_value)
{
  g_return_val_if_fail (preference_name != NULL, FALSE);
  
  nsCOMPtr<nsIPref> pref = do_CreateInstance(PREF_ID);
  
  if (pref)
  {
    nsresult rv = pref->SetIntPref (preference_name, new_int_value);
    return ( NS_SUCCEEDED (rv) ? TRUE : FALSE );
  }
  
  return (FALSE);
  
}

/*
 * mozilla_save: Method used to save an webpage and its files into the disk
 *
 */
static gboolean
mozilla_save(GtkMozEmbed *b, gchar *file_name, gint all)
{
  
  nsCOMPtr<nsIWebBrowser> wb;
  gint i = 0;
  gchar *relative_path = NULL;
  g_return_val_if_fail(b != NULL, FALSE);
  
  gtk_moz_embed_get_nsIWebBrowser(b,getter_AddRefs(wb));
  if (!wb) return (FALSE);
  
  nsCOMPtr<nsIWebNavigation> nav(do_QueryInterface(wb));
  
  nsCOMPtr<nsIDOMDocument> domDoc;
  nav->GetDocument(getter_AddRefs(domDoc));
  
  if (!domDoc) return (FALSE);
  nsCOMPtr<nsIWebBrowserPersist> persist(do_QueryInterface(wb));
  
  if (persist) {
    nsCOMPtr<nsILocalFile> file;
    nsCOMPtr<nsILocalFile> relative = nsnull;
    if (all) {
      
      relative_path = g_strdup(file_name);
      relative_path = g_strconcat(relative_path,"_files/", NULL);
      
      for (i=strlen(relative_path)-1 ; i >= 0; --i)
        if (relative_path[i] == '/') {
          relative_path[i] = '\0';
          break;
        }
      
      mkdir(relative_path,0755);
      
      nsAutoString s; s.AssignWithConversion(relative_path);
      NS_NewLocalFile(s, PR_TRUE, getter_AddRefs(relative));
    }
    
    nsAutoString s; s.AssignWithConversion(file_name);
    NS_NewLocalFile(s,PR_TRUE,getter_AddRefs(file));
    
    if (file) persist->SaveDocument(domDoc,file,relative, nsnull, 0, 0);
    
    if (all) g_free(relative_path);
    
    return (TRUE);
  }
  return (FALSE);
}

/*
 * mozilla_get_link_message: Method used to get "link messages" from the Gecko Render Engine. 
 * eg. When the user put the mouse over a link, the render engine throws messegas that can be cautch for after 
 * used by Minimo Browser.
 *
 */
static 
gchar *mozilla_get_link_message (GtkWidget *embed) {
  
  gchar *link;
  
  link = gtk_moz_embed_get_link_message (GTK_MOZ_EMBED (embed));
  
  return link;
  
}

static
gboolean mozilla_save_prefs (void) {
  
  nsCOMPtr<nsIPrefService> prefService = 
    do_GetService (PREF_ID);
  g_return_val_if_fail (prefService != nsnull, FALSE);
  nsresult rv = prefService->SavePrefFile (nsnull);
  return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}
