/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadMonitor.h"
#include "LoadManager.h"
#include "nsString.h"
#include "mozilla/Logging.h"
#include "prtime.h"
#include "prinrval.h"
#include "prsystem.h"
#include "prprf.h"

#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/Services.h"

#ifdef XP_UNIX
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifdef XP_MACOSX
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/host_info.h>
#endif

#if defined(__DragonFly__) || defined(__FreeBSD__) \
 || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sysctl.h>
# if defined(__OpenBSD__)
#define KERN_CP_TIME KERN_CPTIME
# endif
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sched.h>
#endif

#ifdef XP_WIN
#include <pdh.h>
#include <tchar.h>
#pragma comment(lib, "pdh.lib")
#endif

// NSPR_LOG_MODULES=LoadManager:5
#undef LOG
#undef LOG_ENABLED
#define LOG(args) MOZ_LOG(gLoadManagerLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gLoadManagerLog, mozilla::LogLevel::Debug)
#define LOG_MANY_ENABLED() MOZ_LOG_TEST(gLoadManagerLog, mozilla::LogLevel::Verbose)

namespace mozilla {

NS_IMPL_ISUPPORTS(LoadMonitor, nsIObserver)

LoadMonitor::LoadMonitor(int aLoadUpdateInterval)
  : mLoadUpdateInterval(aLoadUpdateInterval),
    mLock("LoadMonitor.mLock"),
    mCondVar(mLock, "LoadMonitor.mCondVar"),
    mShutdownPending(false),
    mLoadInfoThread(nullptr),
    mSystemLoad(0.0f),
    mProcessLoad(0.0f),
    mLoadNotificationCallback(nullptr)
{
}

LoadMonitor::~LoadMonitor()
{
  Shutdown();
}

NS_IMETHODIMP
LoadMonitor::Observe(nsISupports* /* aSubject */,
                     const char*  aTopic,
                     const char16_t* /* aData */)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(!strcmp("xpcom-shutdown-threads", aTopic), "Bad topic!");
  Shutdown();
  return NS_OK;
}

class LoadMonitorAddObserver : public nsRunnable
{
public:
  explicit LoadMonitorAddObserver(nsRefPtr<LoadMonitor> loadMonitor)
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
  explicit LoadMonitorRemoveObserver(nsRefPtr<LoadMonitor> loadMonitor)
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
  if (mLoadInfoThread) {
    {
      MutexAutoLock lock(mLock);
      LOG(("LoadMonitor: shutting down"));
      mShutdownPending = true;
      mCondVar.Notify();
    }

    // Note: can't just call ->Shutdown() from here; that spins the event
    // loop here, causing re-entrancy issues if we're invoked from cycle
    // collection.  Argh.
    mLoadInfoThread = nullptr;

    nsRefPtr<LoadMonitorRemoveObserver> remObsRunner = new LoadMonitorRemoveObserver(this);
    if (!NS_IsMainThread()) {
      NS_DispatchToMainThread(remObsRunner);
    } else {
      remObsRunner->Run();
    }
  }
}

#ifdef XP_WIN
static LPCTSTR TotalCounterPath = _T("\\Processor(_Total)\\% Processor Time");

class WinProcMon
{
public:
  WinProcMon():
    mQuery(0), mCounter(0) {};
  ~WinProcMon();
  nsresult Init();
  nsresult QuerySystemLoad(float* load_percent);
  static const uint64_t TicksPerSec = 10000000; //100nsec tick (10MHz)
private:
  PDH_HQUERY mQuery;
  PDH_HCOUNTER mCounter;
};

WinProcMon::~WinProcMon()
{
  if (mQuery != 0) {
    PdhCloseQuery(mQuery);
    mQuery = 0;
  }
}

nsresult
WinProcMon::Init()
{
  PDH_HQUERY query;
  PDH_HCOUNTER counter;

  // Get a query handle to the Performance Data Helper
  PDH_STATUS status = PdhOpenQuery(
                        NULL,      // No log file name: use real-time source
                        0,         // zero out user data token: unsued
                        &query);

  if (status != ERROR_SUCCESS) {
    LOG(("PdhOpenQuery error = %X", status));
    return NS_ERROR_FAILURE;
  }

  // Add a pre-defined high performance counter to the query.
  // This one is for the total CPU usage.
  status = PdhAddCounter(query, TotalCounterPath, 0, &counter);

  if (status != ERROR_SUCCESS) {
    PdhCloseQuery(query);
    LOG(("PdhAddCounter (_Total) error = %X", status));
    return NS_ERROR_FAILURE;
  }

  // Need to make an initial query call to set up data capture.
  status = PdhCollectQueryData(query);

  if (status != ERROR_SUCCESS) {
    PdhCloseQuery(query);
    LOG(("PdhCollectQueryData (init) error = %X", status));
    return NS_ERROR_FAILURE;
  }

  mQuery = query;
  mCounter = counter;
  return NS_OK;
}

