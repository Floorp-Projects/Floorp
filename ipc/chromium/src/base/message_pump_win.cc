/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_win.h"

#include <math.h>

#include "base/message_loop.h"
#include "base/histogram.h"
#include "base/win_util.h"
#include "mozilla/ProfilerLabels.h"
#include "WinUtils.h"

using base::Time;

namespace base {

static const wchar_t kWndClass[] = L"Chrome_MessagePumpWindow";

// Message sent to get an additional time slice for pumping (processing) another
// task (a series of such messages creates a continuous task pump).
static const int kMsgHaveWork = WM_USER + 1;

//-----------------------------------------------------------------------------
// MessagePumpWin public:

void MessagePumpWin::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MessagePumpWin::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessagePumpWin::WillProcessMessage(const MSG& msg) {
  FOR_EACH_OBSERVER(Observer, observers_, WillProcessMessage(msg));
}

void MessagePumpWin::DidProcessMessage(const MSG& msg) {
  FOR_EACH_OBSERVER(Observer, observers_, DidProcessMessage(msg));
}

void MessagePumpWin::RunWithDispatcher(Delegate* delegate,
                                       Dispatcher* dispatcher) {
  RunState s;
  s.delegate = delegate;
  s.dispatcher = dispatcher;
  s.should_quit = false;
  s.run_depth = state_ ? state_->run_depth + 1 : 1;

  RunState* previous_state = state_;
  state_ = &s;

  DoRunLoop();

  state_ = previous_state;
}

void MessagePumpWin::Quit() {
  DCHECK(state_);
  state_->should_quit = true;
}

//-----------------------------------------------------------------------------
// MessagePumpWin protected:

int MessagePumpWin::GetCurrentDelay() const {
  if (delayed_work_time_.is_null()) return -1;

  // Be careful here.  TimeDelta has a precision of microseconds, but we want a
  // value in milliseconds.  If there are 5.5ms left, should the delay be 5 or
  // 6?  It should be 6 to avoid executing delayed work too early.
  double timeout =
      ceil((delayed_work_time_ - TimeTicks::Now()).InMillisecondsF());

  // If this value is negative, then we need to run delayed work soon.
  int delay = static_cast<int>(timeout);
  if (delay < 0) delay = 0;

  return delay;
}

//-----------------------------------------------------------------------------
// MessagePumpForUI public:

MessagePumpForUI::MessagePumpForUI() { InitMessageWnd(); }

MessagePumpForUI::~MessagePumpForUI() {
  DestroyWindow(message_hwnd_);
  UnregisterClass(kWndClass, GetModuleHandle(NULL));
}

void MessagePumpForUI::ScheduleWork() {
  if (InterlockedExchange(&have_work_, 1))
    return;  // Someone else continued the pumping.

  // Make sure the MessagePump does some work for us.
  PostMessage(message_hwnd_, kMsgHaveWork, reinterpret_cast<WPARAM>(this), 0);

  // In order to wake up any cross-process COM calls which may currently be
  // pending on the main thread, we also have to post a UI message.
  PostMessage(message_hwnd_, WM_NULL, 0, 0);
}

void MessagePumpForUI::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  //
  // We would *like* to provide high resolution timers.  Windows timers using
  // SetTimer() have a 10ms granularity.  We have to use WM_TIMER as a wakeup
  // mechanism because the application can enter modal windows loops where it
  // is not running our MessageLoop; the only way to have our timers fire in
  // these cases is to post messages there.
  //
  // To provide sub-10ms timers, we process timers directly from our run loop.
  // For the common case, timers will be processed there as the run loop does
  // its normal work.  However, we *also* set the system timer so that WM_TIMER
  // events fire.  This mops up the case of timers not being able to work in
  // modal message loops.  It is possible for the SetTimer to pop and have no
  // pending timers, because they could have already been processed by the
  // run loop itself.
  //
  // We use a single SetTimer corresponding to the timer that will expire
  // soonest.  As new timers are created and destroyed, we update SetTimer.
  // Getting a spurrious SetTimer event firing is benign, as we'll just be
  // processing an empty timer queue.
  //
  delayed_work_time_ = delayed_work_time;

  int delay_msec = GetCurrentDelay();
  DCHECK(delay_msec >= 0);
  if (delay_msec < USER_TIMER_MINIMUM) delay_msec = USER_TIMER_MINIMUM;

  // Create a WM_TIMER event that will wake us up to check for any pending
  // timers (in case we are running within a nested, external sub-pump).
  SetTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this), delay_msec, NULL);
}

