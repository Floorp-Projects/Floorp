/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Radha Kulkarni <radha@netscape.com>
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

// Local Includes 
#include "nsSHistory.h"

// Helper Classes
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

// Interfaces Needed
#include "nsILayoutHistoryState.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsISHContainer.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIURI.h"
#include "nsIContentViewer.h"
#include "nsICacheService.h"
#include "nsIObserverService.h"
#include "prclist.h"
#include "mozilla/Services.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsDocShell.h"

// For calculating max history entries and max cachable contentviewers
#include "nspr.h"
#include <math.h>  // for log()

#define PREF_SHISTORY_SIZE "browser.sessionhistory.max_entries"
#define PREF_SHISTORY_MAX_TOTAL_VIEWERS "browser.sessionhistory.max_total_viewers"

static PRInt32  gHistoryMaxSize = 50;
// Max viewers allowed per SHistory objects
static const PRInt32  gHistoryMaxViewers = 3;
// List of all SHistory objects, used for content viewer cache eviction
static PRCList gSHistoryList;
// Max viewers allowed total, across all SHistory objects - negative default
// means we will calculate how many viewers to cache based on total memory
PRInt32 nsSHistory::sHistoryMaxTotalViewers = -1;

enum HistCmd{
  HIST_CMD_BACK,
  HIST_CMD_FORWARD,
  HIST_CMD_GOTOINDEX,
  HIST_CMD_RELOAD
} ;

//*****************************************************************************
//***      nsSHistoryObserver
//*****************************************************************************

class nsSHistoryObserver : public nsIObserver
{

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsSHistoryObserver() {}

protected:
  ~nsSHistoryObserver() {}
};

NS_IMPL_ISUPPORTS1(nsSHistoryObserver, nsIObserver)

NS_IMETHODIMP
nsSHistoryObserver::Observe(nsISupports *aSubject, const char *aTopic,
                            const PRUnichar *aData)
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefs = do_QueryInterface(aSubject);
    if (prefs) {
      nsSHistory::UpdatePrefs(prefs);
      nsSHistory::EvictGlobalContentViewer();
    }
  } else if (!strcmp(aTopic, NS_CACHESERVICE_EMPTYCACHE_TOPIC_ID) ||
             !strcmp(aTopic, "memory-pressure")) {
    nsSHistory::EvictAllContentViewersGlobally();
  }

  return NS_OK;
}

//*****************************************************************************
//***    nsSHistory: Object Management
//*****************************************************************************

nsSHistory::nsSHistory() : mListRoot(nsnull), mIndex(-1), mLength(0), mRequestedIndex(-1)
{
  // Add this new SHistory object to the list
  PR_APPEND_LINK(this, &gSHistoryList);
}


nsSHistory::~nsSHistory()
{
  // Remove this SHistory object from the list
  PR_REMOVE_LINK(this);
}

//*****************************************************************************
//    nsSHistory: nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsSHistory)
NS_IMPL_RELEASE(nsSHistory)

NS_INTERFACE_MAP_BEGIN(nsSHistory)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISHistory)
   NS_INTERFACE_MAP_ENTRY(nsISHistory)
   NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
   NS_INTERFACE_MAP_ENTRY(nsISHistoryInternal)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    nsSHistory: nsISHistory
//*****************************************************************************

// static
PRUint32
nsSHistory::CalcMaxTotalViewers()
{
  // Calculate an estimate of how many ContentViewers we should cache based
  // on RAM.  This assumes that the average ContentViewer is 4MB (conservative)
  // and caps the max at 8 ContentViewers
  //
  // TODO: Should we split the cache memory betw. ContentViewer caching and
  // nsCacheService?
  //
  // RAM      ContentViewers
  // -----------------------
  // 32   Mb       0
  // 64   Mb       1
  // 128  Mb       2
  // 256  Mb       3
  // 512  Mb       5
  // 1024 Mb       8
  // 2048 Mb       8
  // 4096 Mb       8
  PRUint64 bytes = PR_GetPhysicalMemorySize();

  if (LL_IS_ZERO(bytes))
    return 0;

  // Conversion from unsigned int64 to double doesn't work on all platforms.
  // We need to truncate the value at LL_MAXINT to make sure we don't
  // overflow.
  if (LL_CMP(bytes, >, LL_MAXINT))
    bytes = LL_MAXINT;

  PRUint64 kbytes;
  LL_SHR(kbytes, bytes, 10);

  double kBytesD;
  LL_L2D(kBytesD, (PRInt64) kbytes);

  // This is essentially the same calculation as for nsCacheService,
  // except that we divide the final memory calculation by 4, since
  // we assume each ContentViewer takes on average 4MB
  PRUint32 viewers = 0;
  double x = log(kBytesD)/log(2.0) - 14;
  if (x > 0) {
    viewers    = (PRUint32)(x * x - x + 2.001); // add .001 for rounding
    viewers   /= 4;
  }

  // Cap it off at 8 max
  if (viewers > 8) {
    viewers = 8;
  }
  return viewers;
}

// static
void
nsSHistory::UpdatePrefs(nsIPrefBranch *aPrefBranch)
{
  aPrefBranch->GetIntPref(PREF_SHISTORY_SIZE, &gHistoryMaxSize);
  aPrefBranch->GetIntPref(PREF_SHISTORY_MAX_TOTAL_VIEWERS,
                          &sHistoryMaxTotalViewers);
  // If the pref is negative, that means we calculate how many viewers
  // we think we should cache, based on total memory
  if (sHistoryMaxTotalViewers < 0) {
    sHistoryMaxTotalViewers = CalcMaxTotalViewers();
  }
}

// static
nsresult
nsSHistory::Startup()
{
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    nsCOMPtr<nsIPrefBranch> sesHBranch;
    prefs->GetBranch(nsnull, getter_AddRefs(sesHBranch));
    if (sesHBranch) {
      UpdatePrefs(sesHBranch);
    }

    // The goal of this is to unbreak users who have inadvertently set their
    // session history size to less than the default value.
    PRInt32  defaultHistoryMaxSize = 50;
    nsCOMPtr<nsIPrefBranch> defaultBranch;
    prefs->GetDefaultBranch(nsnull, getter_AddRefs(defaultBranch));
    if (defaultBranch) {
      defaultBranch->GetIntPref(PREF_SHISTORY_SIZE, &defaultHistoryMaxSize);
    }

    if (gHistoryMaxSize < defaultHistoryMaxSize) {
      gHistoryMaxSize = defaultHistoryMaxSize;
    }
    
    // Allow the user to override the max total number of cached viewers,
    // but keep the per SHistory cached viewer limit constant
    nsCOMPtr<nsIPrefBranch2> branch = do_QueryInterface(sesHBranch);
    if (branch) {
      nsSHistoryObserver* obs = new nsSHistoryObserver();
      if (!obs) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      branch->AddObserver(PREF_SHISTORY_SIZE, obs, PR_FALSE);
      branch->AddObserver(PREF_SHISTORY_MAX_TOTAL_VIEWERS,
                          obs, PR_FALSE);

      nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
      if (obsSvc) {
        // Observe empty-cache notifications so tahat clearing the disk/memory
        // cache will also evict all content viewers.
        obsSvc->AddObserver(obs,
                            NS_CACHESERVICE_EMPTYCACHE_TOPIC_ID, PR_FALSE);

        // Same for memory-pressure notifications
        obsSvc->AddObserver(obs, "memory-pressure", PR_FALSE);
      }
    }
  }

  // Initialize the global list of all SHistory objects
  PR_INIT_CLIST(&gSHistoryList);
  return NS_OK;
}

