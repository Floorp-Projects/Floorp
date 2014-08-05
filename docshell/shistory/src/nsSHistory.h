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
#include "nsIWebNavigation.h"
#include "nsISimpleEnumerator.h"
#include "nsTObserverArray.h"
#include "nsWeakPtr.h"

// Needed to maintain global list of all SHistory objects
#include "prclist.h"

class nsIDocShell;
class nsSHEnumerator;
class nsSHistoryObserver;
class nsISHEntry;
class nsISHTransaction;

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
  static uint32_t GetMaxTotalViewers() { return sHistoryMaxTotalViewers; }

protected:
  virtual ~nsSHistory();
  friend class nsSHEnumerator;
  friend class nsSHistoryObserver;

   // Could become part of nsIWebNavigation
   NS_IMETHOD GetTransactionAtIndex(int32_t aIndex, nsISHTransaction ** aResult);
   nsresult CompareFrames(nsISHEntry * prevEntry, nsISHEntry * nextEntry, nsIDocShell * rootDocShell, long aLoadType, bool * aIsFrameFound);
   nsresult InitiateLoad(nsISHEntry * aFrameEntry, nsIDocShell * aFrameDS, long aLoadType);

   NS_IMETHOD LoadEntry(int32_t aIndex, long aLoadType, uint32_t histCmd);

#ifdef DEBUG
   nsresult PrintHistory();
#endif

  // Evict content viewers in this window which don't lie in the "safe" range
  // around aIndex.
  void EvictOutOfRangeWindowContentViewers(int32_t aIndex);
  static void GloballyEvictContentViewers();
  static void GloballyEvictAllContentViewers();

  // Calculates a max number of total
  // content viewers to cache, based on amount of total memory
  static uint32_t CalcMaxTotalViewers();

  void RemoveDynEntries(int32_t aOldIndex, int32_t aNewIndex);

  nsresult LoadNextPossibleEntry(int32_t aNewIndex, long aLoadType, uint32_t aHistCmd);
protected:
  // aIndex is the index of the transaction which may be removed.
  // If aKeepNext is true, aIndex is compared to aIndex + 1,
  // otherwise comparison is done to aIndex - 1.
  bool RemoveDuplicate(int32_t aIndex, bool aKeepNext);

  nsCOMPtr<nsISHTransaction> mListRoot;
  int32_t mIndex;
  int32_t mLength;
  int32_t mRequestedIndex;
  // Session History listeners
  nsAutoTObserverArray<nsWeakPtr, 2> mListeners;
  // Weak reference. Do not refcount this.
  nsIDocShell *  mRootDocShell;

  // Max viewers allowed total, across all SHistory objects
  static int32_t  sHistoryMaxTotalViewers;
};
//*****************************************************************************
//***    nsSHEnumerator: Object Management
//*****************************************************************************
class nsSHEnumerator : public nsISimpleEnumerator
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

  explicit nsSHEnumerator(nsSHistory *  aHistory);
  
protected:
  friend class nsSHistory;
  virtual ~nsSHEnumerator();
private:
  int32_t     mIndex;
  nsSHistory *  mSHistory;  
};

#endif   /* nsSHistory */
