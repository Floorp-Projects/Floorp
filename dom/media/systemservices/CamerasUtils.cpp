/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/unused.h"
#include "nsThreadUtils.h"
#include "nsCOMPtr.h"
#include "mozilla/Assertions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIIPCBackgroundChildCreateCallback.h"

namespace mozilla {
namespace camera {

class WorkerBackgroundChildCallback final :
  public nsIIPCBackgroundChildCreateCallback
{
  bool* mDone;

public:
  explicit WorkerBackgroundChildCallback(bool* aDone)
  : mDone(aDone)
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mDone);
  }

  NS_DECL_ISUPPORTS

private:
  ~WorkerBackgroundChildCallback() { }

  virtual void
  ActorCreated(PBackgroundChild* aActor) override
  {
    *mDone = true;
  }

  virtual void
  ActorFailed() override
  {
    *mDone = true;
  }
};
NS_IMPL_ISUPPORTS(WorkerBackgroundChildCallback, nsIIPCBackgroundChildCreateCallback)

nsresult
SynchronouslyCreatePBackground()
{
  using mozilla::ipc::BackgroundChild;

  MOZ_ASSERT(!BackgroundChild::GetForCurrentThread());

  bool done = false;
  nsCOMPtr<nsIIPCBackgroundChildCreateCallback> callback =
    new WorkerBackgroundChildCallback(&done);

  if (NS_WARN_IF(!BackgroundChild::GetOrCreateForCurrentThread(callback))) {
    return NS_ERROR_FAILURE;
  }

  nsIThread *thread = NS_GetCurrentThread();

  while (!done) {
    if (NS_WARN_IF(!NS_ProcessNextEvent(thread, true /* aMayWait */))) {
      return NS_ERROR_FAILURE;
    }
  }

  if (NS_WARN_IF(!BackgroundChild::GetForCurrentThread())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}
}
