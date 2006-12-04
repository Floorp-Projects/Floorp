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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *   (from original mozilla/embedding/lite/nsEmbedGlobalHistory.cpp)
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
/* XXX This file is probably out of sync with its original.
 *     This is a bad thing
 */
#include "EmbedGlobalHistory.h"
#include "nsIObserverService.h"
#include "nsAutoPtr.h"
#include <nsIURI.h>
#include <nsInt64.h>
#include <nsIIOService.h>
#include <nsNetUtil.h>
#include "gtkmozembed_common.h"
#ifndef MOZILLA_INTERNAL_API
#include "nsCRT.h"
#endif
// Constants
#define defaultSeparator 1
// Number of changes in history before automatic flush
static const PRInt32 kNewEntriesBetweenFlush = 10;
// Default expiration interval: used if can't get preference service value
static const PRUint32 kDefaultExpirationIntervalDays = 7;
// Mozilla and EAL standard are different each other
static const PRInt64 kMSecsPerDay = LL_INIT(0, 60 * 60 * 24 * 1000);
static const PRInt64 kOneThousand = LL_INIT(0, 1000);
// The history list and the entries counter
static GList *mURLList;                 /** < The history list */
static PRInt64 mExpirationInterval;     /** < Expiration interval time */
static EmbedGlobalHistory *sEmbedGlobalHistory = nsnull;

//*****************************************************************************
// HistoryEntryOld: an entry object and its methods
//*****************************************************************************   
typedef struct _HistoryEntry {
  PRInt64         mLastVisitTime;     // Millisecs
  PRPackedBool    mWritten;           // TRUE if ever persisted
  nsCString       mTitle;             // The entry title
  char            *url;               // The url itself
} HistoryEntry;

static void close_file_handle(void *file_handle)
{
  g_return_if_fail(file_handle);
  gnome_vfs_close((GnomeVFSHandle*) file_handle);
}

static bool file_handle_uri_exists(const void *uri)
{
  g_return_val_if_fail(uri, false);
  return gnome_vfs_uri_exists((GnomeVFSURI*)uri);
}

static void* file_handle_uri_new(const char *uri)
{
  g_return_val_if_fail(uri, nsnull);
  return gnome_vfs_uri_new(uri);
}

static bool file_handle_create_uri(void *file_handle, const void *uri)
{
  g_return_val_if_fail(file_handle, false);
  return gnome_vfs_create_uri(
    (GnomeVFSHandle**)file_handle,
    (GnomeVFSURI*)uri,
    GNOME_VFS_OPEN_WRITE,
    1,
    0600
  ) == GNOME_VFS_OK;
}

static bool file_handle_open_uri(void *file_handle, const void *uri)
{
  g_return_val_if_fail(file_handle, false);
  return gnome_vfs_open_uri(
    (GnomeVFSHandle**)file_handle,
    (GnomeVFSURI*)uri,
    (GnomeVFSOpenMode)(GNOME_VFS_OPEN_WRITE
                      | GNOME_VFS_OPEN_RANDOM
                      | GNOME_VFS_OPEN_READ));
}

static bool file_handle_seek(void *file_handle, gboolean end)
{
  g_return_val_if_fail(file_handle, false);

  return gnome_vfs_seek((GnomeVFSHandle*)file_handle,
                        end ? GNOME_VFS_SEEK_END : GNOME_VFS_SEEK_START, 0);
}

static bool file_handle_truncate(void *file_handle)
{
  g_return_val_if_fail(file_handle, false);
  return gnome_vfs_truncate_handle ((GnomeVFSHandle*)file_handle, 0);
}

static int file_handle_file_info_block_size(void *file_handle)
{
  g_return_val_if_fail(file_handle, 0);
  GnomeVFSFileInfo info;
  gnome_vfs_get_file_info_from_handle ((GnomeVFSHandle *)file_handle,
                                       &info,
                                       GNOME_VFS_FILE_INFO_DEFAULT);
  return info.io_block_size;
}

