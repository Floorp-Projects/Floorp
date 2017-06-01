/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrashReporterHost.h"
#include "CrashReporterMetadataShmem.h"
#include "mozilla/Sprintf.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#ifdef MOZ_CRASHREPORTER
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

  nsCOMPtr<nsICrashService> crashService =
    do_GetService("@mozilla.org/crashservice;1");
  if (!crashService) {
    return;
  }

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

  nsCOMPtr<nsISupports> promise;
  crashService->AddCrash(processType, crashType, aChildDumpID, getter_AddRefs(promise));
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
