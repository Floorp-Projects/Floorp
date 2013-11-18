/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsmJSCache.h"

#include <stdio.h>

#include "js/RootingAPI.h"
#include "jsfriendapi.h"
#include "mozilla/CondVar.h"
#include "mozilla/dom/asmjscache/PAsmJSCacheEntryChild.h"
#include "mozilla/dom/asmjscache/PAsmJSCacheEntryParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/OriginOrPatternString.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/unused.h"
#include "nsContentUtils.h"
#include "nsIAtom.h"
#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIXULAppInfo.h"
#include "nsJSPrincipals.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prio.h"
#include "private/pprio.h"

#define ASMJSCACHE_FILE_NAME "module"

using mozilla::dom::quota::AssertIsOnIOThread;
using mozilla::dom::quota::OriginOrPatternString;
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::QuotaObject;
using mozilla::dom::quota::UsageInfo;
using mozilla::unused;

namespace mozilla {
namespace dom {
namespace asmjscache {

namespace {

bool
IsMainProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

// FileDescriptorHolder owns a file descriptor and its memory mapping.
// FileDescriptorHolder is derived by all three runnable classes (that is,
// (Single|Parent|Child)ProcessRunnable. To avoid awkward workarouds,
// FileDescriptorHolder is derived virtually by File and MainProcessRunnable for
// the benefit of SingleProcessRunnable, which derives both. Since File and
// MainProcessRunnable both need to be runnables, FileDescriptorHolder also
// derives nsRunnable.
class FileDescriptorHolder : public nsRunnable
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
  // This method must be called before AllowNextSynchronizedOp (which releases
  // the lock protecting these resources). It is idempotent, so it is ok to call
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

  nsRefPtr<QuotaObject> mQuotaObject;
  int64_t mFileSize;
  PRFileDesc* mFileDesc;
  PRFileMap* mFileMap;
  void* mMappedMemory;
};

// File is a base class shared by (Single|Client)ProcessEntryRunnable that
// presents a single interface to the AsmJSCache ops which need to wait until
// the file is open, regardless of whether we are executing in the main process
// or not.
class File : public virtual FileDescriptorHolder
{
public:
  class AutoClose
  {
    File* mFile;

  public:
    explicit AutoClose(File* aFile = nullptr)
    : mFile(aFile)
    { }

    void
    Init(File* aFile)
    {
      MOZ_ASSERT(!mFile);
      mFile = aFile;
    }

    File*
    operator->() const
    {
      MOZ_ASSERT(mFile);
      return mFile;
    }

    void
    Forget(File** aFile)
    {
      *aFile = mFile;
      mFile = nullptr;
    }

    ~AutoClose()
    {
      if (mFile) {
        mFile->Close();
      }
    }
  };

  bool
  BlockUntilOpen(AutoClose *aCloser)
  {
    MOZ_ASSERT(!mWaiting, "Can only call BlockUntilOpen once");
    MOZ_ASSERT(!mOpened, "Can only call BlockUntilOpen once");

    mWaiting = true;

    nsresult rv = NS_DispatchToMainThread(this);
    NS_ENSURE_SUCCESS(rv, false);

    {
      MutexAutoLock lock(mMutex);
      while (mWaiting) {
        mCondVar.Wait();
      }
    }

    if (!mOpened) {
      return false;
    }

    // Now that we're open, we're guarnateed a Close() call. However, we are
    // not guarnateed someone is holding an outstanding reference until the File
    // is closed, so we do that ourselves and Release() in OnClose().
    aCloser->Init(this);
    AddRef();
    return true;
  }

  // This method must be called if BlockUntilOpen returns 'true'. AutoClose
  // mostly takes care of this. A derived class that implements Close() must
  // guarnatee that OnClose() is called (eventually).
  virtual void
  Close() = 0;

protected:
  File()
  : mMutex("File::mMutex"),
    mCondVar(mMutex, "File::mCondVar"),
    mWaiting(false),
    mOpened(false)
  { }

