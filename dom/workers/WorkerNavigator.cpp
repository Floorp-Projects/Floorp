/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WorkerNavigator.h"

#include <utility>

#include "ErrorList.h"
#include "MainThreadUtils.h"
#include "RuntimeService.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "mozilla/dom/LockManager.h"
#include "mozilla/dom/MediaCapabilities.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/StorageManager.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerNavigatorBinding.h"
#include "mozilla/dom/WorkerStatus.h"
#include "mozilla/dom/network/Connection.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/webgpu/Instance.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIGlobalObject.h"
#include "nsLiteralString.h"
#include "nsPIDOMWindow.h"
#include "nsRFPService.h"
#include "nsString.h"

class JSObject;
struct JSContext;

namespace mozilla::dom {

using namespace workerinternals;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WorkerNavigator)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WorkerNavigator)
  tmp->Invalidate();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WorkerNavigator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStorageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConnection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaCapabilities)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebGpu)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocks)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WorkerNavigator, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WorkerNavigator, Release)

WorkerNavigator::WorkerNavigator(const NavigatorProperties& aProperties,
                                 bool aOnline)
    : mProperties(aProperties), mOnline(aOnline) {}

WorkerNavigator::~WorkerNavigator() { Invalidate(); }

/* static */
already_AddRefed<WorkerNavigator> WorkerNavigator::Create(bool aOnLine) {
  RuntimeService* rts = RuntimeService::GetService();
  MOZ_ASSERT(rts);

  const RuntimeService::NavigatorProperties& properties =
      rts->GetNavigatorProperties();

  RefPtr<WorkerNavigator> navigator = new WorkerNavigator(properties, aOnLine);

  return navigator.forget();
}

void WorkerNavigator::Invalidate() {
  if (mStorageManager) {
    mStorageManager->Shutdown();
    mStorageManager = nullptr;
  }

  mConnection = nullptr;

  mMediaCapabilities = nullptr;

  mWebGpu = nullptr;

  mLocks = nullptr;
}

JSObject* WorkerNavigator::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return WorkerNavigator_Binding::Wrap(aCx, this, aGivenProto);
}

void WorkerNavigator::SetLanguages(const nsTArray<nsString>& aLanguages) {
  WorkerNavigator_Binding::ClearCachedLanguagesValue(this);
  mProperties.mLanguages = aLanguages.Clone();
}

void WorkerNavigator::GetAppName(nsString& aAppName,
                                 CallerType aCallerType) const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if ((!mProperties.mAppNameOverridden.IsEmpty() ||
       StaticPrefs::privacy_resistFingerprinting()) &&
      !workerPrivate->UsesSystemPrincipal()) {
    // We will spoof this value when 'privacy.resistFingerprinting' is true.
    // See nsRFPService.h for spoofed value.
    aAppName = StaticPrefs::privacy_resistFingerprinting()
                   ? NS_LITERAL_STRING_FROM_CSTRING(SPOOFED_APPNAME)
                   : mProperties.mAppNameOverridden;
  } else {
    aAppName = mProperties.mAppName;
  }
}

void WorkerNavigator::GetAppVersion(nsString& aAppVersion,
                                    CallerType aCallerType,
                                    ErrorResult& aRv) const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if ((!mProperties.mAppVersionOverridden.IsEmpty() ||
       StaticPrefs::privacy_resistFingerprinting()) &&
      !workerPrivate->UsesSystemPrincipal()) {
    // We will spoof this value when 'privacy.resistFingerprinting' is true.
    // See nsRFPService.h for spoofed value.
    aAppVersion = StaticPrefs::privacy_resistFingerprinting()
                      ? NS_LITERAL_STRING_FROM_CSTRING(SPOOFED_APPVERSION)
                      : mProperties.mAppVersionOverridden;
  } else {
    aAppVersion = mProperties.mAppVersion;
  }
}

void WorkerNavigator::GetPlatform(nsString& aPlatform, CallerType aCallerType,
                                  ErrorResult& aRv) const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if ((!mProperties.mPlatformOverridden.IsEmpty() ||
       StaticPrefs::privacy_resistFingerprinting()) &&
      !workerPrivate->UsesSystemPrincipal()) {
    // We will spoof this value when 'privacy.resistFingerprinting' is true.
    // See nsRFPService.h for spoofed value.
    aPlatform = StaticPrefs::privacy_resistFingerprinting()
                    ? NS_LITERAL_STRING_FROM_CSTRING(SPOOFED_PLATFORM)
                    : mProperties.mPlatformOverridden;
  } else {
    aPlatform = mProperties.mPlatform;
  }
}

namespace {

class GetUserAgentRunnable final : public WorkerMainThreadRunnable {
  nsString& mUA;

 public:
  GetUserAgentRunnable(WorkerPrivate* aWorkerPrivate, nsString& aUA)
      : WorkerMainThreadRunnable(aWorkerPrivate, "UserAgent getter"_ns),
        mUA(aUA) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  virtual bool MainThreadRun() override {
    AssertIsOnMainThread();

    nsCOMPtr<nsPIDOMWindowInner> window = mWorkerPrivate->GetWindow();

    bool shouldResistFingerprinting =
        mWorkerPrivate->ShouldResistFingerprinting();
    nsresult rv =
        dom::Navigator::GetUserAgent(window, mWorkerPrivate->GetDocument(),
                                     Some(shouldResistFingerprinting), mUA);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to retrieve user-agent from the worker thread.");
    }

    return true;
  }
};

}  // namespace

void WorkerNavigator::GetUserAgent(nsString& aUserAgent, CallerType aCallerType,
                                   ErrorResult& aRv) const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  RefPtr<GetUserAgentRunnable> runnable =
      new GetUserAgentRunnable(workerPrivate, aUserAgent);

  runnable->Dispatch(Canceling, aRv);
}

uint64_t WorkerNavigator::HardwareConcurrency() const {
  RuntimeService* rts = RuntimeService::GetService();
  MOZ_ASSERT(rts);

  WorkerPrivate* aWorkerPrivate = GetCurrentThreadWorkerPrivate();
  bool rfp = aWorkerPrivate->ShouldResistFingerprinting();

  return rts->ClampedHardwareConcurrency(rfp);
}

StorageManager* WorkerNavigator::Storage() {
  if (!mStorageManager) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    RefPtr<nsIGlobalObject> global = workerPrivate->GlobalScope();
    MOZ_ASSERT(global);

    mStorageManager = new StorageManager(global);
  }

  return mStorageManager;
}

network::Connection* WorkerNavigator::GetConnection(ErrorResult& aRv) {
  if (!mConnection) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    mConnection = network::Connection::CreateForWorker(workerPrivate, aRv);
  }

  return mConnection;
}

dom::MediaCapabilities* WorkerNavigator::MediaCapabilities() {
  if (!mMediaCapabilities) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    nsIGlobalObject* global = workerPrivate->GlobalScope();
    MOZ_ASSERT(global);

    mMediaCapabilities = new dom::MediaCapabilities(global);
  }
  return mMediaCapabilities;
}

webgpu::Instance* WorkerNavigator::Gpu() {
  if (!mWebGpu) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    nsIGlobalObject* global = workerPrivate->GlobalScope();
    MOZ_ASSERT(global);

    mWebGpu = webgpu::Instance::Create(global);
  }
  return mWebGpu;
}

dom::LockManager* WorkerNavigator::Locks() {
  if (!mLocks) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    nsIGlobalObject* global = workerPrivate->GlobalScope();
    MOZ_ASSERT(global);

    mLocks = new dom::LockManager(global);
  }
  return mLocks;
}

}  // namespace mozilla::dom
