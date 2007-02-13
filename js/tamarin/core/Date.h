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

#ifndef __avmplus_Date__
#define __avmplus_Date__

namespace avmplus
{
	int YearFromTime(double t);

	/**
	 * Date is used to provide the underpinnings for the Date class.
	 * It is a layer over OS-specific date/time functionality.
	 */
	class Date
	{
	public:
		enum {
			kUTCFullYear,
			kUTCMonth,
			kUTCDate,
			kUTCDay,
			kUTCHours,
			kUTCMinutes,
			kUTCSeconds,
			kUTCMilliseconds,
			kFullYear,
			kMonth,
			kDate,
			kDay,
			kHours,
			kMinutes,
			kSeconds,
			kMilliseconds,
			kTimezoneOffset,
			kTime
		};

		enum {
			kToString,
			kToDateString,
			kToTimeString,
			kToLocaleString,
			kToLocaleDateString,
			kToLocaleTimeString,
			kToUTCString
		};

#define kHalfTimeDomain   8.64e15

		static inline double TimeClip(double t)
		{
			if (MathUtils::isInfinite(t) || MathUtils::isNaN(t) || ((t < 0 ? -t : t) > kHalfTimeDomain)) {
				return MathUtils::nan();
			}
			return MathUtils::toInt(t) + (+0.);
		}

			
		Date();
		Date(const Date& toCopy) {
			m_time = toCopy.m_time;
		}
		Date& operator= (const Date& toCopy) {
			m_time = toCopy.m_time;
			return *this;
		}
		Date(double time) { m_time = TimeClip(time); }
		Date(double year,
			 double month,
			 double date,
			 double hours,
			 double min,
			 double sec,
			 double msec,
			 bool utcFlag);
		~Date() { m_time = 0; }
		double getDateProperty(int index);
		double getTime() const { return m_time; }
		void setDate(double year,
					 double month,
					 double date,
					 bool utcFlag);
		void setTime(double hours,
					 double min,
					 double sec,
					 double msec,
					 bool utcFlag);
		void setTime(double value);
		bool toString(wchar *buffer, int formatIndex, int &len) const;
		
	private:
		double m_time;
		void format(wchar *buffer,
					const char *format, ...) const;
	};
	double GetTimezoneOffset(double t);
}

#endif /* __avmplus_Date__ */
