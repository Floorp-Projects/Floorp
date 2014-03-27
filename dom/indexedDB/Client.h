/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_client_h__
#define mozilla_dom_indexeddb_client_h__

#include "IndexedDatabase.h"

#include "mozilla/dom/quota/Client.h"

#define JOURNAL_DIRECTORY_NAME "journals"

BEGIN_INDEXEDDB_NAMESPACE

class Client : public mozilla::dom::quota::Client
{
  typedef mozilla::dom::quota::OriginOrPatternString OriginOrPatternString;
  typedef mozilla::dom::quota::PersistenceType PersistenceType;
  typedef mozilla::dom::quota::UsageInfo UsageInfo;

public:
  NS_IMETHOD_(MozExternalRefCountType)
  AddRef() MOZ_OVERRIDE;

  NS_IMETHOD_(MozExternalRefCountType)
  Release() MOZ_OVERRIDE;

  virtual Type
  GetType() MOZ_OVERRIDE
  {
    return IDB;
  }

  virtual nsresult
  InitOrigin(PersistenceType aPersistenceType,
             const nsACString& aGroup,
             const nsACString& aOrigin,
             UsageInfo* aUsageInfo) MOZ_OVERRIDE;

  virtual nsresult
  GetUsageForOrigin(PersistenceType aPersistenceType,
                    const nsACString& aGroup,
                    const nsACString& aOrigin,
                    UsageInfo* aUsageInfo) MOZ_OVERRIDE;

  virtual void
  OnOriginClearCompleted(PersistenceType aPersistenceType,
                         const OriginOrPatternString& aOriginOrPattern)
                         MOZ_OVERRIDE;

  virtual void
  ReleaseIOThreadObjects() MOZ_OVERRIDE;

  virtual bool
  IsFileServiceUtilized() MOZ_OVERRIDE
  {
    return true;
  }

  virtual bool
  IsTransactionServiceActivated() MOZ_OVERRIDE;

  virtual void
  WaitForStoragesToComplete(nsTArray<nsIOfflineStorage*>& aStorages,
                            nsIRunnable* aCallback) MOZ_OVERRIDE;

  virtual void
  AbortTransactionsForStorage(nsIOfflineStorage* aStorage) MOZ_OVERRIDE;

  virtual bool
  HasTransactionsForStorage(nsIOfflineStorage* aStorage) MOZ_OVERRIDE;

  virtual void
  ShutdownTransactionService() MOZ_OVERRIDE;

private:
  nsresult
  GetDirectory(PersistenceType aPersistenceType, const nsACString& aOrigin,
               nsIFile** aDirectory);

  nsresult
  GetUsageForDirectoryInternal(nsIFile* aDirectory,
                               UsageInfo* aUsageInfo,
                               bool aDatabaseFiles);

  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_client_h__
