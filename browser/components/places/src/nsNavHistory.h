/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
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

#ifndef nsNavHistory_h_
#define nsNavHistory_h_

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageStatement.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsIAtom.h"
#include "nsINavHistoryService.h"
#include "nsIAutoCompleteSearch.h"
#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteSimpleResult.h"
#include "nsIBrowserHistory.h"
#include "nsICollation.h"
#include "nsIDateTimeFormat.h"
#include "nsIGlobalHistory.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsIStringBundle.h"
#include "nsITimer.h"
#include "nsITreeSelection.h"
#include "nsITreeView.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "nsINavBookmarksService.h"
#include "nsMaybeWeakPtr.h"

#include "nsNavHistoryResult.h"
#include "nsNavHistoryQuery.h"

// Number of prefixes used in the autocomplete sort comparison function
#define AUTOCOMPLETE_PREFIX_LIST_COUNT 6
// Size of visit count boost to give to urls which are sites or paths
#define AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST 5

#define QUERYUPDATE_TIME 0
#define QUERYUPDATE_SIMPLE 1
#define QUERYUPDATE_COMPLEX 2
#define QUERYUPDATE_COMPLEX_WITH_BOOKMARKS 3

class AutoCompleteIntermediateResultSet;
class mozIAnnotationService;
class nsNavHistory;
class nsNavBookmarks;
class QueryKeyValuePair;

// nsNavHistory

