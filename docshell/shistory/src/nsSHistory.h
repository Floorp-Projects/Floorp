/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIPrefBranch2.h"

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
  static void UpdatePrefs(nsIPrefBranch *aPrefBranch);

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
   NS_IMETHOD GetEntryAtIndex(PRInt32 aIndex, PRBool aModifyIndex, nsISHEntry** aResult);
   NS_IMETHOD GetTransactionAtIndex(PRInt32 aIndex, nsISHTransaction ** aResult);
   nsresult CompareFrames(nsISHEntry * prevEntry, nsISHEntry * nextEntry, nsIDocShell * rootDocShell, long aLoadType, PRBool * aIsFrameFound);
   nsresult InitiateLoad(nsISHEntry * aFrameEntry, nsIDocShell * aFrameDS, long aLoadType);

   NS_IMETHOD LoadEntry(PRInt32 aIndex, long aLoadType, PRUint32 histCmd);

#ifdef DEBUG
   nsresult PrintHistory();
#endif

  // Evict the viewers at indices between aStartIndex and aEndIndex,
  // including aStartIndex but not aEndIndex.
  void EvictContentViewersInRange(PRInt32 aStartIndex, PRInt32 aEndIndex);
  void EvictWindowContentViewers(PRInt32 aFromIndex, PRInt32 aToIndex);
  static void EvictGlobalContentViewer();
  static void EvictAllContentViewersGlobally();

  // Calculates a max number of total
  // content viewers to cache, based on amount of total memory
  static PRUint32 CalcMaxTotalViewers();

  void RemoveDynEntries(PRInt32 aOldIndex, PRInt32 aNewIndex);

  nsresult LoadNextPossibleEntry(PRInt32 aNewIndex, long aLoadType, PRUint32 aHistCmd);
protected:
  // Note, aIndex must be > 0, since it is compared to aIndex - 1.
  PRBool RemoveDuplicate(PRInt32 aIndex);

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
