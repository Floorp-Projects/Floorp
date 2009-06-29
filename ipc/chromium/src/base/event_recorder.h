// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_EVENT_RECORDER_H_
#define BASE_EVENT_RECORDER_H_

#include <string>
#if defined(OS_WIN)
#include <windows.h>
#endif
#include "base/basictypes.h"

class FilePath;

namespace base {

// A class for recording and playing back keyboard and mouse input events.
//
// Note - if you record events, and the playback with the windows in
//        different sizes or positions, the playback will fail.  When
//        recording and playing, you should move the relevant windows
//        to constant sizes and locations.
// TODO(mbelshe) For now this is a singleton.  I believe that this class
//        could be easily modified to:
//             support two simultaneous recorders
//             be playing back events while already recording events.
//        Why?  Imagine if the product had a "record a macro" feature.
//        You might be recording globally, while recording or playing back
//        a macro.  I don't think two playbacks make sense.
class EventRecorder {
 public:
  // Get the singleton EventRecorder.
  // We can only handle one recorder/player at a time.
  static EventRecorder* current() {
    if (!current_)
      current_ = new EventRecorder();
    return current_;
  }

  // Starts recording events.
  // Will clobber the file if it already exists.
  // Returns true on success, or false if an error occurred.
  bool StartRecording(const FilePath& filename);

  // Stops recording.
  void StopRecording();

  // Is the EventRecorder currently recording.
  bool is_recording() const { return is_recording_; }

  // Plays events previously recorded.
  // Returns true on success, or false if an error occurred.
  bool StartPlayback(const FilePath& filename);

  // Stops playback.
  void StopPlayback();

  // Is the EventRecorder currently playing.
  bool is_playing() const { return is_playing_; }

#if defined(OS_WIN)
  // C-style callbacks for the EventRecorder.
  // Used for internal purposes only.
  LRESULT RecordWndProc(int nCode, WPARAM wParam, LPARAM lParam);
  LRESULT PlaybackWndProc(int nCode, WPARAM wParam, LPARAM lParam);
#endif

 private:
  // Create a new EventRecorder.  Events are saved to the file filename.
  // If the file already exists, it will be deleted before recording
  // starts.
  explicit EventRecorder()
      : is_recording_(false),
        is_playing_(false),
#if defined(OS_WIN)
        journal_hook_(NULL),
        file_(NULL),
#endif
        playback_first_msg_time_(0),
        playback_start_time_(0) {
  }
  ~EventRecorder();

  static EventRecorder* current_;  // Our singleton.

  bool is_recording_;
  bool is_playing_;
#if defined(OS_WIN)
  HHOOK journal_hook_;
  FILE* file_;
  EVENTMSG playback_msg_;
#endif
  int playback_first_msg_time_;
  int playback_start_time_;

  DISALLOW_EVIL_CONSTRUCTORS(EventRecorder);
};

}  // namespace base

#endif // BASE_EVENT_RECORDER_H_
