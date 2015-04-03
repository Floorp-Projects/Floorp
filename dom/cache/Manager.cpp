/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/Manager.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/dom/cache/Context.h"
#include "mozilla/dom/cache/DBAction.h"
#include "mozilla/dom/cache/DBSchema.h"
#include "mozilla/dom/cache/FileUtils.h"
#include "mozilla/dom/cache/ManagerId.h"
#include "mozilla/dom/cache/PCacheTypes.h"
#include "mozilla/dom/cache/SavedTypes.h"
#include "mozilla/dom/cache/StreamList.h"
#include "mozilla/dom/cache/Types.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozStorageHelper.h"
#include "nsAutoPtr.h"
#include "nsIInputStream.h"
#include "nsID.h"
#include "nsIFile.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsTObserverArray.h"

namespace {

using mozilla::unused;
using mozilla::dom::cache::Action;
using mozilla::dom::cache::DBSchema;
using mozilla::dom::cache::FileUtils;
using mozilla::dom::cache::QuotaInfo;
using mozilla::dom::cache::SyncDBAction;

// An Action that is executed when a Context is first created.  It ensures that
// the directory and database are setup properly.  This lets other actions
// not worry about these details.
class SetupAction final : public SyncDBAction
{
public:
  SetupAction()
    : SyncDBAction(DBAction::Create)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    // TODO: init maintainance marker (bug 1110446)
    // TODO: perform maintainance if necessary (bug 1110446)
    // TODO: find orphaned caches in database (bug 1110446)
    // TODO: have Context create/delete marker files in constructor/destructor
    //       and only do expensive maintenance if that marker is present (bug 1110446)

    nsresult rv = FileUtils::BodyCreateDir(aDBDir);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    mozStorageTransaction trans(aConn, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    rv = DBSchema::CreateSchema(aConn);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = trans.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return rv;
  }
};

// ----------------------------------------------------------------------------

// Action that is executed when we determine that content has stopped using
// a body file that has been orphaned.
class DeleteOrphanedBodyAction final : public Action
{
public:
  explicit DeleteOrphanedBodyAction(const nsTArray<nsID>& aDeletedBodyIdList)
    : mDeletedBodyIdList(aDeletedBodyIdList)
  { }

  explicit DeleteOrphanedBodyAction(const nsID& aBodyId)
  {
    mDeletedBodyIdList.AppendElement(aBodyId);
  }

  virtual void
  RunOnTarget(Resolver* aResolver, const QuotaInfo& aQuotaInfo) override
  {
    MOZ_ASSERT(aResolver);
    MOZ_ASSERT(aQuotaInfo.mDir);

    // Note that since DeleteOrphanedBodyAction isn't used while the context is
    // being initialized, we don't need to check for cancellation here.

    nsCOMPtr<nsIFile> dbDir;
    nsresult rv = aQuotaInfo.mDir->Clone(getter_AddRefs(dbDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResolver->Resolve(rv);
      return;
    }

    rv = dbDir->Append(NS_LITERAL_STRING("cache"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResolver->Resolve(rv);
      return;
    }

    rv = FileUtils::BodyDeleteFiles(dbDir, mDeletedBodyIdList);
    unused << NS_WARN_IF(NS_FAILED(rv));

    aResolver->Resolve(rv);
  }

private:
  nsTArray<nsID> mDeletedBodyIdList;
};

} // anonymous namespace

namespace mozilla {
namespace dom {
namespace cache {

// ----------------------------------------------------------------------------

// Singleton class to track Manager instances and ensure there is only
// one for each unique ManagerId.
class Manager::Factory
{
public:
  friend class StaticAutoPtr<Manager::Factory>;

  static nsresult
  GetOrCreate(ManagerId* aManagerId, Manager** aManagerOut)
  {
    mozilla::ipc::AssertIsOnBackgroundThread();

    // Ensure there is a factory instance.  This forces the Get() call
    // below to use the same factory.
    nsresult rv = MaybeCreateInstance();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    nsRefPtr<Manager> ref = Get(aManagerId);
    if (!ref) {
      // TODO: replace this with a thread pool (bug 1119864)
      nsCOMPtr<nsIThread> ioThread;
      rv = NS_NewNamedThread("DOMCacheThread", getter_AddRefs(ioThread));
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      ref = new Manager(aManagerId, ioThread);

      MOZ_ASSERT(!sFactory->mManagerList.Contains(ref));
      sFactory->mManagerList.AppendElement(ref);
    }

    ref.forget(aManagerOut);

    return NS_OK;
  }

  static already_AddRefed<Manager>
  Get(ManagerId* aManagerId)
  {
    mozilla::ipc::AssertIsOnBackgroundThread();

    nsresult rv = MaybeCreateInstance();
    if (NS_WARN_IF(NS_FAILED(rv))) { return nullptr; }

    ManagerList::ForwardIterator iter(sFactory->mManagerList);
    while (iter.HasMore()) {
      nsRefPtr<Manager> manager = iter.GetNext();
      // If there is an invalid Manager finishing up and a new Manager
      // is created for the same origin, then the new Manager will
      // be blocked until QuotaManager finishes clearing the origin.
      if (manager->IsValid() && *manager->mManagerId == *aManagerId) {
        return manager.forget();
      }
    }

    return nullptr;
  }

  static void
  Remove(Manager* aManager)
  {
    mozilla::ipc::AssertIsOnBackgroundThread();
    MOZ_ASSERT(aManager);
    MOZ_ASSERT(sFactory);

    MOZ_ALWAYS_TRUE(sFactory->mManagerList.RemoveElement(aManager));

    // clean up the factory singleton if there are no more managers
    MaybeDestroyInstance();
  }

  static void
  StartShutdownAllOnMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Lock for sFactoryShutdown and sBackgroundThread.
    StaticMutexAutoLock lock(sMutex);

    sFactoryShutdown = true;

    if (!sBackgroundThread) {
      return;
    }

    // Guaranteed to succeed because we should be shutdown before the
    // background thread is destroyed.
    nsCOMPtr<nsIRunnable> runnable = new ShutdownAllRunnable();
    nsresult rv = sBackgroundThread->Dispatch(runnable,
                                              nsIThread::DISPATCH_NORMAL);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));
  }

  static bool
  IsShutdownAllCompleteOnMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());
    StaticMutexAutoLock lock(sMutex);
    // Infer whether we have shutdown using the sBackgroundThread value.  We
    // guarantee this is nullptr when sFactory is destroyed.
    return sFactoryShutdown && !sBackgroundThread;
  }

private:
  Factory()
    : mInSyncShutdown(false)
  {
    MOZ_COUNT_CTOR(cache::Manager::Factory);
  }

  ~Factory()
  {
    MOZ_COUNT_DTOR(cache::Manager::Factory);
    MOZ_ASSERT(mManagerList.IsEmpty());
    MOZ_ASSERT(!mInSyncShutdown);
  }