static int64 file_handle_read(void *file_handle, gpointer buffer, guint64 bytes)
{
  g_return_val_if_fail(file_handle, false);
  GnomeVFSResult vfs_result = GNOME_VFS_OK;
  GnomeVFSFileSize read_bytes;
  vfs_result = gnome_vfs_read((GnomeVFSHandle *)file_handle,
  (gpointer) buffer, (GnomeVFSFileSize)bytes-1, &read_bytes);
  if (vfs_result!=GNOME_VFS_OK)
    return -1;

  return (int64)read_bytes;
}

static guint64 file_handle_write(void *file_handle, gpointer line)
{
  g_return_val_if_fail(file_handle, 0);
  GnomeVFSFileSize written;
  gnome_vfs_write ((GnomeVFSHandle *)file_handle,
                   (gpointer)line,
                   strlen((const char*)line),
                   &written);
  return written;
}

// Static Routine Prototypes
//GnomeVFSHandle
static nsresult writeEntry(void *file_handle, HistoryEntry *entry);
// when an entry is visited
nsresult OnVisited(HistoryEntry *entry)
{
  NS_ENSURE_ARG(entry);
  entry->mLastVisitTime = PR_Now(); 
  LL_DIV(entry->mLastVisitTime, entry->mLastVisitTime, kOneThousand);
  return NS_OK;
}

// Return the last time an entry was visited
PRInt64 GetLastVisitTime(HistoryEntry *entry)
{
  NS_ENSURE_ARG(entry);
  return entry->mLastVisitTime;
}

// Change the last time an entry was visited
nsresult SetLastVisitTime(HistoryEntry *entry, const PRInt64& aTime)
{
  NS_ENSURE_ARG(entry);
  NS_ENSURE_ARG_POINTER(aTime);
  entry->mLastVisitTime = aTime;
  return NS_OK;
}

// Return TRUE if an entry has been written
PRBool GetIsWritten(HistoryEntry *entry)
{
  NS_ENSURE_ARG(entry);
  return entry->mWritten;
}

// Set TRUE when an entry is visited
nsresult SetIsWritten(HistoryEntry *entry)
{
  NS_ENSURE_ARG(entry);
  entry->mWritten = PR_TRUE;
  return NS_OK;
}

// Change the entry title
#define SET_TITLE(entry, aTitle) entry->mTitle.Assign (aTitle);

// Change the entry title
nsresult SetURL(HistoryEntry *entry, const char *url)
{
  NS_ENSURE_ARG(entry);
  NS_ENSURE_ARG(url);
  if (entry->url)
    g_free(entry->url);
  entry->url = g_strdup(url);
  return NS_OK;
}

// Return the entry title
#define GET_TITLE(entry) (entry->mTitle)

// Return the entry url
char* GetURL(HistoryEntry *entry)
{    
  return entry->url;
}

// Traverse the history list trying to find a frame
int history_entry_find_exist (gconstpointer a, gconstpointer b)
{
  return g_ascii_strcasecmp(GetURL((HistoryEntry *) a), (char *) b);
}

// Traverse the history list looking for the correct place to add a new item
int find_insertion_place (gconstpointer a, gconstpointer b)
{
  PRInt64 lastVisitTime = GetLastVisitTime((HistoryEntry *) a);
  PRInt64 tempTime = GetLastVisitTime((HistoryEntry *) b);
  return LL_CMP(lastVisitTime, <, tempTime);
}

// Check whether an entry has expired
PRBool entryHasExpired(HistoryEntry *entry)
{
  // convert "now" from microsecs to millisecs
  PRInt64 nowInMilliSecs = PR_Now(); 
  LL_DIV(nowInMilliSecs, nowInMilliSecs, kOneThousand);
  // determine when the entry would have expired
  PRInt64 expirationIntervalAgo;
  LL_SUB(expirationIntervalAgo, nowInMilliSecs, mExpirationInterval);
  PRInt64 lastVisitTime = GetLastVisitTime(entry);
  return (LL_CMP(lastVisitTime, <, expirationIntervalAgo));
}

// Traverse the history list to get all the entries data and set the EAL history list
void history_entry_foreach_to_remove (gpointer data, gpointer user_data)
{
  HistoryEntry *entry = (HistoryEntry *) data;
  if (entry) {
    entry->url = NULL;
    entry->mLastVisitTime = 0;
    g_free(entry);
  }
}

