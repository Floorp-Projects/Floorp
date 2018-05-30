/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Clients.h"

#include "ClientDOMUtil.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientsBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/SystemGroup.h"
#include "nsIGlobalObject.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::PrincipalInfo;

NS_IMPL_CYCLE_COLLECTING_ADDREF(Clients);
NS_IMPL_CYCLE_COLLECTING_RELEASE(Clients);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Clients, mGlobal);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Clients)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Clients::Clients(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
{
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
}

JSObject*
Clients::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ClientsBinding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject*
Clients::GetParentObject() const
{
  return mGlobal;
}

already_AddRefed<Promise>
Clients::Get(const nsAString& aClientID, ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate->IsServiceWorker());
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> outerPromise = Promise::Create(mGlobal, aRv);
  if (aRv.Failed()) {
    return outerPromise.forget();
  }

  nsID id;
  // nsID::Parse accepts both "{...}" and "...", but we only emit the latter, so
  // forbid strings that start with "{" to avoid inconsistency and bugs like
  // bug 1446225.
  if (aClientID.IsEmpty() || aClientID.CharAt(0) == '{' ||
      !id.Parse(NS_ConvertUTF16toUTF8(aClientID).get())) {
    // Invalid ID means we will definitely not find a match, so just
    // resolve with undefined indicating "not found".
    outerPromise->MaybeResolveWithUndefined();
    return outerPromise.forget();
  }

  const PrincipalInfo& principalInfo = workerPrivate->GetPrincipalInfo();
  nsCOMPtr<nsISerialEventTarget> target =
    mGlobal->EventTargetFor(TaskCategory::Other);

  RefPtr<ClientOpPromise> innerPromise =
    ClientManager::GetInfoAndState(ClientGetInfoAndStateArgs(id, principalInfo),
                                   target);

  nsCString scope = workerPrivate->ServiceWorkerScope();
  auto holder = MakeRefPtr<DOMMozPromiseRequestHolder<ClientOpPromise>>(mGlobal);

  innerPromise->Then(target, __func__,
    [outerPromise, holder, scope] (const ClientOpResult& aResult) {
      holder->Complete();
      NS_ENSURE_TRUE_VOID(holder->GetParentObject());
      RefPtr<Client> client = new Client(holder->GetParentObject(),
                                         aResult.get_ClientInfoAndState());
      if (client->GetStorageAccess() == nsContentUtils::StorageAccess::eAllow) {
        outerPromise->MaybeResolve(std::move(client));
        return;
      }
      nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableFunction("Clients::Get() storage denied",
        [scope] {
          ServiceWorkerManager::LocalizeAndReportToAllClients(
            scope, "ServiceWorkerGetClientStorageError", nsTArray<nsString>());
        });
      SystemGroup::Dispatch(TaskCategory::Other, r.forget());
      outerPromise->MaybeResolveWithUndefined();
    }, [outerPromise, holder] (nsresult aResult) {
      holder->Complete();
      outerPromise->MaybeResolveWithUndefined();
    })->Track(*holder);

  return outerPromise.forget();
}

namespace {

class MatchAllComparator final
{
public:
  bool
  LessThan(Client* aLeft, Client* aRight) const
  {
    TimeStamp leftFocusTime = aLeft->LastFocusTime();
    TimeStamp rightFocusTime = aRight->LastFocusTime();
    // If the focus times are the same, then default to creation order.
    // MatchAll should return oldest Clients first.
    if (leftFocusTime == rightFocusTime) {
      return aLeft->CreationTime() < aRight->CreationTime();
    }

    // Otherwise compare focus times.  We reverse the logic here so
    // that the most recently focused window is first in the list.
    if (!leftFocusTime.IsNull() && rightFocusTime.IsNull()) {
      return true;
    }
    if (leftFocusTime.IsNull() && !rightFocusTime.IsNull()) {
      return false;
    }
    return leftFocusTime > rightFocusTime;
  }

  bool
  Equals(Client* aLeft, Client* aRight) const
  {
    return aLeft->LastFocusTime() == aRight->LastFocusTime() &&
           aLeft->CreationTime() == aRight->CreationTime();
  }
};

} // anonymous namespace

