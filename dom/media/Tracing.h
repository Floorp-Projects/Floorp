/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRACING_H
#define TRACING_H

#include <cstdint>

#include "AsyncLogger.h"

#include <mozilla/Attributes.h>

#if defined(_WIN32)
#include <process.h>
#define getpid() _getpid()
#else
#include <unistd.h>
#endif

#define TRACING

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
  #define TRACE_AUDIO_CALLBACK(aLogger)                                        \
    AutoTracer trace(aLogger, __PRETTY_FUNCTION__, getpid(), 0);
  #define TRACE_AUDIO_CALLBACK_BUDGET(aLogger, aFrames, aSampleRate)           \
    AutoTracer budget(aLogger, "Real-time budget", getpid(), 1,                \
                      AutoTracer::EventType::BUDGET, aFrames, aSampleRate);
  #define TRACE(aLogger)                                                       \
    AutoTracer trace(aLogger, __PRETTY_FUNCTION__, getpid(),                   \
                     std::hash<std::thread::id>{}(std::this_thread::get_id()));
  #define TRACE_COMMENT(aLogger, aComment)                                     \
    AutoTracer trace(aLogger, __PRETTY_FUNCTION__, getpid(),                   \
                     std::hash<std::thread::id>{}(std::this_thread::get_id()), \
                     AutoTracer::EventType::DURATION,                          \
                     aComment);
#else
  #define TRACE_AUDIO_CALLBACK(aLogger)
  #define TRACE_AUDIO_CALLBACK_BUDGET(aLogger, aFrames, aSampleRate)
  #define TRACE(aLogger)
  #define TRACE_COMMENT(aLogger, aComment)
#endif


class MOZ_RAII AutoTracer
{
public:
  enum class EventType
  {
    DURATION,
    BUDGET
  };

  AutoTracer(mozilla::AsyncLogger& aLogger,
             const char* aLocation,
             uint64_t aPID,
             uint64_t aTID,
             EventType aEventType = EventType::DURATION,
             const char* aComment = nullptr);
  AutoTracer(mozilla::AsyncLogger& aLogger,
             const char* aLocation,
             uint64_t aPID,
             uint64_t aTID,
             EventType aEventType,
             uint64_t aSampleRate,
             uint64_t aFrames);
  ~AutoTracer();
private:
  uint64_t NowInUs();

  enum TracingPhase
  {
    BEGIN,
    END,
    COMPLETE
  };

  const char TRACING_PHASE_STRINGS[3] = {
    'B',
    'E',
    'X'
  };

  void PrintEvent(const char* aName,
                  const char* aCategory,
                  const char* aComment,
                  TracingPhase aPhase,
                  uint64_t aTime,
                  uint64_t aPID,
                  uint64_t aThread);

  void PrintBudget(const char* aName,
                   const char* aCategory,
                   const char* aComment,
                   uint64_t aDuration,
                   uint64_t aPID,
                   uint64_t aThread,
                   uint64_t aFrames);

  // The logger to use. It has a lifetime longer than the block an instance of
  // this class traces.
  mozilla::AsyncLogger& mLogger;
  // The location for this trace point, arbitrary string litteral, often the
  // name of the calling function, with a static lifetime.
  const char* mLocation;
  // A comment for this trace point, abitrary string litteral with a static
  // lifetime.
  const char* mComment;
  // The event type, for now either a budget or a duration.
  const EventType mEventType;
  // The process ID of the calling process, will be displayed in a separate
  // section in the trace visualizer.
  const uint64_t mPID;
  // The thread ID of the calling thread, will be displayed in a separate
  // section in the trace visualizer.
  const uint64_t mTID;
  // Whether or not the logger is enabled, controling the output.
  const bool mLoggerEnabled;
};

#if defined(_WIN32)
#undef getpid
#endif

#endif /* TRACING_H */
