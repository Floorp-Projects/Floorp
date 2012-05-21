/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStorageBaseDB_h___
#define nsDOMStorageBaseDB_h___

#include "nscore.h"
#include "nsDataHashtable.h"

class DOMStorageImpl;

class nsDOMStorageBaseDB
{
public:
  nsDOMStorageBaseDB();
  virtual ~nsDOMStorageBaseDB() {}

  /**
   * Marks the storage as "cached" after the DOMStorageImpl object has loaded
   * all items to its memory copy of the entries - IsScopeDirty returns false
   * after call of this method for this storage.
   *
   * When a key is changed or deleted in the storage, the storage scope is
   * marked as "dirty" again and makes the DOMStorageImpl object recache its
   * keys on next access, because IsScopeDirty returns true again.
   */
  void MarkScopeCached(DOMStorageImpl* aStorage);

  /**
   * Test whether the storage for the scope (i.e. origin or host) has been
   * changed since the last MarkScopeCached call.
   */
  bool IsScopeDirty(DOMStorageImpl* aStorage);

protected:
  nsDataHashtable<nsCStringHashKey, PRUint64> mScopesVersion;

  static PRUint64 NextGlobalVersion();
  PRUint64 CachedScopeVersion(DOMStorageImpl* aStorage);

  void MarkScopeDirty(DOMStorageImpl* aStorage);
  void MarkAllScopesDirty();

private:
  static PRUint64 sGlobalVersion;
};

#endif /* nsDOMStorageDB_h___ */
