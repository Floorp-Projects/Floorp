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
    #include "js2op_flowcontrol.cpp"
            }
            JS2Object::gc(meta);
        }
        return retval;
    }

    // return a pointer to an 8 byte chunk in the gc heap
    void *JS2Engine::gc_alloc_8()
    {
        return JS2Object::alloc(8);
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
    js2val JS2Engine::allocNumber(float64 x)
    {
        uint32 i;
        js2val retval;
        if (JSDOUBLE_IS_INT(x, i) && INT_FITS_IN_JS2VAL(i))
            retval = INT_TO_JS2VAL(i);
        else
            retval = DOUBLE_TO_JS2VAL(newDoubleValue(x));
        return retval;
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

    // x is not a bool
    bool JS2Engine::convertValueToBoolean(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x))
            return false;
        if (JS2VAL_IS_NULL(x))
            return false;
        if (JS2VAL_IS_INT(x))
            return (JS2VAL_TO_INT(x) != 0);
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 *xd = JS2VAL_TO_DOUBLE(x);
            return ! (JSDOUBLE_IS_POSZERO(*xd) || JSDOUBLE_IS_NEGZERO(*xd) || JSDOUBLE_IS_NaN(*xd));
        }
        if (JS2VAL_IS_STRING(x)) {
            String *str = JS2VAL_TO_STRING(x);
            return (str->length() != 0);
        }
        return true;
    }


    #define INIT_STRINGATOM(n) n##_StringAtom(world.identifiers[#n])

    JS2Engine::JS2Engine(World &world)
                : world(world),
                  pc(NULL),
                  bCon(NULL),
                  meta(NULL),
                  INIT_STRINGATOM(true),
                  INIT_STRINGATOM(false),
                  INIT_STRINGATOM(null),
                  INIT_STRINGATOM(undefined),
                  INIT_STRINGATOM(public),
                  INIT_STRINGATOM(private),
                  INIT_STRINGATOM(function),
                  INIT_STRINGATOM(object)
    {
        nanValue = (float64 *)gc_alloc_8();
        *nanValue = nan;

        for (int i = 0; i < 256; i++)
            float64Table[i] = NULL;

        sp = execStack = new js2val[MAX_EXEC_STACK];
        activationStackTop = activationStack = new ActivationFrame[MAX_ACTIVATION_STACK];
    }

    int getStackEffect(JS2Op op)
    {
        switch (op) {
        case eReturn:
            return -1;  

        case eAdd:          // pop two, push one
        case eSubtract:
        case eEqual:
        case eNotEqual:
        case eLess:
        case eGreater:
        case eLessEqual:
        case eGreaterEqual:
            return -1;  

        case eString:
        case eTrue:
        case eFalse:
        case eNumber:
        case eNull:
            return 1;       // push literal value

        case eLexicalRead:
            return 1;       // push the value
        case eLexicalWrite:
            return 0;       // leave the value
        case eLexicalRef:
            return 2;       // push base & value

        case eDotRead:
            return 0;       // pop a base, push the value
        case eDotWrite:
            return -1;      // pop a base, leave the value
        case eDotRef:
            return 1;       // leave the base, push the value

        case eBracketRead:
            return -1;      // pop a base and an index, push the value
        case eBracketWrite:
            return -2;      // pop a base and an index, leave the value
        case eBracketRef:
            return 1;       // leave the base, pop the index, push the value

        case eReturnVoid:
        case eBranch:
            return 0;

        case ePopv:         // pop a statement result value
            return -1;      

    //    case eToBoolean:    // pop object, push boolean
    //        return 0;

        case ePushFrame:    // affect the frame stack...
        case ePopFrame:     // ...not the exec stack
            return 0;

        case eBranchFalse:
        case eBranchTrue:
            return -1;      // pop the boolean condition

        case eNew:          // pop the class or function, push the new instance
            return 0;

        case eLexicalPostInc:
        case eLexicalPostDec:
        case eLexicalPreInc:
        case eLexicalPreDec:
            return 1;       // push the new/old value

        case eDotPostInc:
        case eDotPostDec:
        case eDotPreInc:
        case eDotPreDec:
            return 0;       // pop the base, push the new/old value

        case eBracketPostInc:
        case eBracketPostDec:
        case eBracketPreInc:
        case eBracketPreDec:
            return -1;       // pop the base, pop the index, push the new/old value

        default:
            ASSERT(false);
        }
        return 0;
    }

    size_t JS2Engine::errorPos()
    {
        return bCon->getPosition(pc - bCon->getCodeStart()); 
    }

    JS2Object *JS2Engine::defaultConstructor(JS2Engine *engine)
    {
        js2val v = engine->pop();
        ASSERT(JS2VAL_IS_OBJECT(v) && !JS2VAL_IS_NULL(v));
        JS2Object *obj = JS2VAL_TO_OBJECT(v);
        ASSERT(obj->kind == ClassKind);
        JS2Class *c = checked_cast<JS2Class *>(obj);
        if (c->dynamic)
            return new DynamicInstance(c);
        else
            if (c->prototype)
                return new PrototypeInstance(NULL);
            else
                return new FixedInstance(c);
    }

    void JS2Engine::jsr(BytecodeContainer *new_bCon)
    {
        ASSERT(activationStackTop < (activationStack + MAX_ACTIVATION_STACK));
        activationStackTop->bCon = bCon;
        activationStackTop->pc = pc;
        activationStackTop++;

        bCon = new_bCon;
        pc = new_bCon->getCodeStart();

    }

    void JS2Engine::rts()
    {
        ASSERT(activationStackTop > activationStack);
        activationStackTop--;

        bCon = activationStackTop->bCon;
        pc = activationStackTop->pc;
    }

    void JS2Engine::mark()
    {
        if (bCon)
            bCon->mark();
        for (ActivationFrame *a = activationStack; (a < activationStackTop); a++) {
            a->bCon->mark();
        }
        for (js2val *e = execStack; (e < sp); e++) {
            if (JS2VAL_IS_OBJECT(*e)) {
                JS2Object *obj = JS2VAL_TO_OBJECT(*e);
                GCMARKOBJECT(obj)
            }
        }
        JS2Object::mark(nanValue);
        for (uint32 i = 0; i < 256; i++) {
            if (float64Table[i])
                JS2Object::mark(float64Table[i]);
        }
    }


}
}