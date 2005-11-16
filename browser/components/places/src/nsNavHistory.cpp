//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla History System
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
 *   Joe Hewitt <hewitt@netscape.com> (autocomplete)
 *   Blake Ross <blaker@netscape.com> (autocomplete)
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

/*
Existing columns:
  URL
  Visit count
  Name (title)
  Hostname (reversed)
  Hidden (bool)
  Typed (bool)

History entry
  Unique ID
  URL
  Title
  Hostname
  Hidden
  Visit count
  Typed (true if ever typed)

  Maybe: Last visit date
  Maybe: First visit date

Transitions
  From ID
  To ID
  Date
  Transition type (typed, bookmark, new window, new tab)

Extra:
  Site icon
  Thumbnail
*/

#include <stdio.h>
#include "nsNavHistory.h"

#include "nsArray.h"
#include "nsDebug.h"
#include "nsIComponentManager.h"
#include "nsCollationCID.h"
#include "nsISimpleEnumerator.h"
#include "nsIServiceManager.h"
#include "nsIDOMElement.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsString.h"
//#include "mozIAnnotationService.h"
#include "nsILocalFile.h"
#include "nsITreeColumns.h"
#include "nsISupportsPrimitives.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsNetUtil.h"
#include "prtime.h"
#include "prprf.h"
#include "nsPromiseFlatString.h"
#include "nsIPrefBranch2.h"
#include "nsArrayEnumerator.h"
#include "nsUnicharUtils.h"
#include "nsEnumeratorUtils.h"
#include "nsPrintfCString.h"

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"

// mork
#include "mdb.h"
#include "nsMorkCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIMdbFactoryFactory.h"

#define IN_MEMORY_LINKS

// preference ID strings
#define PREF_BRANCH_BASE                        "browser."
#define PREF_BROWSER_HISTORY_EXPIRE_DAYS        "history_expire_days"
#define PREF_AUTOCOMPLETE_ONLY_TYPED            "urlbar.matchOnlyTyped"
#define PREF_AUTOCOMPLETE_MAX_COUNT             "urlbar.autocomplete.maxCount"
#define PREF_AUTOCOMPLETE_ENABLED               "urlbar.autocomplete.enabled"
#define PREF_LAST_PAGE_VISITED                  "last_url"

// the value of mLastNow expires every 3 seconds
#define HISTORY_EXPIRE_NOW_TIMEOUT (3 * PR_MSEC_PER_SEC)

#define NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID \
  "@mozilla.org/autocomplete/simple-result;1"

NS_IMPL_ISUPPORTS6(nsNavHistory,
                   nsINavHistory,
                   nsIGlobalHistory2,
                   nsIBrowserHistory,
                   nsIObserver,
                   nsISupportsWeakReference,
                   nsIAutoCompleteSearch)

static PRInt32 ComputeAutoCompletePriority(const nsAString& aUrl,
                                           PRInt32 aVisitCount,
                                           PRBool aWasTyped);
static nsresult GetReversedHostname(nsIURI* aURI, nsAString& host);
static void GetReversedHostname(const nsString& aForward, nsAString& aReversed);
static void GetSubstringFromNthDot(const nsString& aInput, PRInt32 aStartingSpot,
                                   PRInt32 aN, PRBool aIncludeDot,
                                   nsAString& aSubstr);
static PRInt32 GetTLDCharCount(const nsString& aHost);
static PRInt32 GetTLDType(const nsString& aHostTail);
static void GetUnreversedHostname(const nsString& aBackward,
                                  nsAString& aForward);
static PRBool IsNumericHostName(const nsString& aHost);
static void ParseSearchQuery(const nsString& aQuery, nsStringArray* aTerms);

inline void ReverseString(const nsString& aInput, nsAString& aReversed)
{
  aReversed.Truncate(0);
  for (PRInt32 i = aInput.Length() - 1; i >= 0; i --)
    aReversed.Append(aInput[i]);
}
inline void parameterString(PRInt32 paramIndex, nsACString& aParamString)
{
  aParamString = nsPrintfCString("?%d", paramIndex + 1);
}

// emulate string comparison (used for sorting) for PRTime and int
inline PRInt32 ComparePRTime(PRTime a, PRTime b)
{
  if (LL_CMP(a, <, b))
    return -1;
  else if (LL_CMP(a, >, b))
    return 1;
  return 0;
}
inline PRInt32 CompareIntegers(PRUint32 a, PRUint32 b)
{
  return a - b;
}


// UpdateBatchScoper
//
//    This just sets begin/end of batch updates to correspond to C++ scopes so
//    we can be sure end always gets called.

class UpdateBatchScoper
{
public:
  UpdateBatchScoper(nsNavHistory& aNavHistory) : mNavHistory(aNavHistory)
  {
    mNavHistory.BeginUpdateBatch();
  }
  ~UpdateBatchScoper()
  {
    mNavHistory.EndUpdateBatch();
  }
protected:
  nsNavHistory& mNavHistory;
};

const PRInt32 nsNavHistory::kGetInfoIndex_PageID = 0;
const PRInt32 nsNavHistory::kGetInfoIndex_URL = 1;
const PRInt32 nsNavHistory::kGetInfoIndex_Title = 2;
const PRInt32 nsNavHistory::kGetInfoIndex_VisitCount = 3;
const PRInt32 nsNavHistory::kGetInfoIndex_VisitDate = 4;
const PRInt32 nsNavHistory::kGetInfoIndex_RevHost = 5;

const PRInt32 nsNavHistory::kAutoCompleteIndex_URL = 0;
const PRInt32 nsNavHistory::kAutoCompleteIndex_Title = 1;
const PRInt32 nsNavHistory::kAutoCompleteIndex_VisitCount = 2;
const PRInt32 nsNavHistory::kAutoCompleteIndex_Typed = 3;

static nsDataHashtable<nsStringHashKey, int>* gTldTypes;
static const char* gQuitApplicationMessage = "quit-application";

nsNavHistory* nsNavHistory::gHistoryService;


// nsNavHistory::nsNavHistory

nsNavHistory::nsNavHistory() : mNowValid(PR_FALSE),
                               mExpireNowTimer(nsnull),
                               mBatchesInProgress(0)
{
  NS_ASSERTION(! gHistoryService, "YOU ARE CREATING 2 COPIES OF THE HISTORY SERVICE. Everything will break.");
  gHistoryService = this;
}


// nsNavHistory::~nsNavHistory

nsNavHistory::~nsNavHistory()
{
  gObserverService->RemoveObserver(this, gQuitApplicationMessage);

  // remove the static reference to the service. Check to make sure its us
  // in case somebody creates an extra instance of the service.
  NS_ASSERTION(gHistoryService == this, "YOU CREATED 2 COPIES OF THE HISTORY SERVICE.");
  gHistoryService = nsnull;
}


// nsNavHistory::Init

