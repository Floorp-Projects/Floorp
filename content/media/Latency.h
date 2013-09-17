/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LATENCY_H
#define MOZILLA_LATENCY_H

#include "mozilla/TimeStamp.h"
#include "prlog.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "mozilla/Monitor.h"
#include "nsISupportsImpl.h"

class AsyncLatencyLogger;
class LogEvent;

PRLogModuleInfo* GetLatencyLog();

// This class is a singleton. It is refcounted.
class AsyncLatencyLogger
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncLatencyLogger);
public:
  enum LatencyLogIndex {
    AudioMediaStreamTrack,
    VideoMediaStreamTrack,
    Cubeb,
    AudioStream,
    NetEQ,
    _MAX_INDEX
  };
  void Log(LatencyLogIndex index, uint64_t aID, int64_t value);
  void WriteLog(LatencyLogIndex index, uint64_t aID, int64_t value);

  static AsyncLatencyLogger* Get(bool aStartTimer = false);
  static void InitializeStatics();
  static void Shutdown();
private:
  AsyncLatencyLogger();
  ~AsyncLatencyLogger();
  int64_t GetTimeStamp();
  void Init();
  // The thread on which the IO happens
  nsCOMPtr<nsIThread> mThread;
  // This can be initialized on multiple threads, but is protected by a
  // monitor. After the initialization phase, it is accessed on the log
  // thread only.
  mozilla::TimeStamp mStart;
  // This monitor protects mStart and mMediaLatencyLog for the
  // initialization sequence. It is initialized at layout startup, and
  // destroyed at layout shutdown.
  mozilla::Mutex mMutex;
};

void LogLatency(AsyncLatencyLogger::LatencyLogIndex index, uint64_t aID, int64_t value);

#endif