  ~File()
  {
    MOZ_ASSERT(!mWaiting, "Shouldn't be destroyed while thread is waiting");
    MOZ_ASSERT(!mOpened, "OnClose() should have been called");
  }

  void
  OnOpen()
  {
    Notify(true);
  }

  void
  OnFailure()
  {
    FileDescriptorHolder::Finish();

    Notify(false);
  }

  void
  OnClose()
  {
    FileDescriptorHolder::Finish();

    MOZ_ASSERT(mOpened);
    mOpened = false;

    // Match the AddRef in BlockUntilOpen(). The main thread event loop still
    // holds an outstanding ref which will keep 'this' alive until returning to
    // the event loop.
    Release();
  }

private:
  void
  Notify(bool aSuccess)
  {
    MOZ_ASSERT(NS_IsMainThread());

    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mWaiting);

    mWaiting = false;
    mOpened = aSuccess;
    mCondVar.Notify();
  }

  Mutex mMutex;
  CondVar mCondVar;
  bool mWaiting;
  bool mOpened;
};

// MainProcessRunnable is a base class shared by (Single|Parent)ProcessRunnable
// that factors out the runnable state machine required to open a cache entry
// that runs in the main process.
class MainProcessRunnable : public virtual FileDescriptorHolder
{
public:
  NS_DECL_NSIRUNNABLE

  // MainProcessRunnable runnable assumes that the derived class ensures
  // (through ref-counting or preconditions) that aPrincipal is kept alive for
  // the lifetime of the MainProcessRunnable.
  MainProcessRunnable(nsIPrincipal* aPrincipal,
                      OpenMode aOpenMode,
                      size_t aSizeToWrite)
  : mPrincipal(aPrincipal),
    mOpenMode(aOpenMode),
    mSizeToWrite(aSizeToWrite),
    mNeedAllowNextSynchronizedOp(false),
    mState(eInitial)
  {
    MOZ_ASSERT(IsMainProcess());
  }

  virtual ~MainProcessRunnable()
  {
    MOZ_ASSERT(mState == eFinished);
    MOZ_ASSERT(!mNeedAllowNextSynchronizedOp);
  }

protected:
  // This method is be called by the derived class (either on the JS
  // compilation thread or the main thread) when JS engine is finished
  // reading/writing the cache entry.
  void
  Close()
  {
    MOZ_ASSERT(mState == eOpened);
    mState = eClosing;
    NS_DispatchToMainThread(this);
  }

  // This method is called both internally and by derived classes upon any
  // failure that prevents the eventual opening of the cache entry.
  void
  Fail()
  {
    MOZ_ASSERT(mState == eInitial || mState == eWaitingToOpen ||
               mState == eReadyToOpen || mState == eNotifying);

    mState = eFailing;
    NS_DispatchToMainThread(this);
  }

  // Called by MainProcessRunnable on the main thread after the entry is open:
  virtual void
  OnOpen() = 0;

  // This method may be overridden, but it must be called from the overrider.
  // Called by MainProcessRunnable on the main thread after a call to Fail():
  virtual void
  OnFailure()
  {
    FinishOnMainThread();
  }

  // This method may be overridden, but it must be called from the overrider.
  // Called by MainProcessRunnable on the main thread after a call to Close():
  virtual void
  OnClose()
  {
    FinishOnMainThread();
  }

private:
  nsresult
  InitOnMainThread();

  nsresult
  OpenFileOnIOThread();

  void
  FinishOnMainThread();

  nsIPrincipal* const mPrincipal;
  const OpenMode mOpenMode;
  const size_t mSizeToWrite;

  // State initialized during eInitial:
  bool mNeedAllowNextSynchronizedOp;
  nsCString mGroup;
  nsCString mOrigin;
  nsCString mStorageId;

