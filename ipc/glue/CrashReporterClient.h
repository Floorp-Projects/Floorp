/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_CrashReporterClient_h
#define mozilla_ipc_CrashReporterClient_h

#include "mozilla/Assertions.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "nsExceptionHandler.h"

namespace mozilla {
namespace ipc {

class CrashReporterClient {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CrashReporterClient);

  // |aTopLevelProtocol| must have a child-to-parent message:
  //
  //   async InitCrashReporter(NativeThreadId threadId);
  template <typename T>
  static void InitSingleton(T* aToplevelProtocol) {
    InitSingleton();
    Unused << aToplevelProtocol->SendInitCrashReporter(
        CrashReporter::CurrentThreadId());
  }

  static void InitSingleton();

  static void DestroySingleton();
  static RefPtr<CrashReporterClient> GetSingleton();

 private:
  explicit CrashReporterClient();
  ~CrashReporterClient();

 private:
  static StaticMutex sLock;
  static StaticRefPtr<CrashReporterClient> sClientSingleton
      MOZ_GUARDED_BY(sLock);
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_CrashReporterClient_h
