/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include "LocalStorageCommon.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/PBackgroundLSDatabaseParent.h"
#include "mozilla/dom/PBackgroundLSRequestParent.h"
#include "mozilla/dom/PBackgroundLSSharedTypes.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

namespace mozilla {
namespace dom {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;

namespace {

/*******************************************************************************
 * Non-actor class declarations
 ******************************************************************************/

class DatastoreOperationBase
  : public Runnable
{
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  nsresult mResultCode;
  Atomic<bool> mMayProceedOnNonOwningThread;
  bool mMayProceed;

public:
  nsIEventTarget*
  OwningEventTarget() const
  {
    MOZ_ASSERT(mOwningEventTarget);

    return mOwningEventTarget;
  }

  bool
  IsOnOwningThread() const
  {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return
      NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) && current;
  }

  void
  AssertIsOnOwningThread() const
  {
    MOZ_ASSERT(IsOnBackgroundThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  nsresult
  ResultCode() const
  {
    return mResultCode;
  }

  void
  MaybeSetFailureCode(nsresult aErrorCode)
  {
    MOZ_ASSERT(NS_FAILED(aErrorCode));

    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = aErrorCode;
    }
  }

  void
  NoteComplete()
  {
    AssertIsOnOwningThread();

    mMayProceed = false;
    mMayProceedOnNonOwningThread = false;
  }

  bool
  MayProceed() const
  {
    AssertIsOnOwningThread();

    return mMayProceed;
  }

  // May be called on any thread, but you should call MayProceed() if you know
  // you're on the background thread because it is slightly faster.
  bool
  MayProceedOnNonOwningThread() const
  {
    return mMayProceedOnNonOwningThread;
  }

protected:
  DatastoreOperationBase()
    : Runnable("dom::DatastoreOperationBase")
    , mOwningEventTarget(GetCurrentThreadEventTarget())
    , mResultCode(NS_OK)
    , mMayProceedOnNonOwningThread(true)
    , mMayProceed(true)
  { }

  ~DatastoreOperationBase() override
  {
    MOZ_ASSERT(!mMayProceed);
  }
};

class Datastore final
{
  nsDataHashtable<nsStringHashKey, nsString> mValues;
  const nsCString mOrigin;

public:
  // Created by PrepareDatastoreOp.
  explicit Datastore(const nsACString& aOrigin);

  const nsCString&
  Origin() const
  {
    return mOrigin;
  }

  uint32_t
  GetLength() const;

  void
  GetKey(uint32_t aIndex, nsString& aKey) const;

  void
  GetItem(const nsString& aKey, nsString& aValue) const;

  void
  SetItem(const nsString& aKey, const nsString& aValue);

  void
  RemoveItem(const nsString& aKey);

  void
  Clear();

  void
  GetKeys(nsTArray<nsString>& aKeys) const;

  NS_INLINE_DECL_REFCOUNTING(Datastore)

private:
  // Reference counted.
  ~Datastore();
};

class PreparedDatastore
{
  RefPtr<Datastore> mDatastore;

public:
  explicit PreparedDatastore(Datastore* aDatastore)
    : mDatastore(aDatastore)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aDatastore);
  }

  already_AddRefed<Datastore>
  ForgetDatastore()
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatastore);

    return mDatastore.forget();
  }
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class Database final
  : public PBackgroundLSDatabaseParent
{
  RefPtr<Datastore> mDatastore;

  bool mAllowedToClose;
  bool mActorDestroyed;
  bool mRequestedAllowToClose;
#ifdef DEBUG
  bool mActorWasAlive;
#endif

public:
  // Created in AllocPBackgroundLSDatabaseParent.
  Database();

  void
  SetActorAlive(already_AddRefed<Datastore>&& aDatastore);

  void
  RequestAllowToClose();

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Database)

