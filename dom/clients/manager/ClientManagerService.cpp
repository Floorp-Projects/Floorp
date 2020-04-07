/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerService.h"

#include "ClientManagerParent.h"
#include "ClientNavigateOpParent.h"
#include "ClientOpenWindowUtils.h"
#include "ClientPrincipalUtils.h"
#include "ClientSourceParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/MozPromise.h"
#include "mozilla/SchedulerGroup.h"
#include "jsfriendapi.h"
#include "nsIAsyncShutdown.h"
#include "nsIXULRuntime.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::PrincipalInfo;

namespace {

ClientManagerService* sClientManagerServiceInstance = nullptr;
bool sClientManagerServiceShutdownRegistered = false;

class ClientShutdownBlocker final : public nsIAsyncShutdownBlocker {
  RefPtr<GenericPromise::Private> mPromise;

  ~ClientShutdownBlocker() = default;

 public:
  explicit ClientShutdownBlocker(GenericPromise::Private* aPromise)
      : mPromise(aPromise) {
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
  }

  NS_IMETHOD
  GetName(nsAString& aNameOut) override {
    aNameOut = NS_LITERAL_STRING(
        "ClientManagerService: start destroying IPC actors early");
    return NS_OK;
  }

  NS_IMETHOD
  BlockShutdown(nsIAsyncShutdownClient* aClient) override {
    mPromise->Resolve(true, __func__);
    aClient->RemoveBlocker(this);
    return NS_OK;
  }

  NS_IMETHOD
  GetState(nsIPropertyBag**) override { return NS_OK; }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(ClientShutdownBlocker, nsIAsyncShutdownBlocker)

// Helper function the resolves a MozPromise when we detect that the browser
// has begun to shutdown.
RefPtr<GenericPromise> OnShutdown() {
  RefPtr<GenericPromise::Private> ref = new GenericPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("ClientManagerServer::OnShutdown", [ref]() {
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

        nsCOMPtr<nsIAsyncShutdownBlocker> blocker =
            new ClientShutdownBlocker(ref);
        nsresult rv = phase->AddBlocker(
            blocker, NS_LITERAL_STRING(__FILE__), __LINE__,
            NS_LITERAL_STRING("ClientManagerService shutdown"));

        if (NS_FAILED(rv)) {
          ref->Resolve(true, __func__);
          return;
        }
      });

  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));

  return ref;
}

}  // anonymous namespace

