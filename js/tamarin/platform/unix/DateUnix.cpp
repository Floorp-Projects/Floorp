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


#include <sys/time.h>
#include <math.h> 

#ifdef AVMPLUS_LINUX
#include <time.h>
#endif

#include "avmplus.h"

namespace avmplus
{
	/*
	 * Unix implementation of platform-dependent date and time code
	 */

#define kMsecPerDay     86400000
#define kMsecPerHour    3600000
#define kMsecPerSecond  1000
#define kMsecPerMinute  60000
 
#define DIVCLOCK ( CLOCKS_PER_SEC / 1000 )
 
#define kMicroPerSec 1000000.0

	double OSDep::localTZA(double /*ti*/)
	{
		struct tm* t;
		time_t current, localSec, globalSec;

		// The win32 implementation ignores the passed in time
		// and uses current time instead, so to keep similar
		// behaviour we will do the same
		time( &current );
	
		t = localtime( &current );
		localSec = mktime( t );

		t = gmtime( &current );
		globalSec = mktime( t );

		return double( localSec - globalSec ) * 1000.0;
	}
 
	double OSDep::getDate()
	{
		struct timeval tv;
		struct timezone tz; // Unused

		gettimeofday(&tv, &tz);
		double v = (tv.tv_sec + (tv.tv_usec/kMicroPerSec)) * kMsecPerSecond;
		double ip;
		::modf(v, &ip); // strip fractional part
		return ip;
	}
                  
	//time is passed in as milliseconds from UTC.
	double OSDep::daylightSavingTA(double newtime) // R41                           
	{
		struct tm *broken_down_time;

		//convert time from milliseconds
		newtime=newtime/kMsecPerSecond;

		long int longint_time=(long int) newtime;

		//pull out a struct tm
		broken_down_time = localtime( (const time_t*) &longint_time );

		if (!broken_down_time)
		{
			return 0;
		}

		if (broken_down_time->tm_isdst > 0)
		{
			//daylight saving is definitely in effect.
			//return # of milliseconds in one hour.
			return kMsecPerHour;
		}

		//either daylight saving is not in effect, or we don't know (if tm_isdst is negative).
		return 0;
	}
}
