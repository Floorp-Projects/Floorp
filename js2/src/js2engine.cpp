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
#include "msvc_pragma.h"
#endif

#include <algorithm>
#include <assert.h>
#include <list>
#include <stack>

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
    js2val JS2Engine::interpret(Phase execPhase, BytecodeContainer *targetbCon, Environment *env)
    {
        packageFrame = env->getPackageFrame();
        jsr(execPhase, targetbCon, sp - execStack, JS2VAL_VOID, env);
        ActivationFrame *f = activationStackTop;
        js2val result;
        try {
            result = interpreterLoop();
        }
        catch (Exception &jsx) {
            activationStackTop = f;
            rts();
            throw jsx;
        }
        activationStackTop = f - 1; // when execution falls 'off the bottom' an rts hasn't occurred
                                    // so the activation stack is off by 1.
        return result;
    }

    // Execute the opcode sequence at pc.
    js2val JS2Engine::interpreterLoop()
    {
        js2val a = JS2VAL_VOID;
        js2val b = JS2VAL_VOID;
        js2val baseVal = JS2VAL_VOID;
        js2val indexVal = JS2VAL_VOID;
	    ParameterFrame *pFrame = NULL;
        const String *astr = NULL;
        const String *bstr = NULL;
        DEFINE_ROOTKEEPER(meta, rk1, pFrame);
        DEFINE_ROOTKEEPER(meta, rk2, astr);
        DEFINE_ROOTKEEPER(meta, rk3, bstr);

        DEFINE_ROOTKEEPER(meta, rk4, a);
        DEFINE_ROOTKEEPER(meta, rk5, b);
        DEFINE_ROOTKEEPER(meta, rk6, baseVal);
        DEFINE_ROOTKEEPER(meta, rk7, indexVal);

        retval = JS2VAL_VOID;
        while (true) {
            try {
                a = JS2VAL_VOID;
                b = JS2VAL_VOID;
#ifdef TRACE_DEBUG
                if (traceInstructions)
                    printInstruction(pc, bCon->getCodeStart(), bCon, this);
#endif
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
            }
            catch (Exception &jsx) {
                if (mTryStack.size() > 0) {
                    // The handler for this exception is on the top of the try stack.
                    // It specifies the activation that was active when the try block
                    // was entered. We unwind the activation stack, looking for the
                    // one that matches the handler's. The bytecode container, pc and
                    // sp are all reset appropriately, and execution continues.
                    HandlerData *hndlr = (HandlerData *)mTryStack.top();
                    ActivationFrame *curAct = (activationStackEmpty()) ? NULL : (activationStackTop - 1);
                
                    js2val x = JS2VAL_UNDEFINED;
                    if (curAct != hndlr->mActivation) {
                        ASSERT(!activationStackEmpty());
                        ActivationFrame *prev;
                        do {
                            prev = curAct;
                            if (prev->pc == NULL) {
                                // Yikes! the exception is getting thrown across a re-invocation
                                // of the interpreter loop.
                                // (pc == NULL) is the flag on an activation to indicate that the
                                // interpreter loop was re-invoked (probably a 'load' or 'eval' is
                                // in process). In this case we simply re-throw the exception and let
                                // the prior invocation deal with it.

                                throw jsx;
                            }
                            // we need to clean up each activation as we pop it off
                            // - basically this means just resetting it's frame
                            curAct = --activationStackTop;
                            localFrame = activationStackTop->localFrame;
                            parameterFrame = activationStackTop->parameterFrame;
                            parameterSlots = activationStackTop->parameterSlots;
                            if (parameterFrame)
                                parameterFrame->argSlots = parameterSlots;
                            bCon = activationStackTop->bCon;
                            if (hndlr->mActivation != curAct) {
                                while (activationStackTop->newEnv->getTopFrame() != activationStackTop->topFrame)
                                    activationStackTop->newEnv->removeTopFrame();
                                meta->env = activationStackTop->env;                        
                            }
                            else
                                break;
                        } while (true);
                        activationStackTop = prev;      // need the one before the target function to 
                                                        // be at the top, because of postincrement
                        localFrame = activationStackTop->localFrame;
                        parameterFrame = activationStackTop->parameterFrame;
                        parameterSlots = activationStackTop->parameterSlots;
                        if (parameterFrame)
                            parameterFrame->argSlots = parameterSlots;
                        bCon = activationStackTop->bCon;
                        meta->env = activationStackTop->env;
                    }
                    // make sure there's a JS object for the catch clause to work with
                    if (jsx.hasKind(Exception::userException)) {
                        x = jsx.value;
                    }
                    else {
                        js2val argv[1];
                        argv[0] = allocString(new String(jsx.fullMessage()));
                        switch (jsx.kind) {
                        case Exception::syntaxError:
                            x = SyntaxError_Constructor(meta, JS2VAL_NULL, argv, 1);
                            break;
                        case Exception::referenceError:
                            x = ReferenceError_Constructor(meta, JS2VAL_NULL, argv, 1);
                            break;
                        case Exception::typeError:
                            x = TypeError_Constructor(meta, JS2VAL_NULL, argv, 1);
                            break;
                        case Exception::rangeError:
                            x = RangeError_Constructor(meta, JS2VAL_NULL, argv, 1);
                            break;
                        default:
                            x = Error_Constructor(meta, JS2VAL_NULL, argv, 1);
                            break;
                        }
                    }
                    sp = execStack + hndlr->mStackTop;
                    pc = hndlr->mPC;
                    meta->env->setTopFrame(hndlr->mFrame);
                    push(x);
                }
                else
                    throw jsx; //reportError(Exception::uncaughtError, "No handler for throw");
            }
        }
        return retval;
    }

    // See if the double value is in the hash table, return it's pointer if so
    // If not, fill the table or return a un-hashed pointer
    float64 *JS2Engine::newDoubleValue(float64 x)
    {
        float64 *p = (float64 *)meta->alloc(sizeof(float64), PondScum::GenericFlag);
        *p = x;
        return p;

/*
        union {
            float64 x;
            uint8 a[8];
        } u;

        u.x = x;
        uint8 hash = (uint8)(u.a[0] ^ u.a[1] ^ u.a[2] ^ u.a[3] ^ u.a[4] ^ u.a[5] ^  u.a[6] ^ u.a[7]);
        if (float64Table[hash]) {
            if (*float64Table[hash] == x)
                return float64Table[hash];
            else {
                float64 *p = (float64 *)JS2Object::alloc(sizeof(float64), PondScum::GenericFlag);
                *p = x;
                return p;
            }
        }
        else {
            float64 *p = (float64 *)JS2Object::alloc(sizeof(float64), PondScum::GenericFlag);
            float64Table[hash] = p;
            *p = x;
            return p;
        }
*/
    }