private:
  // Reference counted.
  ~Database();

  void
  AllowToClose();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvDeleteMe() override;

  mozilla::ipc::IPCResult
  RecvAllowToClose() override;

  mozilla::ipc::IPCResult
  RecvGetLength(uint32_t* aLength) override;

  mozilla::ipc::IPCResult
  RecvGetKey(const uint32_t& aIndex, nsString* aKey) override;

  mozilla::ipc::IPCResult
  RecvGetItem(const nsString& aKey, nsString* aValue) override;

  mozilla::ipc::IPCResult
  RecvGetKeys(nsTArray<nsString>* aKeys) override;

  mozilla::ipc::IPCResult
  RecvSetItem(const nsString& aKey, const nsString& aValue) override;

  mozilla::ipc::IPCResult
  RecvRemoveItem(const nsString& aKey) override;

  mozilla::ipc::IPCResult
  RecvClear() override;
};

class LSRequestBase
  : public DatastoreOperationBase
  , public PBackgroundLSRequestParent
{
public:
  virtual void
  Dispatch() = 0;

private:
  // IPDL methods.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvCancel() override;
};

class PrepareDatastoreOp
  : public LSRequestBase
{
  enum class State
  {
    // Just created on the PBackground thread. Next step is OpeningOnMainThread
    // or OpeningOnOwningThread.
    Initial,

    // Waiting to open/opening on the main thread. Next step is
    // SendingReadyMessage.
    OpeningOnMainThread,

    // Waiting to open/opening on the owning thread. Next step is
    // SendingReadyMessage.
    OpeningOnOwningThread,

    // Waiting to send/sending the ready message on the PBackground thread. Next
    // step is WaitingForFinish.
    SendingReadyMessage,

    // Waiting for the finish message on the PBackground thread. Next step is
    // SendingResults.
    WaitingForFinish,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  const LSRequestPrepareDatastoreParams mParams;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mOrigin;
  State mState;

public:
  explicit PrepareDatastoreOp(const LSRequestParams& aParams);

  void
  Dispatch() override;

private:
  ~PrepareDatastoreOp() override;

  nsresult
  OpenOnMainThread();

  nsresult
  OpenOnOwningThread();

  void
  SendReadyMessage();

  void
  SendResults();

  void
  Cleanup();

  NS_IMETHOD
  Run() override;

  // IPDL overrides.
  mozilla::ipc::IPCResult
  RecvFinish() override;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

class QuotaClient final
  : public mozilla::dom::quota::Client
{
  static QuotaClient* sInstance;

  bool mShutdownRequested;

public:
  QuotaClient();

  static bool
  IsShuttingDownOnBackgroundThread()
  {
    AssertIsOnBackgroundThread();

    if (sInstance) {
      return sInstance->IsShuttingDown();
    }

    return QuotaManager::IsShuttingDown();
  }

  static bool
  IsShuttingDownOnNonBackgroundThread()
  {
    MOZ_ASSERT(!IsOnBackgroundThread());

    return QuotaManager::IsShuttingDown();
  }

  bool
  IsShuttingDown() const
  {
    AssertIsOnBackgroundThread();

    return mShutdownRequested;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::QuotaClient, override)

  Type
  GetType() override;

  nsresult
  InitOrigin(PersistenceType aPersistenceType,
             const nsACString& aGroup,
             const nsACString& aOrigin,
             const AtomicBool& aCanceled,
             UsageInfo* aUsageInfo) override;

  nsresult
  GetUsageForOrigin(PersistenceType aPersistenceType,
                    const nsACString& aGroup,
                    const nsACString& aOrigin,
                    const AtomicBool& aCanceled,
                    UsageInfo* aUsageInfo) override;

  void
  OnOriginClearCompleted(PersistenceType aPersistenceType,
                         const nsACString& aOrigin)
                         override;

  void
  ReleaseIOThreadObjects() override;

  void
  AbortOperations(const nsACString& aOrigin) override;

  void
  AbortOperationsForProcess(ContentParentId aContentParentId) override;

  void
  StartIdleMaintenance() override;

  void
  StopIdleMaintenance() override;

  void
  ShutdownWorkThreads() override;

private:
  ~QuotaClient() override;
};

/*******************************************************************************
 * Globals
 ******************************************************************************/

typedef nsTArray<PrepareDatastoreOp*> PrepareDatastoreOpArray;

StaticAutoPtr<PrepareDatastoreOpArray> gPrepareDatastoreOps;

typedef nsDataHashtable<nsCStringHashKey, Datastore*> DatastoreHashtable;

StaticAutoPtr<DatastoreHashtable> gDatastores;

uint64_t gLastDatastoreId = 0;

typedef nsClassHashtable<nsUint64HashKey, PreparedDatastore>
  PreparedDatastoreHashtable;

StaticAutoPtr<PreparedDatastoreHashtable> gPreparedDatastores;

typedef nsTArray<Database*> LiveDatabaseArray;

StaticAutoPtr<LiveDatabaseArray> gLiveDatabases;

} // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

PBackgroundLSDatabaseParent*
AllocPBackgroundLSDatabaseParent(const uint64_t& aDatastoreId)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  if (NS_WARN_IF(!gPreparedDatastores)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  PreparedDatastore* preparedDatastore = gPreparedDatastores->Get(aDatastoreId);
  if (NS_WARN_IF(!preparedDatastore)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  // If we ever decide to return null from this point on, we need to make sure
  // that the prepared datastore is removed from the gPreparedDatastores
  // hashtable.
  // We also assume that IPDL must call RecvPBackgroundLSDatabaseConstructor
  // once we return a valid actor in this method.

  RefPtr<Database> database = new Database();

  // Transfer ownership to IPDL.
  return database.forget().take();
}

bool
RecvPBackgroundLSDatabaseConstructor(PBackgroundLSDatabaseParent* aActor,
                                     const uint64_t& aDatastoreId)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(gPreparedDatastores);
  MOZ_ASSERT(gPreparedDatastores->Get(aDatastoreId));
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built (it has a manager, channel and it's
  // registered as a subprotocol).
  // ActorDestroy will be called if we fail here.

  nsAutoPtr<PreparedDatastore> preparedDatastore;
  gPreparedDatastores->Remove(aDatastoreId, &preparedDatastore);
  MOZ_ASSERT(preparedDatastore);

  auto* database = static_cast<Database*>(aActor);

  database->SetActorAlive(preparedDatastore->ForgetDatastore());

  return true;
}

bool
DeallocPBackgroundLSDatabaseParent(PBackgroundLSDatabaseParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Database> actor = dont_AddRef(static_cast<Database*>(aActor));

  return true;
}

PBackgroundLSRequestParent*
AllocPBackgroundLSRequestParent(PBackgroundParent* aBackgroundActor,
                                const LSRequestParams& aParams)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  RefPtr<LSRequestBase> actor;

  switch (aParams.type()) {
    case LSRequestParams::TLSRequestPrepareDatastoreParams: {
      bool isOtherProcess =
        BackgroundParent::IsOtherProcessActor(aBackgroundActor);

      const LSRequestPrepareDatastoreParams& params =
        aParams.get_LSRequestPrepareDatastoreParams();

      const PrincipalOrQuotaInfo& info = params.info();

      PrincipalOrQuotaInfo::Type infoType = info.type();

      bool paramsOk =
        (isOtherProcess && infoType == PrincipalOrQuotaInfo::TPrincipalInfo) ||
        (!isOtherProcess && infoType == PrincipalOrQuotaInfo::TQuotaInfo);

      if (NS_WARN_IF(!paramsOk)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      RefPtr<PrepareDatastoreOp> prepareDatastoreOp =
        new PrepareDatastoreOp(aParams);

      if (!gPrepareDatastoreOps) {
        gPrepareDatastoreOps = new PrepareDatastoreOpArray();
      }
      gPrepareDatastoreOps->AppendElement(prepareDatastoreOp);

      actor = std::move(prepareDatastoreOp);

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool
RecvPBackgroundLSRequestConstructor(PBackgroundLSRequestParent* aActor,
                                    const LSRequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != LSRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built.

  auto* op = static_cast<LSRequestBase*>(aActor);

  op->Dispatch();

  return true;
}

bool
DeallocPBackgroundLSRequestParent(PBackgroundLSRequestParent* aActor)
{
  AssertIsOnBackgroundThread();

  // Transfer ownership back from IPDL.
  RefPtr<LSRequestBase> actor =
    dont_AddRef(static_cast<LSRequestBase*>(aActor));

  return true;
}

namespace localstorage {

already_AddRefed<mozilla::dom::quota::Client>
CreateQuotaClient()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  RefPtr<QuotaClient> client = new QuotaClient();
  return client.forget();
}

} // namespace localstorage

/*******************************************************************************
 * DatastoreOperationBase
 ******************************************************************************/

/*******************************************************************************
 * Datastore
 ******************************************************************************/

Datastore::Datastore(const nsACString& aOrigin)
  : mOrigin(aOrigin)
{
  AssertIsOnBackgroundThread();
}

Datastore::~Datastore()
{
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(gDatastores);
  MOZ_ASSERT(gDatastores->Get(mOrigin));
  gDatastores->Remove(mOrigin);

  if (!gDatastores->Count()) {
    gDatastores = nullptr;
  }
}

uint32_t
Datastore::GetLength() const
{
  AssertIsOnBackgroundThread();

  return mValues.Count();
}

void
Datastore::GetKey(uint32_t aIndex, nsString& aKey) const
{
  AssertIsOnBackgroundThread();

  aKey.SetIsVoid(true);
  for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
    if (aIndex == 0) {
      aKey = iter.Key();
      return;
    }
    aIndex--;
  }
}

void
Datastore::GetItem(const nsString& aKey, nsString& aValue) const
{
  AssertIsOnBackgroundThread();

  if (!mValues.Get(aKey, &aValue)) {
    aValue.SetIsVoid(true);
  }
}

void
Datastore::SetItem(const nsString& aKey, const nsString& aValue)
{
  AssertIsOnBackgroundThread();

  mValues.Put(aKey, aValue);
}

void
Datastore::RemoveItem(const nsString& aKey)
{
  AssertIsOnBackgroundThread();

  mValues.Remove(aKey);
}

void
Datastore::Clear()
{
  AssertIsOnBackgroundThread();

  mValues.Clear();
}

void
Datastore::GetKeys(nsTArray<nsString>& aKeys) const
{
  AssertIsOnBackgroundThread();

  for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }
}

/*******************************************************************************
 * Database
 ******************************************************************************/

Database::Database()
  : mAllowedToClose(false)
  , mActorDestroyed(false)
  , mRequestedAllowToClose(false)
#ifdef DEBUG
  , mActorWasAlive(false)
#endif
{
  AssertIsOnBackgroundThread();
}

Database::~Database()
{
  MOZ_ASSERT_IF(mActorWasAlive, mAllowedToClose);
  MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed);
}

void
Database::SetActorAlive(already_AddRefed<Datastore>&& aDatastore)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
  mActorWasAlive = true;
#endif

  mDatastore = std::move(aDatastore);

  if (!gLiveDatabases) {
    gLiveDatabases = new LiveDatabaseArray();
  }

  gLiveDatabases->AppendElement(this);
}

