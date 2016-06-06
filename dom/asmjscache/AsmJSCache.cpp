/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsmJSCache.h"

#include <stdio.h>

#include "js/RootingAPI.h"
#include "jsfriendapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/CondVar.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/dom/asmjscache/PAsmJSCacheEntryChild.h"
#include "mozilla/dom/asmjscache/PAsmJSCacheEntryParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/unused.h"
#include "nsIAtom.h"
#include "nsIFile.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsISimpleEnumerator.h"
#include "nsIThread.h"
#include "nsJSPrincipals.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prio.h"
#include "private/pprio.h"
#include "mozilla/Services.h"

#define ASMJSCACHE_METADATA_FILE_NAME "metadata"
#define ASMJSCACHE_ENTRY_FILE_NAME_BASE "module"

using mozilla::dom::quota::AssertIsOnIOThread;
using mozilla::dom::quota::DirectoryLock;
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::QuotaObject;
using mozilla::dom::quota::UsageInfo;
using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::BackgroundChild;
using mozilla::ipc::IsOnBackgroundThread;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::PrincipalInfo;
using mozilla::Unused;
using mozilla::HashString;

namespace mozilla {

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPRFileDesc, PRFileDesc, PR_Close);

namespace dom {
namespace asmjscache {

namespace {

// Anything smaller should compile fast enough that caching will just add
// overhead.
static const size_t sMinCachedModuleLength = 10000;

// The number of characters to hash into the Metadata::Entry::mFastHash.
static const unsigned sNumFastHashChars = 4096;

nsresult
WriteMetadataFile(nsIFile* aMetadataFile, const Metadata& aMetadata)
{
  int32_t openFlags = PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE;

  JS::BuildIdCharVector buildId;
  bool ok = GetBuildId(&buildId);
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

  ScopedPRFileDesc fd;
  nsresult rv = aMetadataFile->OpenNSPRFileDesc(openFlags, 0644, &fd.rwget());
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t length = buildId.length();
  int32_t bytesWritten = PR_Write(fd, &length, sizeof(length));
  NS_ENSURE_TRUE(bytesWritten == sizeof(length), NS_ERROR_UNEXPECTED);

  bytesWritten = PR_Write(fd, buildId.begin(), length);
  NS_ENSURE_TRUE(bytesWritten == int32_t(length), NS_ERROR_UNEXPECTED);

  bytesWritten = PR_Write(fd, &aMetadata, sizeof(aMetadata));
  NS_ENSURE_TRUE(bytesWritten == sizeof(aMetadata), NS_ERROR_UNEXPECTED);

  return NS_OK;
}

nsresult
ReadMetadataFile(nsIFile* aMetadataFile, Metadata& aMetadata)
{
  int32_t openFlags = PR_RDONLY;

  ScopedPRFileDesc fd;
  nsresult rv = aMetadataFile->OpenNSPRFileDesc(openFlags, 0644, &fd.rwget());
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the buildid and check that it matches the current buildid

  JS::BuildIdCharVector currentBuildId;
  bool ok = GetBuildId(&currentBuildId);
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

  uint32_t length;
  int32_t bytesRead = PR_Read(fd, &length, sizeof(length));
  NS_ENSURE_TRUE(bytesRead == sizeof(length), NS_ERROR_UNEXPECTED);

  NS_ENSURE_TRUE(currentBuildId.length() == length, NS_ERROR_UNEXPECTED);

  JS::BuildIdCharVector fileBuildId;
  ok = fileBuildId.resize(length);
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

  bytesRead = PR_Read(fd, fileBuildId.begin(), length);
  NS_ENSURE_TRUE(bytesRead == int32_t(length), NS_ERROR_UNEXPECTED);

  for (uint32_t i = 0; i < length; i++) {
    if (currentBuildId[i] != fileBuildId[i]) {
      return NS_ERROR_FAILURE;
    }
  }

  // Read the Metadata struct

  bytesRead = PR_Read(fd, &aMetadata, sizeof(aMetadata));
  NS_ENSURE_TRUE(bytesRead == sizeof(aMetadata), NS_ERROR_UNEXPECTED);

  return NS_OK;
}

nsresult
GetCacheFile(nsIFile* aDirectory, unsigned aModuleIndex, nsIFile** aCacheFile)
{
  nsCOMPtr<nsIFile> cacheFile;
  nsresult rv = aDirectory->Clone(getter_AddRefs(cacheFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString cacheFileName = NS_LITERAL_STRING(ASMJSCACHE_ENTRY_FILE_NAME_BASE);
  cacheFileName.AppendInt(aModuleIndex);
  rv = cacheFile->Append(cacheFileName);
  NS_ENSURE_SUCCESS(rv, rv);

  cacheFile.forget(aCacheFile);
  return NS_OK;
}

class AutoDecreaseUsageForOrigin
{
  const nsACString& mGroup;
  const nsACString& mOrigin;

public:
  uint64_t mFreed;

  AutoDecreaseUsageForOrigin(const nsACString& aGroup,
                             const nsACString& aOrigin)

  : mGroup(aGroup),
    mOrigin(aOrigin),
    mFreed(0)
  { }

  ~AutoDecreaseUsageForOrigin()
  {
    AssertIsOnIOThread();

    if (!mFreed) {
      return;
    }

    QuotaManager* qm = QuotaManager::Get();
    MOZ_ASSERT(qm, "We are on the QuotaManager's IO thread");

    qm->DecreaseUsageForOrigin(quota::PERSISTENCE_TYPE_TEMPORARY,
                               mGroup, mOrigin, mFreed);
  }
};

static void
EvictEntries(nsIFile* aDirectory, const nsACString& aGroup,
             const nsACString& aOrigin, uint64_t aNumBytes,
             Metadata& aMetadata)
{
  AssertIsOnIOThread();

  AutoDecreaseUsageForOrigin usage(aGroup, aOrigin);

  for (int i = Metadata::kLastEntry; i >= 0 && usage.mFreed < aNumBytes; i--) {
    Metadata::Entry& entry = aMetadata.mEntries[i];
    unsigned moduleIndex = entry.mModuleIndex;

    nsCOMPtr<nsIFile> file;
    nsresult rv = GetCacheFile(aDirectory, moduleIndex, getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    bool exists;
    rv = file->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (exists) {
      int64_t fileSize;
      rv = file->GetFileSize(&fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }

      rv = file->Remove(false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }

      usage.mFreed += fileSize;
    }

    entry.clear();
  }
}

// FileDescriptorHolder owns a file descriptor and its memory mapping.
// FileDescriptorHolder is derived by two runnable classes (that is,
// (Parent|Child)Runnable.
class FileDescriptorHolder : public Runnable
{
public:
  FileDescriptorHolder()
  : mQuotaObject(nullptr),
    mFileSize(INT64_MIN),
    mFileDesc(nullptr),
    mFileMap(nullptr),
    mMappedMemory(nullptr)
  { }

  ~FileDescriptorHolder()
  {
    // These resources should have already been released by Finish().
    MOZ_ASSERT(!mQuotaObject);
    MOZ_ASSERT(!mMappedMemory);
    MOZ_ASSERT(!mFileMap);
    MOZ_ASSERT(!mFileDesc);
  }

  size_t
  FileSize() const
  {
    MOZ_ASSERT(mFileSize >= 0, "Accessing FileSize of unopened file");
    return mFileSize;
  }

  PRFileDesc*
  FileDesc() const
  {
    MOZ_ASSERT(mFileDesc, "Accessing FileDesc of unopened file");
    return mFileDesc;
  }

  bool
  MapMemory(OpenMode aOpenMode)
  {
    MOZ_ASSERT(!mFileMap, "Cannot call MapMemory twice");

    PRFileMapProtect mapFlags = aOpenMode == eOpenForRead ? PR_PROT_READONLY
                                                          : PR_PROT_READWRITE;

    mFileMap = PR_CreateFileMap(mFileDesc, mFileSize, mapFlags);
    NS_ENSURE_TRUE(mFileMap, false);

    mMappedMemory = PR_MemMap(mFileMap, 0, mFileSize);
    NS_ENSURE_TRUE(mMappedMemory, false);

    return true;
  }

  void*
  MappedMemory() const
  {
    MOZ_ASSERT(mMappedMemory, "Accessing MappedMemory of un-mapped file");
    return mMappedMemory;
  }

protected:
  // This method must be called before the directory lock is released (the lock
  // is protecting these resources). It is idempotent, so it is ok to call
  // multiple times (or before the file has been fully opened).
  void
  Finish()
  {
    if (mMappedMemory) {
      PR_MemUnmap(mMappedMemory, mFileSize);
      mMappedMemory = nullptr;
    }
    if (mFileMap) {
      PR_CloseFileMap(mFileMap);
      mFileMap = nullptr;
    }
    if (mFileDesc) {
      PR_Close(mFileDesc);
      mFileDesc = nullptr;
    }

    // Holding the QuotaObject alive until all the cache files are closed enables
    // assertions in QuotaManager that the cache entry isn't cleared while we
    // are working on it.
    mQuotaObject = nullptr;
  }

  RefPtr<QuotaObject> mQuotaObject;
  int64_t mFileSize;
  PRFileDesc* mFileDesc;
  PRFileMap* mFileMap;
  void* mMappedMemory;
};

// A runnable that implements a state machine required to open a cache entry.
// It executes in the parent for a cache access originating in the child.
// This runnable gets registered as an IPDL subprotocol actor so that it
// can communicate with the corresponding ChildRunnable.
class ParentRunnable final
  : public FileDescriptorHolder
  , public quota::OpenDirectoryListener
  , public PAsmJSCacheEntryParent
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

  ParentRunnable(const PrincipalInfo& aPrincipalInfo,
                 OpenMode aOpenMode,
                 WriteParams aWriteParams)
  : mOwningThread(NS_GetCurrentThread()),
    mPrincipalInfo(aPrincipalInfo),
    mOpenMode(aOpenMode),
    mWriteParams(aWriteParams),
    mPersistence(quota::PERSISTENCE_TYPE_INVALID),
    mState(eInitial),
    mResult(JS::AsmJSCache_InternalError),
    mIsApp(false),
    mEnforcingQuota(true),
    mActorDestroyed(false),
    mOpened(false)
  {
    MOZ_ASSERT(XRE_IsParentProcess());
    AssertIsOnOwningThread();
    MOZ_COUNT_CTOR(ParentRunnable);
  }

private:
  ~ParentRunnable()
  {
    MOZ_ASSERT(mState == eFinished);
    MOZ_ASSERT(!mDirectoryLock);
    MOZ_ASSERT(mActorDestroyed);
    MOZ_COUNT_DTOR(ParentRunnable);
  }

  bool
  IsOnOwningThread() const
  {
    MOZ_ASSERT(mOwningThread);

    bool current;
    return NS_SUCCEEDED(mOwningThread->IsOnCurrentThread(&current)) && current;
  }

  void
  AssertIsOnOwningThread() const
  {
    MOZ_ASSERT(IsOnBackgroundThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  void
  AssertIsOnNonOwningThread() const
  {
    MOZ_ASSERT(!IsOnBackgroundThread());
    MOZ_ASSERT(!IsOnOwningThread());
  }

  // This method is called on the owning thread when no cache entry was found
  // to open. If we just tried a lookup in persistent storage then we might
  // still get a hit in temporary storage (for an asm.js module that wasn't
  // compiled at install-time).
  void
  CacheMiss()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mState == eFailedToReadMetadata ||
               mState == eWaitingToOpenCacheFileForRead);
    MOZ_ASSERT(mOpenMode == eOpenForRead);

    if (mPersistence == quota::PERSISTENCE_TYPE_TEMPORARY) {
      Fail();
      return;
    }

    // Try again with a clean slate. InitOnMainThread will see that mPersistence
    // is initialized and switch to temporary storage.
    MOZ_ASSERT(mPersistence == quota::PERSISTENCE_TYPE_PERSISTENT);
    FinishOnOwningThread();
    mState = eInitial;
    NS_DispatchToMainThread(this);
  }

  // This method is called on the owning thread when the JS engine is finished
  // reading/writing the cache entry.
  void
  Close()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mState == eOpened);

    mState = eFinished;

    MOZ_ASSERT(mOpened);

    FinishOnOwningThread();
  }

  // This method is called upon any failure that prevents the eventual opening
  // of the cache entry.
  void
  Fail()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mState != eFinished);