/* Add an entry to the History list at mIndex and 
 * increment the index to point to the new entry
 */
NS_IMETHODIMP
nsSHistory::AddEntry(nsISHEntry * aSHEntry, PRBool aPersist)
{
  NS_ENSURE_ARG(aSHEntry);

  nsCOMPtr<nsISHTransaction> currentTxn;

  if(mListRoot)
    GetTransactionAtIndex(mIndex, getter_AddRefs(currentTxn));

  PRBool currentPersist = PR_TRUE;
  if(currentTxn)
    currentTxn->GetPersist(&currentPersist);

  if(!currentPersist)
  {
    NS_ENSURE_SUCCESS(currentTxn->SetSHEntry(aSHEntry),NS_ERROR_FAILURE);
    currentTxn->SetPersist(aPersist);
    return NS_OK;
  }

  nsCOMPtr<nsISHTransaction> txn(do_CreateInstance(NS_SHTRANSACTION_CONTRACTID));
  NS_ENSURE_TRUE(txn, NS_ERROR_FAILURE);

  // Notify any listener about the new addition
  if (mListener) {
    nsCOMPtr<nsISHistoryListener> listener(do_QueryReferent(mListener));
    if (listener) {
      nsCOMPtr<nsIURI> uri;
      nsCOMPtr<nsIHistoryEntry> hEntry(do_QueryInterface(aSHEntry));
      if (hEntry) {
        PRInt32 currentIndex = mIndex;
        hEntry->GetURI(getter_AddRefs(uri));
        listener->OnHistoryNewEntry(uri);

        // If a listener has changed mIndex, we need to get currentTxn again,
        // otherwise we'll be left at an inconsistent state (see bug 320742)
        if (currentIndex != mIndex)
          GetTransactionAtIndex(mIndex, getter_AddRefs(currentTxn));
      }
    }
  }

  // Set the ShEntry and parent for the transaction. setting the 
  // parent will properly set the parent child relationship
  txn->SetPersist(aPersist);
  NS_ENSURE_SUCCESS(txn->Create(aSHEntry, currentTxn), NS_ERROR_FAILURE);
   
  // A little tricky math here...  Basically when adding an object regardless of
  // what the length was before, it should always be set back to the current and
  // lop off the forward.
  mLength = (++mIndex + 1);

  // If this is the very first transaction, initialize the list
  if(!mListRoot)
    mListRoot = txn;

  // Purge History list if it is too long
  if ((gHistoryMaxSize >= 0) && (mLength > gHistoryMaxSize))
    PurgeHistory(mLength-gHistoryMaxSize);
  
  RemoveDynEntries(mIndex - 1, mIndex);
  return NS_OK;
}

/* Get size of the history list */
NS_IMETHODIMP
nsSHistory::GetCount(PRInt32 * aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mLength;
  return NS_OK;
}

/* Get index of the history list */
NS_IMETHODIMP
nsSHistory::GetIndex(PRInt32 * aResult)
{
  NS_PRECONDITION(aResult, "null out param?");
  *aResult = mIndex;
  return NS_OK;
}

/* Get the requestedIndex */
NS_IMETHODIMP
nsSHistory::GetRequestedIndex(PRInt32 * aResult)
{
  NS_PRECONDITION(aResult, "null out param?");
  *aResult = mRequestedIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GetEntryAtIndex(PRInt32 aIndex, PRBool aModifyIndex, nsISHEntry** aResult)
{
  nsresult rv;
  nsCOMPtr<nsISHTransaction> txn;

  /* GetTransactionAtIndex ensures aResult is valid and validates aIndex */
  rv = GetTransactionAtIndex(aIndex, getter_AddRefs(txn));
  if (NS_SUCCEEDED(rv) && txn) {
    //Get the Entry from the transaction
    rv = txn->GetSHEntry(aResult);
    if (NS_SUCCEEDED(rv) && (*aResult)) {
      // Set mIndex to the requested index, if asked to do so..
      if (aModifyIndex) {
        mIndex = aIndex;
      }
    } //entry
  }  //Transaction
  return rv;
}


/* Get the entry at a given index */
NS_IMETHODIMP
nsSHistory::GetEntryAtIndex(PRInt32 aIndex, PRBool aModifyIndex, nsIHistoryEntry** aResult)
{
  nsresult rv;
  nsCOMPtr<nsISHEntry> shEntry;
  rv = GetEntryAtIndex(aIndex, aModifyIndex, getter_AddRefs(shEntry));
  if (NS_SUCCEEDED(rv) && shEntry) 
    rv = CallQueryInterface(shEntry, aResult);
 
  return rv;
}

/* Get the transaction at a given index */
NS_IMETHODIMP
nsSHistory::GetTransactionAtIndex(PRInt32 aIndex, nsISHTransaction ** aResult)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aResult);

  if ((mLength <= 0) || (aIndex < 0) || (aIndex >= mLength))
    return NS_ERROR_FAILURE;

  if (!mListRoot) 
    return NS_ERROR_FAILURE;

  if (aIndex == 0)
  {
    *aResult = mListRoot;
    NS_ADDREF(*aResult);
    return NS_OK;
  } 
  PRInt32   cnt=0;
  nsCOMPtr<nsISHTransaction>  tempPtr;

  rv = GetRootTransaction(getter_AddRefs(tempPtr));
  if (NS_FAILED(rv) || !tempPtr)
    return NS_ERROR_FAILURE;

  while(1) {
    nsCOMPtr<nsISHTransaction> ptr;
    rv = tempPtr->GetNext(getter_AddRefs(ptr));
    if (NS_SUCCEEDED(rv) && ptr) {
      cnt++;
      if (cnt == aIndex) {
        *aResult = ptr;
        NS_ADDREF(*aResult);
        break;
      }
      else {
        tempPtr = ptr;
        continue;
      }
    }  //NS_SUCCEEDED
    else 
      return NS_ERROR_FAILURE;
  }  // while 
  
  return NS_OK;
}

