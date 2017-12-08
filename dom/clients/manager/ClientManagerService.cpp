/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerService.h"

#include "ClientManagerParent.h"
#include "ClientNavigateOpParent.h"
#include "ClientOpenWindowOpParent.h"
#include "ClientOpenWindowUtils.h"
#include "ClientSourceParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SystemGroup.h"
#include "nsIAsyncShutdown.h"
#include "nsIXULRuntime.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::ContentPrincipalInfo;
using mozilla::ipc::PrincipalInfo;

namespace {

ClientManagerService* sClientManagerServiceInstance = nullptr;
bool sClientManagerServiceShutdownRegistered = false;

bool
MatchPrincipalInfo(const PrincipalInfo& aLeft, const PrincipalInfo& aRight)
{
  if (aLeft.type() != aRight.type()) {
    return false;
  }

  switch (aLeft.type()) {
    case PrincipalInfo::TContentPrincipalInfo:
    {
      const ContentPrincipalInfo& leftContent = aLeft.get_ContentPrincipalInfo();
      const ContentPrincipalInfo& rightContent = aRight.get_ContentPrincipalInfo();
      return leftContent.attrs() == rightContent.attrs() &&
             leftContent.originNoSuffix() == rightContent.originNoSuffix();
    }
    case PrincipalInfo::TSystemPrincipalInfo:
    {
      // system principal always matches
      return true;
    }
    case PrincipalInfo::TNullPrincipalInfo:
    {
      // null principal never matches
      return false;
    }
    default:
    {
      break;
    }
  }

  // Clients (windows/workers) should never have an expanded principal type.
  MOZ_CRASH("unexpected principal type!");
}

class ClientShutdownBlocker final : public nsIAsyncShutdownBlocker
{
  RefPtr<GenericPromise::Private> mPromise;

  ~ClientShutdownBlocker() = default;

public:
  explicit ClientShutdownBlocker(GenericPromise::Private* aPromise)
    : mPromise(aPromise)
  {
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
  }

  NS_IMETHOD
  GetName(nsAString& aNameOut) override
  {
    aNameOut =
      NS_LITERAL_STRING("ClientManagerService: start destroying IPC actors early");
    return NS_OK;
  }

  NS_IMETHOD
  BlockShutdown(nsIAsyncShutdownClient* aClient) override
  {
    mPromise->Resolve(true, __func__);
    aClient->RemoveBlocker(this);
    return NS_OK;
  }

  NS_IMETHOD
  GetState(nsIPropertyBag**) override
  {
    return NS_OK;
  }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(ClientShutdownBlocker, nsIAsyncShutdownBlocker)

// Helper function the resolves a MozPromise when we detect that the browser
// has begun to shutdown.
RefPtr<GenericPromise>
OnShutdown()
{
  RefPtr<GenericPromise::Private> ref = new GenericPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("ClientManagerServer::OnShutdown",
  [ref] () {
    nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdown();
    if (!svc) {
      ref->Resolve(true, __func__);
      return;
    }

    nsCOMPtr<nsIAsyncShutdownClient> phase;
    MOZ_ALWAYS_SUCCEEDS(svc->GetXpcomWillShutdown(getter_AddRefs(phase)));
    if (!phase) {
      ref->Resolve(true, __func__);
      return;
    }

    nsCOMPtr<nsIAsyncShutdownBlocker> blocker = new ClientShutdownBlocker(ref);
    nsresult rv =
      phase->AddBlocker(blocker, NS_LITERAL_STRING(__FILE__), __LINE__,
                        NS_LITERAL_STRING("ClientManagerService shutdown"));

    if (NS_FAILED(rv)) {
      ref->Resolve(true, __func__);
      return;
    }
  });

  MOZ_ALWAYS_SUCCEEDS(
    SystemGroup::Dispatch(TaskCategory::Other, r.forget()));

  return ref.forget();
}

} // anonymous namespace

ClientManagerService::ClientManagerService()
  : mShutdown(false)
{
  AssertIsOnBackgroundThread();

  // Only register one shutdown handler at a time.  If a previous service
  // instance did this, but shutdown has not come, then we can avoid
  // doing it again.
  if (!sClientManagerServiceShutdownRegistered) {
    sClientManagerServiceShutdownRegistered = true;

    // While the ClientManagerService will be gracefully terminated as windows
    // and workers are naturally killed, this can cause us to do extra work
    // relatively late in the shutdown process.  To avoid this we eagerly begin
    // shutdown at the first sign it has begun.  Since we handle normal shutdown
    // gracefully we don't really need to block anything here.  We just begin
    // destroying our IPC actors immediately.
    OnShutdown()->Then(GetCurrentThreadSerialEventTarget(), __func__,
      [] () {
        // Look up the latest service instance, if it exists.  This may
        // be different from the instance that registered the shutdown
        // handler.
        RefPtr<ClientManagerService> svc = ClientManagerService::GetInstance();
        if (svc) {
          svc->Shutdown();
        }
      });
  }
}

ClientManagerService::~ClientManagerService()
{
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mSourceTable.Count() == 0);
  MOZ_DIAGNOSTIC_ASSERT(mManagerList.IsEmpty());