    mState = eFinished;

    MOZ_ASSERT(!mOpened);

    FinishOnOwningThread();

    if (!mActorDestroyed) {
      Unused << Send__delete__(this, mResult);
    }
  }

  // The same as method above but is intended to be called off the owning
  // thread.
  void
  FailOnNonOwningThread()
  {
    AssertIsOnNonOwningThread();
    MOZ_ASSERT(mState != eOpened &&
               mState != eFailing &&
               mState != eFinished);

    mState = eFailing;
    MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
  }

  void
  InitPersistenceType();

  nsresult
  InitOnMainThread();

  void
  OpenDirectory();

  nsresult
  ReadMetadata();

  nsresult
  OpenCacheFileForWrite();

  nsresult
  OpenCacheFileForRead();

  void
  FinishOnOwningThread();

  void
  DispatchToIOThread()
  {
    AssertIsOnOwningThread();

    // If shutdown just started, the QuotaManager may have been deleted.
    QuotaManager* qm = QuotaManager::Get();
    if (!qm) {
      FailOnNonOwningThread();
      return;
    }

    nsresult rv = qm->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      FailOnNonOwningThread();
      return;
    }
  }

  // OpenDirectoryListener overrides.
  virtual void
  DirectoryLockAcquired(DirectoryLock* aLock) override;

