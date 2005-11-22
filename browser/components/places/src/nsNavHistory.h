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
#include "nsINavHistory.h"
#include "nsIAutoCompleteSearch.h"
#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteResultTypes.h"
#include "nsIAutoCompleteSimpleResult.h"
#include "nsIBrowserHistory.h"
#include "nsICollation.h"
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

// Number of prefixes used in the autocomplete sort comparison function
#define AUTOCOMPLETE_PREFIX_LIST_COUNT 6
// Size of visit count boost to give to urls which are sites or paths
#define AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST 5

class mozIAnnotationService;
class nsNavHistory;

// nsNavHistoryQuery
//
//    This class encapsulates the parameters for basic history queries for
//    building UI, trees, lists, etc.

class nsNavHistoryQuery : public nsINavHistoryQuery
{
public:
  nsNavHistoryQuery();
  // note: we use a copy constructor in Clone(), the default is good enough

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERY

private:
  ~nsNavHistoryQuery() {}

protected:

  PRTime mBeginTime;
  PRTime mEndTime;
  nsString mSearchTerms;
  PRBool mOnlyBookmarked;
  PRBool mDomainIsHost;
  nsString mDomain;
  PRInt32 mGroupingMode;
  PRInt32 mSortingMode;
  PRBool mAsVisits;
  PRBool mExpandPlaces;
};


// nsNavHistoryResultNode

#define NS_NAVHISTORYRESULTNODE_IID \
{0x54b61d38, 0x57c1, 0x11da, {0x95, 0xb8, 0x00, 0x13, 0x21, 0xc9, 0xf6, 0x9e}}

class nsNavHistoryResultNode : public nsINavHistoryResultNode
{
public:
  nsNavHistoryResultNode();

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYRESULTNODE_IID)

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYRESULTNODE

  // Generate the children for this node.
  virtual nsresult BuildChildren(PRUint32 aOptions) { return NS_OK; }

  // Non-XPCOM member accessors
  PRInt32 Type() const { return mType; }
  const nsCString& URL() const { return mUrl; }

protected:
  virtual ~nsNavHistoryResultNode() {}

  // parent of this element, NULL if no parent. Filled in by FillAllElements
  // in the result set.
  nsNavHistoryResultNode* mParent;

  // these are all the basic row info, filled in by nsNavHistory::RowToResult
  PRInt64 mID; // keep this for comparing dups, not exposed in interface
               // might be 0, indicating some kind of parent node
  PRInt32 mType;
  nsCString mUrl;
  nsString mTitle;
  PRInt32 mAccessCount;
  PRTime mTime;
  nsString mHost;

  // Filled in by the result type generator in nsNavHistory
  nsCOMArray<nsNavHistoryResultNode> mChildren;

  // filled in by FillAllElements in the result set.
  PRInt32 mIndentLevel;

  // Index of this element into the flat mAllElements array in the result set.
  // Filled in by FillAllElements. DANGER, this does not necessarily mean that
  // mAllElements[mFlatIndex] = this, because we could be collapsed.
  // mAllElements[mFlatIndex] could be a duplicate of us.
  PRInt32 mFlatIndex;

  // index of this element into the mVisibleElements array in the result set
  PRInt32 mVisibleIndex;

  // when there are children, this stores the open state in the tree
  // this is set to the default in the constructor
  PRBool mExpanded;

  friend class nsNavHistory;
  friend class nsNavHistoryResult;
};

// nsNavHistoryQueryNode is a special type of ResultNode that executes a
// query when asked to build its children.
class nsNavHistoryQueryNode : public nsNavHistoryResultNode
{
public:
  nsNavHistoryQueryNode() : mQueries(nsnull), mQueryCount(0) {}

  // nsINavHistoryResultNode methods
  NS_IMETHOD GetQueries(PRUint32 *aQueryCount,
                        nsINavHistoryQuery ***aQueries);
  NS_IMETHOD GetQueryOptions(nsINavHistoryQueryOptions **aOptions);

