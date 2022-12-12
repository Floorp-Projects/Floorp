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
#include "mozilla/BaseProfilerMarkerTypes.h"
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

// Allows writing 0-terminated C-strings in a buffer, and returns the start
// index of the string that's been appended. Automatically truncates the strings
// as needed if the storage is too small, returning true when that's the case.
class MOZ_STACK_CLASS StringWriter {
 public:
  StringWriter(char* aMemory, size_t aLength)
      : mMemory(aMemory), mWriteIndex(0), mLength(aLength) {}

  bool AppendCString(const char* aString, size_t* aIndexStart) {
    *aIndexStart = mWriteIndex;
    if (!aString) {
      return false;
    }
    size_t toCopy = strlen(aString);
    bool truncated = false;

    if (toCopy > Available()) {
      truncated = true;
      toCopy = Available() - 1;
    }

    memcpy(&(mMemory[mWriteIndex]), aString, toCopy);
    mWriteIndex += toCopy;
    mMemory[mWriteIndex] = 0;
    mWriteIndex++;

    return truncated;
  }

 private:
  size_t Available() {
    MOZ_ASSERT(mLength > mWriteIndex);
    return mLength - mWriteIndex;
  }

  char* mMemory;
  size_t mWriteIndex;
  size_t mLength;
};

const size_t PAYLOAD_TOTAL_SIZE = 2 << 9;

// This class implements a lock-free asynchronous logger, that
// adds profiler markers.
// Any thread can use this logger without external synchronization and without
// being blocked. This log is suitable for use in real-time audio threads.
// This class uses a thread internally, and must be started and stopped
// manually.
// If profiling is disabled, all the calls are no-op and cheap.
class AsyncLogger {
 public:
  enum class TracingPhase : uint8_t { BEGIN, END, COMPLETE };

  const char TRACING_PHASE_STRINGS[3] = {'B', 'E', 'X'};

  struct TextPayload {
    char mPayload[PAYLOAD_TOTAL_SIZE - MPSC_MSG_RESERVED];
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
  TracingPhase mPhase;                                                       \
  /* Offset at which the comment part of the string starts, in mName */      \
  uint8_t mCommentStart;

    MEMBERS_EXCEPT_NAME;

   private:
    // Mock structure, to know where the first character of the name will be.
    struct MembersWithChar {
      MEMBERS_EXCEPT_NAME;
      char c;
    };
    static constexpr size_t scRemainingSpaceForName =
        PAYLOAD_TOTAL_SIZE - offsetof(MembersWithChar, c) -
        ((MPSC_MSG_RESERVED + alignof(MembersWithChar) - 1) &
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

  explicit AsyncLogger() : mThread(nullptr), mRunning(false) {}

  void Start() {
    MOZ_ASSERT(!mRunning, "Double calls to AsyncLogger::Start");
    mRunning = true;
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
    if (!Enabled()) {
      return;
    }

    auto* msg = new MPSCQueue<TracePayload>::Message();

    msg->data.mTID = profiler_current_thread_id();
    msg->data.mPhase = aPhase;
    msg->data.mTimestamp = TimeStamp::Now();
    msg->data.mDurationUs = 0;  // unused, duration is end - begin

    StringWriter writer(msg->data.mName, ArrayLength(msg->data.mName));

    size_t commentIndex;
    DebugOnly<bool> truncated = writer.AppendCString(aName, &commentIndex);
    MOZ_ASSERT(!truncated, "Tracing payload truncated: name");

    if (aComment) {
      truncated = writer.AppendCString(aComment, &commentIndex);
      MOZ_ASSERT(!truncated, "Tracing payload truncated: comment");
      msg->data.mCommentStart = commentIndex;
    } else {
      msg->data.mCommentStart = 0;
    }
    mMessageQueueProfiler.Push(msg);
  }

  // Log something that has a beginning and a duration
  void LogDuration(const char* aName, const char* aCategory, uint64_t aDuration,
                   uint64_t aFrames, uint64_t aSampleRate) {
    if (Enabled()) {
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

  bool Enabled() { return mRunning; }

 private:
  void Run() {
    mThread.reset(new std::thread([this]() {
      AUTO_PROFILER_REGISTER_THREAD("AsyncLogger");
      while (mRunning) {
        {
          struct TracingMarkerWithComment {
            static constexpr Span<const char> MarkerTypeName() {
              return MakeStringSpan("Real-Time");
            }
            static void StreamJSONMarkerData(
                baseprofiler::SpliceableJSONWriter& aWriter,
                const ProfilerString8View& aText) {
              aWriter.StringProperty("name", aText);
            }
            static MarkerSchema MarkerTypeDisplay() {
              using MS = MarkerSchema;
              MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
              schema.SetChartLabel("{marker.data.name}");
              schema.SetTableLabel("{marker.name} - {marker.data.name}");
              schema.AddKeyLabelFormatSearchable("name", "Comment",
                                                 MS::Format::String,
                                                 MS::Searchable::Searchable);
              return schema;
            }
          };

          struct TracingMarker {
            static constexpr Span<const char> MarkerTypeName() {
              return MakeStringSpan("Real-time");
            }
            static void StreamJSONMarkerData(
                baseprofiler::SpliceableJSONWriter& aWriter) {}
            static MarkerSchema MarkerTypeDisplay() {
              using MS = MarkerSchema;
              MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
              // Nothing outside the defaults.
              return schema;
            }
          };

          TracePayload message;
          while (mMessageQueueProfiler.Pop(&message) && mRunning) {
            if (message.mPhase != TracingPhase::COMPLETE) {
              if (!message.mCommentStart) {
                profiler_add_marker(
                    ProfilerString8View::WrapNullTerminatedString(
                        message.mName),
                    geckoprofiler::category::MEDIA_RT,
                    {MarkerThreadId(message.mTID),
                     (message.mPhase == TracingPhase::BEGIN)
                         ? MarkerTiming::IntervalStart(message.mTimestamp)
                         : MarkerTiming::IntervalEnd(message.mTimestamp)},
                    TracingMarker{});
              } else {
                profiler_add_marker(
                    ProfilerString8View::WrapNullTerminatedString(
                        message.mName),
                    geckoprofiler::category::MEDIA_RT,
                    {MarkerThreadId(message.mTID),
                     (message.mPhase == TracingPhase::BEGIN)
                         ? MarkerTiming::IntervalStart(message.mTimestamp)
                         : MarkerTiming::IntervalEnd(message.mTimestamp)},
                    TracingMarkerWithComment{},
                    ProfilerString8View::WrapNullTerminatedString(
                        &(message.mName[message.mCommentStart])));
              }
            } else {
              profiler_add_marker(
                  ProfilerString8View::WrapNullTerminatedString(message.mName),
                  geckoprofiler::category::MEDIA_RT,
                  {MarkerThreadId(message.mTID),
                   MarkerTiming::Interval(
                       message.mTimestamp,
                       message.mTimestamp + TimeDuration::FromMicroseconds(
                                                message.mDurationUs))},
                  TracingMarker{});
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
  MPSCQueue<TracePayload> mMessageQueueProfiler;
  std::atomic<bool> mRunning;
};

}  // end namespace mozilla

#if defined(_WIN32)
#  undef getpid
#endif

#endif  // mozilla_dom_AsyncLogger_h
