/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_dom_media_ipc_RDDChild_h_
#define _include_dom_media_ipc_RDDChild_h_
#include "mozilla/PRDDChild.h"

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
class SandboxBroker;
#endif

namespace ipc {
class CrashReporterHost;
}  // namespace ipc
namespace dom {
class MemoryReportRequestHost;
}  // namespace dom

class RDDProcessHost;

class RDDChild final : public PRDDChild {
  typedef mozilla::dom::MemoryReportRequestHost MemoryReportRequestHost;

 public:
  explicit RDDChild(RDDProcessHost* aHost);
  ~RDDChild();

  bool Init(bool aStartMacSandbox);

  bool EnsureRDDReady();

  // PRDDChild overrides.
  mozilla::ipc::IPCResult RecvInitComplete();
  mozilla::ipc::IPCResult RecvInitCrashReporter(
      Shmem&& shmem, const NativeThreadId& aThreadId);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport);
  mozilla::ipc::IPCResult RecvFinishMemoryReport(const uint32_t& aGeneration);

  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const Maybe<ipc::FileDescriptor>& aDMDFile);

  static void Destroy(UniquePtr<RDDChild>&& aChild);

 private:
  RDDProcessHost* mHost;
  UniquePtr<ipc::CrashReporterHost> mCrashReporter;
  UniquePtr<MemoryReportRequestHost> mMemoryReportRequest;
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  UniquePtr<SandboxBroker> mSandboxBroker;
#endif
  bool mRDDReady;
};

}  // namespace mozilla

#endif  // _include_dom_media_ipc_RDDChild_h_