ClientManagerService::ClientManagerService() : mShutdown(false) {
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
    OnShutdown()->Then(GetCurrentThreadSerialEventTarget(), __func__, []() {
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

ClientManagerService::~ClientManagerService() {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mSourceTable.Count() == 0);
  MOZ_DIAGNOSTIC_ASSERT(mManagerList.IsEmpty());

  MOZ_DIAGNOSTIC_ASSERT(sClientManagerServiceInstance == this);
  sClientManagerServiceInstance = nullptr;
}

void ClientManagerService::Shutdown() {
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
ClientManagerService::GetOrCreateInstance() {
  AssertIsOnBackgroundThread();

  if (!sClientManagerServiceInstance) {
    sClientManagerServiceInstance = new ClientManagerService();
  }

  RefPtr<ClientManagerService> ref(sClientManagerServiceInstance);
  return ref.forget();
}

// static
already_AddRefed<ClientManagerService> ClientManagerService::GetInstance() {
  AssertIsOnBackgroundThread();

  if (!sClientManagerServiceInstance) {
    return nullptr;
  }

  RefPtr<ClientManagerService> ref(sClientManagerServiceInstance);
  return ref.forget();
}

bool ClientManagerService::AddSource(ClientSourceParent* aSource) {
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

  // Now that we've been created, notify any handles that were
  // waiting on us.
  auto* handles = mPendingHandles.GetValue(aSource->Info().Id());
  if (handles) {
    for (auto handle : *handles) {
      handle->FoundSource(aSource);
    }
  }
  mPendingHandles.Remove(aSource->Info().Id());
  return true;
}

bool ClientManagerService::RemoveSource(ClientSourceParent* aSource) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSource);
  auto entry = mSourceTable.Lookup(aSource->Info().Id());
  if (NS_WARN_IF(!entry)) {
    return false;
  }
  entry.Remove();
  return true;
}

ClientSourceParent* ClientManagerService::FindSource(
    const nsID& aID, const PrincipalInfo& aPrincipalInfo) {
  AssertIsOnBackgroundThread();

  auto entry = mSourceTable.Lookup(aID);
  if (!entry) {
    return nullptr;
  }

  ClientSourceParent* source = entry.Data();
  if (source->IsFrozen() ||
      !ClientMatchPrincipalInfo(source->Info().PrincipalInfo(),
                                aPrincipalInfo)) {
    return nullptr;
  }

  return source;
}

void ClientManagerService::WaitForSource(ClientHandleParent* aHandle,
                                         const nsID& aID) {
  auto& entry = mPendingHandles.GetOrInsert(aID);
  entry.AppendElement(aHandle);
}

void ClientManagerService::StopWaitingForSource(ClientHandleParent* aHandle,
                                                const nsID& aID) {
  auto* entry = mPendingHandles.GetValue(aID);
  if (entry) {
    entry->RemoveElement(aHandle);
  }
}

void ClientManagerService::AddManager(ClientManagerParent* aManager) {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(aManager);
  MOZ_ASSERT(!mManagerList.Contains(aManager));
  mManagerList.AppendElement(aManager);

  // If shutdown has already begun then immediately destroy the actor.
  if (mShutdown) {
    Unused << PClientManagerParent::Send__delete__(aManager);
  }
}

void ClientManagerService::RemoveManager(ClientManagerParent* aManager) {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(aManager);
  DebugOnly<bool> removed = mManagerList.RemoveElement(aManager);
  MOZ_ASSERT(removed);
}

RefPtr<ClientOpPromise> ClientManagerService::Navigate(
    const ClientNavigateArgs& aArgs) {
  ClientSourceParent* source =
      FindSource(aArgs.target().id(), aArgs.target().principalInfo());
  if (!source) {
    CopyableErrorResult rv;
    rv.ThrowInvalidStateError("Unknown client");
    return ClientOpPromise::CreateAndReject(rv, __func__);
  }

  const IPCServiceWorkerDescriptor& serviceWorker = aArgs.serviceWorker();

  // Per https://w3c.github.io/ServiceWorker/#dom-windowclient-navigate step 4,
  // if the service worker does not control the client, reject with a TypeError.
  const Maybe<ServiceWorkerDescriptor>& controller = source->GetController();
  if (controller.isNothing() ||
      controller.ref().Scope() != serviceWorker.scope() ||
      controller.ref().Id() != serviceWorker.id()) {
    CopyableErrorResult rv;
    rv.ThrowTypeError("Client is not controlled by this Service Worker");
    return ClientOpPromise::CreateAndReject(rv, __func__);
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
    CopyableErrorResult rv;
    rv.ThrowInvalidStateError("Client is aborted");
    promise->Reject(rv, __func__);
  }

  return promise;
}

namespace {

class PromiseListHolder final {
  RefPtr<ClientOpPromise::Private> mResultPromise;
  nsTArray<RefPtr<ClientOpPromise>> mPromiseList;
  nsTArray<ClientInfoAndState> mResultList;
  uint32_t mOutstandingPromiseCount;

  void ProcessSuccess(const ClientInfoAndState& aResult) {
    mResultList.AppendElement(aResult);
    ProcessCompletion();
  }

  void ProcessCompletion() {
    MOZ_DIAGNOSTIC_ASSERT(mOutstandingPromiseCount > 0);
    mOutstandingPromiseCount -= 1;
    MaybeFinish();
  }

  ~PromiseListHolder() = default;

 public:
  PromiseListHolder()
      : mResultPromise(new ClientOpPromise::Private(__func__)),
        mOutstandingPromiseCount(0) {}

  RefPtr<ClientOpPromise> GetResultPromise() {
    RefPtr<PromiseListHolder> kungFuDeathGrip = this;
    return mResultPromise->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [kungFuDeathGrip](const ClientOpPromise::ResolveOrRejectValue& aValue) {
          return ClientOpPromise::CreateAndResolveOrReject(aValue, __func__);
        });
  }

  void AddPromise(RefPtr<ClientOpPromise>&& aPromise) {
    mPromiseList.AppendElement(std::move(aPromise));
    MOZ_DIAGNOSTIC_ASSERT(mPromiseList.LastElement());
    mOutstandingPromiseCount += 1;

    RefPtr<PromiseListHolder> self(this);
    mPromiseList.LastElement()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [self](const ClientOpResult& aResult) {
          // TODO: This is pretty clunky.  Try to figure out a better
          //       wait for MatchAll() and Claim() to share this code
          //       even though they expect different return values.
          if (aResult.type() == ClientOpResult::TClientInfoAndState) {
            self->ProcessSuccess(aResult.get_ClientInfoAndState());
          } else {
            self->ProcessCompletion();
          }
        },
        [self](const CopyableErrorResult& aResult) {
          self->ProcessCompletion();
        });
  }

  void MaybeFinish() {
    if (!mOutstandingPromiseCount) {
      mResultPromise->Resolve(mResultList, __func__);
    }
  }

  NS_INLINE_DECL_REFCOUNTING(PromiseListHolder)
};

}  // anonymous namespace