  MOZ_DIAGNOSTIC_ASSERT(sClientManagerServiceInstance == this);
  sClientManagerServiceInstance = nullptr;
}

void
ClientManagerService::Shutdown()
{
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(sClientManagerServiceShutdownRegistered);

  // If many ClientManagerService are created and destroyed quickly we can
  // in theory get more than one shutdown listener calling us.
  if (mShutdown) {
    return;
  }
  mShutdown = true;

  // Begin destroying our various manager actors which will in turn destroy
  // all source, handle, and operation actors.
  AutoTArray<ClientManagerParent*, 16> list(mManagerList);
  for (auto actor : list) {
    Unused << PClientManagerParent::Send__delete__(actor);
  }
}

// static
already_AddRefed<ClientManagerService>
ClientManagerService::GetOrCreateInstance()
{
  AssertIsOnBackgroundThread();

  if (!sClientManagerServiceInstance) {
    sClientManagerServiceInstance = new ClientManagerService();
  }

  RefPtr<ClientManagerService> ref(sClientManagerServiceInstance);
  return ref.forget();
}

// static
already_AddRefed<ClientManagerService>
ClientManagerService::GetInstance()
{
  AssertIsOnBackgroundThread();

  if (!sClientManagerServiceInstance) {
    return nullptr;
  }

  RefPtr<ClientManagerService> ref(sClientManagerServiceInstance);
  return ref.forget();
}

bool
ClientManagerService::AddSource(ClientSourceParent* aSource)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSource);
  auto entry = mSourceTable.LookupForAdd(aSource->Info().Id());
  // Do not permit overwriting an existing ClientSource with the same
  // UUID.  This would allow a spoofed ClientParentSource actor to
  // intercept postMessage() intended for the real actor.
  if (NS_WARN_IF(!!entry)) {
    return false;
  }
  entry.OrInsert([&] { return aSource; });
  return true;
}

bool
ClientManagerService::RemoveSource(ClientSourceParent* aSource)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSource);
  auto entry = mSourceTable.Lookup(aSource->Info().Id());
  if (NS_WARN_IF(!entry)) {
    return false;
  }
  entry.Remove();
  return true;
}

ClientSourceParent*
ClientManagerService::FindSource(const nsID& aID, const PrincipalInfo& aPrincipalInfo)
{
  AssertIsOnBackgroundThread();

  auto entry = mSourceTable.Lookup(aID);
  if (!entry) {
    return nullptr;
  }

  ClientSourceParent* source = entry.Data();
  if (source->IsFrozen() ||
      !MatchPrincipalInfo(source->Info().PrincipalInfo(), aPrincipalInfo)) {
    return nullptr;
  }

  return source;
}

void
ClientManagerService::AddManager(ClientManagerParent* aManager)
{
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(aManager);
  MOZ_ASSERT(!mManagerList.Contains(aManager));
  mManagerList.AppendElement(aManager);

  // If shutdown has already begun then immediately destroy the actor.
  if (mShutdown) {
    Unused << PClientManagerParent::Send__delete__(aManager);
  }
}

void
ClientManagerService::RemoveManager(ClientManagerParent* aManager)
{
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(aManager);
  DebugOnly<bool> removed = mManagerList.RemoveElement(aManager);
  MOZ_ASSERT(removed);
}

RefPtr<ClientOpPromise>
ClientManagerService::Navigate(const ClientNavigateArgs& aArgs)
{
  RefPtr<ClientOpPromise> ref;

  ClientSourceParent* source = FindSource(aArgs.target().id(),
                                          aArgs.target().principalInfo());
  if (!source) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    return ref.forget();
  }

  PClientManagerParent* manager = source->Manager();
  MOZ_DIAGNOSTIC_ASSERT(manager);

  ClientNavigateOpConstructorArgs args;
  args.url() = aArgs.url();
  args.baseURL() = aArgs.baseURL();

  // This is safe to do because the ClientSourceChild cannot directly delete
  // itself.  Instead it sends a Teardown message to the parent which then
  // calls delete.  That means we can be sure that we are not racing with
  // source destruction here.
  args.targetParent() = source;

  RefPtr<ClientOpPromise::Private> promise =
    new ClientOpPromise::Private(__func__);

  ClientNavigateOpParent* op = new ClientNavigateOpParent(args, promise);
  PClientNavigateOpParent* result =
    manager->SendPClientNavigateOpConstructor(op, args);
  if (!result) {
    promise->Reject(NS_ERROR_FAILURE, __func__);
    ref = promise;
    return ref.forget();
  }

  ref = promise;
  return ref.forget();
}

