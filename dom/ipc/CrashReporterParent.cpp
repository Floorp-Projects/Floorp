/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CrashReporterParent.h"
#include "mozilla/Sprintf.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/CrashReporterHost.h"
#include "nsAutoPtr.h"
#include "nsXULAppAPI.h"
#include <time.h>

#include "mozilla/Telemetry.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#include "nsICrashService.h"
#include "mozilla/SyncRunnable.h"
#include "nsThreadUtils.h"
#endif

namespace mozilla {
namespace dom {

using namespace mozilla::ipc;

void
CrashReporterParent::AnnotateCrashReport(const nsCString& key,
                                         const nsCString& data)
{
#ifdef MOZ_CRASHREPORTER
  mNotes.Put(key, data);
#endif
}

void
CrashReporterParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005155
}

mozilla::ipc::IPCResult
CrashReporterParent::RecvAppendAppNotes(const nsCString& data)
{
  mAppNotes.Append(data);
  return IPC_OK();
}

CrashReporterParent::CrashReporterParent()
  :
#ifdef MOZ_CRASHREPORTER
    mNotes(4),
#endif
    mStartTime(::time(nullptr))
  , mInitialized(false)
{
  MOZ_COUNT_CTOR(CrashReporterParent);
}

CrashReporterParent::~CrashReporterParent()
{
  MOZ_COUNT_DTOR(CrashReporterParent);
}

void
CrashReporterParent::SetChildData(const NativeThreadId& tid,
                                  const uint32_t& processType)
{
  mInitialized = true;
  mMainThread = tid;
  mProcessType = GeckoProcessType(processType);
}

#ifdef MOZ_CRASHREPORTER
bool
CrashReporterParent::GenerateCrashReportForMinidump(nsIFile* minidump,
                                                    const AnnotationTable* processNotes)
{
  if (!CrashReporter::GetIDFromMinidump(minidump, mChildDumpID)) {
    return false;
  }

  bool result = GenerateChildData(processNotes);
  FinalizeChildData();
  return result;
}

bool
CrashReporterParent::GenerateChildData(const AnnotationTable* processNotes)
{
  MOZ_ASSERT(mInitialized);

  if (mChildDumpID.IsEmpty()) {
    NS_WARNING("problem with GenerateChildData: no child dump id yet!");
    return false;
  }

  nsAutoCString type;
  switch (mProcessType) {
    case GeckoProcessType_Content:
      type = NS_LITERAL_CSTRING("content");
      break;
    case GeckoProcessType_Plugin:
    case GeckoProcessType_GMPlugin:
      type = NS_LITERAL_CSTRING("plugin");
      break;
    default:
      NS_ERROR("unknown process type");
      break;
  }
  mNotes.Put(NS_LITERAL_CSTRING("ProcessType"), type);

  char startTime[32];
  SprintfLiteral(startTime, "%lld", static_cast<long long>(mStartTime));
  mNotes.Put(NS_LITERAL_CSTRING("StartupTime"), nsDependentCString(startTime));

  if (!mAppNotes.IsEmpty()) {
    mNotes.Put(NS_LITERAL_CSTRING("Notes"), mAppNotes);
  }

  // Append these notes to the end of the extra file based on the current
  // dump id we obtained from CreatePairedMinidumps.
  bool ret = CrashReporter::AppendExtraData(mChildDumpID, mNotes);
  if (ret && processNotes) {
    ret = CrashReporter::AppendExtraData(mChildDumpID, *processNotes);
  }

  if (!ret) {
    NS_WARNING("problem appending child data to .extra");
  }
  return ret;
}

void
CrashReporterParent::FinalizeChildData()
{
  MOZ_ASSERT(mInitialized);

  CrashReporterHost::NotifyCrashService(mProcessType, mChildDumpID, &mNotes);
  mNotes.Clear();
}
#endif

} // namespace dom
} // namespace mozilla