  static nsresult
  MaybeCreateInstance()
  {
    mozilla::ipc::AssertIsOnBackgroundThread();

    if (!sFactory) {
      // Be clear about what we are locking.  sFactory is bg thread only, so
      // we don't need to lock it here.  Just protect sFactoryShutdown and
      // sBackgroundThread.
      {
        StaticMutexAutoLock lock(sMutex);

        if (sFactoryShutdown) {
          return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
        }

        // Cannot use ClearOnShutdown() because we're on the background thread.
        // This is automatically cleared when Factory::Remove() calls
        // MaybeDestroyInstance().
        MOZ_ASSERT(!sBackgroundThread);
        sBackgroundThread = NS_GetCurrentThread();
      }

      // We cannot use ClearOnShutdown() here because we're not on the main
      // thread.  Instead, we delete sFactory in Factory::Remove() after the
      // last manager is removed.  ShutdownObserver ensures this happens
      // before shutdown.
      sFactory = new Factory();
    }

    // Never return sFactory to code outside Factory.  We need to delete it
    // out from under ourselves just before we return from Remove().  This
    // would be (even more) dangerous if other code had a pointer to the
    // factory itself.

    return NS_OK;
  }

  static void
  MaybeDestroyInstance()
  {
    mozilla::ipc::AssertIsOnBackgroundThread();
    MOZ_ASSERT(sFactory);

    // If the factory is is still in use then we cannot delete yet.  This
    // could be due to managers still existing or because we are in the
    // middle of shutting down.  We need to be careful not to delete ourself
    // synchronously during shutdown.
    if (!sFactory->mManagerList.IsEmpty() || sFactory->mInSyncShutdown) {
      return;
    }

    // Be clear about what we are locking.  sFactory is bg thread only, so
    // we don't need to lock it here.  Just protect sBackgroundThread.
    {
      StaticMutexAutoLock lock(sMutex);
      MOZ_ASSERT(sBackgroundThread);
      sBackgroundThread = nullptr;
    }

    sFactory = nullptr;
  }

  static void
  ShutdownAllOnBackgroundThread()
  {
    mozilla::ipc::AssertIsOnBackgroundThread();

    // The factory shutdown between when shutdown started on main thread and
    // when we could start shutdown on the worker thread.  Just declare
    // shutdown complete.  The sFactoryShutdown flag prevents the factory
    // from racing to restart here.
    if (!sFactory) {
#ifdef DEBUG
      StaticMutexAutoLock lock(sMutex);
      MOZ_ASSERT(!sBackgroundThread);
#endif
      return;
    }

    MOZ_ASSERT(!sFactory->mManagerList.IsEmpty());

    {
      // Note that we are synchronously calling shutdown code here.  If any
      // of the shutdown code synchronously decides to delete the Factory
      // we need to delay that delete until the end of this method.
      AutoRestore<bool> restore(sFactory->mInSyncShutdown);
      sFactory->mInSyncShutdown = true;

      ManagerList::ForwardIterator iter(sFactory->mManagerList);
      while (iter.HasMore()) {
        nsRefPtr<Manager> manager = iter.GetNext();
        manager->Shutdown();
      }
    }

    MaybeDestroyInstance();
  }

  class ShutdownAllRunnable final : public nsRunnable
  {
  public:
    NS_IMETHOD
    Run() override
    {
      mozilla::ipc::AssertIsOnBackgroundThread();
      ShutdownAllOnBackgroundThread();
      return NS_OK;
    }
  private:
    ~ShutdownAllRunnable() { }
  };

  // Singleton created on demand and deleted when last Manager is cleared
  // in Remove().
  // PBackground thread only.
  static StaticAutoPtr<Factory> sFactory;

  // protects following static attributes
  static StaticMutex sMutex;

  // Indicate if shutdown has occurred to block re-creation of sFactory.
  // Must hold sMutex to access.
  static bool sFactoryShutdown;

  // Background thread owning all Manager objects.  Only set while sFactory is
  // set.
  // Must hold sMutex to access.
  static StaticRefPtr<nsIThread> sBackgroundThread;

  // Weak references as we don't want to keep Manager objects alive forever.
  // When a Manager is destroyed it calls Factory::Remove() to clear itself.
  // PBackground thread only.
  typedef nsTObserverArray<Manager*> ManagerList;
  ManagerList mManagerList;

  // This flag is set when we are looping through the list and calling
  // Shutdown() on each Manager.  We need to be careful not to synchronously
  // trigger the deletion of the factory while still executing this loop.
  bool mInSyncShutdown;
};

// static
StaticAutoPtr<Manager::Factory> Manager::Factory::sFactory;

// static
StaticMutex Manager::Factory::sMutex;

// static
bool Manager::Factory::sFactoryShutdown = false;

// static
StaticRefPtr<nsIThread> Manager::Factory::sBackgroundThread;

// ----------------------------------------------------------------------------

// Abstract class to help implement the various Actions.  The vast majority
// of Actions are synchronous and need to report back to a Listener on the
// Manager.
class Manager::BaseAction : public SyncDBAction
{
protected:
  BaseAction(Manager* aManager, ListenerId aListenerId, RequestId aRequestId)
    : SyncDBAction(DBAction::Existing)
    , mManager(aManager)
    , mListenerId(aListenerId)
    , mRequestId (aRequestId)
  {
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) = 0;

  virtual void
  CompleteOnInitiatingThread(nsresult aRv) override
  {
    NS_ASSERT_OWNINGTHREAD(Manager::BaseAction);
    Listener* listener = mManager->GetListener(mListenerId);
    if (listener) {
      Complete(listener, aRv);
    }

    // ensure we release the manager on the initiating thread
    mManager = nullptr;
  }

  nsRefPtr<Manager> mManager;
  const ListenerId mListenerId;
  const RequestId mRequestId;
};

// ----------------------------------------------------------------------------

// Action that is executed when we determine that content has stopped using
// a Cache object that has been orphaned.
class Manager::DeleteOrphanedCacheAction final : public SyncDBAction
{
public:
  DeleteOrphanedCacheAction(Manager* aManager, CacheId aCacheId)
    : SyncDBAction(DBAction::Existing)
    , mManager(aManager)
    , mCacheId(aCacheId)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    mozStorageTransaction trans(aConn, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    nsresult rv = DBSchema::DeleteCache(aConn, mCacheId, mDeletedBodyIdList);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = trans.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return rv;
  }

  virtual void
  CompleteOnInitiatingThread(nsresult aRv) override
  {
    mManager->NoteOrphanedBodyIdList(mDeletedBodyIdList);

    // ensure we release the manager on the initiating thread
    mManager = nullptr;
  }

private:
  nsRefPtr<Manager> mManager;
  const CacheId mCacheId;
  nsTArray<nsID> mDeletedBodyIdList;
};

// ----------------------------------------------------------------------------

class Manager::CacheMatchAction final : public Manager::BaseAction
{
public:
  CacheMatchAction(Manager* aManager, ListenerId aListenerId,
                   RequestId aRequestId, CacheId aCacheId,
                   const PCacheRequest& aRequest,
                   const PCacheQueryParams& aParams,
                   StreamList* aStreamList)
    : BaseAction(aManager, aListenerId, aRequestId)
    , mCacheId(aCacheId)
    , mRequest(aRequest)
    , mParams(aParams)
    , mStreamList(aStreamList)
    , mFoundResponse(false)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    nsresult rv = DBSchema::CacheMatch(aConn, mCacheId, mRequest, mParams,
                                       &mFoundResponse, &mResponse);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    if (!mFoundResponse || !mResponse.mHasBodyId) {
      return rv;
    }

