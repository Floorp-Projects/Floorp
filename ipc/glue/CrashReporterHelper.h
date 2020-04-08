/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_CrashReporterHelper_h
#define mozilla_ipc_CrashReporterHelper_h

#include "CrashReporterHost.h"
#include "mozilla/UniquePtr.h"
#include "nsExceptionHandler.h"
#include "nsICrashService.h"

namespace mozilla {
namespace ipc {

/**
 * This class encapsulates the common elements of crash report handling for
 * toplevel protocols representing processes. To use this class, you should:
 *
 * 1. Declare a method to initialize the crash reporter in your IPDL:
 *    `async InitCrashReporter(NativeThreadId threadId)`
 *
 * 2. Inherit from this class, providing the appropriate `GeckoProcessType`
 *    enum value for the template parameter PT.
 *
 * 3. When your protocol actor is destroyed with a reason of `AbnormalShutdown`,
 *    you should call `GenerateCrashReport(OtherPid())`. If you need the crash
 *    report ID it will be copied in the second optional parameter upon
 *    successful crash report generation.
 */
template <GeckoProcessType PT>
class CrashReporterHelper {
 public:
  CrashReporterHelper() : mCrashReporter(nullptr) {}
  IPCResult RecvInitCrashReporter(const CrashReporter::ThreadId& aThreadId) {
    mCrashReporter = MakeUnique<ipc::CrashReporterHost>(PT, aThreadId);
    return IPC_OK();
  }

 protected:
  void GenerateCrashReport(base::ProcessId aPid,
                           nsString* aMinidumpId = nullptr) {
    nsAutoString minidumpId;
    if (!mCrashReporter) {
      HandleOrphanedMinidump(aPid, minidumpId);
    } else if (mCrashReporter->GenerateCrashReport(aPid)) {
      minidumpId = mCrashReporter->MinidumpID();
    }

    if (aMinidumpId) {
      *aMinidumpId = minidumpId;
    }

    mCrashReporter = nullptr;
  }

 private:
  void HandleOrphanedMinidump(base::ProcessId aPid, nsString& aMinidumpId) {
    if (CrashReporter::FinalizeOrphanedMinidump(aPid, PT, &aMinidumpId)) {
      CrashReporterHost::RecordCrash(PT, nsICrashService::CRASH_TYPE_CRASH,
                                     aMinidumpId);
    } else {
      NS_WARNING(nsPrintfCString("child process pid = %d crashed without "
                                 "leaving a minidump behind",
                                 aPid)
                     .get());
    }
  }

 protected:
  UniquePtr<ipc::CrashReporterHost> mCrashReporter;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_CrashReporterHelper_h