  // nsNavHistoryResultNode methods
  virtual nsresult BuildChildren(PRUint32 aOptions);

protected:
  virtual ~nsNavHistoryQueryNode();
  nsresult ParseQueries();

  nsINavHistoryQuery **mQueries;
  PRUint32 mQueryCount;
  nsCOMPtr<nsINavHistoryQueryOptions> mOptions;
};

class nsIDateTimeFormat;

// nsNavHistoryResult
//
//    nsNavHistory creates this object and fills in mTopLevel (by getting
//    it through GetTopLevel(). Then FilledAllResults() is called to finish
//    object initialization.

class nsNavHistoryResult : public nsINavHistoryResult,
                           public nsITreeView
{
public:
  nsNavHistoryResult(nsNavHistory* aHistoryService,
                     nsIStringBundle* aHistoryBundle,
                     nsINavHistoryQuery** aQueries,
                     PRUint32 aQueryCount,
                     nsINavHistoryQueryOptions *aOptions);

  // Two-stage init, MUST BE CALLED BEFORE ANYTHING ELSE
  nsresult Init();

  nsCOMArray<nsNavHistoryResultNode>* GetTopLevel() { return &mTopLevelElements; }
  void ApplyTreeState(
      const nsDataHashtable<nsStringHashKey, int>& aExpanded);
  void FilledAllResults();

  nsresult BuildChildrenFor(nsNavHistoryResultNode *aNode);

  void SetBookmarkOptions(PRUint32 aOptions) { mBookmarkOptions = aOptions; }

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYRESULT
  NS_DECL_NSITREEVIEW

private:
  ~nsNavHistoryResult();

protected:
  nsCOMPtr<nsIStringBundle> mBundle;
  nsCOMPtr<nsITreeBoxObject> mTree; // may be null if no tree has bound itself
  nsCOMPtr<nsITreeSelection> mSelection; // may be null

  nsRefPtr<nsNavHistory> mHistoryService;

  PRBool mCollapseDuplicates;

  // what generated this result set
  nsCOMArray<nsINavHistoryQuery> mSourceQueries;
  nsCOMPtr<nsINavHistoryQueryOptions> mSourceOptions;

  // for locale-specific date formatting and string sorting
  nsCOMPtr<nsILocale> mLocale;
  nsCOMPtr<nsICollation> mCollation;
  nsCOMPtr<nsIDateTimeFormat> mDateFormatter;
  PRBool mTimesIncludeDates;

  // these are the roots of the hierarchy. This array is filled in externally
  // (use GetTopLevel). This contains the owning references. Everything else
  // uses void arrays to avoid AddRef overhead and to get better functionality.
  nsCOMArray<nsNavHistoryResultNode> mTopLevelElements;

  // this is the flattened version of the hierarchy containing everything
  nsVoidArray mAllElements;
  nsNavHistoryResultNode* AllElementAt(PRInt32 index)
  {
    return (nsNavHistoryResultNode*)mAllElements[index];
  }

  nsVoidArray mVisibleElements;
  nsNavHistoryResultNode* VisibleElementAt(PRInt32 index)
  {
    return (nsNavHistoryResultNode*)mVisibleElements[index];
  }

  // keep track of sorting state
  PRUint32 mCurrentSort;

  // bookmark types for this result (nsINavBookmarksService::*_CHILDREN)
  PRUint32 mBookmarkOptions;

  void FillTreeStats(nsNavHistoryResultNode* aResult, PRInt32 aLevel);
  void InitializeVisibleList();
  void RebuildList();
  void RebuildAllListRecurse(const nsCOMArray<nsNavHistoryResultNode>& aSource);
  void BuildVisibleSection(const nsCOMArray<nsNavHistoryResultNode>& aSources,
                           nsVoidArray* aVisible);
  void InsertVisibleSection(const nsVoidArray& aAddition, PRInt32 aInsertHere);
  PRInt32 DeleteVisibleChildrenOf(PRInt32 aIndex);

  void RecursiveSortArray(nsCOMArray<nsNavHistoryResultNode>& aSources,
                          PRUint32 aSortingMode);
  void SetTreeSortingIndicator();

  //void RecursiveCopyTreeStateFrom(nsCOMArray<nsNavHistoryResultNode>& aSource,
  //                                nsCOMArray<nsNavHistoryResultNode>& aDest);
  void RecursiveApplyTreeState(
      nsCOMArray<nsNavHistoryResultNode>& aList,
      const nsDataHashtable<nsStringHashKey, int>& aExpanded);
  void RecursiveExpandCollapse(nsCOMArray<nsNavHistoryResultNode>& aList,
                               PRBool aExpand);

  enum ColumnType { Column_Unknown = -1, Column_Title, Column_URL, Column_Date, Column_VisitCount };
  ColumnType GetColumnType(nsITreeColumn* col);
  ColumnType SortTypeToColumnType(PRUint32 aSortType,
                                  PRBool* aDescending = nsnull);

  PR_STATIC_CALLBACK(int) SortComparison_TitleLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_TitleGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_DateLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_DateGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_URLLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_URLGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_VisitCountLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_VisitCountGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
};


