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
#include <list>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "jslong.h"
#include "numerics.h"
#include "fdlibm_ns.h"

#include <map>
#include <algorithm>

#include "reader.h"
#include "parser.h"
#include "js2engine.h"
#include "regexp.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

namespace JavaScript {
namespace MetaData {

    // Begin execution of a bytecodeContainer
    js2val JS2Engine::interpret(Phase execPhase, BytecodeContainer *targetbCon)
    {
        jsr(execPhase, targetbCon);
        js2val result = interpreterLoop();
        return result;
    }

    // Execute the opcode sequence at pc.
    js2val JS2Engine::interpreterLoop()
    {
        retval = JS2VAL_VOID;
        baseVal = JS2VAL_VOID;
        indexVal = JS2VAL_VOID;
        while (true) {
            a = JS2VAL_VOID;
            b = JS2VAL_VOID;
            JS2Op op = (JS2Op)*pc++;
            switch (op) {
    #include "js2op_arithmetic.cpp"
    #include "js2op_invocation.cpp"
    #include "js2op_access.cpp"
    #include "js2op_literal.cpp"
    #include "js2op_flowcontrol.cpp"
            default:
                NOT_REACHED("Bad opcode, no biscuit");
            }
            JS2Object::gc(meta);        // XXX temporarily, for testing
        }
        return retval;
    }

    // See if the double value is in the hash table, return it's pointer if so
    // If not, fill the table or return a un-hashed pointer
    float64 *JS2Engine::newDoubleValue(float64 x)
    {
        union {
            float64 x;
            uint8 a[8];
        } u;

        u.x = x;
        uint8 hash = u.a[0] ^ u.a[1] ^ u.a[2] ^ u.a[3] ^ u.a[4] ^ u.a[5] ^  u.a[6] ^ u.a[7];
        if (float64Table[hash]) {
            if (*float64Table[hash] == x)
                return float64Table[hash];
            else {
                float64 *p = (float64 *)JS2Object::alloc(sizeof(float64));
                *p = x;
                return p;
            }
        }
        else {
            float64 *p = (float64 *)JS2Object::alloc(sizeof(float64));
            float64Table[hash] = p;
            *p = x;
            return p;
        }
    }

    String *JS2Engine::allocStringPtr(const String *s)
    {
        String *p = (String *)(JS2Object::alloc(sizeof(String)));
        return new (p) String(*s);
    }

    // if the argument can be stored as an integer value, do so
    // otherwise get a double value
    js2val JS2Engine::allocNumber(float64 x)
    {
        uint32 i;
        js2val retval;
        if (JSDOUBLE_IS_INT(x, i) && INT_FITS_IN_JS2VAL(i))
            retval = INT_TO_JS2VAL(i);
        else {
            if (x != x)
                return nanValue;
            retval = DOUBLE_TO_JS2VAL(newDoubleValue(x));
        }
        return retval;
    }

    // Don't store as an int, even if possible, we need to retain 'longness'
    js2val JS2Engine::allocULong(uint64 x)
    {
        uint64 *p = (uint64 *)(JS2Object::alloc(sizeof(uint64)));
        *p = x;
        return ULONG_TO_JS2VAL(p);
        
    }

    // Don't store as an int, even if possible, we need to retain 'longness'
    js2val JS2Engine::allocLong(int64 x)
    {
        int64 *p = (int64 *)(JS2Object::alloc(sizeof(int64)));
        *p = x;
        return LONG_TO_JS2VAL(p);
    }

