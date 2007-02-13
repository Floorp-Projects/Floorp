/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

#ifndef __GCSpinLock__
#define __GCSpinLock__

#ifdef __GNUC__
#include <CoreServices/CoreServices.h>
#else // __GNUC__
#include <Multiprocessing.h>
#endif // __GNUC__

#if TARGET_RT_MAC_MACHO
extern "C"
{
extern void _spin_lock(uint32_t *);
extern void _spin_unlock(uint32_t *);
}
#endif

namespace MMgc
{
	/**
	 * GCSpinLock is a simple spin lock class used by GCHeap to
	 * ensure mutually exclusive access.  The GCHeap may be accessed
	 * by multiple threads, so this is necessary to ensure that
	 * the threads do not step on each other.
	 */
	class GCSpinLock
	{
	public:
		GCSpinLock()
		{
#if TARGET_RT_MAC_MACHO
			m1 = 0;
#else
			OSStatus critErr = ::MPCreateCriticalRegion( &mCriticalRegion );
			GCAssert( critErr == noErr );
#endif
		}
	
		~GCSpinLock()
		{
#if !TARGET_RT_MAC_MACHO
			OSStatus critErr = ::MPDeleteCriticalRegion( mCriticalRegion );
			GCAssert( critErr == noErr );
#endif
		}

		inline void Acquire()
		{
#if TARGET_RT_MAC_MACHO
			_spin_lock(&m1);
#else
			OSStatus critErr = ::MPEnterCriticalRegion( mCriticalRegion, kDurationForever );
			GCAssert( critErr == noErr );
#endif
		}
		
		inline void Release()
		{
#if TARGET_RT_MAC_MACHO
			_spin_unlock(&m1);
#else
			OSStatus critErr = ::MPExitCriticalRegion( mCriticalRegion );
			GCAssert( critErr == noErr );
#endif
		}

	private:

#if TARGET_RT_MAC_MACHO
		uint32_t m1;
#else
		MPCriticalRegionID  mCriticalRegion;
#endif
	};

	/**
	 * GCAcquireSpinlock is a convenience class which acquires
	 * the specified spinlock at construct time, then releases
	 * the spinlock at desruct time.  The single statement
	 *
	 *    GCAcquireSpinlock acquire(spinlock);
	 *
	 * ... will acquire the spinlock at the top of the function
	 * and release it at the end.  This makes for less error-prone
	 * code than explicit acquire/release.
	 */
	class GCAcquireSpinlock
	{
	public:
		GCAcquireSpinlock(GCSpinLock& spinlock) : m_spinlock(spinlock)
		{
			m_spinlock.Acquire();
		}
		~GCAcquireSpinlock()
		{
			m_spinlock.Release();
		}

	private:
		GCSpinLock& m_spinlock;
	};
}

#endif /* __GCSpinLock__ */
