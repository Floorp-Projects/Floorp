/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerService.h"

#include "ClientManagerParent.h"
#include "ClientSourceParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SystemGroup.h"
#include "nsIAsyncShutdown.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::ContentPrincipalInfo;
using mozilla::ipc::PrincipalInfo;

namespace {

ClientManagerService* sClientManagerServiceInstance = nullptr;

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

  // While the ClientManagerService will be gracefully terminated as windows
  // and workers are naturally killed, this can cause us to do extra work
  // relatively late in the shutdown process.  To avoid this we eagerly begin
  // shutdown at the first sign it has begun.  Since we handle normal shutdown
  // gracefully we don't really need to block anything here.  We just begin
  // destroying our IPC actors immediately.
  OnShutdown()->Then(GetCurrentThreadSerialEventTarget(), __func__,
    [] () {
      RefPtr<ClientManagerService> svc = ClientManagerService::GetInstance();
      if (svc) {
        svc->Shutdown();
      }
    });
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

} // namespace dom
} // namespace mozilla