  virtual void
  DirectoryLockFailed() override;

  // IPDL methods.
  bool
  Recv__delete__(const JS::AsmJSCacheResult& aResult) override
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mState != eFinished);

    if (mOpened) {
      Close();
    } else {
      Fail();
    }

    MOZ_ASSERT(mState == eFinished);

    return true;
  }

  void
  ActorDestroy(ActorDestroyReason why) override
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(!mActorDestroyed);

    mActorDestroyed = true;

    // Assume ActorDestroy can happen at any time, so probe the current state to
    // determine what needs to happen.

    if (mState == eFinished) {
      return;
    }

    if (mOpened) {
      Close();
    } else {
      Fail();
    }

    MOZ_ASSERT(mState == eFinished);
  }

  bool
  RecvSelectCacheFileToRead(const uint32_t& aModuleIndex) override
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mState == eWaitingToOpenCacheFileForRead);
    MOZ_ASSERT(mOpenMode == eOpenForRead);

    // A cache entry has been selected to open.

    mModuleIndex = aModuleIndex;
    mState = eReadyToOpenCacheFileForRead;
    DispatchToIOThread();

    return true;
  }

  bool
  RecvCacheMiss() override
  {
    AssertIsOnOwningThread();

    CacheMiss();

    return true;
  }

  nsCOMPtr<nsIEventTarget> mOwningThread;
  const PrincipalInfo mPrincipalInfo;
  const OpenMode mOpenMode;
  const WriteParams mWriteParams;

  // State initialized during eInitial:
  quota::PersistenceType mPersistence;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mOrigin;
  RefPtr<DirectoryLock> mDirectoryLock;

  // State initialized during eReadyToReadMetadata
  nsCOMPtr<nsIFile> mDirectory;
  nsCOMPtr<nsIFile> mMetadataFile;
  Metadata mMetadata;

  // State initialized during eWaitingToOpenCacheFileForRead
  unsigned mModuleIndex;

  enum State {
    eInitial, // Just created, waiting to be dispatched to main thread
    eWaitingToFinishInit, // Waiting to finish initialization
    eWaitingToOpenDirectory, // Waiting to open directory
    eWaitingToOpenMetadata, // Waiting to be called back from OpenDirectory
    eReadyToReadMetadata, // Waiting to read the metadata file on the IO thread
    eFailedToReadMetadata, // Waiting to be dispatched to owning thread after fail
    eSendingMetadataForRead, // Waiting to send OnOpenMetadataForRead
    eWaitingToOpenCacheFileForRead, // Waiting to hear back from child
    eReadyToOpenCacheFileForRead, // Waiting to open cache file for read
    eSendingCacheFile, // Waiting to send OnOpenCacheFile on the owning thread
    eOpened, // Finished calling OnOpenCacheFile, waiting to be closed
    eFailing, // Just failed, waiting to be dispatched to the owning thread
    eFinished, // Terminal state
  };
  State mState;
  JS::AsmJSCacheResult mResult;

  bool mIsApp;
  bool mEnforcingQuota;
  bool mActorDestroyed;
  bool mOpened;
};

void
ParentRunnable::InitPersistenceType()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == eInitial);

  if (mOpenMode == eOpenForWrite) {
    MOZ_ASSERT(mPersistence == quota::PERSISTENCE_TYPE_INVALID);

    // If we are performing install-time caching of an app, we'd like to store
    // the cache entry in persistent storage so the entry is never evicted,
    // but we need to check that quota is not enforced for the app.
    // That justifies us in skipping all quota checks when storing the cache
    // entry and avoids all the issues around the persistent quota prompt.
    // If quota is enforced for the app, then we can still cache in temporary
    // for a likely good first-run experience.

    MOZ_ASSERT_IF(mWriteParams.mInstalled, mIsApp);

    if (mWriteParams.mInstalled &&
        !QuotaManager::IsQuotaEnforced(quota::PERSISTENCE_TYPE_PERSISTENT,
                                       mOrigin, mIsApp)) {
      mPersistence = quota::PERSISTENCE_TYPE_PERSISTENT;
    } else {
      mPersistence = quota::PERSISTENCE_TYPE_TEMPORARY;
    }

    return;
  }

  // For the reasons described above, apps may have cache entries in both
  // persistent and temporary storage. At lookup time we don't know how and
  // where the given script was cached, so start the search in persistent
  // storage and, if that fails, search in temporary storage. (Non-apps can
  // only be stored in temporary storage.)

  MOZ_ASSERT_IF(mPersistence != quota::PERSISTENCE_TYPE_INVALID,
                mIsApp && mPersistence == quota::PERSISTENCE_TYPE_PERSISTENT);

  if (mPersistence == quota::PERSISTENCE_TYPE_INVALID && mIsApp) {
    mPersistence = quota::PERSISTENCE_TYPE_PERSISTENT;
  } else {
    mPersistence = quota::PERSISTENCE_TYPE_TEMPORARY;
  }
}

nsresult
ParentRunnable::InitOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == eInitial);
  MOZ_ASSERT(mPrincipalInfo.type() != PrincipalInfo::TNullPrincipalInfo);

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(mPrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = QuotaManager::GetInfoFromPrincipal(principal, &mSuffix, &mGroup,
                                          &mOrigin, &mIsApp);
  NS_ENSURE_SUCCESS(rv, rv);

  InitPersistenceType();

  mEnforcingQuota =
    QuotaManager::IsQuotaEnforced(mPersistence, mOrigin, mIsApp);

  return NS_OK;
}

void
ParentRunnable::OpenDirectory()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == eWaitingToFinishInit ||
             mState == eWaitingToOpenDirectory);
  MOZ_ASSERT(QuotaManager::Get());

  mState = eWaitingToOpenMetadata;

  // XXX The exclusive lock shouldn't be needed for read operations.
  QuotaManager::Get()->OpenDirectory(mPersistence,
                                     mGroup,
                                     mOrigin,
                                     mIsApp,
                                     quota::Client::ASMJS,
                                     /* aExclusive */ true,
                                     this);
}