/*

    runtime strings will now be allocated from a separate 'World'
    which is GC-able

*/

    String *JS2Engine::allocStringPtr(const char *s)
    { 
        String *p = (String *)(meta->alloc(sizeof(String), PondScum::StringFlag));
        size_t len = strlen(s);
        String *result = new (p) String(len, uni::null);
        for (size_t i = 0; i < len; i++) {
            (*result)[i] = widen(s[i]);
        }
//        std::transform(s, s+len, result->begin(), widen);
        return result; 
    }

    String *JS2Engine::allocStringPtr(const char16 *s, uint32 length)
    { 
        String *p = (String *)(meta->alloc(sizeof(String), PondScum::StringFlag));
        String *result = new (p) String(s, length);
        return result; 
    }

    String *JS2Engine::allocStringPtr(const String *s)
    {
        String *p = (String *)(meta->alloc(sizeof(String), PondScum::StringFlag));
        return new (p) String(*s);
    }

    String *JS2Engine::allocStringPtr(const String *s, uint32 index, uint32 length)
    {
        String *p = (String *)(meta->alloc(sizeof(String), PondScum::StringFlag));
        return new (p) String(*s, index, length);
    }

    String *JS2Engine::concatStrings(const String *s1, const String *s2)
    {
        String *p = (String *)(meta->alloc(sizeof(String), PondScum::StringFlag));
        String *result = new (p) String(*s1);
        result->append(*s2);
        return result;
    }

    // if the argument can be stored as an integer value, do so
    // otherwise get a double value
    js2val JS2Engine::allocNumber(float64 x)
    {
        int32 i;
        js2val retval;
        if (JSDOUBLE_IS_INT(x, i) && INT_FITS_IN_JS2VAL(i))
            retval = INT_TO_JS2VAL(i);
        else {
            if (JSDOUBLE_IS_NaN(x))
                return nanValue;
            retval = DOUBLE_TO_JS2VAL(newDoubleValue(x));
        }
        return retval;
    }

    // Don't store as an int, even if possible, we need to retain 'longness'
    js2val JS2Engine::allocULong(uint64 x)
    {
        uint64 *p = (uint64 *)(meta->alloc(sizeof(uint64), PondScum::GenericFlag));
        *p = x;
        return ULONG_TO_JS2VAL(p);
        
    }

    // Don't store as an int, even if possible, we need to retain 'longness'
    js2val JS2Engine::allocLong(int64 x)
    {
        int64 *p = (int64 *)(meta->alloc(sizeof(int64), PondScum::GenericFlag));
        *p = x;
        return LONG_TO_JS2VAL(p);
    }

    // Don't store as an int, even if possible, we need to retain 'floatness'
    js2val JS2Engine::allocFloat(float32 x)
    {
        float32 *p = (float32 *)(meta->alloc(sizeof(float32), PondScum::GenericFlag));
        *p = x;
        return FLOAT_TO_JS2VAL(p);
    }

    // Convert an integer to a string
    String *JS2Engine::numberToString(int32 i)
    {
        char buf[dtosStandardBufferSize];
        const char *chrp = doubleToStr(buf, dtosStandardBufferSize, i, dtosStandard, 0);
        return allocStringPtr(chrp);
    }
    // Convert an integer to a string
    String *JS2Engine::numberToString(uint32 i)
    {
        char buf[dtosStandardBufferSize];
        const char *chrp = doubleToStr(buf, dtosStandardBufferSize, i, dtosStandard, 0);
        return allocStringPtr(chrp);
    }

    // Convert a double to a string
    String *JS2Engine::numberToString(float64 *number)
    {
        char buf[dtosStandardBufferSize];
        const char *chrp = doubleToStr(buf, dtosStandardBufferSize, *number, dtosStandard, 0);
        return allocStringPtr(chrp);
    }

    // Convert an integer to a stringAtom
    StringAtom &JS2Engine::numberToStringAtom(int32 i)
    {
        char buf[dtosStandardBufferSize];
        const char *chrp = doubleToStr(buf, dtosStandardBufferSize, i, dtosStandard, 0);
        return meta->world.identifiers[chrp];
    }
    // Convert an integer to a stringAtom
    StringAtom &JS2Engine::numberToStringAtom(uint32 i)
    {
        char buf[dtosStandardBufferSize];
        const char *chrp = doubleToStr(buf, dtosStandardBufferSize, i, dtosStandard, 0);
        return meta->world.identifiers[chrp];
    }

    // Convert a double to a stringAtom
    StringAtom &JS2Engine::numberToStringAtom(float64 *number)
    {
        char buf[dtosStandardBufferSize];
        const char *chrp = doubleToStr(buf, dtosStandardBufferSize, *number, dtosStandard, 0);
        return meta->world.identifiers[chrp];
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
        else
        if (JS2VAL_IS_ULONG(x)) {
            JSLL_UL2I(i, *JS2VAL_TO_ULONG(x));
            return i;
        }
        ASSERT(JS2VAL_IS_INT(x));
        return JS2VAL_TO_INT(x);
    }

    float64 JS2Engine::truncateFloat64(float64 d)
    {
        if (d == 0)
            return d;
        if (!JSDOUBLE_IS_FINITE(d)) {
            if (JSDOUBLE_IS_NaN(d))
                return 0;
            return d;
        }
        bool neg = (d < 0);
        d = fd::floor(neg ? -d : d);
        return neg ? -d : d;
    }

    int32 JS2Engine::float64toInt32(float64 d)
    {
        if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d) )
            return 0;
        d = fd::fmod(d, two32);
        d = (d >= 0) ? fd::floor(d) : fd::ceil(d) + two32;
        if (d >= two31)
            return (int32)(d - two32);
        else
            return (int32)(d);    
    }

    int64 JS2Engine::float64toInt64(float64 f)
    {
        int64 i;
        JSLL_D2L(i, f);
        return i;
    }

    uint32 JS2Engine::float64toUInt32(float64 d)
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

    uint16 JS2Engine::float64toUInt16(float64 d)
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
        ASSERT(sp < execStackLimit);
        js2val *p = ++sp;
        for (int32 i = 0; i < count; i++) {
            *p = p[-1];
            --p;
        }
        *p = x;            
    }


    #define INIT_STRINGATOM(n) n##_StringAtom(allocStringPtr(#n))

    JS2Engine::JS2Engine(JS2Metadata *meta, World &world)
                : meta(meta),
                  pc(NULL),
                  bCon(NULL),
                  phase(RunPhase),
                  true_StringAtom(world.identifiers["true"]),
                  false_StringAtom(world.identifiers["false"]),
                  null_StringAtom(world.identifiers["null"]),
                  undefined_StringAtom(world.identifiers["undefined"]),
                  public_StringAtom(world.identifiers["public"]),
                  private_StringAtom(world.identifiers["private"]),
                  Function_StringAtom(world.identifiers["Function"]),
                  Object_StringAtom(world.identifiers["Object"]),
                  object_StringAtom(world.identifiers["object"]),
                  Empty_StringAtom(world.identifiers[""]),
                  Dollar_StringAtom(world.identifiers["$"]),
                  prototype_StringAtom(world.identifiers["prototype"]),
                  length_StringAtom(world.identifiers["length"]),
                  toString_StringAtom(world.identifiers["toString"]),
                  valueOf_StringAtom(world.identifiers["valueOf"]),
                  packageFrame(NULL),
                  localFrame(NULL),
                  parameterFrame(NULL),
                  parameterSlots(NULL),
				  parameterCount(0),
				  superConstructorCalled(false),
                  traceInstructions(false)
    {
        for (int i = 0; i < 256; i++)
            float64Table[i] = NULL;

        float64 *p = (float64 *)meta->alloc(sizeof(float64), PondScum::GenericFlag);
        *p = nan;
        nanValue = DOUBLE_TO_JS2VAL(p);
        posInfValue = DOUBLE_TO_JS2VAL(allocNumber(positiveInfinity));
        negInfValue = DOUBLE_TO_JS2VAL(allocNumber(negativeInfinity));

        sp = execStack = new js2val[INITIAL_EXEC_STACK];
        execStackLimit = execStack + INITIAL_EXEC_STACK;
        activationStackTop = activationStack = new ActivationFrame[MAX_ACTIVATION_STACK];
    }

    JS2Engine::~JS2Engine()
    {
        while (!mTryStack.empty()) {
            popHandler();
        }
        delete [] execStack;
        delete [] activationStack;
    }

