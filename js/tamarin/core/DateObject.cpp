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

namespace avmplus
{

	Stringp DateObject::toString(int index)
	{
		wchar buffer[256];
		int len;
		date.toString(buffer, index, len);
		return new (gc()) String(buffer,len);
	}

	double DateObject::valueOf()
	{
		return date.getTime();
	}

	double DateObject::setTime(double value)
	{
		date.setTime(value);
		return date.getTime();
	}

	double DateObject::get(int index)
	{
		return date.getProperty(index);
	}

	double DateObject::set(int index, Atom *argv, int argc)
	{
		AvmCore* core = this->core();

		double num[7];
		int i;

		for (i=0; i<7; i++) {
			num[i] = MathUtils::nan();
		}
		bool utcFlag = (index < 0); 
		index = (int)MathUtils::abs(index);
		int j = index-1;

		for (i=0; i<argc; i++) {
			if (j >= 7) {
				break;
			}
			num[j++] = core->number(argv[i]);
			if (MathUtils::isNaN(num[j-1])) // actually specifying NaN results in a NaN date. Don't pass Nan, however, because we use 
			{							    //  that value to denote that an optional arg was not supplied.
				date.setTime(MathUtils::nan());
				return date.getTime();
			}
		}

		const int minTimeSetterIndex = 4; // any setNames index >= 4 should call setTime() instead of setDate()
										  //  setFullYear/setUTCFullYear/setMonth/setUTCMonth/setDay/setUTCDay are all in indices < 3
		if (index < minTimeSetterIndex)
		{
			date.setDate(num[0],
							num[1],
							num[2],
							utcFlag);
		}
		else
		{
			date.setTime(num[3],
							num[4],
							num[5],
							num[6],
							utcFlag); 
		}
		return date.getTime();
	}

#ifdef AVMPLUS_VERBOSE
	Stringp DateObject::format(AvmCore* core) const
	{
		wchar buffer[256];
		int len;
		date.toString(buffer, Date::kToString, len);
		Stringp result = core->newString("<");
		result = core->concatStrings(result, new (core->GetGC()) String(buffer,len));
		result = core->concatStrings(result, core->newString(">@"));
		result = core->concatStrings(result, core->formatAtomPtr(atom()));
		return result;
	}
#endif
}
