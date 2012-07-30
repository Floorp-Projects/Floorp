/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoUtils_h
#define VideoUtils_h

#include "mozilla/ReentrantMonitor.h"
#include "mozilla/CheckedInt.h"

#include "nsRect.h"
#include "nsIThreadManager.h"
#include "nsThreadUtils.h"

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
class NS_STACK_CLASS ReentrantMonitorAutoExit
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

// Shuts down a thread asynchronously.
class ShutdownThreadEvent : public nsRunnable 
{
public:
  ShutdownThreadEvent(nsIThread* aThread) : mThread(aThread) {}
  ~ShutdownThreadEvent() {}
  NS_IMETHOD Run() {
    mThread->Shutdown();
    mThread = nullptr;
    return NS_OK;
  }
private:
  nsCOMPtr<nsIThread> mThread;
};

} // namespace mozilla

// Converts from number of audio frames (aFrames) to microseconds, given
// the specified audio rate (aRate). Stores result in aOutUsecs. Returns true
// if the operation succeeded, or false if there was an integer overflow
// while calulating the conversion.
CheckedInt64 FramesToUsecs(PRInt64 aFrames, PRUint32 aRate);

// Converts from microseconds (aUsecs) to number of audio frames, given the
// specified audio rate (aRate). Stores the result in aOutFrames. Returns
// true if the operation succeeded, or false if there was an integer
// overflow while calulating the conversion.
CheckedInt64 UsecsToFrames(PRInt64 aUsecs, PRUint32 aRate);

// Number of microseconds per second. 1e6.
static const PRInt64 USECS_PER_S = 1000000;

// Number of microseconds per millisecond.
static const PRInt64 USECS_PER_MS = 1000;

// The maximum height and width of the video. Used for
// sanitizing the memory allocation of the RGB buffer.
// The maximum resolution we anticipate encountering in the
// wild is 2160p - 3840x2160 pixels.
static const PRInt32 MAX_VIDEO_WIDTH = 4000;
static const PRInt32 MAX_VIDEO_HEIGHT = 3000;

// Scales the display rect aDisplay by aspect ratio aAspectRatio.
// Note that aDisplay must be validated by nsVideoInfo::ValidateVideoRegion()
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
