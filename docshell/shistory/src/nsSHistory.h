/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSHistory_h
#define nsSHistory_h

// Helper Classes
#include "nsCOMPtr.h"

//Interfaces Needed
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsISHTransaction.h"
#include "nsIWebNavigation.h"
#include "nsIWeakReference.h"
#include "nsISimpleEnumerator.h"
#include "nsISHistoryListener.h"
#include "nsIHistoryEntry.h"
#include "nsIObserver.h"

// Needed to maintain global list of all SHistory objects
#include "prclist.h"

class nsIDocShell;
class nsSHEnumerator;
class nsSHistoryObserver;
class nsSHistory: public PRCList,
                  public nsISHistory,
                  public nsISHistoryInternal,
                  public nsIWebNavigation
{
public:
  nsSHistory();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHISTORY
  NS_DECL_NSISHISTORYINTERNAL
  NS_DECL_NSIWEBNAVIGATION

  // One time initialization method called upon docshell module construction
  static nsresult Startup();
  static void Shutdown();
  static void UpdatePrefs();

  // Max number of total cached content viewers.  If the pref
  // browser.sessionhistory.max_total_viewers is negative, then
  // this value is calculated based on the total amount of memory.
  // Otherwise, it comes straight from the pref.
  static PRUint32 GetMaxTotalViewers() { return sHistoryMaxTotalViewers; }

protected:
  virtual ~nsSHistory();
  friend class nsSHEnumerator;
  friend class nsSHistoryObserver;

   // Could become part of nsIWebNavigation
   NS_IMETHOD GetEntryAtIndex(PRInt32 aIndex, bool aModifyIndex, nsISHEntry** aResult);
   NS_IMETHOD GetTransactionAtIndex(PRInt32 aIndex, nsISHTransaction ** aResult);
   nsresult CompareFrames(nsISHEntry * prevEntry, nsISHEntry * nextEntry, nsIDocShell * rootDocShell, long aLoadType, bool * aIsFrameFound);
   nsresult InitiateLoad(nsISHEntry * aFrameEntry, nsIDocShell * aFrameDS, long aLoadType);

   NS_IMETHOD LoadEntry(PRInt32 aIndex, long aLoadType, PRUint32 histCmd);

#ifdef DEBUG
   nsresult PrintHistory();
#endif

  // Evict content viewers in this window which don't lie in the "safe" range
  // around aIndex.
  void EvictOutOfRangeWindowContentViewers(PRInt32 aIndex);
  static void GloballyEvictContentViewers();
  static void GloballyEvictAllContentViewers();

  // Calculates a max number of total
  // content viewers to cache, based on amount of total memory
  static PRUint32 CalcMaxTotalViewers();

  void RemoveDynEntries(PRInt32 aOldIndex, PRInt32 aNewIndex);

  nsresult LoadNextPossibleEntry(PRInt32 aNewIndex, long aLoadType, PRUint32 aHistCmd);
protected:
  // aIndex is the index of the transaction which may be removed.
  // If aKeepNext is true, aIndex is compared to aIndex + 1,
  // otherwise comparison is done to aIndex - 1.
  bool RemoveDuplicate(PRInt32 aIndex, bool aKeepNext);

  nsCOMPtr<nsISHTransaction> mListRoot;
  PRInt32 mIndex;
  PRInt32 mLength;
  PRInt32 mRequestedIndex;
  // Session History listener
  nsWeakPtr mListener;
  // Weak reference. Do not refcount this.
  nsIDocShell *  mRootDocShell;

  // Max viewers allowed total, across all SHistory objects
  static PRInt32  sHistoryMaxTotalViewers;
};
//*****************************************************************************
//***    nsSHEnumerator: Object Management
//*****************************************************************************
class nsSHEnumerator : public nsISimpleEnumerator
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

  nsSHEnumerator(nsSHistory *  aHistory);
  
protected:
  friend class nsSHistory;
  virtual ~nsSHEnumerator();
private:
  PRInt32     mIndex;
  nsSHistory *  mSHistory;  
};

#endif   /* nsSHistory */
