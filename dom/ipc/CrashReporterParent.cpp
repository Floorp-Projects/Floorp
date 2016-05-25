/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CrashReporterParent.h"
#include "mozilla/Snprintf.h"
#include "mozilla/dom/ContentParent.h"
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

bool
CrashReporterParent::RecvAppendAppNotes(const nsCString& data)
{
  mAppNotes.Append(data);
  return true;
}

mozilla::ipc::IProtocol*
CrashReporterParent::CloneProtocol(Channel* aChannel,
                                   mozilla::ipc::ProtocolCloneContext* aCtx)
{
#ifdef MOZ_CRASHREPORTER
  ContentParent* contentParent = aCtx->GetContentParent();
  CrashReporter::ThreadId childThreadId = contentParent->Pid();
  GeckoProcessType childProcessType =
    contentParent->Process()->GetProcessType();

  nsAutoPtr<PCrashReporterParent> actor(
    contentParent->AllocPCrashReporterParent(childThreadId,
                                             childProcessType)
  );
  if (!actor ||
      !contentParent->RecvPCrashReporterConstructor(actor,
                                                    childThreadId,
                                                    childThreadId)) {
    return nullptr;
  }

  return actor.forget();
#else
  MOZ_CRASH("Not Implemented");
  return nullptr;
#endif
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
  mProcessType = processType;
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
CrashReporterParent::UseMinidump(nsIFile* aMinidump)
{
  return CrashReporter::GetIDFromMinidump(aMinidump, mChildDumpID);
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
  snprintf_literal(startTime, "%lld", static_cast<long long>(mStartTime));
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

  if (NS_IsMainThread()) {
    // Inline, this is the main thread. Get this done.
    NotifyCrashService();
    return;
  }

  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  class NotifyOnMainThread : public Runnable
  {
  public:
    explicit NotifyOnMainThread(CrashReporterParent* aCR)
      : mCR(aCR)
    { }

    NS_IMETHOD Run() {
      mCR->NotifyCrashService();
      return NS_OK;
    }
  private:
    CrashReporterParent* mCR;
  };
  SyncRunnable::DispatchToThread(mainThread, new NotifyOnMainThread(this));
}

void
CrashReporterParent::NotifyCrashService()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mChildDumpID.IsEmpty());

  nsCOMPtr<nsICrashService> crashService =
    do_GetService("@mozilla.org/crashservice;1");
  if (!crashService) {
    return;
  }

  int32_t processType;
  int32_t crashType = nsICrashService::CRASH_TYPE_CRASH;

  nsCString telemetryKey;

  switch (mProcessType) {
    case GeckoProcessType_Content:
      processType = nsICrashService::PROCESS_TYPE_CONTENT;
      telemetryKey.AssignLiteral("content");
      break;
    case GeckoProcessType_Plugin: {
      processType = nsICrashService::PROCESS_TYPE_PLUGIN;
      telemetryKey.AssignLiteral("plugin");
      nsAutoCString val;
      if (mNotes.Get(NS_LITERAL_CSTRING("PluginHang"), &val) &&
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
    default:
      NS_ERROR("unknown process type");
      return;
  }

  crashService->AddCrash(processType, crashType, mChildDumpID);
  Telemetry::Accumulate(Telemetry::SUBPROCESS_CRASHES_WITH_DUMP, telemetryKey, 1);
  mNotes.Clear();
}
#endif

} // namespace dom
} // namespace mozilla