    nsCOMPtr<nsIInputStream> stream;
    rv = FileUtils::BodyOpen(aQuotaInfo, aDBDir, mResponse.mBodyId,
                             getter_AddRefs(stream));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    if (NS_WARN_IF(!stream)) { return NS_ERROR_FILE_NOT_FOUND; }

    mStreamList->Add(mResponse.mBodyId, stream);

    return rv;
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) override
  {
    if (!mFoundResponse) {
      aListener->OnCacheMatch(mRequestId, aRv, nullptr, nullptr);
    } else {
      mStreamList->Activate(mCacheId);
      aListener->OnCacheMatch(mRequestId, aRv, &mResponse, mStreamList);
    }
    mStreamList = nullptr;
  }

  virtual bool MatchesCacheId(CacheId aCacheId) const override
  {
    return aCacheId == mCacheId;
  }

private:
  const CacheId mCacheId;
  const PCacheRequest mRequest;
  const PCacheQueryParams mParams;
  nsRefPtr<StreamList> mStreamList;
  bool mFoundResponse;
  SavedResponse mResponse;
};

// ----------------------------------------------------------------------------

class Manager::CacheMatchAllAction final : public Manager::BaseAction
{
public:
  CacheMatchAllAction(Manager* aManager, ListenerId aListenerId,
                      RequestId aRequestId, CacheId aCacheId,
                      const PCacheRequestOrVoid& aRequestOrVoid,
                      const PCacheQueryParams& aParams,
                      StreamList* aStreamList)
    : BaseAction(aManager, aListenerId, aRequestId)
    , mCacheId(aCacheId)
    , mRequestOrVoid(aRequestOrVoid)
    , mParams(aParams)
    , mStreamList(aStreamList)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    nsresult rv = DBSchema::CacheMatchAll(aConn, mCacheId, mRequestOrVoid,
                                          mParams, mSavedResponses);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    for (uint32_t i = 0; i < mSavedResponses.Length(); ++i) {
      if (!mSavedResponses[i].mHasBodyId) {
        continue;
      }

      nsCOMPtr<nsIInputStream> stream;
      rv = FileUtils::BodyOpen(aQuotaInfo, aDBDir,
                               mSavedResponses[i].mBodyId,
                               getter_AddRefs(stream));
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
      if (NS_WARN_IF(!stream)) { return NS_ERROR_FILE_NOT_FOUND; }

      mStreamList->Add(mSavedResponses[i].mBodyId, stream);
    }

    return rv;
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) override
  {
    mStreamList->Activate(mCacheId);
    aListener->OnCacheMatchAll(mRequestId, aRv, mSavedResponses, mStreamList);
    mStreamList = nullptr;
  }

  virtual bool MatchesCacheId(CacheId aCacheId) const override
  {
    return aCacheId == mCacheId;
  }

private:
  const CacheId mCacheId;
  const PCacheRequestOrVoid mRequestOrVoid;
  const PCacheQueryParams mParams;
  nsRefPtr<StreamList> mStreamList;
  nsTArray<SavedResponse> mSavedResponses;
};

// ----------------------------------------------------------------------------

// This is the most complex Action.  It puts a request/response pair into the
// Cache.  It does not complete until all of the body data has been saved to
// disk.  This means its an asynchronous Action.
class Manager::CachePutAllAction final : public DBAction
{
public:
  CachePutAllAction(Manager* aManager, ListenerId aListenerId,
                    RequestId aRequestId, CacheId aCacheId,
                    const nsTArray<CacheRequestResponse>& aPutList,
                    const nsTArray<nsCOMPtr<nsIInputStream>>& aRequestStreamList,
                    const nsTArray<nsCOMPtr<nsIInputStream>>& aResponseStreamList)
    : DBAction(DBAction::Existing)
    , mManager(aManager)
    , mListenerId(aListenerId)
    , mRequestId(aRequestId)
    , mCacheId(aCacheId)
    , mList(aPutList.Length())
    , mExpectedAsyncCopyCompletions(1)
    , mAsyncResult(NS_OK)
    , mMutex("cache::Manager::CachePutAllAction")
  {
    MOZ_ASSERT(!aPutList.IsEmpty());
    MOZ_ASSERT(aPutList.Length() == aRequestStreamList.Length());
    MOZ_ASSERT(aPutList.Length() == aResponseStreamList.Length());

    for (uint32_t i = 0; i < aPutList.Length(); ++i) {
      Entry* entry = mList.AppendElement();
      entry->mRequest = aPutList[i].request();
      entry->mRequestStream = aRequestStreamList[i];
      entry->mResponse = aPutList[i].response();
      entry->mResponseStream = aResponseStreamList[i];
    }
  }

private:
  ~CachePutAllAction() { }

