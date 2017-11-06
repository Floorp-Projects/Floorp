/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_functionbrokerchild_h
#define mozilla_plugins_functionbrokerchild_h

#include "mozilla/plugins/PFunctionBrokerChild.h"

namespace mozilla {
namespace plugins {

class FunctionBrokerThread;

/**
 * Dispatches brokered methods to the Parent process to allow functionality
 * that is otherwise blocked by the sandbox.
 */
class FunctionBrokerChild : public PFunctionBrokerChild
{
public:
  static bool Initialize(Endpoint<PFunctionBrokerChild>&& aBrokerEndpoint);
  static FunctionBrokerChild* GetInstance();
  static void Destroy();

  bool IsDispatchThread();
  void PostToDispatchThread(already_AddRefed<nsIRunnable>&& runnable);

  void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  explicit FunctionBrokerChild(FunctionBrokerThread* aThread,
                               Endpoint<PFunctionBrokerChild>&& aEndpoint);
  void ShutdownOnDispatchThread();
  void Bind(Endpoint<PFunctionBrokerChild>&& aEndpoint);

  nsAutoPtr<FunctionBrokerThread> mThread;

  // True if tasks on the FunctionBrokerThread have completed
  bool mShutdownDone;
  // This monitor guards mShutdownDone.
  Monitor mMonitor;

  static FunctionBrokerChild* sInstance;
};


} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_functionbrokerchild_h