#ifdef DEBUG
nsresult
nsSHistory::PrintHistory()
{

  nsCOMPtr<nsISHTransaction>   txn;
  PRInt32 index = 0;
  nsresult rv;

  if (!mListRoot) 
    return NS_ERROR_FAILURE;

  txn = mListRoot;
    
  while (1) {
    if (!txn)
      break;
    nsCOMPtr<nsISHEntry>  entry;
    rv = txn->GetSHEntry(getter_AddRefs(entry));
    if (NS_FAILED(rv) && !entry)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsILayoutHistoryState> layoutHistoryState;
    nsCOMPtr<nsIURI>  uri;
    nsXPIDLString title;

    entry->GetLayoutHistoryState(getter_AddRefs(layoutHistoryState));
    nsCOMPtr<nsIHistoryEntry> hEntry(do_QueryInterface(entry));
    if (hEntry) {
      hEntry->GetURI(getter_AddRefs(uri));
      hEntry->GetTitle(getter_Copies(title));              
    }

#if 0
    nsCAutoString url;
    if (uri)
     uri->GetSpec(url);

    printf("**** SH Transaction #%d, Entry = %x\n", index, entry.get());
    printf("\t\t URL = %s\n", url.get());

    printf("\t\t Title = %s\n", NS_LossyConvertUTF16toASCII(title).get());
    printf("\t\t layout History Data = %x\n", layoutHistoryState.get());
#endif

    nsCOMPtr<nsISHTransaction> next;
    rv = txn->GetNext(getter_AddRefs(next));
    if (NS_SUCCEEDED(rv) && next) {
      txn = next;
      index++;
      continue;
    }
    else
      break;
  }

  return NS_OK;
}
#endif


