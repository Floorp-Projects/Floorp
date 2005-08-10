/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Blake Ross <blaker@netscape.com>
 *   Simon Fraser <smfr@smfr.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "plstr.h"
#include "prprf.h"

#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsQuickSort.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"

#include "nsIURL.h"
#include "nsNetCID.h"

#include "nsInt64.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"

#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"

#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsIObserverService.h"
#include "nsITextToSubURI.h"

#include "nsSimpleGlobalHistory.h"

#define PREF_BRANCH_BASE                        "browser."
#define PREF_BROWSER_HISTORY_EXPIRE_DAYS        "history_expire_days"
#define PREF_AUTOCOMPLETE_ONLY_TYPED            "urlbar.matchOnlyTyped"
#define PREF_AUTOCOMPLETE_ENABLED               "urlbar.autocomplete.enabled"

// sync history every 10 seconds
#define HISTORY_SYNC_TIMEOUT (10 * PR_MSEC_PER_SEC)
//#define HISTORY_SYNC_TIMEOUT 3000 // every 3 seconds - testing only!

// the value of mLastNow expires every 3 seconds
#define HISTORY_EXPIRE_NOW_TIMEOUT (3 * PR_MSEC_PER_SEC)

static const PRInt64 MSECS_PER_DAY = LL_INIT(20, 500654080);  // (1000000LL * 60 * 60 * 24)

PRInt32 nsSimpleGlobalHistory::gRefCnt;
nsIMdbFactory* nsSimpleGlobalHistory::gMdbFactory = nsnull;
nsIPrefBranch* nsSimpleGlobalHistory::gPrefBranch = nsnull;

// list of terms, plus an optional groupby column
struct SearchQueryData {
  nsVoidArray   terms;            // array of searchTerms
  mdb_column    groupBy;           // column to group by
};


// individual search term, pulled from token/value structs
class HistorySearchTerm
{
public:
  HistorySearchTerm(const char* aDatasource, PRUint32 aDatasourceLen,
             const char *aProperty, PRUint32 aPropertyLen,
             const char* aMethod, PRUint32 aMethodLen,
             const char* aText, PRUint32 aTextLen)
  : datasource(aDatasource, aDatasource + aDatasourceLen)
  , property(aProperty, aProperty + aPropertyLen)
  , method(aMethod, aMethod + aMethodLen)
  {
    MOZ_COUNT_CTOR(HistorySearchTerm);
    nsresult rv;
    nsCOMPtr<nsITextToSubURI> textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      textToSubURI->UnEscapeAndConvert("UTF-8",  PromiseFlatCString(Substring(aText, aText + aTextLen)).get(), getter_Copies(text));
  }

  ~HistorySearchTerm()
  {
    MOZ_COUNT_DTOR(HistorySearchTerm);
  }
  
  nsDependentCSubstring   datasource;  // should always be "history" ?
  nsDependentCSubstring   property;    // AgeInDays, Hostname, etc
  nsDependentCSubstring   method;      // is, isgreater, isless
  nsXPIDLString           text;        // text to match
  rowMatchCallback        match;       // matching callback if needed
};


// closure structures for RemoveMatchingRows
struct MatchExpirationData {
  PRTime                  expirationDate;
  nsSimpleGlobalHistory*  history;
};

struct MatchHostData {
  const char*             host;
  PRBool                  entireDomain;          // should we delete the entire domain?
  nsSimpleGlobalHistory*  history;
};

struct MatchSearchTermData {
  nsIMdbEnv*              env;
  nsIMdbStore*            store;
  
  HistorySearchTerm*             term;
  PRBool                  haveClosure;           // are the rest of the fields valid?
  PRTime                  now;
  PRInt32                 intValue;
};

struct MatchQueryData {
  SearchQueryData*            query;
  nsSimpleGlobalHistory*  history;
};

// Used to describe what prefixes shouldn't be cut from
// history urls when doing an autocomplete url comparison.
struct AutocompleteExcludeData {
  PRInt32                 schemePrefix;
  PRInt32                 hostnamePrefix;
};


#pragma mark -

static PRBool
matchExpirationCallback(nsIMdbRow *row, void *aClosure)
{
  MatchExpirationData *expires = (MatchExpirationData*)aClosure;
  return expires->history->MatchExpiration(row, &expires->expirationDate);
}

static PRBool
matchAllCallback(nsIMdbRow *row, void *aClosure)
{
  return PR_TRUE;
}

static PRBool
matchHostCallback(nsIMdbRow *row, void *aClosure)
{
  MatchHostData *hostInfo = (MatchHostData*)aClosure;
  return hostInfo->history->MatchHost(row, hostInfo);
}

static PRBool
matchQueryCallback(nsIMdbRow *row, void *aClosure)
{
  MatchQueryData *query = (MatchQueryData*)aClosure;
  return query->history->RowMatches(row, query->query);
}

static PRBool HasCell(nsIMdbEnv *aEnv, nsIMdbRow* aRow, mdb_column aCol)
{
  mdbYarn yarn;
  mdb_err err = aRow->AliasCellYarn(aEnv, aCol, &yarn);

  // no cell
  if (err != 0)
    return PR_FALSE;

  // if we have the cell, make sure it has a value??
  return (yarn.mYarn_Fill != 0);
}


#pragma mark -

//----------------------------------------------------------------------
//
//  nsHistoryMdbTableEnumerator
//
//    An nsISimpleEnumerator implementation that returns the value of
//    a column as an nsISupports. Allows for some simple selection.
//

class nsHistoryMdbTableEnumerator : public nsISimpleEnumerator
{
protected:
              nsHistoryMdbTableEnumerator();
  virtual     ~nsHistoryMdbTableEnumerator();

public:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsISimpleEnumeratorMethods
  NS_IMETHOD HasMoreElements(PRBool* _result);
  NS_IMETHOD GetNext(nsISupports** _result);

  // Implementation methods
  virtual nsresult Init(nsIMdbEnv* aEnv, nsIMdbTable* aTable);

protected:
  virtual PRBool   IsResult(nsIMdbRow* aRow) = 0;
  virtual nsresult ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult) = 0;

protected:

  nsIMdbEnv*            mEnv;

private:

  // subclasses should not tweak these
  nsIMdbTable*          mTable;
  nsIMdbTableRowCursor* mCursor;
  nsIMdbRow*            mCurrent;

};

//----------------------------------------------------------------------

nsHistoryMdbTableEnumerator::nsHistoryMdbTableEnumerator()
  : mEnv(nsnull),
    mTable(nsnull),
    mCursor(nsnull),
    mCurrent(nsnull)
{
}


nsresult
nsHistoryMdbTableEnumerator::Init(nsIMdbEnv* aEnv,
                           nsIMdbTable* aTable)
{
  NS_PRECONDITION(aEnv != nsnull, "null ptr");
  if (! aEnv)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aTable != nsnull, "null ptr");
  if (! aTable)
    return NS_ERROR_NULL_POINTER;

  mEnv = aEnv;
  NS_ADDREF(mEnv);

  mTable = aTable;
  NS_ADDREF(mTable);

  mdb_err err;
  err = mTable->GetTableRowCursor(mEnv, -1, &mCursor);
  if (err != 0) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsHistoryMdbTableEnumerator::~nsHistoryMdbTableEnumerator()
{
  NS_IF_RELEASE(mCurrent);
  NS_IF_RELEASE(mCursor);
  NS_IF_RELEASE(mTable);
  NS_IF_RELEASE(mEnv);
}


NS_IMPL_ISUPPORTS1(nsHistoryMdbTableEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsHistoryMdbTableEnumerator::HasMoreElements(PRBool* _result)
{
  if (! mCurrent) {
    mdb_err err;

    while (1) {
      mdb_pos pos;
      err = mCursor->NextRow(mEnv, &mCurrent, &pos);
      if (err != 0) return NS_ERROR_FAILURE;

      // If there are no more rows, then bail.
      if (! mCurrent)
        break;

      // If this is a result, the stop.
      if (IsResult(mCurrent))
        break;

      // Otherwise, drop the ref to the row we retrieved, and continue
      // on to the next one.
      NS_RELEASE(mCurrent);
      mCurrent = nsnull;
    }
  }

  *_result = (mCurrent != nsnull);
  return NS_OK;
}


NS_IMETHODIMP
nsHistoryMdbTableEnumerator::GetNext(nsISupports** _result)
{
  nsresult rv;

  PRBool hasMore;
  rv = HasMoreElements(&hasMore);
  if (NS_FAILED(rv)) return rv;

  if (! hasMore)
    return NS_ERROR_UNEXPECTED;

  rv = ConvertToISupports(mCurrent, _result);

  NS_RELEASE(mCurrent);
  mCurrent = nsnull;

  return rv;
}


#pragma mark -

class nsMdbTableAllRowsEnumerator : public nsHistoryMdbTableEnumerator
{
public:
              nsMdbTableAllRowsEnumerator(nsSimpleGlobalHistory* inHistory, mdb_column inHiddenColumnToken)
              : mHistory(inHistory)
              , mHiddenColumnToken(inHiddenColumnToken)
              {
              }
  virtual     ~nsMdbTableAllRowsEnumerator()
              {
              }

protected:
  virtual PRBool IsResult(nsIMdbRow* aRow)
  {
    if (HasCell(mEnv, aRow, mHiddenColumnToken))
      return PR_FALSE;
  
    return PR_TRUE;
  }
  
  // is this required to return the same object each time, cos we ain't now
  virtual nsresult ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult)
  {
    nsHistoryItem* thisItem = new nsHistoryItem;
    thisItem->InitWithRow(mHistory, mEnv, aRow);
    NS_ADDREF(thisItem);
    *aResult = thisItem;
    return NS_OK;
  }

protected:

  nsSimpleGlobalHistory*    mHistory;
  mdb_column                mHiddenColumnToken;
};



#pragma mark -


nsHistoryItem::nsHistoryItem()
: mHistory(nsnull)
{
}

nsHistoryItem::~nsHistoryItem()
{
}


NS_IMPL_ISUPPORTS1(nsHistoryItem, nsIHistoryItem);

NS_IMETHODIMP
nsHistoryItem::InitWithRow(nsSimpleGlobalHistory* inHistory, nsIMdbEnv* inEnv, nsIMdbRow* inRow)
{
  mHistory = inHistory;
  mEnv = inEnv;
  mRow = inRow;
  return NS_OK;
}

NS_IMETHODIMP
nsHistoryItem::GetURL(nsACString& outURL)
{
  return mHistory->GetRowValue(mRow, mHistory->TokenForColumn(nsSimpleGlobalHistory::eColumnURL), outURL);
}

NS_IMETHODIMP
nsHistoryItem::GetReferrer(nsACString& outReferrer)
{
  return mHistory->GetRowValue(mRow, mHistory->TokenForColumn(nsSimpleGlobalHistory::eColumnReferrer), outReferrer);
}

