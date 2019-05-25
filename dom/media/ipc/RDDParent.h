/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_dom_media_ipc_RDDParent_h__
#define _include_dom_media_ipc_RDDParent_h__
#include "mozilla/PRDDParent.h"

#include "mozilla/RefPtr.h"

namespace mozilla {

class TimeStamp;
class ChildProfilerController;

class RDDParent final : public PRDDParent {
 public:
  RDDParent();
  ~RDDParent();

  static RDDParent* GetSingleton();

  bool Init(base::ProcessId aParentPid, const char* aParentBuildID,
            MessageLoop* aIOLoop, IPC::Channel* aChannel);

  mozilla::ipc::IPCResult RecvInit(const Maybe<ipc::FileDescriptor>& aBrokerFd,
                                   bool aStartMacSandbox);
  mozilla::ipc::IPCResult RecvInitProfiler(
      Endpoint<PProfilerChild>&& aEndpoint);

  mozilla::ipc::IPCResult RecvNewContentRemoteDecoderManager(
      Endpoint<PRemoteDecoderManagerParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage,
      const Maybe<ipc::FileDescriptor>& DMDFile);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  const TimeStamp mLaunchTime;
#ifdef MOZ_GECKO_PROFILER
  RefPtr<ChildProfilerController> mProfilerController;
#endif
};

}  // namespace mozilla

#endif  // _include_dom_media_ipc_RDDParent_h__
