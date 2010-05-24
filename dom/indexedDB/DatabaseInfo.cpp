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
bool gShutdown = false;

}

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

  NS_ENSURE_FALSE(gShutdown, false);

  if (!gDatabaseHash) {
    gDatabaseHash = new DatabaseHash();
    if (!gDatabaseHash->Init()) {
      NS_ERROR("Failed to initialize hashtable!");
      return false;
    }
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
  }
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

  NS_ENSURE_FALSE(gShutdown, false);

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
    hash->objectStoreHash = new ObjectStoreInfoHash();
    if (!hash->objectStoreHash->Init()) {
      NS_ERROR("Failed to initialize hashtable!");
      return false;
    }
  }

  if (hash->objectStoreHash->Get(aInfo->name, nsnull)) {
    NS_ERROR("Already have an entry for this objectstore!");
    return false;
  }

  bool ok = !!hash->objectStoreHash->Put(aInfo->name, aInfo);
  if (ok && !hash->info->objectStoreNames.AppendElement(aInfo->name)) {
    NS_ERROR("Out of memory!");
  }
  return ok;
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
    if (gDatabaseHash->Get(aDatabaseId, &hash)) {
      if (hash->objectStoreHash) {
        hash->objectStoreHash->Remove(aName);
      }
      hash->info->objectStoreNames.RemoveElement(aName);
    }
  }
}

// static
void
ObjectStoreInfo::RemoveAllForDatabase(PRUint32 aDatabaseId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  DatabaseInfo::Remove(aDatabaseId);
}

void
mozilla::dom::indexedDB::Shutdown()
{
  NS_ASSERTION(!gShutdown, "Shutdown called twice!");
  gShutdown = true;

  // Kill the hash
  delete gDatabaseHash;
}