    // Don't store as an int, even if possible, we need to retain 'floatness'
    js2val JS2Engine::allocFloat(float32 x)
    {
        float32 *p = (float32 *)(JS2Object::alloc(sizeof(float32)));
        *p = x;
        return FLOAT_TO_JS2VAL(p);
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
    const String *JS2Engine::convertValueToString(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x))
            return undefined_StringAtom;
        if (JS2VAL_IS_NULL(x))
            return null_StringAtom;
        if (JS2VAL_IS_BOOLEAN(x))
            return (JS2VAL_TO_BOOLEAN(x)) ? true_StringAtom : false_StringAtom;
        if (JS2VAL_IS_INT(x))
            return numberToString(JS2VAL_TO_INT(x));
        if (JS2VAL_IS_LONG(x)) {
            float64 d;
            JSLL_L2D(d, *JS2VAL_TO_LONG(x));
            return numberToString(&d);
        }
        if (JS2VAL_IS_ULONG(x)) {
            float64 d;
            JSLL_UL2D(d, *JS2VAL_TO_ULONG(x));
            return numberToString(&d);
        }
        if (JS2VAL_IS_FLOAT(x)) {
            float64 d = *JS2VAL_TO_FLOAT(x);
            return numberToString(&d);
        }
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

        return STRING_TO_JS2VAL(object_StringAtom);
    
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
        return toFloat64(toPrimitive(x));
    }

    // x is not a number, convert it to one
    js2val JS2Engine::convertValueToGeneralNumber(js2val x)
    {
        // XXX Assuming convert to float64, rather than long/ulong
        return allocNumber(toFloat64(x));
    }