  virtual void
  RunWithDBOnTarget(Resolver* aResolver, const QuotaInfo& aQuotaInfo,
                    nsIFile* aDBDir, mozIStorageConnection* aConn) override
  {
    MOZ_ASSERT(aResolver);
    MOZ_ASSERT(aDBDir);
    MOZ_ASSERT(aConn);
    MOZ_ASSERT(!mResolver);
    MOZ_ASSERT(!mDBDir);
    MOZ_ASSERT(!mConn);

    MOZ_ASSERT(!mTargetThread);
    mTargetThread = NS_GetCurrentThread();
    MOZ_ASSERT(mTargetThread);

    // We should be pre-initialized to expect one async completion.  This is
    // the "manual" completion we call at the end of this method in all
    // cases.
    MOZ_ASSERT(mExpectedAsyncCopyCompletions == 1);

    mResolver = aResolver;
    mDBDir = aDBDir;
    mConn = aConn;

    // File bodies are streamed to disk via asynchronous copying.  Start
    // this copying now.  Each copy will eventually result in a call
    // to OnAsyncCopyComplete().
    nsresult rv = NS_OK;
    for (uint32_t i = 0; i < mList.Length(); ++i) {
      rv = StartStreamCopy(aQuotaInfo, mList[i], RequestStream,
                           &mExpectedAsyncCopyCompletions);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }

      rv = StartStreamCopy(aQuotaInfo, mList[i], ResponseStream,
                           &mExpectedAsyncCopyCompletions);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        break;
      }
    }


    // Always call OnAsyncCopyComplete() manually here.  This covers the
    // case where there is no async copying and also reports any startup
    // errors correctly.  If we hit an error, then OnAsyncCopyComplete()
    // will cancel any async copying.
    OnAsyncCopyComplete(rv);
  }

  // Called once for each asynchronous file copy whether it succeeds or
  // fails.  If a file copy is canceled, it still calls this method with
  // an error code.
  void
  OnAsyncCopyComplete(nsresult aRv)
  {
    MOZ_ASSERT(mTargetThread == NS_GetCurrentThread());
    MOZ_ASSERT(mConn);
    MOZ_ASSERT(mResolver);
    MOZ_ASSERT(mExpectedAsyncCopyCompletions > 0);

    // Explicitly check for cancellation here to catch a race condition.
    // Consider:
    //
    // 1) NS_AsyncCopy() executes on IO thread, but has not saved its
    //    copy context yet.
    // 2) CancelAllStreamCopying() occurs on PBackground thread
    // 3) Copy context from (1) is saved on IO thread.
    //
    // Checking for cancellation here catches this condition when we
    // first call OnAsyncCopyComplete() manually from RunWithDBOnTarget().
    //
    // This explicit cancellation check also handles the case where we
    // are canceled just after all stream copying completes.  We should
    // abort the synchronous DB operations in this case if we have not
    // started them yet.
    if (NS_SUCCEEDED(aRv) && IsCanceled()) {
      aRv = NS_ERROR_ABORT;
    }

    // If any of the async copies fail, we need to still wait for them all to
    // complete.  Cancel any other streams still working and remember the
    // error.  All canceled streams will call OnAsyncCopyComplete().
    if (NS_FAILED(aRv) && NS_SUCCEEDED(mAsyncResult)) {
      CancelAllStreamCopying();
      mAsyncResult = aRv;
    }

    // Check to see if async copying is still on-going.  If so, then simply
    // return for now.  We must wait for a later OnAsyncCopyComplete() call.
    mExpectedAsyncCopyCompletions -= 1;
    if (mExpectedAsyncCopyCompletions > 0) {
      return;
    }

    // We have finished with all async copying.  Indicate this by clearing all
    // our copy contexts.
    {
      MutexAutoLock lock(mMutex);
      mCopyContextList.Clear();
    }

    // An error occurred while async copying.  Terminate the Action.
    // DoResolve() will clean up any files we may have written.
    if (NS_FAILED(mAsyncResult)) {
      DoResolve(mAsyncResult);
      return;
    }

    mozStorageTransaction trans(mConn, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    nsresult rv = NS_OK;
    for (uint32_t i = 0; i < mList.Length(); ++i) {
      Entry& e = mList[i];
      if (e.mRequestStream) {
        rv = FileUtils::BodyFinalizeWrite(mDBDir, e.mRequestBodyId);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          DoResolve(rv);
          return;
        }
      }
      if (e.mResponseStream) {
        rv = FileUtils::BodyFinalizeWrite(mDBDir, e.mResponseBodyId);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          DoResolve(rv);
          return;
        }
      }

      rv = DBSchema::CachePut(mConn, mCacheId, e.mRequest,
                              e.mRequestStream ? &e.mRequestBodyId : nullptr,
                              e.mResponse,
                              e.mResponseStream ? &e.mResponseBodyId : nullptr,
                              mDeletedBodyIdList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        DoResolve(rv);
        return;
      }
    }

    rv = trans.Commit();
    unused << NS_WARN_IF(NS_FAILED(rv));

    DoResolve(rv);
  }

  virtual void
  CompleteOnInitiatingThread(nsresult aRv) override
  {
    NS_ASSERT_OWNINGTHREAD(Action);

    for (uint32_t i = 0; i < mList.Length(); ++i) {
      mList[i].mRequestStream = nullptr;
      mList[i].mResponseStream = nullptr;
    }

    mManager->NoteOrphanedBodyIdList(mDeletedBodyIdList);

    Listener* listener = mManager->GetListener(mListenerId);
    mManager = nullptr;
    if (listener) {
      listener->OnCachePutAll(mRequestId, aRv);
    }
  }

  virtual void
  CancelOnInitiatingThread() override
  {
    NS_ASSERT_OWNINGTHREAD(Action);
    Action::CancelOnInitiatingThread();
    CancelAllStreamCopying();
  }

  virtual bool MatchesCacheId(CacheId aCacheId) const override
  {
    NS_ASSERT_OWNINGTHREAD(Action);
    return aCacheId == mCacheId;
  }

  struct Entry
  {
    PCacheRequest mRequest;
    nsCOMPtr<nsIInputStream> mRequestStream;
    nsID mRequestBodyId;
    nsCOMPtr<nsISupports> mRequestCopyContext;

    PCacheResponse mResponse;
    nsCOMPtr<nsIInputStream> mResponseStream;
    nsID mResponseBodyId;
    nsCOMPtr<nsISupports> mResponseCopyContext;
  };

  enum StreamId
  {
    RequestStream,
    ResponseStream
  };

  nsresult
  StartStreamCopy(const QuotaInfo& aQuotaInfo, Entry& aEntry,
                  StreamId aStreamId, uint32_t* aCopyCountOut)
  {
    MOZ_ASSERT(mTargetThread == NS_GetCurrentThread());
    MOZ_ASSERT(aCopyCountOut);

    if (IsCanceled()) {
      return NS_ERROR_ABORT;
    }

    nsCOMPtr<nsIInputStream> source;
    nsID* bodyId;

    if (aStreamId == RequestStream) {
      source = aEntry.mRequestStream;
      bodyId = &aEntry.mRequestBodyId;
    } else {
      MOZ_ASSERT(aStreamId == ResponseStream);
      source = aEntry.mResponseStream;
      bodyId = &aEntry.mResponseBodyId;
    }

    if (!source) {
      return NS_OK;
    }

    nsCOMPtr<nsISupports> copyContext;

    nsresult rv = FileUtils::BodyStartWriteStream(aQuotaInfo, mDBDir, source,
                                                  this, AsyncCopyCompleteFunc,
                                                  bodyId,
                                                  getter_AddRefs(copyContext));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    mBodyIdWrittenList.AppendElement(*bodyId);

    if (copyContext) {
      MutexAutoLock lock(mMutex);
      mCopyContextList.AppendElement(copyContext);
    }

    *aCopyCountOut += 1;

    return rv;
  }

  void
  CancelAllStreamCopying()
  {
    // May occur on either owning thread or target thread
    MutexAutoLock lock(mMutex);
    for (uint32_t i = 0; i < mCopyContextList.Length(); ++i) {
      FileUtils::BodyCancelWrite(mDBDir, mCopyContextList[i]);
    }
    mCopyContextList.Clear();
  }

  static void
  AsyncCopyCompleteFunc(void* aClosure, nsresult aRv)
  {
    // May be on any thread, including STS event target.
    MOZ_ASSERT(aClosure);
    // Weak ref as we are guaranteed to the action is alive until
    // CompleteOnInitiatingThread is called.
    CachePutAllAction* action = static_cast<CachePutAllAction*>(aClosure);
    action->CallOnAsyncCopyCompleteOnTargetThread(aRv);
  }

  void
  CallOnAsyncCopyCompleteOnTargetThread(nsresult aRv)
  {
    // May be on any thread, including STS event target.  Non-owning runnable
    // here since we are guaranteed the Action will survive until
    // CompleteOnInitiatingThread is called.
    nsCOMPtr<nsIRunnable> runnable = NS_NewNonOwningRunnableMethodWithArgs<nsresult>(
      this, &CachePutAllAction::OnAsyncCopyComplete, aRv);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      mTargetThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL)));
  }

  void
  DoResolve(nsresult aRv)
  {
    MOZ_ASSERT(mTargetThread == NS_GetCurrentThread());

    // DoResolve() must not be called until all async copying has completed.
#ifdef DEBUG
    {
      MutexAutoLock lock(mMutex);
      MOZ_ASSERT(mCopyContextList.IsEmpty());
    }
#endif

    // Clean up any files we might have written before hitting the error.
    if (NS_FAILED(aRv)) {
      FileUtils::BodyDeleteFiles(mDBDir, mBodyIdWrittenList);
    }

    // Must be released on the target thread where it was opened.
    mConn = nullptr;

    // Drop our ref to the target thread as we are done with this thread.
    // Also makes our thread assertions catch any incorrect method calls
    // after resolve.
    mTargetThread = nullptr;

    // Make sure to de-ref the resolver per the Action API contract.
    nsRefPtr<Action::Resolver> resolver;
    mResolver.swap(resolver);
    resolver->Resolve(aRv);
  }

  // initiating thread only
  nsRefPtr<Manager> mManager;
  const ListenerId mListenerId;
  const RequestId mRequestId;

  // Set on initiating thread, read on target thread.  State machine guarantees
  // these are not modified while being read by the target thread.
  const CacheId mCacheId;
  nsTArray<Entry> mList;
  uint32_t mExpectedAsyncCopyCompletions;

  // target thread only
  nsRefPtr<Resolver> mResolver;
  nsCOMPtr<nsIFile> mDBDir;
  nsCOMPtr<mozIStorageConnection> mConn;
  nsCOMPtr<nsIThread> mTargetThread;
  nsresult mAsyncResult;
  nsTArray<nsID> mBodyIdWrittenList;

  // Written to on target thread, accessed on initiating thread after target
  // thread activity is guaranteed complete
  nsTArray<nsID> mDeletedBodyIdList;

  // accessed from any thread while mMutex locked
  Mutex mMutex;
  nsTArray<nsCOMPtr<nsISupports>> mCopyContextList;
};