#ifdef TRACE_DEBUG

    enum { BRANCH_OFFSET = 1, STR_PTR, TYPE_PTR, NAME_INDEX, FRAME_INDEX, BRANCH_PAIR, U16, FLOAT64, S32, BREAK_OFFSET_AND_COUNT };
    struct {
        JS2Op op;
        char *name;
        int flags;
    } opcodeData[] =
    {
        { eAdd,  "Add", 0 },
        { eSubtract,  "Subtract", 0 },
        { eMultiply,  "Multiply", 0 },
        { eDivide,  "Divide", 0 },
        { eModulo,  "Modulo", 0 },
        { eEqual,  "Equal", 0 },
        { eNotEqual,  "NotEqual", 0 },
        { eLess,  "Less", 0 },
        { eGreater,  "Greater", 0 },
        { eLessEqual,  "LessEqual", 0 },
        { eGreaterEqual,  "GreaterEqual", 0 },
        { eIdentical,  "Identical", 0 },
        { eNotIdentical,  "NotIdentical", 0 },
        { eLogicalXor,  "LogicalXor", 0 },
        { eLogicalNot,  "LogicalNot", 0 },
        { eMinus,  "Minus", 0 },
        { ePlus,  "Plus", 0 },
        { eComplement,  "Complement", 0 },
        { eLeftShift,  "LeftShift", 0 },
        { eRightShift,  "RightShift", 0 },
        { eLogicalRightShift,  "LogicalRightShift", 0 },
        { eBitwiseAnd,  "BitwiseAnd", 0 },
        { eBitwiseXor,  "BitwiseXor", 0 },
        { eBitwiseOr,  "BitwiseOr", 0 },
        { eTrue,  "True", 0 },
        { eFalse,  "False", 0 },
        { eNull,  "Null", 0 },
        { eUndefined,  "Undefined", 0 },
        { eLongZero,  "0(64)", 0 },
        { eNumber,  "Number", FLOAT64 },
        { eInteger,  "Integer", S32 },
        { eRegExp,  "RegExp", U16 },
        { eFunction,  "Function", U16 },
        { eClosure,  "Closure", U16 },
        { eUInt64,  "UInt64", 0 },
        { eInt64,  "Int64", 0 },
        { eString,  "String", STR_PTR },            // <string pointer:u32>
        { eThis,  "This", 0 },
        { eSuper,  "Super", 0 },
        { eSuperExpr,  "SuperExpr", 0 },
        { eNewObject,  "NewObject", U16 },         // <argCount:u16>
        { eNewArray,  "NewArray", U16 },          // <argCount:u16>

        { eThrow,  "Throw", 0 },
        { eTry,  "Try", BRANCH_PAIR },               // <finally displacement:s32> <catch displacement:s32>
        { eCallFinally,  "CallFinally", BRANCH_OFFSET },       // <branch displacement:s32>
        { eReturnFinally,  "ReturnFinally", 0 },
        { eHandler,  "Handler", 0 },

        { eCoerce, "Coerce", TYPE_PTR },            // <type pointer:u32>

        { eFirst,  "First", 0 },
        { eNext,  "Next", 0 },
        { eForValue,  "ForValue", 0 },

        { eFrameSlotRead, "FrameSlotRead", U16 },           // <slot index:u16>
        { eFrameSlotRef, "FrameSlotRef", U16 },             // <slot index:u16>
        { eFrameSlotWrite, "FrameSlotWrite", U16 },         // <slot index:u16>
        { eFrameSlotDelete, "FrameSlotDelete", U16 },       // <slot index:u16>
        { eFrameSlotPostInc, "FrameSlotPostInc", U16 },     // <slot index:u16>
        { eFrameSlotPostDec, "FrameSlotPostDec", U16 },     // <slot index:u16>
        { eFrameSlotPreInc, "FrameSlotPreInc", U16 },       // <slot index:u16>
        { eFrameSlotPreDec, "FrameSlotPreDec", U16 },       // <slot index:u16>

        { eParameterSlotRead, "ParameterSlotRead", U16 },           // <slot index:u16>
        { eParameterSlotRef, "ParameterSlotRef", U16 },             // <slot index:u16>
        { eParameterSlotWrite, "ParameterSlotWrite", U16 },         // <slot index:u16>
        { eParameterSlotDelete, "ParameterSlotDelete", U16 },       // <slot index:u16>
        { eParameterSlotPostInc, "ParameterSlotPostInc", U16 },     // <slot index:u16>
        { eParameterSlotPostDec, "ParameterSlotPostDec", U16 },     // <slot index:u16>
        { eParameterSlotPreInc, "ParameterSlotPreInc", U16 },       // <slot index:u16>
        { eParameterSlotPreDec, "ParameterSlotPreDec", U16 },       // <slot index:u16>

        { ePackageSlotRead, "PackageSlotRead", U16 },           // <slot index:u16>
        { ePackageSlotRef, "PackageSlotRef", U16 },             // <slot index:u16>
        { ePackageSlotWrite, "PackageSlotWrite", U16 },         // <slot index:u16>
        { ePackageSlotDelete, "PackageSlotDelete", U16 },       // <slot index:u16>
        { ePackageSlotPostInc, "PackageSlotPostInc", U16 },     // <slot index:u16>
        { ePackageSlotPostDec, "PackageSlotPostDec", U16 },     // <slot index:u16>
        { ePackageSlotPreInc, "PackageSlotPreInc", U16 },       // <slot index:u16>
        { ePackageSlotPreDec, "PackageSlotPreDec", U16 },       // <slot index:u16>

        { eLexicalRead,  "LexicalRead", NAME_INDEX },       // <multiname index:u16>
        { eLexicalWrite,  "LexicalWrite", NAME_INDEX },     // <multiname index:u16>
        { eLexicalInit,  "LexicalInit", NAME_INDEX },       // <multiname index:u16>
        { eLexicalRef,  "LexicalRef", NAME_INDEX },         // <multiname index:u16>
        { eLexicalDelete,  "LexicalDelete", NAME_INDEX },   // <multiname index:u16>
        { eDotRead,  "DotRead", NAME_INDEX },               // <multiname index:u16>
        { eDotWrite,  "DotWrite", NAME_INDEX },             // <multiname index:u16>
        { eDotRef,  "DotRef", NAME_INDEX },                 // <multiname index:u16>
        { eDotDelete,  "DotDelete", NAME_INDEX },           // <multiname index:u16>
        { eBracketRead,  "BracketRead", 0 },
        { eBracketWrite,  "BracketWrite", 0 },
        { eBracketRef,  "BracketRef", 0 },
        { eBracketReadForRef,  "BracketReadForRef", 0 },
        { eBracketWriteRef,  "BracketWriteRef", 0 },
        { eBracketDelete,  "BracketDelete", 0 },
        { eSlotRead,  "SlotRead", U16 },            // <slot index:u16>
        { eSlotWrite,  "SlotWrite", U16 },          // <slot index:u16>
        { eSlotRef,  "SlotRef", U16 },              // <slot index:u16>
        { eSlotDelete,  "SlotDelete", U16 },        // <slot index:u16>

        { eReturn,  "Return", 0 },
        { eReturnVoid,  "ReturnVoid", 0 },
        { ePushFrame,  "PushFrame", FRAME_INDEX },         // <frame index:u16>
        { ePopFrame,  "PopFrame", 0 },
        { eWithin, "With", 0 },
        { eWithout, "EndWith", 0 },
        { eBranchFalse,  "BranchFalse", BRANCH_OFFSET },        // <branch displacement:s32> XXX save space with short and long versions instead ?
        { eBranchTrue,  "BranchTrue", BRANCH_OFFSET },          // <branch displacement:s32>
        { eBranch,  "Branch", BRANCH_OFFSET },                  // <branch displacement:s32>
        { eBreak,  "Break", BREAK_OFFSET_AND_COUNT },           // <branch displacement:s32> <blockCount:u16>
        { eNew,  "New", U16 },                // <argCount:u16>
        { eCall,  "Call", U16 },              // <argCount:u16>
        { eSuperCall,  "SuperCall", U16 },    // <argCount:u16>
        { eTypeof,  "Typeof", 0 },
        { eInstanceof,  "Instanceof", 0 },
        { eIs,  "Is", 0 },
        { eIn,  "In", 0 },

        { ePopv,  "Popv", 0 },
        { ePop,  "Pop", 0 },
        { eDup,  "Dup", 0 },
        { eSwap,  "Swap", 0 },
        { eSwap2, "Swap2", 0 },
        { eVoid,  "Void", 0 },

        { eLexicalPostInc,  "LexicalPostInc", NAME_INDEX },    // <multiname index:u16>
        { eLexicalPostDec,  "LexicalPostDec", NAME_INDEX },    // <multiname index:u16>
        { eLexicalPreInc,  "LexicalPreInc", NAME_INDEX },     // <multiname index:u16>
        { eLexicalPreDec,  "LexicalPreDec", NAME_INDEX },     // <multiname index:u16>
        { eDotPostInc,  "DotPostInc", NAME_INDEX },        // <multiname index:u16>
        { eDotPostDec,  "DotPostDec", NAME_INDEX },        // <multiname index:u16>
        { eDotPreInc,  "DotPreInc", NAME_INDEX },         // <multiname index:u16>
        { eDotPreDec,  "DotPreDec", NAME_INDEX },         // <multiname index:u16>
        { eBracketPostInc,  "BracketPostInc", 0 },
        { eBracketPostDec,  "BracketPostDec", 0 },
        { eBracketPreInc,  "BracketPreInc", 0 },
        { eBracketPreDec,  "BracketPreDec", 0 },
        { eSlotPostInc,  "SlotPostInc", U16 },      // <slot index:u16>
        { eSlotPostDec, "SlotPostDec", U16 },       // <slot index:u16>
        { eSlotPreInc, "SlotPreInc", U16 },         // <slot index:u16>
        { eSlotPreDec, "SlotPreDec", U16 },         // <slot index:u16>

        
        { eNop, "Nop", 0 },

    };

    uint8 *printInstruction(uint8 *pc, uint8 *start, BytecodeContainer *bCon, JS2Engine *engine)
    {
        if (engine) {
            stdOut << bCon->fName << " ";
            if (bCon->fName.length() < 30) {
                for (uint32 i = 0; i < (30 - bCon->fName.length()); i++)
                    stdOut << " ";
            }
            printFormat(stdOut, "%.4d %.4d %.4d ", pc - start, 
                            (int32)(engine->sp - engine->execStack), 
                            (int32)(engine->activationStackTop - engine->activationStack));
        }
        else
            printFormat(stdOut, "%.4d ", pc - start);
        stdOut << opcodeData[*pc].name;
        switch (opcodeData[*pc++].flags) {
        case BRANCH_OFFSET:
            {
                int32 offset = BytecodeContainer::getOffset(pc);
                stdOut << " " << offset << " --> " << (pc - start) + offset;
                pc += sizeof(int32);
            }
            break;
        case BREAK_OFFSET_AND_COUNT:
            {
                int32 offset = BytecodeContainer::getOffset(pc);
                stdOut << " " << offset << " --> " << (pc - start) + offset;
                pc += sizeof(int32);
                printFormat(stdOut, " (%d)", BytecodeContainer::getShort(pc));
                pc += sizeof(short);
            }
            break;
        case TYPE_PTR:
            {
                JS2Class *c = BytecodeContainer::getType(pc);
                stdOut << " " << c->name;
                pc += sizeof(JS2Class *);
            }
            break;
        case STR_PTR:
            {
                uint16 index = BytecodeContainer::getShort(pc);
                stdOut << " \"" << bCon->mStringList[index] << "\"";
                pc += sizeof(short);
            }
            break;
        case NAME_INDEX:
            {
                Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
                stdOut << " " << mn->name;
                pc += sizeof(short);
            }
            break;
        case S32:
            {
                printFormat(stdOut, " %d", BytecodeContainer::getInt32(pc));
                pc += sizeof(int32);
            }
            break;
        case FRAME_INDEX:
        case U16:
            {
                printFormat(stdOut, " %d", BytecodeContainer::getShort(pc));
                pc += sizeof(short);
            }
            break;
        case BRANCH_PAIR:
            {
                int32 offset1 = BytecodeContainer::getOffset(pc);
                pc += sizeof(int32);
                int32 offset2 = BytecodeContainer::getOffset(pc);
                pc += sizeof(int32);
                if (offset1 == (int32)NotALabel)
                    stdOut << " no finally; ";
                else
                    stdOut << " (finally) " << offset1 << " --> " << (pc - start) + offset1 << "; ";
                if (offset2 == (int32)NotALabel)
                    stdOut << "no catch;";
                else
                    stdOut << "(catch) " << offset2 << " --> " << (pc - start) + offset2;
            }
            break;
        case FLOAT64:
            {
                stdOut << " " << BytecodeContainer::getFloat64(pc);
                pc += sizeof(float64);
            }
            break;
        }
        stdOut << "\n";
        return pc;
    }

    void dumpBytecode(BytecodeContainer *bCon)
    {
        uint8 *start = bCon->getCodeStart();
        uint8 *end = bCon->getCodeEnd();
        uint8 *pc = start;
        while (pc < end) {
            pc = printInstruction(pc, start, bCon, NULL);
        }
    }

