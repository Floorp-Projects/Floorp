/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_indexeddatabasemanager_h__
#define mozilla_dom_indexeddb_indexeddatabasemanager_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIIndexedDatabaseManager.h"

#include "mozilla/Mutex.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"

#define INDEXEDDB_MANAGER_CONTRACTID "@mozilla.org/dom/indexeddb/manager;1"

class nsIAtom;
class nsPIDOMWindow;
class nsEventChainPostVisitor;

namespace mozilla {
namespace dom {
class TabContext;
}
}

BEGIN_INDEXEDDB_NAMESPACE

class FileManager;

class IndexedDatabaseManager MOZ_FINAL : public nsIIndexedDatabaseManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINDEXEDDATABASEMANAGER

  // Returns a non-owning reference.
  static IndexedDatabaseManager*
  GetOrCreate();

  // Returns a non-owning reference.
  static IndexedDatabaseManager*
  Get();

  // Returns an owning reference! No one should call this but the factory.
  static IndexedDatabaseManager*
  FactoryCreate();

  static bool
  IsClosed();

  static bool
  IsMainProcess()
#ifdef DEBUG
  ;
#else
  {
    return sIsMainProcess;
  }
#endif

  already_AddRefed<FileManager>
  GetFileManager(const nsACString& aOrigin,
                 const nsAString& aDatabaseName);

  void
  AddFileManager(FileManager* aFileManager);

  void
  InvalidateAllFileManagers();

  void
  InvalidateFileManagersForPattern(const nsACString& aPattern);

  void
  InvalidateFileManager(const nsACString& aOrigin,
                        const nsAString& aDatabaseName);

  nsresult
  AsyncDeleteFile(FileManager* aFileManager,
                  int64_t aFileId);

  static mozilla::Mutex&
  FileMutex()
  {
    IndexedDatabaseManager* mgr = Get();
    NS_ASSERTION(mgr, "Must have a manager here!");

    return mgr->mFileMutex;
  }

  static nsresult
  FireWindowOnError(nsPIDOMWindow* aOwner,
                    nsEventChainPostVisitor& aVisitor);

  static bool
  TabContextMayAccessOrigin(const mozilla::dom::TabContext& aContext,
                            const nsACString& aOrigin);

private:
  IndexedDatabaseManager();
  ~IndexedDatabaseManager();

  nsresult
  Init();

  void
  Destroy();

  // Maintains a list of all file managers per origin. This list isn't
  // protected by any mutex but it is only ever touched on the IO thread.
  nsClassHashtable<nsCStringHashKey,
                   nsTArray<nsRefPtr<FileManager> > > mFileManagers;

  // Lock protecting FileManager.mFileInfos and nsDOMFileBase.mFileInfos
  // It's s also used to atomically update FileInfo.mRefCnt, FileInfo.mDBRefCnt
  // and FileInfo.mSliceRefCnt
  mozilla::Mutex mFileMutex;

  static bool sIsMainProcess;
};

END_INDEXEDDB_NAMESPACE

#endif /* mozilla_dom_indexeddb_indexeddatabasemanager_h__ */
