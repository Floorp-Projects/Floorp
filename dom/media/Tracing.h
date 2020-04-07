/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRACING_H
#define TRACING_H

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "AsyncLogger.h"

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

#if defined(_WIN32)
#  include <process.h>
#  define getpid() _getpid()
#else
#  include <unistd.h>
#endif

#if defined(_MSC_VER)
// MSVC
#  define FUNCTION_SIGNATURE __FUNCSIG__
#elif defined(__GNUC__)
// gcc, clang
#  define FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#endif

extern mozilla::AsyncLogger gAudioCallbackTraceLogger;

// This is no-op if tracing is not enabled, and is idempotent.
void StartAudioCallbackTracing();
void StopAudioCallbackTracing();

#ifdef TRACING
/* TRACE is for use in the real-time audio rendering thread.
 * It would be better to always pass in the thread id. However, the thread an
 * audio callback runs on can change when the underlying audio device change,
 * and also it seems to be called from a thread pool in a round-robin fashion
 * when audio remoting is activated, making the traces unreadable.
 * The thread on which the AudioCallbackDriver::DataCallback is to always
 * be thread 0, and the budget is set to always be thread 1. This allows
 * displaying those elements in two separate lanes.
 * The other thread have "normal" tid. Hashing allows being able to get a
 * string representation that is unique and guaranteed to be portable. */
#  define TRACE()                                                \
    AutoTracer trace(                                            \
        gAudioCallbackTraceLogger, FUNCTION_SIGNATURE, getpid(), \
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
#  define TRACE_AUDIO_CALLBACK_BUDGET(aFrames, aSampleRate)                    \
    AutoTracer budget(gAudioCallbackTraceLogger, "Real-time budget", getpid(), \
                      1, AutoTracer::EventType::BUDGET, aFrames, aSampleRate);
#  define TRACE_COMMENT(aFmt, ...)                                             \
    AutoTracer trace(gAudioCallbackTraceLogger, FUNCTION_SIGNATURE, getpid(),  \
                     std::hash<std::thread::id>{}(std::this_thread::get_id()), \
                     AutoTracer::EventType::DURATION, aFmt, ##__VA_ARGS__);
#else
#  define TRACE()
#  define TRACE_AUDIO_CALLBACK_BUDGET(aFrames, aSampleRate)
#  define TRACE_COMMENT(aFmt, ...)
#endif

class MOZ_RAII AutoTracer {
 public:
  static const int32_t BUFFER_SIZE =
      mozilla::AsyncLogger::MAX_MESSAGE_LENGTH / 2;

  enum class EventType { DURATION, BUDGET };

  AutoTracer(mozilla::AsyncLogger& aLogger, const char* aLocation,
             uint64_t aPID, uint64_t aTID,
             EventType aEventType = EventType::DURATION,
             const char* aComment = nullptr);

  template <typename... Args>
  AutoTracer(mozilla::AsyncLogger& aLogger, const char* aLocation,
             uint64_t aPID, uint64_t aTID, EventType aEventType,
             const char* aFormat, Args... aArgs)
      : mLogger(aLogger),
        mLocation(aLocation),
        mComment(mBuffer),
        mEventType(aEventType),
        mPID(aPID),
        mTID(aTID) {
    MOZ_ASSERT(aEventType == EventType::DURATION);
    if (aLogger.Enabled()) {
      int32_t size = snprintf(mBuffer, BUFFER_SIZE, aFormat, aArgs...);
      size = std::min(size, BUFFER_SIZE - 1);
      mBuffer[size] = 0;
      PrintEvent(aLocation, "perf", mComment, TracingPhase::BEGIN, NowInUs(),
                 aPID, aTID);
    }
  }

  AutoTracer(mozilla::AsyncLogger& aLogger, const char* aLocation,
             uint64_t aPID, uint64_t aTID, EventType aEventType,
             uint64_t aFrames, uint64_t aSampleRate);

  ~AutoTracer();

 private:
  uint64_t NowInUs();

  enum class TracingPhase { BEGIN, END, COMPLETE };

  const char TRACING_PHASE_STRINGS[3] = {'B', 'E', 'X'};

  void PrintEvent(const char* aName, const char* aCategory,
                  const char* aComment, TracingPhase aPhase, uint64_t aTime,
                  uint64_t aPID, uint64_t aThread);

  void PrintBudget(const char* aName, const char* aCategory, uint64_t aDuration,
                   uint64_t aPID, uint64_t aThread, uint64_t aFrames,
                   uint64_t aSampleRate);

  // The logger to use. It musdt have a lifetime longer than the block an
  // instance of this class traces.
  mozilla::AsyncLogger& mLogger;
  // The location for this trace point, arbitrary string literal, often the
  // name of the calling function, with a static lifetime.
  const char* mLocation;
  // A comment for this trace point, abitrary string literal with a static
  // lifetime.
  const char* mComment;
  // A buffer used to hold string-formatted traces.
  char mBuffer[BUFFER_SIZE];
  // The event type, for now either a budget or a duration.
  const EventType mEventType;
  // The process ID of the calling process. Traces are grouped by PID in the
  // vizualizer.
  const uint64_t mPID;
  // The thread ID of the calling thread, will be displayed in a separate
  // section in the trace visualizer.
  const uint64_t mTID;
};

#if defined(_WIN32)
#  undef getpid
#endif

#endif /* TRACING_H */