// ----------------------------------------------------------------------------

class Manager::CacheDeleteAction final : public Manager::BaseAction
{
public:
  CacheDeleteAction(Manager* aManager, ListenerId aListenerId,
                    RequestId aRequestId, CacheId aCacheId,
                    const PCacheRequest& aRequest,
                    const PCacheQueryParams& aParams)
    : BaseAction(aManager, aListenerId, aRequestId)
    , mCacheId(aCacheId)
    , mRequest(aRequest)
    , mParams(aParams)
    , mSuccess(false)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    mozStorageTransaction trans(aConn, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    nsresult rv = DBSchema::CacheDelete(aConn, mCacheId, mRequest, mParams,
                                        mDeletedBodyIdList, &mSuccess);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = trans.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mSuccess = false;
      return rv;
    }

    return rv;
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) override
  {
    mManager->NoteOrphanedBodyIdList(mDeletedBodyIdList);
    aListener->OnCacheDelete(mRequestId, aRv, mSuccess);
  }

  virtual bool MatchesCacheId(CacheId aCacheId) const override
  {
    return aCacheId == mCacheId;
  }

private:
  const CacheId mCacheId;
  const PCacheRequest mRequest;
  const PCacheQueryParams mParams;
  bool mSuccess;
  nsTArray<nsID> mDeletedBodyIdList;
};

// ----------------------------------------------------------------------------

class Manager::CacheKeysAction final : public Manager::BaseAction
{
public:
  CacheKeysAction(Manager* aManager, ListenerId aListenerId,
                  RequestId aRequestId, CacheId aCacheId,
                  const PCacheRequestOrVoid& aRequestOrVoid,
                  const PCacheQueryParams& aParams,
                  StreamList* aStreamList)
    : BaseAction(aManager, aListenerId, aRequestId)
    , mCacheId(aCacheId)
    , mRequestOrVoid(aRequestOrVoid)
    , mParams(aParams)
    , mStreamList(aStreamList)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    nsresult rv = DBSchema::CacheKeys(aConn, mCacheId, mRequestOrVoid, mParams,
                                      mSavedRequests);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    for (uint32_t i = 0; i < mSavedRequests.Length(); ++i) {
      if (!mSavedRequests[i].mHasBodyId) {
        continue;
      }

      nsCOMPtr<nsIInputStream> stream;
      rv = FileUtils::BodyOpen(aQuotaInfo, aDBDir,
                               mSavedRequests[i].mBodyId,
                               getter_AddRefs(stream));
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
      if (NS_WARN_IF(!stream)) { return NS_ERROR_FILE_NOT_FOUND; }

      mStreamList->Add(mSavedRequests[i].mBodyId, stream);
    }

    return rv;
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) override
  {
    mStreamList->Activate(mCacheId);
    aListener->OnCacheKeys(mRequestId, aRv, mSavedRequests, mStreamList);
    mStreamList = nullptr;
  }

  virtual bool MatchesCacheId(CacheId aCacheId) const override
  {
    return aCacheId == mCacheId;
  }

private:
  const CacheId mCacheId;
  const PCacheRequestOrVoid mRequestOrVoid;
  const PCacheQueryParams mParams;
  nsRefPtr<StreamList> mStreamList;
  nsTArray<SavedRequest> mSavedRequests;
};

// ----------------------------------------------------------------------------

class Manager::StorageMatchAction final : public Manager::BaseAction
{
public:
  StorageMatchAction(Manager* aManager, ListenerId aListenerId,
                     RequestId aRequestId, Namespace aNamespace,
                     const PCacheRequest& aRequest,
                     const PCacheQueryParams& aParams,
                     StreamList* aStreamList)
    : BaseAction(aManager, aListenerId, aRequestId)
    , mNamespace(aNamespace)
    , mRequest(aRequest)
    , mParams(aParams)
    , mStreamList(aStreamList)
    , mFoundResponse(false)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    nsresult rv = DBSchema::StorageMatch(aConn, mNamespace, mRequest, mParams,
                                         &mFoundResponse, &mSavedResponse);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    if (!mFoundResponse || !mSavedResponse.mHasBodyId) {
      return rv;
    }

    nsCOMPtr<nsIInputStream> stream;
    rv = FileUtils::BodyOpen(aQuotaInfo, aDBDir, mSavedResponse.mBodyId,
                             getter_AddRefs(stream));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    if (NS_WARN_IF(!stream)) { return NS_ERROR_FILE_NOT_FOUND; }

    mStreamList->Add(mSavedResponse.mBodyId, stream);

    return rv;
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) override
  {
    if (!mFoundResponse) {
      aListener->OnStorageMatch(mRequestId, aRv, nullptr, nullptr);
    } else {
      mStreamList->Activate(mSavedResponse.mCacheId);
      aListener->OnStorageMatch(mRequestId, aRv, &mSavedResponse, mStreamList);
    }
    mStreamList = nullptr;
  }

private:
  const Namespace mNamespace;
  const PCacheRequest mRequest;
  const PCacheQueryParams mParams;
  nsRefPtr<StreamList> mStreamList;
  bool mFoundResponse;
  SavedResponse mSavedResponse;
};

// ----------------------------------------------------------------------------

