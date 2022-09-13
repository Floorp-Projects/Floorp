/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrashReporterHost.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Sprintf.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "nsServiceManagerUtils.h"
#include "nsICrashService.h"
#include "nsXULAppAPI.h"
#include "nsIFile.h"

namespace mozilla {
namespace ipc {

CrashReporterHost::CrashReporterHost(GeckoProcessType aProcessType,
                                     CrashReporter::ThreadId aThreadId)
    : mProcessType(aProcessType),
      mThreadId(aThreadId),
      mStartTime(::time(nullptr)),
      mFinalized(false) {}

bool CrashReporterHost::GenerateCrashReport(base::ProcessId aPid) {
  if (!TakeCrashedChildMinidump(aPid, nullptr)) {
    return false;
  }

  FinalizeCrashReport();
  RecordCrash(mProcessType, nsICrashService::CRASH_TYPE_CRASH, mDumpID);
  return true;
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

void CrashReporterHost::FinalizeCrashReport() {
  MOZ_ASSERT(!mFinalized);
  MOZ_ASSERT(HasMinidump());

  mExtraAnnotations[CrashReporter::Annotation::ProcessType] =
      XRE_ChildProcessTypeToAnnotation(mProcessType);

  char startTime[32];
  SprintfLiteral(startTime, "%lld", static_cast<long long>(mStartTime));
  mExtraAnnotations[CrashReporter::Annotation::StartupTime] =
      nsDependentCString(startTime);

  CrashReporter::WriteExtraFile(mDumpID, mExtraAnnotations);
  mFinalized = true;
}

void CrashReporterHost::DeleteCrashReport() {
  if (mFinalized && HasMinidump()) {
    CrashReporter::DeleteMinidumpFilesForID(mDumpID, Some(u"browser"_ns));
  }
}

/* static */
void CrashReporterHost::RecordCrash(GeckoProcessType aProcessType,
                                    int32_t aCrashType,
                                    const nsString& aChildDumpID) {
  if (!NS_IsMainThread()) {
    RefPtr<Runnable> runnable = NS_NewRunnableFunction(
        "ipc::CrashReporterHost::RecordCrash", [&]() -> void {
          CrashReporterHost::RecordCrash(aProcessType, aCrashType,
                                         aChildDumpID);
        });
    RefPtr<nsIThread> mainThread = do_GetMainThread();
    SyncRunnable::DispatchToThread(mainThread, runnable);
    return;
  }

  RecordCrashWithTelemetry(aProcessType, aCrashType);
  NotifyCrashService(aProcessType, aCrashType, aChildDumpID);
}

/* static */
void CrashReporterHost::RecordCrashWithTelemetry(GeckoProcessType aProcessType,
                                                 int32_t aCrashType) {
  nsCString key;

  switch (aProcessType) {
#define GECKO_PROCESS_TYPE(enum_value, enum_name, string_name, proc_typename, \
                           process_bin_type, procinfo_typename,               \
                           webidl_typename, allcaps_name)                     \
  case GeckoProcessType_##enum_name:                                          \
    key.AssignLiteral(string_name);                                           \
    break;
#include "mozilla/GeckoProcessTypes.h"
#undef GECKO_PROCESS_TYPE
    // We can't really hit this, thanks to the above switch, but having it
    // here will placate the compiler.
    default:
      MOZ_ASSERT_UNREACHABLE("unknown process type");
  }

  Telemetry::Accumulate(Telemetry::SUBPROCESS_CRASHES_WITH_DUMP, key, 1);
}

/* static */
void CrashReporterHost::NotifyCrashService(GeckoProcessType aProcessType,
                                           int32_t aCrashType,
                                           const nsString& aChildDumpID) {
  MOZ_ASSERT(!aChildDumpID.IsEmpty());

  nsCOMPtr<nsICrashService> crashService =
      do_GetService("@mozilla.org/crashservice;1");
  if (!crashService) {
    return;
  }

  int32_t processType;

  switch (aProcessType) {
    case GeckoProcessType_IPDLUnitTest:
    case GeckoProcessType_Default:
      NS_ERROR("unknown process type");
      return;
    default:
      processType = (int)aProcessType;
      break;
  }

  RefPtr<dom::Promise> promise;
  crashService->AddCrash(processType, aCrashType, aChildDumpID,
                         getter_AddRefs(promise));
}

void CrashReporterHost::AddAnnotation(CrashReporter::Annotation aKey,
                                      bool aValue) {
  mExtraAnnotations[aKey] = aValue ? "1"_ns : "0"_ns;
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