nsresult
ParentRunnable::ReadMetadata()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == eReadyToReadMetadata);

  QuotaManager* qm = QuotaManager::Get();
  MOZ_ASSERT(qm, "We are on the QuotaManager's IO thread");

  nsresult rv =
    qm->EnsureOriginIsInitialized(mPersistence, mSuffix, mGroup, mOrigin,
                                  mIsApp, getter_AddRefs(mDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mResult = JS::AsmJSCache_StorageInitFailure;
    return rv;
  }

  rv = mDirectory->Append(NS_LITERAL_STRING(ASMJSCACHE_DIRECTORY_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = mDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    rv = mDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    DebugOnly<bool> isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(mDirectory->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory, "Should have caught this earlier!");
  }

  rv = mDirectory->Clone(getter_AddRefs(mMetadataFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMetadataFile->Append(NS_LITERAL_STRING(ASMJSCACHE_METADATA_FILE_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMetadataFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists && NS_FAILED(ReadMetadataFile(mMetadataFile, mMetadata))) {
    exists = false;
  }

  if (!exists) {
    // If we are reading, we can't possibly have a cache hit.
    if (mOpenMode == eOpenForRead) {
      return NS_ERROR_FILE_NOT_FOUND;
    }

    // Initialize Metadata with a valid empty state for the LRU cache.
    for (unsigned i = 0; i < Metadata::kNumEntries; i++) {
      Metadata::Entry& entry = mMetadata.mEntries[i];
      entry.mModuleIndex = i;
      entry.clear();
    }
  }

  return NS_OK;
}

nsresult
ParentRunnable::OpenCacheFileForWrite()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == eReadyToReadMetadata);
  MOZ_ASSERT(mOpenMode == eOpenForWrite);

  mFileSize = mWriteParams.mSize;

  // Kick out the oldest entry in the LRU queue in the metadata.
  mModuleIndex = mMetadata.mEntries[Metadata::kLastEntry].mModuleIndex;

  nsCOMPtr<nsIFile> file;
  nsresult rv = GetCacheFile(mDirectory, mModuleIndex, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  QuotaManager* qm = QuotaManager::Get();
  MOZ_ASSERT(qm, "We are on the QuotaManager's IO thread");

  if (mEnforcingQuota) {
    // Create the QuotaObject before all file IO and keep it alive until caching
    // completes to get maximum assertion coverage in QuotaManager against
    // concurrent removal, etc.
    mQuotaObject = qm->GetQuotaObject(mPersistence, mGroup, mOrigin, file);
    NS_ENSURE_STATE(mQuotaObject);

    if (!mQuotaObject->MaybeUpdateSize(mWriteParams.mSize,
                                       /* aTruncate */ false)) {
      // If the request fails, it might be because mOrigin is using too much
      // space (MaybeUpdateSize will not evict our own origin since it is
      // active). Try to make some space by evicting LRU entries until there is
      // enough space.
      EvictEntries(mDirectory, mGroup, mOrigin, mWriteParams.mSize, mMetadata);
      if (!mQuotaObject->MaybeUpdateSize(mWriteParams.mSize,
                                         /* aTruncate */ false)) {
        mResult = JS::AsmJSCache_QuotaExceeded;
        return NS_ERROR_FAILURE;
      }
    }
  }

  int32_t openFlags = PR_RDWR | PR_TRUNCATE | PR_CREATE_FILE;
  rv = file->OpenNSPRFileDesc(openFlags, 0644, &mFileDesc);
  NS_ENSURE_SUCCESS(rv, rv);

  // Move the mModuleIndex's LRU entry to the recent end of the queue.
  PodMove(mMetadata.mEntries + 1, mMetadata.mEntries, Metadata::kLastEntry);
  Metadata::Entry& entry = mMetadata.mEntries[0];
  entry.mFastHash = mWriteParams.mFastHash;
  entry.mNumChars = mWriteParams.mNumChars;
  entry.mFullHash = mWriteParams.mFullHash;
  entry.mModuleIndex = mModuleIndex;

  rv = WriteMetadataFile(mMetadataFile, mMetadata);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
ParentRunnable::OpenCacheFileForRead()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == eReadyToOpenCacheFileForRead);
  MOZ_ASSERT(mOpenMode == eOpenForRead);

  nsCOMPtr<nsIFile> file;
  nsresult rv = GetCacheFile(mDirectory, mModuleIndex, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  QuotaManager* qm = QuotaManager::Get();
  MOZ_ASSERT(qm, "We are on the QuotaManager's IO thread");

  if (mEnforcingQuota) {
    // Even though it's not strictly necessary, create the QuotaObject before
    // all file IO and keep it alive until caching completes to get maximum
    // assertion coverage in QuotaManager against concurrent removal, etc.
    mQuotaObject = qm->GetQuotaObject(mPersistence, mGroup, mOrigin, file);
    NS_ENSURE_STATE(mQuotaObject);
  }

  rv = file->GetFileSize(&mFileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t openFlags = PR_RDONLY | nsIFile::OS_READAHEAD;
  rv = file->OpenNSPRFileDesc(openFlags, 0644, &mFileDesc);
  NS_ENSURE_SUCCESS(rv, rv);

  // Move the mModuleIndex's LRU entry to the recent end of the queue.
  unsigned lruIndex = 0;
  while (mMetadata.mEntries[lruIndex].mModuleIndex != mModuleIndex) {
    if (++lruIndex == Metadata::kNumEntries) {
      return NS_ERROR_UNEXPECTED;
    }
  }
  Metadata::Entry entry = mMetadata.mEntries[lruIndex];
  PodMove(mMetadata.mEntries + 1, mMetadata.mEntries, lruIndex);
  mMetadata.mEntries[0] = entry;

  rv = WriteMetadataFile(mMetadataFile, mMetadata);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
ParentRunnable::FinishOnOwningThread()
{
  AssertIsOnOwningThread();

  // Per FileDescriptorHolder::Finish()'s comment, call before
  // releasing the directory lock.
  FileDescriptorHolder::Finish();

  mDirectoryLock = nullptr;
}

NS_IMETHODIMP
ParentRunnable::Run()
{
  nsresult rv;

  // All success/failure paths must eventually call Finish() to avoid leaving
  // the parser hanging.
  switch (mState) {
    case eInitial: {
      MOZ_ASSERT(NS_IsMainThread());

      rv = InitOnMainThread();
      if (NS_FAILED(rv)) {
        FailOnNonOwningThread();
        return NS_OK;
      }

      mState = eWaitingToFinishInit;
      MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));

      return NS_OK;
    }

    case eWaitingToFinishInit: {
      AssertIsOnOwningThread();

      if (QuotaManager::IsShuttingDown()) {
        Fail();
        return NS_OK;
      }

      if (QuotaManager::Get()) {
        OpenDirectory();
        return NS_OK;
      }

      mState = eWaitingToOpenDirectory;
      QuotaManager::GetOrCreate(this);

      return NS_OK;
    }

    case eWaitingToOpenDirectory: {
      AssertIsOnOwningThread();

      if (NS_WARN_IF(!QuotaManager::Get())) {
        Fail();
        return NS_OK;
      }

      OpenDirectory();
      return NS_OK;
    }

    case eReadyToReadMetadata: {
      AssertIsOnIOThread();

      rv = ReadMetadata();
      if (NS_FAILED(rv)) {
        mState = eFailedToReadMetadata;
        MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
        return NS_OK;
      }

      if (mOpenMode == eOpenForRead) {
        mState = eSendingMetadataForRead;
        MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));

        return NS_OK;
      }

      rv = OpenCacheFileForWrite();
      if (NS_FAILED(rv)) {
        FailOnNonOwningThread();
        return NS_OK;
      }

      mState = eSendingCacheFile;
      MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
      return NS_OK;
    }

    case eFailedToReadMetadata: {
      AssertIsOnOwningThread();

      if (mOpenMode == eOpenForRead) {
        CacheMiss();
        return NS_OK;
      }

      Fail();
      return NS_OK;
    }

    case eSendingMetadataForRead: {
      AssertIsOnOwningThread();
      MOZ_ASSERT(mOpenMode == eOpenForRead);

      mState = eWaitingToOpenCacheFileForRead;

      // Metadata is now open.
      if (!SendOnOpenMetadataForRead(mMetadata)) {
        Fail();
        return NS_OK;
      }

      return NS_OK;
    }

    case eReadyToOpenCacheFileForRead: {
      AssertIsOnIOThread();
      MOZ_ASSERT(mOpenMode == eOpenForRead);

      rv = OpenCacheFileForRead();
      if (NS_FAILED(rv)) {
        FailOnNonOwningThread();
        return NS_OK;
      }

      mState = eSendingCacheFile;
      MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
      return NS_OK;
    }

    case eSendingCacheFile: {
      AssertIsOnOwningThread();

      mState = eOpened;

      // The entry is now open.
      MOZ_ASSERT(!mOpened);
      mOpened = true;

      FileDescriptor::PlatformHandleType handle =
        FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(mFileDesc));
      if (!SendOnOpenCacheFile(mFileSize, FileDescriptor(handle))) {
        Fail();
        return NS_OK;
      }

      return NS_OK;
    }

    case eFailing: {
      AssertIsOnOwningThread();

      Fail();

      return NS_OK;
    }

    case eWaitingToOpenMetadata:
    case eWaitingToOpenCacheFileForRead:
    case eOpened:
    case eFinished: {
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Shouldn't Run() in this state");
    }
  }

  MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Corrupt state");
  return NS_OK;
}

