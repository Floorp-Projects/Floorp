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

#ifndef nssimpleglobalhistory__h____
#define nssimpleglobalhistory__h____

#include "nsAString.h"
#include "nsITimer.h"

#include "nsWeakReference.h"
#include "mdb.h"
#include "nsIPrefBranch.h"
#include "nsIBrowserHistory.h"
#include "nsIHistoryItems.h"
#include "nsIHistoryObserver.h"

#include "nsIObserver.h"
#include "nsIAutoCompleteSession.h"

#include "nsString.h"
#include "nsVoidArray.h"
#include "nsSupportsArray.h"
#include "nsHashSets.h"

struct MatchHostData;
struct SearchQueryData;
struct AutocompleteExcludeData;

typedef PRBool (*rowMatchCallback)(nsIMdbRow *aRow, void *closure);

// 0049433E-5B6A-11D9-BE15-000393D7254A
#define NS_SIMPLEGLOBALHISTORY_CID \
    { 0x0049433E, 0x5B6A, 0x11D9, { 0xBE, 0x15, 0x00, 0x03, 0x93, 0xD7, 0x25, 0x4A } }


class nsSimpleGlobalHistory;

class nsHistoryItem : public nsIHistoryItem
{
public:

                  nsHistoryItem();
  virtual         ~nsHistoryItem();

  NS_METHOD       InitWithRow(nsSimpleGlobalHistory* inHistory, nsIMdbEnv* inEnv, nsIMdbRow* inRow);
  
  // nsISupports methods 
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHISTORYITEM

protected:

  nsSimpleGlobalHistory*    mHistory;   // not owned
  
  nsIMdbEnv*                mEnv;
  nsCOMPtr<nsIMdbRow>       mRow;

};


#pragma mark -

// this is an RDF-free version of nsGlobalHistory.
class nsSimpleGlobalHistory : nsSupportsWeakReference,
                        public nsIBrowserHistory,
                        public nsIHistoryItems,
                        public nsIObserver,
                        public nsIAutoCompleteSession
{
friend class HistoryAutoCompleteEnumerator;
friend class nsHistoryItem;

public:
                    nsSimpleGlobalHistory();
  virtual           ~nsSimpleGlobalHistory();

  NS_METHOD         Init();

  // nsISupports methods 
  NS_DECL_ISUPPORTS

  NS_DECL_NSIGLOBALHISTORY2
  NS_DECL_NSIBROWSERHISTORY
  NS_DECL_NSIHISTORYITEMS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIAUTOCOMPLETESESSION

public:

  // these must be public so that the callbacks can call them
  PRBool MatchExpiration(nsIMdbRow *row, PRTime* expirationDate);
  PRBool MatchHost(nsIMdbRow *row, MatchHostData *hostInfo);
  PRBool RowMatches(nsIMdbRow* aRow, SearchQueryData *aQuery);

  PRTime  GetNow();

protected:

  //
  // database stuff
  //
  enum eCommitType 
  {
    kLargeCommit = 0,
    kSessionCommit = 1,
    kCompressCommit = 2
  };
  
  nsresult OpenDB();
  nsresult OpenExistingFile(nsIMdbFactory *factory, const char *filePath);
  nsresult OpenNewFile(nsIMdbFactory *factory, const char *filePath);
  nsresult CreateTokens();
  nsresult CloseDB();
  nsresult CheckHostnameEntries();
  nsresult Commit(eCommitType commitType);

  //
  // expiration/removal stuff
  //
  nsresult ExpireEntries(PRBool notify);
  nsresult RemoveMatchingRows(rowMatchCallback aMatchFunc, void *aClosure, PRBool notify);

  // 
  // autocomplete stuff
  //
  nsresult    AutoCompleteSearch(const nsAString& aSearchString,
                              AutocompleteExcludeData* aExclude,
                              nsIAutoCompleteResults* aPrevResults,
                              nsIAutoCompleteResults* aResults);
  void        AutoCompleteCutPrefix(nsAString& aURL, AutocompleteExcludeData* aExclude);
  void        AutoCompleteGetExcludeInfo(const nsAString& aURL, AutocompleteExcludeData* aExclude);
  nsString    AutoCompletePrefilter(const nsAString& aSearchString);
  PRBool      AutoCompleteCompare(nsAString& aHistoryURL, 
                             const nsAString& aUserURL,
                             AutocompleteExcludeData* aExclude);
  PR_STATIC_CALLBACK(int)   AutoCompleteSortComparison(const void *v1, const void *v2, void *unused);

  //
  // sync stuff to write the db to disk every so often
  //
  void          Sync();
  nsresult      SetDirty();
  
  static void   FireSyncTimer(nsITimer *aTimer, void *aClosure);

  //
  // AddPage-oriented stuff
  //
  nsresult AddExistingPageToDatabase(nsIMdbRow *row,
                                     PRTime aDate,
                                     const char *aReferrer,
                                     PRTime *aOldDate,
                                     PRInt32 *aOldCount);
  nsresult AddNewPageToDatabase(const char *aURL,
                                PRTime aDate,
                                const char *aReferrer,
                                nsIMdbRow **aResult);

  nsresult RemovePageInternal(const char *aSpec);

  //
  // Row to history item
  //

  nsresult HistoryItemFromRow(nsIMdbRow* inRow, nsIHistoryItem** outItem);

  nsresult StartBatching();
  nsresult EndBatching();
  
  // 
  // observer utilities
  // 
  
  nsresult NotifyObserversHistoryLoaded();
  nsresult NotifyObserversItemLoaded(nsIMdbRow* inRow, PRBool inFirstVisit);
  nsresult NotifyObserversItemRemoved(nsIMdbRow* inRow);
  nsresult NotifyObserversItemTitleChanged(nsIMdbRow* inRow);
  nsresult NotifyObserversBatchingStarted();
  nsresult NotifyObserversBatchingFinished();
  
  //
  // generic routines for setting/retrieving various datatypes
  //
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRTime& aValue);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRInt32 aValue);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const char *aValue);
  nsresult SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRUnichar *aValue);

  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsAString& aResult);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, nsACString& aResult);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, PRTime* aResult);
  nsresult GetRowValue(nsIMdbRow *aRow, mdb_column aCol, PRInt32* aResult);

  nsresult RowHasCell(nsIMdbRow *aRow, mdb_column aCol, PRBool* aResult);

  nsresult FindRow(mdb_column aCol, const char *aURL, nsIMdbRow **aResult);

  //
  // convenient getter for tokens
  //
  enum EColumn
  {
    eColumnURL,
    eColumnReferrer,
    eColumnLastVisitDate,
    eColumnFirstVisitDate,
    eColumnVisitCount,
    eColumnTitle,     // aka name
    eColumnHostname,
    eColumnHidden,
    eColumnTyped
  };
  
  mdb_column TokenForColumn(EColumn inColumn);
  
  //
  // byte order
  //
  nsresult  SaveByteOrder(const char *aByteOrder);
  nsresult  GetByteOrder(char **_retval);
  nsresult  InitByteOrder(PRBool aForce);
  void      SwapBytes(const PRUnichar *source, PRUnichar *dest, PRInt32 aLen);

