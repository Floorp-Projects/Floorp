/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_functionbrokerthread_h
#define mozilla_plugins_functionbrokerthread_h

#include "nsThreadManager.h"

namespace mozilla {
namespace plugins {

class FunctionBrokerThread
{
public:
  void Dispatch(already_AddRefed<nsIRunnable>&& aRunnable)
  {
    mThread->Dispatch(Move(aRunnable), nsIEventTarget::NS_DISPATCH_NORMAL);
  }

  bool IsOnThread()
  {
    bool on;
    return NS_SUCCEEDED(mThread->IsOnCurrentThread(&on)) && on;
  }

  static FunctionBrokerThread* Create()
  {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIThread> thread;
    if (NS_FAILED(NS_NewNamedThread("Function Broker", getter_AddRefs(thread)))) {
      return nullptr;
    }
    return new FunctionBrokerThread(thread);
  }

  ~FunctionBrokerThread()
  {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    mThread->Shutdown();
  }

private:
  explicit FunctionBrokerThread(nsIThread* aThread) : mThread(aThread)
  {
    MOZ_ASSERT(mThread);
  }

  nsCOMPtr<nsIThread> mThread;
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_functionbrokerthread_h