NS_IMETHODIMP
nsHistoryItem::GetLastVisitDate(PRTime* outLastVisit)
{
  return mHistory->GetRowValue(mRow, mHistory->TokenForColumn(nsSimpleGlobalHistory::eColumnLastVisitDate), outLastVisit);
}

NS_IMETHODIMP
nsHistoryItem::GetFirstVisitDate(PRTime* outFirstVisit)
{
  return mHistory->GetRowValue(mRow, mHistory->TokenForColumn(nsSimpleGlobalHistory::eColumnFirstVisitDate), outFirstVisit);
}

NS_IMETHODIMP
nsHistoryItem::GetVisitCount(PRInt32* outVisitCount)
{
  return mHistory->GetRowValue(mRow, mHistory->TokenForColumn(nsSimpleGlobalHistory::eColumnVisitCount), outVisitCount);
}

NS_IMETHODIMP
nsHistoryItem::GetTitle(nsAString& outURL)
{
  return mHistory->GetRowValue(mRow, mHistory->TokenForColumn(nsSimpleGlobalHistory::eColumnTitle), outURL);
}

NS_IMETHODIMP
nsHistoryItem::GetHostname(nsACString& outURL)
{
  return mHistory->GetRowValue(mRow, mHistory->TokenForColumn(nsSimpleGlobalHistory::eColumnHostname), outURL);
}

NS_IMETHODIMP
nsHistoryItem::GetHidden(PRBool* outHidden)
{
  return mHistory->RowHasCell(mRow, mHistory->TokenForColumn(nsSimpleGlobalHistory::eColumnHidden), outHidden);
}

NS_IMETHODIMP
nsHistoryItem::GetTyped(PRBool* outTyped)
{
  return mHistory->RowHasCell(mRow, mHistory->TokenForColumn(nsSimpleGlobalHistory::eColumnTyped), outTyped);
}

NS_IMETHODIMP
nsHistoryItem::GetID(nsACString& outIDString)
{
  mdbOid    oid;
  mRow->GetOid(mEnv, &oid);
  outIDString = nsPrintfCString(17, "%08x_%08x", oid.mOid_Scope, oid.mOid_Id);
  return NS_OK;
}


#pragma mark -


//----------------------------------------------------------------------
//
// nsSimpleGlobalHistory
//
//   ctor dtor etc.
//


nsSimpleGlobalHistory::nsSimpleGlobalHistory()
  : mExpireDays(9), // make default be nine days
    mAutocompleteOnlyTyped(PR_FALSE),
    mBatchesInProgress(0),
    mDirty(PR_FALSE),
    mPagesRemoved(PR_FALSE),
    mEnv(nsnull),
    mStore(nsnull),
    mTable(nsnull)
{
  LL_I2L(mFileSizeOnDisk, 0);
  
  // commonly used prefixes that should be chopped off all 
  // history and input urls before comparison

  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("http://"));
  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("https://"));
  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("ftp://"));
  mIgnoreHostnames.AppendString(NS_LITERAL_STRING("www."));
  mIgnoreHostnames.AppendString(NS_LITERAL_STRING("ftp."));
}

nsSimpleGlobalHistory::~nsSimpleGlobalHistory()
{
  nsresult rv;
  rv = CloseDB();

  NS_IF_RELEASE(mTable);
  NS_IF_RELEASE(mStore);
  
  if (--gRefCnt == 0) {
    NS_IF_RELEASE(gMdbFactory);
    NS_IF_RELEASE(gPrefBranch);
  }

  NS_IF_RELEASE(mEnv);
  if (mSyncTimer)
    mSyncTimer->Cancel();
}

//----------------------------------------------------------------------
//
// nsSimpleGlobalHistory
//
//   nsISupports methods

NS_IMPL_ISUPPORTS6(nsSimpleGlobalHistory,
                   nsIGlobalHistory2,
                   nsIBrowserHistory,
                   nsIHistoryItems,
                   nsIObserver,
                   nsISupportsWeakReference,
                   nsIAutoCompleteSession)

//----------------------------------------------------------------------
//
// nsSimpleGlobalHistory
//
//   nsIGlobalHistory2 methods
//