class Manager::StorageHasAction final : public Manager::BaseAction
{
public:
  StorageHasAction(Manager* aManager, ListenerId aListenerId,
                   RequestId aRequestId, Namespace aNamespace,
                   const nsAString& aKey)
    : BaseAction(aManager, aListenerId, aRequestId)
    , mNamespace(aNamespace)
    , mKey(aKey)
    , mCacheFound(false)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    CacheId cacheId;
    return DBSchema::StorageGetCacheId(aConn, mNamespace, mKey,
                                       &mCacheFound, &cacheId);
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) override
  {
    aListener->OnStorageHas(mRequestId, aRv, mCacheFound);
  }

private:
  const Namespace mNamespace;
  const nsString mKey;
  bool mCacheFound;
};

// ----------------------------------------------------------------------------

class Manager::StorageOpenAction final : public Manager::BaseAction
{
public:
  StorageOpenAction(Manager* aManager, ListenerId aListenerId,
                    RequestId aRequestId, Namespace aNamespace,
                    const nsAString& aKey)
    : BaseAction(aManager, aListenerId, aRequestId)
    , mNamespace(aNamespace)
    , mKey(aKey)
    , mCacheId(INVALID_CACHE_ID)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    // Cache does not exist, create it instead
    mozStorageTransaction trans(aConn, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    // Look for existing cache
    bool cacheFound;
    nsresult rv = DBSchema::StorageGetCacheId(aConn, mNamespace, mKey,
                                              &cacheFound, &mCacheId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    if (cacheFound) {
      return rv;
    }

    rv = DBSchema::CreateCache(aConn, &mCacheId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = DBSchema::StoragePutCache(aConn, mNamespace, mKey, mCacheId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = trans.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return rv;
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) override
  {
    aListener->OnStorageOpen(mRequestId, aRv, mCacheId);
  }

private:
  const Namespace mNamespace;
  const nsString mKey;
  CacheId mCacheId;
};

// ----------------------------------------------------------------------------

class Manager::StorageDeleteAction final : public Manager::BaseAction
{
public:
  StorageDeleteAction(Manager* aManager, ListenerId aListenerId,
                      RequestId aRequestId, Namespace aNamespace,
                      const nsAString& aKey)
    : BaseAction(aManager, aListenerId, aRequestId)
    , mNamespace(aNamespace)
    , mKey(aKey)
    , mCacheDeleted(false)
    , mCacheId(INVALID_CACHE_ID)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    mozStorageTransaction trans(aConn, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    bool exists;
    nsresult rv = DBSchema::StorageGetCacheId(aConn, mNamespace, mKey, &exists,
                                              &mCacheId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    if (!exists) {
      mCacheDeleted = false;
      return NS_OK;
    }

    rv = DBSchema::StorageForgetCache(aConn, mNamespace, mKey);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = trans.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    mCacheDeleted = true;
    return rv;
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) override
  {
    if (mCacheDeleted) {
      // If content is referencing this cache, mark it orphaned to be
      // deleted later.
      if (!mManager->SetCacheIdOrphanedIfRefed(mCacheId)) {

        // no outstanding references, delete immediately
        nsRefPtr<Context> context = mManager->CurrentContext();
        context->CancelForCacheId(mCacheId);
        nsRefPtr<Action> action =
          new DeleteOrphanedCacheAction(mManager, mCacheId);
        context->Dispatch(mManager->mIOThread, action);
      }
    }

    aListener->OnStorageDelete(mRequestId, aRv, mCacheDeleted);
  }

private:
  const Namespace mNamespace;
  const nsString mKey;
  bool mCacheDeleted;
  CacheId mCacheId;
};

// ----------------------------------------------------------------------------

class Manager::StorageKeysAction final : public Manager::BaseAction
{
public:
  StorageKeysAction(Manager* aManager, ListenerId aListenerId,
                    RequestId aRequestId, Namespace aNamespace)
    : BaseAction(aManager, aListenerId, aRequestId)
    , mNamespace(aNamespace)
  { }

  virtual nsresult
  RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                        mozIStorageConnection* aConn) override
  {
    return DBSchema::StorageGetKeys(aConn, mNamespace, mKeys);
  }

  virtual void
  Complete(Listener* aListener, nsresult aRv) override
  {
    if (NS_FAILED(aRv)) {
      mKeys.Clear();
    }
    aListener->OnStorageKeys(mRequestId, aRv, mKeys);
  }

private:
  const Namespace mNamespace;
  nsTArray<nsString> mKeys;
};

// ----------------------------------------------------------------------------

//static
Manager::ListenerId Manager::sNextListenerId = 0;

// static
nsresult
Manager::GetOrCreate(ManagerId* aManagerId, Manager** aManagerOut)
{
  mozilla::ipc::AssertIsOnBackgroundThread();
  return Factory::GetOrCreate(aManagerId, aManagerOut);
}

// static
already_AddRefed<Manager>
Manager::Get(ManagerId* aManagerId)
{
  mozilla::ipc::AssertIsOnBackgroundThread();
  return Factory::Get(aManagerId);
}

// static
void
Manager::ShutdownAllOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  Factory::StartShutdownAllOnMainThread();

  while (!Factory::IsShutdownAllCompleteOnMainThread()) {
    if (!NS_ProcessNextEvent()) {
      NS_WARNING("Something bad happened!");
      break;
    }
  }
}

void
Manager::RemoveListener(Listener* aListener)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  // There may not be a listener here in the case where an actor is killed
  // before it can perform any actual async requests on Manager.
  mListeners.RemoveElement(aListener, ListenerEntryListenerComparator());
  MOZ_ASSERT(!mListeners.Contains(aListener,
                                  ListenerEntryListenerComparator()));
  MaybeAllowContextToClose();
}

void
Manager::RemoveContext(Context* aContext)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(mContext);
  MOZ_ASSERT(mContext == aContext);

  // Whether the Context destruction was triggered from the Manager going
  // idle or the underlying storage being invalidated, we should be invalid
  // when the context is destroyed.
  MOZ_ASSERT(!mValid);

  mContext = nullptr;

  // If we're trying to shutdown, then note that we're done.  This is the
  // delayed case from Manager::Shutdown().
  if (mShuttingDown) {
    Factory::Remove(this);
  }
}

void
Manager::Invalidate()
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  // QuotaManager can trigger this more than once.
  mValid = false;
}

bool
Manager::IsValid() const
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  return mValid;
}

void
Manager::AddRefCacheId(CacheId aCacheId)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  for (uint32_t i = 0; i < mCacheIdRefs.Length(); ++i) {
    if (mCacheIdRefs[i].mCacheId == aCacheId) {
      mCacheIdRefs[i].mCount += 1;
      return;
    }
  }
  CacheIdRefCounter* entry = mCacheIdRefs.AppendElement();
  entry->mCacheId = aCacheId;
  entry->mCount = 1;
  entry->mOrphaned = false;
}

