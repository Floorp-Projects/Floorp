/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *   Changes: andre.pedralho@indt.org.br (from original:  mozilla/embedding/lite/nsEmbedGlobalHistory.h)
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
#ifndef __EMBEDGLOBALHISTORY_h
#define __EMBEDGLOBALHISTORY_h
#include "nsIGlobalHistory2.h"
#include "nsIObserver.h"
#include "EmbedPrivate.h"
#include <prenv.h>
#include <gtk/gtk.h>
#include "nsDocShellCID.h"

#define OUTPUT_STREAM nsIOutputStream
#define LOCAL_FILE nsILocalFile

//#include "gtkmozembed_common.h"
/* {2f977d51-5485-11d4-87e2-0010a4e75ef2} */
#define NS_EMBEDGLOBALHISTORY_CID \
  { 0x2f977d51, 0x5485, 0x11d4, \
  { 0x87, 0xe2, 0x00, 0x10, 0xa4, 0xe7, 0x5e, 0xf2 } }
#define EMBED_HISTORY_PREF_EXPIRE_DAYS      "browser.history_expire_days"
/** The Mozilla History Class
  * This class is responsible for handling the history stuff.
  */
class EmbedGlobalHistory: public nsIGlobalHistory2,
                          public nsIObserver
{
    public:
    EmbedGlobalHistory();
    virtual ~EmbedGlobalHistory();
    static EmbedGlobalHistory* GetInstance();
    static void DeleteInstance();
    NS_IMETHOD        Init();
    nsresult GetContentList(GtkMozHistoryItem**, int *count);
    NS_DECL_ISUPPORTS
    NS_DECL_NSIGLOBALHISTORY2
    NS_DECL_NSIOBSERVER
    nsresult RemoveEntries(const PRUnichar *url = nsnull, int time = 0);

    protected:
    enum {
        kFlushModeAppend,      /** < Add a new entry in the history file */
        kFlushModeFullWrite    /** < Rewrite all history file */
    };
/** Initiates the history file
  * @return NS_OK on the success.
  */
    nsresult          InitFile();
/** Loads the history file
  * @return NS_OK on the success.
  */
    nsresult          LoadData();
/** Writes entries in the history file
  * @param list The internal history list.
  * @param handle A Gnome VFS handle.
  * @return NS_OK on the success.
*/
    nsresult          WriteEntryIfWritten(GList *list, OUTPUT_STREAM *file_handle);
/** Writes entries in the history file
 * @param list The internal history list.
 * @param handle A Gnome VFS handle.
 * @return NS_OK on the success.
*/
    nsresult          WriteEntryIfUnwritten(GList *list, OUTPUT_STREAM *file_handle);
/** Writes entries in the history file
  * @param mode How to write in the history file
  * @return NS_OK on the success.
  */
    nsresult          FlushData(PRIntn mode = kFlushModeFullWrite);
 /** Remove entries from the URL table
  * @return NS_OK on the success.
  */
    nsresult          ResetData();
/** Reads the history entries using GnomeVFS
  * @param vfs_handle A Gnome VFS handle.
  * @return NS_OK on the success.
  */
    nsresult          ReadEntries(LOCAL_FILE *file_uri);
/** Gets a history entry 
  * @param name The history entry name.
  * @return NS_OK if the history entry name was gotten.
  */
    nsresult          GetEntry(const char *);
    protected:
    OUTPUT_STREAM    *mFileHandle;              /** < The History File handle */
    PRBool            mDataIsLoaded;            /** < If the data is loaded */
    PRBool            mFlushModeFullWriteNeeded;/** < If needs a full flush */
    PRInt32           mEntriesAddedSinceFlush;  /** < Number of entries added since flush */
    gchar*            mHistoryFile;             /** < The history file path */
};
// Default maximum history entries
static const PRUint32 kDefaultMaxSize = 1000;
#endif
