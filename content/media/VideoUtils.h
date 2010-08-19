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

#include "mozilla/Monitor.h"

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
 * MonitorAutoExit
 * Exit the Monitor when it enters scope, and enters it when it leaves 
 * scope.
 *
 * MUCH PREFERRED to bare calls to Monitor.Exit and Enter.
 */ 
class NS_STACK_CLASS MonitorAutoExit
{
public:
    /**
     * Constructor
     * The constructor releases the given lock.  The destructor
     * acquires the lock. The lock must be held before constructing
     * this object!
     * 
     * @param aMonitor A valid mozilla::Monitor* returned by 
     *                 mozilla::Monitor::NewMonitor. It must be
     *                 already locked.
     **/
    MonitorAutoExit(mozilla::Monitor &aMonitor) :
        mMonitor(&aMonitor)
    {
        NS_ASSERTION(mMonitor, "null monitor");
        mMonitor->AssertCurrentThreadIn();
        mMonitor->Exit();
    }
    
    ~MonitorAutoExit(void)
    {
        mMonitor->Enter();
    }
 
private:
    MonitorAutoExit();
    MonitorAutoExit(const MonitorAutoExit&);
    MonitorAutoExit& operator =(const MonitorAutoExit&);
    static void* operator new(size_t) CPP_THROW_NEW;
    static void operator delete(void*);

    mozilla::Monitor* mMonitor;
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

// Converts from number of audio samples (aSamples) to milliseconds, given
// the specified audio rate (aRate). Stores result in aOutMs. Returns PR_TRUE
// if the operation succeeded, or PR_FALSE if there was an integer overflow
// while calulating the conversion.
PRBool SamplesToMs(PRInt64 aSamples, PRUint32 aRate, PRInt64& aOutMs);

// Converts from milliseconds (aMs) to number of audio samples, given the
// specified audio rate (aRate). Stores the result in aOutSamples. Returns
// PR_TRUE if the operation succeeded, or PR_FALSE if there was an integer
// overflow while calulating the conversion.
PRBool MsToSamples(PRInt64 aMs, PRUint32 aRate, PRInt64& aOutSamples);

#endif
