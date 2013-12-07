/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadMonitor.h"
#include "nsString.h"
#include "prlog.h"
#include "prtime.h"
#include "prinrval.h"
#include "prsystem.h"

#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/Util.h"
#include "mozilla/Services.h"

#if defined(ANDROID) || defined(LINUX) || defined(XP_MACOSX)
#include <sys/time.h>
#include <sys/resource.h>
#endif

// NSPR_LOG_MODULES=LoadMonitor:5
PRLogModuleInfo *gLoadMonitorLog = nullptr;
#if defined(PR_LOGGING)
#define LOG(args) PR_LOG(gLoadMonitorLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gLoadMonitorLog, 4)
#define LOG_MANY_ENABLED() PR_LOG_TEST(gLoadMonitorLog, 5)
#else
#define LOG(args)
#define LOG_ENABLED() (false)
#define LOG_MANY_ENABLED() (false)
#endif

using namespace mozilla;

// Update the system load every x milliseconds
static const int kLoadUpdateInterval = 1000;

NS_IMPL_ISUPPORTS1(LoadMonitor, nsIObserver)

LoadMonitor::LoadMonitor()
  : mLock("LoadMonitor.mLock"),
    mCondVar(mLock, "LoadMonitor.mCondVar"),
    mShutdownPending(false),
    mLoadInfoThread(nullptr),
    mSystemLoad(0.0f),
    mProcessLoad(0.0f)
{
}

LoadMonitor::~LoadMonitor()
{
  Shutdown();
}

NS_IMETHODIMP
LoadMonitor::Observe(nsISupports* /* aSubject */,
                     const char*  aTopic,
                     const PRUnichar* /* aData */)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(!strcmp("xpcom-shutdown-threads", aTopic), "Bad topic!");
  Shutdown();
  return NS_OK;
}

class LoadMonitorAddObserver : public nsRunnable
{
public:
  LoadMonitorAddObserver(nsRefPtr<LoadMonitor> loadMonitor)
  {
    mLoadMonitor = loadMonitor;
  }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (!observerService)
      return NS_ERROR_FAILURE;

    nsresult rv = observerService->AddObserver(mLoadMonitor, "xpcom-shutdown-threads", false);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  nsRefPtr<LoadMonitor> mLoadMonitor;
};

class LoadMonitorRemoveObserver : public nsRunnable
{
public:
  LoadMonitorRemoveObserver(nsRefPtr<LoadMonitor> loadMonitor)
  {
    mLoadMonitor = loadMonitor;
  }

  NS_IMETHOD Run()
  {
    // remove xpcom shutdown observer
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

    if (observerService)
      observerService->RemoveObserver(mLoadMonitor, "xpcom-shutdown-threads");

    return NS_OK;
  }

private:
  nsRefPtr<LoadMonitor> mLoadMonitor;
};

void LoadMonitor::Shutdown()
{
  MutexAutoLock lock(mLock);
  if (mLoadInfoThread) {
    mShutdownPending = true;
    mCondVar.Notify();

    mLoadInfoThread = nullptr;

    nsRefPtr<LoadMonitorRemoveObserver> remObsRunner = new LoadMonitorRemoveObserver(this);
    if (!NS_IsMainThread()) {
      NS_DispatchToMainThread(remObsRunner, NS_DISPATCH_NORMAL);
    } else {
      remObsRunner->Run();
    }
  }
}

class LoadStats
{
public:
  LoadStats() :
    mPrevTotalTimes(0),
    mPrevCpuTimes(0),
    mPrevLoad(0) {};

  double GetLoad() { return (double)mPrevLoad; };

  uint64_t mPrevTotalTimes;
  uint64_t mPrevCpuTimes;
  float mPrevLoad;               // Previous load value.
};

class LoadInfo : public mozilla::RefCounted<LoadInfo>
{
public:
  double GetSystemLoad() { return mSystemLoad.GetLoad(); };
  double GetProcessLoad() { return mProcessLoad.GetLoad(); };
  nsresult UpdateSystemLoad();
  nsresult UpdateProcessLoad();

private:
  void UpdateCpuLoad(uint64_t current_total_times,
                     uint64_t current_cpu_times,
                     LoadStats* loadStat);
  LoadStats mSystemLoad;
  LoadStats mProcessLoad;
};

void LoadInfo::UpdateCpuLoad(uint64_t current_total_times,
                             uint64_t current_cpu_times,
                             LoadStats *loadStat) {
  float result = 0.0f;

  if (current_total_times < loadStat->mPrevTotalTimes ||
      current_cpu_times < loadStat->mPrevCpuTimes) {
    //LOG(("Current total: %lld old total: %lld", current_total_times, loadStat->mPrevTotalTimes));
    //LOG(("Current cpu: %lld old cpu: %lld", current_cpu_times, loadStat->mPrevCpuTimes));
    LOG(("Inconsistent time values are passed. ignored"));
  } else {
    const uint64_t cpu_diff = current_cpu_times - loadStat->mPrevCpuTimes;
    const uint64_t total_diff = current_total_times - loadStat->mPrevTotalTimes;
    if (total_diff > 0) {
      result =  (float)cpu_diff / (float)total_diff;
      loadStat->mPrevLoad = result;
    }
  }
  loadStat->mPrevTotalTimes = current_total_times;
  loadStat->mPrevCpuTimes = current_cpu_times;
}