NS_IMETHODIMP
nsSHistory::GetRootTransaction(nsISHTransaction ** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult=mListRoot;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

/* Get the max size of the history list */
NS_IMETHODIMP
nsSHistory::GetMaxLength(PRInt32 * aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = gHistoryMaxSize;
  return NS_OK;
}

/* Set the max size of the history list */
NS_IMETHODIMP
nsSHistory::SetMaxLength(PRInt32 aMaxSize)
{
  if (aMaxSize < 0)
    return NS_ERROR_ILLEGAL_VALUE;

  gHistoryMaxSize = aMaxSize;
  if (mLength > aMaxSize)
    PurgeHistory(mLength-aMaxSize);
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::PurgeHistory(PRInt32 aEntries)
{
  if (mLength <= 0 || aEntries <= 0)
    return NS_ERROR_FAILURE;

  aEntries = NS_MIN(aEntries, mLength);
  
  PRBool purgeHistory = PR_TRUE;
  // Notify the listener about the history purge
  if (mListener) {
    nsCOMPtr<nsISHistoryListener> listener(do_QueryReferent(mListener));
    if (listener) {
      listener->OnHistoryPurge(aEntries, &purgeHistory);
    } 
  }

  if (!purgeHistory) {
    // Listener asked us not to purge
    return NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA;
  }

  PRInt32 cnt = 0;
  while (cnt < aEntries) {
    nsCOMPtr<nsISHTransaction> nextTxn;
    if (mListRoot)
      mListRoot->GetNext(getter_AddRefs(nextTxn));
    mListRoot = nextTxn;
    cnt++;        
  }
  mLength -= cnt;
  mIndex -= cnt;

  // Now if we were not at the end of the history, mIndex could have
  // become far too negative.  If so, just set it to -1.
  if (mIndex < -1) {
    mIndex = -1;
  }

  if (mRootDocShell)
    mRootDocShell->HistoryPurged(cnt);

  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::AddSHistoryListener(nsISHistoryListener * aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);

  // Check if the listener supports Weak Reference. This is a must.
  // This listener functionality is used by embedders and we want to 
  // have the right ownership with who ever listens to SHistory
  nsWeakPtr listener = do_GetWeakReference(aListener);
  if (!listener) return NS_ERROR_FAILURE;
  mListener = listener;
  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::RemoveSHistoryListener(nsISHistoryListener * aListener)
{
  // Make sure the listener that wants to be removed is the
  // one we have in store. 
  nsWeakPtr listener = do_GetWeakReference(aListener);  
  if (listener == mListener) {
    mListener = nsnull;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}


/* Replace an entry in the History list at a particular index.
 * Do not update index or count.
 */
NS_IMETHODIMP
nsSHistory::ReplaceEntry(PRInt32 aIndex, nsISHEntry * aReplaceEntry)
{
  NS_ENSURE_ARG(aReplaceEntry);
  nsresult rv;
  nsCOMPtr<nsISHTransaction> currentTxn;

  if (!mListRoot) // Session History is not initialised.
    return NS_ERROR_FAILURE;

  rv = GetTransactionAtIndex(aIndex, getter_AddRefs(currentTxn));

  if(currentTxn)
  {
    // Set the replacement entry in the transaction
    rv = currentTxn->SetSHEntry(aReplaceEntry);
    rv = currentTxn->SetPersist(PR_TRUE);
  }
  return rv;
}

/* Get a handle to the Session history listener */
NS_IMETHODIMP
nsSHistory::GetListener(nsISHistoryListener ** aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  if (mListener) 
    CallQueryReferent(mListener.get(),  aListener);
  // Don't addref aListener. It is a weak pointer.
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::EvictContentViewers(PRInt32 aPreviousIndex, PRInt32 aIndex)
{
  // Check our per SHistory object limit in the currently navigated SHistory
  EvictWindowContentViewers(aPreviousIndex, aIndex);
  // Check our total limit across all SHistory objects
  EvictGlobalContentViewer();
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::EvictAllContentViewers()
{
  // XXXbz we don't actually do a good job of evicting things as we should, so
  // we might have viewers quite far from mIndex.  So just evict everything.
  EvictContentViewersInRange(0, mLength);
  return NS_OK;
}



//*****************************************************************************
//    nsSHistory: nsIWebNavigation
//*****************************************************************************

NS_IMETHODIMP
nsSHistory::GetCanGoBack(PRBool * aCanGoBack)
{
  NS_ENSURE_ARG_POINTER(aCanGoBack);
  *aCanGoBack = PR_FALSE;

  PRInt32 index = -1;
  NS_ENSURE_SUCCESS(GetIndex(&index), NS_ERROR_FAILURE);
  if(index > 0)
     *aCanGoBack = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GetCanGoForward(PRBool * aCanGoForward)
{
  NS_ENSURE_ARG_POINTER(aCanGoForward);
  *aCanGoForward = PR_FALSE;

  PRInt32 index = -1;
  PRInt32 count = -1;

  NS_ENSURE_SUCCESS(GetIndex(&index), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(GetCount(&count), NS_ERROR_FAILURE);

  if((index >= 0) && (index < (count - 1)))
    *aCanGoForward = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GoBack()
{
  PRBool canGoBack = PR_FALSE;

  GetCanGoBack(&canGoBack);
  if (!canGoBack)  // Can't go back
    return NS_ERROR_UNEXPECTED;
  return LoadEntry(mIndex-1, nsIDocShellLoadInfo::loadHistory, HIST_CMD_BACK);
}


NS_IMETHODIMP
nsSHistory::GoForward()
{
  PRBool canGoForward = PR_FALSE;

  GetCanGoForward(&canGoForward);
  if (!canGoForward)  // Can't go forward
    return NS_ERROR_UNEXPECTED;
  return LoadEntry(mIndex+1, nsIDocShellLoadInfo::loadHistory, HIST_CMD_FORWARD);
}

NS_IMETHODIMP
nsSHistory::Reload(PRUint32 aReloadFlags)
{
  nsresult rv;
  nsDocShellInfoLoadType loadType;
  if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY && 
      aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE)
  {
    loadType = nsIDocShellLoadInfo::loadReloadBypassProxyAndCache;
  }
  else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY)
  {
    loadType = nsIDocShellLoadInfo::loadReloadBypassProxy;
  }
  else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE)
  {
    loadType = nsIDocShellLoadInfo::loadReloadBypassCache;
  }
  else if (aReloadFlags & nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE)
  {
    loadType = nsIDocShellLoadInfo::loadReloadCharsetChange;
  }
  else
  {
    loadType = nsIDocShellLoadInfo::loadReloadNormal;
  }
  
  // Notify listeners
  PRBool canNavigate = PR_TRUE;
  if (mListener) {
    nsCOMPtr<nsISHistoryListener> listener(do_QueryReferent(mListener));
    // We are reloading. Send Reload notifications. 
    // nsDocShellLoadFlagType is not public, where as nsIWebNavigation
    // is public. So send the reload notifications with the
    // nsIWebNavigation flags. 
    if (listener) {
      nsCOMPtr<nsIURI> currentURI;
      rv = GetCurrentURI(getter_AddRefs(currentURI));
      listener->OnHistoryReload(currentURI, aReloadFlags, &canNavigate);
    }
  }
  if (!canNavigate)
    return NS_OK;

  return LoadEntry(mIndex, loadType, HIST_CMD_RELOAD);
}

void
nsSHistory::EvictWindowContentViewers(PRInt32 aFromIndex, PRInt32 aToIndex)
{
  // To enforce the per SHistory object limit on cached content viewers, we
  // need to release all of the content viewers that are no longer in the
  // "window" that now ends/begins at aToIndex.  Existing content viewers
  // should be in the window from
  // aFromIndex - gHistoryMaxViewers to aFromIndex + gHistoryMaxViewers
  //
  // We make the assumption that entries outside this range have no viewers so
  // that we don't have to walk the whole entire session history checking for
  // content viewers.

  // This can happen on the first load of a page in a particular window
  if (aFromIndex < 0 || aToIndex < 0) {
    return;
  }
  NS_ASSERTION(aFromIndex < mLength, "aFromIndex is out of range");
  NS_ASSERTION(aToIndex < mLength, "aToIndex is out of range");
  if (aFromIndex >= mLength || aToIndex >= mLength) {
    return;
  }

  // These indices give the range of SHEntries whose content viewers will be
  // evicted
  PRInt32 startIndex, endIndex;
  if (aToIndex > aFromIndex) { // going forward
    endIndex = aToIndex - gHistoryMaxViewers;
    if (endIndex <= 0) {
      return;
    }
    startIndex = NS_MAX(0, aFromIndex - gHistoryMaxViewers);
  } else { // going backward
    startIndex = aToIndex + gHistoryMaxViewers + 1;
    if (startIndex >= mLength) {
      return;
    }
    endIndex = NS_MIN(mLength, aFromIndex + gHistoryMaxViewers + 1);
  }

#ifdef DEBUG
  nsCOMPtr<nsISHTransaction> trans;
  GetTransactionAtIndex(0, getter_AddRefs(trans));

  // Walk the full session history and check that entries outside the window
  // around aFromIndex have no content viewers
  for (PRInt32 i = 0; i < mLength; ++i) {
    if (i < aFromIndex - gHistoryMaxViewers || 
        i > aFromIndex + gHistoryMaxViewers) {
      nsCOMPtr<nsISHEntry> entry;
      trans->GetSHEntry(getter_AddRefs(entry));
      nsCOMPtr<nsIContentViewer> viewer;
      nsCOMPtr<nsISHEntry> ownerEntry;
      entry->GetAnyContentViewer(getter_AddRefs(ownerEntry),
                                 getter_AddRefs(viewer));
      NS_WARN_IF_FALSE(!viewer,
                       "ContentViewer exists outside gHistoryMaxViewer range");
    }

    nsISHTransaction *temp = trans;
    temp->GetNext(getter_AddRefs(trans));
  }
#endif

  EvictContentViewersInRange(startIndex, endIndex);
}

void
nsSHistory::EvictContentViewersInRange(PRInt32 aStart, PRInt32 aEnd)
{
  nsCOMPtr<nsISHTransaction> trans;
  GetTransactionAtIndex(aStart, getter_AddRefs(trans));

  for (PRInt32 i = aStart; i < aEnd; ++i) {
    nsCOMPtr<nsISHEntry> entry;
    trans->GetSHEntry(getter_AddRefs(entry));
    nsCOMPtr<nsIContentViewer> viewer;
    nsCOMPtr<nsISHEntry> ownerEntry;
    entry->GetAnyContentViewer(getter_AddRefs(ownerEntry),
                               getter_AddRefs(viewer));
    if (viewer) {
      NS_ASSERTION(ownerEntry,
                   "ContentViewer exists but its SHEntry is null");
#ifdef DEBUG_PAGE_CACHE
      nsCOMPtr<nsIURI> uri;
      ownerEntry->GetURI(getter_AddRefs(uri));
      nsCAutoString spec;
      if (uri)
        uri->GetSpec(spec);

      printf("per SHistory limit: evicting content viewer: %s\n", spec.get());
#endif

      // Drop the presentation state before destroying the viewer, so that
      // document teardown is able to correctly persist the state.
      ownerEntry->SetContentViewer(nsnull);
      ownerEntry->SyncPresentationState();
      viewer->Destroy();
    }

    nsISHTransaction *temp = trans;
    temp->GetNext(getter_AddRefs(trans));
  }
}

// static
void
nsSHistory::EvictGlobalContentViewer()
{
  // true until the total number of content viewers is <= total max
  // The usual case is that we only need to evict one content viewer.
  // However, if somebody resets the pref value, we might occasionally
  // need to evict more than one.
  PRBool shouldTryEviction = PR_TRUE;
  while (shouldTryEviction) {
    // Walk through our list of SHistory objects, looking for content
    // viewers in the possible active window of all of the SHEntry objects.
    // Keep track of the SHEntry object that has a ContentViewer and is
    // farthest from the current focus in any SHistory object.  The
    // ContentViewer associated with that SHEntry will be evicted
    PRInt32 distanceFromFocus = 0;
    nsCOMPtr<nsISHEntry> evictFromSHE;
    nsCOMPtr<nsIContentViewer> evictViewer;
    PRInt32 totalContentViewers = 0;
    nsSHistory* shist = static_cast<nsSHistory*>
                                   (PR_LIST_HEAD(&gSHistoryList));
    while (shist != &gSHistoryList) {
      // Calculate the window of SHEntries that could possibly have a content
      // viewer.  There could be up to gHistoryMaxViewers content viewers,
      // but we don't know whether they are before or after the mIndex position
      // in the SHEntry list.  Just check both sides, to be safe.
      PRInt32 startIndex = NS_MAX(0, shist->mIndex - gHistoryMaxViewers);
      PRInt32 endIndex = NS_MIN(shist->mLength - 1,
                                shist->mIndex + gHistoryMaxViewers);
      nsCOMPtr<nsISHTransaction> trans;
      shist->GetTransactionAtIndex(startIndex, getter_AddRefs(trans));
      
      for (PRInt32 i = startIndex; i <= endIndex; ++i) {
        nsCOMPtr<nsISHEntry> entry;
        trans->GetSHEntry(getter_AddRefs(entry));
        nsCOMPtr<nsIContentViewer> viewer;
        nsCOMPtr<nsISHEntry> ownerEntry;
        entry->GetAnyContentViewer(getter_AddRefs(ownerEntry),
                                   getter_AddRefs(viewer));

#ifdef DEBUG_PAGE_CACHE
        nsCOMPtr<nsIURI> uri;
        if (ownerEntry) {
          ownerEntry->GetURI(getter_AddRefs(uri));
        } else {
          entry->GetURI(getter_AddRefs(uri));
        }
        nsCAutoString spec;
        if (uri) {
          uri->GetSpec(spec);
          printf("Considering for eviction: %s\n", spec.get());
        }
#endif
        
        // This SHEntry has a ContentViewer, so check how far away it is from
        // the currently used SHEntry within this SHistory object
        if (viewer) {
          PRInt32 distance = PR_ABS(shist->mIndex - i);
          
#ifdef DEBUG_PAGE_CACHE
          printf("Has a cached content viewer: %s\n", spec.get());
          printf("mIndex: %d i: %d\n", shist->mIndex, i);
#endif
          totalContentViewers++;
          if (distance > distanceFromFocus) {
            
#ifdef DEBUG_PAGE_CACHE
            printf("Choosing as new eviction candidate: %s\n", spec.get());
#endif

            distanceFromFocus = distance;
            evictFromSHE = ownerEntry;
            evictViewer = viewer;
          }
        }
        nsISHTransaction* temp = trans;
        temp->GetNext(getter_AddRefs(trans));
      }
      shist = static_cast<nsSHistory*>(PR_NEXT_LINK(shist));
    }

#ifdef DEBUG_PAGE_CACHE
    printf("Distance from focus: %d\n", distanceFromFocus);
    printf("Total max viewers: %d\n", sHistoryMaxTotalViewers);
    printf("Total number of viewers: %d\n", totalContentViewers);
#endif

    if (totalContentViewers > sHistoryMaxTotalViewers && evictViewer) {
#ifdef DEBUG_PAGE_CACHE
      nsCOMPtr<nsIURI> uri;
      evictFromSHE->GetURI(getter_AddRefs(uri));
      nsCAutoString spec;
      if (uri) {
        uri->GetSpec(spec);
        printf("Evicting content viewer: %s\n", spec.get());
      }
#endif

      // Drop the presentation state before destroying the viewer, so that
      // document teardown is able to correctly persist the state.
      evictFromSHE->SetContentViewer(nsnull);
      evictFromSHE->SyncPresentationState();
      evictViewer->Destroy();

      // If we only needed to evict one content viewer, then we are done.
      // Otherwise, continue evicting until we reach the max total limit.
      if (totalContentViewers - sHistoryMaxTotalViewers == 1) {
        shouldTryEviction = PR_FALSE;
      }
    } else {
      // couldn't find a content viewer to evict, so we are done
      shouldTryEviction = PR_FALSE;
    }
  }  // while shouldTryEviction
}

NS_IMETHODIMP
nsSHistory::EvictExpiredContentViewerForEntry(nsISHEntry *aEntry)
{
  PRInt32 startIndex = NS_MAX(0, mIndex - gHistoryMaxViewers);
  PRInt32 endIndex = NS_MIN(mLength - 1,
                            mIndex + gHistoryMaxViewers);
  nsCOMPtr<nsISHTransaction> trans;
  GetTransactionAtIndex(startIndex, getter_AddRefs(trans));

  PRInt32 i;
  for (i = startIndex; i <= endIndex; ++i) {
    nsCOMPtr<nsISHEntry> entry;
    trans->GetSHEntry(getter_AddRefs(entry));
    if (entry == aEntry)
      break;

    nsISHTransaction *temp = trans;
    temp->GetNext(getter_AddRefs(trans));
  }
  if (i > endIndex)
    return NS_OK;
  
  NS_ASSERTION(i != mIndex, "How did the current session entry expire?");
  if (i == mIndex)
    return NS_OK;
  
  // We evict content viewers for the expired entry and any other entries that
  // we would have to go through the expired entry to get to (i.e. the entries
  // that have the expired entry between them and the current entry). Those
  // other entries should have timed out already, actually, but this is just
  // to be on the safe side.
  if (i < mIndex) {
    EvictContentViewersInRange(startIndex, i + 1);
  } else {
    EvictContentViewersInRange(i, endIndex + 1);
  }
  
  return NS_OK;
}

// Evicts all content viewers in all history objects.  This is very
// inefficient, because it requires a linear search through all SHistory
// objects for each viewer to be evicted.  However, this method is called
// infrequently -- only when the disk or memory cache is cleared.

//static
void
nsSHistory::EvictAllContentViewersGlobally()
{
  PRInt32 maxViewers = sHistoryMaxTotalViewers;
  sHistoryMaxTotalViewers = 0;
  EvictGlobalContentViewer();
  sHistoryMaxTotalViewers = maxViewers;
}

void GetDynamicChildren(nsISHContainer* aContainer,
                        nsTArray<PRUint64>& aDocshellIDs,
                        PRBool aOnlyTopLevelDynamic)
{
  PRInt32 count = 0;
  aContainer->GetChildCount(&count);
  for (PRInt32 i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> child;
    aContainer->GetChildAt(i, getter_AddRefs(child));
    if (child) {
      PRBool dynAdded = PR_FALSE;
      child->IsDynamicallyAdded(&dynAdded);
      if (dynAdded) {
        PRUint64 docshellID = 0;
        child->GetDocshellID(&docshellID);
        aDocshellIDs.AppendElement(docshellID);
      }
      if (!dynAdded || !aOnlyTopLevelDynamic) {
        nsCOMPtr<nsISHContainer> childAsContainer = do_QueryInterface(child);
        if (childAsContainer) {
          GetDynamicChildren(childAsContainer, aDocshellIDs,
                             aOnlyTopLevelDynamic);
        }
      }
    }
  }
}

PRBool
RemoveFromSessionHistoryContainer(nsISHContainer* aContainer,
                                  nsTArray<PRUint64>& aDocshellIDs)
{
  nsCOMPtr<nsISHEntry> root = do_QueryInterface(aContainer);
  NS_ENSURE_TRUE(root, PR_FALSE);

  PRBool didRemove = PR_FALSE;
  PRInt32 childCount = 0;
  aContainer->GetChildCount(&childCount);
  for (PRInt32 i = childCount - 1; i >= 0; --i) {
    nsCOMPtr<nsISHEntry> child;
    aContainer->GetChildAt(i, getter_AddRefs(child));
    if (child) {
      PRUint64 docshelldID = 0;
      child->GetDocshellID(&docshelldID);
      if (aDocshellIDs.Contains(docshelldID)) {
        didRemove = PR_TRUE;
        aContainer->RemoveChild(child);
      } else {
        nsCOMPtr<nsISHContainer> container = do_QueryInterface(child);
        if (container) {
          PRBool childRemoved =
            RemoveFromSessionHistoryContainer(container, aDocshellIDs);
          if (childRemoved) {
            didRemove = PR_TRUE;
          }
        }
      }
    }
  }
  return didRemove;
}

PRBool RemoveChildEntries(nsISHistory* aHistory, PRInt32 aIndex,
                          nsTArray<PRUint64>& aEntryIDs)
{
  nsCOMPtr<nsIHistoryEntry> rootHE;
  aHistory->GetEntryAtIndex(aIndex, PR_FALSE, getter_AddRefs(rootHE));
  nsCOMPtr<nsISHContainer> root = do_QueryInterface(rootHE);
  return root ? RemoveFromSessionHistoryContainer(root, aEntryIDs) : PR_FALSE;
}

PRBool IsSameTree(nsISHEntry* aEntry1, nsISHEntry* aEntry2)
{
  if (!aEntry1 && !aEntry2) {
    return PR_TRUE;
  }
  if ((!aEntry1 && aEntry2) || (aEntry1 && !aEntry2)) {
    return PR_FALSE;
  }
  PRUint32 id1, id2;
  aEntry1->GetID(&id1);
  aEntry2->GetID(&id2);
  if (id1 != id2) {
    return PR_FALSE;
  }

  nsCOMPtr<nsISHContainer> container1 = do_QueryInterface(aEntry1);
  nsCOMPtr<nsISHContainer> container2 = do_QueryInterface(aEntry2);
  PRInt32 count1, count2;
  container1->GetChildCount(&count1);
  container2->GetChildCount(&count2);
  // We allow null entries in the end of the child list.
  PRInt32 count = PR_MAX(count1, count2);
  for (PRInt32 i = 0; i < count; ++i) {
    nsCOMPtr<nsISHEntry> child1, child2;
    container1->GetChildAt(i, getter_AddRefs(child1));
    container2->GetChildAt(i, getter_AddRefs(child2));
    if (!IsSameTree(child1, child2)) {
      return PR_FALSE;
    }
  }
  
  return PR_TRUE;
}

PRBool
nsSHistory::RemoveDuplicate(PRInt32 aIndex, PRBool aKeepNext)
{
  NS_ASSERTION(aIndex >= 0, "aIndex must be >= 0!");
  NS_ASSERTION(aIndex != mIndex, "Shouldn't remove mIndex!");
  PRInt32 compareIndex = aKeepNext ? aIndex + 1 : aIndex - 1;
  nsCOMPtr<nsIHistoryEntry> rootHE1, rootHE2;
  GetEntryAtIndex(aIndex, PR_FALSE, getter_AddRefs(rootHE1));
  GetEntryAtIndex(compareIndex, PR_FALSE, getter_AddRefs(rootHE2));
  nsCOMPtr<nsISHEntry> root1 = do_QueryInterface(rootHE1);
  nsCOMPtr<nsISHEntry> root2 = do_QueryInterface(rootHE2);
  if (IsSameTree(root1, root2)) {
    nsCOMPtr<nsISHTransaction> txToRemove, txToKeep, txNext, txPrev;
    GetTransactionAtIndex(aIndex, getter_AddRefs(txToRemove));
    GetTransactionAtIndex(compareIndex, getter_AddRefs(txToKeep));
    NS_ENSURE_TRUE(txToRemove, PR_FALSE);
    NS_ENSURE_TRUE(txToKeep, PR_FALSE);
    txToRemove->GetNext(getter_AddRefs(txNext));
    txToRemove->GetPrev(getter_AddRefs(txPrev));
    txToRemove->SetNext(nsnull);
    txToRemove->SetPrev(nsnull);
    if (aKeepNext) {
      if (txPrev) {
        txPrev->SetNext(txToKeep);
      } else {
        txToKeep->SetPrev(nsnull);
      }
    } else {
      txToKeep->SetNext(txNext);
    }

    if (aIndex == 0 && aKeepNext) {
      NS_ASSERTION(txToRemove == mListRoot,
                   "Transaction at index 0 should be mListRoot!");
      // We're removing the very first session history transaction!
      mListRoot = txToKeep;
    }
    if (mRootDocShell) {
      static_cast<nsDocShell*>(mRootDocShell)->HistoryTransactionRemoved(aIndex);
    }
    if (mIndex > aIndex) {
      mIndex = mIndex - 1;
    }
    --mLength;
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP_(void)
nsSHistory::RemoveEntries(nsTArray<PRUint64>& aIDs, PRInt32 aStartIndex)
{
  PRInt32 index = aStartIndex;
  while(index >= 0 && RemoveChildEntries(this, --index, aIDs));
  PRInt32 minIndex = index;
  index = aStartIndex;
  while(index >= 0 && RemoveChildEntries(this, index++, aIDs));
  
  // We need to remove duplicate nsSHEntry trees.
  PRBool didRemove = PR_FALSE;
  while (index > minIndex) {
    if (index != mIndex) {
      didRemove = RemoveDuplicate(index, index < mIndex) || didRemove;
    }
    --index;
  }
  if (didRemove && mRootDocShell) {
    nsRefPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(static_cast<nsDocShell*>(mRootDocShell),
                           &nsDocShell::FireDummyOnLocationChange);
    NS_DispatchToCurrentThread(ev);
  }
}

void
nsSHistory::RemoveDynEntries(PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  // Search for the entries which are in the current index,
  // but not in the new one.
  nsCOMPtr<nsISHEntry> originalSH;
  GetEntryAtIndex(aOldIndex, PR_FALSE, getter_AddRefs(originalSH));
  nsCOMPtr<nsISHContainer> originalContainer = do_QueryInterface(originalSH);
  nsAutoTArray<PRUint64, 16> toBeRemovedEntries;
  if (originalContainer) {
    nsTArray<PRUint64> originalDynDocShellIDs;
    GetDynamicChildren(originalContainer, originalDynDocShellIDs, PR_TRUE);
    if (originalDynDocShellIDs.Length()) {
      nsCOMPtr<nsISHEntry> currentSH;
      GetEntryAtIndex(aNewIndex, PR_FALSE, getter_AddRefs(currentSH));
      nsCOMPtr<nsISHContainer> newContainer = do_QueryInterface(currentSH);
      if (newContainer) {
        nsTArray<PRUint64> newDynDocShellIDs;
        GetDynamicChildren(newContainer, newDynDocShellIDs, PR_FALSE);
        for (PRUint32 i = 0; i < originalDynDocShellIDs.Length(); ++i) {
          if (!newDynDocShellIDs.Contains(originalDynDocShellIDs[i])) {
            toBeRemovedEntries.AppendElement(originalDynDocShellIDs[i]);
          }
        }
      }
    }
  }
  if (toBeRemovedEntries.Length()) {
    RemoveEntries(toBeRemovedEntries, aOldIndex);
  }
}

NS_IMETHODIMP
nsSHistory::UpdateIndex()
{
  // Update the actual index with the right value. 
  if (mIndex != mRequestedIndex && mRequestedIndex != -1) {
    RemoveDynEntries(mIndex, mRequestedIndex);
    mIndex = mRequestedIndex;
  }

  mRequestedIndex = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::Stop(PRUint32 aStopFlags)
{
  //Not implemented
  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::GetDocument(nsIDOMDocument** aDocument)
{
  // Not implemented
  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::GetCurrentURI(nsIURI** aResultURI)
{
  NS_ENSURE_ARG_POINTER(aResultURI);
  nsresult rv;

  nsCOMPtr<nsIHistoryEntry> currentEntry;
  rv = GetEntryAtIndex(mIndex, PR_FALSE, getter_AddRefs(currentEntry));
  if (NS_FAILED(rv) && !currentEntry) return rv;
  rv = currentEntry->GetURI(aResultURI);
  return rv;
}


NS_IMETHODIMP
nsSHistory::GetReferringURI(nsIURI** aURI)
{
  *aURI = nsnull;
  // Not implemented
  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::SetSessionHistory(nsISHistory* aSessionHistory)
{
  // Not implemented
  return NS_OK;
}

	
NS_IMETHODIMP
nsSHistory::GetSessionHistory(nsISHistory** aSessionHistory)
{
  // Not implemented
  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::LoadURI(const PRUnichar* aURI,
                    PRUint32 aLoadFlags,
                    nsIURI* aReferringURI,
                    nsIInputStream* aPostStream,
                    nsIInputStream* aExtraHeaderStream)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GotoIndex(PRInt32 aIndex)
{
  return LoadEntry(aIndex, nsIDocShellLoadInfo::loadHistory, HIST_CMD_GOTOINDEX);
}

nsresult
nsSHistory::LoadNextPossibleEntry(PRInt32 aNewIndex, long aLoadType, PRUint32 aHistCmd)
{
  mRequestedIndex = -1;
  if (aNewIndex < mIndex) {
    return LoadEntry(aNewIndex - 1, aLoadType, aHistCmd);
  }
  if (aNewIndex > mIndex) {
    return LoadEntry(aNewIndex + 1, aLoadType, aHistCmd);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSHistory::LoadEntry(PRInt32 aIndex, long aLoadType, PRUint32 aHistCmd)
{
  nsCOMPtr<nsIDocShell> docShell;
  nsCOMPtr<nsISHEntry> shEntry;
  // Keep note of requested history index in mRequestedIndex.
  mRequestedIndex = aIndex;

  nsCOMPtr<nsISHEntry> prevEntry;
  GetEntryAtIndex(mIndex, PR_FALSE, getter_AddRefs(prevEntry));

  nsCOMPtr<nsISHEntry> nextEntry;
  GetEntryAtIndex(mRequestedIndex, PR_FALSE, getter_AddRefs(nextEntry));
  nsCOMPtr<nsIHistoryEntry> nHEntry(do_QueryInterface(nextEntry));
  if (!nextEntry || !prevEntry || !nHEntry) {
    mRequestedIndex = -1;
    return NS_ERROR_FAILURE;
  }

  // Send appropriate listener notifications
  PRBool canNavigate = PR_TRUE;
  // Get the uri for the entry we are about to visit
  nsCOMPtr<nsIURI> nextURI;
  nHEntry->GetURI(getter_AddRefs(nextURI));

  if(mListener) {
    nsCOMPtr<nsISHistoryListener> listener(do_QueryReferent(mListener));
    if (listener) {
      if (aHistCmd == HIST_CMD_BACK) {
        // We are going back one entry. Send GoBack notifications
        listener->OnHistoryGoBack(nextURI, &canNavigate);
      }
      else if (aHistCmd == HIST_CMD_FORWARD) {
        // We are going forward. Send GoForward notification
        listener->OnHistoryGoForward(nextURI, &canNavigate);
      }
      else if (aHistCmd == HIST_CMD_GOTOINDEX) {
        // We are going somewhere else. This is not reload either
        listener->OnHistoryGotoIndex(aIndex, nextURI, &canNavigate);
      }
    }
  }

  if (!canNavigate) {
    // If the listener asked us not to proceed with 
    // the operation, simply return.    
    return NS_OK;  // XXX Maybe I can return some other error code?
  }

  nsCOMPtr<nsIURI> nexturi;
  PRInt32 pCount=0, nCount=0;
  nsCOMPtr<nsISHContainer> prevAsContainer(do_QueryInterface(prevEntry));
  nsCOMPtr<nsISHContainer> nextAsContainer(do_QueryInterface(nextEntry));
  if (prevAsContainer && nextAsContainer) {
    prevAsContainer->GetChildCount(&pCount);
    nextAsContainer->GetChildCount(&nCount);
  }
  
  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  if (mRequestedIndex == mIndex) {
    // Possibly a reload case 
    docShell = mRootDocShell;
  }
  else {
    // Going back or forward.
    if ((pCount > 0) && (nCount > 0)) {
      /* THis is a subframe navigation. Go find 
       * the docshell in which load should happen
       */
      PRBool frameFound = PR_FALSE;
      nsresult rv = CompareFrames(prevEntry, nextEntry, mRootDocShell, aLoadType, &frameFound);
      if (!frameFound) {
        // We did not successfully find the subframe in which
        // the new url was to be loaded. Go further in the history.
        return LoadNextPossibleEntry(aIndex, aLoadType, aHistCmd);
      }
      return rv;
    }   // (pCount >0)
    else {
      // Loading top level page.
      PRUint32 prevID = 0;
      PRUint32 nextID = 0;
      prevEntry->GetID(&prevID);
      nextEntry->GetID(&nextID);
      if (prevID == nextID) {
        // Try harder to find something new to load.
        // This may happen for example if some page removed iframes dynamically.
        return LoadNextPossibleEntry(aIndex, aLoadType, aHistCmd);
      }
      docShell = mRootDocShell;
    }
  }

  if (!docShell) {
    // we did not successfully go to the proper index.
    // return error.
    mRequestedIndex = -1;
    return NS_ERROR_FAILURE;
  }

  // Start the load on the appropriate docshell
  return InitiateLoad(nextEntry, docShell, aLoadType);
}

nsresult
nsSHistory::CompareFrames(nsISHEntry * aPrevEntry, nsISHEntry * aNextEntry, nsIDocShell * aParent, long aLoadType, PRBool * aIsFrameFound)
{
  if (!aPrevEntry || !aNextEntry || !aParent)
    return NS_ERROR_FAILURE;

  // We should be comparing only entries which were created for the
  // same docshell. This is here to just prevent anything strange happening.
  // This check could be possibly an assertion.
  PRUint64 prevdID, nextdID;
  aPrevEntry->GetDocshellID(&prevdID);
  aNextEntry->GetDocshellID(&nextdID);
  NS_ENSURE_STATE(prevdID == nextdID);

  nsresult result = NS_OK;
  PRUint32 prevID, nextID;

  aPrevEntry->GetID(&prevID);
  aNextEntry->GetID(&nextID);
 
  // Check the IDs to verify if the pages are different.
  if (prevID != nextID) {
    if (aIsFrameFound)
      *aIsFrameFound = PR_TRUE;
    // Set the Subframe flag of the entry to indicate that
    // it is subframe navigation        
    aNextEntry->SetIsSubFrame(PR_TRUE);
    InitiateLoad(aNextEntry, aParent, aLoadType);
    return NS_OK;
  }

  /* The root entries are the same, so compare any child frames */
  PRInt32 pcnt=0, ncnt=0, dsCount=0;
  nsCOMPtr<nsISHContainer>  prevContainer(do_QueryInterface(aPrevEntry));
  nsCOMPtr<nsISHContainer>  nextContainer(do_QueryInterface(aNextEntry));
  nsCOMPtr<nsIDocShellTreeNode> dsTreeNode(do_QueryInterface(aParent));

  if (!dsTreeNode)
    return NS_ERROR_FAILURE;
  if (!prevContainer || !nextContainer)
    return NS_ERROR_FAILURE;

  prevContainer->GetChildCount(&pcnt);
  nextContainer->GetChildCount(&ncnt);
  dsTreeNode->GetChildCount(&dsCount);

  // Create an array for child docshells.
  nsCOMArray<nsIDocShell> docshells;
  for (PRInt32 i = 0; i < dsCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> treeItem;
    dsTreeNode->GetChildAt(i, getter_AddRefs(treeItem));
    nsCOMPtr<nsIDocShell> shell = do_QueryInterface(treeItem);
    if (shell) {
      docshells.AppendObject(shell);
    }
  }

  // Search for something to load next.
  for (PRInt32 i = 0; i < ncnt; ++i) {
    // First get an entry which may cause a new page to be loaded.
    nsCOMPtr<nsISHEntry> nChild;
    nextContainer->GetChildAt(i, getter_AddRefs(nChild));
    if (!nChild) {
      continue;
    }
    PRUint64 docshellID = 0;
    nChild->GetDocshellID(&docshellID);

    // Then find the associated docshell.
    nsIDocShell* dsChild = nsnull;
    PRInt32 count = docshells.Count();
    for (PRInt32 j = 0; j < count; ++j) {
      PRUint64 shellID = 0;
      nsIDocShell* shell = docshells[j];
      shell->GetHistoryID(&shellID);
      if (shellID == docshellID) {
        dsChild = shell;
        break;
      }
    }
    if (!dsChild) {
      continue;
    }

    // Then look at the previous entries to see if there was
    // an entry for the docshell.
    nsCOMPtr<nsISHEntry> pChild;
    for (PRInt32 k = 0; k < pcnt; ++k) {
      nsCOMPtr<nsISHEntry> child;
      prevContainer->GetChildAt(k, getter_AddRefs(child));
      if (child) {
        PRUint64 dID = 0;
        child->GetDocshellID(&dID);
        if (dID == docshellID) {
          pChild = child;
          break;
        }
      }
    }

    // Finally recursively call this method.
    // This will either load a new page to shell or some subshell or
    // do nothing.
    CompareFrames(pChild, nChild, dsChild, aLoadType, aIsFrameFound);
  }     
  return result;
}


nsresult 
nsSHistory::InitiateLoad(nsISHEntry * aFrameEntry, nsIDocShell * aFrameDS, long aLoadType)
{
  NS_ENSURE_STATE(aFrameDS && aFrameEntry);

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;

  /* Set the loadType in the SHEntry too to  what was passed on.
   * This will be passed on to child subframes later in nsDocShell,
   * so that proper loadType is maintained through out a frameset
   */
  aFrameEntry->SetLoadType(aLoadType);    
  aFrameDS->CreateLoadInfo (getter_AddRefs(loadInfo));

  loadInfo->SetLoadType(aLoadType);
  loadInfo->SetSHEntry(aFrameEntry);

  nsCOMPtr<nsIURI> nextURI;
  nsCOMPtr<nsIHistoryEntry> hEntry(do_QueryInterface(aFrameEntry));
  hEntry->GetURI(getter_AddRefs(nextURI));
  // Time   to initiate a document load
  return aFrameDS->LoadURI(nextURI, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, PR_FALSE);

}



NS_IMETHODIMP
nsSHistory::SetRootDocShell(nsIDocShell * aDocShell)
{
  mRootDocShell = aDocShell;
  return NS_OK;
}

NS_IMETHODIMP
nsSHistory::GetRootDocShell(nsIDocShell ** aDocShell)
{
  NS_ENSURE_ARG_POINTER(aDocShell);

  *aDocShell = mRootDocShell;
  //Not refcounted. May this method should not be available for public
  // NS_IF_ADDREF(*aDocShell);
  return NS_OK;
}


NS_IMETHODIMP
nsSHistory::GetSHistoryEnumerator(nsISimpleEnumerator** aEnumerator)
{
  nsresult status = NS_OK;

  NS_ENSURE_ARG_POINTER(aEnumerator);
  nsSHEnumerator * iterator = new nsSHEnumerator(this);
  if (iterator && NS_FAILED(status = CallQueryInterface(iterator, aEnumerator)))
    delete iterator;
  return status;
}


//*****************************************************************************
//***    nsSHEnumerator: Object Management
//*****************************************************************************

nsSHEnumerator::nsSHEnumerator(nsSHistory * aSHistory):mIndex(-1)
{
  mSHistory = aSHistory;
}

nsSHEnumerator::~nsSHEnumerator()
{
  mSHistory = nsnull;
}

NS_IMPL_ISUPPORTS1(nsSHEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsSHEnumerator::HasMoreElements(PRBool * aReturn)
{
  PRInt32 cnt;
  *aReturn = PR_FALSE;
  mSHistory->GetCount(&cnt);
  if (mIndex >= -1 && mIndex < (cnt-1) ) { 
    *aReturn = PR_TRUE;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsSHEnumerator::GetNext(nsISupports **aItem)
{
  NS_ENSURE_ARG_POINTER(aItem);
  PRInt32 cnt= 0;

  nsresult  result = NS_ERROR_FAILURE;
  mSHistory->GetCount(&cnt);
  if (mIndex < (cnt-1)) {
    mIndex++;
    nsCOMPtr<nsIHistoryEntry> hEntry;
    result = mSHistory->GetEntryAtIndex(mIndex, PR_FALSE, getter_AddRefs(hEntry));
    if (hEntry)
      result = CallQueryInterface(hEntry, aItem);
  }
  return result;
}