namespace
{

class PromiseListHolder final
{
  RefPtr<ClientOpPromise::Private> mResultPromise;
  nsTArray<RefPtr<ClientOpPromise>> mPromiseList;
  nsTArray<ClientInfoAndState> mResultList;
  uint32_t mOutstandingPromiseCount;

  void
  ProcessSuccess(const ClientInfoAndState& aResult)
  {
    mResultList.AppendElement(aResult);
    ProcessCompletion();
  }

  void
  ProcessCompletion()
  {
    MOZ_DIAGNOSTIC_ASSERT(mOutstandingPromiseCount > 0);
    mOutstandingPromiseCount -= 1;
    MaybeFinish();
  }

  ~PromiseListHolder() = default;
public:
  PromiseListHolder()
    : mResultPromise(new ClientOpPromise::Private(__func__))
    , mOutstandingPromiseCount(0)
  {
  }

  already_AddRefed<ClientOpPromise>
  GetResultPromise()
  {
    RefPtr<PromiseListHolder> kungFuDeathGrip = this;
    mResultPromise->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [kungFuDeathGrip] (const ClientOpResult& aResult) { },
      [kungFuDeathGrip] (nsresult aResult) { });

    RefPtr<ClientOpPromise> ref = mResultPromise;
    return ref.forget();
  }

  void
  AddPromise(RefPtr<ClientOpPromise>&& aPromise)
  {
    mPromiseList.AppendElement(Move(aPromise));
    MOZ_DIAGNOSTIC_ASSERT(mPromiseList.LastElement());
    mOutstandingPromiseCount += 1;

    RefPtr<PromiseListHolder> self(this);
    mPromiseList.LastElement()->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [self] (const ClientOpResult& aResult) {
        // TODO: This is pretty clunky.  Try to figure out a better
        //       wait for MatchAll() and Claim() to share this code
        //       even though they expect different return values.
        if (aResult.type() == ClientOpResult::TClientInfoAndState) {
          self->ProcessSuccess(aResult.get_ClientInfoAndState());
        } else {
          self->ProcessCompletion();
        }
      }, [self] (nsresult aResult) {
        self->ProcessCompletion();
      });
  }

  void
  MaybeFinish()
  {
    if (!mOutstandingPromiseCount) {
      mResultPromise->Resolve(mResultList, __func__);
    }
  }

  NS_INLINE_DECL_REFCOUNTING(PromiseListHolder)
};

} // anonymous namespace

RefPtr<ClientOpPromise>
ClientManagerService::MatchAll(const ClientMatchAllArgs& aArgs)
{
  AssertIsOnBackgroundThread();

  const ClientEndPoint& endpoint = aArgs.endpoint();

  const PrincipalInfo& principalInfo =
    endpoint.type() == ClientEndPoint::TIPCClientInfo
      ? endpoint.get_IPCClientInfo().principalInfo()
      : endpoint.get_IPCServiceWorkerDescriptor().principalInfo();

  RefPtr<PromiseListHolder> promiseList = new PromiseListHolder();

  for (auto iter = mSourceTable.Iter(); !iter.Done(); iter.Next()) {
    ClientSourceParent* source = iter.UserData();
    MOZ_DIAGNOSTIC_ASSERT(source);

    if (source->IsFrozen() || !source->ExecutionReady()) {
      continue;
    }

    if (aArgs.type() != ClientType::All &&
        source->Info().Type() != aArgs.type()) {
      continue;
    }

    if (!MatchPrincipalInfo(source->Info().PrincipalInfo(), principalInfo)) {
      continue;
    }

    if (!aArgs.includeUncontrolled()) {
      if (endpoint.type() != ClientEndPoint::TIPCServiceWorkerDescriptor) {
        continue;
      }

      const Maybe<ServiceWorkerDescriptor>& controller =
        source->GetController();
      if (controller.isNothing()) {
        continue;
      }

      const IPCServiceWorkerDescriptor& serviceWorker =
        endpoint.get_IPCServiceWorkerDescriptor();

      if(controller.ref().Id() != serviceWorker.id() ||
         controller.ref().Scope() != serviceWorker.scope()) {
        continue;
      }
    }

    promiseList->AddPromise(
      source->StartOp(Move(ClientGetInfoAndStateArgs(source->Info().Id(),
                                                     source->Info().PrincipalInfo()))));
  }

  // Maybe finish the promise now in case we didn't find any matching clients.
  promiseList->MaybeFinish();

  return promiseList->GetResultPromise();
}

