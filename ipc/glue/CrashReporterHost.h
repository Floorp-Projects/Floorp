/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_CrashReporterHost_h
#define mozilla_ipc_CrashReporterHost_h

#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/Shmem.h"
#include "base/process.h"
#include "nsExceptionHandler.h"

namespace mozilla {
namespace ipc {

// This is the newer replacement for CrashReporterParent. It is created in
// response to a InitCrashReporter message on a top-level actor, and simply
// holds the metadata shmem alive until the process ends. When the process
// terminates abnormally, the top-level should call GenerateCrashReport to
// automatically integrate metadata.
class CrashReporterHost
{
  typedef mozilla::ipc::Shmem Shmem;
  typedef CrashReporter::AnnotationTable AnnotationTable;

public:
  CrashReporterHost(GeckoProcessType aProcessType, const Shmem& aShmem);

#ifdef MOZ_CRASHREPORTER
  void GenerateCrashReport(base::ProcessId aPid) {
    RefPtr<nsIFile> crashDump;
    if (!XRE_TakeMinidumpForChild(aPid, getter_AddRefs(crashDump), nullptr)) {
      return;
    }
    GenerateCrashReport(crashDump);
  }

  // This is a static helper function to notify the crash service that a
  // crash has occurred. When PCrashReporter is removed, we can make this
  // a member function. This can be called from any thread, and if not
  // called from the main thread, will post a synchronous message to the
  // main thread.
  static void NotifyCrashService(
    GeckoProcessType aProcessType,
    const nsString& aChildDumpID,
    const AnnotationTable* aNotes);
#endif

private:
  void GenerateCrashReport(RefPtr<nsIFile> aCrashDump);
  static void AsyncAddCrash(int32_t aProcessType, int32_t aCrashType,
                            const nsString& aChildDumpID);

private:
  GeckoProcessType mProcessType;
  Shmem mShmem;
  time_t mStartTime;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_CrashReporterHost_h
