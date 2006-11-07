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
	BEGIN_NATIVE_MAP(NumberClass)
		NATIVE_METHOD(Number_private__toString, NumberClass::toString)
		NATIVE_METHOD(Number_private__convert, NumberClass::convert)
	END_NATIVE_MAP()

	NumberClass::NumberClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		toplevel()->numberClass = this;

		// prototype objects are always vanilla objects.
		createVanillaPrototype();
	}

	Atom NumberClass::construct(int argc, Atom* argv)
	{
		// Number called as constructor creates new Number instance
		// Note: SpiderMonkey returns 0 for new Number() with no args
		if (argc == 0)
			return 0|kIntegerType;   // yep this is zero atom
		else
			return core()->numberAtom(argv[1]);
		// TODO ArgumentError if argc > 1
	}

	Stringp NumberClass::convert(double n, int precision, int mode)
	{
		AvmCore* core = this->core();

		if (mode == MathUtils::DTOSTR_PRECISION)
		{
			if (precision < 1 || precision > 21) {
				toplevel()->throwRangeError(kInvalidPrecisionError, core->toErrorString(precision), core->toErrorString(1), core->toErrorString(21));
			}
		}
		else
		{
			if (precision < 0 || precision > 20) {
				toplevel()->throwRangeError(kInvalidPrecisionError, core->toErrorString(precision), core->toErrorString(0), core->toErrorString(20));
			}
		}

		wchar buffer[256];
		int len;
		MathUtils::convertDoubleToString(n,
										 buffer,
										 len,
										 mode,
										 precision);

		return new (core->GetGC()) String(buffer,len);
	}
	
	Stringp NumberClass::toString(double dVal, int radix)
	{
		AvmCore* core = this->core();

		if (radix == 10 || MathUtils::isInfinite(dVal) || MathUtils::isNaN(dVal))
			return core->doubleToString(dVal);

		if (radix < 2 || radix > 36)
			toplevel()->throwRangeError(kInvalidRadixError, core->toErrorString(radix));

		// convertDoubleToStringRadix will convert the integer part of dVal
		// to a string in the specified radix, and it will handle large numbers
		// beyond the range of int/uint.  It will not handle the fractional
		// part.  To properly handle that, MathUtils::convertDoubleToString
		// would have to handle any base.  That's a lot of extra code and complexity for
		// something the ES3 spec says is implementation dependent
		// (i.e. we're not required to do it)
		
		return MathUtils::convertDoubleToStringRadix(core, dVal, radix);
	}
}
