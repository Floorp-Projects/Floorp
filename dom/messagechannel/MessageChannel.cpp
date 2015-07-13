/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessageChannel.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/MessageChannelBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MessageChannel, mWindow, mPort1, mPort2)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MessageChannel)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MessageChannel)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MessageChannel)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

namespace {
bool gPrefInitialized = false;
bool gPrefEnabled = false;

bool
CheckPermission(nsIPrincipal* aPrincipal, bool aCallerChrome)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gPrefInitialized) {
    Preferences::AddBoolVarCache(&gPrefEnabled, "dom.messageChannel.enabled");
    gPrefInitialized = true;
  }

  // Enabled by pref
  if (gPrefEnabled) {
    return true;
  }

  // Chrome callers are allowed.
  if (aCallerChrome) {
    return true;
  }

  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(aPrincipal->GetURI(getter_AddRefs(uri))) || !uri) {
    return false;
  }

  bool isResource = false;
  if (NS_FAILED(uri->SchemeIs("resource", &isResource))) {
    return false;
  }

  return isResource;
}

nsIPrincipal*
GetPrincipalFromWorkerPrivate(workers::WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsIPrincipal* principal = aWorkerPrivate->GetPrincipal();
  if (principal) {
    return principal;
  }

  // Walk up to our containing page
  workers::WorkerPrivate* wp = aWorkerPrivate;
  while (wp->GetParent()) {
    wp = wp->GetParent();
  }

  nsPIDOMWindow* window = wp->GetWindow();
  if (!window) {
    return nullptr;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc) {
    return nullptr;
  }

  return doc->NodePrincipal();
}

// A WorkerMainThreadRunnable to synchronously dispatch the call of
// CheckPermission() from the worker thread to the main thread.
class CheckPermissionRunnable final : public workers::WorkerMainThreadRunnable
{
public:
  bool mResult;
  bool mCallerChrome;

  explicit CheckPermissionRunnable(workers::WorkerPrivate* aWorkerPrivate)
    : workers::WorkerMainThreadRunnable(aWorkerPrivate)
    , mResult(false)
    , mCallerChrome(false)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    mCallerChrome = aWorkerPrivate->UsesSystemPrincipal();
  }

protected:
  virtual bool
  MainThreadRun() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsIPrincipal* principal = GetPrincipalFromWorkerPrivate(mWorkerPrivate);
    if (!principal) {
      return true;
    }

    bool isNullPrincipal;
    nsresult rv = principal->GetIsNullPrincipal(&isNullPrincipal);
    if (NS_WARN_IF(NS_FAILED(rv))) {
       return true;
    }

    if (NS_WARN_IF(isNullPrincipal)) {
      return true;
    }

    mResult = CheckPermission(principal, mCallerChrome);
    return true;
  }
};

} // namespace

/* static */ bool
MessageChannel::Enabled(JSContext* aCx, JSObject* aGlobal)
{
  if (NS_IsMainThread()) {
    JS::Rooted<JSObject*> global(aCx, aGlobal);

    nsCOMPtr<nsPIDOMWindow> win = Navigator::GetWindowFromGlobal(global);
    if (!win) {
      return false;
    }

    nsIDocument* doc = win->GetExtantDoc();
    if (!doc) {
      return false;
    }

    return CheckPermission(doc->NodePrincipal(),
                           nsContentUtils::IsCallerChrome());
  }

  workers::WorkerPrivate* workerPrivate =
    workers::GetWorkerPrivateFromContext(aCx);
  workerPrivate->AssertIsOnWorkerThread();

  nsRefPtr<CheckPermissionRunnable> runnable =
    new CheckPermissionRunnable(workerPrivate);
  runnable->Dispatch(aCx);

  return runnable->mResult;
}

MessageChannel::MessageChannel(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
}

MessageChannel::~MessageChannel()
{
}

JSObject*
MessageChannel::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MessageChannelBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<MessageChannel>
MessageChannel::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  // window can be null in workers.
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());

  nsID portUUID1;
  aRv = nsContentUtils::GenerateUUIDInPlace(portUUID1);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsID portUUID2;
  aRv = nsContentUtils::GenerateUUIDInPlace(portUUID2);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<MessageChannel> channel = new MessageChannel(window);

  channel->mPort1 = MessagePort::Create(window, portUUID1, portUUID2, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  channel->mPort2 = MessagePort::Create(window, portUUID2, portUUID1, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  channel->mPort1->UnshippedEntangle(channel->mPort2);
  channel->mPort2->UnshippedEntangle(channel->mPort1);

  return channel.forget();
}

} // namespace dom
} // namespace mozilla