void
Database::RequestAllowToClose()
{
  AssertIsOnBackgroundThread();

  if (mRequestedAllowToClose) {
    return;
  }

  mRequestedAllowToClose = true;

  // Send the RequestAllowToClose message to the child to avoid racing with the
  // child actor. Except the case when the actor was already destroyed.
  if (mActorDestroyed) {
    MOZ_ASSERT(mAllowedToClose);
  } else {
    Unused << SendRequestAllowToClose();
  }
}

void
Database::AllowToClose()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mAllowedToClose);
  MOZ_ASSERT(mDatastore);

  mAllowedToClose = true;

  mDatastore = nullptr;

  MOZ_ASSERT(gLiveDatabases);
  gLiveDatabases->RemoveElement(this);

  if (gLiveDatabases->IsEmpty()) {
    gLiveDatabases = nullptr;
  }
}

void
Database::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!mAllowedToClose) {
    AllowToClose();
  }
}

mozilla::ipc::IPCResult
Database::RecvDeleteMe()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLSDatabaseParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvAllowToClose()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  AllowToClose();

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvGetLength(uint32_t* aLength)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aLength);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  *aLength = mDatastore->GetLength();

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvGetKey(const uint32_t& aIndex, nsString* aKey)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aKey);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->GetKey(aIndex, *aKey);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvGetItem(const nsString& aKey, nsString* aValue)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aValue);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->GetItem(aKey, *aValue);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvSetItem(const nsString& aKey, const nsString& aValue)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->SetItem(aKey, aValue);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvRemoveItem(const nsString& aKey)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->RemoveItem(aKey);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvClear()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->Clear();

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvGetKeys(nsTArray<nsString>* aKeys)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aKeys);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->GetKeys(*aKeys);

  return IPC_OK();
}