  enum State {
    eInitial, // Just created, waiting to be dispatched to main thread
    eWaitingToOpen, // Waiting to be called back from WaitForOpenAllowed
    eReadyToOpen, // Waiting to be dispatched to the IO thread
    eNotifying, // Waiting to be dispatched to main thread to notify of success
    eOpened, // Finished calling OnOpen from main thread, waiting to be closed
    eClosing, // Waiting to be dispatched to main thread again
    eFailing, // Just failed, waiting to be dispatched to the main thread
    eFinished, // Terminal state
  };
  State mState;
};

nsresult
MainProcessRunnable::InitOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == eInitial);

  QuotaManager* qm = QuotaManager::GetOrCreate();
  NS_ENSURE_STATE(qm);

  nsresult rv = QuotaManager::GetInfoFromPrincipal(mPrincipal, &mGroup,
                                                   &mOrigin, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  QuotaManager::GetStorageId(quota::PERSISTENCE_TYPE_TEMPORARY,
                             mOrigin, quota::Client::ASMJS,
                             NS_LITERAL_STRING(ASMJSCACHE_FILE_NAME),
                             mStorageId);

  return NS_OK;
}

nsresult
MainProcessRunnable::OpenFileOnIOThread()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == eReadyToOpen);

  QuotaManager* qm = QuotaManager::Get();
  MOZ_ASSERT(qm, "We are on the QuotaManager's IO thread");

  nsCOMPtr<nsIFile> path;
  nsresult rv = qm->EnsureOriginIsInitialized(quota::PERSISTENCE_TYPE_TEMPORARY,
                                              mGroup, mOrigin, true,
                                              getter_AddRefs(path));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = path->Append(NS_LITERAL_STRING(ASMJSCACHE_DIRECTORY_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = path->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    rv = path->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    DebugOnly<bool> isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(path->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory, "Should have caught this earlier!");
  }

  rv = path->Append(NS_LITERAL_STRING(ASMJSCACHE_FILE_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  mQuotaObject = qm->GetQuotaObject(quota::PERSISTENCE_TYPE_TEMPORARY,
                                    mGroup, mOrigin, path);
  NS_ENSURE_STATE(mQuotaObject);

  PRIntn openFlags;
  if (mOpenMode == eOpenForRead) {
    rv = path->GetFileSize(&mFileSize);
    if (NS_FAILED(rv)) {
      return rv;
    }

    openFlags = PR_RDONLY | nsIFile::OS_READAHEAD;
  } else {
    if (!mQuotaObject->MaybeAllocateMoreSpace(0, mSizeToWrite)) {
      return NS_ERROR_FAILURE;
    }

    mFileSize = mSizeToWrite;

    MOZ_ASSERT(mOpenMode == eOpenForWrite);
    openFlags = PR_RDWR | PR_TRUNCATE | PR_CREATE_FILE;
  }

  rv = path->OpenNSPRFileDesc(openFlags, 0644, &mFileDesc);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
MainProcessRunnable::FinishOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Per FileDescriptorHolder::Finish()'s comment, call before
  // AllowNextSynchronizedOp.
  FileDescriptorHolder::Finish();

  if (mNeedAllowNextSynchronizedOp) {
    mNeedAllowNextSynchronizedOp = false;
    QuotaManager* qm = QuotaManager::Get();
    if (qm) {
      qm->AllowNextSynchronizedOp(OriginOrPatternString::FromOrigin(mOrigin),
                                  Nullable<PersistenceType>(), mStorageId);
    }
  }
}

NS_IMETHODIMP
MainProcessRunnable::Run()
{
  nsresult rv;

  // All success/failure paths must eventually call Finish() to avoid leaving
  // the parser hanging.
  switch (mState) {
    case eInitial: {
      MOZ_ASSERT(NS_IsMainThread());

      rv = InitOnMainThread();
      if (NS_FAILED(rv)) {
        Fail();
        return NS_OK;
      }

      mState = eWaitingToOpen;
      rv = QuotaManager::Get()->WaitForOpenAllowed(
                                     OriginOrPatternString::FromOrigin(mOrigin),
                                     Nullable<PersistenceType>(), mStorageId,
                                     this);
      if (NS_FAILED(rv)) {
        Fail();
        return NS_OK;
      }

      mNeedAllowNextSynchronizedOp = true;
      return NS_OK;
    }

    case eWaitingToOpen: {
      MOZ_ASSERT(NS_IsMainThread());

      mState = eReadyToOpen;

      QuotaManager* qm = QuotaManager::Get();
      if (!qm) {
        Fail();
        return NS_OK;
      }

      rv = qm->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
      if (NS_FAILED(rv)) {
        Fail();
        return NS_OK;
      }

      return NS_OK;
    }

    case eReadyToOpen: {
      AssertIsOnIOThread();

      rv = OpenFileOnIOThread();
      if (NS_FAILED(rv)) {
        Fail();
        return NS_OK;
      }

      mState = eNotifying;
      NS_DispatchToMainThread(this);
      return NS_OK;
    }

    case eNotifying: {
      MOZ_ASSERT(NS_IsMainThread());

      mState = eOpened;
      OnOpen();
      return NS_OK;
    }

    case eFailing: {
      MOZ_ASSERT(NS_IsMainThread());

      mState = eFinished;
      OnFailure();
      return NS_OK;
    }

    case eClosing: {
      MOZ_ASSERT(NS_IsMainThread());

      mState = eFinished;
      OnClose();
      return NS_OK;
    }

    case eOpened:
    case eFinished: {
      MOZ_ASSUME_UNREACHABLE("Shouldn't Run() in this state");
    }
  }

  MOZ_ASSUME_UNREACHABLE("Corrupt state");
  return NS_OK;
}

// A runnable that executes for a cache access originating in the main process.
class SingleProcessRunnable MOZ_FINAL : public File,
                                        private MainProcessRunnable
{
public:
  // In the single-process case, the calling JS compilation thread holds the
  // nsIPrincipal alive indirectly (via the global object -> compartment ->
  // principal) so we don't have to ref-count it here. This is fortunate since
  // we are off the main thread and nsIPrincipals can only be ref-counted on
  // the main thread.
  SingleProcessRunnable(nsIPrincipal* aPrincipal,
                        OpenMode aOpenMode,
                        size_t aSizeToWrite)
  : MainProcessRunnable(aPrincipal, aOpenMode, aSizeToWrite)
  {
    MOZ_ASSERT(IsMainProcess());
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_COUNT_CTOR(SingleProcessRunnable);
  }

  ~SingleProcessRunnable()
  {
    MOZ_COUNT_DTOR(SingleProcessRunnable);
  }

private:
  void
  Close() MOZ_OVERRIDE MOZ_FINAL
  {
    MainProcessRunnable::Close();
  }

  void
  OnOpen() MOZ_OVERRIDE
  {
    File::OnOpen();
  }

  void
  OnFailure() MOZ_OVERRIDE
  {
    MainProcessRunnable::OnFailure();
    File::OnFailure();
  }

  void
  OnClose() MOZ_OVERRIDE MOZ_FINAL
  {
    MainProcessRunnable::OnClose();
    File::OnClose();
  }

  // Avoid MSVC 'dominance' warning by having clear Run() override.
  NS_IMETHODIMP
  Run() MOZ_OVERRIDE
  {
    return MainProcessRunnable::Run();
  }
};

// A runnable that executes in a parent process for a cache access originating
// in the content process. This runnable gets registered as an IPDL subprotocol
// actor so that it can communicate with the corresponding ChildProcessRunnable.
class ParentProcessRunnable MOZ_FINAL : public PAsmJSCacheEntryParent,
                                        public MainProcessRunnable
{
public:
  // The given principal comes from an IPC::Principal which will be dec-refed
  // at the end of the message, so we must ref-count it here. Fortunately, we
  // are on the main thread (where PContent messages are delivered).
  ParentProcessRunnable(nsIPrincipal* aPrincipal,
                        OpenMode aOpenMode,
                        size_t aSizeToWrite)
  : MainProcessRunnable(aPrincipal, aOpenMode, aSizeToWrite),
    mPrincipalHolder(aPrincipal),
    mActorDestroyed(false),
    mOpened(false),
    mFinished(false)
  {
    MOZ_ASSERT(IsMainProcess());
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_COUNT_CTOR(ParentProcessRunnable);
  }

private:
  ~ParentProcessRunnable()
  {
    MOZ_ASSERT(!mPrincipalHolder, "Should have already been released");
    MOZ_ASSERT(mActorDestroyed);
    MOZ_ASSERT(mFinished);
    MOZ_COUNT_DTOR(ParentProcessRunnable);
  }

  bool
  Recv__delete__() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mOpened);

    MOZ_ASSERT(!mFinished);
    mFinished = true;

    MainProcessRunnable::Close();
    return true;
  }

  void
  ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
  {
    MOZ_ASSERT(!mActorDestroyed);
    mActorDestroyed = true;

    // Assume ActorDestroy can happen at any time, so probe the current state to
    // determine what needs to happen.

    if (mFinished) {
      return;
    }

    mFinished = true;

    if (mOpened) {
      MainProcessRunnable::Close();
    } else {
      MainProcessRunnable::Fail();
    }
  }

  void
  OnOpen() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    MOZ_ASSERT(!mOpened);
    mOpened = true;

    FileDescriptor::PlatformHandleType handle =
      FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(mFileDesc));
    if (!SendOnOpen(mFileSize, handle)) {
      unused << Send__delete__(this);
    }
  }

  void
  OnClose() MOZ_OVERRIDE MOZ_FINAL
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mOpened);

    mFinished = true;

    MainProcessRunnable::OnClose();

    MOZ_ASSERT(mActorDestroyed);

    mPrincipalHolder = nullptr;
  }

  void
  OnFailure() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mOpened);

    mFinished = true;

    MainProcessRunnable::OnFailure();

    if (!mActorDestroyed) {
      unused << Send__delete__(this);
    }

    mPrincipalHolder = nullptr;
  }

  nsCOMPtr<nsIPrincipal> mPrincipalHolder;
  bool mActorDestroyed;
  bool mOpened;
  bool mFinished;
};

} // unnamed namespace