nsresult WinProcMon::QuerySystemLoad(float* load_percent)
{
  *load_percent = 0;

  if (mQuery == 0) {
    return NS_ERROR_FAILURE;
  }

  // Update all counters associated with this query object.
  PDH_STATUS status = PdhCollectQueryData(mQuery);

  if (status != ERROR_SUCCESS) {
    LOG(("PdhCollectQueryData error = %X", status));
    return NS_ERROR_FAILURE;
  }

  PDH_FMT_COUNTERVALUE counter;
  // maximum is 100% regardless of CPU core count.
  status = PdhGetFormattedCounterValue(
               mCounter,
               PDH_FMT_DOUBLE,
               (LPDWORD)NULL,
               &counter);

  if (ERROR_SUCCESS != status ||
      // There are multiple success return values.
      !IsSuccessSeverity(counter.CStatus)) {
    LOG(("PdhGetFormattedCounterValue error"));
    return NS_ERROR_FAILURE;
  }

  // The result is a percent value, reduce to match expected scale.
  *load_percent = (float)(counter.doubleValue / 100.0f);
  return NS_OK;
}
#endif

// Use a non-generic class name, because otherwise we can get name collisions
// with other classes in the codebase.  The normal way of dealing with that is
// to put the class in an anonymous namespace, but this class is used as a
// member of RTCLoadInfo, which can't be in the anonymous namespace, so it also
// can't be in an anonymous namespace: gcc warns about that setup and this
// directory is fail-on-warnings.
class RTCLoadStats
{
public:
  RTCLoadStats() :
    mPrevTotalTimes(0),
    mPrevCpuTimes(0),
    mPrevLoad(0) {};

  double GetLoad() { return (double)mPrevLoad; };

  uint64_t mPrevTotalTimes;
  uint64_t mPrevCpuTimes;
  float mPrevLoad;               // Previous load value.
};

// Use a non-generic class name, because otherwise we can get name collisions
// with other classes in the codebase.  The normal way of dealing with that is
// to put the class in an anonymous namespace, but this class is used as a
// member of LoadInfoCollectRunner, which can't be in the anonymous namespace,
// so it also can't be in an anonymous namespace: gcc warns about that setup
// and this directory is fail-on-warnings.
class RTCLoadInfo final
{
private:
  ~RTCLoadInfo() {}

public:
  NS_INLINE_DECL_REFCOUNTING(RTCLoadInfo)

  RTCLoadInfo(): mLoadUpdateInterval(0) {};
  nsresult Init(int aLoadUpdateInterval);
  double GetSystemLoad() { return mSystemLoad.GetLoad(); };
  double GetProcessLoad() { return mProcessLoad.GetLoad(); };
  nsresult UpdateSystemLoad();
  nsresult UpdateProcessLoad();

private:
  void UpdateCpuLoad(uint64_t ticks_per_interval,
                     uint64_t current_total_times,
                     uint64_t current_cpu_times,
                     RTCLoadStats* loadStat);
#ifdef XP_WIN
  WinProcMon mSysMon;
  HANDLE mProcHandle;
  int mNumProcessors;
#endif
  RTCLoadStats mSystemLoad;
  RTCLoadStats mProcessLoad;
  uint64_t mTicksPerInterval;
  int mLoadUpdateInterval;
};

nsresult RTCLoadInfo::Init(int aLoadUpdateInterval)
{
  mLoadUpdateInterval = aLoadUpdateInterval;
#ifdef XP_WIN
  mTicksPerInterval = (WinProcMon::TicksPerSec /*Hz*/
                       * mLoadUpdateInterval /*msec*/) / 1000 ;
  mNumProcessors = PR_GetNumberOfProcessors();
  mProcHandle = GetCurrentProcess();
  return mSysMon.Init();
#else
  mTicksPerInterval = (sysconf(_SC_CLK_TCK) * mLoadUpdateInterval) / 1000;
  return NS_OK;
#endif
}

