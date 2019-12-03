/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrashReporterHost.h"
#include "CrashReporterMetadataShmem.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/recordreplay/ParentIPC.h"
#include "mozilla/Sprintf.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "nsIAsyncShutdown.h"
#include "nsICrashService.h"
#include "nsXULAppAPI.h"

// Consistency checking for nsICrashService constants.  We depend on the
// equivalence between nsICrashService values and GeckoProcessType values
// in the code below.  Making them equal also ensures that if new process
// types are added, people will know they may need to add crash reporting
// support in various places because compilation errors will be triggered here.
static_assert(nsICrashService::PROCESS_TYPE_MAIN ==
                  (int)GeckoProcessType_Default,
              "GeckoProcessType enum is out of sync with nsICrashService!");
static_assert(nsICrashService::PROCESS_TYPE_PLUGIN ==
                  (int)GeckoProcessType_Plugin,
              "GeckoProcessType enum is out of sync with nsICrashService!");
static_assert(nsICrashService::PROCESS_TYPE_CONTENT ==
                  (int)GeckoProcessType_Content,
              "GeckoProcessType enum is out of sync with nsICrashService!");
static_assert(nsICrashService::PROCESS_TYPE_IPDLUNITTEST ==
                  (int)GeckoProcessType_IPDLUnitTest,
              "GeckoProcessType enum is out of sync with nsICrashService!");
static_assert(nsICrashService::PROCESS_TYPE_GMPLUGIN ==
                  (int)GeckoProcessType_GMPlugin,
              "GeckoProcessType enum is out of sync with nsICrashService!");
static_assert(nsICrashService::PROCESS_TYPE_GPU == (int)GeckoProcessType_GPU,
              "GeckoProcessType enum is out of sync with nsICrashService!");
static_assert(nsICrashService::PROCESS_TYPE_VR == (int)GeckoProcessType_VR,
              "GeckoProcessType enum is out of sync with nsICrashService!");
static_assert(nsICrashService::PROCESS_TYPE_RDD == (int)GeckoProcessType_RDD,
              "GeckoProcessType enum is out of sync with nsICrashService!");
static_assert(nsICrashService::PROCESS_TYPE_SOCKET ==
                  (int)GeckoProcessType_Socket,
              "GeckoProcessType enum is out of sync with nsICrashService!");
static_assert(nsICrashService::PROCESS_TYPE_SANDBOX_BROKER ==
                  (int)GeckoProcessType_RemoteSandboxBroker,
              "GeckoProcessType enum is out of sync with nsICrashService!");
// Add new static asserts here if you add more process types.
// Update this static assert as well.
static_assert(nsICrashService::PROCESS_TYPE_SANDBOX_BROKER + 1 ==
                  (int)GeckoProcessType_End,
              "GeckoProcessType enum is out of sync with nsICrashService!");