class nsNavHistory : public nsSupportsWeakReference,
                     public nsINavHistoryService,
                     public nsIObserver,
                     public nsIBrowserHistory,
                     public nsIAutoCompleteSearch
{
  friend class AutoCompleteIntermediateResultSet;
public:
  nsNavHistory();

  NS_DECL_ISUPPORTS

  NS_DECL_NSINAVHISTORYSERVICE
  NS_DECL_NSIGLOBALHISTORY2
  NS_DECL_NSIBROWSERHISTORY
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIAUTOCOMPLETESEARCH

  nsresult Init();

  /**
   * Used by other components in the places directory such as the annotation
   * service to get a reference to this history object. Returns a pointer to
   * the service if it exists. Otherwise creates one. Returns NULL on error.
   */
  static nsNavHistory* GetHistoryService()
  {
    if (! gHistoryService) {
      nsresult rv;
      nsCOMPtr<nsINavHistoryService> serv(do_GetService("@mozilla.org/browser/nav-history-service;1", &rv));
      NS_ENSURE_SUCCESS(rv, nsnull);

      // our constructor should have set the static variable. If it didn't,
      // something is wrong.
      NS_ASSERTION(gHistoryService, "History service creation failed");
    }
    return gHistoryService;
  }

  nsresult GetUrlIdFor(nsIURI* aURI, PRInt64* aEntryID,
                       PRBool aAutoCreate);

  /**
   * Returns a pointer to the storage connection used by history. This connection
   * object is also used by the annotation service and bookmarks, so that
   * things can be grouped into transactions across these components.
   *
   * This connection can only be used in the thread that created it the
   * history service!
   */
  mozIStorageConnection* GetStorageConnection()
  {
    return mDBConn;
  }

  /**
   * These functions return non-owning references to the locale-specific
   * objects for places components. Guaranteed to return non-NULL.
   */
  nsIStringBundle* GetBundle()
    { return mBundle; }
  nsILocale* GetLocale()
    { return mLocale; }
  nsICollation* GetCollation()
    { return mCollation; }
  nsIDateTimeFormat* GetDateFormatter()
    { return mDateFormatter; }

  // remember tree state
  void SaveExpandItem(const nsAString& aTitle);
  void SaveCollapseItem(const nsAString& aTitle);

  // get the statement for selecting a history row by URL
  mozIStorageStatement* DBGetURLPageInfo() { return mDBGetURLPageInfo; }

  // Constants for the columns returned by the above statement.
  static const PRInt32 kGetInfoIndex_PageID;
  static const PRInt32 kGetInfoIndex_URL;
  static const PRInt32 kGetInfoIndex_Title;
  static const PRInt32 kGetInfoIndex_UserTitle;
  static const PRInt32 kGetInfoIndex_RevHost;
  static const PRInt32 kGetInfoIndex_VisitCount;

  // select a history row by URL, with visit date info (extra work)
  mozIStorageStatement* DBGetURLPageInfoFull()
  { return mDBGetURLPageInfoFull; }

  // select a history row by id, with visit date info (extra work)
  mozIStorageStatement* DBGetIdPageInfoFull()
  { return mDBGetIdPageInfoFull; }

  // Constants for the columns returned by the above statement
  // (in addition to the ones above).
  static const PRInt32 kGetInfoIndex_VisitDate;
  static const PRInt32 kGetInfoIndex_FaviconURL;

  // used in execute queries to get session ID info (only for visits)
  static const PRInt32 kGetInfoIndex_SessionId;

  static nsIAtom* sMenuRootAtom;
  static nsIAtom* sToolbarRootAtom;
  static nsIAtom* sSessionStartAtom;
  static nsIAtom* sSessionContinueAtom;

  // this actually executes a query and gives you results, it is used by
  // nsNavHistoryQueryResultNode
  nsresult GetQueryResults(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                           nsNavHistoryQueryOptions *aOptions,
                           nsCOMArray<nsNavHistoryResultNode>* aResults);

  // Take a row of kGetInfoIndex_* columns and construct a ResultNode.
  // The row must contain the full set of columns.
  nsresult RowToResult(mozIStorageValueArray* aRow,
                       nsNavHistoryQueryOptions* aOptions,
                       nsNavHistoryResultNode** aResult);
  nsresult QueryRowToResult(const nsACString& aURI, const nsACString& aTitle,
                            PRUint32 aAccessCount, PRTime aTime,
                            const nsACString& aFavicon,
                            nsNavHistoryResultNode** aNode);

  nsresult VisitIdToResultNode(PRInt64 visitId,
                               nsNavHistoryQueryOptions* aOptions,
                               nsNavHistoryResultNode** aResult);
  nsresult UriToResultNode(nsIURI* aUri,
                           nsNavHistoryQueryOptions* aOptions,
                           nsNavHistoryResultNode** aResult);

  // used by other places components to send history notifications (for example,
  // when the favicon has changed)
  void SendPageChangedNotification(nsIURI* aURI, PRUint32 aWhat,
                                   const nsAString& aValue)
  {
    ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryObserver,
                        OnPageChanged(aURI, aWhat, aValue));
  }

  // current time optimization
  PRTime GetNow();

  // well-known annotations used by the history and bookmarks systems
  static const char kAnnotationPreviousEncoding[];

  // used by query result nodes to update: see comment on body of CanLiveUpdateQuery
  static PRUint32 GetUpdateRequirements(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                                        nsNavHistoryQueryOptions* aOptions,
                                        PRBool* aHasSearchTerms);
  PRBool EvaluateQueryForNode(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                              nsNavHistoryQueryOptions* aOptions,
                              nsNavHistoryResultNode* aNode);

  static nsresult AsciiHostNameFromHostString(const nsACString& aHostName,
                                              nsACString& aAscii);
  static void DomainNameFromHostName(const nsCString& aHostName,
                                     nsACString& aDomainName);
  static PRTime NormalizeTime(PRUint32 aRelative, PRTime aOffset);
  nsresult RecursiveGroup(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                          const PRUint32* aGroupingMode, PRUint32 aGroupCount,
                          nsCOMArray<nsNavHistoryResultNode>* aDest);

  // better alternative to QueryStringToQueries (in nsNavHistoryQuery.cpp)
  nsresult QueryStringToQueryArray(const nsACString& aQueryString,
                                   nsCOMArray<nsNavHistoryQuery>* aQueries,
                                   nsNavHistoryQueryOptions** aOptions);

private:
  ~nsNavHistory();

  // used by GetHistoryService
  static nsNavHistory* gHistoryService;