class AutoCompleteIntermediateResultSet;

#define NS_NAVHISTORYQUERYOPTIONS_IID \
{0x95f8ba3b, 0xd681, 0x4d89, {0xab, 0xd1, 0xfd, 0xae, 0xf2, 0xa3, 0xde, 0x18}}

class nsNavHistoryQueryOptions : public nsINavHistoryQueryOptions
{
public:
  nsNavHistoryQueryOptions() : mSort(0), mResultType(0),
                               mGroupCount(0), mGroupings(nsnull), mExpandPlaces(PR_FALSE)
  { }

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERYOPTIONS_IID)

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERYOPTIONS

  PRInt32 SortingMode() const { return mSort; }
  PRInt32 ResultType() const { return mResultType; }
  const PRInt32* GroupingMode(PRUint32 *count) const {
    *count = mGroupCount; return mGroupings;
  }
  PRBool ExpandPlaces() const { return mExpandPlaces; }

private:
  nsNavHistoryQueryOptions(const nsNavHistoryQueryOptions& other) {} // no copy

  ~nsNavHistoryQueryOptions() { delete[] mGroupings; }

  PRInt32 mSort;
  PRInt32 mResultType;
  PRUint32 mGroupCount;
  PRInt32 *mGroupings;
  PRBool mExpandPlaces;
};

// nsNavHistory

