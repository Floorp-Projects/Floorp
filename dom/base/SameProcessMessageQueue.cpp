/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SameProcessMessageQueue.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

SameProcessMessageQueue* SameProcessMessageQueue::sSingleton;

SameProcessMessageQueue::SameProcessMessageQueue() = default;

SameProcessMessageQueue::~SameProcessMessageQueue() {
  // This code should run during shutdown, and we should already have pumped the
  // event loop. So we should only see messages here if someone is sending
  // messages pretty late in shutdown.
  NS_WARNING_ASSERTION(mQueue.IsEmpty(),
                       "Shouldn't send messages during shutdown");
  sSingleton = nullptr;
}

void SameProcessMessageQueue::Push(Runnable* aRunnable) {
  mQueue.AppendElement(aRunnable);
  NS_DispatchToCurrentThread(aRunnable);
}

void SameProcessMessageQueue::Flush() {
  const nsTArray<RefPtr<Runnable>> queue = std::move(mQueue);
  for (size_t i = 0; i < queue.Length(); i++) {
    queue[i]->Run();
  }
}

/* static */
SameProcessMessageQueue* SameProcessMessageQueue::Get() {
  if (!sSingleton) {
    sSingleton = new SameProcessMessageQueue();
  }
  return sSingleton;
}

SameProcessMessageQueue::Runnable::Runnable() : mDispatched(false) {}

NS_IMPL_ISUPPORTS(SameProcessMessageQueue::Runnable, nsIRunnable)

nsresult SameProcessMessageQueue::Runnable::Run() {
  if (mDispatched) {
    return NS_OK;
  }

  SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
  queue->mQueue.RemoveElement(this);

  mDispatched = true;
  return HandleMessage();
}
