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

#ifndef mozilla_dom_indexeddb_idbfactory_h__
#define mozilla_dom_indexeddb_idbfactory_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozIStorageConnection.h"
#include "nsIIDBFactory.h"

#include "nsIWeakReferenceUtils.h"
#include "nsXULAppAPI.h"

class nsPIDOMWindow;
class nsIAtom;

BEGIN_INDEXEDDB_NAMESPACE

struct DatabaseInfo;
class IDBDatabase;
struct ObjectStoreInfo;

class IDBFactory : public nsIIDBFactory
{
  typedef nsTArray<nsAutoPtr<ObjectStoreInfo> > ObjectStoreInfoArray;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIDBFACTORY

  static already_AddRefed<nsIIDBFactory> Create(nsPIDOMWindow* aWindow);

  static already_AddRefed<mozIStorageConnection>
  GetConnection(const nsAString& aDatabaseFilePath);

  // Called when a process uses an IndexedDB factory. We only allow
  // a single process type to use IndexedDB - the chrome/single process
  // in Firefox, and the child process in Fennec - so access by more
  // than one process type is a very serious error.
  static void
  NoteUsedByProcessType(GeckoProcessType aProcessType);

  static nsresult
  GetDirectory(nsIFile** aDirectory);

  static nsresult
  GetDirectoryForOrigin(const nsACString& aASCIIOrigin,
                        nsIFile** aDirectory);

  static nsresult
  LoadDatabaseInformation(mozIStorageConnection* aConnection,
                          nsIAtom* aDatabaseId,
                          PRUint64* aVersion,
                          ObjectStoreInfoArray& aObjectStores);

  static nsresult
  UpdateDatabaseMetadata(DatabaseInfo* aDatabaseInfo,
                         PRUint64 aVersion,
                         ObjectStoreInfoArray& aObjectStores);

private:
  IDBFactory();
  ~IDBFactory() { }

  nsresult
  OpenCommon(const nsAString& aName,
             PRInt64 aVersion,
             bool aDeleting,
             nsIIDBOpenDBRequest** _retval);

  nsCOMPtr<nsIWeakReference> mWindow;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbfactory_h__
