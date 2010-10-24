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

#include "DatabaseInfo.h"

#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsThreadUtils.h"

USING_INDEXEDDB_NAMESPACE

namespace {

typedef nsClassHashtable<nsStringHashKey, ObjectStoreInfo>
        ObjectStoreInfoHash;

struct DatabaseInfoHash
{
  DatabaseInfoHash(DatabaseInfo* aInfo) {
    NS_ASSERTION(aInfo, "Null pointer!");
    info = aInfo;
  }

  nsAutoPtr<DatabaseInfo> info;
  nsAutoPtr<ObjectStoreInfoHash> objectStoreHash;
};

typedef nsClassHashtable<nsUint32HashKey, DatabaseInfoHash>
        DatabaseHash;

DatabaseHash* gDatabaseHash = nsnull;

PLDHashOperator
EnumerateObjectStoreNames(const nsAString& aKey,
                          ObjectStoreInfo* aData,
                          void* aUserArg)
{
  nsTArray<nsString>* array = static_cast<nsTArray<nsString>*>(aUserArg);
  if (!array->AppendElement(aData->name)) {
    NS_ERROR("Out of memory?");
    return PL_DHASH_STOP;
  }
  return PL_DHASH_NEXT;
}

}

#ifdef NS_BUILD_REFCNT_LOGGING
DatabaseInfo::DatabaseInfo()
: id(0),
  nextObjectStoreId(1),
  nextIndexId(1)
{
  MOZ_COUNT_CTOR(DatabaseInfo);
}

DatabaseInfo::~DatabaseInfo()
{
  MOZ_COUNT_DTOR(DatabaseInfo);
}

IndexInfo::IndexInfo()
: id(LL_MININT),
  unique(false),
  autoIncrement(false)
{
  MOZ_COUNT_CTOR(IndexInfo);
}

IndexInfo::~IndexInfo()
{
  MOZ_COUNT_DTOR(IndexInfo);
}

ObjectStoreInfo::ObjectStoreInfo()
: id(0),
  autoIncrement(false),
  databaseId(0)
{
  MOZ_COUNT_CTOR(ObjectStoreInfo);
}

ObjectStoreInfo::~ObjectStoreInfo()
{
  MOZ_COUNT_DTOR(ObjectStoreInfo);
}

IndexUpdateInfo::IndexUpdateInfo()
{
  MOZ_COUNT_CTOR(IndexUpdateInfo);
}

IndexUpdateInfo::~IndexUpdateInfo()
{
  MOZ_COUNT_DTOR(IndexUpdateInfo);
}
#endif /* NS_BUILD_REFCNT_LOGGING */

// static
bool
DatabaseInfo::Get(PRUint32 aId,
                  DatabaseInfo** aInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aId, "Bad id!");

  if (gDatabaseHash) {
    DatabaseInfoHash* hash;
    if (gDatabaseHash->Get(aId, &hash)) {
      if (aInfo) {
        *aInfo = hash->info;
      }
      return true;
    }
  }
  return false;
}

// static
bool
DatabaseInfo::Put(DatabaseInfo* aInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aInfo, "Null pointer!");

  if (!gDatabaseHash) {
    nsAutoPtr<DatabaseHash> databaseHash(new DatabaseHash());
    if (!databaseHash->Init()) {
      NS_ERROR("Failed to initialize hashtable!");
      return false;
    }

    gDatabaseHash = databaseHash.forget();
  }

  if (gDatabaseHash->Get(aInfo->id, nsnull)) {
    NS_ERROR("Already know about this database!");
    return false;
  }

  nsAutoPtr<DatabaseInfoHash> hash(new DatabaseInfoHash(aInfo));
  if (!gDatabaseHash->Put(aInfo->id, hash)) {
    NS_ERROR("Put failed!");
    return false;
  }

  hash.forget();
  return true;
}

// static
void
DatabaseInfo::Remove(PRUint32 aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(Get(aId, nsnull), "Don't know anything about this one!");

  if (gDatabaseHash) {
    gDatabaseHash->Remove(aId);

    if (!gDatabaseHash->Count()) {
      delete gDatabaseHash;
      gDatabaseHash = nsnull;
    }
  }
}

bool
DatabaseInfo::GetObjectStoreNames(nsTArray<nsString>& aNames)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(Get(id, nsnull), "Don't know anything about this one!");

  if (!gDatabaseHash) {
    return false;
  }

  DatabaseInfoHash* info;
  if (!gDatabaseHash->Get(id, &info)) {
    return false;
  }

  aNames.Clear();
  if (info->objectStoreHash) {
    info->objectStoreHash->EnumerateRead(EnumerateObjectStoreNames, &aNames);
  }
  return true;
}

bool
DatabaseInfo::ContainsStoreName(const nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(Get(id, nsnull), "Don't know anything about this one!");

  DatabaseInfoHash* hash;
  ObjectStoreInfo* info;

  return gDatabaseHash &&
         gDatabaseHash->Get(id, &hash) &&
         hash->objectStoreHash &&
         hash->objectStoreHash->Get(aName, &info);
}

// static
bool
ObjectStoreInfo::Get(PRUint32 aDatabaseId,
                     const nsAString& aName,
                     ObjectStoreInfo** aInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aName.IsEmpty(), "Empty object store name!");

  if (gDatabaseHash) {
    DatabaseInfoHash* hash;
    if (gDatabaseHash->Get(aDatabaseId, &hash)) {
      if (hash->objectStoreHash) {
        return !!hash->objectStoreHash->Get(aName, aInfo);
      }
    }
  }

  return false;
}

// static
bool
ObjectStoreInfo::Put(ObjectStoreInfo* aInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aInfo, "Null pointer!");

  if (!gDatabaseHash) {
    NS_ERROR("No databases known!");
    return false;
  }

  DatabaseInfoHash* hash;
  if (!gDatabaseHash->Get(aInfo->databaseId, &hash)) {
    NS_ERROR("Don't know about this database!");
    return false;
  }

  if (!hash->objectStoreHash) {
    nsAutoPtr<ObjectStoreInfoHash> objectStoreHash(new ObjectStoreInfoHash());
    if (!objectStoreHash->Init()) {
      NS_ERROR("Failed to initialize hashtable!");
      return false;
    }
    hash->objectStoreHash = objectStoreHash.forget();
  }

  if (hash->objectStoreHash->Get(aInfo->name, nsnull)) {
    NS_ERROR("Already have an entry for this objectstore!");
    return false;
  }

  return !!hash->objectStoreHash->Put(aInfo->name, aInfo);
}

// static
void
ObjectStoreInfo::Remove(PRUint32 aDatabaseId,
                        const nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(Get(aDatabaseId, aName, nsnull), "Don't know about this one!");

  if (gDatabaseHash) {
    DatabaseInfoHash* hash;
    if (gDatabaseHash->Get(aDatabaseId, &hash) && hash->objectStoreHash) {
      hash->objectStoreHash->Remove(aName);
    }
  }
}