//*****************************************************************************
// EmbedGlobalHistory - Creation/Destruction
//*****************************************************************************   
NS_IMPL_ISUPPORTS2(EmbedGlobalHistory, nsIGlobalHistory2, nsIObserver)
/* static */
EmbedGlobalHistory*
EmbedGlobalHistory::GetInstance()
{
  if (!sEmbedGlobalHistory)
  {
    sEmbedGlobalHistory = new EmbedGlobalHistory();
    if (!sEmbedGlobalHistory)
      return nsnull;
    sEmbedGlobalHistory->handle = NULL;
    NS_ADDREF(sEmbedGlobalHistory);   // addref the global
    if (NS_FAILED(sEmbedGlobalHistory->Init()))
    {
      NS_RELEASE(sEmbedGlobalHistory);
      return nsnull;
    }
  }
  NS_ADDREF(sEmbedGlobalHistory);   // addref the return result
  return sEmbedGlobalHistory;
}

/* static */
void
EmbedGlobalHistory::DeleteInstance()
{
  if (sEmbedGlobalHistory)
  {
    delete sEmbedGlobalHistory;
    sEmbedGlobalHistory = nsnull;
  }
}

// The global history component constructor
EmbedGlobalHistory::EmbedGlobalHistory()
{
  if (!mURLList) {
    mDataIsLoaded = PR_FALSE;
    mEntriesAddedSinceFlush = 0;
    LL_I2L(mExpirationInterval, kDefaultExpirationIntervalDays);
    LL_MUL(mExpirationInterval, mExpirationInterval, kMSecsPerDay);
  }
}

// The global history component destructor
EmbedGlobalHistory::~EmbedGlobalHistory()
{
  LoadData();
  FlushData();
  if (mURLList) {
    g_list_foreach(mURLList, (GFunc) history_entry_foreach_to_remove, NULL);
    g_list_free(mURLList);
  }
  if (handle) {
    close_file_handle(handle);
    handle = NULL;
  }
  if (mHistoryFile) {
    g_free(mHistoryFile);
    mHistoryFile = nsnull;
  }
}