nsresult
nsNavHistory::Init()
{
  nsresult rv;

  gExpandedItems.Init(128);

  InitDB();
  //ImportFromMork();

  // commonly used prefixes that should be chopped off all history and input
  // urls before comparison
  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("http://"));
  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("https://"));
  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("ftp://"));
  mIgnoreHostnames.AppendString(NS_LITERAL_STRING("www."));
  mIgnoreHostnames.AppendString(NS_LITERAL_STRING("ftp."));

  // prefs
  if (!gPrefService || !gPrefBranch) {
    gPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = gPrefService->GetBranch(PREF_BRANCH_BASE, getter_AddRefs(gPrefBranch));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // string bundle for localization
  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bundleService->CreateBundle(
      "chrome://global/locale/history/history.properties",
      getter_AddRefs(mBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // annotation service - just ignore errors
  //mAnnotationService = do_GetService("@mozilla.org/annotation-service;1", &rv);

  // prefs
  LoadPrefs();

  // The AddObserver calls must be the last lines in this function, because
  // this function may fail, and thus, this object would be not completely
  // initialized), but the observerservice would still keep a reference to us
  // and notify us about shutdown, which may cause crashes.

  gObserverService = do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch2> pbi = do_QueryInterface(gPrefBranch);
  if (pbi) {
    pbi->AddObserver(PREF_AUTOCOMPLETE_ONLY_TYPED, this, PR_FALSE);
    pbi->AddObserver(PREF_BROWSER_HISTORY_EXPIRE_DAYS, this, PR_FALSE);
    pbi->AddObserver(PREF_AUTOCOMPLETE_MAX_COUNT, this, PR_FALSE);
  }

  gObserverService->AddObserver(this, gQuitApplicationMessage, PR_FALSE);

  return rv;
}


// nsNavHistory::InitDB

nsresult
nsNavHistory::InitDB()
{
  nsresult rv;

  // init DB
  mDBService = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBService->OpenSpecialDatabase("profile", getter_AddRefs(mDBConn));
  NS_ENSURE_SUCCESS(rv, rv);

// REMOVE ME FIXME TODO XXX
  {
    mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA cache_size=10000;"));
  }

  // history table

  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE moz_history (id INTEGER PRIMARY KEY, url LONGVARCHAR, title LONGVARCHAR, rev_host LONGVARCHAR, visit_count INTEGER DEFAULT 0, hidden INTEGER DEFAULT 0 NOT NULL, typed INTEGER DEFAULT 0 NOT NULL)"));
  //NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_history_urlindex ON moz_history (url)"));
  //NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_history_hostindex ON moz_history (rev_host)"));
  //NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_history_visitcount ON moz_history (visit_count)"));
  //NS_ENSURE_SUCCESS(rv, rv);

  // history visit table

  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE moz_historyvisit (visit_id INTEGER PRIMARY KEY, from_visit INTEGER, page_id INTEGER, visit_date INTEGER, visit_type INTEGER, session INTEGER)"));
  //NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_historyvisit_fromindex ON moz_historyvisit (from_visit)"));
  //NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_historyvisit_pageindex ON moz_historyvisit (page_id)"));
  //NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_historyvisit_dateindex ON moz_historyvisit (visit_date)"));
  //NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_historyvisit_sessionindex ON moz_historyvisit (session)"));
  //NS_ENSURE_SUCCESS(rv, rv);

  // functions (must happen after table creation)

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT h.id, h.url, h.title, h.visit_count, v.visit_date FROM moz_historyvisit v, moz_history h WHERE v.visit_id = ?1 AND h.id = v.page_id"),
                               getter_AddRefs(mDBGetVisitPageInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT h.id, h.url, h.title, h.visit_count FROM moz_history h WHERE h.url = ?1"),
                               getter_AddRefs(mDBGetURLPageInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT h.url, h.title, h.visit_count, h.typed FROM moz_history h JOIN moz_historyvisit v ON h.id = v.page_id WHERE h.hidden <> 1 GROUP BY h.id ORDER BY h.visit_count LIMIT ?1"),
                                getter_AddRefs(mDBFullAutoComplete));
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}


// nsNavHistory::InitMemDB
//
//    Should be called after InitDB

nsresult
nsNavHistory::InitMemDB()
{
  printf("Initializing history in-memory DB\n");
  nsresult rv = mDBService->OpenSpecialDatabase("memory", getter_AddRefs(mMemDBConn));
  NS_ENSURE_SUCCESS(rv, rv);

  // create our table and index
  rv = mMemDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE moz_memhistory (url LONGVARCHAR UNIQUE)"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMemDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_memhistory_index ON moz_memhistory (url)"));
  NS_ENSURE_SUCCESS(rv, rv);

  // prepackaged statements
  rv = mMemDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT url FROM moz_memhistory WHERE url = ?1"),
      getter_AddRefs(mMemDBGetPage));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMemDBConn->CreateStatement(
      NS_LITERAL_CSTRING("INSERT OR IGNORE INTO moz_memhistory VALUES (?1)"),
      getter_AddRefs(mMemDBAddPage));
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the URLs over: sort by URL because the original table already has
  // and index. We can therefor not spend time sorting the whole thing another
  // time by inserting in order.
  nsCOMPtr<mozIStorageStatement> selectStatement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT url FROM moz_history WHERE visit_count > 0 ORDER BY url"),
                                getter_AddRefs(selectStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<mozIStorageValueArray> selectRow = do_QueryInterface(selectStatement);
  if (!selectRow)
    return NS_ERROR_FAILURE;

  PRBool hasMore = PR_FALSE;
  //rv = mMemDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("BEGIN TRANSACTION"));
  mozStorageTransaction transaction(mMemDBConn, PR_FALSE);
  nsCString url;
  while(NS_SUCCEEDED(rv = selectStatement->ExecuteStep(&hasMore)) && hasMore) {
    rv = selectRow->GetUTF8String(0, url);
    if (NS_SUCCEEDED(rv) && ! url.IsEmpty()) {
      rv = mMemDBAddPage->BindUTF8StringParameter(0, url);
      if (NS_SUCCEEDED(rv))
        mMemDBAddPage->Execute();
    }
  }
  transaction.Commit();

  printf("DONE initializing history in-memory DB\n");
  return NS_OK;
}


// nsNavHistory::SaveExpandItem
//
//    This adds an item to the persistent list of items that should be
//    expanded.

void
nsNavHistory::SaveExpandItem(const nsAString& aTitle)
{
  gExpandedItems.Put(aTitle, 1);
}


// nsNavHistory::GetUrlIdFor
//
//    Called by the bookmarks and annotation services, this function returns the
//    ID of the row for the given URL, optionally creating one if it doesn't
//    exist. A newly created entry will have no visits.
//
//    If aAutoCreate is false and the item doesn't exist, the entry ID will be
//    zero.
//
//    This DOES NOT check for bad URLs other than that they're nonempty.

nsresult
nsNavHistory::GetUrlIdFor(nsIURI* aURI, PRInt64* aEntryID,
                          PRBool aAutoCreate)
{
  nsresult rv;
  *aEntryID = 0;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  mozStorageStatementScoper statementResetter(mDBGetURLPageInfo);
  rv = BindStatementURI(mDBGetURLPageInfo, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasEntry = PR_FALSE;
  rv = mDBGetURLPageInfo->ExecuteStep(&hasEntry);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasEntry) {
    return mDBGetURLPageInfo->GetInt64(kGetInfoIndex_PageID, aEntryID);
  } else if (aAutoCreate) {
    mDBGetURLPageInfo->Reset();
    statementResetter.Abandon();
    rv = InternalAddNewPage(aURI, nsnull, PR_TRUE, PR_FALSE, 0, aEntryID);
    if (NS_SUCCEEDED(rv))
      transaction.Commit();
    return rv;
  } else {
    // Doesn't exist: don't do anything, entry ID was already set to 0 above
    return NS_OK;
  }
}


// nsNavHistory::SaveCollapseItem
//
//    For now, just remove things that should be collapsed. Maybe in the
//    future we'll want to save things that have been explicitly collapsed
//    versus things that were just never expanded in the first place. This could
//    be done by doing a put with a value of 0.

void
nsNavHistory::SaveCollapseItem(const nsAString& aTitle)
{
  gExpandedItems.Remove(aTitle);
}


// nsNavHistory::InternalAdd
//
//    Adds or updates a page with the given URI. The ID of the new/updated
//    page will be put into aPageID (can be NULL).
//
//    THE RETURNED NEW PAGE ID MAY BE 0 indicating that this page should not be
//    added to the history.

nsresult
nsNavHistory::InternalAdd(nsIURI* aURI, PRUint32 aSessionID,
                          PRUint32 aTransitionType, const PRUnichar* aTitle,
                          PRTime aVisitDate, PRBool aRedirect,
                          PRBool aToplevel, PRInt64* aPageID)
{
  nsresult rv;

  // Filter out unwanted URIs, silently failing
  PRBool canAdd = PR_FALSE;
  rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    if (aPageID)
      *aPageID = 0;
    return NS_OK;
  }

  // in-memory version
#ifdef IN_MEMORY_LINKS
  if (!mMemDBConn)
    InitMemDB();
  rv = BindStatementURI(mMemDBAddPage, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  mMemDBAddPage->Execute();
#endif

  // this will prevent corruption since we have to do a two-phase add
  mozStorageTransaction transaction(mDBConn, PR_FALSE,
                                  mozIStorageConnection::TRANSACTION_EXCLUSIVE);

  // see if this is an update (revisit) or a new page
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT id,visit_count,typed,hidden FROM moz_history WHERE url = ?1"),
      getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = BindStatementURI(dbSelectStatement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool alreadyVisited = PR_TRUE;
  rv = dbSelectStatement->ExecuteStep(&alreadyVisited);

  PRInt64 pageID = 0;

  if (alreadyVisited) {
    // Update the existing entry...

    rv = dbSelectStatement->GetInt64(0, &pageID);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 oldVisitCount = 0;
    rv = dbSelectStatement->GetInt32(1, &oldVisitCount);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 oldTypedState = 0;
    rv = dbSelectStatement->GetInt32(2, &oldTypedState);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 oldHiddenState = 0;
    rv = dbSelectStatement->GetInt32(3, &oldHiddenState);
    NS_ENSURE_SUCCESS(rv, rv);

    // must free the previous statement before we can make a new one
    dbSelectStatement = nsnull;

    // update visit count and last visit date for the URL
    // (only update the title if we have one, usually we don't)
    nsCOMPtr<mozIStorageStatement> dbUpdateStatement;
    if (aTitle) {
      // UNTESTED CODE PATH
      rv = mDBConn->CreateStatement(
          NS_LITERAL_CSTRING("UPDATE moz_history SET visit_count = ?1, hidden = ?2, title = ?3 WHERE id = ?4"),
          getter_AddRefs(dbUpdateStatement));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = dbUpdateStatement->BindInt64Parameter(2, aVisitDate);
    } else {
      rv = mDBConn->CreateStatement(
          NS_LITERAL_CSTRING("UPDATE moz_history SET visit_count = ?1, hidden = ?2 WHERE id = ?4"),
          getter_AddRefs(dbUpdateStatement));
    }
    NS_ENSURE_SUCCESS(rv, rv);
    rv = dbUpdateStatement->BindInt32Parameter(0, oldVisitCount + 1);
    NS_ENSURE_SUCCESS(rv, rv);

    // ...if the column was marked as typed, we should unhide it. If you type
    // a new page, it gets added to the DB but marked as hidden typed. When
    // the page is actually loaded, we get here, and need to unhide it since
    // we know it's valid.
    if (oldTypedState)
      dbUpdateStatement->BindInt32Parameter(1, 0); // hidden = 0
    else
      dbUpdateStatement->BindInt32Parameter(1, oldHiddenState);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbUpdateStatement->BindInt64Parameter(3, pageID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = dbUpdateStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // New page

    // must free the previous statement before we can make a new one
    dbSelectStatement = nsnull;

    // If this is a JS url, or a redirected URI or in a frame, hide it in
    // global history so that it doesn't show up in the autocomplete dropdown.
    // Adding an existing page above unhides pages that were typed regardless
    // of Javascript, etc. disposition. Typed URIs are always existing because
    // the "typed" notification comes *before* the AddURI, which is called only
    // if the page actually is loadeing. See bug 197127 and bug 161531 for
    // detailst.
    PRBool isJavascript;
    rv = aURI->SchemeIs("javascript", &isJavascript);
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool hidden = PR_FALSE;
    if (isJavascript || aRedirect || !aToplevel)
      hidden = PR_TRUE;

    // set as not typed, visited once
    rv = InternalAddNewPage(aURI, aTitle, hidden, PR_FALSE, 1, &pageID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = AddVisit(0, pageID, aVisitDate, aTransitionType, aSessionID);

  if (aPageID)
    *aPageID = pageID;

  // Set the most recently visited page, which is necessary when you have
  // your "new window" page set to the most recent.
  if (aToplevel) {
    nsCAutoString utf8URISpec;
    rv = aURI->GetSpec(utf8URISpec);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = gPrefBranch->SetCharPref(PREF_LAST_PAGE_VISITED,
                                  PromiseFlatCString(utf8URISpec).get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Notify observers. Note that we finish the transaction before doing this in
  // case they need to use the DB
  transaction.Commit();
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnAddURI(aURI, aVisitDate);

  return NS_OK;
}


// nsNavHistory::InternalAddNewPage
//
//    Adds a new page to the DB. THIS SHOULD BE THE ONLY PLACE NEW ROWS ARE
//    CREATED. This allows us to maintain better consistency.
//
//    If non-null, the new page ID will be placed into aPageID.

nsresult
nsNavHistory::InternalAddNewPage(nsIURI* aURI, const PRUnichar* aTitle,
                                 PRBool aHidden, PRBool aTyped,
                                 PRInt32 aVisitCount, PRInt64* aPageID)
{
  nsCOMPtr<mozIStorageStatement> dbInsertStatement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("INSERT INTO moz_history (url, title, rev_host, hidden, typed, visit_count) VALUES ( ?1, ?2, ?3, ?4, ?5, ?6 )"),
      getter_AddRefs(dbInsertStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // URL
  rv = BindStatementURI(dbInsertStatement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // title: use NULL if not given to distinguish it from empty titles
  if (aTitle) {
    nsAutoString title(aTitle);
    rv = dbInsertStatement->BindStringParameter(1, title);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = dbInsertStatement->BindNullParameter(1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // host (reversed with trailing period)
  nsAutoString revHost;
  rv = GetReversedHostname(aURI, revHost);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbInsertStatement->BindStringParameter(2, revHost);
  NS_ENSURE_SUCCESS(rv, rv);

  // hidden
  rv = dbInsertStatement->BindInt32Parameter(3, aHidden);
  NS_ENSURE_SUCCESS(rv, rv);

  // typed
  rv = dbInsertStatement->BindInt32Parameter(4, aTyped);
  NS_ENSURE_SUCCESS(rv, rv);

  // visit count
  rv = dbInsertStatement->BindInt32Parameter(5, aVisitCount);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbInsertStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // If the caller wants the page ID, go get it
  if (aPageID) {
    rv = mDBConn->GetLastInsertRowID(aPageID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavHistory::AddVisit
//
//    Just a wrapper for inserting a new visit in the DB.

nsresult nsNavHistory::AddVisit(PRInt64 aFromStep, PRInt64 aPageID,
                                PRTime aTime, PRInt32 aTransitionType,
                                PRInt64 aSessionID)
{
  nsCOMPtr<mozIStorageStatement> dbInsertStatement;
  nsresult rv = mDBConn->CreateStatement(
    NS_LITERAL_CSTRING("INSERT INTO moz_historyvisit (from_visit, page_id, visit_date, visit_type, session) VALUES ( ?1, ?2, ?3, ?4, ?5 )"),
    getter_AddRefs(dbInsertStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbInsertStatement->BindInt64Parameter(0, aFromStep);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbInsertStatement->BindInt64Parameter(1, aPageID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbInsertStatement->BindInt64Parameter(2, aTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbInsertStatement->BindInt32Parameter(3, aTransitionType);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbInsertStatement->BindInt32Parameter(4, aSessionID);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbInsertStatement->Execute(); // should reset the statement
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavHistory::IsURIStringVisited
//
//    Takes a URL as a string and returns true if we've visited it.
//
//    Be careful to always reset the statement since it will be reused.

PRBool nsNavHistory::IsURIStringVisited(const nsACString& aURIString)
{
#ifdef IN_MEMORY_LINKS
  // check the memory DB
  if (!mMemDBConn)
    InitMemDB();
  nsresult rv = mMemDBGetPage->BindUTF8StringParameter(0, aURIString);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  PRBool hasPage = PR_FALSE;
  mMemDBGetPage->ExecuteStep(&hasPage);
  mMemDBGetPage->Reset();
  return hasPage;
#else
  // check the main DB
  nsresult rv;
  mozStorageStatementScoper statementResetter(mDBGetURLPageInfo);
  rv = mDBGetURLPageInfo->BindUTF8StringParameter(0, aURIString);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  PRBool hasMore = PR_FALSE;
  rv = mDBGetURLPageInfo->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  // Actually get the result to make sure the visit count > 0.  there are
  // several ways that we can get pages with visit counts of 0, and those
  // should not count.
  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(
      mDBGetURLPageInfo, &rv);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  PRInt32 visitCount;
  rv = row->GetInt32(kGetInfoIndex_VisitCount, &visitCount);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  return visitCount > 0;
#endif
}


// nsNavHistory::VacuumDB
//
//    Deletes everything from the database that is older than the expiration
//    time. It is called on app shutdown.
//
//    We DO NOT notify observers that the items are being deleted. I can't think
//    of any reason why anybody would need to know (we are shutting down, after
//    all), and if there is an observer still attached, it could significantly
//    prolong shutdown.
//
//    The statement:
//      DELETE FROM a WHERE a.id in (SELECT id FROM a LEFT OUTER JOIN b ON a.id = b.bar WHERE b.bid IS NULL);
//    matches the visits and history entries. An outer join means that we
//    generate a row for every item in the left-hand table (in this case, the
//    history table), even when there is no corresponding right-hand table.
//    Then we just pick those items where there was no corresponding right-hand
//    one (i.e., a moz_history entry with no visits), and delete it.

nsresult
nsNavHistory::VacuumDB()
{
  // go ahead and commit on error, only some things will get deleted, which,
  // if you are trying to clear your history, is probably better than nothing.
  mozStorageTransaction transaction(mDBConn, PR_TRUE);

  // find the threshold for deleting old visits
  PRTime now = GetNow();
  PRInt64 secsPerDay;     LL_I2L(secsPerDay, 24*60*60);
  PRInt64 usecsPerSec;    LL_I2L(usecsPerSec, 1000000);
  PRInt64 usecsPerDay;    LL_MUL(usecsPerDay, secsPerDay, usecsPerSec);
  PRInt64 expireDaysAgo;  LL_I2L(expireDaysAgo, mExpireDays);
  PRInt64 expireUsecsAgo; LL_MUL(expireUsecsAgo, expireDaysAgo, usecsPerDay);
  PRInt64 expirationTime; LL_SUB(expirationTime, now, expireUsecsAgo);

  // delete expired visits
  nsCOMPtr<mozIStorageStatement> visitDelete;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisit WHERE visit_date < ?1"),
      getter_AddRefs(visitDelete));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = visitDelete->BindInt64Parameter(0, expirationTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = visitDelete->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // FIXME: delete annotations for expiring pages

  // FIXME: delete favicons when no pages reference them

  // delete history entries with no visits
  nsCOMPtr<mozIStorageStatement> historyDelete;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_history WHERE id IN (SELECT id FROM moz_history h LEFT OUTER JOIN moz_historyvisit v ON h.id = v.page_id WHERE v.visit_id IS NULL)"),
      getter_AddRefs(historyDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  // compress the tables
  /* This is commented out because it is extremely-weemely slow. I don't think
   * it's very critical. Maybe we should just do it every X times the app
   * exits. I'm unsure if the freed up space is always lost, or whether it will
   * be used when new stuff is added.
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM moz_history"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM moz_historyvisit"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM moz_anno"));
  NS_ENSURE_SUCCESS(rv, rv);
  */

  return NS_OK;
}


// nsNavHistory::LoadPrefs

nsresult
nsNavHistory::LoadPrefs()
{
  gPrefBranch->GetIntPref(PREF_BROWSER_HISTORY_EXPIRE_DAYS, &mExpireDays);
  gPrefBranch->GetBoolPref(PREF_AUTOCOMPLETE_ONLY_TYPED,
                           &mAutoCompleteOnlyTyped);
  if (NS_FAILED(gPrefBranch->GetIntPref(PREF_AUTOCOMPLETE_MAX_COUNT, &mAutoCompleteMaxCount))) {
    mAutoCompleteMaxCount = 2000; // FIXME: add this to default prefs.js
  }
  return NS_OK;
}


// nsNavHistory::GetNow
//
//    This is a hack to avoid calling PR_Now() too often, as is the case when
//    we're asked the ageindays of many history entries in a row. A timer is
//    set which will clear our valid flag after a short timeout.

PRTime
nsNavHistory::GetNow()
{
  if (!mNowValid) {
    mLastNow = PR_Now();
    mNowValid = PR_TRUE;
    if (!mExpireNowTimer)
      mExpireNowTimer = do_CreateInstance("@mozilla.org/timer;1");

    if (mExpireNowTimer)
      mExpireNowTimer->InitWithFuncCallback(expireNowTimerCallback, this,
                                            HISTORY_EXPIRE_NOW_TIMEOUT,
                                            nsITimer::TYPE_ONE_SHOT);
  }

  return mLastNow;
}


// nsNavHistory::expireNowTimerCallback

void nsNavHistory::expireNowTimerCallback(nsITimer* aTimer,
                                                 void* aClosure)
{
  nsNavHistory* history = NS_STATIC_CAST(nsNavHistory*, aClosure);
  history->mNowValid = PR_FALSE;
  history->mExpireNowTimer = nsnull;
}


// Nav history *****************************************************************


// nsNavHistory::GetHasHistoryEntries

NS_IMETHODIMP
nsNavHistory::GetHasHistoryEntries(PRBool* aHasEntries)
{
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT url FROM moz_history LIMIT 1"),
      getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  return dbSelectStatement->ExecuteStep(aHasEntries);
}


// nsNavHistory::AddPageToSession

/*
nsresult
nsNavHistory::AddPageToSession(nsIURI* aURI)
{
  // FIXME
  return NS_OK;
}
*/


// nsNavHistory::CanAddURI
//
//    Filter out unwanted URIs such as "chrome:", "mailbox:", etc.
//
//    The model is if we don't know differently then add which basically means
//    we are suppose to try all the things we know not to allow in and then if
//    we don't bail go on and allow it in.

nsresult
nsNavHistory::CanAddURI(nsIURI* aURI, PRBool* canAdd)
{
  nsresult rv;

  nsCString scheme;
  rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // first check the most common cases (HTTP, HTTPS) to allow in to avoid most
  // of the work
  if (scheme.EqualsLiteral("http")) {
    *canAdd = PR_TRUE;
    return NS_OK;
  }
  if (scheme.EqualsLiteral("https")) {
    *canAdd = PR_TRUE;
    return NS_OK;
  }

  // now check for all bad things
  if (scheme.EqualsLiteral("about") ||
      scheme.EqualsLiteral("imap") ||
      scheme.EqualsLiteral("news") ||
      scheme.EqualsLiteral("mailbox") ||
      scheme.EqualsLiteral("view-source") ||
      scheme.EqualsLiteral("chrome") ||
      scheme.EqualsLiteral("data")) {
    *canAdd = PR_FALSE;
    return NS_OK;
  }
  *canAdd = PR_TRUE;
  return NS_OK;
}


// nsNavHistory::GetNewQuery

NS_IMETHODIMP nsNavHistory::GetNewQuery(nsINavHistoryQuery **_retval)
{
  *_retval = new nsNavHistoryQuery();
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;
  (*_retval)->AddRef();
  return NS_OK;
}


// nsNavHistory::ExecuteQuery
//

NS_IMETHODIMP
nsNavHistory::ExecuteQuery(nsINavHistoryQuery *aQuery,
                           const PRInt32 *aGroupingMode, PRUint32 aGroupCount,
                           PRInt32 aSortingMode, PRBool aAsVisits,
                           nsINavHistoryResult** _retval)
{
  return ExecuteQueries(NS_CONST_CAST(const nsINavHistoryQuery**, &aQuery), 1,
                        aGroupingMode, aGroupCount,
                        aSortingMode, aAsVisits, _retval);
}


// nsNavHistory::ExecuteQueries
//
//    FIXME: This only does keyword searching for the first query, and does
//    it ANDed with the all the rest of the queries.

NS_IMETHODIMP
nsNavHistory::ExecuteQueries(const nsINavHistoryQuery** aQueries,
                             PRUint32 aQueryCount,
                             const PRInt32 *aGroupingMode, PRUint32 aGroupCount,
                             PRInt32 aSortingMode, PRBool aAsVisits,
                             nsINavHistoryResult** _retval)
{
  nsresult rv;
  if (aSortingMode < 0 || aSortingMode > SORT_BY_VISITCOUNT_DESCENDING)
    return NS_ERROR_INVALID_ARG;

  // conditions we want on all history queries, this just selects history
  // entries that are "active"
  //const nsCString commonConditions("h.visit_count > 0 AND h.hidden <> 1");
  NS_NAMED_LITERAL_CSTRING(commonConditions,
                           "h.visit_count > 0 AND h.hidden <> 1");

  // Query string: Output parameters should be in order of kGetInfoIndex_*
  nsCAutoString queryString;
  nsCAutoString groupBy;
  if (aAsVisits) {
    // if we want visits, this is easy, just combine all possible matches
    // between the history and visits table and do our query.
    queryString = NS_LITERAL_CSTRING("SELECT h.id, h.url, h.title, h.visit_count, v.visit_date, h.rev_host"
      " FROM moz_history h JOIN moz_historyvisit v ON h.id = v.page_id WHERE ");
  } else {
    // For URLs, it is more complicated. Because we want each URL once. The
    // GROUP BY clause gives us this. However, we also want the most recent
    // visit time, so we add ANOTHER join with the visit table (this one does
    // not have any restrictions on if from the query) and do MAX() to get the
    // max visit time.
    queryString = NS_LITERAL_CSTRING("SELECT h.id, h.url, h.title, h.visit_count, MAX(fullv.visit_date), h.rev_host"
      " FROM moz_history h JOIN moz_historyvisit v ON h.id = v.page_id JOIN moz_historyvisit fullv ON h.id = fullv.page_id WHERE ");
    groupBy = NS_LITERAL_CSTRING(" GROUP BY h.id");
  }

  PRInt32 numParameters = 0;
  nsCAutoString conditions;
  for (PRUint32 i = 0; i < aQueryCount; i ++) {
    nsCString queryClause;
    PRInt32 clauseParameters = 0;
    rv = QueryToSelectClause(NS_CONST_CAST(nsINavHistoryQuery*, aQueries[i]),
                             numParameters, &queryClause, &clauseParameters);
    NS_ENSURE_SUCCESS(rv, rv);
    if (! queryClause.IsEmpty()) {
      if (! conditions.IsEmpty()) // exists previous clause: multiple ones are ORed
        conditions += NS_LITERAL_CSTRING(" OR ");
      conditions += NS_LITERAL_CSTRING("(") + queryClause +
        NS_LITERAL_CSTRING(")");
      numParameters += clauseParameters;
    }
  }
  queryString += commonConditions;
  if (! conditions.IsEmpty()) {
    queryString += NS_LITERAL_CSTRING(" AND (") + conditions +
                   NS_LITERAL_CSTRING(")");
  }
  queryString += groupBy;

  // Sort clause: we will sort later, but if it comes out of the DB sorted,
  // our later sort will be basically free. The DB can sort these for free
  // most of the time anyway, because it has indices over these items.
  switch(aSortingMode) {
    case SORT_BY_NONE:
      break;
    case SORT_BY_TITLE_ASCENDING:
    case SORT_BY_TITLE_DESCENDING:
      // the DB doesn't have indices on titles, and we need to do special
      // sorting for locales. This type of sorting is done only at the end.
      break;
    case SORT_BY_DATE_ASCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY v.visit_date ASC");
      break;
    case SORT_BY_DATE_DESCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY v.visit_date DESC");
      break;
    case SORT_BY_URL_ASCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY h.url ASC");
      break;
    case SORT_BY_URL_DESCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY h.url DESC");
      break;
    case SORT_BY_VISITCOUNT_ASCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY h.visit_count ASC");
      break;
    case SORT_BY_VISITCOUNT_DESCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY h.visit_count DESC");
      break;
    default:
      NS_NOTREACHED("Invalid sorting mode");
  }
  //printf("Constructed the query: %s\n", PromiseFlatCString(queryString).get());

  // Put this in a transaction. Even though we are only reading, this will
  // speed up the grouped queries to the annotation service for titles and
  // full text searching.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // create statement
  nsCOMPtr<mozIStorageStatement> statement;
  rv = mDBConn->CreateStatement(queryString, getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  // bind parameters
  numParameters = 0;
  for (PRUint32 i = 0; i < aQueryCount; i ++) {
    PRInt32 clauseParameters = 0;
    rv = BindQueryClauseParameters(statement, numParameters,
                                   NS_CONST_CAST(nsINavHistoryQuery*, aQueries[i]),
                                   &clauseParameters);
    NS_ENSURE_SUCCESS(rv, rv);
    numParameters += clauseParameters;
  }

  // run query and put into result object
  nsRefPtr<nsNavHistoryResult> result(new nsNavHistoryResult(this, mBundle));
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;
  rv = result->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasSearchTerms;
  rv = NS_CONST_CAST(nsINavHistoryQuery*,
                     aQueries[0])->GetHasSearchTerms(&hasSearchTerms);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aGroupCount == 0 && ! hasSearchTerms) {
    // optimize the case where we just want a list with no grouping: this
    // directly fills in the results and we avoid a copy of the whole list
    rv = ResultsAsList(statement, aAsVisits, result->GetTopLevel());
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // generate the toplevel results
    nsCOMArray<nsNavHistoryResultNode> toplevel;
    rv = ResultsAsList(statement, aAsVisits, &toplevel);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasSearchTerms) {
      // keyword search
      nsAutoString searchTerms;
      NS_CONST_CAST(nsINavHistoryQuery*, aQueries[0])
        ->GetSearchTerms(searchTerms);
      if (aGroupCount == 0) {
        FilterResultSet(toplevel, result->GetTopLevel(), searchTerms);
      } else {
        nsCOMArray<nsNavHistoryResultNode> filteredResults;
        FilterResultSet(toplevel, &filteredResults, searchTerms);
        rv = RecursiveGroup(filteredResults, aGroupingMode, aGroupCount,
                            result->GetTopLevel());
        NS_ENSURE_SUCCESS(rv, rv);
      }
    } else {
      // group unfiltered results
      rv = RecursiveGroup(toplevel, aGroupingMode, aGroupCount,
                          result->GetTopLevel());
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (aSortingMode != SORT_BY_NONE)
    result->RecursiveSort(aSortingMode);

  // automatically expand things that were expanded before
  if (gExpandedItems.Count() > 0)
    result->ApplyTreeState(gExpandedItems);

  result->FilledAllResults();

  *_retval = result;
  (*_retval)->AddRef();
  return NS_OK;
}


// nsNavHistory::AddObserver

NS_IMETHODIMP
nsNavHistory::AddObserver(nsINavHistoryObserver* aObserver)
{
  if (mObservers.IndexOfObject(aObserver) >= 0)
    return NS_ERROR_INVALID_ARG; // already registered
  if (!mObservers.AppendObject(aObserver))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}


// nsNavHistory::RemoveObserver

NS_IMETHODIMP
nsNavHistory::RemoveObserver(nsINavHistoryObserver* aObserver)
{
  if (!mObservers.RemoveObject(aObserver))
    return NS_ERROR_INVALID_ARG;
  return NS_OK;
}


// nsNavHistory::BeginUpdateBatch

NS_IMETHODIMP
nsNavHistory::BeginUpdateBatch()
{
  mBatchesInProgress ++;
  if (mBatchesInProgress == 1) {
    for(PRInt32 i = 0; i < mObservers.Count(); i ++)
      mObservers[i]->OnBeginUpdateBatch();
  }
  return NS_OK;
}


// nsNavHistory::EndUpdateBatch

NS_IMETHODIMP
nsNavHistory::EndUpdateBatch()
{
  if (mBatchesInProgress == 0)
    return NS_ERROR_FAILURE;
  mBatchesInProgress --;
  if (mBatchesInProgress == 0) {
    for(PRInt32 i = 0; i < mObservers.Count(); i ++)
      mObservers[i]->OnEndUpdateBatch();
  }
  return NS_OK;
}


// Browser history *************************************************************


// nsNavHistory::AddPageWithDetails
//
//    This function is used by the migration components to import history.
//
//    Note that this always adds the page with one visit and no parent, which
//    is appropriate for imported URIs.
//
//    UNTESTED

NS_IMETHODIMP
nsNavHistory::AddPageWithDetails(nsIURI *aURI, const PRUnichar *aTitle,
                                 PRInt64 aLastVisited)
{
  PRInt64 pageid;
  nsresult rv = InternalAdd(aURI, 0, 0, aTitle, aLastVisited,
                            PR_FALSE, PR_TRUE, &pageid);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// nsNavHistory::GetLastPageVisited

NS_IMETHODIMP
nsNavHistory::GetLastPageVisited(nsACString & aLastPageVisited)
{
  nsXPIDLCString lastPage;
  nsresult rv = gPrefBranch->GetCharPref(PREF_LAST_PAGE_VISITED,
                                         getter_Copies(lastPage));
  NS_ENSURE_SUCCESS(rv, rv);
  aLastPageVisited = lastPage;
  return NS_OK;
}


// nsNavHistory::GetCount
//
//    Finds the total number of history items.
//
//    This function is useless, please don't use it. It's also very slow
//    since in sqlite, count enumerates all results to see how many there are.
//
//    If you want to see if there is any history, use HasHistoryEntries

NS_IMETHODIMP
nsNavHistory::GetCount(PRUint32 *aCount)
{
  NS_WARNING("Don't use history.count: it is slow and useless. Try hasHistoryEntries.");

  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT count(url) FROM moz_history"),
      getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool moreResults;
  rv = dbSelectStatement->ExecuteStep(&moreResults);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!moreResults) {
    // huh? count() should always return one result
    return NS_ERROR_FAILURE;
  }

  PRInt32 countSigned;
  rv = dbSelectStatement->GetInt32(0, &countSigned);
  NS_ENSURE_SUCCESS(rv, rv);

  if (countSigned < 0)
    *aCount = 0;
  else
    *aCount = NS_STATIC_CAST(PRUint32, countSigned);
  return NS_OK;
}


// nsNavHistory::RemovePage
//
//    Removes all visits and the main history entry for the given URI.
//    Silently fails if we have no knowledge of the page.
//
//    UNTESTED

NS_IMETHODIMP
nsNavHistory::RemovePage(nsIURI *aURI)
{
  nsresult rv;
  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  nsCOMPtr<mozIStorageStatement> statement;

  // visits
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisit WHERE page_id IN (SELECT id FROM moz_history WHERE url = ?1)"),
      getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = BindStatementURI(statement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // history entries
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_history WHERE url = ?1"),
      getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = BindStatementURI(statement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Observers: Be sure to finish transaction before calling observers. Note also
  // that we always call the observers even though we aren't sure something
  // actually got deleted.
  transaction.Commit();
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnDeleteURI(aURI);

  return NS_OK;
}


// nsNavHistory::RemovePagesFromHost
//
//    This function will delete all history information about pages from a
//    given host. If aEntireDomain is set, we will also delete pages from
//    sub hosts (so if we are passed in "microsoft.com" we delete
//    "www.microsoft.com", "msdn.microsoft.com", etc.). An empty host name
//    means local files, or you can also pass in the localized "(local files)"
//    title given to you from a history query.
//
//    Silently fails if we have no knowledge of the host
//
//    This function is actually pretty simple, it just boils down to a DELETE
//    statement, but is out of control due to observers and the two types of
//    similar delete operations that we need to supports.
//
//    UNTESTED

NS_IMETHODIMP
nsNavHistory::RemovePagesFromHost(const nsACString& aHost, PRBool aEntireDomain)
{
  nsresult rv;
  UpdateBatchScoper batch(*this); // sends Begin/EndUpdateBatch to obsrvrs.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // Local files don't have any host name. We don't want to delete all files in
  // history when we get passed an empty string, so force to exact match
  if (aHost.IsEmpty())
    aEntireDomain = PR_FALSE;

  // translate "(local files)" to an empty host name
  // be sure to use the TitleForDomain to get the localized name
  nsAutoString host16 = NS_ConvertUTF8toUTF16(aHost);
  nsString localFiles;
  TitleForDomain(NS_LITERAL_STRING(""), localFiles);
  if (host16.Equals(localFiles))
    host16 = NS_LITERAL_STRING("");

  // nsISupports version of the host string for passing to observers
  nsCOMPtr<nsISupportsString> hostSupports(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = hostSupports->SetData(host16);
  NS_ENSURE_SUCCESS(rv, rv);

  // see BindQueryClauseParameters for how this host selection works
  nsAutoString revHostDot;
  GetReversedHostname(host16, revHostDot);
  NS_ASSERTION(revHostDot[revHostDot.Length() - 1] == '.', "Invalid rev. host");
  nsAutoString revHostSlash(revHostDot);
  revHostSlash.Truncate(revHostSlash.Length() - 1);
  revHostSlash.Append(NS_LITERAL_STRING("/"));

  // see if we have to pass all deletes to the observers
  PRBool sendObserversEverything = PR_FALSE;
  PRInt32 i;
  for (i = 0; i < mObservers.Count(); i ++) {
    PRBool thisObserver = PR_FALSE;
    mObservers[i]->GetWantAllDetails(&thisObserver);
    sendObserversEverything |= thisObserver;
  }

  // Tell the observers about everything we are about to delete. Since this is a
  // two-step process, if we get an error, we may tell them we will delete
  // something but then not actually delete it. Too bad.
  nsCOMPtr<mozIStorageStatement> statement;
  if (sendObserversEverything) {
    // create statement depending on delete type
    if (aEntireDomain) {
      rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT url FROM moz_history WHERE rev_host >= ?1 AND rev_host < ?2"),
          getter_AddRefs(statement));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->BindStringParameter(1, revHostSlash);
    } else {
      rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT url FROM moz_history WHERE rev_host = ?1"),
          getter_AddRefs(statement));
    }
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindStringParameter(0, revHostDot);
    NS_ENSURE_SUCCESS(rv, rv);

    // do notifications for all results
    PRBool hasMore = PR_FALSE;
    nsCOMPtr<mozIStorageValueArray> dbRow = do_QueryInterface(statement);
    nsCOMPtr<nsIURI> thisURI;
    while ((statement->ExecuteStep(&hasMore) == NS_OK) && hasMore) {
      nsAutoString thisURIString;
      if (NS_FAILED(dbRow->GetString(0, thisURIString)) || 
          thisURIString.IsEmpty() == 0)
        continue; // no URI
      if (NS_FAILED(NS_NewURI(getter_AddRefs(thisURI), thisURIString,
                              nsnull, nsnull)))
        continue; // bad URI
      for (i = 0; i < mObservers.Count(); i ++)
        mObservers[i]->OnDeleteURI(thisURI);
    }
  }

  if (aEntireDomain) {
    // first, delete the visits
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM moz_historyvisit WHERE page_id IN (SELECT id FROM moz_history WHERE rev_host >= ?1 AND rev_host < ?2)"),
        getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindStringParameter(0, revHostDot);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindStringParameter(1, revHostSlash);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // now delete the actual history entries
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM moz_history WHERE rev_host >= ?1 AND rev_host < ?2"),
        getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindStringParameter(0, revHostDot);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindStringParameter(1, revHostSlash);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // this is deleting a specific host: we require an exact match
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM moz_historyvisit WHERE page_id IN (SELECT id FROM moz_history WHERE rev_host = ?1)"),
        getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindStringParameter(0, revHostDot);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // now delete the actual history entries
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM moz_history WHERE rev_host = ?1"),
        getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindStringParameter(0, revHostDot);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  transaction.Commit();
  return NS_OK;
}


// nsNavHistory::RemoveAllPages
//
//    This function is used to clear history. Notice that we always vacuum the
//    DB, which will compress out the unused space. I'm worried that the freed
//    rows will still be in the DB file after they are deleted, which is
//    probably not what people have in mind when they clear their history.

NS_IMETHODIMP
nsNavHistory::RemoveAllPages()
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // delete visits
  nsCOMPtr<mozIStorageStatement> visitDelete;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisit"), getter_AddRefs(visitDelete));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = visitDelete->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // delete history entries
  nsCOMPtr<mozIStorageStatement> historyDelete;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_history"), getter_AddRefs(historyDelete));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = historyDelete->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // the transaction must complete before the vacuums or they will fail
  transaction.Commit();

  // compress unused space
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM moz_history"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM moz_historyvisit"));
  NS_ENSURE_SUCCESS(rv, rv);

  transaction.Commit();

  // notify observers
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnClearHistory();

  return NS_OK;
}


// nsNavHistory::HidePage
//
//    Sets the 'hidden' column to true.
//
//    If the page doesn't exist in our DB, add it and mark it as hidden. We are
//    probably in the process of loading it (AddURI will only get called when
//    the page actually starts loading OK).

NS_IMETHODIMP
nsNavHistory::HidePage(nsIURI *aURI)
{
  // for speed to save disk accesses
  mozStorageTransaction transaction(mDBConn, PR_FALSE,
                                  mozIStorageConnection::TRANSACTION_EXCLUSIVE);

  // We need to do a query anyway to see if this URL is already in the DB.
  // Might as well ask for the hidden column to save updates in some cases.
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT id,hidden FROM moz_history WHERE url = ?1"),
      getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = BindStatementURI(dbSelectStatement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool alreadyVisited = PR_TRUE;
  rv = dbSelectStatement->ExecuteStep(&alreadyVisited);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!alreadyVisited) {
    // need to add a new page, marked as hidden, never visited

    // need to clear the old statement before we create a new one
    dbSelectStatement = nsnull;

    // add the URL, not typed, hidden, never visited
    rv = InternalAddNewPage(aURI, nsnull, PR_TRUE, PR_FALSE, 0, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

  } else {
    // modify the existing page if necessary

    PRInt32 oldHiddenState = 0;
    rv = dbSelectStatement->GetInt32(1, &oldHiddenState);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!oldHiddenState)
      return NS_OK; // already marked as hidden, we're done

    // find the old ID, which can be found faster than long URLs
    PRInt32 entryid = 0;
    rv = dbSelectStatement->GetInt32(0, &entryid);
    NS_ENSURE_SUCCESS(rv, rv);

    // need to clear the old statement before we create a new one
    dbSelectStatement = nsnull;

    nsCOMPtr<mozIStorageStatement> dbModStatement;
    nsresult rv = mDBConn->CreateStatement(
        NS_LITERAL_CSTRING("UPDATE moz_history SET hidden = 1 WHERE id = ?1"),
        getter_AddRefs(dbModStatement));
    NS_ENSURE_SUCCESS(rv, rv);

    dbModStatement->BindInt32Parameter(0, entryid);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbModStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // notify observers, finish transaction first
  transaction.Commit();
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnPageChanged(aURI, nsINavHistoryObserver::ATTRIBUTE_HIDDEN,
                                 NS_LITERAL_STRING(""));

  return NS_OK;
}


// nsNavHistory::MarkPageAsTyped
//
//    Just sets the typed column to true, which will make this page more likely
//    to float to the top of autocomplete suggestions.
//
//    We can get this notification for pages that have not yet been added to the
//    DB. This happens when you type a new URL. The AddURI is called only when
//    the page is successfully found. If we don't have an entry yet, we add
//    one for this page, marking it as typed but hidden, with a 0 visit count.
//    This will get updated when AddURI is called, and it will clear the hidden
//    flag for typed URLs.

NS_IMETHODIMP
nsNavHistory::MarkPageAsTyped(nsIURI *aURI)
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE,
                                  mozIStorageConnection::TRANSACTION_EXCLUSIVE);

  // We need to do a query anyway to see if this URL is already in the DB.
  // Might as well ask for the typed column to save updates in some cases.
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT id,typed FROM moz_history WHERE url = ?1"),
      getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = BindStatementURI(dbSelectStatement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool alreadyVisited = PR_TRUE;
  rv = dbSelectStatement->ExecuteStep(&alreadyVisited);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!alreadyVisited) {
    // we don't have this URL in the history yet, add it...

    // need to clear the old statement before we create a new one
    dbSelectStatement->Reset();
    dbSelectStatement = nsnull;

    // add the URL, typed but hidden, never visited
    rv = InternalAddNewPage(aURI, nsnull, PR_TRUE, PR_TRUE, 0, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

  } else {
    // we already have this URL, update it if necessary.

    PRInt32 oldTypedState = 0;
    rv = dbSelectStatement->GetInt32(1, &oldTypedState);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!oldTypedState)
      return NS_OK; // already marked as typed, we're done

    // find the old ID, which can be found faster than long URLs
    PRInt32 entryid = 0;
    rv = dbSelectStatement->GetInt32(0, &entryid);
    NS_ENSURE_SUCCESS(rv, rv);

    // need to clear the old statement before we create a new one
    dbSelectStatement->Reset();
    dbSelectStatement = nsnull;

    nsCOMPtr<mozIStorageStatement> dbModStatement;
    nsresult rv = mDBConn->CreateStatement(
        NS_LITERAL_CSTRING("UPDATE moz_history SET typed = 1 WHERE id = ?1"),
        getter_AddRefs(dbModStatement));
    NS_ENSURE_SUCCESS(rv, rv);

    dbModStatement->BindInt32Parameter(0, entryid);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbModStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // observers, be sure to finish transaction first
  transaction.Commit();
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnPageChanged(aURI, nsINavHistoryObserver::ATTRIBUTE_TYPED,
                                 NS_LITERAL_STRING(""));

  return NS_OK;
}

// nsGlobalHistory2 ************************************************************


// nsNavHistory::AddURI

NS_IMETHODIMP
nsNavHistory::AddURI(nsIURI *aURI, PRBool aRedirect,
                     PRBool aToplevel, nsIURI *aReferrer)
{
  // add to main DB
  PRInt64 pageid = 0;
  PRTime now = GetNow();
  nsresult rv = InternalAdd(aURI, 0, 0, nsnull, now, aRedirect, aToplevel,
                            &pageid);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// nsNavHistory::IsVisited
//
//    Note that this ignores the "hidden" flag. This function just checks if the
//    given page is in the DB for link coloring. The "hidden" flag affects
//    the history list view and autocomplete.

NS_IMETHODIMP
nsNavHistory::IsVisited(nsIURI *aURI, PRBool *_retval)
{
  nsCAutoString utf8URISpec;
  nsresult rv = aURI->GetSpec(utf8URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = IsURIStringVisited(utf8URISpec);
  return NS_OK;
}


// nsNavHistory::SetPageTitle

NS_IMETHODIMP
nsNavHistory::SetPageTitle(nsIURI *aURI,
                           const nsAString & aTitle)
{
  nsresult rv;

  nsCOMPtr<mozIStorageStatement> dbModStatement;
  rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("UPDATE moz_history SET title = ?1 WHERE url = ?2"),
      getter_AddRefs(dbModStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  dbModStatement->BindStringParameter(0, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  // url
  rv = BindStatementURI(dbModStatement, 1, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbModStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // observers
  for (PRInt32 i = 0; i < mObservers.Count(); i ++) 
    mObservers[i]->OnPageChanged(aURI, nsINavHistoryObserver::ATTRIBUTE_TITLE,
                                 aTitle);

  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::GetURIGeckoFlags(nsIURI* aURI, PRUint32* aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNavHistory::SetURIGeckoFlags(nsIURI* aURI, PRUint32 aFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIObserver *****************************************************************

NS_IMETHODIMP
nsNavHistory::Observe(nsISupports *aSubject, const char *aTopic,
                    const PRUnichar *aData)
{
  if (nsCRT::strcmp(aTopic, gQuitApplicationMessage) == 0) {
    if (gTldTypes) {
      delete[] gTldTypes;
      gTldTypes = nsnull;
    }
    gPrefService->SavePrefFile(nsnull);
    VacuumDB();
  } else if (nsCRT::strcmp(aTopic, "nsPref:changed") == 0) {
    LoadPrefs();
  }

  //nsCOMPtr<nsIObserver> ob = do_QueryInterface(globalHistory);
  //return ob->Observe(aSubject, aTopic, aData);
  return NS_OK;
}

// nsIAutoCompleteSearch *******************************************************


// AutoCompleteIntermediateResult/Set
//
//    This class holds intermediate autocomplete results so that they can be
//    sorted. This list is then handed off to a result using FillResult. The
//    major reason for this is so that we can use nsArray's sorting functions,
//    not use COM, yet have well-defined lifetimes for the objects. This uses
//    a void array, but makes sure to delete the objects on desctruction.

struct AutoCompleteIntermediateResult
{
  nsString url;
  nsString title;
  PRUint32 priority;
};
class AutoCompleteIntermediateResultSet
{
public:
  AutoCompleteIntermediateResultSet()
  {
  }
  ~AutoCompleteIntermediateResultSet()
  {
    for (PRInt32 i = 0; i < mArray.Count(); i ++) {
      AutoCompleteIntermediateResult* cur = NS_STATIC_CAST(
                                    AutoCompleteIntermediateResult*, mArray[i]);
      delete cur;
    }
  }
  PRBool Add(const nsString& aURL, const nsString& aTitle, PRInt32 aPriority)
  {
    AutoCompleteIntermediateResult* cur = new AutoCompleteIntermediateResult;
    if (! cur)
      return PR_FALSE;
    cur->url = aURL;
    cur->title = aTitle;
    cur->priority = aPriority;
    mArray.AppendElement(cur);
    return PR_TRUE;
  }
  void Sort(nsNavHistory* navHistory)
  {
    mArray.Sort(nsNavHistory::AutoCompleteSortComparison, navHistory);
  }
  nsresult FillResult(nsIAutoCompleteSimpleResult* aResult)
  {
    for (PRInt32 i = 0; i < mArray.Count(); i ++)
    {
      AutoCompleteIntermediateResult* cur = NS_STATIC_CAST(
                                    AutoCompleteIntermediateResult*, mArray[i]);
      nsresult rv = aResult->AppendMatch(cur->url, cur->title);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  }
protected:
  nsVoidArray mArray;
};

// nsNavHistory::StartSearch
//

NS_IMETHODIMP
nsNavHistory::StartSearch(const nsAString & aSearchString,
                          const nsAString & aSearchParam,
                          nsIAutoCompleteResult *aPreviousResult,
                          nsIAutoCompleteObserver *aListener)
{
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aListener);

  nsCOMPtr<nsIAutoCompleteSimpleResult> result =
      do_CreateInstance(NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  result->SetSearchString(aSearchString);

  PRBool usePrevious = (aPreviousResult != nsnull);

  // throw away previous results if the search string is empty after it has had
  // the prefixes removed: if the user types "http://" throw away the search
  // for "http:/" and start fresh with the new query.
  if (usePrevious) {
    nsAutoString cut(aSearchString);
    AutoCompleteCutPrefix(&cut, nsnull);
    if (cut.IsEmpty())
      usePrevious = PR_FALSE;
  }

  // throw away previous results if the new string doesn't begin with the
  // previous search string
  if (usePrevious) {
    nsAutoString previousSearch;
    aPreviousResult->GetSearchString(previousSearch);
    if (previousSearch.Length() > aSearchString.Length()  ||
        !Substring(aSearchString, 0, previousSearch.Length()).Equals(previousSearch)) {
      usePrevious = PR_FALSE;
    }
  }

  if (aSearchString.IsEmpty()) {
    rv = AutoCompleteTypedSearch(result);
  } else if (usePrevious) {
    rv = AutoCompleteRefineHistorySearch(aSearchString, aPreviousResult, result);
  } else {
    rv = AutoCompleteFullHistorySearch(aSearchString, result);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Determine the result of the search
  PRUint32 count;
  result->GetMatchCount(&count);
  if (count > 0) {
    result->SetSearchResult(nsIAutoCompleteResult::RESULT_SUCCESS);
    result->SetDefaultIndex(0);
  } else {
    result->SetSearchResult(nsIAutoCompleteResult::RESULT_NOMATCH);
    result->SetDefaultIndex(-1);
  }

  aListener->OnSearchResult(this, result);
  return NS_OK;
}


// nsNavHistory::StopSearch

NS_IMETHODIMP
nsNavHistory::StopSearch()
{
  return NS_OK;
}


// nsNavHistory::AutoCompleteTypedSearch
//
//    Called when there is no search string. This happens when you press
//    down arrow from the URL bar: the most recent things you typed are listed.
//
//    Ordering here is simpler because there are no boosts for typing, and there
//    is no URL information to use. The ordering just comes out of the DB by
//    visit count (primary) and time since last visited (secondary).

nsresult nsNavHistory::AutoCompleteTypedSearch(
                                            nsIAutoCompleteSimpleResult* result)
{
  // note: need to test against hidden = 1 since the column can also be null
  // (meaning not hidden)
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT url,title FROM moz_history WHERE typed = 1 AND hidden <> 1 ORDER BY visit_count DESC LIMIT 1000"),
      getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(dbSelectStatement->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString entryURL, entryTitle;
    dbSelectStatement->GetString(0, entryURL);
    dbSelectStatement->GetString(1, entryTitle);

    rv = result->AppendMatch(entryURL, entryTitle);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavHistory::AutoCompleteFullHistorySearch
//
//    A brute-force search of the entire history. This matches the given input
//    with every possible history entry, and sorts them by likelihood.
//
//    This may be slow for people on slow computers with large histories.

nsresult
nsNavHistory::AutoCompleteFullHistorySearch(const nsAString& aSearchString,
                                            nsIAutoCompleteSimpleResult* result)
{
  // figure out whether any known prefixes are present in the search string
  // so we know not to ignore them when comparing results later
  AutoCompleteExclude exclude;
  AutoCompleteGetExcludeInfo(aSearchString, &exclude);

  mozStorageStatementScoper scoper(mDBFullAutoComplete);
  nsresult rv = mDBFullAutoComplete->BindInt32Parameter(0, mAutoCompleteMaxCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // load the intermediate result so it can be sorted
  AutoCompleteIntermediateResultSet resultSet;
  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(mDBFullAutoComplete->ExecuteStep(&hasMore)) && hasMore) {

    PRBool typed = mDBFullAutoComplete->AsInt32(kAutoCompleteIndex_Typed);
    if (!typed) {
      if (mAutoCompleteOnlyTyped)
        continue;
    }

    nsAutoString entryurl;
    mDBFullAutoComplete->GetString(kAutoCompleteIndex_URL, entryurl);
    if (AutoCompleteCompare(entryurl, aSearchString, exclude)) {
      nsAutoString entrytitle;
      rv = mDBFullAutoComplete->GetString(kAutoCompleteIndex_Title, entrytitle);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt32 priority = ComputeAutoCompletePriority(entryurl,
                    mDBFullAutoComplete->AsInt32(kAutoCompleteIndex_VisitCount),
                    typed);

      if (! resultSet.Add(entryurl, entrytitle, priority))
        return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // sort and populate the final result
  resultSet.Sort(this);
  return resultSet.FillResult(result);
}


// nsNavHistory::AutoCompleteRefineHistorySearch
//
//    Called when a previous autocomplete result exists and we are adding to
//    the query (making it more specific). The list is already nicely sorted,
//    so we can just remove all the old elements that DON'T match the new query.

nsresult
nsNavHistory::AutoCompleteRefineHistorySearch(
                                  const nsAString& aSearchString,
                                  nsIAutoCompleteResult* aPreviousResult,
                                  nsIAutoCompleteSimpleResult* aNewResult)
{
  // figure out whether any known prefixes are present in the search string
  // so we know not to ignore them when comparing results later
  AutoCompleteExclude exclude;
  AutoCompleteGetExcludeInfo(aSearchString, &exclude);

  PRUint32 matchCount;
  aPreviousResult->GetMatchCount(&matchCount);

  for (PRUint32 i = 0; i < matchCount; i ++) {
    nsAutoString cururl;
    nsresult rv = aPreviousResult->GetValueAt(i, cururl);
    NS_ENSURE_SUCCESS(rv, rv);
    if (AutoCompleteCompare(cururl, aSearchString, exclude)) {
      nsAutoString curcomment;
      rv = aPreviousResult->GetCommentAt(i, curcomment);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aNewResult->AppendMatch(cururl, curcomment);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


// nsNavHistory::AutoCompleteGetExcludeInfo
//
//    If aURL begins with a protocol or domain prefix from our lists, then mark
//    their index in an AutocompleteExclude struct.

void
nsNavHistory::AutoCompleteGetExcludeInfo(const nsAString& aURL,
                                         AutoCompleteExclude* aExclude)
{
  aExclude->schemePrefix = -1;
  aExclude->hostnamePrefix = -1;

  PRInt32 index = 0;
  PRInt32 i;
  for (i = 0; i < mIgnoreSchemes.Count(); ++i) {
    nsString* string = mIgnoreSchemes.StringAt(i);
    if (Substring(aURL, 0, string->Length()).Equals(*string)) {
      aExclude->schemePrefix = i;
      index = string->Length();
      break;
    }
  }

  for (i = 0; i < mIgnoreHostnames.Count(); ++i) {
    nsString* string = mIgnoreHostnames.StringAt(i);
    if (Substring(aURL, index, string->Length()).Equals(*string)) {
      aExclude->hostnamePrefix = i;
      index += string->Length();
      break;
    }
  }

  aExclude->postPrefixOffset = index;
}


// nsNavHistory::AutoCompleteCutPrefix
//
//    Cut any protocol and domain prefixes from aURL, except for those which
//    are specified in aExclude.
//
//    aExclude can be NULL to cut all possible prefixes
//
//    This comparison is case-sensitive.  Therefore, it assumes that aUserURL
//    is a potential URL whose host name is in all lower case.
//
//    The aExclude identifies which prefixes were present in the user's typed
//    entry, which we DON'T want to cut so that it can match the user input.
//    For example. Typed "xyz.com" matches "xyz.com" AND "http://www.xyz.com"
//    but "www.xyz.com" still matches "www.xyz.com".

void
nsNavHistory::AutoCompleteCutPrefix(nsString* aURL,
                                    const AutoCompleteExclude* aExclude)
{
  PRInt32 idx = 0;
  PRInt32 i;
  for (i = 0; i < mIgnoreSchemes.Count(); ++i) {
    if (aExclude && i == aExclude->schemePrefix)
      continue;
    nsString* string = mIgnoreSchemes.StringAt(i);
    if (Substring(*aURL, 0, string->Length()).Equals(*string)) {
      idx = string->Length();
      break;
    }
  }

  if (idx > 0)
    aURL->Cut(0, idx);

  idx = 0;
  for (i = 0; i < mIgnoreHostnames.Count(); ++i) {
    if (aExclude && i == aExclude->hostnamePrefix)
      continue;
    nsString* string = mIgnoreHostnames.StringAt(i);
    if (Substring(*aURL, 0, string->Length()).Equals(*string)) {
      idx = string->Length();
      break;
    }
  }

  if (idx > 0)
    aURL->Cut(0, idx);
}



// nsNavHistory::AutoCompleteCompare
//
//    Determines if a given URL from the history matches the user input.
//    See AutoCompleteCutPrefix for the meaning of aExclude.

PRBool
nsNavHistory::AutoCompleteCompare(const nsAString& aHistoryURL,
                                  const nsAString& aUserURL,
                                  const AutoCompleteExclude& aExclude)
{
  nsAutoString cutHistoryURL(aHistoryURL);
  AutoCompleteCutPrefix(&cutHistoryURL, &aExclude);
  return StringBeginsWith(cutHistoryURL, aUserURL);
}


// nsGlobalHistory::AutoCompleteSortComparison
//
//    The design and reasoning behind the following autocomplete sort
//    implementation is documented in bug 78270. In most cases, the precomputed
//    priorities (by ComputeAutoCompletePriority) are used without further
//    computation.

PRInt32 PR_CALLBACK // static
nsNavHistory::AutoCompleteSortComparison(const void* match1Void,
                                         const void* match2Void,
                                         void *navHistoryVoid)
{
  // get our class pointer (this is a static function)
  nsNavHistory* navHistory = NS_STATIC_CAST(nsNavHistory*, navHistoryVoid);

  const AutoCompleteIntermediateResult* match1 = NS_STATIC_CAST(
                             const AutoCompleteIntermediateResult*, match1Void);
  const AutoCompleteIntermediateResult* match2 = NS_STATIC_CAST(
                             const AutoCompleteIntermediateResult*, match2Void);

  // In most cases the priorities will be different and we just use them
  if (match1->priority != match2->priority)
  {
    return match2->priority - match1->priority;
  }
  else
  {
    // secondary sorting gives priority to site names and paths (ending in a /)
    PRBool isPath1 = PR_FALSE, isPath2 = PR_FALSE;
    if (!match1->url.IsEmpty())
      isPath1 = (match1->url.Last() == PRUnichar('/'));
    if (!match2->url.IsEmpty())
      isPath2 = (match2->url.Last() == PRUnichar('/'));

    if (isPath1 && !isPath2) return -1; // match1->url is a website/path, match2->url isn't
    if (!isPath1 && isPath2) return  1; // match1->url isn't a website/path, match2->url is

    // find the prefixes so we can sort by the stuff after the prefixes
    AutoCompleteExclude prefix1, prefix2;
    navHistory->AutoCompleteGetExcludeInfo(match1->url, &prefix1);
    navHistory->AutoCompleteGetExcludeInfo(match2->url, &prefix2);

    // compare non-prefixed urls
    PRInt32 ret = Compare(
      Substring(match1->url, prefix1.postPrefixOffset, match1->url.Length()),
      Substring(match2->url, prefix2.postPrefixOffset, match2->url.Length()));
    if (ret != 0)
      return ret;

    // sort http://xyz.com before http://www.xyz.com
    return prefix1.postPrefixOffset - prefix2.postPrefixOffset;
  }
  return 0;
}


// Query stuff *****************************************************************


// nsNavHistory::QueryToSelectClause
//
//    THE ORDER AND BEHAVIOR SHOULD BE IN SYNC WITH BindQueryClauseParameters
//
//    I don't check return values from the query object getters because there's
//    no way for those to fail.

nsresult
nsNavHistory::QueryToSelectClause(nsINavHistoryQuery* aQuery, // const
                                  PRInt32 aStartParameter,
                                  nsCString* aClause,
                                  PRInt32* aParamCount)
{
  PRBool hasIt;

  aClause->Truncate();
  *aParamCount = 0;
  nsCAutoString paramString;

  // begin time
  if (NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) {
    parameterString(aStartParameter + *aParamCount, paramString);
    *aClause += NS_LITERAL_CSTRING("v.visit_date >= ") + paramString;
    (*aParamCount) ++;
  }

  // end time
  if (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt) {
    if (! aClause->IsEmpty())
      *aClause += NS_LITERAL_CSTRING(" AND ");
    parameterString(aStartParameter + *aParamCount, paramString);
    *aClause += NS_LITERAL_CSTRING("v.visit_date <= ") + paramString;
    (*aParamCount) ++;
  }

  // search terms FIXME

  // onlyBookmarked FIXME

  // domain
  if (NS_SUCCEEDED(aQuery->GetHasDomain(&hasIt)) && hasIt) {
    if (! aClause->IsEmpty())
      *aClause += NS_LITERAL_CSTRING(" AND ");

    PRBool domainIsHost = PR_FALSE;
    aQuery->GetDomainIsHost(&domainIsHost);
    if (domainIsHost) {
      parameterString(aStartParameter + *aParamCount, paramString);
      *aClause += NS_LITERAL_CSTRING("h.rev_host = ") + paramString;
      (*aParamCount) ++;
    } else {
      // see domain setting in BindQueryClauseParameters for why we do this
      parameterString(aStartParameter + *aParamCount, paramString);
      *aClause += NS_LITERAL_CSTRING("h.rev_host >= ") + paramString;
      (*aParamCount) ++;

      parameterString(aStartParameter + *aParamCount, paramString);
      *aClause += NS_LITERAL_CSTRING(" AND h.rev_host < ") + paramString;
      (*aParamCount) ++;
    }
  }
  return NS_OK;
}


// nsNavHistory::BindQueryClauseParameters
//
//    THE ORDER AND BEHAVIOR SHOULD BE IN SYNC WITH QueryToSelectClause

nsresult
nsNavHistory::BindQueryClauseParameters(mozIStorageStatement* statement,
                                        PRInt32 aStartParameter,
                                        nsINavHistoryQuery* aQuery, // const
                                        PRInt32* aParamCount)
{
  nsresult rv;
  (*aParamCount) = 0;

  PRBool hasIt;

  // begin time
  if (NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) {
    PRTime time;
    aQuery->GetBeginTime(&time);
    rv = statement->BindInt64Parameter(aStartParameter + *aParamCount, time);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aParamCount) ++;
  }

  // end time
  if (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt) {
    PRTime time;
    aQuery->GetEndTime(&time);
    rv = statement->BindInt64Parameter(aStartParameter + *aParamCount, time);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aParamCount) ++;
  }

  // search terms FIXME

  // onlyBookmarked FIXME

  // domain (see GetReversedHostname for more info on reversed host names)
  if (NS_SUCCEEDED(aQuery->GetHasDomain(&hasIt)) && hasIt) {
    nsAutoString domain;
    aQuery->GetDomain(domain);
    nsString revDomain;
    GetReversedHostname(domain, revDomain);

    PRBool domainIsHost = PR_FALSE;
    aQuery->GetDomainIsHost(&domainIsHost);
    if (domainIsHost) {
      rv = statement->BindStringParameter(aStartParameter + *aParamCount, revDomain);
      NS_ENSURE_SUCCESS(rv, rv);
      (*aParamCount) ++;
    } else {
      // for "mozilla.org" do query >= "gro.allizom." AND < "gro.allizom/"
      // which will get everything starting with "gro.allizom." while using the
      // index (using SUBSTRING() causes indexes to be discarded).
      NS_ASSERTION(revDomain[revDomain.Length() - 1] == '.', "Invalid rev. host");
      rv = statement->BindStringParameter(aStartParameter + *aParamCount, revDomain);
      NS_ENSURE_SUCCESS(rv, rv);
      (*aParamCount) ++;
      revDomain.Truncate(revDomain.Length() - 1);
      revDomain.Append(PRUnichar('/'));
      rv = statement->BindStringParameter(aStartParameter + *aParamCount, revDomain);
      NS_ENSURE_SUCCESS(rv, rv);
      (*aParamCount) ++;
    }
  }
  return NS_OK;
}


// nsNavHistory::ResultsAsList
//

nsresult
nsNavHistory::ResultsAsList(mozIStorageStatement* statement, PRBool aAsVisits,
                            nsCOMArray<nsNavHistoryResultNode>* aResults)
{
  nsresult rv;
  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(statement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
    nsCOMPtr<nsNavHistoryResultNode> result;
    rv = RowToResult(row, aAsVisits, getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);
    aResults->AppendObject(result);
  }
  return NS_OK;
}


// nsNavHistory::RecursiveGroup
//
//    aSource and aDest must be different!

nsresult
nsNavHistory::RecursiveGroup(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                             const PRInt32* aGroupingMode, PRUint32 aGroupCount,
                             nsCOMArray<nsNavHistoryResultNode>* aDest)
{
  NS_ASSERTION(aGroupCount > 0, "Invalid group count");
  NS_ASSERTION(aDest->Count() == 0, "Destination array is not empty");
  NS_ASSERTION(&aSource != aDest, "Source and dest must be different for grouping");

  nsresult rv;
  switch (aGroupingMode[0]) {
    case GROUP_BY_DAY:
      rv = GroupByDay(aSource, aDest);
      break;
    case GROUP_BY_HOST:
      rv = GroupByHost(aSource, aDest);
      break;
    case GROUP_BY_DOMAIN:
      rv = GroupByDomain(aSource, aDest);
      break;
    default:
      // unknown grouping mode
      return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (aGroupCount > 1) {
    // Sort another level: We need to copy the array since we want the output
    // to be our level's destionation arrays.
    for (PRInt32 i = 0; i < aDest->Count(); i ++) {
      nsNavHistoryResultNode* curNode = (*aDest)[i];
      if (curNode->mChildren.Count() > 0) {
        nsCOMArray<nsNavHistoryResultNode> temp(curNode->mChildren);
        curNode->mChildren.Clear();
        rv = RecursiveGroup(temp, &aGroupingMode[1], aGroupCount - 1,
                            &curNode->mChildren);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  return NS_OK;
}


// nsNavHistory::GroupByDay
//
//    FIXME TODO IMPLEMENT ME (or maybe we don't care about this?)

nsresult
nsNavHistory::GroupByDay(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                         nsCOMArray<nsNavHistoryResultNode>* aDest)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistory::GroupByHost
//
//    Here, we just hash every host name to get the unique ones. 256 is a guess
//    as to the upper bound on the number of unique hosts. The hash table will
//    grow when the number of elements is greater than 0.75 * the current size
//    of the table.

nsresult
nsNavHistory::GroupByHost(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                          nsCOMArray<nsNavHistoryResultNode>* aDest)
{
  nsDataHashtable<nsStringHashKey, nsNavHistoryResultNode*> hosts;
  if (! hosts.Init(256))
    return NS_ERROR_OUT_OF_MEMORY;
  for (PRInt32 i = 0; i < aSource.Count(); i ++)
  {
    const nsString& curHostName = aSource[i]->mHost;
    nsNavHistoryResultNode* curHostGroup = nsnull;
    if (hosts.Get(curHostName, &curHostGroup)) {
      // already have an entry for this host, use it
      curHostGroup->mChildren.AppendObject(aSource[i]);
    } else {
      // need to create an entry for this host
      curHostGroup = new nsNavHistoryResultNode();
      if (! curHostGroup)
        return NS_ERROR_OUT_OF_MEMORY;
      curHostGroup->mType = nsNavHistoryResultNode::RESULT_TYPE_HOST;
      curHostGroup->mHost = curHostName;
      TitleForDomain(curHostName, curHostGroup->mTitle);
      curHostGroup->mChildren.AppendObject(aSource[i]);

      if (! hosts.Put(curHostName, curHostGroup))
        return NS_ERROR_OUT_OF_MEMORY;
      nsresult rv = aDest->AppendObject(curHostGroup);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


// nsNavHistory::GroupByDomain
//

nsresult
nsNavHistory::GroupByDomain(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                            nsCOMArray<nsNavHistoryResultNode>* aDest)
{
  nsDataHashtable<nsStringHashKey, nsNavHistoryResultNode*> hosts;
  if (! hosts.Init(256))
    return NS_ERROR_OUT_OF_MEMORY;
  for (PRInt32 i = 0; i < aSource.Count(); i ++)
  {
    const nsString& curHostName = aSource[i]->mHost;
    nsAutoString topDomain;
    if (IsNumericHostName(curHostName)) {
      // numeric hosts just use the full host name without collapsing
      topDomain = curHostName;
    } else {
      // regular host name, find the substring to use as the parent host name
      PRInt32 tldLength = GetTLDCharCount(curHostName);
      if (tldLength < (int)curHostName.Length()) {
        // bugzilla.mozilla.org : tldLength = 3, topDomain = mozilla.org
        GetSubstringFromNthDot(curHostName,
                               curHostName.Length() - tldLength - 2,
                               1, PR_FALSE, topDomain);
      }
    }

    nsNavHistoryResultNode* curTopGroup = nsnull;
    if (hosts.Get(topDomain, &curTopGroup)) {
      // already have an entry for this host, use it
      curTopGroup->mChildren.AppendObject(aSource[i]);
    } else {
      // need to create an entry for this host
      curTopGroup = new nsNavHistoryResultNode();
      if (! curTopGroup)
        return NS_ERROR_OUT_OF_MEMORY;
      curTopGroup->mType = nsNavHistoryResultNode::RESULT_TYPE_HOST;
      curTopGroup->mHost = topDomain;
      TitleForDomain(topDomain, curTopGroup->mTitle);
      curTopGroup->mChildren.AppendObject(aSource[i]);

      if (! hosts.Put(topDomain, curTopGroup))
        return NS_ERROR_OUT_OF_MEMORY;
      nsresult rv = aDest->AppendObject(curTopGroup);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


// nsNavHistory::FilterResultSet
//
//    Currently, this just does title/url filtering. This should be expanded in
//    the future.

nsresult
nsNavHistory::FilterResultSet(const nsCOMArray<nsNavHistoryResultNode>& aSet,
                              nsCOMArray<nsNavHistoryResultNode>* aFiltered,
                              const nsString& aSearch)
{
  nsStringArray terms;
  ParseSearchQuery(aSearch, &terms);

  // if there are no search terms, just return everything (i.e. do nothing)
  if (terms.Count() == 0) {
    aFiltered->AppendObjects(aSet);
    return NS_OK;
  }

  nsCStringArray searchAnnotations;
  /*
  if (mAnnotationService) {
    searchAnnotations.AppendCString(NS_LITERAL_CSTRING("qwer"));
    searchAnnotations.AppendCString(NS_LITERAL_CSTRING("asdf"));
    searchAnnotations.AppendCString(NS_LITERAL_CSTRING("zxcv"));
    //mAnnotationService->GetSearchableAnnotations();
  }
  */

  for (PRInt32 nodeIndex = 0; nodeIndex < aSet.Count(); nodeIndex ++) {
    PRBool allTermsFound = PR_TRUE;

    nsStringArray curAnnotations;
    /*
    if (searchAnnotations.Count()) {
      // come up with a list of all annotation *values* we need to search
      for (PRInt32 annotIndex = 0; annotIndex < searchAnnotations.Count(); annotIndex ++) {
        nsString annot;
        if (NS_SUCCEEDED(mAnnotationService->GetAnnotationString(
                                         aSet[nodeIndex]->mUrl,
                                         *searchAnnotations[annotIndex],
                                         annot)))
          curAnnotations.AppendString(annot);
      }
    }
    */

    for (PRInt32 termIndex = 0; termIndex < terms.Count(); termIndex ++) {
      PRBool termFound = PR_FALSE;
      // title and URL
      if (CaseInsensitiveFindInReadable(*terms[termIndex],
                                        aSet[nodeIndex]->mTitle) ||
          CaseInsensitiveFindInReadable(*terms[termIndex],
                                        NS_ConvertUTF8toUTF16(aSet[nodeIndex]->mUrl)))
        termFound = PR_TRUE;
      // searchable annotations
      if (! termFound) {
        for (PRInt32 annotIndex = 0; annotIndex < curAnnotations.Count(); annotIndex ++) {
          if (CaseInsensitiveFindInReadable(*terms[termIndex],
                                            *curAnnotations[annotIndex]))
            termFound = PR_TRUE;
        }
      }
      if (! termFound) {
        allTermsFound = PR_FALSE;
        break;
      }
    }
    if (allTermsFound)
      aFiltered->AppendObject(aSet[nodeIndex]);
  }
  return NS_OK;
}


// nsNavHistory::RowToResult
//

nsresult
nsNavHistory::RowToResult(mozIStorageValueArray* aRow, PRBool aAsVisits,
                          nsNavHistoryResultNode** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsNavHistoryResultNode> result = new nsNavHistoryResultNode();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  // ID
  nsresult rv = aRow->GetInt64(kGetInfoIndex_PageID, &result->mID);
  NS_ENSURE_SUCCESS(rv, rv);

  // url
  rv = aRow->GetUTF8String(kGetInfoIndex_URL, result->mUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  rv = aRow->GetString(kGetInfoIndex_Title, result->mTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  // access count
  rv = aRow->GetInt32(kGetInfoIndex_VisitCount, &result->mAccessCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // access time
  rv = aRow->GetInt64(kGetInfoIndex_VisitDate, &result->mTime);
  NS_ENSURE_SUCCESS(rv, rv);

  // reversed hostname
  nsAutoString revHost;
  rv = aRow->GetString(kGetInfoIndex_RevHost, revHost);
  GetUnreversedHostname(revHost, result->mHost);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aAsVisits) {
    result->mType = nsNavHistoryResultNode::RESULT_TYPE_VISIT;
  } else {
    result->mType = nsNavHistoryResultNode::RESULT_TYPE_URL;
  }

  result.swap(*aResult);
  return NS_OK;
}


// nsNavHistory::TitleForDomain
//
//    This computes the title for a given domain. Normally, this is just the
//    domain name, but we specially handle empty cases to give you a nice
//    localized string.

void
nsNavHistory::TitleForDomain(const nsString& domain, nsAString& aTitle)
{
  if (! domain.IsEmpty()) {
    aTitle = domain;
    return;
  }

  // use the localized one instead
  nsXPIDLString value;
  nsresult rv = mBundle->GetStringFromName(
      NS_LITERAL_STRING("localhost").get(), getter_Copies(value));
  if (NS_SUCCEEDED(rv))
    aTitle = value;
  else
    aTitle.Truncate(0);
}


// nsNavHistory::ImportFromMork
//
//    FIXME: this is basically a hack, but it works.
//
//    This is for development/testing purposes only. Uncomment the call to it
//    from nsNavHistory::Init, run the app, and recomment the call. If you call
//    this more than once, you'll get duplicate entries. Also, this assumes you
//    have no history. If there is a URL that already exists, you'll get
//    a duplicate entry and everything will break.
//
//    This will be replaced by an importer that does not depend on the Mork
//    library for release (since the library is very large).

nsresult nsNavHistory::ImportFromMork()
{
  nsresult rv;

  nsCOMPtr<nsIFile> historyFile;
  rv = NS_GetSpecialDirectory(NS_APP_HISTORY_50_FILE, getter_AddRefs(historyFile));

  static NS_DEFINE_CID(kMorkCID, NS_MORK_CID);
  nsIMdbFactory* mdbFactory;
  nsCOMPtr<nsIMdbFactoryFactory> factoryfactory =
      do_CreateInstance(kMorkCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = factoryfactory->GetMdbFactory(&mdbFactory);
  NS_ENSURE_SUCCESS(rv, rv);

  mdb_err err;

  nsIMdbEnv* env;
  err = mdbFactory->MakeEnv(nsnull, &env);
  env->SetAutoClear(PR_TRUE);
  NS_ASSERTION((err == 0), "unable to create mdb env");
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  // MDB requires native file paths
  nsCAutoString filePath;
  rv = historyFile->GetNativePath(filePath);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists = PR_TRUE;
  historyFile->Exists(&exists);
  if (! exists) {
    printf("No MDB file, not importing\n");
    return NS_ERROR_FAILURE;
  }

  mdb_bool canopen = 0;
  mdbYarn outfmt = { nsnull, 0, 0, 0, 0, nsnull };

  nsIMdbHeap* dbHeap = 0;
  mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable
  nsCOMPtr<nsIMdbFile> oldFile; // ensures file is released
  err = mdbFactory->OpenOldFile(env, dbHeap, PromiseFlatCString(filePath).get(),
                                dbFrozen, getter_AddRefs(oldFile));
  if ((err !=0) || !oldFile) {
    printf("Some error, not importing.\n");
    return NS_ERROR_FAILURE;
  }

  err = mdbFactory->CanOpenFilePort(env, oldFile, // the file to investigate
                                    &canopen, &outfmt);
  if ((err !=0) || !canopen) {
    printf("Some error 2, not importing.\n");
    return NS_ERROR_FAILURE;
  }

  nsIMdbThumb* thumb = nsnull;
  mdbOpenPolicy policy = { { 0, 0 }, 0, 0 };

  err = mdbFactory->OpenFileStore(env, dbHeap, oldFile, &policy, &thumb);
  if ((err !=0) || !thumb) return NS_ERROR_FAILURE;

  mdb_count total;
  mdb_count current;
  mdb_bool done;
  mdb_bool broken;

  do {
    err = thumb->DoMore(env, &total, &current, &done, &broken);
  } while ((err == 0) && !broken && !done);

  nsIMdbStore* store;
  if ((err == 0) && done) {
    err = mdbFactory->ThumbToOpenStore(env, thumb, &store);
  }

  NS_IF_RELEASE(thumb);

  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  //rv = CreateTokens();
  //NS_ENSURE_SUCCESS(rv, rv);

  mdb_scope  kToken_HistoryRowScope;
  err = store->StringToToken(env, "ns:history:db:row:scope:history:all", &kToken_HistoryRowScope);

  mdbOid oid = { kToken_HistoryRowScope, 1 };
  nsIMdbTable* table;
  err = store->GetTable(env, &oid, &table);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);
  if (!table) {
    NS_WARNING("Your history file is somehow corrupt.. deleting it.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIMdbRow> metaRow;
  err = table->GetMetaRow(env, &oid, nsnull, getter_AddRefs(metaRow));
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);
  if (err != 0) {
    printf("Some error 3, not importing\n");
    return NS_ERROR_FAILURE;
  }

  // FIXME: init byte order, may need to swap strings

  nsIMdbTableRowCursor* cursor;
  table->GetTableRowCursor(env, -1, &cursor);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  nsIMdbRow* currentRow;
  mdb_pos pos;

  mdb_column urlColumn, hiddenColumn, typedColumn, titleColumn,
             visitCountColumn, visitDateColumn;
  err = store->StringToToken(env, "URL", &urlColumn);
  err = store->StringToToken(env, "Hidden", &hiddenColumn);
  err = store->StringToToken(env, "Typed", &typedColumn);
  err = store->StringToToken(env, "Name", &titleColumn);
  err = store->StringToToken(env, "VisitCount", &visitCountColumn);
  err = store->StringToToken(env, "LastVisitDate", &visitDateColumn);

  nsCOMPtr<mozIStorageStatement> insertHistory, insertVisit;
  rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("INSERT INTO moz_history (url, title, rev_host, visit_count, hidden, typed) VALUES (?1, ?2, ?3, ?4, ?5, ?6)"),
      getter_AddRefs(insertHistory));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("INSERT INTO moz_historyvisit (from_visit, page_id, visit_date, visit_type, session) VALUES (0, ?1, ?2, 0, 0)"),
      getter_AddRefs(insertVisit));
  NS_ENSURE_SUCCESS(rv, rv);

  mDBConn->BeginTransaction();

  PRInt32 insertCount = 0;
  while(1) {
    cursor->NextRow(env, &currentRow, &pos);
    NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);
    if (! currentRow)
      break;

    // url
    mdbYarn yarn;
    err = currentRow->AliasCellYarn(env, urlColumn, &yarn);
    if (err != 0 || !yarn.mYarn_Buf) continue;
    nsCString urlString((char*)yarn.mYarn_Buf);

    // typed
    PRBool typed = PR_TRUE;
    currentRow->AliasCellYarn(env, typedColumn, &yarn);
    if (err != 0 || !yarn.mYarn_Buf) typed = PR_FALSE;

    // hidden
    PRBool hidden = PR_TRUE;
    currentRow->AliasCellYarn(env, hiddenColumn, &yarn);
    if (err != 0 || !yarn.mYarn_Buf) hidden = PR_FALSE;

    // title
    nsAutoString title;
    err = currentRow->AliasCellYarn(env, titleColumn, &yarn);
    if (err == 0 && yarn.mYarn_Buf)
      title = nsString((PRUnichar*)yarn.mYarn_Buf);

    // visit count
    PRInt32 visitCount = 0;
    currentRow->AliasCellYarn(env, visitCountColumn, &yarn);
    if (err == 0 && yarn.mYarn_Buf)
      sscanf((char*)yarn.mYarn_Buf, "%d", &visitCount);
    if (visitCount == 0)
      continue;

    // last visit date (if none, don't add to history
    PRTime visitDate;
    currentRow->AliasCellYarn(env, visitDateColumn, &yarn);
    if (err != 0 || ! yarn.mYarn_Buf)
      continue;
    sscanf((char*)yarn.mYarn_Buf, "%lld", &visitDate);

    NS_RELEASE(currentRow);

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), PromiseFlatCString(urlString).get());
    nsAutoString revhost;
    rv = GetReversedHostname(uri, revhost);

    insertHistory->BindUTF8StringParameter(0, urlString);
    insertHistory->BindStringParameter(1, title);
    insertHistory->BindStringParameter(2, revhost);
    insertHistory->BindInt32Parameter(3, visitCount);
    insertHistory->BindInt32Parameter(4, hidden);
    insertHistory->BindInt32Parameter(5, typed);
    rv = insertHistory->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt64 pageid;
    rv = mDBConn->GetLastInsertRowID(&pageid);
    NS_ENSURE_SUCCESS(rv, rv);

    insertVisit->BindInt64Parameter(0, pageid);
    insertVisit->BindInt64Parameter(1, visitDate);
    rv = insertVisit->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    insertCount ++;
  }
  mDBConn->CommitTransaction();
  printf("INSERTED %d ROWS FROM MORK\n", insertCount);
  return NS_OK;
}

// Local function **************************************************************


// ComputeAutoCompletePriority
//
//    Favor websites and webpaths more than webpages by boosting
//    their visit counts.  This assumes that URLs have been normalized,
//    appending a trailing '/'.
//
//    We use addition to boost the visit count rather than multiplication
//    since we want URLs with large visit counts to remain pretty much
//    in raw visit count order - we assume the user has visited these urls
//    often for a reason and there shouldn't be a problem with putting them
//    high in the autocomplete list regardless of whether they are sites/
//    paths or pages.  However for URLs visited only a few times, sites
//    & paths should be presented before pages since they are generally
//    more likely to be visited again.

PRInt32
ComputeAutoCompletePriority(const nsAString& aUrl, PRInt32 aVisitCount,
                            PRBool aWasTyped)
{
  PRInt32 aPriority = aVisitCount;

  if (!aUrl.IsEmpty()) {
    // url is a site/path if it has a trailing slash
    if (aUrl.Last() == PRUnichar('/'))
      aPriority += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;
  }

  if (aWasTyped)
    aPriority += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;

  return aPriority;
}


// GetReversedHostname
//
//    This extracts the hostname from the URI and reverses it in the
//    form that we use (always ending with a "."). So
//    "http://microsoft.com/" becomes "moc.tfosorcim."
//
//    The idea behind this is that we can create an index over the items in
//    the reversed host name column, and then query for as much or as little
//    of the host name as we feel like.
//
//    For example, the query "host >= 'gro.allizom.' AND host < 'gro.allizom/'
//    Matches all host names ending in '.mozilla.org', including
//    'developer.mozilla.org' and just 'mozilla.org' (since we define all
//    reversed host names to end in a period, even 'mozilla.org' matches).
//    The important thing is that this operation uses the index. Any substring
//    calls in a select statement (even if it's for the beginning of a string)
//    will bypass any indices and will be slow).

nsresult
GetReversedHostname(nsIURI* aURI, nsAString& aRevHost)
{
  nsCString forward8;
  nsresult rv = aURI->GetHost(forward8);
  NS_ENSURE_SUCCESS(rv, rv);

  // can't do reversing in UTF8, better use 16-bit chars
  nsAutoString forward = NS_ConvertUTF8toUTF16(forward8);
  GetReversedHostname(forward, aRevHost);
  return NS_OK;
}


// GetReversedHostname
//
//    Same as previous but for strings

void
GetReversedHostname(const nsString& aForward, nsAString& aRevHost)
{
  ReverseString(aForward, aRevHost);
  aRevHost.Append(PRUnichar('.'));
}


// GetUnreversedHostname
//
//    This takes a reversed hostname as above and converts it to a
//    "regular" hostname with no dot at the beginning.
//
//    Input:
//      gor.allizom.
//    Output
//      mozilla.org
//
//    See GetReversedHostname() above

void
GetUnreversedHostname(const nsString& aBackward, nsAString& aForward)
{
  NS_ASSERTION(! aBackward.IsEmpty() > 0 && aBackward[aBackward.Length()-1] == '.',
               "Malformed reversed hostname with no trailing dot");

  aForward.Truncate(0);
  if (! aBackward.IsEmpty() && aBackward[aBackward.Length()-1] == '.') {
    // copy everything except the trailing dot
    for (PRInt32 i = aBackward.Length() - 2; i >= 0; i -- )
      aForward.Append(aBackward[i]);
  } else {
    NS_WARNING("Malformed reversed host name: no trailing dot");
    ReverseString(aBackward, aForward);
  }
}


// IsNumericHostName
//
//    For host-based groupings, numeric IPs should not be collapsed by the
//    last part of the domain name, but should stand alone. This code determines
//    if this is the case so we don't collapse "10.1.2.3" to "3". It would be
//    nice to use the URL parsing code, but it doesn't give us this information,
//    this is usually done by the OS in response to DNS lookups.
//
//    This implementation is not perfect, we just check for all numbers and
//    digits, and three periods. You could come up with crazy internal host
//    names that would fool this logic, but I bet there are no real examples.

PRBool IsNumericHostName(const nsString& aHost)
{
  PRInt32 periodCount = 0;
  for (PRUint32 i = 0; i < aHost.Length(); i ++) {
    char cur = aHost[i];
    if (cur == '.')
      periodCount ++;
    else if (cur < '0' || cur > '9')
      return PR_FALSE;
  }
  return (periodCount == 3);
}


// ParseSearchQuery
//
//    This just breaks the query up into words. We don't do anything fancy,
//    not even quoting. We do, however, strip quotes, because people might
//    try to input quotes expecting them to do something and get no results
//    back.

inline PRBool isQueryWhitespace(PRUnichar ch)
{
  return ch == ' ';
}

void ParseSearchQuery(const nsString& aQuery, nsStringArray* aTerms)
{
  PRInt32 lastBegin = -1;
  for (PRUint32 i = 0; i < aQuery.Length(); i ++) {
    if (isQueryWhitespace(aQuery[i]) || aQuery[i] == '"') {
      if (lastBegin >= 0) {
        // found the end of a word
        aTerms->AppendString(Substring(aQuery, lastBegin, i - lastBegin));
        lastBegin = -1;
      }
    } else {
      if (lastBegin < 0) {
        // found the beginning of a word
        lastBegin = i;
      }
    }
  }
  // last word
  if (lastBegin >= 0)
    aTerms->AppendString(Substring(aQuery, lastBegin));
}


// GetTLDCharCount
//
//    Given a normal, forward host name ("bugzilla.mozilla.org")
//    returns the number of characters that the TLD occupies, NOT including the
//    trailing dot: bugzilla.mozilla.org -> 3
//                  theregister.co.uk -> 5
//                  mysite.us -> 2

PRInt32
GetTLDCharCount(const nsString& aHost)
{
  nsString trailing;
  GetSubstringFromNthDot(aHost, aHost.Length() - 1, 1,
                         PR_FALSE, trailing);

  switch (GetTLDType(trailing)) {
    case 0:
      // not a known TLD
      return 0;
    case 1:
      // first-level TLD
      return trailing.Length();
    case 2: {
      // need to check second level and trim it too (if valid)
      nsString trailingMore;
      GetSubstringFromNthDot(aHost, aHost.Length() - 1,
                             2, PR_FALSE, trailingMore);
      if (GetTLDType(trailingMore))
        return trailingMore.Length();
      else
        return trailing.Length();
    }
    default:
      NS_NOTREACHED("Invalid TLD type");
      return 0;
  }
}


// GetTLDType
//
//    Given the last part of a host name, tells you whether this is a known TLD.
//      0 -> not known
//      1 -> known 1st or second level TLD ("com", "co.uk")
//      2 -> end of a two-part TLD ("uk")
//
//    If this returns 2, you should probably re-call the function including
//    the next level of name. For example ("uk" -> 2, then you call with
//    "co.uk" and know that the last two pars of this domain name are
//    "toplevel".
//
//    This should be moved somewhere else (like cookies) and made easier to
//    update.

PRInt32
GetTLDType(const nsString& aHostTail)
{
  //static nsDataHashtable<nsStringHashKey, int> tldTypes;
  if (! gTldTypes) {
    // need to populate table
    gTldTypes = new nsDataHashtable<nsStringHashKey, int>();
    if (! gTldTypes)
      return 0;

    gTldTypes->Init(256);

    gTldTypes->Put(NS_LITERAL_STRING("com"), 1);
    gTldTypes->Put(NS_LITERAL_STRING("org"), 1);
    gTldTypes->Put(NS_LITERAL_STRING("net"), 1);
    gTldTypes->Put(NS_LITERAL_STRING("edu"), 1);
    gTldTypes->Put(NS_LITERAL_STRING("gov"), 1);
    gTldTypes->Put(NS_LITERAL_STRING("mil"), 1);
    gTldTypes->Put(NS_LITERAL_STRING("uk"), 2);
    gTldTypes->Put(NS_LITERAL_STRING("co.uk"), 1);
    gTldTypes->Put(NS_LITERAL_STRING("kr"), 2);
    gTldTypes->Put(NS_LITERAL_STRING("co.kr"), 1);
    gTldTypes->Put(NS_LITERAL_STRING("hu"), 1);
    gTldTypes->Put(NS_LITERAL_STRING("us"), 1);

    // FIXME: add the rest
  }

  PRInt32 type = 0;
  if (gTldTypes->Get(aHostTail, &type))
    return type;
  else
    return 0;
}


// GetSubstringFromNthDot
//
//    Similar to GetSubstringToNthDot except searches backward
//      GetSubstringFromNthDot("foo.bar", length, 1, PR_FALSE) -> "bar"
//
//    It is legal to pass in a starting position < 0 so you can just
//    use Length()-1 as the starting position even if the length is 0.

void GetSubstringFromNthDot(const nsString& aInput, PRInt32 aStartingSpot,
                            PRInt32 aN, PRBool aIncludeDot, nsAString& aSubstr)
{
  PRInt32 dotsFound = 0;
  for (PRInt32 i = aStartingSpot; i >= 0; i --) {
    if (aInput[i] == '.') {
      dotsFound ++;
      if (dotsFound == aN) {
        if (aIncludeDot)
          aSubstr = Substring(aInput, i, aInput.Length() - i);
        else
          aSubstr = Substring(aInput, i + 1, aInput.Length() - i - 1);
        return;
      }
    }
  }
  aSubstr = aInput; // no dot found
}


// BindStatementURI
//
//    Binds the specified URI as the parameter 'index' for the statment.
//    URIs are always bound as UTF8

nsresult BindStatementURI(mozIStorageStatement* statement, PRInt32 index,
                          nsIURI* aURI)
{
  nsCAutoString utf8URISpec;
  nsresult rv = aURI->GetSpec(utf8URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindUTF8StringParameter(index, utf8URISpec);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// nsNavHistoryResultNode ******************************************************


NS_IMPL_ISUPPORTS1(nsNavHistoryResultNode, nsINavHistoryResultNode)

nsNavHistoryResultNode::nsNavHistoryResultNode() : mID(0), mExpanded(PR_FALSE)
{
}

/* attribute nsINavHistoryResultNode parent */
NS_IMETHODIMP nsNavHistoryResultNode::GetParent(nsINavHistoryResultNode** parent)
{
  *parent = mParent;
  (*parent)->AddRef();
  return NS_OK;
}

/* attribute PRInt32 type; */
NS_IMETHODIMP nsNavHistoryResultNode::GetType(PRUint32 *aType)
{
  *aType = mType;
  return NS_OK;
}

/* attribute string url; */
NS_IMETHODIMP nsNavHistoryResultNode::GetUrl(nsACString& aUrl)
{
  aUrl = mUrl;
  return NS_OK;
}

/* attribute string title; */
NS_IMETHODIMP nsNavHistoryResultNode::GetTitle(nsAString& aTitle)
{
  aTitle = mTitle;
  return NS_OK;
}

/* attribute PRInt32 accessCount; */
NS_IMETHODIMP nsNavHistoryResultNode::GetAccessCount(PRInt32 *aAccessCount)
{
  *aAccessCount = mAccessCount;
  return NS_OK;
}

/* attribute PRTime time; */
NS_IMETHODIMP nsNavHistoryResultNode::GetTime(PRTime *aTime)
{
  *aTime = mTime;
  return NS_OK;
}

/* attribute pRInt32 indentLevel; */
NS_IMETHODIMP nsNavHistoryResultNode::GetIndentLevel(PRInt32* aIndentLevel)
{
  *aIndentLevel = mIndentLevel;
  return NS_OK;
}

/* attribute PRInt32 childCount; */
NS_IMETHODIMP nsNavHistoryResultNode::GetChildCount(PRInt32* aChildCount)
{
  *aChildCount = mChildren.Count();
  return NS_OK;
}

/* nsINavHistoryResultNode getChild(in PRInt32 index); */
NS_IMETHODIMP nsNavHistoryResultNode::GetChild(PRInt32 aIndex,
                                               nsINavHistoryResultNode** _retval)
{
  if (aIndex < 0 || aIndex >= mChildren.Count())
    return NS_ERROR_INVALID_ARG;
  *_retval = mChildren[aIndex];
  (*_retval)->AddRef();
  return NS_OK;
}

// nsINavHistoryQuery **********************************************************

NS_IMPL_ISUPPORTS1(nsNavHistoryQuery, nsINavHistoryQuery)

// nsINavHistoryQuery::nsNavHistoryQuery
//
//    This must initialize the object such that the default values will cause
//    all history to be returned if this query is used. Then the caller can
//    just set the things it's interested in.

nsNavHistoryQuery::nsNavHistoryQuery()
  : mBeginTime(0), mEndTime(0), mOnlyBookmarked(PR_FALSE),
    mDomainIsHost(PR_FALSE)
{
}

/* attribute PRTime beginTime; */
NS_IMETHODIMP nsNavHistoryQuery::GetBeginTime(PRTime *aBeginTime)
{
  *aBeginTime = mBeginTime;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetBeginTime(PRTime aBeginTime)
{
  mBeginTime = aBeginTime;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasBeginTime(PRBool* _retval)
{
  *_retval = (mBeginTime > 0);
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetBeginDate(PRInt32 year, PRInt32 month,
                                              PRInt32 day)
{
  // FIXME
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute PRTime endTime; */
NS_IMETHODIMP nsNavHistoryQuery::GetEndTime(PRTime *aEndTime)
{
  *aEndTime = mEndTime;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetEndTime(PRTime aEndTime)
{
  mEndTime = aEndTime;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasEndTime(PRBool* _retval)
{
  *_retval = (mEndTime > 0);
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetEndDate(PRInt32 year, PRInt32 month,
                                            PRInt32 day)
{
  // FIXME
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute string searchTerms; */
NS_IMETHODIMP nsNavHistoryQuery::GetSearchTerms(nsAString& aSearchTerms)
{
  aSearchTerms = mSearchTerms;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetSearchTerms(const nsAString& aSearchTerms)
{
  mSearchTerms = aSearchTerms;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasSearchTerms(PRBool* _retval)
{
  *_retval = (! mSearchTerms.IsEmpty());
  return NS_OK;
}

/* attribute boolean onlyBookmarked; */
NS_IMETHODIMP nsNavHistoryQuery::GetOnlyBookmarked(PRBool *aOnlyBookmarked)
{
  *aOnlyBookmarked = mOnlyBookmarked;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetOnlyBookmarked(PRBool aOnlyBookmarked)
{
  mOnlyBookmarked = aOnlyBookmarked;
  return NS_OK;
}

/* attribute boolean domainIsHost; */
NS_IMETHODIMP nsNavHistoryQuery::GetDomainIsHost(PRBool *aDomainIsHost)
{
  *aDomainIsHost = mDomainIsHost;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetDomainIsHost(PRBool aDomainIsHost)
{
  mDomainIsHost = aDomainIsHost;
  return NS_OK;
}

/* attribute string domain; */
NS_IMETHODIMP nsNavHistoryQuery::GetDomain(nsAString& aDomain)
{
  aDomain = mDomain;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetDomain(const nsAString& aDomain)
{
  mDomain = aDomain;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasDomain(PRBool* _retval)
{
  *_retval = (! mDomain.IsEmpty());
  return NS_OK;
}


// nsNavHistoryResult **********************************************************

NS_IMPL_ISUPPORTS2(nsNavHistoryResult,
                   nsINavHistoryResult,
                   nsITreeView)


// nsNavHistoryResult::nsNavHistoryResult

nsNavHistoryResult::nsNavHistoryResult(nsNavHistory* aHistoryService,
                                       nsIStringBundle* aHistoryBundle)
  : mBundle(aHistoryBundle), mHistoryService(aHistoryService),
    mCollapseDuplicates(PR_TRUE),
    mTimesIncludeDates(PR_TRUE), mCurrentSort(nsNavHistory::SORT_BY_NONE)
{
}

// nsNavHistoryResult::~nsNavHistoryResult

nsNavHistoryResult::~nsNavHistoryResult()
{
}


// nsNavHistoryResult::Init

nsresult
nsNavHistoryResult::Init()
{
  nsresult rv;
  nsCOMPtr<nsILocaleService> ls = do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ls->GetApplicationLocale(getter_AddRefs(mLocale));
  NS_ENSURE_SUCCESS(rv, rv);

  // collation service for sorting
  static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
  nsCOMPtr<nsICollationFactory> cfact = do_CreateInstance(
     kCollationFactoryCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  cfact->CreateCollation(mLocale, getter_AddRefs(mCollation));

  // date time formatting
  static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
  mDateFormatter = do_CreateInstance(kDateTimeFormatCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavHistoryResult::FilledAllResults

void
nsNavHistoryResult::FilledAllResults()
{
  for (PRInt32 i = 0; i < mTopLevelElements.Count(); i ++) {
    mTopLevelElements[i]->mParent = nsnull;
    FillTreeStats(mTopLevelElements[i], 0);
  }
  RebuildAllListRecurse(mTopLevelElements);
  InitializeVisibleList();
}


// nsNavHistoryResult::GetTopLevelNodeCount

NS_IMETHODIMP nsNavHistoryResult::GetTopLevelNodeCount(PRInt32* aCount)
{
  *aCount = mTopLevelElements.Count();
  return NS_OK;
}


// nsNavHistoryResult::GetTopLevelNode

NS_IMETHODIMP nsNavHistoryResult::GetTopLevelNode(PRInt32 aIndex,
                                               nsINavHistoryResultNode** _retval)
{
  if (aIndex < 0 || aIndex >= mTopLevelElements.Count())
    return NS_ERROR_INVALID_ARG;
  *_retval = mTopLevelElements[aIndex];
  NS_ADDREF(*_retval);
  return NS_OK;
}


// nsNavHistoryResult::GetTopLevel

NS_IMETHODIMP
nsNavHistoryResult::GetTopLevel(nsIArray** aTopLevel)
{
  nsCOMPtr<nsIMutableArray> mutableArray;
  nsresult rv = NS_NewArray(getter_AddRefs(mutableArray), mTopLevelElements);
  NS_ENSURE_SUCCESS(rv, rv);

  *aTopLevel = mutableArray;
  NS_ADDREF(*aTopLevel);
  return NS_OK;
}


// nsNavHistoryResult::RecursiveSort

NS_IMETHODIMP
nsNavHistoryResult::RecursiveSort(PRUint32 aSortingMode)
{
  if (aSortingMode > nsNavHistory::SORT_BY_VISITCOUNT_DESCENDING)
    return NS_ERROR_INVALID_ARG;

  mCurrentSort = aSortingMode;
  RecursiveSortArray(mTopLevelElements, aSortingMode);

  // This sorting function is called from two contexts. First, when everything
  // is being built, executeQueries will do a sort and then call
  // FilledAllResults to build up all the bookkeeping. In this case, we don't
  // need to do anything more since everything will be built later.
  //
  // Second, anybody can call this function to cause a resort. In this case,
  // we need to rebuild the visible element lists and notify the tree box object
  // of the change. The way to disambiguate these cases is whether the flatted
  // mAllElements list has been generated yet.
  if (mAllElements.Count() > 0)
    RebuildList();

  // update the UI on the tree columns
  if (mTree)
    SetTreeSortingIndicator();
  return NS_OK;
}


// nsNavHistoryResult::Get/SetTimesIncludeDates

NS_IMETHODIMP nsNavHistoryResult::SetTimesIncludeDates(PRBool aValue)
{
  mTimesIncludeDates = aValue;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryResult::GetTimesIncludeDates(PRBool* aValue)
{
  *aValue = mTimesIncludeDates;
  return NS_OK;
}


// nsNavHistoryResult::Get/SetCollapseDuplicates

NS_IMETHODIMP nsNavHistoryResult::SetCollapseDuplicates(PRBool aValue)
{
  PRBool oldValue = mCollapseDuplicates;
  mCollapseDuplicates = aValue;
  if (oldValue != mCollapseDuplicates && mAllElements.Count() > 0)
    RebuildList();
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryResult::GetCollapseDuplicates(PRBool* aValue)
{
  *aValue = mCollapseDuplicates;
  return NS_OK;
}


// nsNavHistoryResult::RecursiveSortArray

void
nsNavHistoryResult::RecursiveSortArray(
    nsCOMArray<nsNavHistoryResultNode>& aSources, PRUint32 aSortingMode)
{
  switch (aSortingMode)
  {
    case nsNavHistory::SORT_BY_NONE:
      break;
    case nsNavHistory::SORT_BY_TITLE_ASCENDING:
      aSources.Sort(SortComparison_TitleLess, NS_STATIC_CAST(void*, this));
      break;
    case nsNavHistory::SORT_BY_TITLE_DESCENDING:
      aSources.Sort(SortComparison_TitleGreater, NS_STATIC_CAST(void*, this));
      break;
    case nsNavHistory::SORT_BY_DATE_ASCENDING:
      aSources.Sort(SortComparison_DateLess, NS_STATIC_CAST(void*, this));
      break;
    case nsNavHistory::SORT_BY_DATE_DESCENDING:
      aSources.Sort(SortComparison_DateGreater, NS_STATIC_CAST(void*, this));
      break;
    case nsNavHistory::SORT_BY_URL_ASCENDING:
      aSources.Sort(SortComparison_URLLess, NS_STATIC_CAST(void*, this));
      break;
    case nsNavHistory::SORT_BY_URL_DESCENDING:
      aSources.Sort(SortComparison_URLGreater, NS_STATIC_CAST(void*, this));
      break;
    case nsNavHistory::SORT_BY_VISITCOUNT_ASCENDING:
      aSources.Sort(SortComparison_VisitCountLess, NS_STATIC_CAST(void*, this));
      break;
    case nsNavHistory::SORT_BY_VISITCOUNT_DESCENDING:
      aSources.Sort(SortComparison_VisitCountGreater, NS_STATIC_CAST(void*, this));
      break;
    default:
      NS_NOTREACHED("Bad sorting type");
      break;
  }

  // sort any children
  for (PRInt32 i = 0; i < aSources.Count(); i ++) {
    if (aSources[i]->mChildren.Count() > 0)
      RecursiveSortArray(aSources[i]->mChildren, aSortingMode);
  }
}


// nsNavHistoryResult::ApplyTreeState
//

void nsNavHistoryResult::ApplyTreeState(
    const nsDataHashtable<nsStringHashKey, int>& aExpanded)
{
  RecursiveApplyTreeState(mTopLevelElements, aExpanded);

  // If the list has been build yet, we need to redo the visible list.
  // Normally, this function is called during object creation, and we don't
  // need to worry about the list of visible nodes.
  if (mAllElements.Count() > 0)
    RebuildList();
}


// nsNavHistoryResult::RecursiveApplyTreeState
//
//    See ApplyTreeState

void
nsNavHistoryResult::RecursiveApplyTreeState(
    nsCOMArray<nsNavHistoryResultNode>& aList,
    const nsDataHashtable<nsStringHashKey, int>& aExpanded)
{
  for (PRInt32 i = 0; i < aList.Count(); i ++) {
    if (aList[i]->mChildren.Count()) {
      PRInt32 beenExpanded = 0;
      if (aExpanded.Get(aList[i]->mTitle, &beenExpanded) && beenExpanded)
        aList[i]->mExpanded = PR_TRUE;
      else
        aList[i]->mExpanded = PR_FALSE;
      RecursiveApplyTreeState(aList[i]->mChildren, aExpanded);
    }
  }
}


// nsNavHistoryResult::ExpandAll

NS_IMETHODIMP
nsNavHistoryResult::ExpandAll()
{
  RecursiveExpandCollapse(mTopLevelElements, PR_TRUE);
  RebuildList();
  return NS_OK;
}


// nsNavHistoryResult::CollapseAll

NS_IMETHODIMP
nsNavHistoryResult::CollapseAll()
{
  RecursiveExpandCollapse(mTopLevelElements, PR_FALSE);
  RebuildList();
  return NS_OK;
}


// nsNavHistoryResult::RecursiveExpandCollapse
//
//    aExpand = true  -> expand all
//    aExpand = false -> collapse all

void
nsNavHistoryResult::RecursiveExpandCollapse(
    nsCOMArray<nsNavHistoryResultNode>& aList, PRBool aExpand)
{
  for (PRInt32 i = 0; i < aList.Count(); i ++) {
    if (aList[i]->mChildren.Count()) {
      aList[i]->mExpanded = aExpand;
      RecursiveExpandCollapse(aList[i]->mChildren, aExpand);
    }
  }
}


// nsNavHistoryResult::NodeForTreeIndex

NS_IMETHODIMP
nsNavHistoryResult::NodeForTreeIndex(PRUint32 index,
                                     nsINavHistoryResultNode** aResult)
{
  if (index >= (PRUint32)mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  *aResult = VisibleElementAt(index);
  (*aResult)->AddRef();
  return NS_OK;
}


// nsNavHistoryResult::SetTreeSortingIndicator
//
//    This is called to assign the correct arrow to the column header. It is
//    called both when you resort, and when a tree is first attached (to
//    initialize it's headers).

void
nsNavHistoryResult::SetTreeSortingIndicator()
{
  nsCOMPtr<nsITreeColumns> columns;
  nsresult rv = mTree->GetColumns(getter_AddRefs(columns));
  if (NS_FAILED(rv)) return;

  NS_NAMED_LITERAL_STRING(sortDirectionKey, "sortDirection");

  // clear old sorting indicator. Watch out: GetSortedColumn returns NS_OK
  // but null element if there is no sorted element
  nsCOMPtr<nsITreeColumn> column;
  rv = columns->GetSortedColumn(getter_AddRefs(column));
  if (NS_FAILED(rv)) return;
  if (column) {
    nsCOMPtr<nsIDOMElement> element;
    rv = column->GetElement(getter_AddRefs(element));
    if (NS_FAILED(rv)) return;
    element->SetAttribute(sortDirectionKey, NS_LITERAL_STRING(""));
  }

  // set new sorting indicator by looking through all columns for ours
  if (mCurrentSort == nsNavHistory::SORT_BY_NONE)
    return;
  PRBool desiredIsDescending;
  ColumnType desiredColumn = SortTypeToColumnType(mCurrentSort,
                                                  &desiredIsDescending);
  PRInt32 colCount;
  rv = columns->GetCount(&colCount);
  if (NS_FAILED(rv)) return;
  for (PRInt32 i = 0; i < colCount; i ++) {
    columns->GetColumnAt(i, getter_AddRefs(column));
    if (NS_FAILED(rv)) return;
    if (GetColumnType(column) == desiredColumn) {
      // found our desired one, set
      nsCOMPtr<nsIDOMElement> element;
      rv = column->GetElement(getter_AddRefs(element));
      if (NS_FAILED(rv)) return;
      if (desiredIsDescending)
        element->SetAttribute(sortDirectionKey, NS_LITERAL_STRING("descending"));
      else
        element->SetAttribute(sortDirectionKey, NS_LITERAL_STRING("ascending"));
      break;
    }
  }
}


// nsNavHistoryResult::SortComparison_Title*
//
//    These are a little more complicated because they do a localization
//    conversion. If this is too slow, we can compute the sort keys once in
//    advance, sort that array, and then reorder the real array accordingly.
//    This would save some key generations.
//
//    The collation object must be allocated before sorting on title!

PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_TitleLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  nsNavHistoryResult* result = NS_STATIC_CAST(nsNavHistoryResult*, closure);
  PRInt32 value = -1; // default to returning "true" on failure
  result->mCollation->CompareString(
      nsICollation::kCollationCaseInSensitive, a->mTitle, b->mTitle, &value);
  if (value == 0) {
    // resolve by URL
    value = a->mUrl.Compare(PromiseFlatCString(b->mUrl).get());
    if (value == 0) {
      // resolve by date
      return ComparePRTime(a->mTime, b->mTime);
    }
  }
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_TitleGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_TitleLess(a, b, closure);
}


// nsNavHistoryResult::SortComparison_Date*
//
//    Don't bother doing conflict resolution. Since dates are stored in
//    microseconds, it will be very difficult to get collisions. This would be
//    most likely for imported history, which I'm not too worried about.

PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_DateLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return ComparePRTime(a->mTime, b->mTime);
}
PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_DateGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -ComparePRTime(a->mTime, b->mTime);
}


// nsNavHistoryResult::SortComparison_URL*
//
//    Certain types of parent nodes are treated specially because URLs are not
//    meaningful.

PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_URLLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  if (a->mType != b->mType) {
    // we really shouldn't be comparing different row types
    NS_NOTREACHED("Comparing different row types!");
    return 0;
  }

  PRInt32 value;
  if (a->mType == nsINavHistoryResultNode::RESULT_TYPE_HOST) {
    // for host nodes, use title (= host name)
    nsNavHistoryResult* result = NS_STATIC_CAST(nsNavHistoryResult*, closure);
    result->mCollation->CompareString(
        nsICollation::kCollationCaseInSensitive, a->mTitle, b->mTitle, &value);
  } else if (a->mType == nsINavHistoryResultNode::RESULT_TYPE_DAY) {
    // date nodes use date (skip conflict resolution becuase it uses date too)
    return ComparePRTime(a->mTime, b->mTime);
  } else {
    // normal URL or visit
    value = a->mUrl.Compare(PromiseFlatCString(b->mUrl).get());
  }

  // resolve conflicts using date
  if (value == 0) {
    return ComparePRTime(a->mTime, b->mTime);
  }
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_URLGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_URLLess(a, b, closure);
}


// nsNavHistory::SortComparison_VisitCount*
//
//    Fall back on dates for conflict resolution

PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_VisitCountLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = CompareIntegers(a->mAccessCount, b->mAccessCount);
  if (value == 0)
    return ComparePRTime(a->mTime, b->mTime);
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_VisitCountGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = -CompareIntegers(a->mAccessCount, b->mAccessCount);
  if (value == 0)
    return -ComparePRTime(a->mTime, b->mTime);
  return value;
}


// nsNavHistoryResult::FillTreeStats
//
//    This basically does a recursive depth-first traversal of the tree to fill
//    in bookeeping information and statistics for parent nodes.

void
nsNavHistoryResult::FillTreeStats(nsNavHistoryResultNode* aResult, PRInt32 aLevel)
{
  aResult->mIndentLevel = aLevel;
  if (aResult->mChildren.Count() > 0) {
    PRInt32 totalAccessCount = 0;
    PRTime mostRecentTime = 0;
    for (PRInt32 i = 0; i < aResult->mChildren.Count(); i ++ ) {
      nsNavHistoryResultNode* child = NS_STATIC_CAST(
          nsNavHistoryResultNode*, aResult->mChildren[i]);

      child->mParent = aResult;
      FillTreeStats(aResult->mChildren[i], aLevel + 1);

      totalAccessCount += child->mAccessCount;
      if (LL_CMP(child->mTime, >, mostRecentTime))
        mostRecentTime = child->mTime;
    }

    // when there are children, the parent is the aggregate of its children
    aResult->mAccessCount = totalAccessCount;
    aResult->mTime = mostRecentTime;
  }
}


// nsNavHistoryResult::InitializeVisibleList

void
nsNavHistoryResult::InitializeVisibleList()
{
  NS_ASSERTION(mVisibleElements.Count() == 0, "Initializing visible list twice");

  // Fill the visible element list and notify those elements of their new
  // positions. We fill directly into the result list, so we need to manually
  // set the visible indices on those elements (normally this is done by
  // InsertVisibleSection)
  BuildVisibleSection(mTopLevelElements, &mVisibleElements);
  for (PRInt32 i = 0; i < mVisibleElements.Count(); i ++)
    VisibleElementAt(i)->mVisibleIndex = i;
}


// nsNavHistoryResult::RebuildList
//
//    Called when something big changes, like sorting. This rebuilds the
//    flat and visible element lists using the current toplevel array.

void
nsNavHistoryResult::RebuildList()
{
  PRInt32 oldVisibleCount = mVisibleElements.Count();

  mAllElements.Clear();
  mVisibleElements.Clear();
  RebuildAllListRecurse(mTopLevelElements);
  InitializeVisibleList();

  // We need to update the tree to tell it about the new list
  if (mTree) {
    if (mVisibleElements.Count() != oldVisibleCount)
      mTree->RowCountChanged(0, mVisibleElements.Count() - oldVisibleCount);
    mTree->InvalidateRange(0, mVisibleElements.Count());
  }
}


// nsNavHistoryResult::RebuildAllListRecurse
//
//    Does the work for RebuildList in generating the new mAllElements list

void
nsNavHistoryResult::RebuildAllListRecurse(
    const nsCOMArray<nsNavHistoryResultNode>& aSource)
{
  for (PRInt32 i = 0; i < aSource.Count(); i ++) {
    PRUint32 allCount = mAllElements.Count();
    if (mCollapseDuplicates && allCount > 0 && aSource[i]->mID != 0 &&
        AllElementAt(allCount - 1)->mID == aSource[i]->mID) {
      // ignore duplicate, that elements' flat index is the index of its dup
      // note, we don't collapse ID=0 elements since that is all parent nodes
      aSource[i]->mFlatIndex = allCount - 1;
    } else {
      aSource[i]->mFlatIndex = mAllElements.Count();
      mAllElements.AppendElement(aSource[i]);
      if (aSource[i]->mChildren.Count() > 0)
        RebuildAllListRecurse(aSource[i]->mChildren);
    }
  }
}

// nsNavHistoryResult::BuildVisibleSection
//
//    This takes all nodes in the given source array and recursively (when
//    containers are open) appends them to the flattened visible result array.
//    This is used when expanding folders and in initially building the
//    visible array list.
//
//    This will also collapse duplicate elements. As long as this is called for
//    an entire subtree, it is OK, since duplicate elements should not
//    occur next to each other at different levels of the tree.

void
nsNavHistoryResult::BuildVisibleSection(
    const nsCOMArray<nsNavHistoryResultNode>& aSources, nsVoidArray* aVisible)
{
  for (PRInt32 i = 0; i < aSources.Count(); i ++) {
    nsNavHistoryResultNode* cur = aSources[i];
    if (mCollapseDuplicates && aVisible->Count() > 0 && aSources[i]->mID != 0) {
      nsNavHistoryResultNode* prev =
        (nsNavHistoryResultNode*)(*aVisible)[aVisible->Count() - 1];
      if (prev->mID == cur->mID)
        continue; // collapse duplicate
    }

    aVisible->AppendElement(cur);
    if (cur->mChildren.Count() > 0 && cur->mExpanded)
      BuildVisibleSection(cur->mChildren, aVisible);
  }
}


// nsNavHistoryResult::InsertVisibleSection
//
//    This takes a list of items (probably generated by BuildVisibleSection)
//    and inserts it into the visible element list, updating those elements as
//    needed. This is used when expanding the tree.

void
nsNavHistoryResult::InsertVisibleSection(const nsVoidArray& aAddition,
                                         PRInt32 aInsertHere)
{
  NS_ASSERTION(aInsertHere >= 0 && aInsertHere <= mVisibleElements.Count(),
               "Invalid insertion point");
  mVisibleElements.InsertElementsAt(aAddition, aInsertHere);

  // we need to update all the elements from the insertion point to the end
  // of the list of their new indices
  for (PRInt32 i = aInsertHere; i < mVisibleElements.Count(); i ++)
    VisibleElementAt(i)->mVisibleIndex = i;
}


// nsNavHistoryResult::DeleteVisibleChildrenOf
//
//    This is called when an element is collapsed. We search for all elements
//    that are at a greater level of indent and remove them from the visible
//    element list. Returns the number of rows deleted

int
nsNavHistoryResult::DeleteVisibleChildrenOf(PRInt32 aIndex)
{
  NS_ASSERTION(aIndex >= 0 && aIndex < mVisibleElements.Count(),
               "Index out of bounds");

  const nsNavHistoryResultNode* parentNode = VisibleElementAt(aIndex);
  NS_ASSERTION(parentNode->mChildren.Count() > 0 && parentNode->mExpanded,
               "Trying to collapse an improper node");

  // compute the index of the element just after the end of the deleted region
  PRInt32 outerLevel = parentNode->mIndentLevel;
  PRInt32 nextOuterIndex = mVisibleElements.Count();
  PRInt32 i;
  for (i = aIndex + 1; i < mVisibleElements.Count(); i ++) {
    if (VisibleElementAt(i)->mIndentLevel <= outerLevel) {
      nextOuterIndex = i;
      break;
    }
  }

  // Mark those elements as invisible and remove them.
  for (i = aIndex + 1; i < nextOuterIndex; i ++)
    VisibleElementAt(i)->mVisibleIndex = -1;
  PRInt32 deleteCount = nextOuterIndex - aIndex - 1;
  mVisibleElements.RemoveElementsAt(aIndex + 1, deleteCount);

  // re-number the moved elements
  for (i = aIndex + 1; i < mVisibleElements.Count(); i ++)
    VisibleElementAt(i)->mVisibleIndex = i;

  return deleteCount;
}


// nsNavHistoryResult::GetRowCount (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = mVisibleElements.Count();
  return NS_OK;
}


// nsNavHistoryResult::Get/SetSelection (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetSelection(nsITreeSelection** aSelection)
{
  if (! mSelection) {
    *aSelection = nsnull;
    return NS_ERROR_FAILURE;
  }
  *aSelection = mSelection;
  (*aSelection)->AddRef();
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryResult::SetSelection(nsITreeSelection* aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

/* void getRowProperties (in long index, in nsISupportsArray properties); */
NS_IMETHODIMP nsNavHistoryResult::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getCellProperties (in long row, in nsITreeColumn col, in nsISupportsArray properties); */
NS_IMETHODIMP nsNavHistoryResult::GetCellProperties(PRInt32 row, nsITreeColumn *col, nsISupportsArray *properties)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getColumnProperties (in nsITreeColumn col, in nsISupportsArray properties); */
NS_IMETHODIMP nsNavHistoryResult::GetColumnProperties(nsITreeColumn *col, nsISupportsArray *properties)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::IsContainer (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsContainer(PRInt32 index, PRBool *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  *_retval = (VisibleElementAt(index)->mChildren.Count() > 0);
  return NS_OK;
}


// nsNavHistoryResult::IsContainerOpen (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  *_retval = VisibleElementAt(index)->mExpanded;
  return NS_OK;
}


// nsNavHistoryResult::IsContainerEmpty (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  *_retval = (VisibleElementAt(index)->mChildren.Count() == 0);
  return NS_OK;
}


// nsNavHistoryResult::IsSeparator (nsITreeView)
//
//    We don't support separators

NS_IMETHODIMP nsNavHistoryResult::IsSeparator(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean isSorted (); */
NS_IMETHODIMP nsNavHistoryResult::IsSorted(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::CanDrop (nsITreeView)
//
//    No drag-and-drop to history.

NS_IMETHODIMP nsNavHistoryResult::CanDrop(PRInt32 index, PRInt32 orientation,
                                          PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}


// nsNavHistoryResult::Drop (nsITreeView)
//
//    No drag-and-drop to history.

NS_IMETHODIMP nsNavHistoryResult::Drop(PRInt32 row, PRInt32 orientation)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::GetParentIndex (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetParentIndex(PRInt32 index, PRInt32 *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  nsNavHistoryResultNode* parent = VisibleElementAt(index)->mParent;
  if (!parent || parent->mVisibleIndex < 0)
    *_retval = -1;
  else
    *_retval = parent->mVisibleIndex;
  return NS_OK;
}


// nsNavHistoryResult::HasNextSibling (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::HasNextSibling(PRInt32 index,
                                                 PRInt32 afterIndex,
                                                 PRBool *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  if (index == mVisibleElements.Count() - 1) {
    // this is the last thing in the list -> no next sibling
    *_retval = PR_FALSE;
    return NS_OK;
  }
  *_retval = (VisibleElementAt(index)->mIndentLevel ==
              VisibleElementAt(index + 1)->mIndentLevel);
  return NS_OK;
}


// nsNavHistoryResult::GetLevel (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  *_retval = ((nsNavHistoryResultNode*)mVisibleElements[index])->mIndentLevel;
  return NS_OK;
}


// nsNavHistoryResult::GetImageSrc (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetImageSrc(PRInt32 row, nsITreeColumn *col,
                                              nsAString& _retval)
{
  _retval.Truncate(0);
  return NS_OK;
}

/* long getProgressMode (in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::GetProgressMode(PRInt32 row, nsITreeColumn *col, PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* AString getCellValue (in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::GetCellValue(PRInt32 row, nsITreeColumn *col, nsAString & _retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::GetCellText (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetCellText(PRInt32 rowIndex,
                                              nsITreeColumn *col,
                                              nsAString& _retval)
{
  if (rowIndex < 0 || rowIndex >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  PRInt32 colIndex;
  col->GetIndex(&colIndex);

  nsNavHistoryResultNode* elt = VisibleElementAt(rowIndex);

  switch (GetColumnType(col)) {
    case Column_Title:
    {
      // title: normally, this is just the title, but we don't want empty
      // items in the tree view so return a special string if the title is
      // empty. Do it here so that callers can still get at the 0 length title
      // if they go through the "result" API.
      if (! elt->mTitle.IsEmpty()) {
        _retval = elt->mTitle;
      } else {
        nsXPIDLString value;
        nsresult rv = mBundle->GetStringFromName(
            NS_LITERAL_STRING("noTitle").get(), getter_Copies(value));
        NS_ENSURE_SUCCESS(rv, rv);
        _retval = value;
      }
      break;
    }
    case Column_URL:
    {
      _retval = NS_ConvertUTF8toUTF16(elt->mUrl);
      break;
    }
    case Column_Date:
    {
      if (elt->mType == nsINavHistoryResultNode::RESULT_TYPE_HOST ||
          elt->mType == nsINavHistoryResultNode::RESULT_TYPE_DAY) {
        // hosts and days shouldn't have a value for the date column. Actually,
        // you could argue this point, but looking at the results, seeing the
        // most recently visited date is not what I expect, and gives me no
        // information I know how to use.
        _retval.Truncate(0);
      } else {
        nsDateFormatSelector dateFormat;
        if (mTimesIncludeDates)
          dateFormat = nsIScriptableDateFormat::dateFormatShort;
        else
          dateFormat = nsIScriptableDateFormat::dateFormatNone;
        nsAutoString realString; // stupid function won't take an nsAString
        mDateFormatter->FormatPRTime(mLocale, dateFormat,
                                     nsIScriptableDateFormat::timeFormatNoSeconds,
                                     elt->mTime, realString);
        _retval = realString;
      }
      break;
    }
    case Column_VisitCount:
    {
      _retval = NS_ConvertUTF8toUTF16(nsPrintfCString("%d", elt->mAccessCount));
      break;
    }
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}


// nsNavHistoryResult::SetTree (nsITreeView)
//
//    This will get called when this view is attached to the tree to tell us
//    the box object, AND when the tree is being destroyed, with NULL.

NS_IMETHODIMP nsNavHistoryResult::SetTree(nsITreeBoxObject *tree)
{
  mTree = tree;
  if (mTree) // might be a NULL tree
    SetTreeSortingIndicator();
  return NS_OK;
}


// nsNavHistoryResult::ToggleOpenState (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::ToggleOpenState(PRInt32 index)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  nsNavHistoryResultNode* curNode = VisibleElementAt(index);
  if (curNode->mExpanded) {
    // collapse
    PRInt32 deleteCount = DeleteVisibleChildrenOf(index);
    curNode->mExpanded = PR_FALSE;
    if (mTree)
      mTree->RowCountChanged(index + 1, -deleteCount);

    mHistoryService->SaveCollapseItem(curNode->mTitle);
  } else {
    // expand
    nsVoidArray addition;
    BuildVisibleSection(curNode->mChildren, &addition);
    InsertVisibleSection(addition, index + 1);

    curNode->mExpanded = PR_TRUE;
    if (mTree)
      mTree->RowCountChanged(index + 1, addition.Count());

    mHistoryService->SaveExpandItem(curNode->mTitle);
  }
  /* Maybe we don't need this (test me)
  if (mTree)
    mTree->InvalidateRow(index);
  */
  return NS_OK;
}


// nsNavHistoryResult::CycleHeader (nsITreeView)
//
//    If we already sorted by that column, the sorting is reversed, otherwise
//    we use the default sorting direction for that data type.

NS_IMETHODIMP nsNavHistoryResult::CycleHeader(nsITreeColumn *col)
{
  PRInt32 colIndex;
  col->GetIndex(&colIndex);

  PRInt32 newSort;
  switch (GetColumnType(col)) {
    case Column_Title:
      if (mCurrentSort == nsNavHistory::SORT_BY_TITLE_ASCENDING)
        newSort = nsNavHistory::SORT_BY_TITLE_DESCENDING;
      else
        newSort = nsNavHistory::SORT_BY_TITLE_ASCENDING;
      break;
    case Column_URL:
      if (mCurrentSort == nsNavHistory::SORT_BY_URL_ASCENDING)
        newSort = nsNavHistory::SORT_BY_URL_DESCENDING;
      else
        newSort = nsNavHistory::SORT_BY_URL_ASCENDING;
      break;
    case Column_Date:
      if (mCurrentSort == nsNavHistory::SORT_BY_DATE_ASCENDING)
        newSort = nsNavHistory::SORT_BY_DATE_DESCENDING;
      else
        newSort = nsNavHistory::SORT_BY_DATE_ASCENDING;
      break;
    case Column_VisitCount:
      // visit count default is unusual because it is descending
      if (mCurrentSort == nsNavHistory::SORT_BY_VISITCOUNT_DESCENDING)
        newSort = nsNavHistory::SORT_BY_VISITCOUNT_ASCENDING;
      else
        newSort = nsNavHistory::SORT_BY_VISITCOUNT_DESCENDING;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return RecursiveSort(newSort);
}

/* void selectionChanged (); */
NS_IMETHODIMP nsNavHistoryResult::SelectionChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cycleCell (in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::CycleCell(PRInt32 row, nsITreeColumn *col)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isEditable (in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::IsEditable(PRInt32 row, nsITreeColumn *col, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setCellValue (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP nsNavHistoryResult::SetCellValue(PRInt32 row, nsITreeColumn *col, const nsAString & value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setCellText (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP nsNavHistoryResult::SetCellText(PRInt32 row, nsITreeColumn *col, const nsAString & value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void performAction (in wstring action); */
NS_IMETHODIMP nsNavHistoryResult::PerformAction(const PRUnichar *action)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void performActionOnRow (in wstring action, in long row); */
NS_IMETHODIMP nsNavHistoryResult::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void performActionOnCell (in wstring action, in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::PerformActionOnCell(const PRUnichar *action, PRInt32 row, nsITreeColumn *col)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::GetColumnType
//

nsNavHistoryResult::ColumnType
nsNavHistoryResult::GetColumnType(nsITreeColumn* col)
{
  const PRUnichar* idConst;
  col->GetIdConst(&idConst);
  switch(idConst[0]) {
    case 't':
      return Column_Title;
    case 'u':
      return Column_URL;
    case 'd':
      return Column_Date;
    case 'v':
      return Column_VisitCount;
    default:
      return Column_Unknown;
  }
}


// nsNavHistoryResult::SortTypeToColumnType
//
//    Places whether its ascending or descending into the given
//    boolean buffer

nsNavHistoryResult::ColumnType
nsNavHistoryResult::SortTypeToColumnType(PRUint32 aSortType,
                                         PRBool* aDescending)
{
  switch(aSortType) {
    case nsINavHistory::SORT_BY_TITLE_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_Title;
    case nsINavHistory::SORT_BY_TITLE_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_Title;
    case nsINavHistory::SORT_BY_DATE_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_Date;
    case nsINavHistory::SORT_BY_DATE_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_Date;
    case nsINavHistory::SORT_BY_URL_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_URL;
    case nsINavHistory::SORT_BY_URL_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_URL;
    case nsINavHistory::SORT_BY_VISITCOUNT_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_VisitCount;
    case nsINavHistory::SORT_BY_VISITCOUNT_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_VisitCount;
    default:
      return Column_Unknown;
  }
}
