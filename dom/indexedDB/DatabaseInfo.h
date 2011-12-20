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

#ifndef mozilla_dom_indexeddb_databaseinfo_h__
#define mozilla_dom_indexeddb_databaseinfo_h__

// Only meant to be included in IndexedDB source files, not exported.
#include "IndexedDatabase.h"

#include "Key.h"
#include "IDBObjectStore.h"

#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

BEGIN_INDEXEDDB_NAMESPACE

struct ObjectStoreInfo;

typedef nsRefPtrHashtable<nsStringHashKey, ObjectStoreInfo>
        ObjectStoreInfoHash;

class IDBDatabase;
class OpenDatabaseHelper;

struct DatabaseInfo
{
  friend class IDBDatabase;
  friend class OpenDatabaseHelper;

private:
  DatabaseInfo()
  : nextObjectStoreId(1),
    nextIndexId(1),
    cloned(false)
  { }
  ~DatabaseInfo();

  static bool Get(nsIAtom* aId,
                  DatabaseInfo** aInfo);

  static bool Put(DatabaseInfo* aInfo);

public:
  static void Remove(nsIAtom* aId);

  static void RemoveAllForOrigin(const nsACString& aOrigin);

  bool GetObjectStoreNames(nsTArray<nsString>& aNames);
  bool ContainsStoreName(const nsAString& aName);

  ObjectStoreInfo* GetObjectStore(const nsAString& aName);

  bool PutObjectStore(ObjectStoreInfo* aInfo);

  void RemoveObjectStore(const nsAString& aName);

  already_AddRefed<DatabaseInfo> Clone();

  nsString name;
  nsCString origin;
  PRUint64 version;
  nsCOMPtr<nsIAtom> id;
  nsString filePath;
  PRInt64 nextObjectStoreId;
  PRInt64 nextIndexId;
  bool cloned;

  nsAutoPtr<ObjectStoreInfoHash> objectStoreHash;

  NS_INLINE_DECL_REFCOUNTING(DatabaseInfo)
};

struct IndexInfo
{
#ifdef NS_BUILD_REFCNT_LOGGING
  IndexInfo();
  IndexInfo(const IndexInfo& aOther);
  ~IndexInfo();
#else
  IndexInfo()
  : id(LL_MININT), unique(false), multiEntry(false) { }
#endif

  PRInt64 id;
  nsString name;
  nsString keyPath;
  nsTArray<nsString> keyPathArray;
  bool unique;
  bool multiEntry;
};

struct ObjectStoreInfo
{
#ifdef NS_BUILD_REFCNT_LOGGING
  ObjectStoreInfo();
#else
  ObjectStoreInfo()
  : id(0), nextAutoIncrementId(0), comittedAutoIncrementId(0) { }
#endif

  ObjectStoreInfo(ObjectStoreInfo& aOther);

private:
#ifdef NS_BUILD_REFCNT_LOGGING
  ~ObjectStoreInfo();
#else
  ~ObjectStoreInfo() {}
#endif
public:

  // Constant members, can be gotten on any thread
  nsString name;
  PRInt64 id;
  nsString keyPath;
  nsTArray<nsString> keyPathArray;

  // Main-thread only members. This must *not* be touced on the database thread
  nsTArray<IndexInfo> indexes;

  // Database-thread members. After the ObjectStoreInfo has been initialized,
  // these can *only* be touced on the database thread.
  PRInt64 nextAutoIncrementId;
  PRInt64 comittedAutoIncrementId;

  // This is threadsafe since the ObjectStoreInfos are created on the database
  // thread but then only used from the main thread. Ideal would be if we
  // could transfer ownership from the database thread to the main thread, but
  // we don't have that ability yet.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ObjectStoreInfo)
};

struct IndexUpdateInfo
{
#ifdef NS_BUILD_REFCNT_LOGGING
  IndexUpdateInfo();
  ~IndexUpdateInfo();
#endif

  PRInt64 indexId;
  bool indexUnique;
  Key value;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_databaseinfo_h__