void RTCLoadInfo::UpdateCpuLoad(uint64_t ticks_per_interval,
                                uint64_t current_total_times,
                                uint64_t current_cpu_times,
                                RTCLoadStats *loadStat) {
  // Check if we get an inconsistent number of ticks.
  if (((current_total_times - loadStat->mPrevTotalTimes)
       > (ticks_per_interval * 10))
      || current_total_times < loadStat->mPrevTotalTimes
      || current_cpu_times < loadStat->mPrevCpuTimes) {
    // Bug at least on the Nexus 4 and Galaxy S4
    // https://code.google.com/p/android/issues/detail?id=41630
    // We do need to update our previous times, or we can get stuck
    // when there is a blip upwards and then we get a bunch of consecutive
    // lower times. Just skip the load calculation.
    LOG(("Inconsistent time values are passed. ignored"));
    // Try to recover next tick
    loadStat->mPrevTotalTimes = current_total_times;
    loadStat->mPrevCpuTimes = current_cpu_times;
    return;
  }

  const uint64_t cpu_diff = current_cpu_times - loadStat->mPrevCpuTimes;
  const uint64_t total_diff = current_total_times - loadStat->mPrevTotalTimes;
  if (total_diff > 0) {
#ifdef XP_WIN
    float result =  (float)cpu_diff / (float)total_diff/ (float)mNumProcessors;
#else
    float result =  (float)cpu_diff / (float)total_diff;
#endif
    loadStat->mPrevLoad = result;
  }
  loadStat->mPrevTotalTimes = current_total_times;
  loadStat->mPrevCpuTimes = current_cpu_times;
}

nsresult RTCLoadInfo::UpdateSystemLoad()
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
  if (PR_sscanf(buffer.get(), "cpu %llu %llu %llu %llu",
                &user, &nice,
                &system, &idle) != 4) {
    LOG(("Error parsing /proc/stat"));
    return NS_ERROR_FAILURE;
  }

  const uint64_t cpu_times = nice + system + user;
  const uint64_t total_times = cpu_times + idle;

  UpdateCpuLoad(mTicksPerInterval,
                total_times,
                cpu_times,
                &mSystemLoad);
  return NS_OK;
#elif defined(XP_MACOSX)
  mach_msg_type_number_t info_cnt = HOST_CPU_LOAD_INFO_COUNT;
  host_cpu_load_info_data_t load_info;
  kern_return_t rv = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                                     (host_info_t)(&load_info), &info_cnt);

  if (rv != KERN_SUCCESS || info_cnt != HOST_CPU_LOAD_INFO_COUNT) {
    LOG(("Error from mach/host_statistics call"));
    return NS_ERROR_FAILURE;
  }

  const uint64_t cpu_times = load_info.cpu_ticks[CPU_STATE_NICE]
                           + load_info.cpu_ticks[CPU_STATE_SYSTEM]
                           + load_info.cpu_ticks[CPU_STATE_USER];
  const uint64_t total_times = cpu_times + load_info.cpu_ticks[CPU_STATE_IDLE];

  UpdateCpuLoad(mTicksPerInterval,
                total_times,
                cpu_times,
                &mSystemLoad);
  return NS_OK;
#elif defined(__DragonFly__) || defined(__FreeBSD__) \
   || defined(__NetBSD__) || defined(__OpenBSD__)
#if defined(__NetBSD__)
  uint64_t cp_time[CPUSTATES];
#else
  long cp_time[CPUSTATES];
#endif // __NetBSD__
  size_t sz = sizeof(cp_time);
#ifdef KERN_CP_TIME
  int mib[] = {
    CTL_KERN,
    KERN_CP_TIME,
  };
  u_int miblen = sizeof(mib) / sizeof(mib[0]);
  if (sysctl(mib, miblen, &cp_time, &sz, nullptr, 0)) {
#else
  if (sysctlbyname("kern.cp_time", &cp_time, &sz, nullptr, 0)) {
#endif // KERN_CP_TIME
    LOG(("sysctl kern.cp_time failed"));
    return NS_ERROR_FAILURE;
  }

  const uint64_t cpu_times = cp_time[CP_NICE]
                           + cp_time[CP_SYS]
                           + cp_time[CP_INTR]
                           + cp_time[CP_USER];
  const uint64_t total_times = cpu_times + cp_time[CP_IDLE];

  UpdateCpuLoad(mTicksPerInterval,
                total_times,
                cpu_times,
                &mSystemLoad);
  return NS_OK;
#elif defined(XP_WIN)
  float load;
  nsresult rv = mSysMon.QuerySystemLoad(&load);

  if (rv == NS_OK) {
    mSystemLoad.mPrevLoad = load;
  }

  return rv;
#else
  // Not implemented
  return NS_OK;
#endif
}