/*******************************************************************************
 * LSRequestBase
 ******************************************************************************/

void
LSRequestBase::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  NoteComplete();
}

mozilla::ipc::IPCResult
LSRequestBase::RecvCancel()
{
  AssertIsOnOwningThread();

  IProtocol* mgr = Manager();
  if (!PBackgroundLSRequestParent::Send__delete__(this, NS_ERROR_FAILURE)) {
    return IPC_FAIL_NO_REASON(mgr);
  }

  return IPC_OK();
}

/*******************************************************************************
 * PrepareDatastoreOp
 ******************************************************************************/

PrepareDatastoreOp::PrepareDatastoreOp(const LSRequestParams& aParams)
  : mParams(aParams.get_LSRequestPrepareDatastoreParams())
  , mState(State::Initial)
{
  MOZ_ASSERT(aParams.type() ==
               LSRequestParams::TLSRequestPrepareDatastoreParams);
}

PrepareDatastoreOp::~PrepareDatastoreOp()
{
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
}

void
PrepareDatastoreOp::Dispatch()
{
  AssertIsOnOwningThread();

  const PrincipalOrQuotaInfo& info = mParams.info();

  if (info.type() == PrincipalOrQuotaInfo::TPrincipalInfo) {
    mState = State::OpeningOnMainThread;
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
  } else {
    MOZ_ASSERT(info.type() == PrincipalOrQuotaInfo::TQuotaInfo);

    mState = State::OpeningOnOwningThread;
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(this));
  }
}

