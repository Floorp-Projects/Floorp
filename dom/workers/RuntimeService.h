/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_runtimeservice_h__
#define mozilla_dom_workers_runtimeservice_h__

#include "Workers.h"

#include "nsIObserver.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

class nsIRunnable;
class nsIThread;
class nsITimer;
class nsPIDOMWindow;

BEGIN_WORKERS_NAMESPACE

class SharedWorker;
class WorkerPrivate;

class RuntimeService MOZ_FINAL : public nsIObserver
{
public:
  class WorkerThread;

private:
  struct SharedWorkerInfo
  {
    WorkerPrivate* mWorkerPrivate;
    nsCString mScriptSpec;
    nsCString mName;

    SharedWorkerInfo(WorkerPrivate* aWorkerPrivate,
                     const nsACString& aScriptSpec,
                     const nsACString& aName)
    : mWorkerPrivate(aWorkerPrivate), mScriptSpec(aScriptSpec), mName(aName)
    { }
  };

  struct WorkerDomainInfo
  {
    nsCString mDomain;
    nsTArray<WorkerPrivate*> mActiveWorkers;
    nsTArray<WorkerPrivate*> mQueuedWorkers;
    nsClassHashtable<nsCStringHashKey, SharedWorkerInfo> mSharedWorkerInfos;
    uint32_t mChildWorkerCount;

    WorkerDomainInfo()
    : mActiveWorkers(1), mChildWorkerCount(0)
    { }

    uint32_t
    ActiveWorkerCount() const
    {
      return mActiveWorkers.Length() + mChildWorkerCount;
    }
  };

  struct IdleThreadInfo
  {
    nsRefPtr<WorkerThread> mThread;
    mozilla::TimeStamp mExpirationTime;
  };

  struct MatchSharedWorkerInfo
  {
    WorkerPrivate* mWorkerPrivate;
    SharedWorkerInfo* mSharedWorkerInfo;

    MatchSharedWorkerInfo(WorkerPrivate* aWorkerPrivate)
    : mWorkerPrivate(aWorkerPrivate), mSharedWorkerInfo(nullptr)
    { }
  };

  mozilla::Mutex mMutex;

  // Protected by mMutex.
  nsClassHashtable<nsCStringHashKey, WorkerDomainInfo> mDomainMap;

  // Protected by mMutex.
  nsTArray<IdleThreadInfo> mIdleThreadArray;

  // *Not* protected by mMutex.
  nsClassHashtable<nsPtrHashKey<nsPIDOMWindow>,
                   nsTArray<WorkerPrivate*> > mWindowMap;

  // Only used on the main thread.
  nsCOMPtr<nsITimer> mIdleThreadTimer;

  static JSSettings sDefaultJSSettings;
  static bool sDefaultPreferences[WORKERPREF_COUNT];

public:
  struct NavigatorProperties
  {
    nsString mAppName;
    nsString mAppVersion;
    nsString mPlatform;
    nsString mUserAgent;
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
  RegisterWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  void
  UnregisterWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  void
  CancelWorkersForWindow(nsPIDOMWindow* aWindow);

  void
  SuspendWorkersForWindow(nsPIDOMWindow* aWindow);

  void
  ResumeWorkersForWindow(nsPIDOMWindow* aWindow);

  nsresult
  CreateSharedWorker(const GlobalObject& aGlobal,
                     const nsAString& aScriptURL,
                     const nsACString& aName,
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
  GetDefaultJSSettings(JSSettings& aSettings)
  {
    AssertIsOnMainThread();
    aSettings = sDefaultJSSettings;
  }

  static void
  GetDefaultPreferences(bool aPreferences[WORKERPREF_COUNT])
  {
    AssertIsOnMainThread();
    memcpy(aPreferences, sDefaultPreferences, WORKERPREF_COUNT * sizeof(bool));
  }

  static void
  SetDefaultRuntimeAndContextOptions(
                                    const JS::RuntimeOptions& aRuntimeOptions,
                                    const JS::ContextOptions& aContentCxOptions,
                                    const JS::ContextOptions& aChromeCxOptions)
  {
    AssertIsOnMainThread();
    sDefaultJSSettings.runtimeOptions = aRuntimeOptions;
    sDefaultJSSettings.content.contextOptions = aContentCxOptions;
    sDefaultJSSettings.chrome.contextOptions = aChromeCxOptions;
  }

  void
  UpdateAllWorkerRuntimeAndContextOptions();

  void
  UpdateAllWorkerPreference(WorkerPreference aPref, bool aValue);

  static void
  SetDefaultJSGCSettings(JSGCParamKey aKey, uint32_t aValue)
  {
    AssertIsOnMainThread();
    sDefaultJSSettings.ApplyGCSetting(aKey, aValue);
  }

  void
  UpdateAllWorkerMemoryParameter(JSGCParamKey aKey, uint32_t aValue);

  static uint32_t
  GetContentCloseHandlerTimeoutSeconds()
  {
    return sDefaultJSSettings.content.maxScriptRuntime;
  }

  static uint32_t
  GetChromeCloseHandlerTimeoutSeconds()
  {
    return sDefaultJSSettings.chrome.maxScriptRuntime;
  }

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

private:
  RuntimeService();
  ~RuntimeService();

  nsresult
  Init();

  void
  Shutdown();

  void
  Cleanup();

  static PLDHashOperator
  AddAllTopLevelWorkersToArray(const nsACString& aKey,
                               WorkerDomainInfo* aData,
                               void* aUserArg);

  static PLDHashOperator
  RemoveSharedWorkerFromWindowMap(nsPIDOMWindow* aKey,
                                  nsAutoPtr<nsTArray<WorkerPrivate*> >& aData,
                                  void* aUserArg);

  static PLDHashOperator
  FindSharedWorkerInfo(const nsACString& aKey,
                       SharedWorkerInfo* aData,
                       void* aUserArg);

  void
  GetWorkersForWindow(nsPIDOMWindow* aWindow,
                      nsTArray<WorkerPrivate*>& aWorkers);

  bool
  ScheduleWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  static void
  ShutdownIdleThreads(nsITimer* aTimer, void* aClosure);

  static void
  WorkerPrefChanged(const char* aPrefName, void* aClosure);

  static void
  JSVersionChanged(const char* aPrefName, void* aClosure);
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_runtimeservice_h__ */