protected:

  static PRInt32      gRefCnt;

  static nsIMdbFactory* gMdbFactory;
  static nsIPrefBranch* gPrefBranch;

  PRInt64             mFileSizeOnDisk;
  PRInt32             mExpireDays;

  PRInt32             mBatchesInProgress;
  PRBool              mDirty;             // if we've changed history
  PRBool              mPagesRemoved;      // true if we've removed pages but not committed.
  nsCOMPtr<nsITimer>  mSyncTimer;

  // observers
  nsSupportsArray     mHistoryObservers;
  
  // autocomplete stuff
  PRBool              mAutocompleteOnlyTyped;
  nsStringArray       mIgnoreSchemes;
  nsStringArray       mIgnoreHostnames;

  // N.B., these are MDB interfaces, _not_ XPCOM interfaces.
  // XXX does that mean we can't use nsCOMPtrs on them?
  nsIMdbEnv*          mEnv;       // OWNER
  nsIMdbStore*        mStore;     // OWNER
  nsIMdbTable*        mTable;     // OWNER

  PRBool              mReverseByteOrder;
  
  nsCOMPtr<nsIMdbRow> mMetaRow;
  
  mdb_scope  kToken_HistoryRowScope;
  mdb_kind   kToken_HistoryKind;

  mdb_column kToken_URLColumn;
  mdb_column kToken_ReferrerColumn;
  mdb_column kToken_LastVisitDateColumn;
  mdb_column kToken_FirstVisitDateColumn;
  mdb_column kToken_VisitCountColumn;
  mdb_column kToken_NameColumn;
  mdb_column kToken_HostnameColumn;
  mdb_column kToken_HiddenColumn;
  mdb_column kToken_TypedColumn;

  // meta-data tokens
  mdb_column kToken_LastPageVisited;
  mdb_column kToken_ByteOrder;

  nsCStringHashSet mTypedHiddenURIs;
};


#endif // nssimpleglobalhistory__h____
