/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DatabaseInfo.h"

#include "nsDataHashtable.h"
#include "nsThreadUtils.h"

USING_INDEXEDDB_NAMESPACE

namespace {

typedef nsDataHashtable<nsISupportsHashKey, DatabaseInfo*>
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

PLDHashOperator
CloneObjectStoreInfo(const nsAString& aKey,
                     ObjectStoreInfo* aData,
                     void* aUserArg)
{
  ObjectStoreInfoHash* hash = static_cast<ObjectStoreInfoHash*>(aUserArg);

  nsRefPtr<ObjectStoreInfo> newInfo(new ObjectStoreInfo(*aData));

  hash->Put(aKey, newInfo);

  return PL_DHASH_NEXT;
}

}

DatabaseInfo::~DatabaseInfo()
{
  // Clones are never in the hash.
  if (!cloned) {
    DatabaseInfo::Remove(id);
  }
}

ObjectStoreInfo::ObjectStoreInfo(ObjectStoreInfo& aOther)
: name(aOther.name),
  id(aOther.id),
  keyPath(aOther.keyPath),
  indexes(aOther.indexes),
  nextAutoIncrementId(aOther.nextAutoIncrementId),
  comittedAutoIncrementId(aOther.comittedAutoIncrementId)
{
  // Doesn't copy the refcount
  MOZ_COUNT_CTOR(ObjectStoreInfo);
}

#ifdef NS_BUILD_REFCNT_LOGGING

IndexInfo::IndexInfo()
: id(LL_MININT),
  unique(false),
  multiEntry(false)
{
  MOZ_COUNT_CTOR(IndexInfo);
}

IndexInfo::IndexInfo(const IndexInfo& aOther)
: id(aOther.id),
  name(aOther.name),
  keyPath(aOther.keyPath),
  keyPathArray(aOther.keyPathArray),
  unique(aOther.unique),
  multiEntry(aOther.multiEntry)
{
  MOZ_COUNT_CTOR(IndexInfo);
}

IndexInfo::~IndexInfo()
{
  MOZ_COUNT_DTOR(IndexInfo);
}

ObjectStoreInfo::ObjectStoreInfo()
: id(0),
  nextAutoIncrementId(0),
  comittedAutoIncrementId(0)
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
DatabaseInfo::Get(nsIAtom* aId,
                  DatabaseInfo** aInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aId, "Bad id!");

  if (gDatabaseHash &&
      gDatabaseHash->Get(aId, aInfo)) {
    NS_IF_ADDREF(*aInfo);
    return true;
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
    databaseHash->Init();
    gDatabaseHash = databaseHash.forget();
  }

  if (gDatabaseHash->Get(aInfo->id, nsnull)) {
    NS_ERROR("Already know about this database!");
    return false;
  }

  gDatabaseHash->Put(aInfo->id, aInfo);

  return true;
}

// static
void
DatabaseInfo::Remove(nsIAtom* aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (gDatabaseHash) {
    gDatabaseHash->Remove(aId);

    if (!gDatabaseHash->Count()) {
      delete gDatabaseHash;
      gDatabaseHash = nsnull;
    }
  }
}

PLDHashOperator
EnumerateDatabasesRemoveOrigin(nsISupports* aId,
                               DatabaseInfo*& aDatabaseInfo,
                               void* aUserArg)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  const nsACString* origin = static_cast<const nsACString*>(aUserArg);
  return aDatabaseInfo->origin.Equals(*origin) ?
    PL_DHASH_REMOVE :
    PL_DHASH_NEXT;
}

// static
void
DatabaseInfo::RemoveAllForOrigin(const nsACString& aOrigin)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (gDatabaseHash) {
    gDatabaseHash->Enumerate(EnumerateDatabasesRemoveOrigin,
                             const_cast<nsACString*>(&aOrigin));
  }
}

bool
DatabaseInfo::GetObjectStoreNames(nsTArray<nsString>& aNames)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aNames.Clear();
  if (objectStoreHash) {
    objectStoreHash->EnumerateRead(EnumerateObjectStoreNames, &aNames);
  }
  return true;
}

bool
DatabaseInfo::ContainsStoreName(const nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return objectStoreHash && objectStoreHash->Get(aName, nsnull);
}

ObjectStoreInfo*
DatabaseInfo::GetObjectStore(const nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (objectStoreHash) {
    return objectStoreHash->GetWeak(aName);
  }

  return nsnull;
}

bool
DatabaseInfo::PutObjectStore(ObjectStoreInfo* aInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aInfo, "Null pointer!");

  if (!objectStoreHash) {
    nsAutoPtr<ObjectStoreInfoHash> hash(new ObjectStoreInfoHash());
    hash->Init();
    objectStoreHash = hash.forget();
  }

  if (objectStoreHash->Get(aInfo->name, nsnull)) {
    NS_ERROR("Already have an entry for this objectstore!");
    return false;
  }

  objectStoreHash->Put(aInfo->name, aInfo);
  return true;
}

void
DatabaseInfo::RemoveObjectStore(const nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(GetObjectStore(aName), "Don't know about this one!");

  if (objectStoreHash) {
    objectStoreHash->Remove(aName);
  }
}

already_AddRefed<DatabaseInfo>
DatabaseInfo::Clone()
{
  NS_ASSERTION(!cloned, "Should never clone a clone!");

  nsRefPtr<DatabaseInfo> dbInfo(new DatabaseInfo());

  dbInfo->cloned = true;
  dbInfo->name = name;
  dbInfo->version = version;
  dbInfo->id = id;
  dbInfo->filePath = filePath;
  dbInfo->nextObjectStoreId = nextObjectStoreId;
  dbInfo->nextIndexId = nextIndexId;

  if (objectStoreHash) {
    dbInfo->objectStoreHash = new ObjectStoreInfoHash();
    dbInfo->objectStoreHash->Init();
    objectStoreHash->EnumerateRead(CloneObjectStoreInfo,
                                   dbInfo->objectStoreHash);
  }

  return dbInfo.forget();
}
