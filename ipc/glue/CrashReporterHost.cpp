/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrashReporterHost.h"
#include "CrashReporterMetadataShmem.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Sprintf.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#ifdef MOZ_CRASHREPORTER
# include "nsIAsyncShutdown.h"
# include "nsICrashService.h"
#endif

namespace mozilla {
namespace ipc {

CrashReporterHost::CrashReporterHost(GeckoProcessType aProcessType,
                                     const Shmem& aShmem,
                                     CrashReporter::ThreadId aThreadId)
 : mProcessType(aProcessType),
   mShmem(aShmem),
   mThreadId(aThreadId),
   mStartTime(::time(nullptr)),
   mFinalized(false)
{
}

#ifdef MOZ_CRASHREPORTER
bool
CrashReporterHost::GenerateCrashReport(base::ProcessId aPid)
{
  if (!TakeCrashedChildMinidump(aPid, nullptr)) {
    return false;
  }
  return FinalizeCrashReport();
}

RefPtr<nsIFile>
CrashReporterHost::TakeCrashedChildMinidump(base::ProcessId aPid, uint32_t* aOutSequence)
{
  MOZ_ASSERT(!HasMinidump());

  RefPtr<nsIFile> crashDump;
  if (!XRE_TakeMinidumpForChild(aPid, getter_AddRefs(crashDump), aOutSequence)) {
    return nullptr;
  }
  if (!AdoptMinidump(crashDump)) {
    return nullptr;
  }
  return crashDump.get();
}

bool
CrashReporterHost::AdoptMinidump(nsIFile* aFile)
{
  return CrashReporter::GetIDFromMinidump(aFile, mDumpID);
}

bool
CrashReporterHost::FinalizeCrashReport()
{
  MOZ_ASSERT(!mFinalized);
  MOZ_ASSERT(HasMinidump());

  CrashReporter::AnnotationTable notes;

  nsAutoCString type;
  switch (mProcessType) {
    case GeckoProcessType_Content:
      type = NS_LITERAL_CSTRING("content");
      break;
    case GeckoProcessType_Plugin:
    case GeckoProcessType_GMPlugin:
      type = NS_LITERAL_CSTRING("plugin");
      break;
    case GeckoProcessType_GPU:
      type = NS_LITERAL_CSTRING("gpu");
      break;
    default:
      NS_ERROR("unknown process type");
      break;
  }
  notes.Put(NS_LITERAL_CSTRING("ProcessType"), type);

  char startTime[32];
  SprintfLiteral(startTime, "%lld", static_cast<long long>(mStartTime));
  notes.Put(NS_LITERAL_CSTRING("StartupTime"), nsDependentCString(startTime));

  // We might not have shmem (for example, when running crashreporter tests).
  if (mShmem.IsReadable()) {
    CrashReporterMetadataShmem::ReadAppNotes(mShmem, &notes);
  }
  CrashReporter::AppendExtraData(mDumpID, mExtraNotes);
  CrashReporter::AppendExtraData(mDumpID, notes);

  // Use mExtraNotes, since NotifyCrashService looks for "PluginHang" which is
  // set in the parent process.
  NotifyCrashService(mProcessType, mDumpID, &mExtraNotes);

  mFinalized = true;
  return true;
}

/**
 * Runnable used to execute the minidump analyzer program asynchronously after
 * a crash. This should run on a background thread not to block the main thread
 * over the potentially long minidump analysis. Once analysis is over, the
 * crash information will be relayed to the crash manager via another runnable
 * sent to the main thread. Shutdown will be blocked for the duration of the
 * entire process to ensure this information is sent.
 */
class AsyncMinidumpAnalyzer final : public nsIRunnable
                                  , public nsIAsyncShutdownBlocker
{
public:
  /**
   * Create a new minidump analyzer runnable, this will also block shutdown
   * until the associated crash has been added to the crash manager.
   */
  AsyncMinidumpAnalyzer(int32_t aProcessType,
                        int32_t aCrashType,
                        const nsString& aChildDumpID)
    : mProcessType(aProcessType)
    , mCrashType(aCrashType)
    , mChildDumpID(aChildDumpID)
    , mName(NS_LITERAL_STRING("Crash reporter: blocking on minidump analysis"))
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsresult rv = GetShutdownBarrier()->AddBlocker(
      this, NS_LITERAL_STRING(__FILE__), __LINE__,
      NS_LITERAL_STRING("Minidump analysis"));
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());

    if (mProcessType == nsICrashService::PROCESS_TYPE_CONTENT ||
        mProcessType == nsICrashService::PROCESS_TYPE_GPU) {
      CrashReporter::RunMinidumpAnalyzer(mChildDumpID);
    }

    // Make a copy of these so we can copy them into the runnable lambda
    int32_t processType = mProcessType;
    int32_t crashType = mCrashType;
    nsString childDumpID(mChildDumpID);
    nsCOMPtr<nsIAsyncShutdownBlocker> self(this);

