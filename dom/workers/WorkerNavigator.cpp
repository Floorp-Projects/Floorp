/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/StorageManager.h"
#include "mozilla/dom/WorkerNavigator.h"
#include "mozilla/dom/WorkerNavigatorBinding.h"
#include "mozilla/dom/network/Connection.h"

#include "nsProxyRelease.h"
#include "RuntimeService.h"

#include "nsIDocument.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

#include "mozilla/dom/Navigator.h"

namespace mozilla {
namespace dom {

using namespace mozilla::dom::workers;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WorkerNavigator, mStorageManager,
                                      mConnection);

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WorkerNavigator, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WorkerNavigator, Release)

WorkerNavigator::WorkerNavigator(const NavigatorProperties& aProperties,
                                 bool aOnline)
  : mProperties(aProperties)
  , mOnline(aOnline)
{
}

WorkerNavigator::~WorkerNavigator()
{
}

/* static */ already_AddRefed<WorkerNavigator>
WorkerNavigator::Create(bool aOnLine)
{
  RuntimeService* rts = RuntimeService::GetService();
  MOZ_ASSERT(rts);

  const RuntimeService::NavigatorProperties& properties =
    rts->GetNavigatorProperties();

  RefPtr<WorkerNavigator> navigator =
    new WorkerNavigator(properties, aOnLine);

  return navigator.forget();
}

JSObject*
WorkerNavigator::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WorkerNavigatorBinding::Wrap(aCx, this, aGivenProto);
}

void
WorkerNavigator::SetLanguages(const nsTArray<nsString>& aLanguages)
{
  WorkerNavigatorBinding::ClearCachedLanguagesValue(this);
  mProperties.mLanguages = aLanguages;
}

void
WorkerNavigator::GetAppName(nsString& aAppName, CallerType aCallerType) const
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if (!mProperties.mAppNameOverridden.IsEmpty() &&
      !workerPrivate->UsesSystemPrincipal()) {
    aAppName = mProperties.mAppNameOverridden;
  } else {
    aAppName = mProperties.mAppName;
  }
}

void
WorkerNavigator::GetAppVersion(nsString& aAppVersion, CallerType aCallerType,
                               ErrorResult& aRv) const
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if (!mProperties.mAppVersionOverridden.IsEmpty() &&
      !workerPrivate->UsesSystemPrincipal()) {
    aAppVersion = mProperties.mAppVersionOverridden;
  } else {
    aAppVersion = mProperties.mAppVersion;
  }
}

void
WorkerNavigator::GetPlatform(nsString& aPlatform, CallerType aCallerType,
                             ErrorResult& aRv) const
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if (!mProperties.mPlatformOverridden.IsEmpty() &&
      !workerPrivate->UsesSystemPrincipal()) {
    aPlatform = mProperties.mPlatformOverridden;
  } else {
    aPlatform = mProperties.mPlatform;
  }
}

namespace {

class GetUserAgentRunnable final : public WorkerMainThreadRunnable
{
  nsString& mUA;

public:
  GetUserAgentRunnable(WorkerPrivate* aWorkerPrivate, nsString& aUA)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("UserAgent getter"))
    , mUA(aUA)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  virtual bool MainThreadRun() override
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsPIDOMWindowInner> window = mWorkerPrivate->GetWindow();
    nsCOMPtr<nsIURI> uri;
    if (window && window->GetDocShell()) {
      nsIDocument* doc = window->GetExtantDoc();
      if (doc) {
        doc->NodePrincipal()->GetURI(getter_AddRefs(uri));
      }
    }

    bool isCallerChrome = mWorkerPrivate->UsesSystemPrincipal();
    nsresult rv = dom::Navigator::GetUserAgent(window, uri,
                                               isCallerChrome, mUA);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to retrieve user-agent from the worker thread.");
    }

    return true;
  }
};

} // namespace

void
WorkerNavigator::GetUserAgent(nsString& aUserAgent, CallerType aCallerType,
                              ErrorResult& aRv) const
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  RefPtr<GetUserAgentRunnable> runnable =
    new GetUserAgentRunnable(workerPrivate, aUserAgent);

  runnable->Dispatch(Terminating, aRv);
}

uint64_t
WorkerNavigator::HardwareConcurrency() const
{
  RuntimeService* rts = RuntimeService::GetService();
  MOZ_ASSERT(rts);

  return rts->ClampedHardwareConcurrency();
}

StorageManager*
WorkerNavigator::Storage()
{
  if (!mStorageManager) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    RefPtr<nsIGlobalObject> global = workerPrivate->GlobalScope();
    MOZ_ASSERT(global);

    mStorageManager = new StorageManager(global);
  }

  return mStorageManager;
}

network::Connection*
WorkerNavigator::GetConnection(ErrorResult& aRv)
{
  if (!mConnection) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    mConnection = network::Connection::CreateForWorker(workerPrivate, aRv);
  }

  return mConnection;
}


} // namespace dom
} // namespace mozilla