nsresult
PrepareDatastoreOp::OpenOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::OpeningOnMainThread);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalOrQuotaInfo& info = mParams.info();

  MOZ_ASSERT(info.type() == PrincipalOrQuotaInfo::TPrincipalInfo);

  const PrincipalInfo& principalInfo = info.get_PrincipalInfo();

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(&mSuffix, &mGroup, &mOrigin);
  } else {
    MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(principalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = QuotaManager::GetInfoFromPrincipal(principal,
                                            &mSuffix,
                                            &mGroup,
                                            &mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mState = State::SendingReadyMessage;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult
PrepareDatastoreOp::OpenOnOwningThread()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::OpeningOnOwningThread);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalOrQuotaInfo& info = mParams.info();

  MOZ_ASSERT(info.type() == PrincipalOrQuotaInfo::TQuotaInfo);

  const QuotaInfo& quotaInfo = info.get_QuotaInfo();

  mSuffix = quotaInfo.suffix();
  mGroup = quotaInfo.group();
  mOrigin = quotaInfo.origin();

  mState = State::SendingReadyMessage;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void
PrepareDatastoreOp::SendReadyMessage()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingReadyMessage);

  if (!MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);

    Cleanup();

    mState = State::Completed;
  } else {
    Unused << SendReady();

    mState = State::WaitingForFinish;
  }
}

void
PrepareDatastoreOp::SendResults()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (!MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  } else {
    LSRequestResponse response;

    if (NS_SUCCEEDED(ResultCode())) {
      RefPtr<Datastore> datastore;

      if (gDatastores) {
        datastore = gDatastores->Get(mOrigin);
      }

      if (!datastore) {
        datastore = new Datastore(mOrigin);

        if (!gDatastores) {
          gDatastores = new DatastoreHashtable();
        }

        gDatastores->Put(mOrigin, datastore);
      }

      uint64_t datastoreId = ++gLastDatastoreId;

      nsAutoPtr<PreparedDatastore> preparedDatastore(
        new PreparedDatastore(datastore));

      if (!gPreparedDatastores) {
        gPreparedDatastores = new PreparedDatastoreHashtable();
      }
      gPreparedDatastores->Put(datastoreId, preparedDatastore.forget());

      LSRequestPrepareDatastoreResponse prepareDatastoreResponse;
      prepareDatastoreResponse.datastoreId() = datastoreId;

      response = prepareDatastoreResponse;
    } else {
      response = ResultCode();
    }

    Unused <<
      PBackgroundLSRequestParent::Send__delete__(this, response);
  }

  Cleanup();

  mState = State::Completed;
}