    NS_DispatchToMainThread(NS_NewRunnableFunction([
      self, processType, crashType, childDumpID
    ] {
      nsCOMPtr<nsICrashService> crashService =
        do_GetService("@mozilla.org/crashservice;1");
      if (crashService) {
        crashService->AddCrash(processType, crashType, childDumpID);
      }

      nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();

      if (barrier) {
        barrier->RemoveBlocker(self);
      }
    }));

    return NS_OK;
  }

  NS_IMETHOD BlockShutdown(nsIAsyncShutdownClient* aBarrierClient) override
  {
    return NS_OK;
  }

  NS_IMETHOD GetName(nsAString& aName) override
  {
    aName = mName;
    return NS_OK;
  }

  NS_IMETHOD GetState(nsIPropertyBag**) override
  {
    return NS_OK;
  }

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  ~AsyncMinidumpAnalyzer() {}

  // Returns the profile-before-change shutdown barrier
  static nsCOMPtr<nsIAsyncShutdownClient> GetShutdownBarrier()
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdown();
    nsCOMPtr<nsIAsyncShutdownClient> barrier;
    nsresult rv = svc->GetProfileBeforeChange(getter_AddRefs(barrier));

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    return barrier.forget();
  }

  int32_t mProcessType;
  int32_t mCrashType;
  const nsString mChildDumpID;
  const nsString mName;
};

NS_IMPL_ISUPPORTS(AsyncMinidumpAnalyzer, nsIRunnable, nsIAsyncShutdownBlocker)

/**
 * Add information about a crash to the crash manager. This method runs the
 * various activities required to gather additional information and notify the
 * crash manager asynchronously, since many of them involve I/O or potentially
 * long processing.
 *
 * @param aProcessType The type of process that crashed
 * @param aCrashType The type of crash (crash or hang)
 * @param aChildDumpID A string holding the ID of the dump associated with this
 *        crash
 */
/* static */ void
CrashReporterHost::AsyncAddCrash(int32_t aProcessType,
                                 int32_t aCrashType,
                                 const nsString& aChildDumpID)
{
  MOZ_ASSERT(NS_IsMainThread());

  static StaticRefPtr<LazyIdleThread> sBackgroundThread;

  if (!sBackgroundThread) {
    // Background thread used for running minidump analysis. It will be
    // destroyed immediately after it's done with the task.
    sBackgroundThread =
      new LazyIdleThread(0, NS_LITERAL_CSTRING("CrashReporterHost"));
    ClearOnShutdown(&sBackgroundThread);
  }

  RefPtr<AsyncMinidumpAnalyzer> task =
    new AsyncMinidumpAnalyzer(aProcessType, aCrashType, aChildDumpID);

  Unused << sBackgroundThread->Dispatch(task, NS_DISPATCH_NORMAL);
}

/* static */ void
CrashReporterHost::NotifyCrashService(GeckoProcessType aProcessType,
                                      const nsString& aChildDumpID,
                                      const AnnotationTable* aNotes)
{
  if (!NS_IsMainThread()) {
    RefPtr<Runnable> runnable = NS_NewRunnableFunction([=] () -> void {
      CrashReporterHost::NotifyCrashService(aProcessType, aChildDumpID, aNotes);
    });
    RefPtr<nsIThread> mainThread = do_GetMainThread();
    SyncRunnable::DispatchToThread(mainThread, runnable);
    return;
  }

  MOZ_ASSERT(!aChildDumpID.IsEmpty());

  int32_t processType;
  int32_t crashType = nsICrashService::CRASH_TYPE_CRASH;

  nsCString telemetryKey;

  switch (aProcessType) {
    case GeckoProcessType_Content:
      processType = nsICrashService::PROCESS_TYPE_CONTENT;
      telemetryKey.AssignLiteral("content");
      break;
    case GeckoProcessType_Plugin: {
      processType = nsICrashService::PROCESS_TYPE_PLUGIN;
      telemetryKey.AssignLiteral("plugin");
      nsAutoCString val;
      if (aNotes->Get(NS_LITERAL_CSTRING("PluginHang"), &val) &&
        val.Equals(NS_LITERAL_CSTRING("1"))) {
        crashType = nsICrashService::CRASH_TYPE_HANG;
        telemetryKey.AssignLiteral("pluginhang");
      }
      break;
    }
    case GeckoProcessType_GMPlugin:
      processType = nsICrashService::PROCESS_TYPE_GMPLUGIN;
      telemetryKey.AssignLiteral("gmplugin");
      break;
    case GeckoProcessType_GPU:
      processType = nsICrashService::PROCESS_TYPE_GPU;
      telemetryKey.AssignLiteral("gpu");
      break;
    default:
      NS_ERROR("unknown process type");
      return;
  }

  AsyncAddCrash(processType, crashType, aChildDumpID);
  Telemetry::Accumulate(Telemetry::SUBPROCESS_CRASHES_WITH_DUMP, telemetryKey, 1);
}

void
CrashReporterHost::AddNote(const nsCString& aKey, const nsCString& aValue)
{
  mExtraNotes.Put(aKey, aValue);
}
#endif

} // namespace ipc
} // namespace mozilla