void
ParentRunnable::DirectoryLockAcquired(DirectoryLock* aLock)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == eWaitingToOpenMetadata);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  mState = eReadyToReadMetadata;
  DispatchToIOThread();
}

void
ParentRunnable::DirectoryLockFailed()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == eWaitingToOpenMetadata);
  MOZ_ASSERT(!mDirectoryLock);

  Fail();
}

NS_IMPL_ISUPPORTS_INHERITED0(ParentRunnable, FileDescriptorHolder)

bool
FindHashMatch(const Metadata& aMetadata, const ReadParams& aReadParams,
              unsigned* aModuleIndex)
{
  // Perform a fast hash of the first sNumFastHashChars chars. Each cache entry
  // also stores an mFastHash of its first sNumFastHashChars so this gives us a
  // fast way to probabilistically determine whether we have a cache hit. We
  // still do a full hash of all the chars before returning the cache file to
  // the engine to avoid penalizing the case where there are multiple cached
  // asm.js modules where the first sNumFastHashChars are the same. The
  // mFullHash of each cache entry can have a different mNumChars so the fast
  // hash allows us to avoid performing up to Metadata::kNumEntries separate
  // full hashes.
  uint32_t numChars = aReadParams.mLimit - aReadParams.mBegin;
  MOZ_ASSERT(numChars > sNumFastHashChars);
  uint32_t fastHash = HashString(aReadParams.mBegin, sNumFastHashChars);

  for (unsigned i = 0; i < Metadata::kNumEntries ; i++) {
    // Compare the "fast hash" first to see whether it is worthwhile to
    // hash all the chars.
    Metadata::Entry entry = aMetadata.mEntries[i];
    if (entry.mFastHash != fastHash) {
      continue;
    }

    // Assuming we have enough characters, hash all the chars it would take
    // to match this cache entry and compare to the cache entry. If we get a
    // hit we'll still do a full source match later (in the JS engine), but
    // the full hash match means this is probably the cache entry we want.
    if (numChars < entry.mNumChars) {
      continue;
    }
    uint32_t fullHash = HashString(aReadParams.mBegin, entry.mNumChars);
    if (entry.mFullHash != fullHash) {
      continue;
    }

    *aModuleIndex = entry.mModuleIndex;
    return true;
  }

  return false;
}

} // unnamed namespace

PAsmJSCacheEntryParent*
AllocEntryParent(OpenMode aOpenMode,
                 WriteParams aWriteParams,
                 const PrincipalInfo& aPrincipalInfo)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(aPrincipalInfo.type() == PrincipalInfo::TNullPrincipalInfo)) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  RefPtr<ParentRunnable> runnable =
    new ParentRunnable(aPrincipalInfo, aOpenMode, aWriteParams);

  nsresult rv = NS_DispatchToMainThread(runnable);
  NS_ENSURE_SUCCESS(rv, nullptr);

  // Transfer ownership to IPDL.
  return runnable.forget().take();
}

void
DeallocEntryParent(PAsmJSCacheEntryParent* aActor)
{
  // Transfer ownership back from IPDL.
  RefPtr<ParentRunnable> op =
    dont_AddRef(static_cast<ParentRunnable*>(aActor));
}

namespace {

// A runnable that presents a single interface to the AsmJSCache ops which need
// to wait until the file is open.
class ChildRunnable final
  : public FileDescriptorHolder
  , public PAsmJSCacheEntryChild
  , public nsIIPCBackgroundChildCreateCallback
{
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;

public:
  class AutoClose
  {
    ChildRunnable* mChildRunnable;

  public:
    explicit AutoClose(ChildRunnable* aChildRunnable = nullptr)
    : mChildRunnable(aChildRunnable)
    { }

    void
    Init(ChildRunnable* aChildRunnable)
    {
      MOZ_ASSERT(!mChildRunnable);
      mChildRunnable = aChildRunnable;
    }

    ChildRunnable*
    operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN
    {
      MOZ_ASSERT(mChildRunnable);
      return mChildRunnable;
    }

    void
    Forget(ChildRunnable** aChildRunnable)
    {
      *aChildRunnable = mChildRunnable;
      mChildRunnable = nullptr;
    }

    ~AutoClose()
    {
      if (mChildRunnable) {
        mChildRunnable->Close();
      }
    }
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK

  ChildRunnable(nsIPrincipal* aPrincipal,
                OpenMode aOpenMode,
                WriteParams aWriteParams,
                ReadParams aReadParams)
  : mPrincipal(aPrincipal),
    mWriteParams(aWriteParams),
    mReadParams(aReadParams),
    mMutex("ChildRunnable::mMutex"),
    mCondVar(mMutex, "ChildRunnable::mCondVar"),
    mOpenMode(aOpenMode),
    mState(eInitial),
    mResult(JS::AsmJSCache_InternalError),
    mActorDestroyed(false),
    mWaiting(false),
    mOpened(false)
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_COUNT_CTOR(ChildRunnable);
  }

  JS::AsmJSCacheResult
  BlockUntilOpen(AutoClose* aCloser)
  {
    MOZ_ASSERT(!mWaiting, "Can only call BlockUntilOpen once");
    MOZ_ASSERT(!mOpened, "Can only call BlockUntilOpen once");

    mWaiting = true;

    nsresult rv = NS_DispatchToMainThread(this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return JS::AsmJSCache_InternalError;
    }

    {
      MutexAutoLock lock(mMutex);
      while (mWaiting) {
        mCondVar.Wait();
      }
    }

    if (!mOpened) {
      return mResult;
    }

    // Now that we're open, we're guaranteed a Close() call. However, we are
    // not guaranteed someone is holding an outstanding reference until the File
    // is closed, so we do that ourselves and Release() in OnClose().
    aCloser->Init(this);
    AddRef();
    return JS::AsmJSCache_Success;
  }

  void Cleanup()
  {
#ifdef DEBUG
    NoteActorDestroyed();
#endif
  }

private:
  ~ChildRunnable()
  {
    MOZ_ASSERT(!mWaiting, "Shouldn't be destroyed while thread is waiting");
    MOZ_ASSERT(!mOpened);
    MOZ_ASSERT(mState == eFinished);
    MOZ_ASSERT(mActorDestroyed);
    MOZ_COUNT_DTOR(ChildRunnable);
  }

  // IPDL methods.
  bool
  RecvOnOpenMetadataForRead(const Metadata& aMetadata) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mState == eOpening);

