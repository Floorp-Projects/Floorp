/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

#ifndef mozilla_dom_indexeddb_indexeddatabasemanager_h__
#define mozilla_dom_indexeddb_indexeddatabasemanager_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/IDBDatabase.h"

#include "nsIIndexedDatabaseManager.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIURI.h"

#include "nsClassHashtable.h"
#include "nsHashKeys.h"

#define INDEXEDDB_MANAGER_CONTRACTID \
  "@mozilla.org/dom/indexeddb/manager;1"

class nsITimer;

BEGIN_INDEXEDDB_NAMESPACE

class IndexedDatabaseManager : public nsIIndexedDatabaseManager,
                               public nsIObserver
{
  friend class IDBDatabase;

public:
  static already_AddRefed<IndexedDatabaseManager> GetOrCreate();

  // Returns a non-owning reference.
  static IndexedDatabaseManager* Get();

  // Returns an owning reference! No one should call this but the factory.
  static IndexedDatabaseManager* FactoryCreate();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINDEXEDDATABASEMANAGER
  NS_DECL_NSIOBSERVER

  nsresult WaitForClearAndDispatch(const nsACString& aOrigin,
                                   nsIRunnable* aRunnable);

private:
  IndexedDatabaseManager();
  ~IndexedDatabaseManager();

  bool RegisterDatabase(IDBDatabase* aDatabase);
  void UnregisterDatabase(IDBDatabase* aDatabase);

  // Responsible for clearing the database files for a particular origin on the
  // IO thread.
  class OriginClearRunnable : public nsIRunnable
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    static void DatabaseCompleteCallback(IDBDatabase* aDatabase,
                                         void* aClosure)
    {
      nsRefPtr<OriginClearRunnable> runnable =
        static_cast<OriginClearRunnable*>(aClosure);
      runnable->OnDatabaseComplete(aDatabase);
    }

    void OnDatabaseComplete(IDBDatabase* aDatabase);

    OriginClearRunnable(const nsACString& aOrigin,
                        nsIThread* aThread,
                        nsTArray<nsRefPtr<IDBDatabase> >& aDatabasesWaiting)
    : mOrigin(aOrigin),
      mThread(aThread)
    {
      mDatabasesWaiting.SwapElements(aDatabasesWaiting);
    }

    nsCString mOrigin;
    nsCOMPtr<nsIThread> mThread;
    nsTArray<nsRefPtr<IDBDatabase> > mDatabasesWaiting;
    nsTArray<nsCOMPtr<nsIRunnable> > mDelayedRunnables;
  };

  inline void OnOriginClearComplete(OriginClearRunnable* aRunnable);

  // Responsible for calculating the amount of space taken up by databases of a
  // certain origin. Runs on the IO thread.
  class AsyncUsageRunnable : public nsIRunnable
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    AsyncUsageRunnable(nsIURI* aURI,
                       const nsACString& aOrigin,
                       nsIIndexedDatabaseUsageCallback* aCallback);

    void Cancel();

    inline nsresult RunInternal();

    nsCOMPtr<nsIURI> mURI;
    nsCString mOrigin;
    nsCOMPtr<nsIIndexedDatabaseUsageCallback> mCallback;
    PRUint64 mUsage;
    PRInt32 mCanceled;
  };

  inline void OnUsageCheckComplete(AsyncUsageRunnable* aRunnable);

  // Maintains a list of live databases per origin.
  nsClassHashtable<nsCStringHashKey, nsTArray<IDBDatabase*> > mLiveDatabases;

  // Maintains a list of origins that are currently being cleared.
  nsAutoTArray<nsRefPtr<OriginClearRunnable>, 1> mOriginClearRunnables;

  // Maintains a list of origins that we're currently enumerating to gather
  // usage statistics.
  nsAutoTArray<nsRefPtr<AsyncUsageRunnable>, 1> mUsageRunnables;

  nsCOMPtr<nsIThread> mIOThread;
  nsCOMPtr<nsITimer> mShutdownTimer;
};

END_INDEXEDDB_NAMESPACE

#endif /* mozilla_dom_indexeddb_indexeddatabasemanager_h__ */
