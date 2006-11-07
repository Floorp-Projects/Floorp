/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

#include "avmplus.h"
#include "DateMac.h"

#include <OSUtils.h>

namespace avmplus
{
	double UTC(double t);

	namespace MacDateTime
	{
		/**
		 * Constants used internally
		 */
		const int kMsecPerSecond          = 1000;
		const int kMsecPerDay             = 86400000;
		const int kMsecPerHour            = 3600000;
		const int kMsecPerMinute          = 60000;
		const unsigned long kMacEpochBias = 2082844800;
		const double kTwoPower32          = (4294967296.0); /* 2^32 */

		double       g_timeAtStartup;
		UnsignedWide g_microsecondsAtStartup;

		/**
		 * Core Foundation internal functions
		 */
		CFAbsoluteTime ECMADateToCFAbsoluteTime(double ecmaDate);
		double CFAbsoluteTimeToECMADate(CFAbsoluteTime time);
	
		/**
		 * Classic internal functions
		 */
		void ReadMachineLocation(MachineLocation* loc);
		void MicrosecondDelta(register const UnsignedWide *startPtr,
							  register const UnsignedWide *endPtr,
							  register UnsignedWide *resultPtr);					  

		/**
		 * MacDateTime::ECMADateToCFAbsoluteTime
		 *
		 * Converts an ECMAscript time (milliseconds since Jan 1 1970 GMT)
		 * to a Macintosh CFAbsoluteTime. 
		 */
		CFAbsoluteTime MacDateTime::ECMADateToCFAbsoluteTime(double ecmaDate)
		{
			CFAbsoluteTime result = ecmaDate;
	
			// ECMAScript times are in milliseconds, CFAbsoluteTime
			// is in seconds.
			result /= kMsecPerSecond;
	
			// ECMAScript time is based at January 1 1970 midnight GMT.
			result -= kCFAbsoluteTimeIntervalSince1970;
	
			return result;
		}

		/**
		 * MacDateTime::ECMADateToCFAbsoluteTime
		 *
		 * Converts a CFAbsoluteTime to an ECMAscript time (milliseconds since
		 * Jan 1 1970 GMT)
		 */
		double MacDateTime::CFAbsoluteTimeToECMADate(CFAbsoluteTime time)
		{
			double result = time;
	
			// ECMAScript time is based at January 1 1970 midnight GMT.
			result += kCFAbsoluteTimeIntervalSince1970;
	
			// ECMAScript times are in milliseconds, CFAbsoluteTime
			// is in seconds.
			result *= kMsecPerSecond;
	
			return result;
		}

		/**
		 * MacDateTime::ReadMachineLocation
		 * 
		 * (Classic) Reads the machine location, for determining time zone
		 * and Daylight Saving Time
		 */
		void MacDateTime::ReadMachineLocation(MachineLocation* loc)
		{
			static MachineLocation gStoredLoc;
			static bool gDidReadLocation = false;
			if (!gDidReadLocation) {
				::ReadLocation(&gStoredLoc);
				gDidReadLocation = true;
			}
			*loc = gStoredLoc;
		}

		/**
		 * MacDateTime::LocalTZA
		 * 
		 * Determines the Local Time Zone Adjustment, in seconds from GMT
		 */
		double MacDateTime::LocalTZA(double time)
		{
			CFTimeZoneRef tz = CFTimeZoneCopySystem();
			if (tz) 
			{			
				CFAbsoluteTime at = ECMADateToCFAbsoluteTime(time);
				// pass now as the reference. 
				// This will include any daylight savings offsets in the result.
				CFTimeInterval result = CFTimeZoneGetSecondsFromGMT(tz, at);
						
				CFRelease(tz);	

				result *= kMsecPerSecond;
		
				// Remove the effects of daylight saving time
				return ( result -= MacDateTime::DaylightSavingTA(time) );
			}

			MachineLocation location;
			ReadMachineLocation(&location);

			// Extract the GMT delta
			long gmtDelta = location.u.gmtDelta & 0xFFFFFF;
			if (gmtDelta & 0x800000) {
				gmtDelta |= 0xFF000000;
			}

			// Remove the effects of daylight saving time
			if (location.u.dlsDelta < 0) {
				gmtDelta -= 3600;
			}
 
			return (double)gmtDelta * kMsecPerSecond;
		}

		/**
		 * MacDateTime::DaylightSavingTA
		 *
		 * Determines whether the specified time is affected by 
		 * Daylight Saving Time in the current system time zone
		 */
		double MacDateTime::DaylightSavingTA(double time)
		{
			CFTimeZoneRef tz = CFTimeZoneCopyDefault();
			if (tz) {
				CFAbsoluteTime at = ECMADateToCFAbsoluteTime(time);
				Boolean isDST = CFTimeZoneIsDaylightSavingTime(tz, at);
				CFRelease(tz);	
				return (isDST ? kMsecPerHour : 0);
			}

			time;
	
			// On Macintosh, punt and just report the daylight
			// saving adjustment currently in effect, rather
			// than the daylight saving time adjustment for the
			// exact time passed in.
			MachineLocation location;
			ReadMachineLocation(&location);
			return (location.u.dlsDelta < 0) ? kMsecPerHour : 0;
		}

		/**
		 * MacDateTime::MicrosecondDelta
		 *
		 * Determines the interval between two Macintosh "Microseconds" 64-bit
		 * quantities.
		 */
		void MacDateTime::MicrosecondDelta(register const UnsignedWide *startPtr,
										   register const UnsignedWide *endPtr,
										   register UnsignedWide *resultPtr)
		{
			if (endPtr->lo >= startPtr->lo) {
				resultPtr->hi = endPtr->hi - startPtr->hi;
			} else {
				resultPtr->hi = (endPtr->hi - 1) - startPtr->hi;
			}
			resultPtr->lo = endPtr->lo - startPtr->lo;
		}

		/**
		 * MacDateTime::GetTime
		 *
		 * Returns the ECMAscript time (milliseconds since Jan 1 1970 GMT)
		 */
		double MacDateTime::GetTime()
		{
			CFAbsoluteTime at = CFAbsoluteTimeGetCurrent();
			return CFAbsoluteTimeToECMADate(at);
		}

		/**
		 * MacDateTime::GetMsecCount
		 *
		 * Returns the number of milliseconds since system startup time,
		 * in a 64-bit integer.
		 */
		uint64 MacDateTime::GetMsecCount()
		{
			UnsignedWide microsecs;
			::Microseconds(&microsecs);

			UInt64 usecs;
			UInt64 divisor = 1000;
			memcpy(&usecs, &microsecs, sizeof(usecs));

			return usecs / divisor;
		}
	}
}
	
