/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_functionbrokerparent_h
#define mozilla_plugins_functionbrokerparent_h

#include "mozilla/plugins/PFunctionBrokerParent.h"

namespace mozilla {
namespace plugins {

class FunctionBrokerThread;

/**
 * Top-level actor run on the process to which we broker calls from sandboxed
 * plugin processes.
 */
class FunctionBrokerParent : public PFunctionBrokerParent
{
public:
  static FunctionBrokerParent* Create(Endpoint<PFunctionBrokerParent>&& aParentEnd);
  static void Destroy(FunctionBrokerParent* aInst);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvBrokerFunction(const FunctionHookId &aFunctionId, const IpdlTuple &aInTuple,
                   IpdlTuple *aOutTuple) override;

private:
  explicit FunctionBrokerParent(FunctionBrokerThread* aThread,
                                Endpoint<PFunctionBrokerParent>&& aParentEnd);
  void ShutdownOnBrokerThread();
  void Bind(Endpoint<PFunctionBrokerParent>&& aEnd);

  static bool RunBrokeredFunction(base::ProcessId aClientId,
                                  const FunctionHookId &aFunctionId,
                                  const IPC::IpdlTuple &aInTuple,
                                  IPC::IpdlTuple *aOutTuple);

  nsAutoPtr<FunctionBrokerThread> mThread;
  Monitor mMonitor;
  bool mShutdownDone;
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_functionbrokerparent_hk
