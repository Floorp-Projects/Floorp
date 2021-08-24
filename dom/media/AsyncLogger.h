/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of an asynchronous lock-free logging system. */

#ifndef mozilla_dom_AsyncLogger_h
#define mozilla_dom_AsyncLogger_h

#include <atomic>
#include <thread>
#include <cinttypes>
#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TimeStamp.h"
#include "GeckoProfiler.h"
#include "MPSCQueue.h"

#if defined(_WIN32)
#  include <process.h>
#  define getpid() _getpid()
#else
#  include <unistd.h>
#endif

namespace mozilla {

const size_t PAYLOAD_TOTAL_SIZE = 2 << 9;

// This class implements a lock-free asynchronous logger, that outputs to
// MOZ_LOG or adds profiler markers (the default).
// Any thread can use this logger without external synchronization and without
// being blocked. This log is suitable for use in real-time audio threads.
// This class uses a thread internally, and must be started and stopped
// manually.
// If logging/profiling is disabled, all the calls are no-op and cheap.
class AsyncLogger {
 public:
  enum class TracingPhase : uint8_t { BEGIN, END, COMPLETE };

  const char TRACING_PHASE_STRINGS[3] = {'B', 'E', 'X'};

  enum AsyncLoggerOutputMode { MOZLOG, PROFILER };

  struct TextPayload {
    char mPayload[PAYLOAD_TOTAL_SIZE - MPSC_MSG_RESERVERD];
  };

  // The order of the fields is important here to minimize padding.
  struct TracePayload {
#define MEMBERS_EXCEPT_NAME                                                  \
  /* If this marker is of phase B or E (begin or end), this is the time at   \
   * which it was captured. */                                               \
  TimeStamp mTimestamp;                                                      \
  /* The thread on which this tracepoint was gathered. */                    \
  ProfilerThreadId mTID;                                                     \
  /* If this marker is of phase X (COMPLETE), this holds the duration of the \
   * event in microseconds. Else, the value is not used. */                  \
  uint32_t mDurationUs;                                                      \
  /* A trace payload can be either:                                          \
   * - Begin - this marks the beginning of a temporal region                 \
   * - End - this marks the end of a temporal region                         \
   * - Complete - this is a timestamp and a length, forming complete a       \
   * temporal region */                                                      \
  TracingPhase mPhase

    MEMBERS_EXCEPT_NAME;

   private:
    // Mock structure, to know where the first character of the name will be.
    struct MembersWithChar {
      MEMBERS_EXCEPT_NAME;
      char c;
    };
    static constexpr size_t scRemainingSpaceForName =
        PAYLOAD_TOTAL_SIZE - offsetof(MembersWithChar, c) -
        ((MPSC_MSG_RESERVERD + alignof(MembersWithChar) - 1) &
         ~(alignof(MembersWithChar) - 1));
#undef MEMBERS_EXCEPT_NAME

   public:
    // An arbitrary string, usually containing a function signature or a
    // recognizable tag of some sort, to be displayed when analyzing the
    // profile.
    char mName[scRemainingSpaceForName];
  };

  // The goal here is to make it easy on the allocator. We pack a pointer in the
  // message struct, and we still want to do power of two allocations to
  // minimize allocator slop.
  static_assert(sizeof(MPSCQueue<TracePayload>::Message) == PAYLOAD_TOTAL_SIZE,
                "MPSCQueue internal allocations has an unexpected size.");

  // aLogModuleName is the name of the MOZ_LOG module.
  explicit AsyncLogger(const char* aLogModuleName,
                       AsyncLogger::AsyncLoggerOutputMode aMode =
                           AsyncLogger::AsyncLoggerOutputMode::PROFILER)
      : mThread(nullptr),
        mLogModule(aLogModuleName),
        mRunning(false),
        mMode(aMode) {}

  void Start() {
    MOZ_ASSERT(!mRunning, "Double calls to AsyncLogger::Start");
    mRunning = true;
    if (mMode == AsyncLogger::AsyncLoggerOutputMode::MOZLOG) {
      LogMozLog("[");
    }
    Run();
  }

  void Stop() {
    if (mRunning) {
      mRunning = false;
    }
  }

  // Log something that has a beginning and an end
  void Log(const char* aName, const char* aCategory, const char* aComment,
           TracingPhase aPhase) {
    if (Enabled()) {
      if (mMode == AsyncLogger::AsyncLoggerOutputMode::MOZLOG) {
        LogMozLog(
            "{\"name\": \"%s\", \"cat\": \"%s\", \"ph\": \"%c\","
            "\"ts\": %" PRIu64
            ", \"pid\": %d, \"tid\":"
            " %zu, \"args\": { \"comment\": \"%s\"}},",
            aName, aCategory, TRACING_PHASE_STRINGS[static_cast<int>(aPhase)],
            NowInUs(), getpid(),
            std::hash<std::thread::id>{}(std::this_thread::get_id()), aComment);
      } else {
        auto* msg = new MPSCQueue<TracePayload>::Message();
        msg->data.mTID = profiler_current_thread_id();
        msg->data.mPhase = aPhase;
        msg->data.mTimestamp = TimeStamp::Now();
        msg->data.mDurationUs = 0;  // unused, duration is end - begin
        size_t len = std::min(strlen(aName), ArrayLength(msg->data.mName));
        memcpy(msg->data.mName, aName, len);
        msg->data.mName[len] = 0;
        mMessageQueueProfiler.Push(msg);
      }
    }
  }