void MessagePumpForUI::PumpOutPendingPaintMessages() {
  // If we are being called outside of the context of Run, then don't try to do
  // any work.
  if (!state_) return;

  // Create a mini-message-pump to force immediate processing of only Windows
  // WM_PAINT messages.  Don't provide an infinite loop, but do enough peeking
  // to get the job done.  Actual common max is 4 peeks, but we'll be a little
  // safe here.
  const int kMaxPeekCount = 20;
  int peek_count;
  for (peek_count = 0; peek_count < kMaxPeekCount; ++peek_count) {
    MSG msg;
    if (!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_QS_PAINT)) break;
    ProcessMessageHelper(msg);
    if (state_->should_quit)  // Handle WM_QUIT.
      break;
  }
}

//-----------------------------------------------------------------------------
// MessagePumpForUI private:

// static
LRESULT CALLBACK MessagePumpForUI::WndProcThunk(HWND hwnd, UINT message,
                                                WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case kMsgHaveWork:
      reinterpret_cast<MessagePumpForUI*>(wparam)->HandleWorkMessage();
      break;
    case WM_TIMER:
      reinterpret_cast<MessagePumpForUI*>(wparam)->HandleTimerMessage();
      break;
  }
  return DefWindowProc(hwnd, message, wparam, lparam);
}

void MessagePumpForUI::DoRunLoop() {
  // IF this was just a simple PeekMessage() loop (servicing all possible work
  // queues), then Windows would try to achieve the following order according
  // to MSDN documentation about PeekMessage with no filter):
  //    * Sent messages
  //    * Posted messages
  //    * Sent messages (again)
  //    * WM_PAINT messages
  //    * WM_TIMER messages
  //
  // Summary: none of the above classes is starved, and sent messages has twice
  // the chance of being processed (i.e., reduced service time).

  for (;;) {
    // If we do any work, we may create more messages etc., and more work may
    // possibly be waiting in another task group.  When we (for example)
    // ProcessNextWindowsMessage(), there is a good chance there are still more
    // messages waiting.  On the other hand, when any of these methods return
    // having done no work, then it is pretty unlikely that calling them again
    // quickly will find any work to do.  Finally, if they all say they had no
    // work, then it is a good time to consider sleeping (waiting) for more
    // work.

    bool more_work_is_plausible = ProcessNextWindowsMessage();
    if (state_->should_quit) break;

    more_work_is_plausible |= state_->delegate->DoWork();
    if (state_->should_quit) break;

    more_work_is_plausible |=
        state_->delegate->DoDelayedWork(&delayed_work_time_);
    // If we did not process any delayed work, then we can assume that our
    // existing WM_TIMER if any will fire when delayed work should run.  We
    // don't want to disturb that timer if it is already in flight.  However,
    // if we did do all remaining delayed work, then lets kill the WM_TIMER.
    if (more_work_is_plausible && delayed_work_time_.is_null())
      KillTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this));
    if (state_->should_quit) break;

    if (more_work_is_plausible) continue;

    more_work_is_plausible = state_->delegate->DoIdleWork();
    if (state_->should_quit) break;

    if (more_work_is_plausible) continue;

    WaitForWork();  // Wait (sleep) until we have work to do again.
  }
}

void MessagePumpForUI::InitMessageWnd() {
  HINSTANCE hinst = GetModuleHandle(NULL);

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProcThunk;
  wc.hInstance = hinst;
  wc.lpszClassName = kWndClass;
  RegisterClassEx(&wc);

  message_hwnd_ =
      CreateWindow(kWndClass, 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hinst, 0);
  DCHECK(message_hwnd_);
}

void MessagePumpForUI::WaitForWork() {
  AUTO_PROFILER_LABEL("MessagePumpForUI::WaitForWork", IDLE);

  // Wait until a message is available, up to the time needed by the timer
  // manager to fire the next set of timers.
  int delay = GetCurrentDelay();
  if (delay < 0)  // Negative value means no timers waiting.
    delay = INFINITE;

  mozilla::widget::WinUtils::WaitForMessage(delay);
}

void MessagePumpForUI::HandleWorkMessage() {
  // If we are being called outside of the context of Run, then don't try to do
  // any work.  This could correspond to a MessageBox call or something of that
  // sort.
  if (!state_) {
    // Since we handled a kMsgHaveWork message, we must still update this flag.
    InterlockedExchange(&have_work_, 0);
    return;
  }

  // Let whatever would have run had we not been putting messages in the queue
  // run now.  This is an attempt to make our dummy message not starve other
  // messages that may be in the Windows message queue.
  ProcessPumpReplacementMessage();

  // Now give the delegate a chance to do some work.  He'll let us know if he
  // needs to do more work.
  if (state_->delegate->DoWork()) ScheduleWork();
}