NS_IMETHODIMP
nsSimpleGlobalHistory::AddURI(nsIURI *aURI, PRBool aRedirect, PRBool aTopLevel, nsIURI *aReferrer)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aURI);

  // If history is set to expire after 0 days,
  // then it's technically disabled. Don't even
  // bother adding the page
  if (mExpireDays == 0)
    return NS_OK;

  // filter out unwanted URIs such as chrome: mailbox: etc
  // The model is really if we don't know differently then add which basically
  // means we are suppose to try all the things we know not to allow in and
  // then if we don't bail go on and allow it in.  But here lets compare
  // against the most common case we know to allow in and go on and say yes
  // to it.

  PRBool isHTTP = PR_FALSE;
  PRBool isHTTPS = PR_FALSE;

  NS_ENSURE_SUCCESS(rv = aURI->SchemeIs("http", &isHTTP), rv);
  NS_ENSURE_SUCCESS(rv = aURI->SchemeIs("https", &isHTTPS), rv);

  if (!isHTTP && !isHTTPS) {
    PRBool isAbout, isImap, isNews, isMailbox, isViewSource, isChrome, isData;

    rv = aURI->SchemeIs("about", &isAbout);
    rv |= aURI->SchemeIs("imap", &isImap);
    rv |= aURI->SchemeIs("news", &isNews);
    rv |= aURI->SchemeIs("mailbox", &isMailbox);
    rv |= aURI->SchemeIs("view-source", &isViewSource);
    rv |= aURI->SchemeIs("chrome", &isChrome);
    rv |= aURI->SchemeIs("data", &isData);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if (isAbout || isImap || isNews || isMailbox || isViewSource || isChrome || isData) {
      return NS_OK;
    }
  }

  rv = OpenDB();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString URISpec;
  rv = aURI->GetSpec(URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString referrerSpec;
  if (aReferrer) {
    rv = aReferrer->GetSpec(referrerSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRTime now = GetNow();

  nsCOMPtr<nsIMdbRow> row;
  rv = FindRow(kToken_URLColumn, URISpec.get(), getter_AddRefs(row));

  if (NS_SUCCEEDED(rv))
  {
    // update the database, and get the old info back
    PRTime oldDate;
    PRInt32 oldCount;
    rv = AddExistingPageToDatabase(row, now, referrerSpec.get(), &oldDate, &oldCount);
    NS_ASSERTION(NS_SUCCEEDED(rv), "AddExistingPageToDatabase failed; see bug 88961");
    if (NS_FAILED(rv)) return rv;
    
    // Notify observers
    NotifyObserversItemLoaded(row, PR_FALSE);
  }
  else
  {
    rv = AddNewPageToDatabase(URISpec.get(), now, referrerSpec.get(), getter_AddRefs(row));
    NS_ASSERTION(NS_SUCCEEDED(rv), "AddNewPageToDatabase failed; see bug 88961");
    if (NS_FAILED(rv)) return rv;
    
    PRBool isJavascript;
    rv = aURI->SchemeIs("javascript", &isJavascript);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isJavascript || aRedirect || !aTopLevel) {
      // if this is a JS url, or a redirected URI or in a frame, hide it in
      // global history so that it doesn't show up in the autocomplete
      // dropdown. AddExistingPageToDatabase has logic to override this
      // behavior for URIs which were typed. See bug 197127 and bug 161531
      // for details.
      rv = SetRowValue(row, kToken_HiddenColumn, 1);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
      // Notify observers
      NotifyObserversItemLoaded(row, PR_TRUE);
    }
  }

  // Store last visited page if we have the pref set accordingly
  if (aTopLevel)
  {
    PRInt32 choice = 0;
    if (NS_SUCCEEDED(gPrefBranch->GetIntPref("startup.page", &choice))) {
      if (choice != 2) {
        if (NS_SUCCEEDED(gPrefBranch->GetIntPref("windows.loadOnNewWindow", &choice))) {
          if (choice != 2) {
            gPrefBranch->GetIntPref("tabs.loadOnNewTab", &choice);
          }
        }
      }
    }
    if (choice == 2) {
      NS_ENSURE_STATE(mMetaRow);

      SetRowValue(mMetaRow, kToken_LastPageVisited, URISpec.get());
    }
  }
 
  SetDirty();
  
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::AddExistingPageToDatabase(nsIMdbRow *row,
                                           PRTime aDate,
                                           const char *aReferrer,
                                           PRTime *aOldDate,
                                           PRInt32 *aOldCount)
{
  nsresult rv;
  nsCAutoString oldReferrer;
  
  // if the page was typed, unhide it now because it's
  // known to be valid
  if (HasCell(mEnv, row, kToken_TypedColumn)) {
    nsCAutoString URISpec;
    rv = GetRowValue(row, kToken_URLColumn, URISpec);
    NS_ENSURE_SUCCESS(rv, rv);

    mTypedHiddenURIs.Remove(URISpec);
    row->CutColumn(mEnv, kToken_HiddenColumn);
  }

  // Update last visit date.
  // First get the old date so we can update observers...
  rv = GetRowValue(row, kToken_LastVisitDateColumn, aOldDate);
  if (NS_FAILED(rv)) return rv;

  // get the old count, so we can update it
  rv = GetRowValue(row, kToken_VisitCountColumn, aOldCount);
  if (NS_FAILED(rv) || *aOldCount < 1)
    *aOldCount = 1;             // assume we've visited at least once

  // ...now set the new date.
  SetRowValue(row, kToken_LastVisitDateColumn, aDate);
  SetRowValue(row, kToken_VisitCountColumn, (*aOldCount) + 1);

  if (aReferrer && *aReferrer) {
    rv = GetRowValue(row, kToken_ReferrerColumn, oldReferrer);
    // No referrer? Now there is!
    if (NS_FAILED(rv) || oldReferrer.IsEmpty())
      SetRowValue(row, kToken_ReferrerColumn, aReferrer);
  }

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::AddNewPageToDatabase(const char *aURL,
                                      PRTime aDate,
                                      const char *aReferrer,
                                      nsIMdbRow **aResult)
{
  mdb_err err;
  
  // Create a new row
  mdbOid rowId;
  rowId.mOid_Scope = kToken_HistoryRowScope;
  rowId.mOid_Id    = mdb_id(-1);
  
  NS_PRECONDITION(mTable != nsnull, "not initialized");
  if (! mTable)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIMdbRow> row;
  err = mTable->NewRow(mEnv, &rowId, getter_AddRefs(row));
  if (err != 0) return NS_ERROR_FAILURE;

  // Set the URL
  SetRowValue(row, kToken_URLColumn, aURL);
  
  // Set the date.
  SetRowValue(row, kToken_LastVisitDateColumn, aDate);
  SetRowValue(row, kToken_FirstVisitDateColumn, aDate);

  // Set the referrer if there is one.
  if (aReferrer && *aReferrer)
    SetRowValue(row, kToken_ReferrerColumn, aReferrer);

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), nsDependentCString(aURL), nsnull, nsnull);
  nsCAutoString hostname;
  if (uri)
      uri->GetHost(hostname);

  SetRowValue(row, kToken_HostnameColumn, hostname.get());

  *aResult = row;
  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::RemovePageInternal(const char *aSpec)
{
  if (!mTable) return NS_ERROR_NOT_INITIALIZED;
  // find the old row, ignore it if we don't have it
  nsCOMPtr<nsIMdbRow> row;
  nsresult rv = FindRow(kToken_URLColumn, aSpec, getter_AddRefs(row));
  if (NS_FAILED(rv)) return NS_OK;

  // remove the row
  mdb_err err = mTable->CutRow(mEnv, row);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  NotifyObserversItemRemoved(row);

  // not a fatal error if we can't cut all column
  err = row->CutAllColumns(mEnv);
  NS_ASSERTION(err == 0, "couldn't cut all columns");

  mPagesRemoved = PR_TRUE;
  SetDirty();

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::HistoryItemFromRow(nsIMdbRow* inRow, nsIHistoryItem** outItem)
{
  // XXX get from array
  nsHistoryItem* thisItem = new nsHistoryItem;
  thisItem->InitWithRow(this, mEnv, inRow);
  
  NS_ADDREF(thisItem);
  *outItem = thisItem;
  
  return NS_OK;
}

#pragma mark -

nsresult
nsSimpleGlobalHistory::StartBatching()
{
  if (mBatchesInProgress == 0)
    NotifyObserversBatchingStarted();

  ++mBatchesInProgress;
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::EndBatching()
{
  --mBatchesInProgress;
  NS_ASSERTION(mBatchesInProgress >= 0, "Batch count went negative");

  if (mBatchesInProgress == 0)
    NotifyObserversBatchingFinished();

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::NotifyObserversHistoryLoaded()
{
  PRUint32 numObservers;
  mHistoryObservers.Count(&numObservers);
  
  for (PRUint32 i = 0; i < numObservers; i ++)
  {
    nsCOMPtr<nsISupports> element;
    mHistoryObservers.GetElementAt(i, getter_AddRefs(element));
  
    nsCOMPtr<nsIHistoryObserver> historyObserver = do_QueryInterface(element);
    if (historyObserver)
      historyObserver->HistoryLoaded(); 
  }
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::NotifyObserversItemLoaded(nsIMdbRow* inRow, PRBool inFirstVisit)
{
  if (mBatchesInProgress > 0)
    return NS_OK;
  
  nsCOMPtr<nsIHistoryItem> historyItem;
  nsresult rv = HistoryItemFromRow(inRow, getter_AddRefs(historyItem));
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRUint32 numObservers;
  mHistoryObservers.Count(&numObservers);
  
  for (PRUint32 i = 0; i < numObservers; i ++)
  {
    nsCOMPtr<nsISupports> element;
    mHistoryObservers.GetElementAt(i, getter_AddRefs(element));
  
    nsCOMPtr<nsIHistoryObserver> historyObserver = do_QueryInterface(element);
    if (historyObserver)
      historyObserver->ItemLoaded(historyItem, inFirstVisit);
  }
  
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::NotifyObserversItemRemoved(nsIMdbRow* inRow)
{
  if (mBatchesInProgress > 0)
    return NS_OK;

  // we assume that the row is still valid at this point
  nsCOMPtr<nsIHistoryItem> historyItem;
  nsresult rv = HistoryItemFromRow(inRow, getter_AddRefs(historyItem));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 numObservers;
  mHistoryObservers.Count(&numObservers);
  
  for (PRUint32 i = 0; i < numObservers; i ++)
  {
    nsCOMPtr<nsISupports> element;
    mHistoryObservers.GetElementAt(i, getter_AddRefs(element));
  
    nsCOMPtr<nsIHistoryObserver> historyObserver = do_QueryInterface(element);
    if (historyObserver)
      historyObserver->ItemRemoved(historyItem); 
  }

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::NotifyObserversItemTitleChanged(nsIMdbRow* inRow)
{
  if (mBatchesInProgress > 0)
    return NS_OK;
  
  // we assume that the row is still valid at this point
  nsCOMPtr<nsIHistoryItem> historyItem;
  nsresult rv = HistoryItemFromRow(inRow, getter_AddRefs(historyItem));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 numObservers;
  mHistoryObservers.Count(&numObservers);
  
  for (PRUint32 i = 0; i < numObservers; i ++)
  {
    nsCOMPtr<nsISupports> element;
    mHistoryObservers.GetElementAt(i, getter_AddRefs(element));
  
    nsCOMPtr<nsIHistoryObserver> historyObserver = do_QueryInterface(element);
    if (historyObserver)
      historyObserver->ItemTitleChanged(historyItem);
  }
  
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::NotifyObserversBatchingStarted()
{
  PRUint32 numObservers;
  mHistoryObservers.Count(&numObservers);
  
  for (PRUint32 i = 0; i < numObservers; i ++)
  {
    nsCOMPtr<nsISupports> element;
    mHistoryObservers.GetElementAt(i, getter_AddRefs(element));
  
    nsCOMPtr<nsIHistoryObserver> historyObserver = do_QueryInterface(element);
    if (historyObserver)
      historyObserver->StartBatchChanges(); 
  }
  
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::NotifyObserversBatchingFinished()
{
  PRUint32 numObservers;
  mHistoryObservers.Count(&numObservers);
  
  for (PRUint32 i = 0; i < numObservers; i ++)
  {
    nsCOMPtr<nsISupports> element;
    mHistoryObservers.GetElementAt(i, getter_AddRefs(element));
  
    nsCOMPtr<nsIHistoryObserver> historyObserver = do_QueryInterface(element);
    if (historyObserver)
      historyObserver->EndBatchChanges(); 
  }

  return NS_OK;
}

#pragma mark -

nsresult
nsSimpleGlobalHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRTime& aValue)
{
  mdb_err err;
  nsCAutoString val;
  val.AppendInt(aValue);

  mdbYarn yarn = { (void *)val.get(), val.Length(), val.Length(), 0, 0, nsnull };
  
  err = aRow->AddColumn(mEnv, aCol, &yarn);

  if ( err != 0 ) return NS_ERROR_FAILURE;
  
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             const PRUnichar* aValue)
{
  mdb_err err;

  PRInt32 len = (nsCRT::strlen(aValue) * sizeof(PRUnichar));
  PRUnichar *swapval = nsnull;

  // eventually turn this on when we're confident in mork's abilitiy
  // to handle yarn forms properly
#if 0
  NS_ConvertUCS2toUTF8 utf8Value(aValue);
  printf("Storing utf8 value %s\n", utf8Value.get());
  mdbYarn yarn = { (void *)utf8Value.get(), utf8Value.Length(), utf8Value.Length(), 0, 1, nsnull };
#else

  if (mReverseByteOrder) {
    // The file is other-endian.  Byte-swap the value.
    swapval = (PRUnichar *)malloc(len);
    if (!swapval)
      return NS_ERROR_OUT_OF_MEMORY;
    SwapBytes(aValue, swapval, len / sizeof(PRUnichar));
    aValue = swapval;
  }
  mdbYarn yarn = { (void *)aValue, len, len, 0, 0, nsnull };
  
#endif
  err = aRow->AddColumn(mEnv, aCol, &yarn);
  if (swapval)
    free(swapval);
  if (err != 0) return NS_ERROR_FAILURE;
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             const char* aValue)
{
  mdb_err err;
  PRInt32 len = PL_strlen(aValue);
  mdbYarn yarn = { (void*) aValue, len, len, 0, 0, nsnull };
  err = aRow->AddColumn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRInt32 aValue)
{
  mdb_err err;
  
  nsCAutoString buf; buf.AppendInt(aValue);
  mdbYarn yarn = { (void *)buf.get(), buf.Length(), buf.Length(), 0, 0, nsnull };

  err = aRow->AddColumn(mEnv, aCol, &yarn);
  
  if (err != 0) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             nsAString& aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  aResult.Truncate(0);
  if (!yarn.mYarn_Fill)
    return NS_OK;
  
  switch (yarn.mYarn_Form) {
  case 0:                       // unicode
    if (mReverseByteOrder) {
      // The file is other-endian; we must byte-swap the result.
      PRUnichar *swapval;
      int len = yarn.mYarn_Fill / sizeof(PRUnichar);
      swapval = (PRUnichar *)malloc(yarn.mYarn_Fill);
      if (!swapval)
        return NS_ERROR_OUT_OF_MEMORY;
      SwapBytes((const PRUnichar *)yarn.mYarn_Buf, swapval, len);
      aResult.Assign(swapval, len);
      free(swapval);
    }
    else
      aResult.Assign((const PRUnichar *)yarn.mYarn_Buf, yarn.mYarn_Fill/sizeof(PRUnichar));
    break;

    // eventually we'll be supporting this in SetRowValue()
  case 1:                       // UTF8
    CopyUTF8toUTF16(Substring((const char*)yarn.mYarn_Buf,
                              (const char*)yarn.mYarn_Buf + yarn.mYarn_Fill),
                    aResult);
    break;

  default:
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

// Copy an array of 16-bit values, reversing the byte order.
void
nsSimpleGlobalHistory::SwapBytes(const PRUnichar *source, PRUnichar *dest,
                           PRInt32 aLen)
{
  PRUint16 c;
  const PRUnichar *inp;
  PRUnichar *outp;
  PRInt32 i;

  inp = source;
  outp = dest;
  for (i = 0; i < aLen; i++) {
    c = *inp++;
    *outp++ = (((c >> 8) & 0xff) | (c << 8));
  }
  return;
}
      
nsresult
nsSimpleGlobalHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             PRTime *aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  *aResult = LL_ZERO;
  
  if (!yarn.mYarn_Fill || !yarn.mYarn_Buf)
    return NS_OK;

  PR_sscanf((const char*)yarn.mYarn_Buf, "%lld", aResult);

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             PRInt32 *aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  if (yarn.mYarn_Buf)
    *aResult = atoi((char *)yarn.mYarn_Buf);
  else
    *aResult = 0;
  
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             nsACString& aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  const char* startPtr = (const char*)yarn.mYarn_Buf;
  if (startPtr)
    aResult.Assign(Substring(startPtr, startPtr + yarn.mYarn_Fill));
  else
    aResult.Truncate();
  
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::RowHasCell(nsIMdbRow *aRow, mdb_column aCol, PRBool* aResult)
{
  *aResult = HasCell(mEnv, aRow, aCol);
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::GetCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  if (!mTable) return NS_ERROR_FAILURE;

  mdb_err err = mTable->GetCount(mEnv, aCount);
  return (err == 0) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::SetPageTitle(nsIURI *aURI, const nsAString& aTitle)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aURI);

  const nsAFlatString& titleString = PromiseFlatString(aTitle);

  // skip about: URIs to avoid reading in the db (about:blank, especially)
  PRBool isAbout;
  rv = aURI->SchemeIs("about", &isAbout);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isAbout) return NS_OK;

  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  
  nsCAutoString URISpec;
  rv = aURI->GetSpec(URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMdbRow> row;
  rv = FindRow(kToken_URLColumn, URISpec.get(), getter_AddRefs(row));

  // if the row doesn't exist, we silently succeed
  if (rv == NS_ERROR_NOT_AVAILABLE) return NS_OK;
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the old title so we can notify observers
  nsAutoString oldtitle;
  rv = GetRowValue(row, kToken_NameColumn, oldtitle);
  if (NS_FAILED(rv)) return rv;

  SetRowValue(row, kToken_NameColumn, titleString.get());

  // ...and update observers
  NotifyObserversItemTitleChanged(row);

  SetDirty();

  return NS_OK;
}


NS_IMETHODIMP
nsSimpleGlobalHistory::RemovePage(nsIURI *aURI)
{
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_SUCCEEDED(rv))
    rv = RemovePageInternal(spec.get());
  return rv;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::RemovePagesFromHost(const nsACString &aHost, PRBool aEntireDomain)
{
  const nsCString &host = PromiseFlatCString(aHost);

  MatchHostData hostInfo;
  hostInfo.history = this;
  hostInfo.entireDomain = aEntireDomain;
  hostInfo.host = host.get();
  
  nsresult rv = RemoveMatchingRows(matchHostCallback, (void *)&hostInfo, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  mPagesRemoved = PR_TRUE;
  SetDirty();

  return NS_OK;
}

PRBool
nsSimpleGlobalHistory::MatchExpiration(nsIMdbRow *row, PRTime* expirationDate)
{
  nsresult rv;
  
  // hidden and typed urls always match because they're invalid,
  // so we want to expire them asap.  (if they were valid, they'd
  // have been unhidden -- see AddExistingPageToDatabase)
  if (HasCell(mEnv, row, kToken_HiddenColumn) && HasCell(mEnv, row, kToken_TypedColumn))
    return PR_TRUE;

  PRTime lastVisitedTime;
  rv = GetRowValue(row, kToken_LastVisitDateColumn, &lastVisitedTime);

  if (NS_FAILED(rv)) 
    return PR_FALSE;
  
  PRBool matches = LL_CMP(lastVisitedTime, <, (*expirationDate));
#if 0
  if (matches)
  {
    nsCAutoString url;
    GetRowValue(row, kToken_URLColumn, url);
    printf("  Removing history row for %s\n", url.get());
  }
#endif
  return matches;
}



PRBool
nsSimpleGlobalHistory::MatchHost(nsIMdbRow *aRow,
                           MatchHostData *hostInfo)
{
  mdb_err err;
  nsresult rv;

  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, kToken_URLColumn, &yarn);
  if (err != 0) return PR_FALSE;

  nsCOMPtr<nsIURI> uri;
  // do smart zero-termination
  const char* startPtr = (const char *)yarn.mYarn_Buf;
  rv = NS_NewURI(getter_AddRefs(uri),
                 Substring(startPtr, startPtr + yarn.mYarn_Fill));
  if (NS_FAILED(rv)) return PR_FALSE;

  nsCAutoString urlHost;
  rv = uri->GetHost(urlHost);
  if (NS_FAILED(rv)) return PR_FALSE;

  if (PL_strcmp(urlHost.get(), hostInfo->host) == 0)
    return PR_TRUE;

  // now try for a domain match, if necessary
  if (hostInfo->entireDomain) {
    // do a reverse-search to match the end of the string
    const char *domain = PL_strrstr(urlHost.get(), hostInfo->host);
    
    // now verify that we're matching EXACTLY the domain, and
    // not some random string inside the hostname
    if (domain && (PL_strcmp(domain, hostInfo->host) == 0))
      return PR_TRUE;
  }
  
  return PR_FALSE;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::RemoveAllPages()
{
  nsresult rv;

  rv = RemoveMatchingRows(matchAllCallback, nsnull, PR_FALSE);
  if (NS_FAILED(rv)) return rv;
  
  // Reset the file byte order.
  rv = InitByteOrder(PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  return Commit(kCompressCommit);
}

nsresult
nsSimpleGlobalHistory::RemoveMatchingRows(rowMatchCallback aMatchFunc,
                                    void *aClosure,
                                    PRBool inNotify)
{
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  if (!mTable) return NS_OK;

  mdb_err err;
  mdb_count count;
  err = mTable->GetCount(mEnv, &count);
  if (err != 0) return NS_ERROR_FAILURE;

  if (!inNotify)
    StartBatching();

  // Begin the batch.
  int marker;
  err = mTable->StartBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "unable to start batch");
  if (err != 0) return NS_ERROR_FAILURE;

  // XXX from here until end batch, no early returns!
  for (mdb_pos pos = count - 1; pos >= 0; --pos)
  {
    nsCOMPtr<nsIMdbRow> row;
    err = mTable->PosToRow(mEnv, pos, getter_AddRefs(row));
    NS_ASSERTION(err == 0, "unable to get row");
    if (err != 0)
      break;

    NS_ASSERTION(row != nsnull, "no row");
    if (! row)
      continue;

    // now we actually do the match. If this row doesn't match, loop again
    if (!(aMatchFunc)(row, aClosure))
      continue;

    if (inNotify)
      NotifyObserversItemRemoved(row);

#if 0
    nsCAutoString url;
    GetRowValue(row, kToken_URLColumn, url);
    printf("Removing row %s\n", url.get());
#endif

    // Officially cut the row *now*, before notifying any observers:
    // that way, any re-entrant calls won't find the row.
    err = mTable->CutRow(mEnv, row);
    NS_ASSERTION(err == 0, "couldn't cut row");
    if (err != 0)
      continue;
  
    // possibly avoid leakage
    err = row->CutAllColumns(mEnv);
    NS_ASSERTION(err == 0, "couldn't cut all columns");
    // we'll notify regardless of whether we could successfully
    // CutAllColumns or not.
    // XXX notify?
  }
  
  // Finish the batch.
  err = mTable->EndBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "error ending batch");

  if (!inNotify)
    EndBatching();

  return ( err == 0) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::IsVisited(nsIURI* aURI, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsresult rv;
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_NOT_INITIALIZED);

  nsCAutoString URISpec;
  rv = aURI->GetSpec(URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FindRow(kToken_URLColumn, URISpec.get(), nsnull);
  *_retval = NS_SUCCEEDED(rv);
  
  // Hidden, typed URIs haven't really been visited yet. They've only
  // been typed in and the actual load hasn't happened yet. We maintain
  // the list of hidden+typed URIs in memory in mTypedHiddenURIs because
  // the list will usually be small and checking the actual Mork row
  // would require several dynamic memory allocations.
  if (*_retval && mTypedHiddenURIs.Contains(URISpec))
  {
    *_retval = PR_FALSE;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::GetLastPageVisited(nsACString& _retval)
{ 
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);

  NS_ENSURE_STATE(mMetaRow);

  mdb_err err = GetRowValue(mMetaRow, kToken_LastPageVisited, _retval);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  return NS_OK;
}

#pragma mark -

NS_IMETHODIMP
nsSimpleGlobalHistory::GetMaxItemCount(PRUint32 *outCount)
{
  NS_ENSURE_ARG_POINTER(outCount);
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  if (!mTable) return NS_ERROR_FAILURE;

  mdb_err err = mTable->GetCount(mEnv, outCount);
  return (err == 0) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::GetItemEnumerator(nsISimpleEnumerator** outEnumerator)
{
  NS_ENSURE_ARG_POINTER(outEnumerator);
  *outEnumerator = nsnull;

  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  if (!mTable) return NS_ERROR_FAILURE;

  nsMdbTableAllRowsEnumerator* allRowsEnum = new nsMdbTableAllRowsEnumerator(this, kToken_HiddenColumn);
  NS_ADDREF(allRowsEnum);
  
  nsresult rv = allRowsEnum->Init(mEnv, mTable);
  if (NS_FAILED(rv))
  {
    NS_RELEASE(allRowsEnum);
    return rv;
  }
  
  *outEnumerator = allRowsEnum;
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::Flush()
{
  Sync();
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::AddObserver(nsIHistoryObserver* inObserver)
{
  if (mHistoryObservers.IndexOf(inObserver) == -1)
    return mHistoryObservers.AppendElement(inObserver);
    
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::RemoveObserver(nsIHistoryObserver* inObserver)
{
  mHistoryObservers.RemoveElement(inObserver);
  return NS_OK;
}


#pragma mark -

// Set the byte order in the history file.  The given string value should
// be either "BE" (big-endian) or "LE" (little-endian).
nsresult
nsSimpleGlobalHistory::SaveByteOrder(const char *aByteOrder)
{
  if (PL_strcmp(aByteOrder, "BE") != 0 && PL_strcmp(aByteOrder, "LE") != 0) {
    NS_WARNING("Invalid byte order argument.");
    return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_STATE(mMetaRow);

  mdb_err err = SetRowValue(mMetaRow, kToken_ByteOrder, aByteOrder);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  return NS_OK;
}

// Get the file byte order.
nsresult
nsSimpleGlobalHistory::GetByteOrder(char **_retval)
{ 
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);

  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_STATE(mMetaRow);

  nsCAutoString byteOrder;
  mdb_err err = GetRowValue(mMetaRow, kToken_ByteOrder, byteOrder);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  *_retval = ToNewCString(byteOrder);
  NS_ENSURE_TRUE(*_retval, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::HidePage(nsIURI *aURI)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aURI);

  nsCAutoString URISpec;
  rv = aURI->GetSpec(URISpec);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIMdbRow> row;

  rv = FindRow(kToken_URLColumn, URISpec.get(), getter_AddRefs(row));

  if (NS_FAILED(rv)) {
    // it hasn't been visited yet, but if one ever comes in, we need
    // to hide it when it is visited
    rv = AddURI(aURI, PR_FALSE, PR_FALSE, nsnull);
    if (NS_FAILED(rv)) return rv;
    
    rv = FindRow(kToken_URLColumn, URISpec.get(), getter_AddRefs(row));
    if (NS_FAILED(rv)) return rv;
  }

  rv = SetRowValue(row, kToken_HiddenColumn, 1);
  if (NS_FAILED(rv)) return rv;

  // now pretend as if this row was deleted (notify)

  SetDirty();
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::MarkPageAsTyped(nsIURI *aURI)
{
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMdbRow> row;
  rv = FindRow(kToken_URLColumn, spec.get(), getter_AddRefs(row));
  if (NS_FAILED(rv))
  {
    rv = AddNewPageToDatabase(spec.get(), GetNow(), nsnull, getter_AddRefs(row));
    NS_ENSURE_SUCCESS(rv, rv);

    // We don't know if this is a valid URI yet. Hide it until it finishes
    // loading.
    SetRowValue(row, kToken_HiddenColumn, 1);
    mTypedHiddenURIs.Put(spec);
  }
  
  rv = SetRowValue(row, kToken_TypedColumn, 1);
  if (NS_FAILED(rv)) return rv;

  SetDirty();
  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   Miscellaneous implementation methods
//

nsresult
nsSimpleGlobalHistory::Init()
{
  nsresult rv;

  // we'd like to get this pref when we need it, but at that point,
  // we can't get the pref service. register a pref observer so we update
  // if the pref changes

  if (!gPrefBranch)
  {
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = prefs->GetBranch(PREF_BRANCH_BASE, &gPrefBranch);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  gPrefBranch->GetIntPref(PREF_BROWSER_HISTORY_EXPIRE_DAYS, &mExpireDays);  
  gPrefBranch->GetBoolPref(PREF_AUTOCOMPLETE_ONLY_TYPED, &mAutocompleteOnlyTyped);
  nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(gPrefBranch);
  if (pbi) {
    pbi->AddObserver(PREF_AUTOCOMPLETE_ONLY_TYPED, this, PR_FALSE);
    pbi->AddObserver(PREF_BROWSER_HISTORY_EXPIRE_DAYS, this, PR_FALSE);
  }

  if (gRefCnt++ == 0)
  {
  }

  // register to observe profile changes
  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ASSERTION(observerService, "failed to get observer service");
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", PR_TRUE);
    observerService->AddObserver(this, "profile-do-change", PR_TRUE);
  }

  mTypedHiddenURIs.Init(3);

  return NS_OK;
}


nsresult
nsSimpleGlobalHistory::OpenDB()
{
  nsresult rv;

  if (mStore) return NS_OK;
  
  nsCOMPtr <nsIFile> historyFile;
  rv = NS_GetSpecialDirectory(NS_APP_HISTORY_50_FILE, getter_AddRefs(historyFile));
  NS_ENSURE_SUCCESS(rv, rv);

  static NS_DEFINE_CID(kMorkCID, NS_MORK_CID);
  nsCOMPtr<nsIMdbFactoryFactory> factoryfactory = do_CreateInstance(kMorkCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = factoryfactory->GetMdbFactory(&gMdbFactory);
  NS_ENSURE_SUCCESS(rv, rv);

  mdb_err err;

  err = gMdbFactory->MakeEnv(nsnull, &mEnv);
  mEnv->SetAutoClear(PR_TRUE);
  NS_ASSERTION((err == 0), "unable to create mdb env");
  if (err != 0) return NS_ERROR_FAILURE;

  // MDB requires native file paths

  nsCAutoString filePath;
  rv = historyFile->GetNativePath(filePath);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists = PR_TRUE;

  historyFile->Exists(&exists);

  if (!exists || NS_FAILED(rv = OpenExistingFile(gMdbFactory, filePath.get())))
  {
    // we couldn't open the file, so it's either corrupt or doesn't exist.
    // attempt to delete the file, but ignore the error
    NS_ASSERTION(0, "Failed to open history.dat file");
#if DEBUG
    nsCOMPtr<nsIFile> fileCopy;
    historyFile->Clone(getter_AddRefs(fileCopy));
    fileCopy->SetLeafName(NS_LITERAL_STRING("history_BAD.dat"));
    fileCopy->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    nsString uniqueFileName;
    fileCopy->GetLeafName(uniqueFileName);
    historyFile->MoveTo(nsnull, uniqueFileName);
#else
    historyFile->Remove(PR_FALSE);
#endif
    rv = OpenNewFile(gMdbFactory, filePath.get());
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // get the initial filesize. Used in Commit() to determine type of commit.
  rv = historyFile->GetFileSize(&mFileSizeOnDisk);
  if (NS_FAILED(rv)) {
    LL_I2L(mFileSizeOnDisk, 0);
  }
  
  // See if we need to byte-swap.
  InitByteOrder(PR_FALSE);

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::OpenExistingFile(nsIMdbFactory *factory, const char *filePath)
{
  mdb_err err;
  nsresult rv;
  mdb_bool canopen = 0;
  mdbYarn outfmt = { nsnull, 0, 0, 0, 0, nsnull };

  nsIMdbHeap* dbHeap = 0;
  mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable
  nsCOMPtr<nsIMdbFile> oldFile; // ensures file is released
  err = factory->OpenOldFile(mEnv, dbHeap, filePath,
                             dbFrozen, getter_AddRefs(oldFile));

  // don't assert, the file might just not be there
  if ((err !=0) || !oldFile) return NS_ERROR_FAILURE;

  err = factory->CanOpenFilePort(mEnv, oldFile, // the file to investigate
                                 &canopen, &outfmt);

  // XXX possible that format out of date, in which case we should
  // just re-write the file.
  if ((err !=0) || !canopen) return NS_ERROR_FAILURE;

  nsIMdbThumb* thumb = nsnull;
  mdbOpenPolicy policy = { { 0, 0 }, 0, 0 };

  err = factory->OpenFileStore(mEnv, dbHeap, oldFile, &policy, &thumb);
  if ((err !=0) || !thumb) return NS_ERROR_FAILURE;

  mdb_count total;
  mdb_count current;
  mdb_bool done;
  mdb_bool broken;

  do {
    err = thumb->DoMore(mEnv, &total, &current, &done, &broken);
  } while ((err == 0) && !broken && !done);

  if ((err == 0) && done) {
    err = factory->ThumbToOpenStore(mEnv, thumb, &mStore);
  }

  NS_IF_RELEASE(thumb);

  if (err != 0) return NS_ERROR_FAILURE;

  rv = CreateTokens();

  NS_ENSURE_SUCCESS(rv, rv);

  mdbOid oid = { kToken_HistoryRowScope, 1 };
  err = mStore->GetTable(mEnv, &oid, &mTable);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);
  if (!mTable) {
    NS_WARNING("Your history file is somehow corrupt.. deleting it.");
    return NS_ERROR_FAILURE;
  }

  err = mTable->GetMetaRow(mEnv, &oid, nsnull, getter_AddRefs(mMetaRow));
  if (err != 0)
    NS_WARNING("Could not get meta row\n");

  CheckHostnameEntries();

  if (err != 0) return NS_ERROR_FAILURE;
  
  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::OpenNewFile(nsIMdbFactory *factory, const char *filePath)
{
  nsresult rv;
  mdb_err err;
  
  nsIMdbHeap* dbHeap = 0;
  nsCOMPtr<nsIMdbFile> newFile; // ensures file is released
  err = factory->CreateNewFile(mEnv, dbHeap, filePath,
                               getter_AddRefs(newFile));

  if ((err != 0) || !newFile) return NS_ERROR_FAILURE;
  
  mdbOpenPolicy policy = { { 0, 0 }, 0, 0 };
  err = factory->CreateNewFileStore(mEnv, dbHeap, newFile, &policy, &mStore);
  
  if (err != 0) return NS_ERROR_FAILURE;
  
  rv = CreateTokens();
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the one and only table in the history db
  err = mStore->NewTable(mEnv, kToken_HistoryRowScope, kToken_HistoryKind, PR_TRUE, nsnull, &mTable);
  if (err != 0) return NS_ERROR_FAILURE;
  if (!mTable) return NS_ERROR_FAILURE;

  // Create the meta row.
  mdbOid oid = { kToken_HistoryRowScope, 1 };
  err = mTable->GetMetaRow(mEnv, &oid, nsnull, getter_AddRefs(mMetaRow));
  if (err != 0)
    NS_WARNING("Could not get meta row\n");

  // Force a commit now to get it written out.
  nsCOMPtr<nsIMdbThumb> thumb;
  err = mStore->LargeCommit(mEnv, getter_AddRefs(thumb));
  if (err != 0) return NS_ERROR_FAILURE;

  mdb_count total;
  mdb_count current;
  mdb_bool done;
  mdb_bool broken;

  do {
    err = thumb->DoMore(mEnv, &total, &current, &done, &broken);
  } while ((err == 0) && !broken && !done);

  if ((err != 0) || !done) return NS_ERROR_FAILURE;

  return NS_OK;
}

// Set the history file byte order if necessary, and determine if
// we need to byte-swap Unicode values.
// If the force argument is true, the file byte order will be set
// to that of this machine.
nsresult
nsSimpleGlobalHistory::InitByteOrder(PRBool aForce)
{
#ifdef IS_LITTLE_ENDIAN
  NS_NAMED_LITERAL_CSTRING(machine_byte_order, "LE");
#endif
#ifdef IS_BIG_ENDIAN
  NS_NAMED_LITERAL_CSTRING(machine_byte_order, "BE");
#endif
  nsXPIDLCString file_byte_order;
  nsresult rv = NS_OK;

  if (!aForce)
    rv = GetByteOrder(getter_Copies(file_byte_order));
  if (aForce || NS_FAILED(rv) ||
      !(file_byte_order.EqualsLiteral("BE") ||
        file_byte_order.EqualsLiteral("LE"))) {
    // Byte order is not yet set, or needs to be reset; initialize it.
    mReverseByteOrder = PR_FALSE;
    rv = SaveByteOrder(machine_byte_order.get());
    if (NS_FAILED(rv))
      return rv;
  }
  else
    mReverseByteOrder = !file_byte_order.Equals(machine_byte_order);

  return NS_OK;
}

// for each row, we need to parse out the hostname from the url
// then store it in a column
nsresult
nsSimpleGlobalHistory::CheckHostnameEntries()
{
  nsresult rv = NS_OK;

  mdb_err err;

  nsCOMPtr<nsIMdbTableRowCursor> cursor;
  nsCOMPtr<nsIMdbRow> row;

  err = mTable->GetTableRowCursor(mEnv, -1, getter_AddRefs(cursor));
  if (err != 0) return NS_ERROR_FAILURE;

  int marker;
  err = mTable->StartBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "unable to start batch");
  if (err != 0) return NS_ERROR_FAILURE;
  
  mdb_pos pos;
  err = cursor->NextRow(mEnv, getter_AddRefs(row), &pos);
  if (err != 0) return NS_ERROR_FAILURE;

  // comment out this code to rebuild the hostlist at startup
#if 1
  // bail early if the first row has a hostname
  if (row) {
    nsCAutoString hostname;
    rv = GetRowValue(row, kToken_HostnameColumn, hostname);
    if (NS_SUCCEEDED(rv) && !hostname.IsEmpty())
      return NS_OK;
  }
#endif
  
  // cached variables used in the loop
  nsCAutoString url;
  nsXPIDLCString hostname;

  nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID);
  if (!ioService) return NS_ERROR_FAILURE;
  

  while (row) {
#if 0
    rv = GetRowValue(row, kToken_URLColumn, url);
    if (NS_FAILED(rv)) break;

    ioService->ExtractUrlPart(url, nsIIOService::url_Host, 0, 0, getter_Copies(hostname));

    SetRowValue(row, kToken_HostnameColumn, hostname);
    
#endif

    // to be turned on when we're confident in mork's ability
    // to handle yarn forms properly
#if 0
    nsAutoString title;
    rv = GetRowValue(row, kToken_NameColumn, title);
    // reencode back into UTF8
    if (NS_SUCCEEDED(rv))
      SetRowValue(row, kToken_NameColumn, title.get());
#endif
    cursor->NextRow(mEnv, getter_AddRefs(row), &pos);
  }

  // Finish the batch.
  err = mTable->EndBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "error ending batch");
  
  return rv;
}

nsresult
nsSimpleGlobalHistory::CreateTokens()
{
  mdb_err err;

  NS_PRECONDITION(mStore != nsnull, "not initialized");
  if (! mStore)
    return NS_ERROR_NOT_INITIALIZED;

  err = mStore->StringToToken(mEnv, "ns:history:db:row:scope:history:all", &kToken_HistoryRowScope);
  if (err != 0) return NS_ERROR_FAILURE;
  
  err = mStore->StringToToken(mEnv, "ns:history:db:table:kind:history", &kToken_HistoryKind);
  if (err != 0) return NS_ERROR_FAILURE;
  
  err = mStore->StringToToken(mEnv, "URL", &kToken_URLColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Referrer", &kToken_ReferrerColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "LastVisitDate", &kToken_LastVisitDateColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "FirstVisitDate", &kToken_FirstVisitDateColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "VisitCount", &kToken_VisitCountColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Name", &kToken_NameColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Hostname", &kToken_HostnameColumn);
  if (err != 0) return NS_ERROR_FAILURE;
  
  err = mStore->StringToToken(mEnv, "Hidden", &kToken_HiddenColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Typed", &kToken_TypedColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  // meta-data tokens
  err = mStore->StringToToken(mEnv, "LastPageVisited", &kToken_LastPageVisited);
  err = mStore->StringToToken(mEnv, "ByteOrder", &kToken_ByteOrder);

  return NS_OK;
}


nsresult
nsSimpleGlobalHistory::Commit(eCommitType commitType)
{
  if (!mStore || !mTable)
    return NS_OK;

  nsresult	err = NS_OK;
  nsCOMPtr<nsIMdbThumb> thumb;

  if (commitType == kLargeCommit || commitType == kSessionCommit)
  {
    mdb_percent outActualWaste = 0;
    mdb_bool outShould;
    if (mStore) 
    {
      // check how much space would be saved by doing a compress commit.
      // If it's more than 30%, go for it.
      // N.B. - I'm not sure this calls works in Mork for all cases.
      err = mStore->ShouldCompress(mEnv, 30, &outActualWaste, &outShould);
      if (NS_SUCCEEDED(err) && outShould)
      {
          commitType = kCompressCommit;
      }
      else
      {
        mdb_count count;
        err = mTable->GetCount(mEnv, &count);
        // Since Mork's shouldCompress doesn't work, we need to look
        // at the file size and the number of rows, and make a stab
        // at guessing if we've got a lot of deleted rows. The file
        // size is the size when we opened the db, and we really want
        // it to be the size after we've written out the file,
        // but I think this is a good enough approximation.
        if (count > 0)
        {
          PRInt64 numRows;
          PRInt64 bytesPerRow;
          PRInt64 desiredAvgRowSize;

          LL_UI2L(numRows, count);
          LL_DIV(bytesPerRow, mFileSizeOnDisk, numRows);
          LL_I2L(desiredAvgRowSize, 400);
          if (LL_CMP(bytesPerRow, >, desiredAvgRowSize))
            commitType = kCompressCommit;
        }
      }
    }
  }

  switch (commitType)
  {
    case kLargeCommit:
      err = mStore->LargeCommit(mEnv, getter_AddRefs(thumb));
      break;
    case kSessionCommit:
      err = mStore->SessionCommit(mEnv, getter_AddRefs(thumb));
      break;
    case kCompressCommit:
      err = mStore->CompressCommit(mEnv, getter_AddRefs(thumb));
      break;
  }
  if (err == 0) {
    mdb_count total;
    mdb_count current;
    mdb_bool done;
    mdb_bool broken;

    do {
      err = thumb->DoMore(mEnv, &total, &current, &done, &broken);
    } while ((err == 0) && !broken && !done);
  }

  if (err != 0) // mork doesn't return NS error codes. Yet.
    return NS_ERROR_FAILURE;

  return NS_OK;
}

// if notify is true, we'll notify observers of deleted rows.
// If we're shutting down history, then (maybe?) we don't
// need or want to notify rdf.
nsresult
nsSimpleGlobalHistory::ExpireEntries(PRBool inNotify)
{
  PRTime expirationDate;
  PRInt64 microSecondsPerSecond, secondsInDays, microSecondsInExpireDays;
  
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_UI2L(secondsInDays, 60 * 60 * 24 * mExpireDays);
  LL_MUL(microSecondsInExpireDays, secondsInDays, microSecondsPerSecond);
  LL_SUB(expirationDate, GetNow(), microSecondsInExpireDays);

  MatchExpirationData expiration;
  expiration.history = this;
  expiration.expirationDate = expirationDate;
  
  return RemoveMatchingRows(matchExpirationCallback, (void *)&expiration, inNotify);
}

void
nsSimpleGlobalHistory::Sync()
{
  if (mDirty)
    Commit(mPagesRemoved ? kCompressCommit : kLargeCommit);
  
  mPagesRemoved = PR_FALSE;
  mDirty = PR_FALSE;
  mSyncTimer = nsnull;
}

// when we're dirty, we want to make sure we sync again soon,
// but make sure that we don't keep syncing if someone is surfing
// a lot, so cancel the existing timer if any is still waiting to fire
nsresult
nsSimpleGlobalHistory::SetDirty()
{
  nsresult rv;

  if (mSyncTimer)
    mSyncTimer->Cancel();

  if (!mSyncTimer) {
    mSyncTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) return rv;
  }
  
  mDirty = PR_TRUE;
  mSyncTimer->InitWithFuncCallback(FireSyncTimer, this, HISTORY_SYNC_TIMEOUT, nsITimer::TYPE_ONE_SHOT);

  return NS_OK;
}

/* static */
void
nsSimpleGlobalHistory::FireSyncTimer(nsITimer *aTimer, void *aClosure)
{
  ((nsSimpleGlobalHistory *)aClosure)->Sync();
}


PRTime
nsSimpleGlobalHistory::GetNow()
{
  return PR_Now();
}

nsresult
nsSimpleGlobalHistory::CloseDB()
{
  if (!mStore)
    return NS_OK;

  mdb_err err;

  ExpireEntries(PR_FALSE /* don't notify */);
  err = Commit(kSessionCommit);

  // order is important here - logically smallest objects first
  mMetaRow = nsnull;
  
  if (mTable)
    mTable->Release();

  mStore->Release();

  if (mEnv)
    mEnv->Release();

  mTable = nsnull;
  mEnv = nsnull;
  mStore = nsnull;

  return NS_OK;
}

nsresult
nsSimpleGlobalHistory::FindRow(mdb_column aCol,
                         const char *aValue, nsIMdbRow **aResult)
{
  if (! mStore)
    return NS_ERROR_NOT_INITIALIZED;

  mdb_err err;
  PRInt32 len = PL_strlen(aValue);
  mdbYarn yarn = { (void*) aValue, len, len, 0, 0, nsnull };

  mdbOid rowId;
  nsCOMPtr<nsIMdbRow> row;
  err = mStore->FindRow(mEnv, kToken_HistoryRowScope,
                        aCol, &yarn,
                        &rowId, getter_AddRefs(row));


  if (!row) return NS_ERROR_NOT_AVAILABLE;
  
  
  // make sure it's actually stored in the main table
  mdb_bool hasRow;
  mTable->HasRow(mEnv, row, &hasRow);

  if (!hasRow) return NS_ERROR_NOT_AVAILABLE;
  
  if (aResult) {
    *aResult = row;
    (*aResult)->AddRef();
  }
    
  return NS_OK;
}

mdb_column
nsSimpleGlobalHistory::TokenForColumn(EColumn inColumn)
{
  switch (inColumn)
  {
    case eColumnURL:                return kToken_URLColumn;
    case eColumnReferrer:           return kToken_ReferrerColumn;
    case eColumnLastVisitDate:      return kToken_LastVisitDateColumn;
    case eColumnFirstVisitDate:     return kToken_FirstVisitDateColumn;
    case eColumnVisitCount:         return kToken_VisitCountColumn;
    case eColumnTitle:              return kToken_NameColumn;
    case eColumnHostname:           return kToken_HostnameColumn;
    case eColumnHidden:             return kToken_HiddenColumn;
    case eColumnTyped:              return kToken_TypedColumn;
    default:
      NS_ERROR("Unknown column type");
  }
  return 0;
}

//
// determines if the row matches the given terms, used above
//
PRBool
nsSimpleGlobalHistory::RowMatches(nsIMdbRow *aRow, SearchQueryData *aQuery)
{
  PRUint32 length = aQuery->terms.Count();
  PRUint32 i;

  for (i = 0; i < length; i++)
  {
    HistorySearchTerm *term = (HistorySearchTerm*)aQuery->terms[i];

    if (!term->datasource.Equals("history"))
      continue;                 // we only match against history queries
    
    // use callback if it exists
    if (term->match) {
      // queue up some values just in case callback needs it
      // (how would we do this dynamically?)
      MatchSearchTermData matchSearchTerm = { mEnv, mStore, term, PR_FALSE, 0, 0 };
      
      if (!term->match(aRow, (void *)&matchSearchTerm))
        return PR_FALSE;
    } else {
      mdb_err err;

      mdb_column property_column;
      nsCAutoString property_name(term->property);
      property_name.Append(char(0));
      
      err = mStore->QueryToken(mEnv, property_name.get(), &property_column);
      if (err != 0) {
        NS_WARNING("Unrecognized column!");
        continue;               // assume we match???
      }
      
      // match the term directly against the column?
      mdbYarn yarn;
      err = aRow->AliasCellYarn(mEnv, property_column, &yarn);
      if (err != 0 || !yarn.mYarn_Buf) return PR_FALSE;

      const char* startPtr;
      PRInt32 yarnLength = yarn.mYarn_Fill;
      nsCAutoString titleStr;
      if (property_column == kToken_NameColumn) {
        AppendUTF16toUTF8(Substring((const PRUnichar*)yarn.mYarn_Buf,
                                    (const PRUnichar*)yarn.mYarn_Buf + yarnLength),
                          titleStr);
        startPtr = titleStr.get();
        yarnLength = titleStr.Length();
      }
      else {
        // account for null strings
        if (yarn.mYarn_Buf)
          startPtr = (const char *)yarn.mYarn_Buf;
        else 
          startPtr = "";
      }
      
      const nsASingleFragmentCString& rowVal =
          Substring(startPtr, startPtr + yarnLength);

      // set up some iterators
      nsASingleFragmentCString::const_iterator start, end;
      rowVal.BeginReading(start);
      rowVal.EndReading(end);
  
      NS_ConvertUCS2toUTF8 utf8Value(term->text);
      
      if (term->method.Equals("is")) {
        if (!utf8Value.Equals(rowVal, nsCaseInsensitiveCStringComparator()))
          return PR_FALSE;
      }

      else if (term->method.Equals("isnot")) {
        if (utf8Value.Equals(rowVal, nsCaseInsensitiveCStringComparator()))
          return PR_FALSE;
      }

      else if (term->method.Equals("contains")) {
        if (!FindInReadable(utf8Value, start, end, nsCaseInsensitiveCStringComparator()))
          return PR_FALSE;
      }

      else if (term->method.Equals("doesntcontain")) {
        if (FindInReadable(utf8Value, start, end, nsCaseInsensitiveCStringComparator()))
          return PR_FALSE;
      }

      else if (term->method.Equals("startswith")) {
        // need to make sure that the found string is 
        // at the beginning of the string
        nsACString::const_iterator real_start = start;
        if (!(FindInReadable(utf8Value, start, end, nsCaseInsensitiveCStringComparator()) &&
              real_start == start))
          return PR_FALSE;
      }

      else if (term->method.Equals("endswith")) {
        // need to make sure that the found string ends
        // at the end of the string
        nsACString::const_iterator real_end = end;
        if (!(RFindInReadable(utf8Value, start, end, nsCaseInsensitiveCStringComparator()) &&
              real_end == end))
          return PR_FALSE;
      }

      else {
        NS_WARNING("Unrecognized search method in SearchEnumerator::RowMatches");
        // don't handle other match types like isgreater/etc yet,
        // so assume the match failed and bail
        return PR_FALSE;
      }
      
    }
  }
  
  // we've gone through each term and didn't bail, so they must have
  // all matched!
  return PR_TRUE;
}



#pragma mark -

NS_IMETHODIMP
nsSimpleGlobalHistory::Observe(nsISupports *aSubject, 
                         const char *aTopic,
                         const PRUnichar *aSomeData)
{
  nsresult rv;
  // pref changing - update member vars
  if (!nsCRT::strcmp(aTopic, "nsPref:changed")) {
    NS_ENSURE_STATE(gPrefBranch);

    // expiration date
    if (!nsCRT::strcmp(aSomeData, NS_LITERAL_STRING(PREF_BROWSER_HISTORY_EXPIRE_DAYS).get())) {
      gPrefBranch->GetIntPref(PREF_BROWSER_HISTORY_EXPIRE_DAYS, &mExpireDays);
    }
    else if (!nsCRT::strcmp(aSomeData, NS_LITERAL_STRING(PREF_AUTOCOMPLETE_ONLY_TYPED).get())) {
      gPrefBranch->GetBoolPref(PREF_AUTOCOMPLETE_ONLY_TYPED, &mAutocompleteOnlyTyped);
    }
  }
  else if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    rv = CloseDB();    
    if (!nsCRT::strcmp(aSomeData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      nsCOMPtr <nsIFile> historyFile;
      rv = NS_GetSpecialDirectory(NS_APP_HISTORY_50_FILE, getter_AddRefs(historyFile));
      if (NS_SUCCEEDED(rv))
        rv = historyFile->Remove(PR_FALSE);
    }
  }
  else if (!nsCRT::strcmp(aTopic, "profile-do-change"))
    rv = OpenDB();

  return NS_OK;
}

#pragma mark -


//----------------------------------------------------------------------
//
// HistoryAutoCompleteEnumerator
//
//   Implementation

// HistoryAutoCompleteEnumerator - for searching for a partial url match  
class HistoryAutoCompleteEnumerator : public nsHistoryMdbTableEnumerator
{
protected:
  nsSimpleGlobalHistory* mHistory;
  mdb_column mURLColumn;
  mdb_column mHiddenColumn;
  mdb_column mTypedColumn;
  mdb_column mCommentColumn;
  AutocompleteExcludeData* mExclude;
  const nsAString& mSelectValue;
  PRBool mMatchOnlyTyped;

  virtual ~HistoryAutoCompleteEnumerator()
  {
  }

public:
  HistoryAutoCompleteEnumerator(nsSimpleGlobalHistory* aHistory,
                         mdb_column aURLColumn,
                         mdb_column aCommentColumn,
                         mdb_column aHiddenColumn,
                         mdb_column aTypedColumn,
                         PRBool aMatchOnlyTyped,
                         const nsAString& aSelectValue,
                         AutocompleteExcludeData* aExclude) :
    mHistory(aHistory),
    mURLColumn(aURLColumn),
    mHiddenColumn(aHiddenColumn),
    mTypedColumn(aTypedColumn),
    mCommentColumn(aCommentColumn),
    mExclude(aExclude),
    mSelectValue(aSelectValue), 
    mMatchOnlyTyped(aMatchOnlyTyped) {}

protected:
  virtual PRBool    IsResult(nsIMdbRow* aRow);
  virtual nsresult  ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult);
};


PRBool
HistoryAutoCompleteEnumerator::IsResult(nsIMdbRow* aRow)
{
  if (!HasCell(mEnv, aRow, mTypedColumn))
  {
    if (mMatchOnlyTyped || HasCell(mEnv, aRow, mHiddenColumn))
      return PR_FALSE;
  }

  nsCAutoString url;
  mHistory->GetRowValue(aRow, mURLColumn, url);

  NS_ConvertUTF8toUCS2 utf8Url(url);

  PRBool result = mHistory->AutoCompleteCompare(utf8Url, mSelectValue, mExclude); 
  
  return result;
}

nsresult
HistoryAutoCompleteEnumerator::ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult)
{
  nsCAutoString url;
  mHistory->GetRowValue(aRow, mURLColumn, url);
  nsAutoString comments;
  mHistory->GetRowValue(aRow, mCommentColumn, comments);

  nsCOMPtr<nsIAutoCompleteItem> newItem(do_CreateInstance(NS_AUTOCOMPLETEITEM_CONTRACTID));
  NS_ENSURE_TRUE(newItem, NS_ERROR_FAILURE);

  newItem->SetValue(NS_ConvertUTF8toUCS2(url.get()));
  newItem->SetParam(aRow);
  newItem->SetComment(comments.get());

  *aResult = newItem;
  NS_ADDREF(*aResult);
  return NS_OK;
}

// Number of prefixes used in the autocomplete sort comparison function
#define AUTOCOMPLETE_PREFIX_LIST_COUNT 6
// Size of visit count boost to give to urls which are sites or paths
#define AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST 5

// AutoCompleteSortClosure - used to pass info into 
// AutoCompleteSortComparison from the NS_QuickSort() function
struct AutoCompleteSortClosure
{
  nsSimpleGlobalHistory*  history;
  size_t                  prefixCount;
  const nsAFlatString*    prefixes[AUTOCOMPLETE_PREFIX_LIST_COUNT];
};

#pragma mark -

//----------------------------------------------------------------------
//
// nsIAutoCompleteSession implementation
//

NS_IMETHODIMP 
nsSimpleGlobalHistory::OnStartLookup(const PRUnichar *searchString,
                               nsIAutoCompleteResults *previousSearchResult,
                               nsIAutoCompleteListener *listener)
{
  NS_ASSERTION(searchString, "searchString can't be null, fix your caller");
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_STATE(gPrefBranch);

  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);

  PRBool enabled = PR_FALSE;
  gPrefBranch->GetBoolPref(PREF_AUTOCOMPLETE_ENABLED, &enabled);      

  if (!enabled || searchString[0] == 0) {
    listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
    return NS_OK;
  }
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsIAutoCompleteResults> results;
  results = do_CreateInstance(NS_AUTOCOMPLETERESULTS_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoCompleteStatus status = nsIAutoCompleteStatus::failed;

  // if the search string is empty after it has had prefixes removed, then 
  // there is no need to proceed with the search
  nsAutoString cut(searchString);
  AutoCompleteCutPrefix(cut, nsnull);
  if (cut.IsEmpty()) {
    listener->OnAutoComplete(results, status);
    return NS_OK;
  }
  
  // pass string through filter and then determine which prefixes to exclude
  // when chopping prefixes off of history urls during comparison
  nsString filtered = AutoCompletePrefilter(nsDependentString(searchString));
  AutocompleteExcludeData exclude;
  AutoCompleteGetExcludeInfo(filtered, &exclude);
  
  // perform the actual search here
  rv = AutoCompleteSearch(filtered, &exclude, previousSearchResult, results);

  // describe the search results
  if (NS_SUCCEEDED(rv)) {
  
    results->SetSearchString(searchString);
    results->SetDefaultItemIndex(0);
  
    // determine if we have found any matches or not
    nsCOMPtr<nsISupportsArray> array;
    rv = results->GetItems(getter_AddRefs(array));
    if (NS_SUCCEEDED(rv)) {
      PRUint32 nbrOfItems;
      rv = array->Count(&nbrOfItems);
      if (NS_SUCCEEDED(rv)) {
        if (nbrOfItems >= 1) {
          status = nsIAutoCompleteStatus::matchFound;
        } else {
          status = nsIAutoCompleteStatus::noMatch;
        }
      }
    }
    
    // notify the listener
    listener->OnAutoComplete(results, status);
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsSimpleGlobalHistory::OnStopLookup()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSimpleGlobalHistory::OnAutoComplete(const PRUnichar *searchString,
                                nsIAutoCompleteResults *previousSearchResult,
                                nsIAutoCompleteListener *listener)
{
  return NS_OK;
}

//----------------------------------------------------------------------
//
// AutoComplete stuff
//

nsresult
nsSimpleGlobalHistory::AutoCompleteSearch(const nsAString& aSearchString,
                                    AutocompleteExcludeData* aExclude,
                                    nsIAutoCompleteResults* aPrevResults,
                                    nsIAutoCompleteResults* aResults)
{
  // determine if we can skip searching the whole history and only search
  // through the previous search results
  PRBool searchPrevious = PR_FALSE;
  if (aPrevResults) {
    nsXPIDLString prevURL;
    aPrevResults->GetSearchString(getter_Copies(prevURL));
    // if search string begins with the previous search string, it's a go
    searchPrevious = StringBeginsWith(aSearchString, prevURL);
  }
    
  nsCOMPtr<nsISupportsArray> resultItems;
  nsresult rv = aResults->GetItems(getter_AddRefs(resultItems));

  if (searchPrevious) {
    // searching through the previous results...
    
    nsCOMPtr<nsISupportsArray> prevResultItems;
    aPrevResults->GetItems(getter_AddRefs(prevResultItems));
    
    PRUint32 count;
    prevResultItems->Count(&count);
    for (PRUint32 i = 0; i < count; ++i) {
      nsCOMPtr<nsIAutoCompleteItem> item;
      prevResultItems->GetElementAt(i, getter_AddRefs(item));

      // make a copy of the value because AutoCompleteCompare
      // is destructive
      nsAutoString url;
      item->GetValue(url);
      
      if (AutoCompleteCompare(url, aSearchString, aExclude))
        resultItems->AppendElement(item);
    }    
  } else {
    // searching through the entire history...
        
    // prepare the search enumerator
    HistoryAutoCompleteEnumerator* enumerator;
    enumerator = new HistoryAutoCompleteEnumerator(this, kToken_URLColumn, 
                                            kToken_NameColumn,
                                            kToken_HiddenColumn,
                                            kToken_TypedColumn,
                                            mAutocompleteOnlyTyped,
                                            aSearchString, aExclude);
    
    nsCOMPtr<nsISupports> kungFuDeathGrip(enumerator);

    rv = enumerator->Init(mEnv, mTable);
    if (NS_FAILED(rv)) return rv;
  
    // store hits in an auto array initially
    nsAutoVoidArray array;
      
    // not using nsCOMPtr here to avoid time spent
    // refcounting while passing these around between the 3 arrays
    nsISupports* entry; 

    // step through the enumerator to get the items into 'array'
    // because we don't know how many items there will be
    PRBool hasMore;
    while (PR_TRUE) {
      enumerator->HasMoreElements(&hasMore);
      if (!hasMore) break;
      
      // addref's each entry as it enters 'array'
      enumerator->GetNext(&entry);
      array.AppendElement(entry);
    }

    // turn auto array into flat array for quick sort, now that we
    // know how many items there are
    PRUint32 count = array.Count();
    nsIAutoCompleteItem** items = new nsIAutoCompleteItem*[count];
    PRUint32 i;
    for (i = 0; i < count; ++i)
      items[i] = (nsIAutoCompleteItem*)array.ElementAt(i);
    
    // Setup the structure we pass into the sort function,
    // including a set of url prefixes to ignore.   These prefixes 
    // must match with the logic in nsGlobalHistory::nsGlobalHistory().
    NS_NAMED_LITERAL_STRING(prefixHWStr, "http://www.");
    NS_NAMED_LITERAL_STRING(prefixHStr, "http://");
    NS_NAMED_LITERAL_STRING(prefixHSWStr, "https://www.");
    NS_NAMED_LITERAL_STRING(prefixHSStr, "https://");
    NS_NAMED_LITERAL_STRING(prefixFFStr, "ftp://ftp.");
    NS_NAMED_LITERAL_STRING(prefixFStr, "ftp://");

    // note: the number of prefixes stored in the closure below 
    // must match with the constant AUTOCOMPLETE_PREFIX_LIST_COUNT
    AutoCompleteSortClosure closure;
    closure.history = this;
    closure.prefixCount = AUTOCOMPLETE_PREFIX_LIST_COUNT;
    closure.prefixes[0] = &prefixHWStr;
    closure.prefixes[1] = &prefixHStr;
    closure.prefixes[2] = &prefixHSWStr;
    closure.prefixes[3] = &prefixHSStr;
    closure.prefixes[4] = &prefixFFStr;
    closure.prefixes[5] = &prefixFStr;

    // sort it
    NS_QuickSort(items, count, sizeof(nsIAutoCompleteItem*),
                 AutoCompleteSortComparison,
                 NS_STATIC_CAST(void*, &closure));
  
    // place the sorted array into the autocomplete results
    for (i = 0; i < count; ++i) {
      nsISupports* item = (nsISupports*)items[i];
      resultItems->AppendElement(item);
      NS_IF_RELEASE(item); // release manually since we didn't use nsCOMPtr above
    }
    
    delete[] items;
  }
    
  return NS_OK;
}

// If aURL begins with a protocol or domain prefix from our lists,
// then mark their index in an AutocompleteExcludeData struct.
void
nsSimpleGlobalHistory::AutoCompleteGetExcludeInfo(const nsAString& aURL, AutocompleteExcludeData* aExclude)
{
  aExclude->schemePrefix = -1;
  aExclude->hostnamePrefix = -1;
  
  PRInt32 index = 0;
  PRInt32 i;
  for (i = 0; i < mIgnoreSchemes.Count(); ++i) {
    nsString* string = mIgnoreSchemes.StringAt(i);    
    if (StringBeginsWith(aURL, *string)) {
      aExclude->schemePrefix = i;
      index = string->Length();
      break;
    }
  }
  
  for (i = 0; i < mIgnoreHostnames.Count(); ++i) {
    nsString* string = mIgnoreHostnames.StringAt(i);    
    if (Substring(aURL, index, string->Length()).Equals(*string)) {
      aExclude->hostnamePrefix = i;
      break;
    }
  }
}

// Cut any protocol and domain prefixes from aURL, except for those which
// are specified in aExclude
void
nsSimpleGlobalHistory::AutoCompleteCutPrefix(nsAString& aURL, AutocompleteExcludeData* aExclude)
{
  // This comparison is case-sensitive.  Therefore, it assumes that aUserURL is a 
  // potential URL whose host name is in all lower case.
  PRInt32 idx = 0;
  PRInt32 i;
  for (i = 0; i < mIgnoreSchemes.Count(); ++i) {
    if (aExclude && i == aExclude->schemePrefix)
      continue;
    nsString* string = mIgnoreSchemes.StringAt(i);    
    if (StringBeginsWith(aURL, *string)) {
      idx = string->Length();
      break;
    }
  }

  if (idx > 0)
    aURL.Cut(0, idx);

  idx = 0;
  for (i = 0; i < mIgnoreHostnames.Count(); ++i) {
    if (aExclude && i == aExclude->hostnamePrefix)
      continue;
    nsString* string = mIgnoreHostnames.StringAt(i);    
    if (StringBeginsWith(aURL, *string)) {
      idx = string->Length();
      break;
    }
  }

  if (idx > 0)
    aURL.Cut(0, idx);
}

nsString
nsSimpleGlobalHistory::AutoCompletePrefilter(const nsAString& aSearchString)
{
  nsAutoString url(aSearchString);

  PRInt32 slash = url.FindChar('/', 0);
  if (slash >= 0) {
    // if user is typing a url but has already typed past the host,
    // then convert the host to lowercase
    nsAutoString host;
    url.Left(host, slash);
    ToLowerCase(host);
    url.Assign(host + Substring(url, slash, url.Length()-slash));
  } else {
    // otherwise, assume the user could still be typing the host, and
    // convert everything to lowercase
    ToLowerCase(url);
  }
  
  return nsString(url);
}

PRBool
nsSimpleGlobalHistory::AutoCompleteCompare(nsAString& aHistoryURL, 
                                     const nsAString& aUserURL, 
                                     AutocompleteExcludeData* aExclude)
{
  AutoCompleteCutPrefix(aHistoryURL, aExclude);
  
  return StringBeginsWith(aHistoryURL, aUserURL);
}

int PR_CALLBACK 
nsSimpleGlobalHistory::AutoCompleteSortComparison(const void *v1, const void *v2,
                                            void *closureVoid) 
{
  //
  // NOTE: The design and reasoning behind the following autocomplete 
  // sort implementation is documented in bug 78270.
  //

  // cast our function parameters back into their real form
  nsIAutoCompleteItem *item1 = *(nsIAutoCompleteItem**) v1;
  nsIAutoCompleteItem *item2 = *(nsIAutoCompleteItem**) v2;
  AutoCompleteSortClosure* closure = 
      NS_STATIC_CAST(AutoCompleteSortClosure*, closureVoid);

  // get history rows
  nsCOMPtr<nsIMdbRow> row1, row2;
  item1->GetParam(getter_AddRefs(row1));
  item2->GetParam(getter_AddRefs(row2));

  // get visit counts - we're ignoring all errors from GetRowValue(), 
  // and relying on default values
  PRInt32 item1visits = 0, item2visits = 0;
  closure->history->GetRowValue(row1, 
                                closure->history->kToken_VisitCountColumn, 
                                &item1visits);
  closure->history->GetRowValue(row2, 
                                closure->history->kToken_VisitCountColumn, 
                                &item2visits);

  // get URLs
  nsAutoString url1, url2;
  item1->GetValue(url1);
  item2->GetValue(url2);

  // Favour websites and webpaths more than webpages by boosting 
  // their visit counts.  This assumes that URLs have been normalized, 
  // appending a trailing '/'.
  // 
  // We use addition to boost the visit count rather than multiplication 
  // since we want URLs with large visit counts to remain pretty much 
  // in raw visit count order - we assume the user has visited these urls
  // often for a reason and there shouldn't be a problem with putting them 
  // high in the autocomplete list regardless of whether they are sites/
  // paths or pages.  However for URLs visited only a few times, sites 
  // & paths should be presented before pages since they are generally 
  // more likely to be visited again.
  //
  PRBool isPath1 = PR_FALSE, isPath2 = PR_FALSE;
  if (!url1.IsEmpty())
  {
    // url is a site/path if it has a trailing slash
    isPath1 = (url1.Last() == PRUnichar('/'));
    if (isPath1)
      item1visits += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;
  }
  if (!url2.IsEmpty())
  {
    // url is a site/path if it has a trailing slash
    isPath2 = (url2.Last() == PRUnichar('/'));
    if (isPath2)
      item2visits += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;
  }

  // primary sort by visit count
  if (item1visits != item2visits)
  {
    // return visit count comparison
    return item2visits - item1visits;
  }
  else
  {
    // Favour websites and webpaths more than webpages
    if (isPath1 && !isPath2) return -1; // url1 is a website/path, url2 isn't
    if (!isPath1 && isPath2) return  1; // url1 isn't a website/path, url2 is

    // We have two websites/paths.. ignore "http[s]://[www.]" & "ftp://[ftp.]"
    // prefixes.  Find a starting position in the string, just past any of the 
    // above prefixes.  Only check for the prefix once, in the far left of the 
    // string - it is assumed there is no whitespace.
    PRInt32 postPrefix1 = 0, postPrefix2 = 0;

    size_t i;
    // iterate through our prefixes looking for a match
    for (i=0; i<closure->prefixCount; i++)
    {
      // Check if string is prefixed.  Note: the parameters of the Find() 
      // method specify the url is searched at the 0th character and if there
      // is no match the rest of the url is not searched.
      if (url1.Find((*closure->prefixes[i]), 0, 1) == 0)
      {
        // found a match - record post prefix position
        postPrefix1 = closure->prefixes[i]->Length();
        // bail out of the for loop
        break;
      }
    }

    // iterate through our prefixes looking for a match
    for (i=0; i<closure->prefixCount; i++)
    {
      // Check if string is prefixed.  Note: the parameters of the Find() 
      // method specify the url is searched at the 0th character and if there
      // is no match the rest of the url is not searched.
      if (url2.Find((*closure->prefixes[i]), 0, 1) == 0)
      {
        // found a match - record post prefix position
        postPrefix2 = closure->prefixes[i]->Length();
        // bail out of the for loop
        break;
      }
    }

    // compare non-prefixed urls
    PRInt32 ret = Compare(
      Substring(url1, postPrefix1, url1.Length()),
      Substring(url2, postPrefix2, url2.Length()));
    if (ret != 0) return ret;

    // sort http://xyz.com before http://www.xyz.com
    return postPrefix1 - postPrefix2;
  }
}

