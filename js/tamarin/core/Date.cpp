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


#include <stdarg.h>

#include "avmplus.h"

namespace avmplus
{	
#ifdef FIX_RECIPROCAL_BUG
	//
	// Visual C++ 6.0 optimizes division by turning it into multiplication
	// by the reciprocal.  This loses precision and results in inaccuracies
	// in date calculations.  Avoid this "optimization" by putting the
	// numbers in static variables.
	// 
	static double kMsecPerDay       = 86400000;
	static double kMsecPerHour      = 3600000;
	static double kMsecPerSecond    = 1000;
	static double kMsecPerMinute    = 60000;
#else
	//
	// OK, we have a compiler that doesn't want to optimize our divisions.
	//
#define kMsecPerDay       86400000
#define kMsecPerHour      3600000
#define kMsecPerSecond    1000
#define kMsecPerMinute    60000
#endif

	//
	// We don't ever divide by these, so they should be fine.
	//
#define kSecondsPerMinute 60
#define kMinutesPerHour   60
#define kHoursPerDay      24
#define kMsecPerSecondInt 1000


	static double Day(double t)
	{
		return MathUtils::floor(t / (double)kMsecPerDay);
	}

	static double DayFromYear(double year)
	{
		return (365 * (year - 1970) +
				MathUtils::floor((year - 1969) / 4) -
				MathUtils::floor((year - 1901)/100) +
				MathUtils::floor((year - 1601) / 400));
	}

	static inline double TimeFromYear(int year)
	{
		return (double)kMsecPerDay * DayFromYear(year);
	}

	static int DaysInYear(int year)
	{
		if (year % 4) {
			return 365;
		}
		if (year % 100) {
			return 366;
		}
		if (year % 400) {
			return 365;
		}
		return 366;
	}

	static inline double TimeWithinDay(double t)
	{
		double result = MathUtils::mod(t, kMsecPerDay);
		if (result < 0)
            result += kMsecPerDay;
		return result;
	}

	// NOTE: this is used by the Player core code. Changing this will change legacy behavior.
	int YearFromTime(double t)
	{
		double day = Day(t);
		int lo, hi;
		lo = (int) MathUtils::floor((t < 0) ? (day / 365) : (day / 366)) + 1970;
		hi = (int) MathUtils::ceil((t < 0) ? (day / 366) : (day / 365)) + 1970;
		while (lo < hi) {

			// 13may04 grandma :
			// This was pivot = (lo + hi) / 2, but bug 89715 inadvertantly calls this with
			// t = -6.5438017398347670e+019, which produces lo = -2075023950 and hi = -2069354479,
			// and (lo + hi) overflows. The below expression won't overflow
			//int pivot = (lo / 2) + (hi / 2) + (lo & hi & 1);

			// 8/17/04 edsmith: 
			// the above expression does overflow, with other large numbers.
			// this one below uses double math to avoid overflow.
			int pivot = (int) ((((double)lo) + ((double)hi)) / 2);

			double pivotTime = TimeFromYear(pivot);
			if (pivotTime <= t) {
				if (TimeFromYear(pivot + 1) > t) { // R41
					return pivot;
				} else {
					lo = pivot + 1;
				}
			} else if (pivotTime > t) {
				hi = pivot - 1;
			}
		}
		return lo;
	}

	static inline bool IsLeapYear(int year)
	{
		return DaysInYear(year) == 366;
	}

	static inline bool TimeInLeapYear(double t)
	{
		return IsLeapYear(YearFromTime(t));
	}

	static inline int DayWithinYear(double t)
	{
		return (int) (Day(t) - DayFromYear((int) YearFromTime(t)));
	}

