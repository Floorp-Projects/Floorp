/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_runtimeservice_h__
#define mozilla_dom_workers_runtimeservice_h__

#include "mozilla/dom/WorkerCommon.h"

#include "nsIObserver.h"

#include "js/ContextOptions.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/SafeRefPtr.h"
#include "mozilla/dom/workerinternals/JSSettings.h"
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

class nsITimer;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {
struct WorkerLoadInfo;
class WorkerThread;

namespace workerinternals {

class RuntimeService final : public nsIObserver {
  struct WorkerDomainInfo {
    nsCString mDomain;
    nsTArray<WorkerPrivate*> mActiveWorkers;
    nsTArray<WorkerPrivate*> mActiveServiceWorkers;
    nsTArray<WorkerPrivate*> mQueuedWorkers;
    uint32_t mChildWorkerCount;

    WorkerDomainInfo() : mActiveWorkers(1), mChildWorkerCount(0) {}

    uint32_t ActiveWorkerCount() const {
      return mActiveWorkers.Length() + mChildWorkerCount;
    }

    uint32_t ActiveServiceWorkerCount() const {
      return mActiveServiceWorkers.Length();
    }

    bool HasNoWorkers() const {
      return ActiveWorkerCount() == 0 && ActiveServiceWorkerCount() == 0;
    }
  };

  struct IdleThreadInfo {
    SafeRefPtr<WorkerThread> mThread;
    mozilla::TimeStamp mExpirationTime;
  };

  mozilla::Mutex mMutex;

  // Protected by mMutex.
  nsClassHashtable<nsCStringHashKey, WorkerDomainInfo> mDomainMap;

  // Protected by mMutex.
  nsTArray<IdleThreadInfo> mIdleThreadArray;

  // *Not* protected by mMutex.
  nsClassHashtable<nsPtrHashKey<const nsPIDOMWindowInner>,
                   nsTArray<WorkerPrivate*> >
      mWindowMap;

  // Only used on the main thread.
  nsCOMPtr<nsITimer> mIdleThreadTimer;

  static UniquePtr<workerinternals::JSSettings> sDefaultJSSettings;

 public:
  struct NavigatorProperties {
    nsString mAppName;
    nsString mAppNameOverridden;
    nsString mAppVersion;
    nsString mAppVersionOverridden;
    nsString mPlatform;
    nsString mPlatformOverridden;
    CopyableTArray<nsString> mLanguages;
  };

 private:
  NavigatorProperties mNavigatorProperties;

  // True when the observer service holds a reference to this object.
  bool mObserved;
  bool mShuttingDown;
  bool mNavigatorPropertiesLoaded;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static RuntimeService* GetOrCreateService();

  static RuntimeService* GetService();

  bool RegisterWorker(WorkerPrivate& aWorkerPrivate);

  void UnregisterWorker(WorkerPrivate& aWorkerPrivate);

  void CancelWorkersForWindow(const nsPIDOMWindowInner& aWindow);

  void FreezeWorkersForWindow(const nsPIDOMWindowInner& aWindow);

  void ThawWorkersForWindow(const nsPIDOMWindowInner& aWindow);

  void SuspendWorkersForWindow(const nsPIDOMWindowInner& aWindow);

  void ResumeWorkersForWindow(const nsPIDOMWindowInner& aWindow);

  void PropagateStorageAccessPermissionGranted(
      const nsPIDOMWindowInner& aWindow);

  const NavigatorProperties& GetNavigatorProperties() const {
    return mNavigatorProperties;
  }

  void NoteIdleThread(SafeRefPtr<WorkerThread> aThread);

  static void GetDefaultJSSettings(workerinternals::JSSettings& aSettings) {
    AssertIsOnMainThread();
    aSettings = *sDefaultJSSettings;
  }

  static void SetDefaultContextOptions(
      const JS::ContextOptions& aContextOptions) {
    AssertIsOnMainThread();
    sDefaultJSSettings->contextOptions = aContextOptions;
  }

  void UpdateAppNameOverridePreference(const nsAString& aValue);

  void UpdateAppVersionOverridePreference(const nsAString& aValue);

  void UpdatePlatformOverridePreference(const nsAString& aValue);

  void UpdateAllWorkerContextOptions();

  void UpdateAllWorkerLanguages(const nsTArray<nsString>& aLanguages);

  static void SetDefaultJSGCSettings(JSGCParamKey aKey,
                                     Maybe<uint32_t> aValue) {
    AssertIsOnMainThread();
    sDefaultJSSettings->ApplyGCSetting(aKey, aValue);
  }

  void UpdateAllWorkerMemoryParameter(JSGCParamKey aKey,
                                      Maybe<uint32_t> aValue);

#ifdef JS_GC_ZEAL
  static void SetDefaultGCZeal(uint8_t aGCZeal, uint32_t aFrequency) {
    AssertIsOnMainThread();
    sDefaultJSSettings->gcZeal = aGCZeal;
    sDefaultJSSettings->gcZealFrequency = aFrequency;
  }

  void UpdateAllWorkerGCZeal();
#endif

  void SetLowMemoryStateAllWorkers(bool aState);

  void GarbageCollectAllWorkers(bool aShrinking);

  void CycleCollectAllWorkers();

  void SendOfflineStatusChangeEventToAllWorkers(bool aIsOffline);

  void MemoryPressureAllWorkers();

  uint32_t ClampedHardwareConcurrency() const;

  void CrashIfHanging();

  bool IsShuttingDown() const { return mShuttingDown; }

 private:
  RuntimeService();
  ~RuntimeService();

  nsresult Init();

  void Shutdown();

  void Cleanup();

  void AddAllTopLevelWorkersToArray(nsTArray<WorkerPrivate*>& aWorkers);

  nsTArray<WorkerPrivate*> GetWorkersForWindow(
      const nsPIDOMWindowInner& aWindow) const;

  bool ScheduleWorker(WorkerPrivate& aWorkerPrivate);

  static void ShutdownIdleThreads(nsITimer* aTimer, void* aClosure);

  template <typename Func>
  void BroadcastAllWorkers(const Func& aFunc);
};

}  // namespace workerinternals
}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_workers_runtimeservice_h__ */