PAsmJSCacheEntryParent*
AllocEntryParent(OpenMode aOpenMode,
                 uint32_t aSizeToWrite,
                 nsIPrincipal* aPrincipal)
{
  ParentProcessRunnable* runnable =
    new ParentProcessRunnable(aPrincipal, aOpenMode, aSizeToWrite);

  // AddRef to keep the runnable alive until DeallocEntryParent.
  runnable->AddRef();

  nsresult rv = NS_DispatchToMainThread(runnable);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return runnable;
}

void
DeallocEntryParent(PAsmJSCacheEntryParent* aActor)
{
  // Match the AddRef in AllocEntryParent.
  static_cast<ParentProcessRunnable*>(aActor)->Release();
}

namespace {

class ChildProcessRunnable MOZ_FINAL : public File,
                                       public PAsmJSCacheEntryChild
{
public:
  NS_DECL_NSIRUNNABLE

  // In the single-process case, the calling JS compilation thread holds the
  // nsIPrincipal alive indirectly (via the global object -> compartment ->
  // principal) so we don't have to ref-count it here. This is fortunate since
  // we are off the main thread and nsIPrincipals can only be ref-counted on
  // the main thread.
  ChildProcessRunnable(nsIPrincipal* aPrincipal,
                       OpenMode aOpenMode,
                       size_t aSizeToWrite)
  : mPrincipal(aPrincipal),
    mOpenMode(aOpenMode),
    mSizeToWrite(aSizeToWrite),
    mActorDestroyed(false),
    mState(eInitial)
  {
    MOZ_ASSERT(!IsMainProcess());
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_COUNT_CTOR(ChildProcessRunnable);
  }

  ~ChildProcessRunnable()
  {
    MOZ_ASSERT(mState == eFinished);
    MOZ_ASSERT(mActorDestroyed);
    MOZ_COUNT_DTOR(ChildProcessRunnable);
  }

private:
  bool
  RecvOnOpen(const int64_t& aFileSize, const FileDescriptor& aFileDesc)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mState == eOpening);