    // x is not an Object, it needs to be wrapped in one
    js2val JS2Engine::convertValueToObject(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x) || JS2VAL_IS_NULL(x) || JS2VAL_IS_SPECIALREF(x))
            meta->reportError(Exception::typeError, "Can't convert to Object", errorPos());
        if (JS2VAL_IS_STRING(x))
            return String_Constructor(meta, JS2VAL_NULL, &x, 1);
        // XXX need more
        return OBJECT_TO_JS2VAL(new PrototypeInstance(meta->objectClass->prototype, meta->objectClass));
    }
    
    // x is a Number, validate that it has no fractional component
    int64 JS2Engine::checkInteger(js2val x)
    {
        int64 i;
        if (JS2VAL_IS_FLOAT(x)) {
            float64 f = *JS2VAL_TO_FLOAT(x);
            if (!JSDOUBLE_IS_FINITE(f))
                meta->reportError(Exception::rangeError, "Non integer", errorPos());
            JSLL_D2L(i, f);
            JSLL_L2D(f, i);
            if (f != *JS2VAL_TO_FLOAT(x))
                meta->reportError(Exception::rangeError, "Non integer", errorPos());
            return i;
        }
        else
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 d = *JS2VAL_TO_DOUBLE(x);
            if (!JSDOUBLE_IS_FINITE(d))
                meta->reportError(Exception::rangeError, "Non integer", errorPos());
            JSLL_D2L(i, d);
            JSLL_L2D(d, i);
            if (d != *JS2VAL_TO_DOUBLE(x))
                meta->reportError(Exception::rangeError, "Non integer", errorPos());
            return i;
        }
        else
        if (JS2VAL_IS_LONG(x)) {
            JSLL_L2I(i, *JS2VAL_TO_LONG(x));
            return i;
        }
        ASSERT(JS2VAL_IS_ULONG(x));
        JSLL_UL2I(i, *JS2VAL_TO_ULONG(x));
        return i;
    }

    // x is any js2val
    float64 JS2Engine::toFloat64(js2val x)
    { 
        if (JS2VAL_IS_INT(x)) 
            return JS2VAL_TO_INT(x); 
        else
        if (JS2VAL_IS_DOUBLE(x)) 
            return *JS2VAL_TO_DOUBLE(x); 
        else
        if (JS2VAL_IS_LONG(x)) {
            float64 d;
            JSLL_L2D(d, *JS2VAL_TO_LONG(x));
            return d;
        }
        else
        if (JS2VAL_IS_ULONG(x)) {
            float64 d;
            JSLL_UL2D(d, *JS2VAL_TO_ULONG(x));
            return d; 
        }
        else
        if (JS2VAL_IS_FLOAT(x))
            return *JS2VAL_TO_FLOAT(x);
        else 
            return convertValueToDouble(x); 
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
        if (JS2VAL_IS_LONG(x) || JS2VAL_IS_ULONG(x))
            return (!JSLL_IS_ZERO(x));
        if (JS2VAL_IS_FLOAT(x)) {
            float64 xd = *JS2VAL_TO_FLOAT(x);
            return ! (JSDOUBLE_IS_POSZERO(xd) || JSDOUBLE_IS_NEGZERO(xd) || JSDOUBLE_IS_NaN(xd));
        }
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 xd = *JS2VAL_TO_DOUBLE(x);
            return ! (JSDOUBLE_IS_POSZERO(xd) || JSDOUBLE_IS_NEGZERO(xd) || JSDOUBLE_IS_NaN(xd));
        }
        if (JS2VAL_IS_STRING(x)) {
            String *str = JS2VAL_TO_STRING(x);
            return (str->length() != 0);
        }
        return true;
    }

    // x is not an int
    int32 JS2Engine::convertValueToInteger(js2val x)
    {
        int32 i;
        if (JS2VAL_IS_LONG(x)) {
            JSLL_L2I(i, *JS2VAL_TO_LONG(x));
            return i;
        }
        if (JS2VAL_IS_ULONG(x)) {
            JSLL_UL2I(i, *JS2VAL_TO_ULONG(x));
            return i;
        }
        if (JS2VAL_IS_FLOAT(x)) {
            float64 f = *JS2VAL_TO_FLOAT(x);
            return toInt32(f);
        }
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 d = *JS2VAL_TO_DOUBLE(x);
            return toInt32(d);
        }
        float64 d = convertValueToDouble(x);
        return toInt32(d);
    }

    int32 JS2Engine::toInt32(float64 d)
    {
        if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d) )
            return 0;
        d = fd::fmod(d, two32);
        d = (d >= 0) ? d : d + two32;
        if (d >= two31)
            return (int32)(d - two32);
        else
            return (int32)(d);    
    }

    uint32 JS2Engine::toUInt32(float64 d)
    {
        if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d) )
            return 0;
        bool neg = (d < 0);
        d = fd::floor(neg ? -d : d);
        d = neg ? -d : d;
        d = fd::fmod(d, two32);
        d = (d >= 0) ? d : d + two32;
        return (uint32)(d);    
    }

    uint16 JS2Engine::toUInt16(float64 d)
    {
        if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d))
            return 0;
        bool neg = (d < 0);
        d = fd::floor(neg ? -d : d);
        d = neg ? -d : d;
        d = fd::fmod(d, two16);
        d = (d >= 0) ? d : d + two16;
        return (uint16)(d);
    }

    // Insert x before the top count stack entries
    void JS2Engine::insert(js2val x, int count)
    {
        ASSERT(sp < (execStack + MAX_EXEC_STACK));
        js2val *p = ++sp;
        for (uint32 i = 0; i < count; i++) {
            *p = p[-1];
            --p;
        }
        *p = x;            
    }


    #define INIT_STRINGATOM(n) n##_StringAtom(allocStringPtr(#n))

    JS2Engine::JS2Engine(World &world)
                : pc(NULL),
                  bCon(NULL),
                  meta(NULL),
                  retval(JS2VAL_VOID),
                  INIT_STRINGATOM(true),
                  INIT_STRINGATOM(false),
                  INIT_STRINGATOM(null),
                  INIT_STRINGATOM(undefined),
                  INIT_STRINGATOM(public),
                  INIT_STRINGATOM(private),
                  INIT_STRINGATOM(function),
                  INIT_STRINGATOM(object),
                  Empty_StringAtom(&world.identifiers[""]),
                  Dollar_StringAtom(&world.identifiers["$"])
    {
        for (int i = 0; i < 256; i++)
            float64Table[i] = NULL;

        nanValue = DOUBLE_TO_JS2VAL(allocNumber(nan));
        posInfValue = DOUBLE_TO_JS2VAL(allocNumber(positiveInfinity));
        negInfValue = DOUBLE_TO_JS2VAL(allocNumber(negativeInfinity));

        sp = execStack = new js2val[MAX_EXEC_STACK];
        activationStackTop = activationStack = new ActivationFrame[MAX_ACTIVATION_STACK];
    }

    // Return the effect of an opcode on the execution stack.
    // Some ops (e.g. eCall) have a variable effect, those are handled separately
    // (see emitOp)
    int getStackEffect(JS2Op op)
    {
        switch (op) {
        case eReturn:
            return -1;  

        case eAdd:          // pop two, push one
        case eSubtract:
        case eMultiply:
        case eDivide:
        case eModulo:
        case eEqual:
        case eNotEqual:
        case eLess:
        case eGreater:
        case eLessEqual:
        case eGreaterEqual:
        case eLogicalXor:
        case eLeftShift:
        case eRightShift:
        case eLogicalRightShift:
        case eBitwiseAnd:
        case eBitwiseXor:
        case eBitwiseOr:
            return -1;  

        case eMinus:        // pop one, push one
        case ePlus:
        case eComplement:
        case eTypeof:
            return 0;

        case eString:
        case eTrue:
        case eFalse:
        case eNumber:
        case eUInt64:
        case eInt64:
        case eNull:
        case eThis:
            return 1;       // push literal value

        case eLexicalRead:
            return 1;       // push the value
        case eLexicalWrite:
            return 0;       // leave the value
        case eLexicalRef:
            return 2;       // push base & value
        case eLexicalDelete:
            return 1;       // push boolean result

        case eDotRead:
            return 0;       // pop a base, push the value
        case eDotWrite:
            return -1;      // pop a base, leave the value
        case eDotRef:
            return 1;       // leave the base, push the value
        case eDotDelete:    // pop base, push boolean result
            return 0;

        case eBracketRead:
            return -1;      // pop a base and an index, push the value
        case eBracketWrite:
            return -2;      // pop a base and an index, leave the value
        case eBracketRef:
            return 1;       // leave the base, pop the index, push the value
        case eBracketDelete:
            return -1;      // pop base and index, push boolean result

        case eReturnVoid:
        case eBranch:
            return 0;

        case eDup:          // duplicate top item
            return 1;      

        case ePop:          // remove top item
            return -1;      

        case ePopv:         // pop a statement result value
            return -1;      

        case ePushFrame:    // affect the frame stack...
        case ePopFrame:     // ...not the exec stack
            return 0;

        case eBranchFalse:
        case eBranchTrue:
            return -1;      // pop the boolean condition

        case eNew:          // pop the class or function, push the new instance
            return 0;

        case eFirst:        // pop object, push iterator helper
            return 1;       // and push boolean result value
        case eNext:         // leave iterator helper
            return 1;       // and push boolean result value
        case eForValue:     // leave the iterator helper
            return 1;       // and push iteration value


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

        case eBracketReadForRef:    // leave base and index, push value
            return 1;

        case eBracketWriteRef:      // pop base and index, leave value
            return -2;

        default:
            ASSERT(false);
        }
        return 0;
    }
    
    // Return the mapped source location for the current pc
    size_t JS2Engine::errorPos()
    {
        return bCon->getPosition(pc - bCon->getCodeStart()); 
    }

    // XXX Default construction of an instance of the class
    // that is the value at the top of the execution stack
    js2val JS2Engine::defaultConstructor(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
    {
        js2val v = meta->engine->pop();
        ASSERT(JS2VAL_IS_OBJECT(v) && !JS2VAL_IS_NULL(v));
        JS2Object *obj = JS2VAL_TO_OBJECT(v);
        ASSERT(obj->kind == ClassKind);
        JS2Class *c = checked_cast<JS2Class *>(obj);
        if (c->dynamic)
            return OBJECT_TO_JS2VAL(new DynamicInstance(c));
        else
            if (c->prototype)
                return OBJECT_TO_JS2VAL(new PrototypeInstance(c->prototype, c));
            else
                return OBJECT_TO_JS2VAL(new FixedInstance(c));
    }

    // Save current engine state (pc, environment top) and
    // jump to start of new bytecodeContainer
    void JS2Engine::jsr(Phase execPhase, BytecodeContainer *new_bCon)
    {
        ASSERT(activationStackTop < (activationStack + MAX_ACTIVATION_STACK));
        activationStackTop->bCon = bCon;
        activationStackTop->pc = pc;
        activationStackTop->phase = phase;
        activationStackTop->topFrame = meta->env.getTopFrame();
        activationStackTop++;
        bCon = new_bCon;
        pc = new_bCon->getCodeStart();
        phase = execPhase;

    }

    // Return to previously saved execution state
    void JS2Engine::rts()
    {
        ASSERT(activationStackTop > activationStack);
        activationStackTop--;

        bCon = activationStackTop->bCon;
        pc = activationStackTop->pc;
        phase = activationStackTop->phase;
        while (meta->env.getTopFrame() != activationStackTop->topFrame)
            meta->env.removeTopFrame();

    }

    // GC-mark any JS2Objects in the activation frame stack, the execution stack
    // and in structures contained in those locations.
    void JS2Engine::mark()
    {
        if (bCon)
            bCon->mark();
        for (ActivationFrame *f = activationStack; (f < activationStackTop); f++) {
            if (f->bCon)
                f->bCon->mark();
        }
        for (js2val *e = execStack; (e < sp); e++) {
            GCMARKVALUE(*e);
        }
        JS2Object::mark(JS2VAL_TO_DOUBLE(nanValue));
        JS2Object::mark(JS2VAL_TO_DOUBLE(posInfValue));
        JS2Object::mark(JS2VAL_TO_DOUBLE(negInfValue));
        for (uint32 i = 0; i < 256; i++) {
            if (float64Table[i])
                JS2Object::mark(float64Table[i]);
        }
        GCMARKVALUE(retval);
        GCMARKVALUE(a);
        GCMARKVALUE(b);
        GCMARKVALUE(baseVal);
        GCMARKVALUE(indexVal);
        JS2Object::mark(true_StringAtom);
        JS2Object::mark(false_StringAtom);
        JS2Object::mark(null_StringAtom);
        JS2Object::mark(undefined_StringAtom);
        JS2Object::mark(public_StringAtom);
        JS2Object::mark(private_StringAtom);
        JS2Object::mark(function_StringAtom);
        JS2Object::mark(object_StringAtom);
        JS2Object::mark(Empty_StringAtom);
        JS2Object::mark(Dollar_StringAtom);
    }

    //
    // XXX Only scanning dynamic properties
    //
    // Initialize and build a list of names of properties in the object.
    //
    bool ForIteratorObject::first()
    {
        if (JS2VAL_IS_NULL(objValue))
            return false;
        JS2Object *obj = JS2VAL_TO_OBJECT(objValue);
        DynamicPropertyMap *dMap = NULL;
        if (obj->kind == DynamicInstanceKind)
            dMap = &(checked_cast<DynamicInstance *>(obj))->dynamicProperties;
        else
        if (obj->kind == GlobalObjectKind)
            dMap = &(checked_cast<GlobalObject *>(obj))->dynamicProperties;
        else
            dMap = &(checked_cast<PrototypeInstance *>(obj))->dynamicProperties;

        if (dMap) {
            originalObj = obj;
            nameList = new const String *[dMap->size()];
            length = 0;
            for (DynamicPropertyIterator i = dMap->begin(), end = dMap->end(); (i != end); i++) {
                nameList[length++] = &i->first;
            }
            ASSERT(length == dMap->size());
            it = 0;
            return (length != 0);
        }
        else
            return false;
    }

    //
    // Set the iterator to the first property in that list that is not
    // shadowed by a property higher up the prototype chain. If we get 
    // to the end of the list, bump down to the next object on the chain.
    //
    bool ForIteratorObject::next(JS2Engine *engine)
    {
        if (nameList) {
            JS2Object *obj = JS2VAL_TO_OBJECT(objValue);
            it++;
            if (originalObj != obj) {
                while (it != length)
                    if (engine->meta->lookupDynamicProperty(originalObj, nameList[it]) != obj) it++;
            }
            return (it != length);
        }
        else
            return false;
    }

    js2val ForIteratorObject::value(JS2Engine *engine)
    { 
        ASSERT(nameList);
        return engine->allocString(nameList[it]);
    }

}
}