RefPtr<ClientOpPromise> ClientManagerService::MatchAll(
    const ClientMatchAllArgs& aArgs) {
  AssertIsOnBackgroundThread();

  ServiceWorkerDescriptor swd(aArgs.serviceWorker());
  const PrincipalInfo& principalInfo = swd.PrincipalInfo();

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

    if (!ClientMatchPrincipalInfo(source->Info().PrincipalInfo(),
                                  principalInfo)) {
      continue;
    }

    if (!aArgs.includeUncontrolled()) {
      const Maybe<ServiceWorkerDescriptor>& controller =
          source->GetController();
      if (controller.isNothing()) {
        continue;
      }

      if (controller.ref().Id() != swd.Id() ||
          controller.ref().Scope() != swd.Scope()) {
        continue;
      }
    }

    promiseList->AddPromise(source->StartOp(ClientGetInfoAndStateArgs(
        source->Info().Id(), source->Info().PrincipalInfo())));
  }

  // Maybe finish the promise now in case we didn't find any matching clients.
  promiseList->MaybeFinish();

  return promiseList->GetResultPromise();
}

namespace {

RefPtr<ClientOpPromise> ClaimOnMainThread(
    const ClientInfo& aClientInfo, const ServiceWorkerDescriptor& aDescriptor) {
  RefPtr<ClientOpPromise::Private> promise =
      new ClientOpPromise::Private(__func__);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [promise, clientInfo = std::move(aClientInfo),
                 desc = std::move(aDescriptor)]() {
        auto scopeExit = MakeScopeExit([&] {
          // This will truncate the URLs if they have embedded nulls, if that
          // can happen, but for // our purposes here that's OK.
          nsPrintfCString err(
              "Service worker at <%s> can't claim Client at <%s>",
              desc.ScriptURL().get(), clientInfo.URL().get());
          CopyableErrorResult rv;
          rv.ThrowInvalidStateError(err);
          promise->Reject(rv, __func__);
        });

        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        NS_ENSURE_TRUE_VOID(swm);

        RefPtr<GenericErrorResultPromise> inner =
            swm->MaybeClaimClient(clientInfo, desc);
        inner->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [promise](bool aResult) {
              promise->Resolve(CopyableErrorResult(), __func__);
            },
            [promise](const CopyableErrorResult& aRv) {
              promise->Reject(aRv, __func__);
            });

        scopeExit.release();
      });

  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));

  return promise;
}

}  // anonymous namespace

RefPtr<ClientOpPromise> ClientManagerService::Claim(
    const ClientClaimArgs& aArgs) {
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

    if (!ClientMatchPrincipalInfo(source->Info().PrincipalInfo(),
                                  principalInfo)) {
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

    if (ServiceWorkerParentInterceptEnabled()) {
      promiseList->AddPromise(ClaimOnMainThread(
          source->Info(), ServiceWorkerDescriptor(serviceWorker)));
    } else {
      promiseList->AddPromise(source->StartOp(aArgs));
    }
  }

  // Maybe finish the promise now in case we didn't find any matching clients.
  promiseList->MaybeFinish();

  return promiseList->GetResultPromise();
}

RefPtr<ClientOpPromise> ClientManagerService::GetInfoAndState(
    const ClientGetInfoAndStateArgs& aArgs) {
  ClientSourceParent* source = FindSource(aArgs.id(), aArgs.principalInfo());

  if (!source) {
    CopyableErrorResult rv;
    rv.ThrowInvalidStateError("Unknown client");
    return ClientOpPromise::CreateAndReject(rv, __func__);
  }

  if (!source->ExecutionReady()) {
    RefPtr<ClientManagerService> self = this;

    // rejection ultimately converted to `undefined` in Clients::Get
    return source->ExecutionReadyPromise()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [self, aArgs]() -> RefPtr<ClientOpPromise> {
          ClientSourceParent* source =
              self->FindSource(aArgs.id(), aArgs.principalInfo());

          if (!source) {
            CopyableErrorResult rv;
            rv.ThrowInvalidStateError("Unknown client");
            return ClientOpPromise::CreateAndReject(rv, __func__);
          }

          return source->StartOp(aArgs);
        });
  }

  return source->StartOp(aArgs);
}

RefPtr<ClientOpPromise> ClientManagerService::OpenWindow(
    const ClientOpenWindowArgs& aArgs) {
  return InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
                     [aArgs]() { return ClientOpenWindow(aArgs); });
}

bool ClientManagerService::HasWindow(
    const Maybe<ContentParentId>& aContentParentId,
    const PrincipalInfo& aPrincipalInfo, const nsID& aClientId) {
  AssertIsOnBackgroundThread();

  ClientSourceParent* source = FindSource(aClientId, aPrincipalInfo);
  if (!source) {
    return false;
  }

  if (!source->ExecutionReady()) {
    return false;
  }

  if (source->Info().Type() != ClientType::Window) {
    return false;
  }

  if (aContentParentId && !source->IsOwnedByProcess(aContentParentId.value())) {
    return false;
  }

  return true;
}

}  // namespace dom
}  // namespace mozilla