    uint32_t moduleIndex;
    if (FindHashMatch(aMetadata, mReadParams, &moduleIndex)) {
      return SendSelectCacheFileToRead(moduleIndex);
    }

    return SendCacheMiss();
  }

  bool
  RecvOnOpenCacheFile(const int64_t& aFileSize,
                      const FileDescriptor& aFileDesc) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mState == eOpening);

    mFileSize = aFileSize;

    mFileDesc = PR_ImportFile(PROsfd(aFileDesc.PlatformHandle()));
    if (!mFileDesc) {
      return false;
    }

    mState = eOpened;
    Notify(JS::AsmJSCache_Success);
    return true;
  }

  bool
  Recv__delete__(const JS::AsmJSCacheResult& aResult) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mState == eOpening);

    Fail(aResult);
    return true;
  }

  void
  ActorDestroy(ActorDestroyReason why) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    NoteActorDestroyed();
  }

  void
  Close()
  {
    MOZ_ASSERT(mState == eOpened);

    mState = eClosing;
    NS_DispatchToMainThread(this);
  }

  void
  Fail(JS::AsmJSCacheResult aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mState == eInitial || mState == eOpening);
    MOZ_ASSERT(aResult != JS::AsmJSCache_Success);

    mState = eFinished;

    FileDescriptorHolder::Finish();
    Notify(aResult);
  }

  void
  Notify(JS::AsmJSCacheResult aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());

    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mWaiting);

    mWaiting = false;
    mOpened = aResult == JS::AsmJSCache_Success;
    mResult = aResult;
    mCondVar.Notify();
  }

  void NoteActorDestroyed()
  {
    mActorDestroyed = true;
  }

  nsIPrincipal* const mPrincipal;
  nsAutoPtr<PrincipalInfo> mPrincipalInfo;
  WriteParams mWriteParams;
  ReadParams mReadParams;
  Mutex mMutex;
  CondVar mCondVar;

  // Couple enums and bools together
  const OpenMode mOpenMode;
  enum State {
    eInitial, // Just created, waiting to be dispatched to the main thread
    eBackgroundChildPending, // Waiting for the background child to be created
    eOpening, // Waiting for the parent process to respond
    eOpened, // Parent process opened the entry and sent it back
    eClosing, // Waiting to be dispatched to the main thread to Send__delete__
    eFinished // Terminal state
  };
  State mState;
  JS::AsmJSCacheResult mResult;

  bool mActorDestroyed;
  bool mWaiting;
  bool mOpened;
};

NS_IMETHODIMP
ChildRunnable::Run()
{
  switch (mState) {
    case eInitial: {
      MOZ_ASSERT(NS_IsMainThread());

      bool nullPrincipal;
      nsresult rv = mPrincipal->GetIsNullPrincipal(&nullPrincipal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        Fail(JS::AsmJSCache_InternalError);
        return NS_OK;
      }

      if (nullPrincipal) {
        NS_WARNING("AsmsJSCache not supported on null principal.");
        Fail(JS::AsmJSCache_InternalError);
        return NS_OK;
      }

      nsAutoPtr<PrincipalInfo> principalInfo(new PrincipalInfo());
      rv = PrincipalToPrincipalInfo(mPrincipal, principalInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        Fail(JS::AsmJSCache_InternalError);
        return NS_OK;
      }

      mPrincipalInfo = Move(principalInfo);

      PBackgroundChild* actor = BackgroundChild::GetForCurrentThread();
      if (actor) {
        ActorCreated(actor);
        return NS_OK;
      }

      if (NS_WARN_IF(!BackgroundChild::GetOrCreateForCurrentThread(this))) {
        Fail(JS::AsmJSCache_InternalError);
        return NS_OK;
      }

      mState = eBackgroundChildPending;
      return NS_OK;
    }

    case eClosing: {
      MOZ_ASSERT(NS_IsMainThread());

      // Per FileDescriptorHolder::Finish()'s comment, call before
      // releasing the directory lock (which happens in the parent upon receipt
      // of the Send__delete__ message).
      FileDescriptorHolder::Finish();

      MOZ_ASSERT(mOpened);
      mOpened = false;

      // Match the AddRef in BlockUntilOpen(). The main thread event loop still
      // holds an outstanding ref which will keep 'this' alive until returning to
      // the event loop.
      Release();

      if (!mActorDestroyed) {
        Unused << Send__delete__(this, JS::AsmJSCache_Success);
      }

      mState = eFinished;
      return NS_OK;
    }

    case eBackgroundChildPending:
    case eOpening:
    case eOpened:
    case eFinished: {
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Shouldn't Run() in this state");
    }
  }

  MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Corrupt state");
  return NS_OK;
}

void
ChildRunnable::ActorCreated(PBackgroundChild* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aActor->SendPAsmJSCacheEntryConstructor(this, mOpenMode, mWriteParams,
                                               *mPrincipalInfo)) {
    // Unblock the parsing thread with a failure.

    Fail(JS::AsmJSCache_InternalError);

    return;
  }

  // AddRef to keep this runnable alive until IPDL deallocates the
  // subprotocol (DeallocEntryChild).
  AddRef();

  mState = eOpening;
}

void
ChildRunnable::ActorFailed()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == eBackgroundChildPending);

  Fail(JS::AsmJSCache_InternalError);
}

