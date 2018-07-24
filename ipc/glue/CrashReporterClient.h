/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_CrashReporterClient_h
#define mozilla_ipc_CrashReporterClient_h

#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace ipc {

class CrashReporterMetadataShmem;

class CrashReporterClient
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CrashReporterClient);

  // |aTopLevelProtocol| must be a top-level protocol instance, as sub-actors
  // do not have AllocUnsafeShmem. It must also have a child-to-parent message:
  //
  //   async SetCrashReporterClient(Shmem shmem);
  //
  // The parent-side receive function of this message should save the shmem
  // somewhere, and when the top-level actor's ActorDestroy runs (or when the
  // crash reporter needs metadata), the shmem should be parsed.
  template <typename T>
  static bool InitSingleton(T* aToplevelProtocol) {
    // The crash reporter is not enabled in recording/replaying processes.
    if (recordreplay::IsRecordingOrReplaying()) {
      return true;
    }

    Shmem shmem;
    if (!AllocShmem(aToplevelProtocol, &shmem)) {
      return false;
    }

    InitSingletonWithShmem(shmem);
    Unused << aToplevelProtocol->SendInitCrashReporter(
      shmem,
      CrashReporter::CurrentThreadId());
    return true;
  }

  template <typename T>
  static bool AllocShmem(T* aToplevelProtocol, Shmem* aOutShmem) {
    // 16KB should be enough for most metadata - see bug 1278717 comment #11.
    static const size_t kShmemSize = 16 * 1024;

    return aToplevelProtocol->AllocUnsafeShmem(
      kShmemSize,
      SharedMemory::TYPE_BASIC,
      aOutShmem);
  }

  static void InitSingletonWithShmem(const Shmem& aShmem);

  static void DestroySingleton();
  static RefPtr<CrashReporterClient> GetSingleton();

  void AnnotateCrashReport(const nsCString& aKey, const nsCString& aData);
  void AppendAppNotes(const nsCString& aData);

private:
  explicit CrashReporterClient(const Shmem& aShmem);
  ~CrashReporterClient();

private:
  static StaticMutex sLock;
  static StaticRefPtr<CrashReporterClient> sClientSingleton;

private:
  UniquePtr<CrashReporterMetadataShmem> mMetadata;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_CrashReporterClient_h