class nsNavHistory : public nsSupportsWeakReference,
                     public nsINavHistory,
                     public nsIObserver,
                     public nsIBrowserHistory,
                     public nsIAutoCompleteSearch
{
  friend class AutoCompleteIntermediateResultSet;
public:
  nsNavHistory();

  NS_DECL_ISUPPORTS

  NS_DECL_NSINAVHISTORY
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
      nsCOMPtr<nsINavHistory> serv(do_GetService("@mozilla.org/browser/nav-history;1", &rv));
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

  // remember tree state

  void SaveExpandItem(const nsAString& aTitle);
  void SaveCollapseItem(const nsAString& aTitle);

  // get the statement for selecting a history row by URL
  mozIStorageStatement* DBGetURLPageInfo() { return mDBGetURLPageInfo; }

  // Constants for the columns returned by the above statement.
  static const PRInt32 kGetInfoIndex_PageID;
  static const PRInt32 kGetInfoIndex_URL;
  static const PRInt32 kGetInfoIndex_Title;
  static const PRInt32 kGetInfoIndex_VisitCount;
  static const PRInt32 kGetInfoIndex_VisitDate;
  static const PRInt32 kGetInfoIndex_RevHost;

  // Take a result returned from DBGetURLPageInfo and construct a
  // ResultNode.
  nsresult RowToResult(mozIStorageValueArray* aRow, PRBool aAsVisits,
                       nsNavHistoryResultNode** aResult);

  // Construct a new HistoryResult object. You can give it null query/options.
  nsNavHistoryResult* NewHistoryResult(nsINavHistoryQuery** aQueries,
                                       PRUint32 aQueryCount,
                                       nsINavHistoryQueryOptions* aOptions)
  {
    return new nsNavHistoryResult(this, mBundle, aQueries, aQueryCount,
                                  aOptions);
  }

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

  nsCOMPtr<mozIStorageStatement> mDBGetVisitPageInfo; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBGetURLPageInfo;   // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBFullAutoComplete; // kAutoCompleteIndex_* results, 1 arg (max # results)
  static const PRInt32 kAutoCompleteIndex_URL;
  static const PRInt32 kAutoCompleteIndex_Title;
  static const PRInt32 kAutoCompleteIndex_VisitCount;
  static const PRInt32 kAutoCompleteIndex_Typed;

  nsresult InitDB();

  // this is the cache DB in memory used for storing visited URLs
  nsCOMPtr<mozIStorageConnection> mMemDBConn;
  nsCOMPtr<mozIStorageStatement> mMemDBAddPage;
  nsCOMPtr<mozIStorageStatement> mMemDBGetPage;

  nsresult InitMemDB();

  nsresult InternalAdd(nsIURI* aURI, PRInt64 aSessionID,
                       PRUint32 aTransitionType, const PRUnichar* aTitle,
                       PRTime aVisitDate, PRBool aRedirect,
                       PRBool aToplevel, PRInt64* aPageID);
  nsresult InternalAddNewPage(nsIURI* aURI, const PRUnichar* aTitle,
                              PRBool aHidden, PRBool aTyped,
                              PRInt32 aVisitCount, PRInt64* aPageID);
  nsresult AddVisit(PRInt64 aFromStep, PRInt64 aPageID, PRTime aTime,
                    PRInt32 aTransitionType, PRInt64 aSessionID);
  PRBool IsURIStringVisited(const nsACString& url);
  nsresult VacuumDB();
  nsresult LoadPrefs();

  // Current time optimization
  PRTime mLastNow;
  PRBool mNowValid;
  nsCOMPtr<nsITimer> mExpireNowTimer;
  PRTime GetNow();
  static void expireNowTimerCallback(nsITimer* aTimer, void* aClosure);

  nsresult QueryToSelectClause(nsINavHistoryQuery* aQuery,
                               PRInt32 aStartParameter,
                               nsCString* aClause,
                               PRInt32* aParamCount);
  nsresult BindQueryClauseParameters(mozIStorageStatement* statement,
                                     PRInt32 aStartParameter,
                                     nsINavHistoryQuery* aQuery,
                                     PRInt32* aParamCount);

  nsresult ResultsAsList(mozIStorageStatement* statement, PRBool aAsVisits,
                         nsCOMArray<nsNavHistoryResultNode>* aResults);

  void TitleForDomain(const nsString& domain, nsAString& aTitle);

  nsresult RecursiveGroup(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                          const PRInt32* aGroupingMode, PRUint32 aGroupCount,
                          nsCOMArray<nsNavHistoryResultNode>* aDest);
  nsresult GroupByDay(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                      nsCOMArray<nsNavHistoryResultNode>* aDest);
  nsresult GroupByHost(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                       nsCOMArray<nsNavHistoryResultNode>* aDest);
  nsresult GroupByDomain(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                         nsCOMArray<nsNavHistoryResultNode>* aDest);

  nsresult FilterResultSet(const nsCOMArray<nsNavHistoryResultNode>& aSet,
                           nsCOMArray<nsNavHistoryResultNode>* aFiltered,
                           const nsString& aSearch);

  // observers
  nsCOMArray<nsINavHistoryObserver> mObservers;
  PRInt32 mBatchesInProgress;

  // string bundles
  nsCOMPtr<nsIStringBundle> mBundle;

  // annotation service : MAY BE NULL!
  //nsCOMPtr<mozIAnnotationService> mAnnotationService;

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
  nsresult TokensToQueries(const nsVoidArray& aTokens,
                              nsCOMArray<nsINavHistoryQuery>* aQueries,
                              nsNavHistoryQueryOptions* aOptions);

  nsresult ImportFromMork();
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