namespace mozilla {
namespace ipc {

CrashReporterHost::CrashReporterHost(GeckoProcessType aProcessType,
                                     const Shmem& aShmem,
                                     CrashReporter::ThreadId aThreadId)
    : mProcessType(aProcessType),
      mShmem(aShmem),
      mThreadId(aThreadId),
      mStartTime(::time(nullptr)),
      mFinalized(false) {}

bool CrashReporterHost::GenerateCrashReport(base::ProcessId aPid) {
  if (!TakeCrashedChildMinidump(aPid, nullptr)) {
    return false;
  }
  return FinalizeCrashReport();
}

RefPtr<nsIFile> CrashReporterHost::TakeCrashedChildMinidump(
    base::ProcessId aPid, uint32_t* aOutSequence) {
  CrashReporter::AnnotationTable annotations;
  MOZ_ASSERT(!HasMinidump());

  RefPtr<nsIFile> crashDump;
  if (!CrashReporter::TakeMinidumpForChild(aPid, getter_AddRefs(crashDump),
                                           annotations, aOutSequence)) {
    return nullptr;
  }
  if (!AdoptMinidump(crashDump, annotations)) {
    return nullptr;
  }
  return crashDump;
}

bool CrashReporterHost::AdoptMinidump(nsIFile* aFile,
                                      const AnnotationTable& aAnnotations) {
  if (!CrashReporter::GetIDFromMinidump(aFile, mDumpID)) {
    return false;
  }

  MergeCrashAnnotations(mExtraAnnotations, aAnnotations);
  return true;
}

int32_t CrashReporterHost::GetCrashType() {
  if (mExtraAnnotations[CrashReporter::Annotation::RecordReplayHang]
          .EqualsLiteral("1")) {
    return nsICrashService::CRASH_TYPE_HANG;
  }

  if (mExtraAnnotations[CrashReporter::Annotation::PluginHang].EqualsLiteral(
          "1")) {
    return nsICrashService::CRASH_TYPE_HANG;
  }

  return nsICrashService::CRASH_TYPE_CRASH;
}

bool CrashReporterHost::FinalizeCrashReport() {
  MOZ_ASSERT(!mFinalized);
  MOZ_ASSERT(HasMinidump());

  CrashReporter::AnnotationTable annotations;

  annotations[CrashReporter::Annotation::ProcessType] =
      XRE_ChildProcessTypeToAnnotation(mProcessType);

  char startTime[32];
  SprintfLiteral(startTime, "%lld", static_cast<long long>(mStartTime));
  annotations[CrashReporter::Annotation::StartupTime] =
      nsDependentCString(startTime);

  // We might not have shmem (for example, when running crashreporter tests).
  if (mShmem.IsReadable()) {
    CrashReporterMetadataShmem::ReadAppNotes(mShmem, annotations);
  }

  MergeCrashAnnotations(mExtraAnnotations, annotations);
  CrashReporter::WriteExtraFile(mDumpID, mExtraAnnotations);

  int32_t crashType = GetCrashType();
  NotifyCrashService(mProcessType, crashType, mDumpID);

  mFinalized = true;
  return true;
}

/* static */
void CrashReporterHost::NotifyCrashService(GeckoProcessType aProcessType,
                                           int32_t aCrashType,
                                           const nsString& aChildDumpID) {
  if (!NS_IsMainThread()) {
    RefPtr<Runnable> runnable = NS_NewRunnableFunction(
        "ipc::CrashReporterHost::NotifyCrashService", [&]() -> void {
          CrashReporterHost::NotifyCrashService(aProcessType, aCrashType,
                                                aChildDumpID);
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
  nsCString telemetryKey;

  switch (aProcessType) {
    case GeckoProcessType_IPDLUnitTest:
    case GeckoProcessType_Default:
      NS_ERROR("unknown process type");
      return;
    default:
      processType = (int)aProcessType;
      break;
  }

  if (aProcessType == GeckoProcessType_Plugin &&
      aCrashType == nsICrashService::CRASH_TYPE_HANG) {
    telemetryKey.AssignLiteral("pluginhang");
  } else {
    switch (aProcessType) {
#define GECKO_PROCESS_TYPE(enum_name, string_name, xre_name, bin_type) \
  case GeckoProcessType_##enum_name:                                   \
    telemetryKey.AssignLiteral(string_name);                           \
    break;
#include "mozilla/GeckoProcessTypes.h"
#undef GECKO_PROCESS_TYPE
      // We can't really hit this, thanks to the above switch, but having it
      // here will placate the compiler.
      default:
        NS_ERROR("unknown process type");
        return;
    }
  }

  RefPtr<Promise> promise;
  crashService->AddCrash(processType, aCrashType, aChildDumpID,
                         getter_AddRefs(promise));
  Telemetry::Accumulate(Telemetry::SUBPROCESS_CRASHES_WITH_DUMP, telemetryKey,
                        1);
}

void CrashReporterHost::AddAnnotation(CrashReporter::Annotation aKey,
                                      bool aValue) {
  mExtraAnnotations[aKey] =
      aValue ? NS_LITERAL_CSTRING("1") : NS_LITERAL_CSTRING("0");
}

void CrashReporterHost::AddAnnotation(CrashReporter::Annotation aKey,
                                      int aValue) {
  nsAutoCString valueString;
  valueString.AppendInt(aValue);
  mExtraAnnotations[aKey] = valueString;
}

void CrashReporterHost::AddAnnotation(CrashReporter::Annotation aKey,
                                      unsigned int aValue) {
  nsAutoCString valueString;
  valueString.AppendInt(aValue);
  mExtraAnnotations[aKey] = valueString;
}

void CrashReporterHost::AddAnnotation(CrashReporter::Annotation aKey,
                                      const nsACString& aValue) {
  mExtraAnnotations[aKey] = aValue;
}

}  // namespace ipc
}  // namespace mozilla
