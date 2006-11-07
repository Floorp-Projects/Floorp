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
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
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


#ifndef __avmplus_NativeObjectHelpers__
#define __avmplus_NativeObjectHelpers__

namespace avmplus
{
	namespace NativeObjectHelpers
	{		
		// CN:  ES3 compliance requires uint32 array and string indicies.  The arguments to several
		//  array methods are integral values held in a double (to allow for negative offsets from
		//  the array length).  To deal with this, ClampIndex replaces ClampA
		// i.e. instead of doing this:

		//  int start = core->integer(argv[0]);
		//  int end   = core->integer(argv[1]]);
		//  ClampA(start,end,a->getLength());
		//
		//  do this:
		//  uint32 start = ClampArrayIndex( core->toInteger(argv[0]), a->getLength() );
		//  uint32 end   = ClampArrayIndex( core->toInteger(argv[1]), a->getLength() );
		//  if (end < start)
		//    end == start;
		inline uint32 ClampIndex(double intValue, uint32 length)
		{
			uint32 result;
			if (intValue < 0.0) 
			{
				if (intValue + length < 0.0) 
					result = 0;
				else 
					result = (uint32)(intValue + length);
			} 
			else if (intValue > length) 
				result = length;
			else if (intValue != intValue) // lookout for NaN.  It converts to a 0 int value on Win, but as 0xffffffff on Mac
				result = 0;
			else
				result = (uint32)intValue;

			return result;
		}

		inline uint32 ClampIndexInt(int intValue, uint32 length)
		{
			uint32 result;
			if (intValue < 0) 
			{
				if (intValue + length < 0) 
					result = 0;
				else 
					result = (uint32)(intValue + length);
			} 
			else if (intValue > (int)length) 
				result = length;
			else
				result = (uint32)intValue;

			return result;
		}


		inline void ClampB(double& start, double& end, int32 length)
		{
			AvmAssert(length >= 0);

			if (end < 0)
				end = 0;

			if (end >= length)
				end = length;

			if (start < 0)
				start = 0;

			if (start >= length)
				start = length;

			if (start > end)
			{
				double swap = start;
				start = end;
				end = swap;
			}

			// Have the indexes been successfully normalized?
			// Postconditions:
			AvmAssert(start >= 0 && start <= length);
			AvmAssert(end >= 0 && end <= length);
			AvmAssert(start <= end);
		}

		inline void ClampBInt(int& start, int& end, int32 length)
		{
			AvmAssert(length >= 0);

			if (end < 0)
				end = 0;

			if (end >= length)
				end = length;

			if (start < 0)
				start = 0;

			if (start >= length)
				start = length;

			if (start > end)
			{
				int swap = start;
				start = end;
				end = swap;
			}

			// Have the indexes been successfully normalized?
			// Postconditions:
			AvmAssert(start >= 0 && start <= length);
			AvmAssert(end >= 0 && end <= length);
			AvmAssert(start <= end);
		}

		inline void copyNarrowToWide(const char *src, wchar *dst)
		{
			while (*src) {
				*dst++ = *src++;
			}
			*dst = 0;
		}
	}
}

#endif /* __avmplus_NativeObjectHelpers__ */
