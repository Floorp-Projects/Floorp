/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of an asynchronous lock-free logging system. */

#ifndef mozilla_dom_AsyncLogger_h
#define mozilla_dom_AsyncLogger_h

#include <atomic>
#include <thread>
#include "mozilla/Logging.h"
#include "mozilla/Attributes.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Sprintf.h"

namespace mozilla {

// This class implements a lock-free asynchronous logger, that outputs to
// MOZ_LOG.
// Any thread can use this logger without external synchronization and without
// being blocked. This log is suitable for use in real-time audio threads.
// Log formatting is best done externally, this class implements the output
// mechanism only.
// This class uses a thread internally, and must be started and stopped
// manually.
// If logging is disabled, all the calls are no-op.
class AsyncLogger {
 public:
  enum class TracingPhase : uint8_t { BEGIN, END, COMPLETE };

  const char TRACING_PHASE_STRINGS[3] = {'B', 'E', 'X'};

  enum AsyncLoggerOutputMode { MOZLOG, PROFILER };
  typedef char TextPayload[504];

  // aLogModuleName is the name of the MOZ_LOG module.
  explicit AsyncLogger(const char* aLogModuleName,
                       AsyncLogger::AsyncLoggerOutputMode aMode =
                           AsyncLogger::AsyncLoggerOutputMode::PROFILER)
      : mThread(nullptr), mLogModule(aLogModuleName), mRunning(false) {}

  void Start() {
    MOZ_ASSERT(!mRunning, "Double calls to AsyncLogger::Start");
    if (mMode == AsyncLogger::AsyncLoggerOutputMode::MOZLOG) {
      LogMozLog("[");
    }
    if (Enabled()) {
      mRunning = true;
      Run();
    }
  }

  void Stop() {
    if (Enabled()) {
      if (mRunning) {
        mRunning = false;
        mThread->join();
      }
    } else {
      MOZ_ASSERT(!mRunning && !mThread);
    }
  }

  // Log something that has a beginning and an end
  void Log(const char* aName, const char* aCategory, const char* aComment,
           TracingPhase aPhase) {
    if (Enabled()) {
      auto* msg = new detail::MPSCQueue<MAX_MESSAGE_LENGTH>::Message();
      va_list args;
      va_start(args, format);
      VsprintfLiteral(msg->data, format, args);
      va_end(args);
      mMessageQueue.Push(msg);
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
        // todo
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
            aName, aCategory, NowInUs(), aDuration,
            getpid(), std::hash<std::thread::id>{}(std::this_thread::get_id()),
            aFrames, aSampleRate);
      } else {
      }
    }
  }
  void LogMozLog(const char* format, ...) MOZ_FORMAT_PRINTF(2, 3) {
    auto* msg = new MPSCQueue<TextPayload>::Message();
    va_list args;
    va_start(args, format);
    VsprintfLiteral(msg->data, format, args);
    va_end(args);
    mMessageQueue.Push(msg);
  }

  bool Enabled() {
    return MOZ_LOG_TEST(mLogModule, mozilla::LogLevel::Verbose);
  }

 private:
  void Run() {
    MOZ_ASSERT(Enabled());
    mThread.reset(new std::thread([this]() {
      while (mRunning) {
        char message[MAX_MESSAGE_LENGTH];
        while (mMessageQueue.Pop(message) && mRunning) {
          MOZ_LOG(mLogModule, mozilla::LogLevel::Verbose, ("%s", message));
        }
        Sleep();
      }
    }));
  }

  uint64_t NowInUs() {
    static TimeStamp base = TimeStamp::NowUnfuzzed();
    return (TimeStamp::NowUnfuzzed() - base).ToMicroseconds();
  }

  void Sleep() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }

  std::unique_ptr<std::thread> mThread;
  mozilla::LazyLogModule mLogModule;
  detail::MPSCQueue<MAX_MESSAGE_LENGTH> mMessageQueue;
  std::atomic<bool> mRunning;
  std::atomic<AsyncLoggerOutputMode> mMode;
};

}  // end namespace mozilla

#endif  // mozilla_dom_AsyncLogger_h
