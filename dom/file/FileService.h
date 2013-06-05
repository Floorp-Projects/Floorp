/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_fileservice_h__
#define mozilla_dom_file_fileservice_h__

#include "FileCommon.h"

#include "nsIObserver.h"

#include "nsClassHashtable.h"
#include "mozilla/Attributes.h"

#include "mozilla/dom/file/FileHelper.h"
#include "mozilla/dom/file/LockedFile.h"

BEGIN_FILE_NAMESPACE

class FileService MOZ_FINAL : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Returns a non-owning reference!
  static FileService*
  GetOrCreate();

  // Returns a non-owning reference!
  static FileService*
  Get();

  static void
  Shutdown();

  // Returns true if we've begun the shutdown process.
  static bool
  IsShuttingDown();

  nsresult
  Enqueue(LockedFile* aLockedFile, FileHelper* aFileHelper);

  void
  NotifyLockedFileCompleted(LockedFile* aLockedFile);

  void
  WaitForStoragesToComplete(nsTArray<nsCOMPtr<nsIFileStorage> >& aStorages,
                            nsIRunnable* aCallback);

  void
  AbortLockedFilesForStorage(nsIFileStorage* aFileStorage);

  bool
  HasLockedFilesForStorage(nsIFileStorage* aFileStorage);

  nsIEventTarget*
  StreamTransportTarget()
  {
    NS_ASSERTION(mStreamTransportTarget, "This should never be null!");
    return mStreamTransportTarget;
  }

private:
  class LockedFileQueue MOZ_FINAL : public FileHelperListener
  {
    friend class FileService;

  public:
    NS_IMETHOD_(nsrefcnt)
    AddRef() MOZ_OVERRIDE;

    NS_IMETHOD_(nsrefcnt)
    Release() MOZ_OVERRIDE;

    inline nsresult
    Enqueue(FileHelper* aFileHelper);

    virtual void
    OnFileHelperComplete(FileHelper* aFileHelper) MOZ_OVERRIDE;

  private:
    inline
    LockedFileQueue(LockedFile* aLockedFile);

    nsresult
    ProcessQueue();

    nsAutoRefCnt mRefCnt;
    NS_DECL_OWNINGTHREAD
    nsRefPtr<LockedFile> mLockedFile;
    nsTArray<nsRefPtr<FileHelper> > mQueue;
    nsRefPtr<FileHelper> mCurrentHelper;
  };

  struct DelayedEnqueueInfo
  {
    nsRefPtr<LockedFile> mLockedFile;
    nsRefPtr<FileHelper> mFileHelper;
  };

  class FileStorageInfo
  {
    friend class FileService;

  public:
    inline LockedFileQueue*
    CreateLockedFileQueue(LockedFile* aLockedFile);

    inline LockedFileQueue*
    GetLockedFileQueue(LockedFile* aLockedFile);

    void
    RemoveLockedFileQueue(LockedFile* aLockedFile);

    bool
    HasRunningLockedFiles()
    {
      return !mLockedFileQueues.IsEmpty();
    }

    inline bool
    HasRunningLockedFiles(nsIFileStorage* aFileStorage);

    inline DelayedEnqueueInfo*
    CreateDelayedEnqueueInfo(LockedFile* aLockedFile, FileHelper* aFileHelper);

    inline void
    CollectRunningAndDelayedLockedFiles(
                                 nsIFileStorage* aFileStorage,
                                 nsTArray<nsRefPtr<LockedFile> >& aLockedFiles);

    void
    LockFileForReading(const nsAString& aFileName)
    {
      mFilesReading.PutEntry(aFileName);
    }

    void
    LockFileForWriting(const nsAString& aFileName)
    {
      mFilesWriting.PutEntry(aFileName);
    }

    bool
    IsFileLockedForReading(const nsAString& aFileName)
    {
      return mFilesReading.Contains(aFileName);
    }

    bool
    IsFileLockedForWriting(const nsAString& aFileName)
    {
      return mFilesWriting.Contains(aFileName);
    }

  private:
    FileStorageInfo()
    {
      mFilesReading.Init();
      mFilesWriting.Init();
    }

    nsTArray<nsRefPtr<LockedFileQueue> > mLockedFileQueues;
    nsTArray<DelayedEnqueueInfo> mDelayedEnqueueInfos;
    nsTHashtable<nsStringHashKey> mFilesReading;
    nsTHashtable<nsStringHashKey> mFilesWriting;
  };

  struct StoragesCompleteCallback
  {
    nsTArray<nsCOMPtr<nsIFileStorage> > mStorages;
    nsCOMPtr<nsIRunnable> mCallback;
  };

  FileService();
  ~FileService();

  nsresult
  Init();

  nsresult
  Cleanup();

  bool
  MaybeFireCallback(StoragesCompleteCallback& aCallback);

  nsCOMPtr<nsIEventTarget> mStreamTransportTarget;
  nsClassHashtable<nsISupportsHashKey, FileStorageInfo> mFileStorageInfos;
  nsTArray<StoragesCompleteCallback> mCompleteCallbacks;
};

END_FILE_NAMESPACE

#endif /* mozilla_dom_file_fileservice_h__ */