void
Manager::ReleaseCacheId(CacheId aCacheId)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  for (uint32_t i = 0; i < mCacheIdRefs.Length(); ++i) {
    if (mCacheIdRefs[i].mCacheId == aCacheId) {
      DebugOnly<uint32_t> oldRef = mCacheIdRefs[i].mCount;
      mCacheIdRefs[i].mCount -= 1;
      MOZ_ASSERT(mCacheIdRefs[i].mCount < oldRef);
      if (mCacheIdRefs[i].mCount == 0) {
        bool orphaned = mCacheIdRefs[i].mOrphaned;
        mCacheIdRefs.RemoveElementAt(i);
        // TODO: note that we need to check this cache for staleness on startup (bug 1110446)
        if (orphaned && !mShuttingDown && mValid) {
          nsRefPtr<Context> context = CurrentContext();
          context->CancelForCacheId(aCacheId);
          nsRefPtr<Action> action = new DeleteOrphanedCacheAction(this,
                                                                  aCacheId);
          context->Dispatch(mIOThread, action);
        }
      }
      MaybeAllowContextToClose();
      return;
    }
  }
  MOZ_ASSERT_UNREACHABLE("Attempt to release CacheId that is not referenced!");
}

void
Manager::AddRefBodyId(const nsID& aBodyId)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  for (uint32_t i = 0; i < mBodyIdRefs.Length(); ++i) {
    if (mBodyIdRefs[i].mBodyId == aBodyId) {
      mBodyIdRefs[i].mCount += 1;
      return;
    }
  }
  BodyIdRefCounter* entry = mBodyIdRefs.AppendElement();
  entry->mBodyId = aBodyId;
  entry->mCount = 1;
  entry->mOrphaned = false;
}

void
Manager::ReleaseBodyId(const nsID& aBodyId)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  for (uint32_t i = 0; i < mBodyIdRefs.Length(); ++i) {
    if (mBodyIdRefs[i].mBodyId == aBodyId) {
      DebugOnly<uint32_t> oldRef = mBodyIdRefs[i].mCount;
      mBodyIdRefs[i].mCount -= 1;
      MOZ_ASSERT(mBodyIdRefs[i].mCount < oldRef);
      if (mBodyIdRefs[i].mCount < 1) {
        bool orphaned = mBodyIdRefs[i].mOrphaned;
        mBodyIdRefs.RemoveElementAt(i);
        // TODO: note that we need to check this body for staleness on startup (bug 1110446)
        if (orphaned && !mShuttingDown && mValid) {
          nsRefPtr<Action> action = new DeleteOrphanedBodyAction(aBodyId);
          nsRefPtr<Context> context = CurrentContext();
          context->Dispatch(mIOThread, action);
        }
      }
      MaybeAllowContextToClose();
      return;
    }
  }
  MOZ_ASSERT_UNREACHABLE("Attempt to release BodyId that is not referenced!");
}

already_AddRefed<ManagerId>
Manager::GetManagerId() const
{
  nsRefPtr<ManagerId> ref = mManagerId;
  return ref.forget();
}

void
Manager::AddStreamList(StreamList* aStreamList)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aStreamList);
  mStreamLists.AppendElement(aStreamList);
}

void
Manager::RemoveStreamList(StreamList* aStreamList)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aStreamList);
  mStreamLists.RemoveElement(aStreamList);
}

void
Manager::CacheMatch(Listener* aListener, RequestId aRequestId, CacheId aCacheId,
                    const PCacheRequest& aRequest,
                    const PCacheQueryParams& aParams)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnCacheMatch(aRequestId, NS_ERROR_FAILURE, nullptr, nullptr);
    return;
  }
  nsRefPtr<Context> context = CurrentContext();
  nsRefPtr<StreamList> streamList = new StreamList(this, context);
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new CacheMatchAction(this, listenerId, aRequestId,
                                                 aCacheId, aRequest, aParams,
                                                 streamList);
  context->Dispatch(mIOThread, action);
}

void
Manager::CacheMatchAll(Listener* aListener, RequestId aRequestId,
                       CacheId aCacheId, const PCacheRequestOrVoid& aRequest,
                       const PCacheQueryParams& aParams)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnCacheMatchAll(aRequestId, NS_ERROR_FAILURE,
                               nsTArray<SavedResponse>(), nullptr);
    return;
  }
  nsRefPtr<Context> context = CurrentContext();
  nsRefPtr<StreamList> streamList = new StreamList(this, context);
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new CacheMatchAllAction(this, listenerId, aRequestId,
                                                    aCacheId, aRequest, aParams,
                                                    streamList);
  context->Dispatch(mIOThread, action);
}

void
Manager::CachePutAll(Listener* aListener, RequestId aRequestId, CacheId aCacheId,
                     const nsTArray<CacheRequestResponse>& aPutList,
                     const nsTArray<nsCOMPtr<nsIInputStream>>& aRequestStreamList,
                     const nsTArray<nsCOMPtr<nsIInputStream>>& aResponseStreamList)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnCachePutAll(aRequestId, NS_ERROR_FAILURE);
    return;
  }
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new CachePutAllAction(this, listenerId, aRequestId,
                                                  aCacheId, aPutList,
                                                  aRequestStreamList,
                                                  aResponseStreamList);
  nsRefPtr<Context> context = CurrentContext();
  context->Dispatch(mIOThread, action);
}

void
Manager::CacheDelete(Listener* aListener, RequestId aRequestId,
                     CacheId aCacheId, const PCacheRequest& aRequest,
                     const PCacheQueryParams& aParams)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnCacheDelete(aRequestId, NS_ERROR_FAILURE, false);
    return;
  }
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new CacheDeleteAction(this, listenerId, aRequestId,
                                                  aCacheId, aRequest, aParams);
  nsRefPtr<Context> context = CurrentContext();
  context->Dispatch(mIOThread, action);
}

void
Manager::CacheKeys(Listener* aListener, RequestId aRequestId,
                   CacheId aCacheId, const PCacheRequestOrVoid& aRequestOrVoid,
                   const PCacheQueryParams& aParams)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnCacheKeys(aRequestId, NS_ERROR_FAILURE,
                           nsTArray<SavedRequest>(), nullptr);
    return;
  }
  nsRefPtr<Context> context = CurrentContext();
  nsRefPtr<StreamList> streamList = new StreamList(this, context);
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new CacheKeysAction(this, listenerId, aRequestId,
                                                aCacheId, aRequestOrVoid,
                                                aParams, streamList);
  context->Dispatch(mIOThread, action);
}

void
Manager::StorageMatch(Listener* aListener, RequestId aRequestId,
                      Namespace aNamespace, const PCacheRequest& aRequest,
                      const PCacheQueryParams& aParams)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnStorageMatch(aRequestId, NS_ERROR_FAILURE,
                              nullptr, nullptr);
    return;
  }
  nsRefPtr<Context> context = CurrentContext();
  nsRefPtr<StreamList> streamList = new StreamList(this, context);
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new StorageMatchAction(this, listenerId, aRequestId,
                                                   aNamespace, aRequest,
                                                   aParams, streamList);
  context->Dispatch(mIOThread, action);
}

void
Manager::StorageHas(Listener* aListener, RequestId aRequestId,
                    Namespace aNamespace, const nsAString& aKey)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnStorageHas(aRequestId, NS_ERROR_FAILURE,
                            false);
    return;
  }
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new StorageHasAction(this, listenerId, aRequestId,
                                                 aNamespace, aKey);
  nsRefPtr<Context> context = CurrentContext();
  context->Dispatch(mIOThread, action);
}