void MessagePumpForUI::HandleTimerMessage() {
  KillTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this));

  // If we are being called outside of the context of Run, then don't do
  // anything.  This could correspond to a MessageBox call or something of
  // that sort.
  if (!state_) return;

  state_->delegate->DoDelayedWork(&delayed_work_time_);
  if (!delayed_work_time_.is_null()) {
    // A bit gratuitous to set delayed_work_time_ again, but oh well.
    ScheduleDelayedWork(delayed_work_time_);
  }
}

bool MessagePumpForUI::ProcessNextWindowsMessage() {
  // If there are sent messages in the queue then PeekMessage internally
  // dispatches the message and returns false. We return true in this
  // case to ensure that the message loop peeks again instead of calling
  // MsgWaitForMultipleObjectsEx again.
  bool sent_messages_in_queue = false;
  DWORD queue_status = GetQueueStatus(QS_SENDMESSAGE);
  if (HIWORD(queue_status) & QS_SENDMESSAGE) sent_messages_in_queue = true;

  MSG msg;
  if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    return ProcessMessageHelper(msg);

  return sent_messages_in_queue;
}

bool MessagePumpForUI::ProcessMessageHelper(const MSG& msg) {
  if (WM_QUIT == msg.message) {
    // Repost the QUIT message so that it will be retrieved by the primary
    // GetMessage() loop.
    state_->should_quit = true;
    PostQuitMessage(static_cast<int>(msg.wParam));
    return false;
  }

  // While running our main message pump, we discard kMsgHaveWork messages.
  if (msg.message == kMsgHaveWork && msg.hwnd == message_hwnd_)
    return ProcessPumpReplacementMessage();

  WillProcessMessage(msg);

  if (state_->dispatcher) {
    if (!state_->dispatcher->Dispatch(msg)) state_->should_quit = true;
  } else {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DidProcessMessage(msg);
  return true;
}

bool MessagePumpForUI::ProcessPumpReplacementMessage() {
  // When we encounter a kMsgHaveWork message, this method is called to peek
  // and process a replacement message, such as a WM_PAINT or WM_TIMER.  The
  // goal is to make the kMsgHaveWork as non-intrusive as possible, even though
  // a continuous stream of such messages are posted.  This method carefully
  // peeks a message while there is no chance for a kMsgHaveWork to be pending,
  // then resets the have_work_ flag (allowing a replacement kMsgHaveWork to
  // possibly be posted), and finally dispatches that peeked replacement.  Note
  // that the re-post of kMsgHaveWork may be asynchronous to this thread!!

  MSG msg;
  bool have_message = false;
  if (MessageLoop::current()->os_modal_loop()) {
    // We only peek out WM_PAINT and WM_TIMER here for reasons mentioned above.
    have_message = PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE) ||
                   PeekMessage(&msg, NULL, WM_TIMER, WM_TIMER, PM_REMOVE);
  } else {
    have_message = (0 != PeekMessage(&msg, NULL, 0, 0, PM_REMOVE));

    if (have_message && msg.message == WM_NULL)
      have_message = (0 != PeekMessage(&msg, NULL, 0, 0, PM_REMOVE));
  }

  DCHECK(!have_message || kMsgHaveWork != msg.message ||
         msg.hwnd != message_hwnd_);

  // Since we discarded a kMsgHaveWork message, we must update the flag.
  int old_have_work = InterlockedExchange(&have_work_, 0);
  DCHECK(old_have_work);

  // We don't need a special time slice if we didn't have_message to process.
  if (!have_message) return false;

  // Guarantee we'll get another time slice in the case where we go into native
  // windows code.   This ScheduleWork() may hurt performance a tiny bit when
  // tasks appear very infrequently, but when the event queue is busy, the
  // kMsgHaveWork events get (percentage wise) rarer and rarer.
  ScheduleWork();
  return ProcessMessageHelper(msg);
}

//-----------------------------------------------------------------------------
// MessagePumpForIO public:

MessagePumpForIO::MessagePumpForIO() {
  port_.Set(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1));
  DCHECK(port_.IsValid());
}

void MessagePumpForIO::ScheduleWork() {
  if (InterlockedExchange(&have_work_, 1))
    return;  // Someone else continued the pumping.

  // Make sure the MessagePump does some work for us.
  BOOL ret =
      PostQueuedCompletionStatus(port_, 0, reinterpret_cast<ULONG_PTR>(this),
                                 reinterpret_cast<OVERLAPPED*>(this));
  DCHECK(ret);
}

