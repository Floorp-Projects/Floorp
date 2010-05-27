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

#ifndef mozilla_dom_indexeddb_transactionthreadpool_h__
#define mozilla_dom_indexeddb_transactionthreadpool_h__

// Only meant to be included in IndexedDB source files, not exported.
#include "IndexedDatabase.h"

#include "nsIObserver.h"

#include "mozilla/Mutex.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"

class nsIRunnable;
class nsIThreadPool;

BEGIN_INDEXEDDB_NAMESPACE

class IDBTransactionRequest;
class TransactionQueue;

template<class T>
class RefPtrHashKey : public PLDHashEntryHdr
{
 public:
  typedef T *KeyType;
  typedef const T *KeyTypePointer;

  RefPtrHashKey(const T *key) : mKey(const_cast<T*>(key)) {}
  RefPtrHashKey(const RefPtrHashKey<T> &toCopy) : mKey(toCopy.mKey) {}
  ~RefPtrHashKey() {}

  KeyType GetKey() const { return mKey; }

  PRBool KeyEquals(KeyTypePointer key) const { return key == mKey; }

  static KeyTypePointer KeyToPointer(KeyType key) { return key; }
  static PLDHashNumber HashKey(KeyTypePointer key)
  {
    return NS_PTR_TO_INT32(key) >> 2;
  }
  enum { ALLOW_MEMMOVE = PR_TRUE };

 protected:
  nsRefPtr<T> mKey;
};

class TransactionThreadPool : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // returns a non-owning ref!
  static TransactionThreadPool* GetOrCreate();
  static void Shutdown();

  nsresult Dispatch(IDBTransactionRequest* aTransaction,
                    nsIRunnable* aRunnable,
                    bool aFinish = false);

protected:
  TransactionThreadPool();
  ~TransactionThreadPool();

  nsresult Init();
  nsresult Cleanup();

  mozilla::Mutex mMutex;
  nsCOMPtr<nsIThreadPool> mThreadPool;

  // Protected by mMutex.
  nsRefPtrHashtable<RefPtrHashKey<IDBTransactionRequest>, TransactionQueue>
    mTransactionsInProgress;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_transactionthreadpool_h__