  // Log something that has a beginning and a duration
  void LogDuration(const char* aName, const char* aCategory, uint64_t aDuration,
                   uint64_t aFrames, uint64_t aSampleRate) {
    if (Enabled()) {
      if (mMode == AsyncLogger::AsyncLoggerOutputMode::MOZLOG) {
        LogMozLog(
            "{\"name\": \"%s\", \"cat\": \"%s\", \"ph\": \"X\","
            "\"ts\": %" PRIu64 ", \"dur\": %" PRIu64
            ", \"pid\": %d,"
            "\"tid\": %zu, \"args\": { \"comment\": \"%" PRIu64 "/%" PRIu64
            "\"}},",
            aName, aCategory, NowInUs(), aDuration, getpid(),
            std::hash<std::thread::id>{}(std::this_thread::get_id()), aFrames,
            aSampleRate);
      } else {
        auto* msg = new MPSCQueue<TracePayload>::Message();
        msg->data.mTID = profiler_current_thread_id();
        msg->data.mPhase = TracingPhase::COMPLETE;
        msg->data.mTimestamp = TimeStamp::Now();
        msg->data.mDurationUs =
            (static_cast<double>(aFrames) / aSampleRate) * 1e6;
        size_t len = std::min(strlen(aName), ArrayLength(msg->data.mName));
        memcpy(msg->data.mName, aName, len);
        msg->data.mName[len] = 0;
        mMessageQueueProfiler.Push(msg);
      }
    }
  }
  void LogMozLog(const char* format, ...) MOZ_FORMAT_PRINTF(2, 3) {
    auto* msg = new MPSCQueue<TextPayload>::Message();
    va_list args;
    va_start(args, format);
    VsprintfLiteral(msg->data.mPayload, format, args);
    va_end(args);
    mMessageQueueLog.Push(msg);
  }

  bool Enabled() {
    return (mMode == AsyncLoggerOutputMode::MOZLOG &&
            MOZ_LOG_TEST(mLogModule, mozilla::LogLevel::Verbose)) ||
           (mMode == AsyncLoggerOutputMode::PROFILER && profiler_is_active());
  }

 private:
  void Run() {
    mThread.reset(new std::thread([this]() {
      AUTO_PROFILER_REGISTER_THREAD("AsyncLogger");
      while (mRunning) {
        {
          TextPayload message;
          while (mMessageQueueLog.Pop(&message) && mRunning) {
            MOZ_LOG(mLogModule, mozilla::LogLevel::Verbose,
                    ("%s", message.mPayload));
          }
        }
        {
          struct BudgetMarker {
            static constexpr Span<const char> MarkerTypeName() {
              return MakeStringSpan("Budget");
            }
            static void StreamJSONMarkerData(
                baseprofiler::SpliceableJSONWriter& aWriter) {}
            static MarkerSchema MarkerTypeDisplay() {
              using MS = MarkerSchema;
              MS schema{MS::Location::markerChart, MS::Location::markerTable};
              // Nothing outside the defaults.
              return schema;
            }
          };

          TracePayload message;
          while (mMessageQueueProfiler.Pop(&message) && mRunning) {
            if (message.mPhase != TracingPhase::COMPLETE) {
              profiler_add_marker(
                  ProfilerString8View::WrapNullTerminatedString(message.mName),
                  geckoprofiler::category::MEDIA_RT,
                  {MarkerThreadId(message.mTID),
                   (message.mPhase == TracingPhase::BEGIN)
                       ? MarkerTiming::IntervalStart(message.mTimestamp)
                       : MarkerTiming::IntervalEnd(message.mTimestamp)},
                  BudgetMarker{});
            } else {
              profiler_add_marker(
                  ProfilerString8View::WrapNullTerminatedString(message.mName),
                  geckoprofiler::category::MEDIA_RT,
                  {MarkerThreadId(message.mTID),
                   MarkerTiming::Interval(
                       message.mTimestamp,
                       message.mTimestamp + TimeDuration::FromMicroseconds(
                                                message.mDurationUs))},
                  BudgetMarker{});
            }
          }
        }
        Sleep();
      }
    }));
    // cleanup is done via mRunning
    mThread->detach();
  }

  uint64_t NowInUs() {
    static TimeStamp base = TimeStamp::Now();
    return (TimeStamp::Now() - base).ToMicroseconds();
  }

  void Sleep() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }

  std::unique_ptr<std::thread> mThread;
  mozilla::LazyLogModule mLogModule;
  MPSCQueue<TextPayload> mMessageQueueLog;
  MPSCQueue<TracePayload> mMessageQueueProfiler;
  std::atomic<bool> mRunning;
  std::atomic<AsyncLoggerOutputMode> mMode;
};

}  // end namespace mozilla

#if defined(_WIN32)
#  undef getpid
#endif

#endif  // mozilla_dom_AsyncLogger_h