    mFileSize = aFileSize;

    mFileDesc = PR_ImportFile(PROsfd(aFileDesc.PlatformHandle()));
    if (!mFileDesc) {
      return false;
    }

    mState = eOpened;
    File::OnOpen();
    return true;
  }

  bool
  Recv__delete__() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mState == eOpening);

    mState = eFinished;
    File::OnFailure();
    return true;
  }

  void
  ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    mActorDestroyed = true;
  }

  void
  Close() MOZ_OVERRIDE MOZ_FINAL
  {
    MOZ_ASSERT(mState == eOpened);

    mState = eClosing;
    NS_DispatchToMainThread(this);
  }

private:
  nsIPrincipal* const mPrincipal;
  const OpenMode mOpenMode;
  size_t mSizeToWrite;
  bool mActorDestroyed;

  enum State {
    eInitial, // Just created, waiting to dispatched to the main thread
    eOpening, // Waiting for the parent process to respond
    eOpened, // Parent process opened the entry and sent it back
    eClosing, // Waiting to be dispatched to the main thread to Send__delete__
    eFinished // Terminal state
  };
  State mState;
};

NS_IMETHODIMP
ChildProcessRunnable::Run()
{
  switch (mState) {
    case eInitial: {
      MOZ_ASSERT(NS_IsMainThread());

      // AddRef to keep this runnable alive until IPDL deallocates the
      // subprotocol (DeallocEntryChild).
      AddRef();

      if (!ContentChild::GetSingleton()->SendPAsmJSCacheEntryConstructor(
        this, mOpenMode, mSizeToWrite, IPC::Principal(mPrincipal)))
      {
        // On failure, undo the AddRef (since DeallocEntryChild will not be
        // called) and unblock the parsing thread with a failure. The main
        // thread event loop still holds an outstanding ref which will keep
        // 'this' alive until returning to the event loop.
        Release();

        mState = eFinished;
        File::OnFailure();
        return NS_OK;
      }

      mState = eOpening;
      return NS_OK;
    }

    case eClosing: {
      MOZ_ASSERT(NS_IsMainThread());

      // Per FileDescriptorHolder::Finish()'s comment, call before
      // AllowNextSynchronizedOp (which happens in the parent upon receipt of
      // the Send__delete__ message).
      File::OnClose();

      if (!mActorDestroyed) {
        unused << Send__delete__(this);
      }

      mState = eFinished;
      return NS_OK;
    }

    case eOpening:
    case eOpened:
    case eFinished: {
      MOZ_ASSUME_UNREACHABLE("Shouldn't Run() in this state");
    }
  }

  MOZ_ASSUME_UNREACHABLE("Corrupt state");
  return NS_OK;
}

} // unnamed namespace

