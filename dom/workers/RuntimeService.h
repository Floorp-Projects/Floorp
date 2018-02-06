/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_runtimeservice_h__
#define mozilla_dom_workers_runtimeservice_h__

#include "mozilla/dom/WorkerCommon.h"

#include "nsIObserver.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/workerinternals/JSSettings.h"
#include "mozilla/Mutex.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

class nsITimer;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {
class SharedWorker;
struct WorkerLoadInfo;
class WorkerThread;

namespace workerinternals {

class RuntimeService final : public nsIObserver
{
  struct SharedWorkerInfo
  {
    WorkerPrivate* mWorkerPrivate;
    nsCString mScriptSpec;
    nsString mName;

    SharedWorkerInfo(WorkerPrivate* aWorkerPrivate,
                     const nsACString& aScriptSpec,
                     const nsAString& aName)
    : mWorkerPrivate(aWorkerPrivate), mScriptSpec(aScriptSpec), mName(aName)
    { }
  };

  struct WorkerDomainInfo
  {
    nsCString mDomain;
    nsTArray<WorkerPrivate*> mActiveWorkers;
    nsTArray<WorkerPrivate*> mActiveServiceWorkers;
    nsTArray<WorkerPrivate*> mQueuedWorkers;
    nsTArray<UniquePtr<SharedWorkerInfo>> mSharedWorkerInfos;
    uint32_t mChildWorkerCount;

    WorkerDomainInfo()
    : mActiveWorkers(1), mChildWorkerCount(0)
    { }

    uint32_t
    ActiveWorkerCount() const
    {
      return mActiveWorkers.Length() +
             mChildWorkerCount;
    }

    uint32_t
    ActiveServiceWorkerCount() const
    {
      return mActiveServiceWorkers.Length();
    }

    bool
    HasNoWorkers() const
    {
      return ActiveWorkerCount() == 0 &&
             ActiveServiceWorkerCount() == 0;
    }
  };

  struct IdleThreadInfo;

  mozilla::Mutex mMutex;

  // Protected by mMutex.
  nsClassHashtable<nsCStringHashKey, WorkerDomainInfo> mDomainMap;

  // Protected by mMutex.
  nsTArray<IdleThreadInfo> mIdleThreadArray;

  // *Not* protected by mMutex.
  nsClassHashtable<nsPtrHashKey<nsPIDOMWindowInner>,
                   nsTArray<WorkerPrivate*> > mWindowMap;

  // Only used on the main thread.
  nsCOMPtr<nsITimer> mIdleThreadTimer;

  static workerinternals::JSSettings sDefaultJSSettings;

public:
  struct NavigatorProperties
  {
    nsString mAppName;
    nsString mAppNameOverridden;
    nsString mAppVersion;
    nsString mAppVersionOverridden;
    nsString mPlatform;
    nsString mPlatformOverridden;
    nsTArray<nsString> mLanguages;
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

  static RuntimeService*
  GetOrCreateService();

  static RuntimeService*
  GetService();

  bool
  RegisterWorker(WorkerPrivate* aWorkerPrivate);

  void
  UnregisterWorker(WorkerPrivate* aWorkerPrivate);

  void
  RemoveSharedWorker(WorkerDomainInfo* aDomainInfo,
                     WorkerPrivate* aWorkerPrivate);

  void
  CancelWorkersForWindow(nsPIDOMWindowInner* aWindow);

  void
  FreezeWorkersForWindow(nsPIDOMWindowInner* aWindow);

  void
  ThawWorkersForWindow(nsPIDOMWindowInner* aWindow);

  void
  SuspendWorkersForWindow(nsPIDOMWindowInner* aWindow);

  void
  ResumeWorkersForWindow(nsPIDOMWindowInner* aWindow);

  nsresult
  CreateSharedWorker(const GlobalObject& aGlobal,
                     const nsAString& aScriptURL,
                     const nsAString& aName,
                     SharedWorker** aSharedWorker);

  void
  ForgetSharedWorker(WorkerPrivate* aWorkerPrivate);

  const NavigatorProperties&
  GetNavigatorProperties() const
  {
    return mNavigatorProperties;
  }

  void
  NoteIdleThread(WorkerThread* aThread);

  static void
  GetDefaultJSSettings(workerinternals::JSSettings& aSettings)
  {
    AssertIsOnMainThread();
    aSettings = sDefaultJSSettings;
  }

  static void
  SetDefaultContextOptions(const JS::ContextOptions& aContextOptions)
  {
    AssertIsOnMainThread();
    sDefaultJSSettings.contextOptions = aContextOptions;
  }

  void
  UpdateAppNameOverridePreference(const nsAString& aValue);

  void
  UpdateAppVersionOverridePreference(const nsAString& aValue);

  void
  UpdatePlatformOverridePreference(const nsAString& aValue);

  void
  UpdateAllWorkerContextOptions();

  void
  UpdateAllWorkerLanguages(const nsTArray<nsString>& aLanguages);

  static void
  SetDefaultJSGCSettings(JSGCParamKey aKey, uint32_t aValue)
  {
    AssertIsOnMainThread();
    sDefaultJSSettings.ApplyGCSetting(aKey, aValue);
  }

  void
  UpdateAllWorkerMemoryParameter(JSGCParamKey aKey, uint32_t aValue);

#ifdef JS_GC_ZEAL
  static void
  SetDefaultGCZeal(uint8_t aGCZeal, uint32_t aFrequency)
  {
    AssertIsOnMainThread();
    sDefaultJSSettings.gcZeal = aGCZeal;
    sDefaultJSSettings.gcZealFrequency = aFrequency;
  }

  void
  UpdateAllWorkerGCZeal();
#endif

  void
  GarbageCollectAllWorkers(bool aShrinking);

  void
  CycleCollectAllWorkers();

  void
  SendOfflineStatusChangeEventToAllWorkers(bool aIsOffline);

  void
  MemoryPressureAllWorkers();

  uint32_t ClampedHardwareConcurrency() const;

  void CrashIfHanging();

private:
  RuntimeService();
  ~RuntimeService();

  nsresult
  Init();

  void
  Shutdown();

  void
  Cleanup();

  void
  AddAllTopLevelWorkersToArray(nsTArray<WorkerPrivate*>& aWorkers);

  void
  GetWorkersForWindow(nsPIDOMWindowInner* aWindow,
                      nsTArray<WorkerPrivate*>& aWorkers);

  bool
  ScheduleWorker(WorkerPrivate* aWorkerPrivate);

  static void
  ShutdownIdleThreads(nsITimer* aTimer, void* aClosure);

  nsresult
  CreateSharedWorkerFromLoadInfo(JSContext* aCx,
                                 WorkerLoadInfo* aLoadInfo,
                                 const nsAString& aScriptURL,
                                 const nsAString& aName,
                                 SharedWorker** aSharedWorker);
};

} // workerinternals namespace
} // dom namespace
} // mozilla namespace

#endif /* mozilla_dom_workers_runtimeservice_h__ */