#endif          // DEBUG

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
        case eIdentical:
        case eNotIdentical:
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
        case eLogicalNot:
            return 0;

        case eIs:           // pop expr, pop type, push boolean
        case eInstanceof:
        case eIn:
            return 1;

        case eCoerce:       // pop value, push coerced value (type is arg)
            return 0;

        case eTry:          // no exec. stack impact
        case eHandler:
        case eCallFinally:
        case eReturnFinally:
            return 0;

        case eThrow:
            return -1;      // pop the exception object

        case eString:
        case eTrue:
        case eFalse:
        case eNumber:
        case eInteger:
        case eUInt64:
        case eInt64:
        case eNull:
        case eThis:
        case eSuper:
        case eRegExp:
        case eFunction:
        case eUndefined:
        case eLongZero:
            return 1;       // push literal value

        case eClosure:
            return 0;

        case eSuperExpr:
            return 0;

        case eSlotWrite:
            return -1;      // write the value, don't preserve it
        case eSlotRead:
            return 1;       // push the value
        case eSlotDelete:   
            return 1;       // push boolean result
        case eSlotRef:      
            return 2;       // push base & value

        case eLexicalInit:
            return -1;      // pop the value
        case eLexicalRead:
            return 1;       // push the value
        case eLexicalWrite:
            return 0;       // leave the value
        case eLexicalRef:
            return 2;       // push base & value
        case eLexicalDelete:
            return 1;       // push boolean result

        case eFrameSlotRead:
        case ePackageSlotRead:
        case eParameterSlotRead:
            return 1;          // push value
        case eFrameSlotWrite:
        case ePackageSlotWrite:
        case eParameterSlotWrite:
            return 0;          // leaves value on stack
        case eFrameSlotRef:
        case ePackageSlotRef:
        case eParameterSlotRef:
            return 2;          // push base and value
        case eFrameSlotDelete:
        case ePackageSlotDelete:
        case eParameterSlotDelete:
            return 1;          // push boolean result;

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
            return 0;       // leave the base, pop the index, push the value
        case eBracketDelete:
            return -1;      // pop base and index, push boolean result

        case eReturnVoid:
        case eBranch:
        case eBreak:
            return 0;

        case eVoid:         // remove top item, push undefined
            return 0;      

        case eDup:          // duplicate top item
            return 1;      

        case eSwap:         // swap top two items
        case eSwap2:        // or top three items
            return 0;      

        case ePop:          // remove top item
            return -1;      

        case ePopv:         // pop a statement result value
            return -1;      

        case ePushFrame:    // affect the frame stack...
        case ePopFrame:     // ...not the exec stack
            return 0;

        case eWithin:
            return -1;      // pop with'd object
        case eWithout:       
            return 0;       // frame stack pop only

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

        case eFrameSlotPostInc:
        case eFrameSlotPostDec:
        case eFrameSlotPreInc:
        case eFrameSlotPreDec:
        case eParameterSlotPostInc:
        case eParameterSlotPostDec:
        case eParameterSlotPreInc:
        case eParameterSlotPreDec:
        case ePackageSlotPostInc:
        case ePackageSlotPostDec:
        case ePackageSlotPreInc:
        case ePackageSlotPreDec:
            return 1;           // push the new/old value

        case eLexicalPostInc:
        case eLexicalPostDec:
        case eLexicalPreInc:
        case eLexicalPreDec:
            return 1;           // push the new/old value

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


        case eNop:
            return 0;

        default:
            ASSERT(false);
        }
        return 0;
    }
    
    // Return the mapped source location for the current pc
    size_t JS2Engine::errorPos()
    {
        return bCon->getPosition((uint16)(pc - bCon->getCodeStart())); 
    }

    // XXX Default construction of an instance of the class
    // that is the value of the passed in 'this'
    js2val JS2Engine::defaultConstructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        ASSERT(JS2VAL_IS_OBJECT(thisValue) && !JS2VAL_IS_NULL(thisValue));
        JS2Object *obj = JS2VAL_TO_OBJECT(thisValue);
        ASSERT(obj->kind == ClassKind);
        JS2Class *c = checked_cast<JS2Class *>(obj);
        if (!c->complete)
            meta->reportError(Exception::constantError, "Cannot construct an instance of a class before its definition has been compiled", meta->engine->errorPos());
        SimpleInstance *result = new (meta) SimpleInstance(meta, c->prototype, c);
        DEFINE_ROOTKEEPER(meta, rk, result);
        meta->invokeInit(c, OBJECT_TO_JS2VAL(result), argv, argc);
        return OBJECT_TO_JS2VAL(result);
    }

    // Save current engine state (pc, environment top) and
    // jump to start of new bytecodeContainer
    void JS2Engine::jsr(Phase execPhase, BytecodeContainer *new_bCon, uint32 stackBase, js2val returnVal, Environment *env)
    {
        if (activationStackTop >= (activationStack + MAX_ACTIVATION_STACK))
            meta->reportError(Exception::internalError, "out of activation stack", meta->engine->errorPos());
        activationStackTop->bCon = bCon;
        activationStackTop->pc = pc;
        activationStackTop->phase = phase;
        activationStackTop->execStackBase = stackBase;
        activationStackTop->retval = returnVal;
        activationStackTop->env = meta->env;    // save current environment, to be restored on rts
        activationStackTop->newEnv = env;       // and save the new environment, if an exception occurs, we can't depend on meta->env
        activationStackTop->topFrame = env->getTopFrame();  // remember how big the new env. is supposed to be so that local frames don't accumulate
        activationStackTop->localFrame = localFrame;
        activationStackTop->parameterFrame = parameterFrame;
        activationStackTop->parameterSlots = parameterSlots;
        activationStackTop->parameterCount = parameterCount;
        activationStackTop->superConstructorCalled = superConstructorCalled;
        activationStackTop++;
        if (new_bCon) {
            bCon = new_bCon;
            if ((int32)bCon->getMaxStack() >= (execStackLimit - sp)) {
                uint32 curDepth = sp - execStack;
                uint32 newDepth = curDepth + bCon->getMaxStack();
                js2val *newStack = new js2val[newDepth];
                ::memcpy(newStack, execStack, curDepth * sizeof(js2val));
                execStack = newStack;
                sp = execStack + curDepth;
                execStackLimit = execStack + newDepth;
            }
            pc = new_bCon->getCodeStart();
        }
        phase = execPhase;
        meta->env = env;
		retval = JS2VAL_VOID;
    }

    // Return to previously saved execution state
    void JS2Engine::rts()
    {
        ASSERT(activationStackTop > activationStack);
        activationStackTop--;

        bCon = activationStackTop->bCon;
        pc = activationStackTop->pc;
        phase = activationStackTop->phase;
        localFrame = activationStackTop->localFrame;
        parameterFrame = activationStackTop->parameterFrame;
        parameterSlots = activationStackTop->parameterSlots;
        parameterCount = activationStackTop->parameterCount;
        superConstructorCalled = activationStackTop->superConstructorCalled;
        if (parameterFrame) {
            parameterFrame->argSlots = parameterSlots;
            parameterFrame->argCount = parameterCount;
            parameterFrame->superConstructorCalled = superConstructorCalled;
        }
        // reset the env. top
        while (activationStackTop->newEnv->getTopFrame() != activationStackTop->topFrame)
            activationStackTop->newEnv->removeTopFrame();
        // reset to previous env.
        meta->env = activationStackTop->env;
        sp = execStack + activationStackTop->execStackBase;

		if (!JS2VAL_IS_VOID(activationStackTop->retval))
			retval = activationStackTop->retval;
    
	}

    // GC-mark any JS2Objects in the activation frame stack, the execution stack
    // and in structures contained in those locations.
    void JS2Engine::mark()
    {
        uint32 i;
        if (bCon)
            bCon->mark();
        for (ActivationFrame *f = activationStack; (f < activationStackTop); f++) {
            GCMARKOBJECT(f->env);
            GCMARKOBJECT(f->newEnv);
            if (f->bCon)
                f->bCon->mark();
            if (f->parameterSlots) {
                for (i = 0; i < f->parameterCount; i++)
                    GCMARKVALUE(f->parameterSlots[i]);
            }
        }
        for (js2val *e = execStack; (e < sp); e++) {
            GCMARKVALUE(*e);
        }
        JS2Object::mark(JS2VAL_TO_DOUBLE(nanValue));
        JS2Object::mark(JS2VAL_TO_DOUBLE(posInfValue));
        JS2Object::mark(JS2VAL_TO_DOUBLE(negInfValue));
        for (i = 0; i < 256; i++) {
            if (float64Table[i])
                JS2Object::mark(float64Table[i]);
        }
        if (parameterSlots) {
            for (i = 0; i < parameterCount; i++) {
                GCMARKVALUE(parameterSlots[i]);
            }
        }
        GCMARKVALUE(retval);
    }

    void JS2Engine::pushHandler(uint8 *pc)
    { 
        ActivationFrame *curAct = (activationStackEmpty()) ? NULL : (activationStackTop - 1);
        mTryStack.push(new HandlerData(pc, sp - execStack, curAct, meta->env->getTopFrame())); 
    }

    void JS2Engine::popHandler()
    {
        HandlerData *hndlr = mTryStack.top();
        mTryStack.pop();
        delete hndlr;
    }

    js2val JS2Engine::typeofString(js2val a)
    {
        if (JS2VAL_IS_UNDEFINED(a))
            a = allocString(undefined_StringAtom);
        else
        if (JS2VAL_IS_BOOLEAN(a))
            a = allocString("boolean");
        else
        if (JS2VAL_IS_NUMBER(a))
            a = allocString("number");
        else
        if (JS2VAL_IS_STRING(a))
            a = allocString("string");
        else {
            ASSERT(JS2VAL_IS_OBJECT(a));
            if (JS2VAL_IS_NULL(a))
                a = allocString(object_StringAtom);
            else {
                JS2Object *obj = JS2VAL_TO_OBJECT(a);
                switch (obj->kind) {
                case MultinameKind:
                    a = allocString("namespace"); 
                    break;
                case AttributeObjectKind:
                    a = allocString("attribute"); 
                    break;
                case ClassKind:
                    // typeof returns lower-case 'function', whereas the [[class]] value
                    // has upper-case 'Function'
                    a = allocString("function"); //a = STRING_TO_JS2VAL(Function_StringAtom); 
                    break;
                case SimpleInstanceKind:
                    if (checked_cast<SimpleInstance *>(obj)->type == meta->functionClass)
                        a = allocString("function"); //STRING_TO_JS2VAL(Function_StringAtom);
                    else
                        a = allocString(object_StringAtom);
//                    a = STRING_TO_JS2VAL(checked_cast<SimpleInstance *>(obj)->type->getName());
                    break;
                case PackageKind:
                    a = allocString(object_StringAtom);
                    break;
                default:
                    ASSERT(false);
                    break;
                }
            }
        }
        return a;
    }

    //
    // XXX Only scanning dynamic properties
    //
    // Initialize and build a list of names of properties in the object.
    //
    bool ForIteratorObject::first(JS2Engine *engine)
    {
        if (obj == NULL)
            return false;
        originalObj = obj;
        return buildNameList(engine->meta);
    }

    // Iterate over LocalBindings
    bool ForIteratorObject::buildNameList(JS2Metadata *meta)
    {
        LocalBindingMap *lMap = NULL;
        if (obj->kind == SimpleInstanceKind)
            lMap = &(checked_cast<SimpleInstance *>(obj))->localBindings;
        else
        if (obj->kind == PackageKind)
            lMap = &(checked_cast<Package *>(obj))->localBindings;
        else
        if (obj->kind == ClassKind)
            lMap = &(checked_cast<JS2Class *>(obj))->localBindings;

        if (lMap) {
            nameList = new const String *[lMap->size()];
            length = 0;
            for (LocalBindingIterator bi(*lMap); bi; ++bi) {
                LocalBindingEntry &lbe = *bi;
                for (LocalBindingEntry::NS_Iterator i = lbe.begin(), end = lbe.end(); (i != end); i++) {
                    LocalBindingEntry::NamespaceBinding ns = *i;
                    if ((ns.first == meta->publicNamespace) && ns.second->enumerable)
                        nameList[length++] = &lbe.name;
                }
            }
            if (length == 0) {
                if (obj->kind == SimpleInstanceKind) {
                    js2val protoval = (checked_cast<SimpleInstance *>(obj))->super;
                    if (!JS2VAL_IS_NULL(protoval)) {
                        if (JS2VAL_IS_OBJECT(protoval)) {
                            obj = JS2VAL_TO_OBJECT(protoval);
                            return buildNameList(meta);
                        }
                    }
                }
            }
            it = 0;
            return (length != 0);
        }
        return false;
    }

    //
    // Set the iterator to the next property in that list that is not
    // shadowed by a property higher up the prototype chain. If we get 
    // to the end of the list, bump down to the next object on the chain.
    //
    bool ForIteratorObject::next(JS2Engine *engine)
    {
        if (nameList) {
            it++;
            if (obj->kind == ClassKind) {
                return (it != length);
            }
            else {
/*
                if (originalObj != obj) {
                    while (it != length)
                        if (engine->meta->lookupDynamicProperty(originalObj, nameList[it]) != obj)
                            // shadowed by a property higher in the chain, so skip to next
                            it++;
                        else
                            break;
                }
*/
                if (it == length) {
                    if (obj->kind == SimpleInstanceKind) {
                        js2val protoval = (checked_cast<SimpleInstance *>(obj))->super;
                        if (!JS2VAL_IS_NULL(protoval)) {
                            if (JS2VAL_IS_OBJECT(protoval)) {
                                obj = JS2VAL_TO_OBJECT(protoval);
                                return buildNameList(engine->meta);
                            }
                        }
                    }
                    return false;
                }
                else
                    return true;
            }
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