already_AddRefed<Promise>
Clients::MatchAll(const ClientQueryOptions& aOptions, ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate->IsServiceWorker());
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> outerPromise = Promise::Create(mGlobal, aRv);
  if (aRv.Failed()) {
    return outerPromise.forget();
  }

  nsCOMPtr<nsIGlobalObject> global = mGlobal;
  nsCString scope = workerPrivate->ServiceWorkerScope();

  ClientMatchAllArgs args(workerPrivate->GetServiceWorkerDescriptor().ToIPC(),
                          aOptions.mType,
                          aOptions.mIncludeUncontrolled);
  StartClientManagerOp(&ClientManager::MatchAll, args, mGlobal,
    [outerPromise, global, scope] (const ClientOpResult& aResult) {
      nsTArray<RefPtr<Client>> clientList;
      bool storageDenied = false;
      for (const ClientInfoAndState& value : aResult.get_ClientList().values()) {
        RefPtr<Client> client = new Client(global, value);
        if (client->GetStorageAccess() != nsContentUtils::StorageAccess::eAllow) {
          storageDenied = true;
          continue;
        }
        clientList.AppendElement(std::move(client));
      }
      if (storageDenied) {
        nsCOMPtr<nsIRunnable> r =
          NS_NewRunnableFunction("Clients::MatchAll() storage denied",
          [scope] {
            ServiceWorkerManager::LocalizeAndReportToAllClients(
              scope, "ServiceWorkerGetClientStorageError", nsTArray<nsString>());
          });
        SystemGroup::Dispatch(TaskCategory::Other, r.forget());
      }
      clientList.Sort(MatchAllComparator());
      outerPromise->MaybeResolve(clientList);
    }, [outerPromise] (nsresult aResult) {
      outerPromise->MaybeReject(aResult);
    });

  return outerPromise.forget();
}

already_AddRefed<Promise>
Clients::OpenWindow(const nsAString& aURL, ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate->IsServiceWorker());
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> outerPromise = Promise::Create(mGlobal, aRv);
  if (aRv.Failed()) {
    return outerPromise.forget();
  }

  if (aURL.EqualsLiteral("about:blank")) {
    // TODO: Improve this error in bug 1412856.
    outerPromise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);
    return outerPromise.forget();
  }

  if (!workerPrivate->GlobalScope()->WindowInteractionAllowed()) {
    outerPromise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return outerPromise.forget();
  }

  const PrincipalInfo& principalInfo = workerPrivate->GetPrincipalInfo();
  nsCString baseURL = workerPrivate->GetLocationInfo().mHref;
  ClientOpenWindowArgs args(principalInfo, NS_ConvertUTF16toUTF8(aURL),
                            baseURL);

  nsCOMPtr<nsIGlobalObject> global = mGlobal;

  StartClientManagerOp(&ClientManager::OpenWindow, args, mGlobal,
    [outerPromise, global] (const ClientOpResult& aResult) {
      if (aResult.type() != ClientOpResult::TClientInfoAndState) {
        outerPromise->MaybeResolve(JS::NullHandleValue);
        return;
      }
      RefPtr<Client> client =
        new Client(global, aResult.get_ClientInfoAndState());
      outerPromise->MaybeResolve(client);
    }, [outerPromise] (nsresult aResult) {
      // TODO: Improve this error in bug 1412856.  Ideally we should throw
      //       the TypeError in the child process and pass it back to here.
      outerPromise->MaybeReject(NS_ERROR_TYPE_ERR);
    });

  return outerPromise.forget();
}

already_AddRefed<Promise>
Clients::Claim(ErrorResult& aRv)
{
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate->IsServiceWorker());
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<Promise> outerPromise = Promise::Create(mGlobal, aRv);
  if (aRv.Failed()) {
    return outerPromise.forget();
  }

  const ServiceWorkerDescriptor& serviceWorker =
    workerPrivate->GetServiceWorkerDescriptor();

  if (serviceWorker.State() != ServiceWorkerState::Activating &&
      serviceWorker.State() != ServiceWorkerState::Activated) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return outerPromise.forget();
  }

  StartClientManagerOp(
    &ClientManager::Claim, ClientClaimArgs(serviceWorker.ToIPC()), mGlobal,
    [outerPromise] (const ClientOpResult& aResult) {
      outerPromise->MaybeResolveWithUndefined();
    }, [outerPromise] (nsresult aResult) {
      outerPromise->MaybeReject(aResult);
    });

  return outerPromise.forget();
}

} // namespace dom
} // namespace mozilla
