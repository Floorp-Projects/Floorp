/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifdef _WIN32
#include "msvc_pragma.h"
#endif

#include <algorithm>
#include <list>
#include <map>
#include <stack>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "numerics.h"
#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

namespace JavaScript {    
namespace MetaData {


    js2val Number_Constructor(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
    {   
        js2val thatValue = OBJECT_TO_JS2VAL(new (meta) NumberInstance(meta, meta->numberClass->prototype, meta->numberClass));
        NumberInstance *numInst = checked_cast<NumberInstance *>(JS2VAL_TO_OBJECT(thatValue));

        if (argc > 0)
            numInst->mValue = meta->toFloat64(argv[0]);
        else
            numInst->mValue = 0.0;
        return thatValue;
    }
    
    static js2val Number_Call(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
    {   
        if (argc > 0)
            return meta->engine->allocNumber(meta->toFloat64(argv[0]));
        else
            return JS2VAL_ZERO;
    }

    static js2val Number_toString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->numberClass))
            meta->reportError(Exception::typeError, "Number.toString called on something other than a number thing", meta->engine->errorPos());
        NumberInstance *numInst = checked_cast<NumberInstance *>(JS2VAL_TO_OBJECT(thisValue));
        return STRING_TO_JS2VAL(meta->engine->numberToString(&numInst->mValue));
    }

    static js2val Number_valueOf(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->numberClass))
            meta->reportError(Exception::typeError, "Number.valueOf called on something other than a number thing", meta->engine->errorPos());
        NumberInstance *numInst = checked_cast<NumberInstance *>(JS2VAL_TO_OBJECT(thisValue));
        return meta->engine->allocNumber(numInst->mValue);
    }

#define MAX_PRECISION 100

	static js2val
	num_to(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc, DToStrMode zeroArgMode,
		   DToStrMode oneArgMode, int32 precisionMin, int32 precisionMax, uint32 precisionOffset)
	{
		float64 d, precision;
		char buf[dtosVariableBufferSize(MAX_PRECISION+1)], *numStr; /* Use MAX_PRECISION+1 because precisionOffset can be 1 */

		if (!JS2VAL_IS_OBJECT(thisValue) 
				|| (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
				|| ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->numberClass))
			meta->reportError(Exception::typeError, "Number.<<conversion>> called on something other than a number thing", meta->engine->errorPos());
		NumberInstance *numInst = checked_cast<NumberInstance *>(JS2VAL_TO_OBJECT(thisValue));
		d = numInst->mValue;
    
		if (JS2VAL_IS_VOID(argv[0])) {
			precision = 0.0;
			oneArgMode = zeroArgMode;
		} else {
			precision = meta->toInteger(argv[0]);
			if (precision < precisionMin || precision > precisionMax) {
				numStr = doubleToStr(buf, sizeof buf, precision, dtosStandard, 0);
				meta->reportError(Exception::typeError, "Precision value out of range {0}", meta->engine->errorPos(), numStr);
			}
		}

		numStr = doubleToStr(buf, sizeof buf, d, oneArgMode, (uint32)precision + precisionOffset);
		return meta->engine->allocString(numStr);
	}

    static js2val Number_toFixed(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
	{
		/* We allow a larger range of precision than ECMA requires; this is permitted by ECMA. */
		return num_to(meta, thisValue, argv, argc, dtosFixed, dtosFixed, -20, MAX_PRECISION, 0);
	}

    static js2val Number_toExponential(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
	{
		/* We allow a larger range of precision than ECMA requires; this is permitted by ECMA. */
		return num_to(meta, thisValue, argv, argc, dtosStandardExponential, dtosExponential, 0, MAX_PRECISION, 1);
	}

    static js2val Number_toPrecision(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
	{
		/* We allow a larger range of precision than ECMA requires; this is permitted by ECMA. */
		return num_to(meta, thisValue, argv, argc, dtosStandard, dtosPrecision, 1, MAX_PRECISION, 0);
	}

    void initNumberObject(JS2Metadata *meta)
    {
        meta->numberClass->construct = Number_Constructor;
        meta->numberClass->call = Number_Call;

#define N_CONSTANTS_COUNT (5)

        struct {
            char *name;
            float64 value;
        } NumberObjectConstants[N_CONSTANTS_COUNT] = {
            { "MAX_VALUE", maxDouble },
            { "MIN_VALUE", minDouble },
            { "NaN", nan },
            { "POSITIVE_INFINITY", positiveInfinity },
            { "NEGATIVE_INFINITY", negativeInfinity },
        };
        uint32 i;
        meta->env->addFrame(meta->numberClass);
        for (i = 0; i < N_CONSTANTS_COUNT; i++)
        {
            Variable *v = new Variable(meta->numberClass, meta->engine->allocNumber(NumberObjectConstants[i].value), true);
            meta->defineLocalMember(meta->env, meta->world.identifiers[NumberObjectConstants[i].name], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
        }
        meta->env->removeTopFrame();


        FunctionData prototypeFunctions[] =
        {
            { "toString",            0, Number_toString },
            { "toLocaleString",      0, Number_toString },	// [sic]
            { "valueOf",             0, Number_valueOf  },
			{ "toFixed",			 1, Number_toFixed  },
			{ "toExponential",   	 1, Number_toExponential  },
			{ "toPrecision",	     1, Number_toPrecision  },

            { NULL }
        };

        meta->initBuiltinClass(meta->numberClass, NULL, Number_Constructor, Number_Call);
        meta->numberClass->prototype = OBJECT_TO_JS2VAL(new (meta) NumberInstance(meta, meta->objectClass->prototype, meta->numberClass));
        meta->initBuiltinClassPrototype(meta->numberClass, &prototypeFunctions[0]);

    }



}
}