void MessagePumpForIO::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  // We know that we can't be blocked right now since this method can only be
  // called on the same thread as Run, so we only need to update our record of
  // how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

void MessagePumpForIO::RegisterIOHandler(HANDLE file_handle,
                                         IOHandler* handler) {
  ULONG_PTR key = reinterpret_cast<ULONG_PTR>(handler);
  HANDLE port = CreateIoCompletionPort(file_handle, port_, key, 1);
  DCHECK(port == port_.Get());
}

//-----------------------------------------------------------------------------
// MessagePumpForIO private:

void MessagePumpForIO::DoRunLoop() {
  for (;;) {
    // If we do any work, we may create more messages etc., and more work may
    // possibly be waiting in another task group.  When we (for example)
    // WaitForIOCompletion(), there is a good chance there are still more
    // messages waiting.  On the other hand, when any of these methods return
    // having done no work, then it is pretty unlikely that calling them
    // again quickly will find any work to do.  Finally, if they all say they
    // had no work, then it is a good time to consider sleeping (waiting) for
    // more work.

    bool more_work_is_plausible = state_->delegate->DoWork();
    if (state_->should_quit) break;

    more_work_is_plausible |= WaitForIOCompletion(0, NULL);
    if (state_->should_quit) break;

    more_work_is_plausible |=
        state_->delegate->DoDelayedWork(&delayed_work_time_);
    if (state_->should_quit) break;

    if (more_work_is_plausible) continue;

    more_work_is_plausible = state_->delegate->DoIdleWork();
    if (state_->should_quit) break;

    if (more_work_is_plausible) continue;

    WaitForWork();  // Wait (sleep) until we have work to do again.
  }
}

// Wait until IO completes, up to the time needed by the timer manager to fire
// the next set of timers.
void MessagePumpForIO::WaitForWork() {
  // We do not support nested IO message loops. This is to avoid messy
  // recursion problems.
  DCHECK(state_->run_depth == 1) << "Cannot nest an IO message loop!";

  int timeout = GetCurrentDelay();
  if (timeout < 0)  // Negative value means no timers waiting.
    timeout = INFINITE;

  WaitForIOCompletion(timeout, NULL);
}

bool MessagePumpForIO::WaitForIOCompletion(DWORD timeout, IOHandler* filter) {
  IOItem item;
  if (completed_io_.empty() || !MatchCompletedIOItem(filter, &item)) {
    // We have to ask the system for another IO completion.
    if (!GetIOItem(timeout, &item)) return false;

    if (ProcessInternalIOItem(item)) return true;
  }

  if (item.context->handler) {
    if (filter && item.handler != filter) {
      // Save this item for later
      completed_io_.push_back(item);
    } else {
      DCHECK(item.context->handler == item.handler);
      item.handler->OnIOCompleted(item.context, item.bytes_transfered,
                                  item.error);
    }
  } else {
    // The handler must be gone by now, just cleanup the mess.
    delete item.context;
  }
  return true;
}

// Asks the OS for another IO completion result.
bool MessagePumpForIO::GetIOItem(DWORD timeout, IOItem* item) {
  memset(item, 0, sizeof(*item));
  ULONG_PTR key = 0;
  OVERLAPPED* overlapped = NULL;
  AUTO_PROFILER_LABEL("MessagePumpForIO::GetIOItem::Wait", IDLE);
  if (!GetQueuedCompletionStatus(port_.Get(), &item->bytes_transfered, &key,
                                 &overlapped, timeout)) {
    if (!overlapped) return false;  // Nothing in the queue.
    item->error = GetLastError();
    item->bytes_transfered = 0;
  }

  item->handler = reinterpret_cast<IOHandler*>(key);
  item->context = reinterpret_cast<IOContext*>(overlapped);
  return true;
}

bool MessagePumpForIO::ProcessInternalIOItem(const IOItem& item) {
  if (this == reinterpret_cast<MessagePumpForIO*>(item.context) &&
      this == reinterpret_cast<MessagePumpForIO*>(item.handler)) {
    // This is our internal completion.
    DCHECK(!item.bytes_transfered);
    InterlockedExchange(&have_work_, 0);
    return true;
  }
  return false;
}

// Returns a completion item that was previously received.
bool MessagePumpForIO::MatchCompletedIOItem(IOHandler* filter, IOItem* item) {
  DCHECK(!completed_io_.empty());
  for (std::list<IOItem>::iterator it = completed_io_.begin();
       it != completed_io_.end(); ++it) {
    if (!filter || it->handler == filter) {
      *item = *it;
      completed_io_.erase(it);
      return true;
    }
  }
  return false;
}

}  // namespace base
