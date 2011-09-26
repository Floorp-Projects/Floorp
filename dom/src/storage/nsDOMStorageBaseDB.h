/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Honza Bambas <honzab@firemni.cz>
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