RefPtr<ClientOpPromise>
ClientManagerService::Claim(const ClientClaimArgs& aArgs)
{
  AssertIsOnBackgroundThread();

  const IPCServiceWorkerDescriptor& serviceWorker = aArgs.serviceWorker();
  const PrincipalInfo& principalInfo = serviceWorker.principalInfo();

  RefPtr<PromiseListHolder> promiseList = new PromiseListHolder();

  for (auto iter = mSourceTable.Iter(); !iter.Done(); iter.Next()) {
    ClientSourceParent* source = iter.UserData();
    MOZ_DIAGNOSTIC_ASSERT(source);

    if (source->IsFrozen()) {
      continue;
    }

    if (!MatchPrincipalInfo(source->Info().PrincipalInfo(), principalInfo)) {
      continue;
    }

    const Maybe<ServiceWorkerDescriptor>& controller = source->GetController();
    if (controller.isSome() &&
        controller.ref().Scope() == serviceWorker.scope() &&
        controller.ref().Id() == serviceWorker.id()) {
      continue;
    }

    // TODO: This logic to determine if a service worker should control
    //       a particular client should be moved to the ServiceWorkerManager.
    //       This can't happen until the SWM is moved to the parent process,
    //       though.
    if (!source->ExecutionReady() ||
        source->Info().Type() == ClientType::Serviceworker ||
        source->Info().URL().Find(serviceWorker.scope()) != 0) {
      continue;
    }

    promiseList->AddPromise(source->StartOp(aArgs));
  }

  // Maybe finish the promise now in case we didn't find any matching clients.
  promiseList->MaybeFinish();

  return promiseList->GetResultPromise();
}

RefPtr<ClientOpPromise>
ClientManagerService::GetInfoAndState(const ClientGetInfoAndStateArgs& aArgs)
{
  RefPtr<ClientOpPromise> ref;

  ClientSourceParent* source = FindSource(aArgs.id(), aArgs.principalInfo());
  if (!source || !source->ExecutionReady()) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    return ref.forget();
  }

  return source->StartOp(aArgs);
}

namespace {

class OpenWindowRunnable final : public Runnable
{
  RefPtr<ClientOpPromise::Private> mPromise;
  const ClientOpenWindowArgs mArgs;
  RefPtr<ContentParent> mSourceProcess;

  ~OpenWindowRunnable()
  {
    NS_ReleaseOnMainThreadSystemGroup(mSourceProcess.forget());
  }

public:
  OpenWindowRunnable(ClientOpPromise::Private* aPromise,
                     const ClientOpenWindowArgs& aArgs,
                     already_AddRefed<ContentParent> aSourceProcess)
    : Runnable("ClientManagerService::OpenWindowRunnable")
    , mPromise(aPromise)
    , mArgs(aArgs)
    , mSourceProcess(aSourceProcess)
  {
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!BrowserTabsRemoteAutostart()) {
      RefPtr<ClientOpPromise> p = ClientOpenWindowInCurrentProcess(mArgs);
      p->ChainTo(mPromise.forget(), __func__);
      return NS_OK;
    }

    RefPtr<ContentParent> targetProcess;

    // Possibly try to open the window in the same process that called
    // openWindow().  This is a temporary compat setting until the
    // multi-e10s service worker refactor is complete.
    if (Preferences::GetBool("dom.clients.openwindow_favors_same_process",
                             false)) {
      targetProcess = mSourceProcess;
    }

    // Otherwise, use our normal remote process selection mechanism for
    // opening the window.  This will start a process if one is not
    // present.
    if (!targetProcess) {
      targetProcess =
        ContentParent::GetNewOrUsedBrowserProcess(NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE),
                                                  ContentParent::GetInitialProcessPriority(nullptr),
                                                  nullptr);
    }

    ClientOpenWindowOpParent* actor =
      new ClientOpenWindowOpParent(mArgs, mPromise);

    // If this fails the actor will be automatically destroyed which will
    // reject the promise.
    Unused << targetProcess->SendPClientOpenWindowOpConstructor(actor, mArgs);

    return NS_OK;
  }
};

} // anonymous namespace

RefPtr<ClientOpPromise>
ClientManagerService::OpenWindow(const ClientOpenWindowArgs& aArgs,
                                 already_AddRefed<ContentParent> aSourceProcess)
{
  RefPtr<ClientOpPromise::Private> promise =
    new ClientOpPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r = new OpenWindowRunnable(promise, aArgs,
                                                   Move(aSourceProcess));
  MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other,
                                            r.forget()));

  RefPtr<ClientOpPromise> ref = promise;
  return ref.forget();
}

} // namespace dom
} // namespace mozilla