NS_IMPL_ISUPPORTS_INHERITED(ChildRunnable,
                            FileDescriptorHolder,
                            nsIIPCBackgroundChildCreateCallback)

} // unnamed namespace

void
DeallocEntryChild(PAsmJSCacheEntryChild* aActor)
{
  // Match the AddRef before SendPAsmJSCacheEntryConstructor.
  static_cast<ChildRunnable*>(aActor)->Release();
}

namespace {

JS::AsmJSCacheResult
OpenFile(nsIPrincipal* aPrincipal,
         OpenMode aOpenMode,
         WriteParams aWriteParams,
         ReadParams aReadParams,
         ChildRunnable::AutoClose* aChildRunnable)
{
  MOZ_ASSERT_IF(aOpenMode == eOpenForRead, aWriteParams.mSize == 0);
  MOZ_ASSERT_IF(aOpenMode == eOpenForWrite, aReadParams.mBegin == nullptr);

  // There are three reasons we don't attempt caching from the main thread:
  //  1. In the parent process: QuotaManager::WaitForOpenAllowed prevents
  //     synchronous waiting on the main thread requiring a runnable to be
  //     dispatched to the main thread.
  //  2. In the child process: the IPDL PContent messages we need to
  //     synchronously wait on are dispatched to the main thread.
  //  3. While a cache lookup *should* be much faster than compilation, IO
  //     operations can be unpredictably slow and we'd like to avoid the
  //     occasional janks on the main thread.
  // We could use a nested event loop to address 1 and 2, but we're potentially
  // in the middle of running JS (eval()) and nested event loops can be
  // semantically observable.
  if (NS_IsMainThread()) {
    return JS::AsmJSCache_SynchronousScript;
  }

  // We need to synchronously call into the parent to open the file and
  // interact with the QuotaManager. The child can then map the file into its
  // address space to perform I/O.
  RefPtr<ChildRunnable> childRunnable =
    new ChildRunnable(aPrincipal, aOpenMode, aWriteParams, aReadParams);

  JS::AsmJSCacheResult openResult =
    childRunnable->BlockUntilOpen(aChildRunnable);
  if (openResult != JS::AsmJSCache_Success) {
    childRunnable->Cleanup();
    return openResult;
  }

  if (!childRunnable->MapMemory(aOpenMode)) {
    return JS::AsmJSCache_InternalError;
  }

  return JS::AsmJSCache_Success;
}

} // namespace

typedef uint32_t AsmJSCookieType;
static const uint32_t sAsmJSCookie = 0x600d600d;

bool
OpenEntryForRead(nsIPrincipal* aPrincipal,
                 const char16_t* aBegin,
                 const char16_t* aLimit,
                 size_t* aSize,
                 const uint8_t** aMemory,
                 intptr_t* aHandle)
{
  if (size_t(aLimit - aBegin) < sMinCachedModuleLength) {
    return false;
  }

  ReadParams readParams;
  readParams.mBegin = aBegin;
  readParams.mLimit = aLimit;

  ChildRunnable::AutoClose childRunnable;
  WriteParams notAWrite;
  JS::AsmJSCacheResult openResult =
    OpenFile(aPrincipal, eOpenForRead, notAWrite, readParams, &childRunnable);
  if (openResult != JS::AsmJSCache_Success) {
    return false;
  }

  // Although we trust that the stored cache files have not been arbitrarily
  // corrupted, it is possible that a previous execution aborted in the middle
  // of writing a cache file (crash, OOM-killer, etc). To protect against
  // partially-written cache files, we use the following scheme:
  //  - Allocate an extra word at the beginning of every cache file which
  //    starts out 0 (OpenFile opens with PR_TRUNCATE).
  //  - After the asm.js serialization is complete, PR_SyncMemMap to write
  //    everything to disk and then store a non-zero value (sAsmJSCookie)
  //    in the first word.
  //  - When attempting to read a cache file, check whether the first word is
  //    sAsmJSCookie.
  if (childRunnable->FileSize() < sizeof(AsmJSCookieType) ||
      *(AsmJSCookieType*)childRunnable->MappedMemory() != sAsmJSCookie) {
    return false;
  }

  *aSize = childRunnable->FileSize() - sizeof(AsmJSCookieType);
  *aMemory = (uint8_t*) childRunnable->MappedMemory() + sizeof(AsmJSCookieType);

  // The caller guarnatees a call to CloseEntryForRead (on success or
  // failure) at which point the file will be closed.
  childRunnable.Forget(reinterpret_cast<ChildRunnable**>(aHandle));
  return true;
}

void
CloseEntryForRead(size_t aSize,
                  const uint8_t* aMemory,
                  intptr_t aHandle)
{
  ChildRunnable::AutoClose childRunnable(
    reinterpret_cast<ChildRunnable*>(aHandle));

  MOZ_ASSERT(aSize + sizeof(AsmJSCookieType) == childRunnable->FileSize());
  MOZ_ASSERT(aMemory - sizeof(AsmJSCookieType) ==
               childRunnable->MappedMemory());
}

JS::AsmJSCacheResult
OpenEntryForWrite(nsIPrincipal* aPrincipal,
                  bool aInstalled,
                  const char16_t* aBegin,
                  const char16_t* aEnd,
                  size_t aSize,
                  uint8_t** aMemory,
                  intptr_t* aHandle)
{
  if (size_t(aEnd - aBegin) < sMinCachedModuleLength) {
    return JS::AsmJSCache_ModuleTooSmall;
  }

  // Add extra space for the AsmJSCookieType (see OpenEntryForRead).
  aSize += sizeof(AsmJSCookieType);

  static_assert(sNumFastHashChars < sMinCachedModuleLength, "HashString safe");

  WriteParams writeParams;
  writeParams.mInstalled = aInstalled;
  writeParams.mSize = aSize;
  writeParams.mFastHash = HashString(aBegin, sNumFastHashChars);
  writeParams.mNumChars = aEnd - aBegin;
  writeParams.mFullHash = HashString(aBegin, writeParams.mNumChars);

  ChildRunnable::AutoClose childRunnable;
  ReadParams notARead;
  JS::AsmJSCacheResult openResult =
    OpenFile(aPrincipal, eOpenForWrite, writeParams, notARead, &childRunnable);
  if (openResult != JS::AsmJSCache_Success) {
    return openResult;
  }

  // Strip off the AsmJSCookieType from the buffer returned to the caller,
  // which expects a buffer of aSize, not a buffer of sizeWithCookie starting
  // with a cookie.
  *aMemory = (uint8_t*) childRunnable->MappedMemory() + sizeof(AsmJSCookieType);

  // The caller guarnatees a call to CloseEntryForWrite (on success or
  // failure) at which point the file will be closed
  childRunnable.Forget(reinterpret_cast<ChildRunnable**>(aHandle));
  return JS::AsmJSCache_Success;
}