void
PrepareDatastoreOp::Cleanup()
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(gPrepareDatastoreOps);
  gPrepareDatastoreOps->RemoveElement(this);

  if (gPrepareDatastoreOps->IsEmpty()) {
    gPrepareDatastoreOps = nullptr;
  }
}

NS_IMETHODIMP
PrepareDatastoreOp::Run()
{
  nsresult rv;

  switch (mState) {
    case State::OpeningOnMainThread:
      rv = OpenOnMainThread();
      break;

    case State::OpeningOnOwningThread:
      rv = OpenOnOwningThread();
      break;

    case State::SendingReadyMessage:
      SendReadyMessage();
      return NS_OK;

    case State::SendingResults:
      SendResults();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::SendingReadyMessage) {
    MaybeSetFailureCode(rv);

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State::SendingReadyMessage;

    if (IsOnOwningThread()) {
      SendReadyMessage();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
        OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

mozilla::ipc::IPCResult
PrepareDatastoreOp::RecvFinish()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForFinish);

  mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(this));

  return IPC_OK();
}

/*******************************************************************************
 * QuotaClient
 ******************************************************************************/

QuotaClient* QuotaClient::sInstance = nullptr;

QuotaClient::QuotaClient()
  : mShutdownRequested(false)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!sInstance, "We expect this to be a singleton!");

  sInstance = this;
}

QuotaClient::~QuotaClient()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this, "We expect this to be a singleton!");

  sInstance = nullptr;
}

mozilla::dom::quota::Client::Type
QuotaClient::GetType()
{
  return QuotaClient::LS;
}

nsresult
QuotaClient::InitOrigin(PersistenceType aPersistenceType,
                        const nsACString& aGroup,
                        const nsACString& aOrigin,
                        const AtomicBool& aCanceled,
                        UsageInfo* aUsageInfo)
{
  AssertIsOnIOThread();

  if (!aUsageInfo) {
    return NS_OK;
  }

  return GetUsageForOrigin(aPersistenceType,
                           aGroup,
                           aOrigin,
                           aCanceled,
                           aUsageInfo);
}

nsresult
QuotaClient::GetUsageForOrigin(PersistenceType aPersistenceType,
                               const nsACString& aGroup,
                               const nsACString& aOrigin,
                               const AtomicBool& aCanceled,
                               UsageInfo* aUsageInfo)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aUsageInfo);

  return NS_OK;
}

void
QuotaClient::OnOriginClearCompleted(PersistenceType aPersistenceType,
                                    const nsACString& aOrigin)
{
  AssertIsOnIOThread();
}

void
QuotaClient::ReleaseIOThreadObjects()
{
  AssertIsOnIOThread();
}

void
QuotaClient::AbortOperations(const nsACString& aOrigin)
{
  AssertIsOnBackgroundThread();
}

void
QuotaClient::AbortOperationsForProcess(ContentParentId aContentParentId)
{
  AssertIsOnBackgroundThread();
}

void
QuotaClient::StartIdleMaintenance()
{
  AssertIsOnBackgroundThread();
}

void
QuotaClient::StopIdleMaintenance()
{
  AssertIsOnBackgroundThread();
}

void
QuotaClient::ShutdownWorkThreads()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  mShutdownRequested = true;

  // gPrepareDatastoreOps are short lived objects running a state machine.
  // The shutdown flag is checked between states, so we don't have to notify
  // all the objects here.
  // Allocation of a new PrepareDatastoreOp object is prevented once the
  // shutdown flag is set.
  // When the last PrepareDatastoreOp finishes, the gPrepareDatastoreOps array
  // is destroyed.

  // If database actors haven't been created yet, don't do anything special.
  // We are shutting down and we can release prepared datastores immediatelly
  // since database actors will never be created for them.
  if (gPreparedDatastores) {
    gPreparedDatastores->Clear();
    gPreparedDatastores = nullptr;
  }

  if (gLiveDatabases) {
    for (Database* database : *gLiveDatabases) {
      database->RequestAllowToClose();
    }
  }

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() {
    // Don't have to check gPreparedDatastores since we nulled it out above.
    return !gPrepareDatastoreOps && !gDatastores && !gLiveDatabases;
  }));
}

} // namespace dom
} // namespace mozilla