void
DeallocEntryChild(PAsmJSCacheEntryChild* aActor)
{
  // Match the AddRef before SendPAsmJSCacheEntryConstructor.
  static_cast<ChildProcessRunnable*>(aActor)->Release();
}

namespace {

bool
OpenFile(JS::Handle<JSObject*> aGlobal,
         OpenMode aOpenMode,
         size_t aSizeToWrite,
         File::AutoClose* aFile)
{
  MOZ_ASSERT_IF(aOpenMode == eOpenForRead, aSizeToWrite == 0);

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
    return false;
  }

  // This assumes a non-worker global.
  nsIPrincipal* principal = nsContentUtils::GetObjectPrincipal(aGlobal);

  // If we are in a child process, we need to synchronously call into the
  // parent process to open the file and interact with the QuotaManager. The
  // child can then map the file into its address space to perform I/O.
  nsRefPtr<File> file;
  if (IsMainProcess()) {
    file = new SingleProcessRunnable(principal, aOpenMode, aSizeToWrite);
  } else {
    file = new ChildProcessRunnable(principal, aOpenMode, aSizeToWrite);
  }

  if (!file->BlockUntilOpen(aFile)) {
    return false;
  }

  return file->MapMemory(aOpenMode);
}

} // anonymous namespace

typedef uint32_t AsmJSCookieType;
static const uint32_t sAsmJSCookie = 0x600d600d;

