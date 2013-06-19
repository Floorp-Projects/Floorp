/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoUtils_h
#define VideoUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/CheckedInt.h"

#include "nsRect.h"
#include "nsIThreadManager.h"
#include "nsThreadUtils.h"
#include "prtime.h"

using mozilla::CheckedInt64;
using mozilla::CheckedUint64;
using mozilla::CheckedInt32;
using mozilla::CheckedUint32;

// This file contains stuff we'd rather put elsewhere, but which is
// dependent on other changes which we don't want to wait for. We plan to
// remove this file in the near future.


// This belongs in xpcom/monitor/Monitor.h, once we've made
// mozilla::Monitor non-reentrant.
namespace mozilla {

/**
 * ReentrantMonitorAutoExit
 * Exit the ReentrantMonitor when it enters scope, and enters it when it leaves 
 * scope.
 *
 * MUCH PREFERRED to bare calls to ReentrantMonitor.Exit and Enter.
 */ 
class MOZ_STACK_CLASS ReentrantMonitorAutoExit
{
public:
    /**
     * Constructor
     * The constructor releases the given lock.  The destructor
     * acquires the lock. The lock must be held before constructing
     * this object!
     * 
     * @param aReentrantMonitor A valid mozilla::ReentrantMonitor*. It
     *                 must be already locked.
     **/
    ReentrantMonitorAutoExit(ReentrantMonitor& aReentrantMonitor) :
        mReentrantMonitor(&aReentrantMonitor)
    {
        NS_ASSERTION(mReentrantMonitor, "null monitor");
        mReentrantMonitor->AssertCurrentThreadIn();
        mReentrantMonitor->Exit();
    }
    
    ~ReentrantMonitorAutoExit(void)
    {
        mReentrantMonitor->Enter();
    }
 
private:
    ReentrantMonitorAutoExit();
    ReentrantMonitorAutoExit(const ReentrantMonitorAutoExit&);
    ReentrantMonitorAutoExit& operator =(const ReentrantMonitorAutoExit&);
    static void* operator new(size_t) CPP_THROW_NEW;
    static void operator delete(void*);

    ReentrantMonitor* mReentrantMonitor;
};

/**
 * ReentrantMonitorConditionallyEnter
 *
 * Enters the supplied monitor only if the conditional value |aEnter| is true.
 * E.g. Used to allow unmonitored read access on the decode thread,
 * and monitored access on all other threads.
 */
class MOZ_STACK_CLASS ReentrantMonitorConditionallyEnter
{
public:
  ReentrantMonitorConditionallyEnter(bool aEnter,
                                     ReentrantMonitor &aReentrantMonitor) :
    mReentrantMonitor(nullptr)
  {
    MOZ_COUNT_CTOR(ReentrantMonitorConditionallyEnter);
    if (aEnter) {
      mReentrantMonitor = &aReentrantMonitor;
      NS_ASSERTION(mReentrantMonitor, "null monitor");
      mReentrantMonitor->Enter();
    }
  }
  ~ReentrantMonitorConditionallyEnter(void)
  {
    if (mReentrantMonitor) {
      mReentrantMonitor->Exit();
    }
    MOZ_COUNT_DTOR(ReentrantMonitorConditionallyEnter);
  }
private:
  // Restrict to constructor and destructor defined above.
  ReentrantMonitorConditionallyEnter();
  ReentrantMonitorConditionallyEnter(const ReentrantMonitorConditionallyEnter&);
  ReentrantMonitorConditionallyEnter& operator =(const ReentrantMonitorConditionallyEnter&);
  static void* operator new(size_t) CPP_THROW_NEW;
  static void operator delete(void*);

  ReentrantMonitor* mReentrantMonitor;
};

// Shuts down a thread asynchronously.
class ShutdownThreadEvent : public nsRunnable 
{
public:
  ShutdownThreadEvent(nsIThread* aThread) : mThread(aThread) {}
  ~ShutdownThreadEvent() {}
  NS_IMETHOD Run() MOZ_OVERRIDE {
    mThread->Shutdown();
    mThread = nullptr;
    return NS_OK;
  }
private:
  nsCOMPtr<nsIThread> mThread;
};

class MediaResource;
} // namespace mozilla

namespace mozilla {
namespace dom {
class TimeRanges;
}
}

// Estimates the buffered ranges of a MediaResource using a simple
// (byteOffset/length)*duration method. Probably inaccurate, but won't
// do file I/O, and can be used when we don't have detailed knowledge
// of the byte->time mapping of a resource. aDurationUsecs is the duration
// of the media in microseconds. Estimated buffered ranges are stored in
// aOutBuffered. Ranges are 0-normalized, i.e. in the range of (0,duration].
void GetEstimatedBufferedTimeRanges(mozilla::MediaResource* aStream,
                                    int64_t aDurationUsecs,
                                    mozilla::dom::TimeRanges* aOutBuffered);

// Converts from number of audio frames (aFrames) to microseconds, given
// the specified audio rate (aRate). Stores result in aOutUsecs. Returns true
// if the operation succeeded, or false if there was an integer overflow
// while calulating the conversion.
CheckedInt64 FramesToUsecs(int64_t aFrames, uint32_t aRate);

// Converts from microseconds (aUsecs) to number of audio frames, given the
// specified audio rate (aRate). Stores the result in aOutFrames. Returns
// true if the operation succeeded, or false if there was an integer
// overflow while calulating the conversion.
CheckedInt64 UsecsToFrames(int64_t aUsecs, uint32_t aRate);

// Number of microseconds per second. 1e6.
static const int64_t USECS_PER_S = 1000000;

// Number of microseconds per millisecond.
static const int64_t USECS_PER_MS = 1000;

// Converts seconds to milliseconds.
#define MS_TO_SECONDS(s) ((double)(s) / (PR_MSEC_PER_SEC))

// The maximum height and width of the video. Used for
// sanitizing the memory allocation of the RGB buffer.
// The maximum resolution we anticipate encountering in the
// wild is 2160p - 3840x2160 pixels.
static const int32_t MAX_VIDEO_WIDTH = 4000;
static const int32_t MAX_VIDEO_HEIGHT = 3000;

// Scales the display rect aDisplay by aspect ratio aAspectRatio.
// Note that aDisplay must be validated by VideoInfo::ValidateVideoRegion()
// before being used!
void ScaleDisplayByAspectRatio(nsIntSize& aDisplay, float aAspectRatio);

// The amount of virtual memory reserved for thread stacks.
#if (defined(XP_WIN) || defined(XP_MACOSX) || defined(LINUX)) && \
    !defined(MOZ_ASAN)
#define MEDIA_THREAD_STACK_SIZE (128 * 1024)
#else
// All other platforms use their system defaults.
#define MEDIA_THREAD_STACK_SIZE nsIThreadManager::DEFAULT_STACK_SIZE
#endif

#endif
