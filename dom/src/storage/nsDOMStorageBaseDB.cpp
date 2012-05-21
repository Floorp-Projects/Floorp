/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMStorageBaseDB.h"
#include "nsDOMStorage.h"

PRUint64 nsDOMStorageBaseDB::sGlobalVersion = 1;

nsDOMStorageBaseDB::nsDOMStorageBaseDB()
{
  mScopesVersion.Init(8);
}

// public

void
nsDOMStorageBaseDB::MarkScopeCached(DOMStorageImpl* aStorage)
{
  aStorage->SetCachedVersion(CachedScopeVersion(aStorage));
}

bool
nsDOMStorageBaseDB::IsScopeDirty(DOMStorageImpl* aStorage)
{
  return !aStorage->CachedVersion() ||
         (aStorage->CachedVersion() != CachedScopeVersion(aStorage));
}

// protected

// static
PRUint64
nsDOMStorageBaseDB::NextGlobalVersion()
{
  sGlobalVersion++;
  if (sGlobalVersion == 0) // Control overlap, never return 0
    sGlobalVersion = 1;
  return sGlobalVersion;
}

PRUint64
nsDOMStorageBaseDB::CachedScopeVersion(DOMStorageImpl* aStorage)
{
  PRUint64 currentVersion;
  if (mScopesVersion.Get(aStorage->GetScopeDBKey(), &currentVersion))
    return currentVersion;

  mScopesVersion.Put(aStorage->GetScopeDBKey(), sGlobalVersion);
  return sGlobalVersion;
}

void
nsDOMStorageBaseDB::MarkScopeDirty(DOMStorageImpl* aStorage)
{
  PRUint64 nextVersion = NextGlobalVersion();
  mScopesVersion.Put(aStorage->GetScopeDBKey(), nextVersion);

  // We may do this because the storage updates its cache along with
  // updating the database.
  aStorage->SetCachedVersion(nextVersion);
}

void
nsDOMStorageBaseDB::MarkAllScopesDirty()
{
  mScopesVersion.Clear();
  NextGlobalVersion();
}