void
CloseEntryForWrite(size_t aSize,
                   uint8_t* aMemory,
                   intptr_t aHandle)
{
  ChildRunnable::AutoClose childRunnable(
    reinterpret_cast<ChildRunnable*>(aHandle));

  MOZ_ASSERT(aSize + sizeof(AsmJSCookieType) == childRunnable->FileSize());
  MOZ_ASSERT(aMemory - sizeof(AsmJSCookieType) ==
               childRunnable->MappedMemory());

  // Flush to disk before writing the cookie (see OpenEntryForRead).
  if (PR_SyncMemMap(childRunnable->FileDesc(),
                    childRunnable->MappedMemory(),
                    childRunnable->FileSize()) == PR_SUCCESS) {
    *(AsmJSCookieType*)childRunnable->MappedMemory() = sAsmJSCookie;
  }
}

class Client : public quota::Client
{
  ~Client() {}

public:
  NS_IMETHOD_(MozExternalRefCountType)
  AddRef() override;

  NS_IMETHOD_(MozExternalRefCountType)
  Release() override;

  virtual Type
  GetType() override
  {
    return ASMJS;
  }

  virtual nsresult
  InitOrigin(PersistenceType aPersistenceType,
             const nsACString& aGroup,
             const nsACString& aOrigin,
             UsageInfo* aUsageInfo) override
  {
    if (!aUsageInfo) {
      return NS_OK;
    }
    return GetUsageForOrigin(aPersistenceType, aGroup, aOrigin, aUsageInfo);
  }

  virtual nsresult
  GetUsageForOrigin(PersistenceType aPersistenceType,
                    const nsACString& aGroup,
                    const nsACString& aOrigin,
                    UsageInfo* aUsageInfo) override
  {
    QuotaManager* qm = QuotaManager::Get();
    MOZ_ASSERT(qm, "We were being called by the QuotaManager");

    nsCOMPtr<nsIFile> directory;
    nsresult rv = qm->GetDirectoryForOrigin(aPersistenceType, aOrigin,
                                            getter_AddRefs(directory));
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(directory, "We're here because the origin directory exists");

    rv = directory->Append(NS_LITERAL_STRING(ASMJSCACHE_DIRECTORY_NAME));
    NS_ENSURE_SUCCESS(rv, rv);

    DebugOnly<bool> exists;
    MOZ_ASSERT(NS_SUCCEEDED(directory->Exists(&exists)) && exists);

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore;
    while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
           hasMore && !aUsageInfo->Canceled()) {
      nsCOMPtr<nsISupports> entry;
      rv = entries->GetNext(getter_AddRefs(entry));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
      NS_ENSURE_TRUE(file, NS_NOINTERFACE);

      int64_t fileSize;
      rv = file->GetFileSize(&fileSize);
      NS_ENSURE_SUCCESS(rv, rv);

      MOZ_ASSERT(fileSize >= 0, "Negative size?!");

      // Since the client is not explicitly storing files, append to database
      // usage which represents implicit storage allocation.
      aUsageInfo->AppendToDatabaseUsage(uint64_t(fileSize));
    }
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  virtual void
  OnOriginClearCompleted(PersistenceType aPersistenceType,
                         const nsACString& aOrigin)
                         override
  { }

  virtual void
  ReleaseIOThreadObjects() override
  { }

  virtual void
  AbortOperations(const nsACString& aOrigin) override
  { }

  virtual void
  AbortOperationsForProcess(ContentParentId aContentParentId) override
  { }

  virtual void
  StartIdleMaintenance() override
  { }

  virtual void
  StopIdleMaintenance() override
  { }

  virtual void
  ShutdownWorkThreads() override
  { }

private:
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

NS_IMPL_ADDREF(asmjscache::Client)
NS_IMPL_RELEASE(asmjscache::Client)

quota::Client*
CreateClient()
{
  return new Client();
}

} // namespace asmjscache
} // namespace dom
} // namespace mozilla

namespace IPC {

using mozilla::dom::asmjscache::Metadata;
using mozilla::dom::asmjscache::WriteParams;

void
ParamTraits<Metadata>::Write(Message* aMsg, const paramType& aParam)
{
  for (unsigned i = 0; i < Metadata::kNumEntries; i++) {
    const Metadata::Entry& entry = aParam.mEntries[i];
    WriteParam(aMsg, entry.mFastHash);
    WriteParam(aMsg, entry.mNumChars);
    WriteParam(aMsg, entry.mFullHash);
    WriteParam(aMsg, entry.mModuleIndex);
  }
}

bool
ParamTraits<Metadata>::Read(const Message* aMsg, PickleIterator* aIter,
                            paramType* aResult)
{
  for (unsigned i = 0; i < Metadata::kNumEntries; i++) {
    Metadata::Entry& entry = aResult->mEntries[i];
    if (!ReadParam(aMsg, aIter, &entry.mFastHash) ||
        !ReadParam(aMsg, aIter, &entry.mNumChars) ||
        !ReadParam(aMsg, aIter, &entry.mFullHash) ||
        !ReadParam(aMsg, aIter, &entry.mModuleIndex))
    {
      return false;
    }
  }
  return true;
}

void
ParamTraits<Metadata>::Log(const paramType& aParam, std::wstring* aLog)
{
  for (unsigned i = 0; i < Metadata::kNumEntries; i++) {
    const Metadata::Entry& entry = aParam.mEntries[i];
    LogParam(entry.mFastHash, aLog);
    LogParam(entry.mNumChars, aLog);
    LogParam(entry.mFullHash, aLog);
    LogParam(entry.mModuleIndex, aLog);
  }
}

void
ParamTraits<WriteParams>::Write(Message* aMsg, const paramType& aParam)
{
  WriteParam(aMsg, aParam.mSize);
  WriteParam(aMsg, aParam.mFastHash);
  WriteParam(aMsg, aParam.mNumChars);
  WriteParam(aMsg, aParam.mFullHash);
  WriteParam(aMsg, aParam.mInstalled);
}

bool
ParamTraits<WriteParams>::Read(const Message* aMsg, PickleIterator* aIter,
                               paramType* aResult)
{
  return ReadParam(aMsg, aIter, &aResult->mSize) &&
         ReadParam(aMsg, aIter, &aResult->mFastHash) &&
         ReadParam(aMsg, aIter, &aResult->mNumChars) &&
         ReadParam(aMsg, aIter, &aResult->mFullHash) &&
         ReadParam(aMsg, aIter, &aResult->mInstalled);
}

void
ParamTraits<WriteParams>::Log(const paramType& aParam, std::wstring* aLog)
{
  LogParam(aParam.mSize, aLog);
  LogParam(aParam.mFastHash, aLog);
  LogParam(aParam.mNumChars, aLog);
  LogParam(aParam.mFullHash, aLog);
  LogParam(aParam.mInstalled, aLog);
}

} // namespace IPC