protected:

  //
  // Constants
  //
  nsCOMPtr<nsIPrefService> gPrefService;
  nsCOMPtr<nsIPrefBranch> gPrefBranch;
  nsCOMPtr<nsIObserverService> gObserverService;
  nsDataHashtable<nsStringHashKey, int> gExpandedItems;

  //
  // Database stuff
  //
  nsCOMPtr<mozIStorageService> mDBService;
  nsCOMPtr<mozIStorageConnection> mDBConn;

  //nsCOMPtr<mozIStorageStatement> mDBGetVisitPageInfo; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBGetURLPageInfo;   // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBGetURLPageInfoFull; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBGetIdPageInfoFull; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBFullAutoComplete; // kAutoCompleteIndex_* results, 1 arg (max # results)
  static const PRInt32 kAutoCompleteIndex_URL;
  static const PRInt32 kAutoCompleteIndex_Title;
  static const PRInt32 kAutoCompleteIndex_UserTitle;
  static const PRInt32 kAutoCompleteIndex_VisitCount;
  static const PRInt32 kAutoCompleteIndex_Typed;

  nsCOMPtr<mozIStorageStatement> mDBRecentVisitOfURL; // converts URL into most recent visit ID/session ID
  nsCOMPtr<mozIStorageStatement> mDBInsertVisit; // used by AddVisit
  nsCOMPtr<mozIStorageStatement> mDBIncrementVisitCount;

  // these are used by VisitIdToResultNode for making new result nodes from IDs
  nsCOMPtr<mozIStorageStatement> mDBVisitToURLResult; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBVisitToVisitResult; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBUrlToUrlResult; // kGetInfoIndex_* results

  nsresult InitDB();

  // this is the cache DB in memory used for storing visited URLs
  nsCOMPtr<mozIStorageConnection> mMemDBConn;
  nsCOMPtr<mozIStorageStatement> mMemDBAddPage;
  nsCOMPtr<mozIStorageStatement> mMemDBGetPage;

  nsresult InitMemDB();

  nsresult InternalAdd(nsIURI* aURI, nsIURI* aReferrer, PRInt64 aSessionID,
                       PRUint32 aTransitionType, const PRUnichar* aTitle,
                       PRTime aVisitDate, PRBool aRedirect,
                       PRBool aToplevel, PRInt64* aPageID);
  nsresult InternalAddNewPage(nsIURI* aURI, const PRUnichar* aTitle,
                              PRBool aHidden, PRBool aTyped,
                              PRInt32 aVisitCount, PRInt64* aPageID);
  nsresult InternalAddVisit(nsIURI* aReferrer, PRInt64 aPageID, PRTime aTime,
                            PRInt32 aTransitionType, PRInt64* aVisitID,
                            PRInt64* aReferringID);
  PRBool IsURIStringVisited(const nsACString& url);
  nsresult VacuumDB(PRTime aTimeAgo, PRBool aCompress);
  nsresult LoadPrefs();

  // Current time optimization
  PRTime mLastNow;
  PRBool mNowValid;
  nsCOMPtr<nsITimer> mExpireNowTimer;
  static void expireNowTimerCallback(nsITimer* aTimer, void* aClosure);

  nsresult QueryToSelectClause(nsNavHistoryQuery* aQuery,
                               PRInt32 aStartParameter,
                               nsCString* aClause,
                               PRInt32* aParamCount,
                               const nsACString& aCommonConditions);
  nsresult BindQueryClauseParameters(mozIStorageStatement* statement,
                                     PRInt32 aStartParameter,
                                     nsNavHistoryQuery* aQuery,
                                     PRInt32* aParamCount);

  nsresult ResultsAsList(mozIStorageStatement* statement,
                         nsNavHistoryQueryOptions* aOptions,
                         nsCOMArray<nsNavHistoryResultNode>* aResults);

  void TitleForDomain(const nsCString& domain, nsACString& aTitle);
  nsresult SetPageTitleInternal(nsIURI* aURI, PRBool aIsUserTitle,
                                const nsAString& aTitle);

  nsresult GroupByHost(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                       nsCOMArray<nsNavHistoryResultNode>* aDest,
                       PRBool aIsDomain);

  nsresult FilterResultSet(const nsCOMArray<nsNavHistoryResultNode>& aSet,
                           nsCOMArray<nsNavHistoryResultNode>* aFiltered,
                           const nsString& aSearch);

  // observers
  nsMaybeWeakPtrArray<nsINavHistoryObserver> mObservers;
  PRInt32 mBatchesInProgress;

  // localization
  nsCOMPtr<nsIStringBundle> mBundle;
  nsCOMPtr<nsILocale> mLocale;
  nsCOMPtr<nsICollation> mCollation;
  nsCOMPtr<nsIDateTimeFormat> mDateFormatter;

  // annotation service : MAY BE NULL!
  //nsCOMPtr<mozIAnnotationService> mAnnotationService;

  // recent events
  typedef nsDataHashtable<nsCStringHashKey, PRInt64> RecentEventHash;
  RecentEventHash mRecentTyped;
  RecentEventHash mRecentBookmark;

  PRBool CheckIsRecentEvent(RecentEventHash* hashTable,
                            const nsACString& url);
  void ExpireNonrecentEvents(RecentEventHash* hashTable);

  // session tracking
  PRInt64 mLastSessionID;

  //
  // AutoComplete stuff
  //
  nsStringArray mIgnoreSchemes;
  nsStringArray mIgnoreHostnames;

  PRInt32 mAutoCompleteMaxCount;
  PRInt32 mExpireDays;
  PRBool mAutoCompleteOnlyTyped;

  // Used to describe what prefixes shouldn't be cut from
  // history urls when doing an autocomplete url comparison.
  struct AutoCompleteExclude {
    // these are indices into mIgnoreSchemes and mIgnoreHostnames, or -1
    PRInt32 schemePrefix;
    PRInt32 hostnamePrefix;

    // this is the offset of the character immediately after the prefix
    PRInt32 postPrefixOffset;
  };

  nsresult AutoCompleteTypedSearch(nsIAutoCompleteSimpleResult* result);
  nsresult AutoCompleteFullHistorySearch(const nsAString& aSearchString,
                                         nsIAutoCompleteSimpleResult* result);
  nsresult AutoCompleteRefineHistorySearch(const nsAString& aSearchString,
                                   nsIAutoCompleteResult* aPreviousResult,
                                   nsIAutoCompleteSimpleResult* aNewResult);
  void AutoCompleteCutPrefix(nsString* aURL,
                             const AutoCompleteExclude* aExclude);
  void AutoCompleteGetExcludeInfo(const nsAString& aURL,
                                  AutoCompleteExclude* aExclude);
  PRBool AutoCompleteCompare(const nsAString& aHistoryURL,
                             const nsAString& aUserURL,
                             const AutoCompleteExclude& aExclude);
  PR_STATIC_CALLBACK(int) AutoCompleteSortComparison(
                             const void* match1Void,
                             const void* match2Void,
                             void *navHistoryVoid);

  // in nsNavHistoryQuery.cpp
  nsresult TokensToQueries(const nsTArray<QueryKeyValuePair>& aTokens,
                           nsCOMArray<nsNavHistoryQuery>* aQueries,
                           nsNavHistoryQueryOptions* aOptions);
};

/**
 * Shared between the places components, this function binds the given URI as
 * UTF8 to the given parameter for the statement.
 */
nsresult BindStatementURI(mozIStorageStatement* statement, PRInt32 index,
                          nsIURI* aURI);

NS_NAMED_LITERAL_CSTRING(placesURIPrefix, "place:");

/* Returns true if the given URI represents a history query. */
inline PRBool IsQueryURI(const nsCString &uri)
{
  return StringBeginsWith(uri, placesURIPrefix);
}

/* Extracts the query string from a query URI. */
inline const nsDependentCSubstring QueryURIToQuery(const nsCString &uri)
{
  NS_ASSERTION(IsQueryURI(uri), "should only be called for query URIs");
  return Substring(uri, placesURIPrefix.Length());
}

#endif // nsNavHistory_h_
