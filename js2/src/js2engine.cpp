/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the JavaScript 2 Prototype.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.   Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the
* terms of the GNU Public License (the "GPL"), in which case the
* provisions of the GPL are applicable instead of those above.
* If you wish to allow use of your version of this file only
* under the terms of the GPL and not to allow others to use your
* version of this file under the NPL, indicate your decision by
* deleting the provisions above and replace them with the notice
* and other provisions required by the GPL.  If you do not delete
* the provisions above, a recipient may use your version of this
* file under either the NPL or the GPL.
*/


/* JS2 Engine - */

#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif

#include <algorithm>
#include <assert.h>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "numerics.h"

#include <map>
#include <algorithm>

#include "reader.h"
#include "parser.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

namespace JavaScript {
namespace MetaData {

js2val JS2Engine::interpret(JS2Metadata *metadata, Phase execPhase, BytecodeContainer *targetbCon)
{
    bCon = targetbCon;
    pc = bCon->getCodeStart();
    meta = metadata;
    phase = execPhase;
    return interpreterLoop();
}

js2val JS2Engine::interpreterLoop()
{
    js2val retval = JS2VAL_VOID;
    while (true) {
        JS2Op op = (JS2Op)*pc++;
        switch (op) {
#include "js2op_arithmetic.cpp"
#include "js2op_invocation.cpp"
#include "js2op_access.cpp"
#include "js2op_literal.cpp"
        }
    }
    return retval;
}

// return a pointer to an 8 byte chunk in the gc heap
void *JS2Engine::gc_alloc_8()
{
    return STD::malloc(8);
}

// See if the double value is in the hash table, return it's pointer if so
// If not, fill the table or return a un-hashed pointer
float64 *JS2Engine::newDoubleValue(float64 x)
{
    union {
        float64 x;
        uint8 a[8];
    } u;
    if (x != x)
        return nanValue;

    u.x = x;
    uint8 hash = u.a[0] ^ u.a[1] ^ u.a[2] ^ u.a[3] ^ u.a[4] ^ u.a[5] ^  u.a[6] ^ u.a[7];
    if (float64Table[hash]) {
        if (*float64Table[hash] == x)
            return float64Table[hash];
        else {
            float64 *p = (float64 *)gc_alloc_8();
            *p = x;
            return p;
        }
    }
    else {
        float64 *p = (float64 *)gc_alloc_8();
        float64Table[hash] = p;
        *p = x;
        return p;
    }


}

// if the argument can be stored as an integer value, do so
// otherwise get a double value
void JS2Engine::pushNumber(float64 x)
{
    uint32 i;
    if (JSDOUBLE_IS_INT(x, i) && INT_FITS_IN_JS2VAL(i))
        push(INT_TO_JS2VAL(i));
    else
        push(DOUBLE_TO_JS2VAL(newDoubleValue(x)));
}

// Convert an integer to a string
String *numberToString(int32 i)
{
    char buf[dtosStandardBufferSize];
    const char *chrp = doubleToStr(buf, dtosStandardBufferSize, i, dtosStandard, 0);
    return new JavaScript::String(widenCString(chrp));
}

// Convert a double to a string
String *numberToString(float64 *number)
{
    char buf[dtosStandardBufferSize];
    const char *chrp = doubleToStr(buf, dtosStandardBufferSize, *number, dtosStandard, 0);
    return new JavaScript::String(widenCString(chrp));
}

// x is not a String
String *JS2Engine::convertValueToString(js2val x)
{
    if (JS2VAL_IS_UNDEFINED(x))
        return &undefined_StringAtom;
    if (JS2VAL_IS_NULL(x))
        return &null_StringAtom;
    if (JS2VAL_IS_BOOLEAN(x))
        return (JS2VAL_TO_BOOLEAN(x)) ? &true_StringAtom : &false_StringAtom;
    if (JS2VAL_IS_INT(x))
        return numberToString(JS2VAL_TO_INT(x));
    if (JS2VAL_IS_DOUBLE(x))
        return numberToString(JS2VAL_TO_DOUBLE(x));
    return toString(toPrimitive(x));
}

// x is not a primitive (it is an object and not null)
js2val JS2Engine::convertValueToPrimitive(js2val x)
{
    // return [[DefaultValue]] --> get property 'toString' and invoke it, 
    // if not available or result is not primitive then try property 'valueOf'
    // if that's not available or returns a non primitive, throw a TypeError

    return STRING_TO_JS2VAL(&object_StringAtom);
    
    ASSERT(false);
    return JS2VAL_VOID;
}

// x is not a number
float64 JS2Engine::convertValueToDouble(js2val x)
{
    if (JS2VAL_IS_UNDEFINED(x))
        return nan;
    if (JS2VAL_IS_NULL(x))
        return 0;
    if (JS2VAL_IS_BOOLEAN(x))
        return (JS2VAL_TO_BOOLEAN(x)) ? 1.0 : 0.0;
    if (JS2VAL_IS_STRING(x)) {
        String *str = JS2VAL_TO_STRING(x);
        char16 *numEnd;
        return stringToDouble(str->data(), str->data() + str->length(), numEnd);
    }
    return toNumber(toPrimitive(x));

}

#define INIT_STRINGATOM(n) n##_StringAtom(world.identifiers[#n])

JS2Engine::JS2Engine(World &world)
            : world(world),
              INIT_STRINGATOM(true),
              INIT_STRINGATOM(false),
              INIT_STRINGATOM(null),
              INIT_STRINGATOM(undefined),
              INIT_STRINGATOM(public),
              INIT_STRINGATOM(object)
{
    nanValue = (float64 *)gc_alloc_8();
    *nanValue = nan;

    for (int i = 0; i < 256; i++)
        float64Table[i] = NULL;

    sp = execStack = new js2val[MAX_EXEC_STACK];

}

int JS2Engine::getStackEffect(JS2Op op)
{
    switch (op) {
    case eReturn:
    case ePlus:
        return -1;

    case eString:
    case eTrue:
    case eFalse:
    case eNumber:
        return 1;

    case eLexicalRead:
        return 0;       // consumes a multiname, pushes the value
    case eLexicalWrite:
        return -2;      // consumes a multiname and the value

    case eReturnVoid:
        return 0;

    case eMultiname:
    case eQMultiname:
        return 1;       // push the multiname object


    default:
        ASSERT(false);
    }
    return 0;
}

}
}