	static const uint16 kMonthOffset[2][13] = {
		//    Jan Feb Mar Apr May  Jun  Jul  Aug  Sep  Oct  Nov  Dec  Total
		{ 0,  31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
		{ 0,  31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
	};

	static int MonthFromTime(double t)
	{
		int day = DayWithinYear(t);
		int leap = (int) TimeInLeapYear(t);
		int i;

		for (i=0; i<11; i++) {
			if (day < kMonthOffset[leap][i+1]) {
				break;
			}
		}
		return i;
	}

	static int DateFromTime(double t)
	{
		int month = MonthFromTime(t);
		return DayWithinYear(t) - kMonthOffset[(int) TimeInLeapYear(t)][month] + 1;
	}

	//mds: gcc has problems when this is inlined.
	int WeekDay(double t)
	{
		int result = (int) MathUtils::mod(Day(t) + 4,  7);
		if (result < 0) {
			result = 7 + result;
		}
		return result;
	}

	/* This needs to be publicly available, for Mac edge code. (srj) */
	double UTC(double t)
	{
		return (t - OSDep::localTZA(t) - OSDep::daylightSavingTA(t - OSDep::localTZA(t)));
	}

	static double LocalTime(double t)
	{
		return (t + OSDep::localTZA(t) + OSDep::daylightSavingTA(t));
	}

	// this needs to be publicly available, for data paraser
	double GetTimezoneOffset(double t)
	{
		return (t - LocalTime(t)) / kMsecPerMinute;
	}

	static int HourFromTime(double t)
	{
		int result = (int) MathUtils::mod(MathUtils::floor((t + 0.5) / kMsecPerHour), kHoursPerDay);
		if (result < 0) {
			result += kHoursPerDay;
		}
		return result;
	}

	static double DayFromMonth(double year, double month)
	{
		int iMonth = (int) MathUtils::floor(month);
		if (iMonth < 0 || iMonth >= 12) {
			return MathUtils::nan();
		}
		return DayFromYear((int)year) + kMonthOffset[(int)IsLeapYear((int)year)][iMonth];
	}

	static int MinFromTime(double time)
	{
		int result = (int) MathUtils::mod(MathUtils::floor(time / kMsecPerMinute), kMinutesPerHour);
		if (result < 0) {
			result += kMinutesPerHour;
		}
		return result;
	}

	static int SecFromTime(double time)
	{
		int result = (int) MathUtils::mod(MathUtils::floor(time / kMsecPerSecond), kSecondsPerMinute);
		if (result < 0) {
			result += kSecondsPerMinute;
		}
		return result;
	}

	static int MsecFromTime(double time)
	{
		int result = (int) MathUtils::mod(time, kMsecPerSecond);
		if (result < 0) {
			result += kMsecPerSecondInt;
		}
		return result;
	}

	double MakeDate(double day, double time)
	{
		// if any value is not finite, return NaN
		if (MathUtils::isInfinite(day) || MathUtils::isInfinite(time) ||
			day != day || time != time)
			return MathUtils::nan();

		day = MathUtils::toInt(day);
		time = MathUtils::toInt(time);
		
		return day * kMsecPerDay + time;
	}

	double MakeTime(double hour, double min, double sec, double ms)
	{
		// if any value is not finite, return NaN
		if (MathUtils::isInfinite(hour) || MathUtils::isInfinite(min) || MathUtils::isInfinite(sec) || MathUtils::isInfinite(ms) ||
			hour != hour || min != min || sec != sec || ms != ms)
			return MathUtils::nan();

		hour = MathUtils::toInt(hour);
		min  = MathUtils::toInt(min);
		sec  = MathUtils::toInt(sec);
		ms   = MathUtils::toInt(ms);
		
		return hour * (double)kMsecPerHour + min * (double)kMsecPerMinute + sec * (double)kMsecPerSecond + ms;
	}

	double MakeDay(double year, double month, double date)
	{
		// if any value is not finite, return NaN
		if (MathUtils::isInfinite(year) || MathUtils::isInfinite(month) || MathUtils::isInfinite(date) ||
			year != year || month != month || date != date)
			return MathUtils::nan();

		year  = MathUtils::toInt(year);
		month = MathUtils::toInt(month);
		date  = MathUtils::toInt(date);

		year += MathUtils::floor(month / 12);
		month = MathUtils::mod(month, 12);
		if (month < 0) {
			month += 12;
		}
		return DayFromMonth(year, month) + (date - 1);
	}

	Date::Date()
	{
		m_time = OSDep::getDate();
	}
		
	Date::Date(double year,
			   double month,
			   double date,
			   double hours,
			   double min,
			   double sec,
			   double msec,
			   bool utcFlag)
	{
		if (year < 100) {
			year += 1900;
		}
		m_time = MakeDate(MakeDay(year, month, date),
						  MakeTime(hours, min, sec, msec));
		if (!utcFlag) {
			m_time = UTC(m_time);
		}
	}

	void Date::format(wchar *buffer,
					  const char *format,
					  ...) const
	{
		va_list ap;

		va_start(ap, format);
		while (*format) {
			if (*format == '%') {
				switch (*++format) {
				case 's':
					{
						char *str = va_arg(ap, char *);
						while (*str) {
							*buffer++ = *str++;
						}
					}
					break;
				case '2':
					{
						int value = va_arg(ap, int);
						*buffer++ = (value/10) + '0';
						*buffer++ = (value%10) + '0';
					}
					break;
				case '3':
					{
						char *str = va_arg(ap, char *);
						*buffer++ = *str++;
						*buffer++ = *str++;
						*buffer++ = *str++;
					}
					break;
				case 'c':
					{
						// gcc complains if you put va_arg(ap, char)
						char value = va_arg(ap, int);
						*buffer++ = value;
					}
					break;
				case 'd':
					{
						int value = va_arg(ap, int);
						wchar intbuf[256];
						int len;
						MathUtils::convertIntegerToString(value, intbuf, len);
						wchar *intptr = intbuf;
						while (*intptr) {
							*buffer++ = *intptr++;
						}
					}
					break;
				}
			} else {
				*buffer++ = *format;
			}
			format++;
		}
		*buffer = 0;
		va_end(ap);
	}
					  
	bool Date::toString(wchar *buffer,
						int formatIndex, int &len) const
	{
		// todo we could try to do a much better job on
		// localized date stuff
		
		static const char kMonths[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
		static const char kDaysOfWeek[] = "SunMonTueWedThuFriSat";

		if (MathUtils::isNaN(m_time)) {
			UnicodeUtils::Utf8ToUtf16( (const uint8 *)"Invalid Date", 
													 12,
													 buffer, 
													 12 );
			buffer[len=12] = 0;
			return true;
		}
		
		double time = m_time;

		if (formatIndex != kToUTCString) {
			time = LocalTime(m_time);
		}

		int year = YearFromTime(time);
		int month = MonthFromTime(time);
		int day = WeekDay(time);
		if (month < 0 || month >= 12 || day < 0 || day >= 7) {
			return false;
		}
		
		int delta = (int) ((time - m_time) / kMsecPerMinute);
		char signChar = '+';
		if (delta < 0) {
			delta = -delta;
			signChar = '-';
		}
		int deltaH = (delta / 60);
		int deltaM = (delta % 60);

		int date = (int)DateFromTime(time);

		int hour24 = (int)HourFromTime(time);
		int hour12 = hour24 % 12;
		if (hour12 == 0) {
			hour12 = 12;
		}
		char ampm = (hour24 >= 12) ? 'P' : 'A';

		int min = (int)MinFromTime(time);
		int seconds = (int)SecFromTime(time);

		const char *dayOfWeekStr = kDaysOfWeek + day * 3;
		const char *monthStr = kMonths + month * 3;
		
		switch (formatIndex) {
			/* CN:  ecma3 leaves the string format implementation dependant, as long
			/   as it contains all the info below.  As a result, IE and Mozilla
			/   had different formats for date.  In 2002, Mozilla changed their date
			/   format to make it easier to write code which parses dates in string format 
			/   regardless of the implementation which produced it.
			/   http://bugzilla.mozilla.org/show_bug.cgi?id=118266 
			/   We are not required by the standard to follow suit, but the one of the goals
			/   of compliance is easy porting of code / techniques from ecmascript.  This change
			/   should be done in a AS2/AS3 conditional manner, however:

			// CN: 1/8/05 well, maybe this does break some existing user code (like the ATS).  Since
			//  we aren't required to match Spidermonkey by the ES3 spec, lets decide to break
			//  existing ECMAscript code over existing Actionscript code.

			//AS2.0 format:

			*/
		case kToString:
			format(buffer,
				   "%3 %3 %d %2:%2:%2 GMT%c%2%2 %d",
				   dayOfWeekStr,
				   monthStr,
				   date,
				   hour24,
				   min,
				   seconds,
				   signChar,
				   deltaH,
				   deltaM,
				   year);
			break;
		case kToLocaleString:
			format(buffer,
				   "%3 %3 %d %d %2:%2:%2 %cM",
				   dayOfWeekStr,
				   monthStr,
				   date,
				   year,
				   hour12,
				   min,
				   seconds,
				   ampm);
			break;			
		case kToUTCString:
			format(buffer,
				   "%3 %3 %d %2:%2:%2 %d UTC",
				   dayOfWeekStr,
				   monthStr,
				   date,
				   hour24,
				   min,
				   seconds,
				   year);
			break;
				
		/*  This would be the SpiderMonkey / IE format (well, logically at least in that
		/   toString() == toDateString() + toTimeString().  See long comment above...
			
		case kToString:
			format(buffer,
				   "%3 %3 %d %d %2:%2:%2 GMT%c%2%2",
				   dayOfWeekStr,
				   monthStr,
				   date,
				   year,
				   hour24,
				   min,
				   seconds,
				   signChar,
				   deltaH,
				   deltaM);
			break;
		case kToLocaleString:
			format(buffer,
				   "%3 %3 %d %d %2:%2:%2 %cM",
				   dayOfWeekStr,
				   monthStr,
				   date,
				   year,
				   hour12,
				   min,
				   seconds,
				   ampm);
			break;			
		case kToUTCString:
			format(buffer,
				   "%3 %3 %d %d %2:%2:%2 UTC",
				   dayOfWeekStr,
				   monthStr,
				   date,
				   year,
				   hour24,
				   min,
				   seconds);
			break;
		*/

		case kToDateString:
		case kToLocaleDateString:
			format(buffer,
				   "%3 %3 %d %d",
				   dayOfWeekStr,
				   monthStr,
				   (int)DateFromTime(time),
				   (int)YearFromTime(time));
			break;
		case kToTimeString:
			format(buffer,
				   "%2:%2:%2 GMT%c%2%2",
				   hour24,
				   min,
				   seconds,
				   signChar,
				   deltaH,
				   deltaM);
			break;			
		case kToLocaleTimeString:
			format(buffer,
				   "%2:%2:%2 %cM",
				   hour12,
				   min,
				   seconds,
				   ampm);
			break;
		default:
			return false;
		}
		len = String::Length(buffer);
		return true;
	}

	double Date::getProperty(int index)
	{
		double t = m_time;

		// Short-circuit and return NaN if the date
		// is NaN
		if (MathUtils::isNaN(t)) {
			return MathUtils::nan();
		}
		
		switch (index) {
		case kUTCFullYear:
			return YearFromTime(t);
		case kUTCMonth:
			return MonthFromTime(t);
		case kUTCDate:
			return DateFromTime(t);
		case kUTCDay:
			return WeekDay(t);
		case kUTCHours:
			return HourFromTime(t);
		case kUTCMinutes:
			return MinFromTime(t);
		case kUTCSeconds:
			return SecFromTime(t);
		case kUTCMilliseconds:
			return MsecFromTime(t);
		case kFullYear:
			return YearFromTime(LocalTime(t));
		case kMonth:
			return MonthFromTime(LocalTime(t));
		case kDate:
			return DateFromTime(LocalTime(t));
		case kDay:
			return WeekDay(LocalTime(t));
		case kHours:
			return HourFromTime(LocalTime(t));
		case kMinutes:
			return MinFromTime(LocalTime(t));
		case kSeconds:
			return SecFromTime(LocalTime(t));
		case kMilliseconds:
			return MsecFromTime(LocalTime(t));
		case kTimezoneOffset:
			return (t - LocalTime(t)) / kMsecPerMinute;
		case kTime:
			return t;
		}

		AvmAssert(false);
		return 0;
	}

	void Date::setTime(double value)
	{
		m_time = TimeClip(value);
	}

	// To not change a particular value, pass NaN.
	void Date::setTime(double hours,
					   double min,
					   double sec,
					   double msec,
					   bool utcFlag)
	{
		double t = utcFlag ? m_time : LocalTime(m_time);

		if (MathUtils::isNaN(hours)) {
			hours = HourFromTime(t);
		}
		if (MathUtils::isNaN(min)) {
			min = MinFromTime(t);
		}
		if (MathUtils::isNaN(sec)) {
			sec = SecFromTime(t);
		}
		if (MathUtils::isNaN(msec)) {
			msec = MsecFromTime(t);
		}
		t = MakeDate(Day(t),
					 MakeTime(hours,
							  min,
							  sec,
							  msec));

		m_time = TimeClip(utcFlag ? t : UTC(t));
	}

	void Date::setDate(double year,
					   double month,
					   double date,
					   bool utcFlag)
	{
		double t = utcFlag ? m_time : LocalTime(m_time);

		// date may already be NaN.  It stays as NaN unless we are setting the year
		if (MathUtils::isNaN(m_time))
		{
			if (MathUtils::isNaN(year))
				return;
			else 
				t = 0; // treat time as zero.
		}

		if (MathUtils::isNaN(year)) {
			year = YearFromTime(t);
		}
		if (MathUtils::isNaN(month)) {
			month = MonthFromTime(t);
		}
		if (MathUtils::isNaN(date)) {
			date = DateFromTime(t);
		}

		// cn 2/14/06  Not sure whent this was added, but its not ECMAScript compatible.
		//   Yes, we will unexpectedly roll over into the next month and that's what
		//   the spec says to do.  Disabling this "correction"
		/*
		// If we are setting the month on the 31st to a month that has 30 days,
		// this will have the unexpected effect of rolling the date over into
		// the next month.  Correct for that here.
		int iMonth = (int)month;
		int iDate  = (int)date;
		int leap   = IsLeapYear((int)year);
		if (iMonth >= 0 && iMonth <= 11 && iDate >= 1 && iDate <= 31 ) {
			if (iDate >= DaysInMonth(leap, iMonth)) {
				iDate = DaysInMonth(leap, iMonth);
				date = (double)iDate;
			}
		}
		*/
		
		t = MakeDate(MakeDay(year, month, date), TimeWithinDay(t));
		m_time = TimeClip(utcFlag ? t : UTC(t));
	}
}