nsresult LoadInfo::UpdateSystemLoad()
{
#if defined(LINUX) || defined(ANDROID)
  nsCOMPtr<nsIFile> procStatFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  procStatFile->InitWithPath(NS_LITERAL_STRING("/proc/stat"));

  nsCOMPtr<nsIInputStream> fileInputStream;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream),
                                           procStatFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString buffer;
  bool isMore = true;
  lineInputStream->ReadLine(buffer, &isMore);

  uint64_t user;
  uint64_t nice;
  uint64_t system;
  uint64_t idle;
  if (sscanf(buffer.get(), "cpu %Lu %Lu %Lu %Lu",
             &user, &nice,
             &system, &idle) != 4) {
    LOG(("Error parsing /proc/stat"));
    return NS_ERROR_FAILURE;
  }

  const uint64_t cpu_times = nice + system + user;
  const uint64_t total_times = cpu_times + idle;

  UpdateCpuLoad(total_times,
                cpu_times * PR_GetNumberOfProcessors(),
                &mSystemLoad);
  return NS_OK;
#else
  // Not implemented
  return NS_OK;
#endif
}

nsresult LoadInfo::UpdateProcessLoad() {
#if defined(LINUX) || defined(ANDROID)
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  const uint64_t total_times = tv.tv_sec * PR_USEC_PER_SEC + tv.tv_usec;

  rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) < 0) {
    LOG(("getrusage failed"));
    return NS_ERROR_FAILURE;
  }

  const uint64_t cpu_times =
      (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * PR_USEC_PER_SEC +
       usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;

  UpdateCpuLoad(total_times,
                cpu_times,
                &mProcessLoad);
#endif // defined(LINUX) || defined(ANDROID)
  return NS_OK;
}

class LoadInfoCollectRunner : public nsRunnable
{
public:
  LoadInfoCollectRunner(nsRefPtr<LoadMonitor> loadMonitor)
  {
    mLoadMonitor = loadMonitor;
    mLoadInfo = new LoadInfo();
    mLoadNoiseCounter = 0;
  }

  NS_IMETHOD Run()
  {
    MutexAutoLock lock(mLoadMonitor->mLock);
    while (!mLoadMonitor->mShutdownPending) {
      mLoadInfo->UpdateSystemLoad();
      mLoadInfo->UpdateProcessLoad();
      float sysLoad = mLoadInfo->GetSystemLoad();
      float procLoad = mLoadInfo->GetProcessLoad();
      if ((++mLoadNoiseCounter % (LOG_MANY_ENABLED() ? 1 : 10)) == 0) {
        LOG(("System Load: %f Process Load: %f", sysLoad, procLoad));
        mLoadNoiseCounter = 0;
      }
      mLoadMonitor->SetSystemLoad(sysLoad);
      mLoadMonitor->SetProcessLoad(procLoad);

      mLoadMonitor->mCondVar.Wait(PR_MillisecondsToInterval(kLoadUpdateInterval));
    }
    return NS_OK;
  }

private:
  RefPtr<LoadInfo> mLoadInfo;
  nsRefPtr<LoadMonitor> mLoadMonitor;
  int mLoadNoiseCounter;
};

void
LoadMonitor::SetProcessLoad(float load) {
  mLock.AssertCurrentThreadOwns();
  mProcessLoad = load;
}

void
LoadMonitor::SetSystemLoad(float load) {
  mLock.AssertCurrentThreadOwns();
  mSystemLoad = load;
}

float
LoadMonitor::GetProcessLoad() {
  MutexAutoLock lock(mLock);
  float load = mProcessLoad;
  return load;
}

float
LoadMonitor::GetSystemLoad() {
  MutexAutoLock lock(mLock);
  float load = mSystemLoad;
  return load;
}

nsresult
LoadMonitor::Init(nsRefPtr<LoadMonitor> &self)
{
#if defined(PR_LOGGING)
  if (!gLoadMonitorLog)
    gLoadMonitorLog = PR_NewLogModule("LoadMonitor");
  LOG(("Initializing LoadMonitor"));
#endif

#if defined(ANDROID) || defined(LINUX)
  nsRefPtr<LoadMonitorAddObserver> addObsRunner = new LoadMonitorAddObserver(self);
  NS_DispatchToMainThread(addObsRunner, NS_DISPATCH_NORMAL);

  NS_NewNamedThread("Sys Load Info", getter_AddRefs(mLoadInfoThread));

  nsRefPtr<LoadInfoCollectRunner> runner = new LoadInfoCollectRunner(self);
  mLoadInfoThread->Dispatch(runner, NS_DISPATCH_NORMAL);
#endif

  return NS_OK;
}
