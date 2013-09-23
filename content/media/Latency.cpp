/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We want this available in opt builds
#define FORCE_PR_LOG

#include "Latency.h"
#include "nsThreadUtils.h"
#include <prlog.h>
#include <cmath>
#include <algorithm>

#include <mozilla/StaticPtr.h>

using namespace mozilla;

const char* LatencyLogIndex2Strings[] = {
  "Audio MediaStreamTrack",
  "Video MediaStreamTrack",
  "Cubeb",
  "AudioStream",
  "NetStat"
};

static StaticRefPtr<AsyncLatencyLogger> gAsyncLogger;

PRLogModuleInfo*
GetLatencyLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("MediaLatency");
  }
  return sLog;
}


class LogEvent : public nsRunnable
{
public:
  LogEvent(AsyncLatencyLogger::LatencyLogIndex aIndex, uint64_t aID, int64_t aValue) :
    mIndex(aIndex),
    mID(aID),
    mValue(aValue)
  {}
  ~LogEvent() {}

  NS_IMETHOD Run() {
    AsyncLatencyLogger::Get(true)->WriteLog(mIndex, mID, mValue);
    return NS_OK;
  }

protected:
  AsyncLatencyLogger::LatencyLogIndex mIndex;
  uint64_t mID;
  int64_t mValue;
};

// This is the only function that clients should use.
void LogLatency(AsyncLatencyLogger::LatencyLogIndex aIndex, uint64_t aID, int64_t aValue)
{
  AsyncLatencyLogger::Get()->Log(aIndex, aID, aValue);
}

void AsyncLatencyLogger::InitializeStatics()
{
  NS_ASSERTION(NS_IsMainThread(), "Main thread only");
  GetLatencyLog();
  gAsyncLogger = new AsyncLatencyLogger();
}

void AsyncLatencyLogger::Shutdown()
{
  gAsyncLogger = nullptr;
}

/* static */
AsyncLatencyLogger* AsyncLatencyLogger::Get(bool aStartTimer)
{
  if (aStartTimer) {
    gAsyncLogger->Init();
  }
  return gAsyncLogger;
}

AsyncLatencyLogger::AsyncLatencyLogger()
  : mThread(nullptr),
    mMutex("AsyncLatencyLogger")
{ }

AsyncLatencyLogger::~AsyncLatencyLogger()
{
  MutexAutoLock lock(mMutex);
  if (mThread) {
    mThread->Shutdown();
  }
  mStart = TimeStamp();
}

void AsyncLatencyLogger::Init()
{
  MutexAutoLock lock(mMutex);
  if (mStart.IsNull()) {
    mStart = TimeStamp::Now();
    nsresult rv = NS_NewNamedThread("Latency Logger", getter_AddRefs(mThread));
    NS_ENSURE_SUCCESS_VOID(rv);
  }
}

// aID is a sub-identifier (in particular a specific MediaStramTrack)
void AsyncLatencyLogger::WriteLog(LatencyLogIndex aIndex, uint64_t aID, int64_t aValue)
{
  PR_LOG(GetLatencyLog(), PR_LOG_DEBUG,
         ("%s,%llu,%lld.,%lld.",
          LatencyLogIndex2Strings[aIndex], aID, GetTimeStamp(), aValue));
}

int64_t AsyncLatencyLogger::GetTimeStamp()
{
  TimeDuration t = TimeStamp::Now() - mStart;
  return t.ToMilliseconds();
}

void AsyncLatencyLogger::Log(LatencyLogIndex aIndex, uint64_t aID, int64_t aValue)
{
  if (PR_LOG_TEST(GetLatencyLog(), PR_LOG_DEBUG)) {
    nsCOMPtr<nsIRunnable> event = new LogEvent(aIndex, aID, aValue);
    if (mThread) {
      mThread->Dispatch(event, NS_DISPATCH_NORMAL);
    }
  }
}