void
Manager::StorageOpen(Listener* aListener, RequestId aRequestId,
                     Namespace aNamespace, const nsAString& aKey)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnStorageOpen(aRequestId, NS_ERROR_FAILURE, 0);
    return;
  }
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new StorageOpenAction(this, listenerId, aRequestId,
                                                  aNamespace, aKey);
  nsRefPtr<Context> context = CurrentContext();
  context->Dispatch(mIOThread, action);
}

void
Manager::StorageDelete(Listener* aListener, RequestId aRequestId,
                       Namespace aNamespace, const nsAString& aKey)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnStorageDelete(aRequestId, NS_ERROR_FAILURE,
                               false);
    return;
  }
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new StorageDeleteAction(this, listenerId, aRequestId,
                                                    aNamespace, aKey);
  nsRefPtr<Context> context = CurrentContext();
  context->Dispatch(mIOThread, action);
}

void
Manager::StorageKeys(Listener* aListener, RequestId aRequestId,
                     Namespace aNamespace)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(aListener);
  if (mShuttingDown || !mValid) {
    aListener->OnStorageKeys(aRequestId, NS_ERROR_FAILURE,
                             nsTArray<nsString>());
    return;
  }
  ListenerId listenerId = SaveListener(aListener);
  nsRefPtr<Action> action = new StorageKeysAction(this, listenerId, aRequestId,
                                                  aNamespace);
  nsRefPtr<Context> context = CurrentContext();
  context->Dispatch(mIOThread, action);
}

Manager::Manager(ManagerId* aManagerId, nsIThread* aIOThread)
  : mManagerId(aManagerId)
  , mIOThread(aIOThread)
  , mContext(nullptr)
  , mShuttingDown(false)
  , mValid(true)
{
  MOZ_ASSERT(mManagerId);
  MOZ_ASSERT(mIOThread);
}

Manager::~Manager()
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  MOZ_ASSERT(!mContext);
  Shutdown();

  nsCOMPtr<nsIThread> ioThread;
  mIOThread.swap(ioThread);

  // Don't spin the event loop in the destructor waiting for the thread to
  // shutdown.  Defer this to the main thread, instead.
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(ioThread, &nsIThread::Shutdown);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
}

void
Manager::Shutdown()
{
  NS_ASSERT_OWNINGTHREAD(Manager);

  // Ignore duplicate attempts to shutdown.  This can occur when we start
  // a browser initiated shutdown and then run ~Manager() which also
  // calls Shutdown().
  if (mShuttingDown) {
    return;
  }

  // Set a flag to prevent any new requests from coming in and creating
  // a new Context.  We must ensure all Contexts and IO operations are
  // complete before shutdown proceeds.
  mShuttingDown = true;

  // If there is a context, then we must wait for it to complete.  Cancel and
  // only note that we are done after its cleaned up.
  if (mContext) {
    nsRefPtr<Context> context = mContext;
    context->CancelAll();
    return;
  }

  // Otherwise, note that we are complete immediately
  Factory::Remove(this);
}

already_AddRefed<Context>
Manager::CurrentContext()
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  nsRefPtr<Context> ref = mContext;
  if (!ref) {
    MOZ_ASSERT(!mShuttingDown);
    MOZ_ASSERT(mValid);
    nsRefPtr<Action> setupAction = new SetupAction();
    ref = Context::Create(this, setupAction);
    mContext = ref;
  }
  return ref.forget();
}

Manager::ListenerId
Manager::SaveListener(Listener* aListener)
{
  NS_ASSERT_OWNINGTHREAD(Manager);

  // Once a Listener is added, we keep a reference to it until its
  // removed.  Since the same Listener might make multiple requests,
  // ensure we only have a single reference in our list.
  ListenerList::index_type index =
    mListeners.IndexOf(aListener, 0, ListenerEntryListenerComparator());
  if (index != ListenerList::NoIndex) {
    return mListeners[index].mId;
  }

  ListenerId id = sNextListenerId;
  sNextListenerId += 1;

  mListeners.AppendElement(ListenerEntry(id, aListener));
  return id;
}

Manager::Listener*
Manager::GetListener(ListenerId aListenerId) const
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  ListenerList::index_type index =
    mListeners.IndexOf(aListenerId, 0, ListenerEntryIdComparator());
  if (index != ListenerList::NoIndex) {
    return mListeners[index].mListener;
  }

  // This can legitimately happen if the actor is deleted while a request is
  // in process.  For example, the child process OOMs.
  return nullptr;
}

bool
Manager::SetCacheIdOrphanedIfRefed(CacheId aCacheId)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  for (uint32_t i = 0; i < mCacheIdRefs.Length(); ++i) {
    if (mCacheIdRefs[i].mCacheId == aCacheId) {
      MOZ_ASSERT(mCacheIdRefs[i].mCount > 0);
      MOZ_ASSERT(!mCacheIdRefs[i].mOrphaned);
      mCacheIdRefs[i].mOrphaned = true;
      return true;
    }
  }
  return false;
}

// TODO: provide way to set body non-orphaned if its added back to a cache (bug 1110479)

bool
Manager::SetBodyIdOrphanedIfRefed(const nsID& aBodyId)
{
  NS_ASSERT_OWNINGTHREAD(Manager);
  for (uint32_t i = 0; i < mBodyIdRefs.Length(); ++i) {
    if (mBodyIdRefs[i].mBodyId == aBodyId) {
      MOZ_ASSERT(mBodyIdRefs[i].mCount > 0);
      MOZ_ASSERT(!mBodyIdRefs[i].mOrphaned);
      mBodyIdRefs[i].mOrphaned = true;
      return true;
    }
  }
  return false;
}

void
Manager::NoteOrphanedBodyIdList(const nsTArray<nsID>& aDeletedBodyIdList)
{
  NS_ASSERT_OWNINGTHREAD(Manager);

  nsAutoTArray<nsID, 64> deleteNowList;
  deleteNowList.SetCapacity(aDeletedBodyIdList.Length());

  for (uint32_t i = 0; i < aDeletedBodyIdList.Length(); ++i) {
    if (!SetBodyIdOrphanedIfRefed(aDeletedBodyIdList[i])) {
      deleteNowList.AppendElement(aDeletedBodyIdList[i]);
    }
  }

  if (!deleteNowList.IsEmpty()) {
    nsRefPtr<Action> action = new DeleteOrphanedBodyAction(deleteNowList);
    nsRefPtr<Context> context = CurrentContext();
    context->Dispatch(mIOThread, action);
  }
}

void
Manager::MaybeAllowContextToClose()
{
  NS_ASSERT_OWNINGTHREAD(Manager);

  // If we have an active context, but we have no more users of the Manager,
  // then let it shut itself down.  We must wait for all possible users of
  // Cache state information to complete before doing this.  Once we allow
  // the Context to close we may not reliably get notified of storage
  // invalidation.
  if (mContext && mListeners.IsEmpty()
               && mCacheIdRefs.IsEmpty()
               && mBodyIdRefs.IsEmpty()) {

    // Mark this Manager as invalid so that it won't get used again.  We don't
    // want to start any new operations once we allow the Context to close since
    // it may race with the underlying storage getting invalidated.
    mValid = false;

    mContext->AllowToClose();
  }
}

} // namespace cache
} // namespace dom
} // namespace mozilla