// Initialize the global history component
NS_IMETHODIMP EmbedGlobalHistory::Init()
{
  if (mURLList) return NS_OK;
  // Get Pref and convert to millisecs
  PRInt32 expireDays;
  int success = gtk_moz_embed_common_get_pref(G_TYPE_INT, EMBED_HISTORY_PREF_EXPIRE_DAYS, &expireDays);
  if (success) {
    LL_I2L(mExpirationInterval, expireDays);
    LL_MUL(mExpirationInterval, mExpirationInterval, kMSecsPerDay);
  }
  // register to observe profile changes
  nsCOMPtr<nsIObserverService> observerService = 
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  NS_ASSERTION(observerService, "failed to get observer service");
  if (observerService)
    observerService->AddObserver(this, "quit-application", PR_FALSE);
  nsresult rv = InitFile();
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  rv = LoadData();
  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

#define BROKEN_RV_HANDLING_CODE(rv) PR_BEGIN_MACRO                        \
  if (NS_FAILED(rv)) {                                                    \ 
    /* OOPS the coder (not timeless) didn't handle this case well at all. \
     * unfortunately the coder will remain anonymous.                     \
     * XXX please fix me.                                                 \
     */                                                                   \ 
  }                                                                       \
  PR_END_MACRO

#define BROKEN_STRING_GETTER(out) PR_BEGIN_MACRO                      \
  /* OOPS the coder (not timeless) decided not to do anything in this \
   * method, but to return NS_OK anyway. That's not very polite.      \
   */                                                                 \
  out.Truncate();                                                     \
  PR_END_MACRO

#define BROKEN_STRING_BUILDER(var) PR_BEGIN_MACRO \
 /* This is just wrong */                         \
 PR_END_MACRO

//*****************************************************************************
// EmbedGlobalHistory::nsIGlobalHistory
//*****************************************************************************   
// Add a new URI to the history
NS_IMETHODIMP EmbedGlobalHistory::AddURI(nsIURI *aURI, PRBool aRedirect, PRBool aToplevel, nsIURI *aReferrer)
{
  NS_ENSURE_ARG(aURI);
  nsCAutoString URISpec;
  aURI->GetSpec(URISpec);
  const char *aURL = URISpec.get();
  if (!aToplevel)
    return NS_OK;
  PRBool isHTTP, isHTTPS;
  nsresult rv = NS_OK;
  rv |= aURI->SchemeIs("http", &isHTTP);
  rv |= aURI->SchemeIs("https", &isHTTPS);
  NS_ENSURE_SUCCESS (rv, NS_ERROR_FAILURE);
  // Only get valid uri schemes
  if (!isHTTP && !isHTTPS)
  {
    /* the following blacklist is silly.
     * if there's some need to whitelist http(s) + ftp,
     * that's what we should do.
     */
    PRBool isAbout, isImap, isNews, isMailbox, isViewSource, isChrome, isData, isJavascript;
    rv  = aURI->SchemeIs("about", &isAbout);
    rv |= aURI->SchemeIs("imap", &isImap);
    rv |= aURI->SchemeIs("news", &isNews);
    rv |= aURI->SchemeIs("mailbox", &isMailbox);
    rv |= aURI->SchemeIs("view-source", &isViewSource);
    rv |= aURI->SchemeIs("chrome", &isChrome);
    rv |= aURI->SchemeIs("data", &isData);
    rv |= aURI->SchemeIs("javascript", &isJavascript);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
    if (isAbout ||
        isImap ||
        isNews ||
        isMailbox ||
        isViewSource ||
        isChrome ||
        isData ||
        isJavascript) {
      return NS_OK;
    }
  }
#ifdef DEBUG
  g_print("[HISTORY] Visited URL: %s\n", aURL);
#endif
  rv = LoadData();
  NS_ENSURE_SUCCESS(rv, rv);
  GList *node = g_list_find_custom(mURLList, aURL, (GCompareFunc) history_entry_find_exist);
  HistoryEntry *entry = NULL;
  if (node && node->data)
    entry = (HistoryEntry *)(node->data);
  nsCAutoString hostname;
  aURI->GetHost(hostname);

  // It is not in the history
  if (!entry) {
    entry = g_new0(HistoryEntry, 1);
    rv |= OnVisited(entry);
    SET_TITLE(entry, hostname);
    rv |= SetURL(entry, aURL);
    BROKEN_RV_HANDLING_CODE(rv);
    unsigned int listSize = g_list_length(mURLList);
    if (listSize+1 > kDefaultMaxSize) {
      GList *last = g_list_last (mURLList);
      mURLList = g_list_remove (mURLList, last->data);
      mEntriesAddedSinceFlush++;
    }
    mURLList = g_list_insert_sorted(mURLList, entry,
                                    (GCompareFunc) find_insertion_place);
    // Flush after kNewEntriesBetweenFlush changes
    BROKEN_RV_HANDLING_CODE(rv);
    if (++mEntriesAddedSinceFlush >= kNewEntriesBetweenFlush)
      rv |= FlushData(kFlushModeAppend);
    // emit signal to let EAL knows a new item was added
    //FIXME REIMP g_signal_emit_by_name (g_mozilla_get_current_web(),
    //"global-history-item-added", aURL);
  } else {
    // update the last visited time
    rv |= OnVisited(entry);
    SET_TITLE(entry, hostname);
    // Move the element to the start of the list
    BROKEN_RV_HANDLING_CODE(rv);
    mURLList = g_list_remove (mURLList, entry);   
    mURLList = g_list_insert_sorted(mURLList, entry, (GCompareFunc) find_insertion_place);
    // Flush after kNewEntriesBetweenFlush changes
    BROKEN_RV_HANDLING_CODE(rv);
    if (++mEntriesAddedSinceFlush >= kNewEntriesBetweenFlush)
      rv |= FlushData(kFlushModeFullWrite);
  }
  return rv;
}

// Return TRUE if the url is already in history
NS_IMETHODIMP EmbedGlobalHistory::IsVisited(nsIURI *aURI, PRBool *_retval)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(_retval);
  nsCAutoString URISpec;
  aURI->GetSpec(URISpec);
  const char *aURL = URISpec.get();
  nsresult rv = LoadData();
  NS_ENSURE_SUCCESS(rv, rv);
  GList *node = g_list_find_custom(mURLList, aURL,
                                   (GCompareFunc) history_entry_find_exist);
  *_retval = (node && node->data);
  return rv;
}

