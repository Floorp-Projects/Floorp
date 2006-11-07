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
	BEGIN_NATIVE_MAP(MathClass)
		NATIVE_METHOD_FLAGS(Math_abs,    MathClass::abs, 0)
		NATIVE_METHOD_FLAGS(Math_acos,   MathClass::acos, 0)
		NATIVE_METHOD_FLAGS(Math_asin,   MathClass::asin, 0)
		NATIVE_METHOD_FLAGS(Math_atan,   MathClass::atan, 0)
		NATIVE_METHOD_FLAGS(Math_atan2,  MathClass::atan2, 0)
		NATIVE_METHOD_FLAGS(Math_ceil,   MathClass::ceil, 0)
		NATIVE_METHOD_FLAGS(Math_cos,    MathClass::cos, 0)
		NATIVE_METHOD_FLAGS(Math_exp,    MathClass::exp, 0)
		NATIVE_METHOD_FLAGS(Math_floor,  MathClass::floor, 0)
		NATIVE_METHOD_FLAGS(Math_log,    MathClass::log, 0)
		NATIVE_METHOD_FLAGS(Math_pow,    MathClass::pow, 0)
		NATIVE_METHOD_FLAGS(Math_round,  MathClass::round, 0)
		NATIVE_METHOD_FLAGS(Math_sin,    MathClass::sin, 0)
		NATIVE_METHOD_FLAGS(Math_sqrt,   MathClass::sqrt, 0)
		NATIVE_METHOD_FLAGS(Math_tan,    MathClass::tan, 0)
		NATIVE_METHOD_FLAGS(Math_private__min,    MathClass::min2, 0)
		NATIVE_METHOD_FLAGS(Math_private__max,    MathClass::max2, 0)
		NATIVE_METHODV(Math_max,     MathClass::max)
		NATIVE_METHODV(Math_min,     MathClass::min)
		NATIVE_METHOD_FLAGS(Math_random,  MathClass::random, 0)
	END_NATIVE_MAP()

	Atom MathClass::construct(int /*argc*/, Atom* /*argv*/)
	{
		// according to ES3 15.8, Math cannot be used as a function or constructor.
		toplevel()->throwTypeError(kMathNotConstructorError);
		return 0;
	}
	
	Atom MathClass::call(int /*argc*/, Atom* /*argv*/)
	{
		// according to ES3 15.8, Math cannot be used as a function or constructor.
		toplevel()->throwTypeError(kMathNotFunctionError);
		return 0;
	}

	MathClass::MathClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		AvmAssert(traits()->sizeofInstance == sizeof(MathClass));
		MathUtils::initRandom(&seed);

        // todo does ES4 Math have a prototype object?
	}

	//
	// ISSUE. ES3 15.8.2 says all these functions call ToNumber on its argument.
	// this is more forgiving than declaring the args to be Number.  We either
	// have to declare these Object and to ToNumber internally, or add some special
	// type support so compiler will have ToNumber.
	//
	// for E3 compatibility all args should be optional=NaN and rest args ignored,
	// except for min, max where rest args are processed.
	//

	double MathClass::abs(double x)
	{
		return MathUtils::abs(x);
	}
	
	double MathClass::acos(double x)
	{
		return MathUtils::acos(x);
	}

	double MathClass::asin(double x)
	{
		return MathUtils::asin(x);
	}

	double MathClass::atan(double x)
	{
		return MathUtils::atan(x);
	}

	double MathClass::atan2(double x, double y)
	{
		return MathUtils::atan2(x, y);
	}

	double MathClass::ceil(double x)
	{
		return MathUtils::ceil(x);
	}
	
	double MathClass::cos(double x)
	{
		return MathUtils::cos(x);
	}
	
	double MathClass::exp(double x)
	{
		return MathUtils::exp(x);
	}

	double MathClass::floor(double x)
	{
		return MathUtils::floor(x);
	}

	double MathClass::log(double x)
	{
		return MathUtils::log(x);
	}
	
	double MathClass::min(Atom* argv, int argc)
	{
		double x = MathUtils::infinity();
		AvmCore* core = this->core();
		for (int i=0; i < argc; i++)
		{
			double y = core->number(argv[i]);
			if (y < x)
				x = y;
			else if (MathUtils::isNaN(y))
				return y;
		}
		return x;
	}

	double MathClass::max(Atom* argv, int argc)
	{
		double x = -MathUtils::infinity();
		AvmCore* core = this->core();
		for (int i=0; i < argc; i++)
		{
			double y = core->number(argv[i]);
			if (y > x)
				x = y;
			else if (MathUtils::isNaN(y))
				return y;
		}
		return x;
	}

	double MathClass::pow(double x, double y)
	{
		return MathUtils::pow(x, y);
	}
	
	double MathClass::random()
	{
		return MathUtils::random(&seed);
	}

	double MathClass::round(double x)
	{
		return MathUtils::round(x);
	}
	
	double MathClass::sin(double x)
	{
		return MathUtils::sin(x);
	}
	
	double MathClass::sqrt(double x)
	{
		return MathUtils::sqrt(x);
	}

	double MathClass::tan(double x)
	{
		return MathUtils::tan(x);
	}

	double MathClass::min2(double x, double y)
	{
		return (x < y || MathUtils::isNaN(x)) ? x : y;
	}

	double MathClass::max2(double x, double y)
	{
		return (x > y || MathUtils::isNaN(x)) ? x : y;
	}
}
