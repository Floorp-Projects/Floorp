/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePump.h"

using namespace mozilla::ipc;

NS_IMPL_ADDREF_INHERITED(MessagePumpForNonMainUIThreads, MessagePump)
NS_IMPL_RELEASE_INHERITED(MessagePumpForNonMainUIThreads, MessagePump)
NS_IMPL_QUERY_INTERFACE(MessagePumpForNonMainUIThreads, nsIThreadObserver)

#define CHECK_QUIT_STATE       \
  {                            \
    if (state_->should_quit) { \
      break;                   \
    }                          \
  }

void MessagePumpForNonMainUIThreads::DoRunLoop() {
  MOZ_RELEASE_ASSERT(!NS_IsMainThread(),
                     "Use mozilla::ipc::MessagePump instead!");

  // If this is a chromium thread and no nsThread is associated
  // with it, this call will create a new nsThread.
  nsIThread* thread = NS_GetCurrentThread();
  MOZ_ASSERT(thread);

  // Set the main thread observer so we can wake up when
  // xpcom events need to get processed.
  nsCOMPtr<nsIThreadInternal> ti(do_QueryInterface(thread));
  MOZ_ASSERT(ti);
  ti->SetObserver(this);

  for (;;) {
    bool didWork = NS_ProcessNextEvent(thread, false);

    didWork |= ProcessNextWindowsMessage();
    CHECK_QUIT_STATE

    didWork |= state_->delegate->DoWork();
    CHECK_QUIT_STATE

    didWork |= state_->delegate->DoDelayedWork(&delayed_work_time_);
    if (didWork && delayed_work_time_.is_null()) {
      KillTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this));
    }
    CHECK_QUIT_STATE

    if (didWork) {
      continue;
    }

    DebugOnly<bool> didIdleWork = state_->delegate->DoIdleWork();
    MOZ_ASSERT(!didIdleWork);
    CHECK_QUIT_STATE

    SetInWait();
    bool hasWork = NS_HasPendingEvents(thread);
    if (didWork || hasWork) {
      ClearInWait();
      continue;
    }
    WaitForWork();  // Calls MsgWaitForMultipleObjectsEx(QS_ALLINPUT)
    ClearInWait();
  }

  ClearInWait();

  ti->SetObserver(nullptr);
}

NS_IMETHODIMP
MessagePumpForNonMainUIThreads::OnDispatchedEvent() {
  // If our thread is sleeping in DoRunLoop's call to WaitForWork() and an
  // event posts to the nsIThread event queue - break our thread out of
  // chromium's WaitForWork.
  if (GetInWait()) {
    ScheduleWork();
  }
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