// It is called when Mozilla get real name of a URL
NS_IMETHODIMP EmbedGlobalHistory::SetPageTitle(nsIURI *aURI,
                                               const nsAString & aTitle)
{
  NS_ENSURE_ARG(aURI);
  nsresult rv;
  // skip about: URIs to avoid reading in the db (about:blank, especially)
  PRBool isAbout;
  rv = aURI->SchemeIs("about", &isAbout);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isAbout)
    return NS_OK;
  nsCAutoString URISpec;
  aURI->GetSpec(URISpec);
  const char *aURL = URISpec.get();
  rv |= LoadData();
  BROKEN_RV_HANDLING_CODE(rv);
  NS_ENSURE_SUCCESS(rv, rv);

  GList *node = g_list_find_custom(mURLList, aURL,
                                   (GCompareFunc) history_entry_find_exist);
  HistoryEntry *entry = NULL;
  if (node)
    entry = (HistoryEntry *)(node->data);
  if (entry) {
    nsCString title;
    CopyUTF16toUTF8(aTitle, title);
    SET_TITLE(entry, title);
    BROKEN_RV_HANDLING_CODE(rv);
    if (++mEntriesAddedSinceFlush >= kNewEntriesBetweenFlush)
      rv |= FlushData(kFlushModeAppend);
    BROKEN_RV_HANDLING_CODE(rv);
  }
  return rv;
}

nsresult EmbedGlobalHistory::RemoveAllPages()
{
  nsresult rv;
  if (mURLList) {
    g_list_foreach (mURLList, (GFunc) history_entry_foreach_to_remove, NULL);
    g_list_free(mURLList);
    mURLList = NULL;
  }
  mDataIsLoaded = PR_FALSE;
  rv = FlushData(kFlushModeFullWrite);
  mEntriesAddedSinceFlush = 0;
  return rv;
}

//*****************************************************************************
// EmbedGlobalHistory::nsIObserver
//*****************************************************************************   
NS_IMETHODIMP EmbedGlobalHistory::Observe(nsISupports *aSubject,
                                          const char *aTopic,
                                          const PRUnichar *aData)
{
  nsresult rv = NS_OK;
  // used when the browser is closed and the EmbedGlobalHistory destructor is not called
  if (strcmp(aTopic, "quit-application") == 0) {
    rv = LoadData();
    // we have to sort the list before flush it
    rv |= FlushData();
    if (mURLList) {
      g_list_foreach(mURLList, (GFunc) history_entry_foreach_to_remove, NULL);
      g_list_free(mURLList);
    }
    if (handle)
      close_file_handle(handle);
  } else if (strcmp(aTopic, "RemoveAllPages") == 0) {
    RemoveAllPages();
  }
  return rv;
}