nsresult RTCLoadInfo::UpdateProcessLoad() {
#if defined(XP_UNIX)
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

  UpdateCpuLoad(PR_USEC_PER_MSEC * mLoadUpdateInterval,
                total_times,
                cpu_times,
                &mProcessLoad);
#elif defined(XP_WIN)
  FILETIME clk_time, sys_time, user_time;
  uint64_t total_times, cpu_times;

  GetSystemTimeAsFileTime(&clk_time);
  total_times = (((uint64_t)clk_time.dwHighDateTime) << 32)
                + (uint64_t)clk_time.dwLowDateTime;
  BOOL ok = GetProcessTimes(mProcHandle, &clk_time, &clk_time, &sys_time, &user_time);

  if (ok == 0) {
    return NS_ERROR_FAILURE;
  }

  cpu_times = (((uint64_t)sys_time.dwHighDateTime
                + (uint64_t)user_time.dwHighDateTime) << 32)
              + (uint64_t)sys_time.dwLowDateTime
              + (uint64_t)user_time.dwLowDateTime;

  UpdateCpuLoad(mTicksPerInterval,
                total_times,
                cpu_times,
                &mProcessLoad);
#endif
  return NS_OK;
}

// Note: This class can't be in the anonymous namespace, because then we can't
// declare it as a friend of LoadMonitor.
class LoadInfoCollectRunner : public nsRunnable
{
public:
  LoadInfoCollectRunner(nsRefPtr<LoadMonitor> loadMonitor,
                        RefPtr<RTCLoadInfo> loadInfo,
                        nsIThread *loadInfoThread)
    : mThread(loadInfoThread),
      mLoadUpdateInterval(loadMonitor->mLoadUpdateInterval),
      mLoadNoiseCounter(0)
  {
    mLoadMonitor = loadMonitor;
    mLoadInfo = loadInfo;
  }

  NS_IMETHOD Run()
  {
    if (NS_IsMainThread()) {
      if (mThread) {
        // Don't leak threads!
        mThread->Shutdown(); // can't Shutdown from the thread itself, darn
        // Don't null out mThread!
        // See bug 999104.  We must hold a ref to the thread across Dispatch()
        // since the internal mThread ref could be released while processing
        // the Dispatch(), and Dispatch/PutEvent itself doesn't hold a ref; it
        // assumes the caller does.
      }
      return NS_OK;
    }

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
      mLoadMonitor->FireCallbacks();

      mLoadMonitor->mCondVar.Wait(PR_MillisecondsToInterval(mLoadUpdateInterval));
    }
    // ok, we need to exit safely and can't shut ourselves down (DARN)
    NS_DispatchToMainThread(this);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIThread> mThread;
  RefPtr<RTCLoadInfo> mLoadInfo;
  nsRefPtr<LoadMonitor> mLoadMonitor;
  int mLoadUpdateInterval;
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

void
LoadMonitor::FireCallbacks() {
  if (mLoadNotificationCallback) {
    mLoadNotificationCallback->LoadChanged(mSystemLoad, mProcessLoad);
  }
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
  LOG(("Initializing LoadMonitor"));

  RefPtr<RTCLoadInfo> load_info = new RTCLoadInfo();
  nsresult rv = load_info->Init(mLoadUpdateInterval);

  if (NS_FAILED(rv)) {
    LOG(("RTCLoadInfo::Init error"));
    return rv;
  }

  nsRefPtr<LoadMonitorAddObserver> addObsRunner = new LoadMonitorAddObserver(self);
  NS_DispatchToMainThread(addObsRunner);

  NS_NewNamedThread("Sys Load Info", getter_AddRefs(mLoadInfoThread));

  nsRefPtr<LoadInfoCollectRunner> runner =
    new LoadInfoCollectRunner(self, load_info, mLoadInfoThread);
  mLoadInfoThread->Dispatch(runner, NS_DISPATCH_NORMAL);

  return NS_OK;
}

void
LoadMonitor::SetLoadChangeCallback(LoadNotificationCallback* aCallback)
{
  mLoadNotificationCallback = aCallback;
}

}
