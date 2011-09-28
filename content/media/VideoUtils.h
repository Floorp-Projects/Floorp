/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Pearce <chris@pearce.org.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef VideoUtils_h
#define VideoUtils_h

#include "mozilla/ReentrantMonitor.h"

#include "nsRect.h"
#include "nsIThreadManager.h"

// This file contains stuff we'd rather put elsewhere, but which is
// dependent on other changes which we don't want to wait for. We plan to
// remove this file in the near future.


// This belongs in prtypes.h
/************************************************************************
 * MACROS:      PR_INT64_MAX
 *              PR_INT64_MIN
 *              PR_UINT64_MAX
 * DESCRIPTION:
 *  The maximum and minimum values of a PRInt64 or PRUint64.
************************************************************************/

#define PR_INT64_MAX (~((PRInt64)(1) << 63))
#define PR_INT64_MIN (-PR_INT64_MAX - 1)
#define PR_UINT64_MAX (~(PRUint64)(0))

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

} // namespace mozilla

// Adds two 32bit unsigned numbers, retuns PR_TRUE if addition succeeded,
// or PR_FALSE the if addition would result in an overflow.
PRBool AddOverflow32(PRUint32 a, PRUint32 b, PRUint32& aResult);
 
// 32 bit integer multiplication with overflow checking. Returns PR_TRUE
// if the multiplication was successful, or PR_FALSE if the operation resulted
// in an integer overflow.
PRBool MulOverflow32(PRUint32 a, PRUint32 b, PRUint32& aResult);

// Adds two 64bit numbers, retuns PR_TRUE if addition succeeded, or PR_FALSE
// if addition would result in an overflow.
PRBool AddOverflow(PRInt64 a, PRInt64 b, PRInt64& aResult);

// 64 bit integer multiplication with overflow checking. Returns PR_TRUE
// if the multiplication was successful, or PR_FALSE if the operation resulted
// in an integer overflow.
PRBool MulOverflow(PRInt64 a, PRInt64 b, PRInt64& aResult);

// Converts from number of audio frames (aFrames) to microseconds, given
// the specified audio rate (aRate). Stores result in aOutUsecs. Returns PR_TRUE
// if the operation succeeded, or PR_FALSE if there was an integer overflow
// while calulating the conversion.
PRBool FramesToUsecs(PRInt64 aFrames, PRUint32 aRate, PRInt64& aOutUsecs);

// Converts from microseconds (aUsecs) to number of audio frames, given the
// specified audio rate (aRate). Stores the result in aOutFrames. Returns
// PR_TRUE if the operation succeeded, or PR_FALSE if there was an integer
// overflow while calulating the conversion.
PRBool UsecsToFrames(PRInt64 aUsecs, PRUint32 aRate, PRInt64& aOutFrames);

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
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(LINUX)
#define MEDIA_THREAD_STACK_SIZE (128 * 1024)
#else
// All other platforms use their system defaults.
#define MEDIA_THREAD_STACK_SIZE nsIThreadManager::DEFAULT_STACK_SIZE
#endif

#endif
