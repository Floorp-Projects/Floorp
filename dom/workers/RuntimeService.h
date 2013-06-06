/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_runtimeservice_h__
#define mozilla_dom_workers_runtimeservice_h__

#include "Workers.h"

#include "nsIObserver.h"

#include "jsapi.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

class nsIThread;
class nsITimer;
class nsPIDOMWindow;

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

class RuntimeService MOZ_FINAL : public nsIObserver
{
  struct WorkerDomainInfo
  {
    nsCString mDomain;
    nsTArray<WorkerPrivate*> mActiveWorkers;
    nsTArray<WorkerPrivate*> mQueuedWorkers;
    uint32_t mChildWorkerCount;

    WorkerDomainInfo() : mActiveWorkers(1), mChildWorkerCount(0) { }

    uint32_t
    ActiveWorkerCount() const
    {
      return mActiveWorkers.Length() + mChildWorkerCount;
    }
  };

  struct IdleThreadInfo
  {
    nsCOMPtr<nsIThread> mThread;
    mozilla::TimeStamp mExpirationTime;
  };

  mozilla::Mutex mMutex;

  // Protected by mMutex.
  nsClassHashtable<nsCStringHashKey, WorkerDomainInfo> mDomainMap;

  // Protected by mMutex.
  nsTArray<IdleThreadInfo> mIdleThreadArray;

  // *Not* protected by mMutex.
  nsClassHashtable<nsPtrHashKey<nsPIDOMWindow>, nsTArray<WorkerPrivate*> > mWindowMap;

  // Only used on the main thread.
  nsCOMPtr<nsITimer> mIdleThreadTimer;

  nsCString mDetectorName;
  nsCString mSystemCharset;

  static uint32_t sDefaultJSContextOptions;
  static uint32_t sDefaultJSRuntimeHeapSize;
  static uint32_t sDefaultJSAllocationThreshold;
  static int32_t sCloseHandlerTimeoutSeconds;

#ifdef JS_GC_ZEAL
  static uint8_t sDefaultGCZeal;
#endif

public:
  struct NavigatorStrings
  {
    nsString mAppName;
    nsString mAppVersion;
    nsString mPlatform;
    nsString mUserAgent;
  };

private:
  NavigatorStrings mNavigatorStrings;

  // True when the observer service holds a reference to this object.
  bool mObserved;
  bool mShuttingDown;
  bool mNavigatorStringsLoaded;

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
  CancelWorkersForWindow(JSContext* aCx, nsPIDOMWindow* aWindow);

  void
  SuspendWorkersForWindow(JSContext* aCx, nsPIDOMWindow* aWindow);

  void
  ResumeWorkersForWindow(nsIScriptContext* aCx, nsPIDOMWindow* aWindow);

  const nsACString&
  GetDetectorName() const
  {
    return mDetectorName;
  }

  const nsACString&
  GetSystemCharset() const
  {
    return mSystemCharset;
  }

  const NavigatorStrings&
  GetNavigatorStrings() const
  {
    return mNavigatorStrings;
  }

  void
  NoteIdleThread(nsIThread* aThread);

  static uint32_t
  GetDefaultJSContextOptions()
  {
    AssertIsOnMainThread();
    return sDefaultJSContextOptions;
  }

  static void
  SetDefaultJSContextOptions(uint32_t aOptions)
  {
    AssertIsOnMainThread();
    sDefaultJSContextOptions = aOptions;
  }

  void
  UpdateAllWorkerJSContextOptions();

  static uint32_t
  GetDefaultJSWorkerMemoryParameter(JSGCParamKey aKey)
  {
    AssertIsOnMainThread();
    switch (aKey) {
      case JSGC_ALLOCATION_THRESHOLD:
        return sDefaultJSAllocationThreshold;
      case JSGC_MAX_BYTES:
        return sDefaultJSRuntimeHeapSize;
      default:
        MOZ_NOT_REACHED("Unknown Worker Memory Parameter.");
    }
  }

  static void
  SetDefaultJSWorkerMemoryParameter(JSGCParamKey aKey, uint32_t aValue)
  {
    AssertIsOnMainThread();
    switch(aKey) {
      case JSGC_ALLOCATION_THRESHOLD:
        sDefaultJSAllocationThreshold = aValue;
        break;
      case JSGC_MAX_BYTES:
        sDefaultJSRuntimeHeapSize = aValue;
        break;
      default:
        MOZ_NOT_REACHED("Unknown Worker Memory Parameter.");
    }
  }

  void
  UpdateAllWorkerMemoryParameter(JSGCParamKey aKey);

  static uint32_t
  GetCloseHandlerTimeoutSeconds()
  {
    return sCloseHandlerTimeoutSeconds > 0 ? sCloseHandlerTimeoutSeconds : 0;
  }

#ifdef JS_GC_ZEAL
  static uint8_t
  GetDefaultGCZeal()
  {
    AssertIsOnMainThread();
    return sDefaultGCZeal;
  }

  static void
  SetDefaultGCZeal(uint8_t aGCZeal)
  {
    AssertIsOnMainThread();
    sDefaultGCZeal = aGCZeal;
  }

  void
  UpdateAllWorkerGCZeal();
#endif

  void
  GarbageCollectAllWorkers(bool aShrinking);

private:
  RuntimeService();
  ~RuntimeService();

  nsresult
  Init();

  void
  Cleanup();

  static PLDHashOperator
  AddAllTopLevelWorkersToArray(const nsACString& aKey,
                               WorkerDomainInfo* aData,
                               void* aUserArg);

  void
  GetWorkersForWindow(nsPIDOMWindow* aWindow,
                      nsTArray<WorkerPrivate*>& aWorkers);

  bool
  ScheduleWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  static void
  ShutdownIdleThreads(nsITimer* aTimer, void* aClosure);
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_runtimeservice_h__ */
