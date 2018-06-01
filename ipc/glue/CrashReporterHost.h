/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_CrashReporterHost_h
#define mozilla_ipc_CrashReporterHost_h

#include <functional>

#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/Shmem.h"
#include "base/process.h"
#include "nsExceptionHandler.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace ipc {

class GeckoChildProcessHost;

// This is the newer replacement for CrashReporterParent. It is created in
// response to a InitCrashReporter message on a top-level actor, and simply
// holds the metadata shmem alive until the process ends. When the process
// terminates abnormally, the top-level should call GenerateCrashReport to
// automatically integrate metadata.
class CrashReporterHost
{
  typedef mozilla::ipc::Shmem Shmem;
  typedef CrashReporter::AnnotationTable AnnotationTable;
  typedef CrashReporter::ThreadId ThreadId;

public:

  template <typename T>
  class CallbackWrapper {
  public:
    void Init(std::function<void(T)>&& aCallback, bool aAsync)
    {
      mCallback = std::move(aCallback);
      mAsync = aAsync;
      if (IsAsync()) {
        // Don't call do_GetCurrentThread() if this is called synchronously
        // because 1. it's unnecessary, and 2. more importantly, it might create
        // one if called from a native thread, and the thread will be leaked.
        mTargetThread = do_GetCurrentThread();
      }
    }

    bool IsEmpty()
    {
      return !mCallback;
    }

    bool IsAsync()
    {
      return mAsync;
    }

    void Invoke(T aResult)
    {
      if (IsAsync()) {
        decltype(mCallback) callback = std::move(mCallback);
        mTargetThread->
          Dispatch(NS_NewRunnableFunction("ipc::CrashReporterHost::CallbackWrapper::Invoke",
                                          [callback, aResult](){
                     callback(aResult);
                   }), NS_DISPATCH_NORMAL);
      } else {
        MOZ_ASSERT(!mTargetThread);
        mCallback(aResult);
      }

      Clear();
    }

  private:
    void Clear()
    {
      mCallback = nullptr;
      mTargetThread = nullptr;
      mAsync = false;
    }

    bool mAsync;
    std::function<void(T)> mCallback;
    nsCOMPtr<nsIThread> mTargetThread;
  };

  CrashReporterHost(GeckoProcessType aProcessType,
                    const Shmem& aShmem,
                    ThreadId aThreadId);

  // Helper function for generating a crash report for a process that probably
  // crashed (i.e., had an AbnormalShutdown in ActorDestroy). Returns true if
  // the process has a minidump attached and we were able to generate a report.
  bool GenerateCrashReport(base::ProcessId aPid);

  // Given an existing minidump for a crashed child process, take ownership of
  // it from IPDL. After this, FinalizeCrashReport may be called.
  RefPtr<nsIFile> TakeCrashedChildMinidump(base::ProcessId aPid, uint32_t* aOutSequence);

  // Replace the stored minidump with a new one. After this,
  // FinalizeCrashReport may be called.
  bool AdoptMinidump(nsIFile* aFile);

  // If a minidump was already captured (e.g. via the hang reporter), this
  // finalizes the existing report by attaching metadata and notifying the
  // crash service.
  bool FinalizeCrashReport();

  // Generate a paired minidump. This does not take the crash report, as
  // GenerateCrashReport does. After this, FinalizeCrashReport may be called.
  // Minidump(s) can be generated synchronously or asynchronously, specified in
  // argument aAsync. When the operation completes, aCallback is invoked, where
  // the callback argument denotes whether the operation succeeded.
  void
  GenerateMinidumpAndPair(GeckoChildProcessHost* aChildProcess,
                          nsIFile* aMinidumpToPair,
                          const nsACString& aPairName,
                          std::function<void(bool)>&& aCallback,
                          bool aAsync);

  // This is a static helper function to notify the crash service that a
  // crash has occurred. When PCrashReporter is removed, we can make this
  // a member function. This can be called from any thread, and if not
  // called from the main thread, will post a synchronous message to the
  // main thread.
  static void NotifyCrashService(
    GeckoProcessType aProcessType,
    const nsString& aChildDumpID,
    const AnnotationTable* aNotes);

  void AddNote(const nsCString& aKey, const nsCString& aValue);

  bool HasMinidump() const {
    return !mDumpID.IsEmpty();
  }
  const nsString& MinidumpID() const {
    MOZ_ASSERT(HasMinidump());
    return mDumpID;
  }

private:
  static void AsyncAddCrash(int32_t aProcessType, int32_t aCrashType,
                            const nsString& aChildDumpID);

private:
  CallbackWrapper<bool> mCreateMinidumpCallback;
  GeckoProcessType mProcessType;
  Shmem mShmem;
  ThreadId mThreadId;
  time_t mStartTime;
  AnnotationTable mExtraNotes;
  nsString mDumpID;
  bool mFinalized;
  nsCOMPtr<nsIFile> mTargetDump;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_CrashReporterHost_h
