// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <windows.h>
#include <mmsystem.h>

#include "base/event_recorder.h"
#include "base/file_util.h"
#include "base/logging.h"

// A note about time.
// For perfect playback of events, you'd like a very accurate timer
// so that events are played back at exactly the same time that
// they were recorded.  However, windows has a clock which is only
// granular to ~15ms.  We see more consistent event playback when
// using a higher resolution timer.  To do this, we use the
// timeGetTime API instead of the default GetTickCount() API.

namespace base {

EventRecorder* EventRecorder::current_ = NULL;

LRESULT CALLBACK StaticRecordWndProc(int nCode, WPARAM wParam,
                                     LPARAM lParam) {
  CHECK(EventRecorder::current());
  return EventRecorder::current()->RecordWndProc(nCode, wParam, lParam);
}

LRESULT CALLBACK StaticPlaybackWndProc(int nCode, WPARAM wParam,
                                       LPARAM lParam) {
  CHECK(EventRecorder::current());
  return EventRecorder::current()->PlaybackWndProc(nCode, wParam, lParam);
}

EventRecorder::~EventRecorder() {
  // Try to assert early if the caller deletes the recorder
  // while it is still in use.
  DCHECK(!journal_hook_);
  DCHECK(!is_recording_ && !is_playing_);
}

bool EventRecorder::StartRecording(const FilePath& filename) {
  if (journal_hook_ != NULL)
    return false;
  if (is_recording_ || is_playing_)
    return false;

  // Open the recording file.
  DCHECK(file_ == NULL);
  file_ = file_util::OpenFile(filename, "wb+");
  if (!file_) {
    DLOG(ERROR) << "EventRecorder could not open log file";
    return false;
  }

  // Set the faster clock, if possible.
  ::timeBeginPeriod(1);

  // Set the recording hook.  JOURNALRECORD can only be used as a global hook.
  journal_hook_ = ::SetWindowsHookEx(WH_JOURNALRECORD, StaticRecordWndProc,
                                     GetModuleHandle(NULL), 0);
  if (!journal_hook_) {
    DLOG(ERROR) << "EventRecorder Record Hook failed";
    file_util::CloseFile(file_);
    return false;
  }

  is_recording_ = true;
  return true;
}

void EventRecorder::StopRecording() {
  if (is_recording_) {
    DCHECK(journal_hook_ != NULL);

    if (!::UnhookWindowsHookEx(journal_hook_)) {
      DLOG(ERROR) << "EventRecorder Unhook failed";
      // Nothing else we can really do here.
      return;
    }

    ::timeEndPeriod(1);

    DCHECK(file_ != NULL);
    file_util::CloseFile(file_);
    file_ = NULL;

    journal_hook_ = NULL;
    is_recording_ = false;
  }
}

bool EventRecorder::StartPlayback(const FilePath& filename) {
  if (journal_hook_ != NULL)
    return false;
  if (is_recording_ || is_playing_)
    return false;

  // Open the recording file.
  DCHECK(file_ == NULL);
  file_ = file_util::OpenFile(filename, "rb");
  if (!file_) {
    DLOG(ERROR) << "EventRecorder Playback could not open log file";
    return false;
  }
  // Read the first event from the record.
  if (fread(&playback_msg_, sizeof(EVENTMSG), 1, file_) != 1) {
    DLOG(ERROR) << "EventRecorder Playback has no records!";
    file_util::CloseFile(file_);
    return false;
  }

  // Set the faster clock, if possible.
  ::timeBeginPeriod(1);

  // Playback time is tricky.  When playing back, we read a series of events,
  // each with timeouts.  Simply subtracting the delta between two timers will
  // lead to fast playback (about 2x speed).  The API has two events, one
  // which advances to the next event (HC_SKIP), and another that requests the
  // event (HC_GETNEXT).  The same event will be requested multiple times.
  // Each time the event is requested, we must calculate the new delay.
  // To do this, we track the start time of the playback, and constantly
  // re-compute the delay.   I mention this only because I saw two examples
  // of how to use this code on the net, and both were broken :-)
  playback_start_time_ = timeGetTime();
  playback_first_msg_time_ = playback_msg_.time;

  // Set the hook.  JOURNALPLAYBACK can only be used as a global hook.
  journal_hook_ = ::SetWindowsHookEx(WH_JOURNALPLAYBACK, StaticPlaybackWndProc,
                                     GetModuleHandle(NULL), 0);
  if (!journal_hook_) {
    DLOG(ERROR) << "EventRecorder Playback Hook failed";
    return false;
  }

  is_playing_ = true;

  return true;
}

void EventRecorder::StopPlayback() {
  if (is_playing_) {
    DCHECK(journal_hook_ != NULL);

    if (!::UnhookWindowsHookEx(journal_hook_)) {
      DLOG(ERROR) << "EventRecorder Unhook failed";
      // Nothing else we can really do here.
    }

    DCHECK(file_ != NULL);
    file_util::CloseFile(file_);
    file_ = NULL;

    ::timeEndPeriod(1);

    journal_hook_ = NULL;
    is_playing_ = false;
  }
}

// Windows callback hook for the recorder.
LRESULT EventRecorder::RecordWndProc(int nCode, WPARAM wParam, LPARAM lParam) {
  static bool recording_enabled = true;
  EVENTMSG* msg_ptr = NULL;

  // The API says we have to do this.
  // See http://msdn2.microsoft.com/en-us/library/ms644983(VS.85).aspx
  if (nCode < 0)
    return ::CallNextHookEx(journal_hook_, nCode, wParam, lParam);

  // Check for the break key being pressed and stop recording.
  if (::GetKeyState(VK_CANCEL) & 0x8000) {
    StopRecording();
    return ::CallNextHookEx(journal_hook_, nCode, wParam, lParam);
  }

  // The Journal Recorder must stop recording events when system modal
  // dialogs are present. (see msdn link above)
  switch(nCode) {
    case HC_SYSMODALON:
      recording_enabled = false;
      break;
    case HC_SYSMODALOFF:
      recording_enabled = true;
      break;
  }

  if (nCode == HC_ACTION && recording_enabled) {
    // Aha - we have an event to record.
    msg_ptr = reinterpret_cast<EVENTMSG*>(lParam);
    msg_ptr->time = timeGetTime();
    fwrite(msg_ptr, sizeof(EVENTMSG), 1, file_);
    fflush(file_);
  }

  return CallNextHookEx(journal_hook_, nCode, wParam, lParam);
}

// Windows callback for the playback mode.
LRESULT EventRecorder::PlaybackWndProc(int nCode, WPARAM wParam,
                                       LPARAM lParam) {
  static bool playback_enabled = true;
  int delay = 0;

  switch(nCode) {
    // A system modal dialog box is being displayed.  Stop playing back
    // messages.
    case HC_SYSMODALON:
      playback_enabled = false;
      break;

    // A system modal dialog box is destroyed.  We can start playing back
    // messages again.
    case HC_SYSMODALOFF:
      playback_enabled = true;
      break;

    // Prepare to copy the next mouse or keyboard event to playback.
    case HC_SKIP:
      if (!playback_enabled)
        break;

      // Read the next event from the record.
      if (fread(&playback_msg_, sizeof(EVENTMSG), 1, file_) != 1)
        this->StopPlayback();
      break;

    // Copy the mouse or keyboard event to the EVENTMSG structure in lParam.
    case HC_GETNEXT:
      if (!playback_enabled)
        break;

      memcpy(reinterpret_cast<void*>(lParam), &playback_msg_,
             sizeof(playback_msg_));

      // The return value is the amount of time (in milliseconds) to wait
      // before playing back the next message in the playback queue.  Each
      // time this is called, we recalculate the delay relative to our current
      // wall clock.
      delay = (playback_msg_.time - playback_first_msg_time_) -
              (timeGetTime() - playback_start_time_);
      if (delay < 0)
        delay = 0;
      return delay;

    // An application has called PeekMessage with wRemoveMsg set to PM_NOREMOVE
    // indicating that the message is not removed from the message queue after
    // PeekMessage processing.
    case HC_NOREMOVE:
      break;
  }

  return CallNextHookEx(journal_hook_, nCode, wParam, lParam);
}

}  // namespace base
