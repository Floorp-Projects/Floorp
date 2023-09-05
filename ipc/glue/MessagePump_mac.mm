/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePump.h"

#include <Foundation/Foundation.h>

#include "base/scoped_nsautorelease_pool.h"
#include "mozilla/ProfilerMarkers.h"
#include "nsISupportsImpl.h"

using namespace mozilla::ipc;

static void NoOp(void* info) {}

NS_IMPL_ADDREF_INHERITED(MessagePumpForNonMainUIThreads, MessagePump)
NS_IMPL_RELEASE_INHERITED(MessagePumpForNonMainUIThreads, MessagePump)
NS_IMPL_QUERY_INTERFACE(MessagePumpForNonMainUIThreads, nsIThreadObserver)

MessagePumpForNonMainUIThreads::MessagePumpForNonMainUIThreads(
    nsISerialEventTarget* aEventTarget)
    : mEventTarget(aEventTarget), keep_running_(true) {
  MOZ_ASSERT(mEventTarget);
  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.perform = NoOp;
  quit_source_ = CFRunLoopSourceCreate(nullptr,  // allocator
                                       0,        // priority
                                       &source_context);
  CFRunLoopAddSource(run_loop(), quit_source_, kCFRunLoopCommonModes);
}

MessagePumpForNonMainUIThreads::~MessagePumpForNonMainUIThreads() {
  CFRunLoopRemoveSource(run_loop(), quit_source_, kCFRunLoopCommonModes);
  CFRelease(quit_source_);
}

void MessagePumpForNonMainUIThreads::DoRun(
    base::MessagePump::Delegate* aDelegate) {
  // If this is a chromium thread and no nsThread is associated with it, this
  // call will create a new nsThread.
  nsIThread* thread = NS_GetCurrentThread();
  MOZ_ASSERT(thread);

  // Set the main thread observer so we can wake up when xpcom events need to
  // get processed.
  nsCOMPtr<nsIThreadInternal> ti(do_QueryInterface(thread));
  MOZ_ASSERT(ti);
  ti->SetObserver(this);

  base::ScopedNSAutoreleasePool autoReleasePool;

  while (keep_running_) {
    // Drain the xpcom event loop first.
    if (NS_ProcessNextEvent(nullptr, false)) {
      continue;
    }

    autoReleasePool.Recycle();

    if (!keep_running_) {
      break;
    }

    // Now process the CFRunLoop. It exits after running once.
    // NSRunLoop manages autorelease pools itself.
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate distantFuture]];
  }

  ti->SetObserver(nullptr);
  keep_running_ = true;
}

void MessagePumpForNonMainUIThreads::Quit() {
  keep_running_ = false;
  CFRunLoopSourceSignal(quit_source_);
  CFRunLoopWakeUp(run_loop());
}

NS_IMETHODIMP MessagePumpForNonMainUIThreads::OnDispatchedEvent() {
  // ScheduleWork will signal an input source to the run loop, making it exit so
  // it can process the xpcom event.
  ScheduleWork();
  return NS_OK;
}

NS_IMETHODIMP
MessagePumpForNonMainUIThreads::OnProcessNextEvent(nsIThreadInternal* thread,
                                                   bool mayWait) {
  return NS_OK;
}

NS_IMETHODIMP
MessagePumpForNonMainUIThreads::AfterProcessNextEvent(nsIThreadInternal* thread,
                                                      bool eventWasProcessed) {
  return NS_OK;
}