// Anything smaller should compile fast enough that caching will just add
// overhead.
static const size_t sMinCachedModuleLength = 10000;

bool
OpenEntryForRead(JS::Handle<JSObject*> aGlobal,
                 const jschar* aBegin,
                 const jschar* aLimit,
                 size_t* aSize,
                 const uint8_t** aMemory,
                 intptr_t* aFile)
{
  if (size_t(aLimit - aBegin) < sMinCachedModuleLength) {
    return false;
  }

  File::AutoClose file;
  if (!OpenFile(aGlobal, eOpenForRead, 0, &file)) {
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
  if (file->FileSize() < sizeof(AsmJSCookieType) ||
      *(AsmJSCookieType*)file->MappedMemory() != sAsmJSCookie) {
    return false;
  }

  *aSize = file->FileSize() - sizeof(AsmJSCookieType);
  *aMemory = (uint8_t*) file->MappedMemory() + sizeof(AsmJSCookieType);

  // The caller guarnatees a call to CloseEntryForRead (on success or
  // failure) at which point the file will be closed.
  file.Forget(reinterpret_cast<File**>(aFile));
  return true;
}

void
CloseEntryForRead(JS::Handle<JSObject*> global,
                  size_t aSize,
                  const uint8_t* aMemory,
                  intptr_t aFile)
{
  File::AutoClose file(reinterpret_cast<File*>(aFile));

  MOZ_ASSERT(aSize + sizeof(AsmJSCookieType) == file->FileSize());
  MOZ_ASSERT(aMemory - sizeof(AsmJSCookieType) == file->MappedMemory());
}

bool
OpenEntryForWrite(JS::Handle<JSObject*> aGlobal,
                  const jschar* aBegin,
                  const jschar* aEnd,
                  size_t aSize,
                  uint8_t** aMemory,
                  intptr_t* aFile)
{
  if (size_t(aEnd - aBegin) < sMinCachedModuleLength) {
    return false;
  }

  // Add extra space for the AsmJSCookieType (see OpenEntryForRead).
  aSize += sizeof(AsmJSCookieType);

  File::AutoClose file;
  if (!OpenFile(aGlobal, eOpenForWrite, aSize, &file)) {
    return false;
  }

  // Strip off the AsmJSCookieType from the buffer returned to the caller,
  // which expects a buffer of aSize, not a buffer of sizeWithCookie starting
  // with a cookie.
  *aMemory = (uint8_t*) file->MappedMemory() + sizeof(AsmJSCookieType);

  // The caller guarnatees a call to CloseEntryForWrite (on success or
  // failure) at which point the file will be closed
  file.Forget(reinterpret_cast<File**>(aFile));
  return true;
}

void
CloseEntryForWrite(JS::Handle<JSObject*> global,
                   size_t aSize,
                   uint8_t* aMemory,
                   intptr_t aFile)
{
  File::AutoClose file(reinterpret_cast<File*>(aFile));

  MOZ_ASSERT(aSize + sizeof(AsmJSCookieType) == file->FileSize());
  MOZ_ASSERT(aMemory - sizeof(AsmJSCookieType) == file->MappedMemory());

  // Flush to disk before writing the cookie (see OpenEntryForRead).
  if (PR_SyncMemMap(file->FileDesc(),
                    file->MappedMemory(),
                    file->FileSize()) == PR_SUCCESS) {
    *(AsmJSCookieType*)file->MappedMemory() = sAsmJSCookie;
  }
}

bool
GetBuildId(js::Vector<char>* aBuildID)
{
  nsCOMPtr<nsIXULAppInfo> info = do_GetService("@mozilla.org/xre/app-info;1");
  if (!info) {
    return false;
  }

  nsCString buildID;
  nsresult rv = info->GetPlatformBuildID(buildID);
  NS_ENSURE_SUCCESS(rv, false);

  if (!aBuildID->resize(buildID.Length())) {
    return false;
  }

  for (size_t i = 0; i < buildID.Length(); i++) {
    (*aBuildID)[i] = buildID[i];
  }

  return true;
}

class Client : public quota::Client
{
public:
  NS_IMETHOD_(nsrefcnt)
  AddRef() MOZ_OVERRIDE;

  NS_IMETHOD_(nsrefcnt)
  Release() MOZ_OVERRIDE;

  virtual Type
  GetType() MOZ_OVERRIDE
  {
    return ASMJS;
  }

  virtual nsresult
  InitOrigin(PersistenceType aPersistenceType,
             const nsACString& aGroup,
             const nsACString& aOrigin,
             UsageInfo* aUsageInfo) MOZ_OVERRIDE
  {
    return GetUsageForOrigin(aPersistenceType, aGroup, aOrigin, aUsageInfo);
  }

  virtual nsresult
  GetUsageForOrigin(PersistenceType aPersistenceType,
                    const nsACString& aGroup,
                    const nsACString& aOrigin,
                    UsageInfo* aUsageInfo) MOZ_OVERRIDE
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

    bool exists;
    MOZ_ASSERT(NS_SUCCEEDED(directory->Exists(&exists)) && exists);

    nsIFile* path = directory;
    rv = path->Append(NS_LITERAL_STRING(ASMJSCACHE_FILE_NAME));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = path->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
      int64_t fileSize;
      rv = path->GetFileSize(&fileSize);
      NS_ENSURE_SUCCESS(rv, rv);

      MOZ_ASSERT(fileSize >= 0, "Negative size?!");

      // Since the client is not explicitly storing files, append to database
      // usage which represents implicit storage allocation.
      aUsageInfo->AppendToDatabaseUsage(uint64_t(fileSize));
    }

    return NS_OK;
  }

  virtual void
  OnOriginClearCompleted(PersistenceType aPersistenceType,
                         const OriginOrPatternString& aOriginOrPattern)
                         MOZ_OVERRIDE
  { }

  virtual void
  ReleaseIOThreadObjects() MOZ_OVERRIDE
  { }

  virtual bool
  IsFileServiceUtilized() MOZ_OVERRIDE
  {
    return false;
  }

  virtual bool
  IsTransactionServiceActivated() MOZ_OVERRIDE
  {
    return false;
  }

  virtual void
  WaitForStoragesToComplete(nsTArray<nsIOfflineStorage*>& aStorages,
                            nsIRunnable* aCallback) MOZ_OVERRIDE
  {
    MOZ_ASSUME_UNREACHABLE("There are no storages");
  }

  virtual void
  AbortTransactionsForStorage(nsIOfflineStorage* aStorage) MOZ_OVERRIDE
  {
    MOZ_ASSUME_UNREACHABLE("There are no storages");
  }

  virtual bool
  HasTransactionsForStorage(nsIOfflineStorage* aStorage) MOZ_OVERRIDE
  {
    return false;
  }

  virtual void
  ShutdownTransactionService() MOZ_OVERRIDE
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