//*****************************************************************************
// EmbedGlobalHistory
//*****************************************************************************   
// Open/Create the history.dat file if it does not exist
nsresult EmbedGlobalHistory::InitFile()
{
  // Get the history file in our profile dir.
  // Notice we are not just getting NS_APP_HISTORY_50_FILE
  // because it is used by the "real" global history component.
#ifdef MOZ_ENABLE_LIBXUL
  if (EmbedPrivate::sProfileDir) {
    nsCString path;
    EmbedPrivate::sProfileDir->GetNativePath(path);
    mHistoryFile = g_strdup_printf("%s/history.dat", path.get());
    BROKEN_STRING_BUILDER(mHistoryFile);
  } else
#else
  if (EmbedPrivate::sProfileDirS) {
    mHistoryFile = g_strdup_printf("%s/history.dat", EmbedPrivate::sProfileDirS);
    BROKEN_STRING_BUILDER(mHistoryFile);
  } else
#endif
  {
    mHistoryFile = g_strdup_printf("%s/history.dat", g_get_tmp_dir());
  }
  void *uri = file_handle_uri_new(mHistoryFile);
  gboolean rs = FALSE;
  if (!file_handle_uri_exists(uri)) {
    if (file_handle_create_uri(&handle, uri)) {
      g_print("Could not create a history file\n");
      return NS_ERROR_FAILURE;
    }
    close_file_handle(handle);
  }
  rs = file_handle_open_uri(&handle, uri);
  if (rs) {
    //g_print("Could not open history URI. Result: %s\n", gnome_vfs_result_to_string(rs));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// Get the data from history.dat file
nsresult EmbedGlobalHistory::LoadData()
{
  nsresult rv = NS_OK;
  if (!mDataIsLoaded) {            
    mDataIsLoaded = PR_TRUE;
    void *uri = file_handle_uri_new(mHistoryFile);
    if (!file_handle_uri_exists(uri)) {
      rv = InitFile();
      if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    }
    file_handle_seek(handle, FALSE);
    rv |= ReadEntries(handle);
  }
  return rv;
}

// Call a function to write each entry in the history hash table
nsresult EmbedGlobalHistory::WriteEntryIfWritten(GList *list, void *file_handle)
{
  if (!file_handle)
    return NS_ERROR_FAILURE;

  unsigned int counter = g_list_length(list);
  while (counter > 0) {
    HistoryEntry *entry = NS_STATIC_CAST(HistoryEntry*, g_list_nth_data(list, counter-1));
    counter--;
    if (!entry || entryHasExpired(entry)) {
      continue;
    }
    writeEntry(file_handle, entry);
  }
  return NS_OK;
}

// Call a function to write each unwritten entry in the history hash table
nsresult EmbedGlobalHistory::WriteEntryIfUnwritten(GList *list, void *file_handle)
{
  if (!file_handle)
    return NS_ERROR_FAILURE;
  unsigned int counter = g_list_length(list);
  while (counter > 0) {
    HistoryEntry *entry = NS_STATIC_CAST(HistoryEntry*, g_list_nth_data(list, counter-1));
    if (!entry || entryHasExpired(entry)) {
      counter--;
      continue;
    }
    if (!GetIsWritten(entry))
      writeEntry(file_handle, entry);
    counter--;
  }
  return NS_OK;
}

// Write the history in history.dat file
nsresult EmbedGlobalHistory::FlushData(PRIntn mode)
{
  nsresult rv = NS_OK;
  if (mEntriesAddedSinceFlush == 0)
    return NS_OK;
  if (!mHistoryFile)
  {
    rv |= InitFile();
    rv |= FlushData(kFlushModeFullWrite);
    BROKEN_RV_HANDLING_CODE(rv);
    return rv; 
  }
  void *uri = file_handle_uri_new(mHistoryFile);
  if (!file_handle_uri_exists(uri)) {
    rv = InitFile();
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
  }
  else {
    rv |= InitFile();
    rv |= FlushData(kFlushModeFullWrite);

    return rv;
  }
  mEntriesAddedSinceFlush = 0;
  return NS_OK;
}

// Split an entry in last visit time, title and url.
// Add a stored entry in the history.dat file in the history hash table
nsresult EmbedGlobalHistory::GetEntry(char *entry)
{
  char separator = (char) defaultSeparator;
  int pos = 0;
  nsInt64 outValue = 0;
  while (PR_TRUE) {
    PRInt32 digit;
    if (entry[pos] == separator) {
      pos++;
      break;
    }
    if (entry[pos] == '\0' || !isdigit(entry[pos]))
      return NS_ERROR_FAILURE;    
    digit = entry[pos] - '0';              
    outValue *= nsInt64(10);
    outValue += nsInt64(digit);
    pos++;
  }
  char url[1024], title[1024];
  int urlLength= 0, titleLength= 0, numStrings=1;
  // get the url and title
  // FIXME
  while(PR_TRUE) {
    if (entry[pos] == separator) {
      numStrings++;
      pos++;
      continue;
    }
    if (numStrings > 2)
      break;
    if (numStrings==1) {
      url[urlLength++] = entry[pos];
    } else {
      title[titleLength++] = entry[pos];
  }
  pos++;            
  }
  url[urlLength]='\0';
  title[titleLength]='\0';
  HistoryEntry *newEntry = g_new0(HistoryEntry, 1);
  if (!newEntry)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = NS_OK;
  SET_TITLE(newEntry, title);
  rv |= SetLastVisitTime(newEntry, outValue);
  rv |= SetIsWritten(newEntry);
  rv |= SetURL(newEntry, url);
  BROKEN_RV_HANDLING_CODE(rv);
  // Check wheter the entry has expired
  if (!entryHasExpired(newEntry)) {
    mURLList = g_list_prepend(mURLList, newEntry);
  }
  return rv;
}

// Get the history entries from history.dat file
nsresult EmbedGlobalHistory::ReadEntries(void *file_handle)
{
  if (!file_handle)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  guint64 bytes;
  int64 read_bytes;
  char separator = (char) defaultSeparator;
  int pos = 0;
  int numStrings = 0;
  bytes = file_handle_file_info_block_size (file_handle);
  /* Optimal buffer size for reading/writing the file. */
  nsAutoArrayPtr<char> line(new char[bytes]);
  nsAutoArrayPtr<char> buffer(new char[bytes]);
  do {
    read_bytes = file_handle_read(file_handle, (gpointer) buffer, bytes-1);
    if (read_bytes < 0)
      break;
    buffer[read_bytes] = '\0';
    unsigned int buf_pos = 0;
    while (buf_pos< read_bytes) {
      if (buffer[buf_pos]== separator)
        numStrings++;
      if (buffer[buf_pos]!= '\n')
        line[pos] = buffer[buf_pos];
      else
        buf_pos++;
      if (numStrings==3) {
        line[pos+1] = '\0';
        rv = GetEntry(line);
        if (NS_FAILED(rv))
          return NS_ERROR_FAILURE;
        pos = -1;
        numStrings = 0;
      }
      pos++;
      buf_pos++;
    }
  } while (read_bytes != -1);
  return rv; 
}

//*****************************************************************************
// Static Functions
//*****************************************************************************   
// Get last visit time from a string
static nsresult writePRInt64(char time[14], const PRInt64& inValue)
{
  nsInt64 value(inValue);
  if (value == nsInt64(0)) {
    strcpy(time, "0");
    return NS_OK;
  }
  nsCAutoString tempString;
  while (value != nsInt64(0)) {
    PRInt32 ones = PRInt32(value % nsInt64(10));
    value /= nsInt64(10);
    tempString.Insert(char('0' + ones), 0);
  }
  strcpy(time,(char *) tempString.get());
  return NS_OK;
}

// Write an entry in the history.dat file
nsresult writeEntry(void *file_handle, HistoryEntry *entry)
{
  nsresult rv = NS_OK;
  char sep = (char) defaultSeparator;
  char time[14];
  writePRInt64(time, GetLastVisitTime(entry));  
  char *line = g_strdup_printf("%s%c%s%c%s%c\n", time, sep, GetURL(entry), sep, GET_TITLE(entry).get(), sep);
  BROKEN_STRING_BUILDER(line);
  guint64 size = file_handle_write (file_handle, (gpointer)line);
  if (size != strlen(line))
    rv = NS_ERROR_FAILURE;
  rv |= SetIsWritten(entry);
  g_free(line);
  return rv;
}

nsresult EmbedGlobalHistory::GetContentList(GtkMozHistoryItem **GtkHI, int *count)
{
  if (!mURLList) return NS_ERROR_FAILURE;

  unsigned int num_items = 0;
  *GtkHI = g_new0(GtkMozHistoryItem, g_list_length(mURLList));
  GtkMozHistoryItem * item = (GtkMozHistoryItem *)*GtkHI;
  while (num_items < g_list_length(mURLList)) {
    HistoryEntry *entry = NS_STATIC_CAST(HistoryEntry*,
                                         g_list_nth_data(mURLList, num_items));
    // verify if the entry has expired and discard it
    if (entryHasExpired(entry)) {
      break;
    }
    glong accessed;
    PRInt64 temp, outValue;
    LL_MUL(outValue, GetLastVisitTime(entry), kOneThousand);
    LL_DIV(temp, outValue, PR_USEC_PER_SEC);
    LL_L2I(accessed, temp);
    // Set the EAL history list
    item[num_items].title = g_strdup(GET_TITLE(entry).get());
    BROKEN_STRING_BUILDER(item[num_items].title);
    item[num_items].url = GetURL(entry);
    item[num_items].accessed = accessed;
    num_items++;
  }
  *count = num_items;
  return NS_OK;
}

