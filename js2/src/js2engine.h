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

#ifndef js2engine_h___
#define js2engine_h___


#ifdef _WIN32
#include "msvc_pragma.h"
#endif

namespace JavaScript {
namespace MetaData {




enum JS2Op {
    eAdd,
    eSubtract,
    eMultiply,
    eDivide,
    eModulo,
    eEqual,
    eNotEqual,
    eLess,
    eGreater,
    eLessEqual,
    eGreaterEqual,
    eIdentical,
    eNotIdentical,
    eLogicalXor,
    eLogicalNot,
    eMinus,
    ePlus,
    eComplement,
    eLeftShift,
    eRightShift,
    eLogicalRightShift,
    eBitwiseAnd,
    eBitwiseXor,
    eBitwiseOr,
    eTrue,
    eFalse,
    eNull,
    eUndefined,
    eLongZero,
    eNumber,
    eInteger,
    eRegExp,
    eFunction,          // <object index:u16>
    eClosure,           // <object index:u16>
    eUInt64,
    eInt64,
    eString,            // <string pointer:u32>
    eThis,
    eSuper,
    eSuperExpr,
    eNewObject,         // <argCount:u16>
    eNewArray,          // <argCount:u16>

    eThrow,
    eTry,               // <finally displacement:s32> <catch displacement:s32>
    eCallFinally,       // <branch displacement:s32>
    eReturnFinally,
    eHandler,

    eCoerce,            // <type pointer:u32>

    eFirst,
    eNext,
    eForValue,

    eFrameSlotRead,     // <slot index:u16>
    eFrameSlotRef,      // <slot index:u16>
    eFrameSlotWrite,    // <slot index:u16>
    eFrameSlotDelete,   // <slot index:u16>
    eFrameSlotPostInc,  // <slot index:u16>
    eFrameSlotPostDec,  // <slot index:u16>
    eFrameSlotPreInc,   // <slot index:u16>
    eFrameSlotPreDec,   // <slot index:u16>

    eParameterSlotRead,     // <slot index:u16>
    eParameterSlotRef,      // <slot index:u16>
    eParameterSlotWrite,    // <slot index:u16>
    eParameterSlotDelete,   // <slot index:u16>
    eParameterSlotPostInc,  // <slot index:u16>
    eParameterSlotPostDec,  // <slot index:u16>
    eParameterSlotPreInc,   // <slot index:u16>
    eParameterSlotPreDec,   // <slot index:u16>

    ePackageSlotRead,     // <slot index:u16>
    ePackageSlotRef,      // <slot index:u16>
    ePackageSlotWrite,    // <slot index:u16>
    ePackageSlotDelete,   // <slot index:u16>
    ePackageSlotPostInc,  // <slot index:u16>
    ePackageSlotPostDec,  // <slot index:u16>
    ePackageSlotPreInc,   // <slot index:u16>
    ePackageSlotPreDec,   // <slot index:u16>

    eLexicalRead,       // <multiname index:u16>
    eLexicalWrite,      // <multiname index:u16>
    eLexicalInit,       // <multiname index:u16>
    eLexicalRef,        // <multiname index:u16>
    eLexicalDelete,     // <multiname index:u16>
    eDotRead,           // <multiname index:u16>
    eDotWrite,          // <multiname index:u16>
    eDotRef,            // <multiname index:u16>
    eDotDelete,         // <multiname index:u16>
    eBracketRead,
    eBracketWrite,
    eBracketRef,
    eBracketReadForRef,
    eBracketWriteRef,
    eBracketDelete,
    eSlotRead,           // <slot index:u16>
    eSlotWrite,          // <slot index:u16>
    eSlotRef,            // <slot index:u16>
    eSlotDelete,         // <slot index:u16>

    eReturn,
    eReturnVoid,
    ePushFrame,         // <frame index:u16>
    ePopFrame,
    eWithin,
    eWithout,
    eBranchFalse,       // <branch displacement:s32> XXX save space with short and long versions instead ?
    eBranchTrue,        // <branch displacement:s32>
    eBranch,            // <branch displacement:s32>
    eBreak,             // <branch displacement:s32> <blockCount:u16>
    eNew,               // <argCount:u16>
    eCall,              // <argCount:u16>
    eSuperCall,         // <argCount:u16>
    eTypeof,
    eInstanceof,
    eIs,
    eIn,

    ePopv,
    ePop,
    eDup,
    eSwap,
    eSwap2,
    eVoid,

    eLexicalPostInc,    // <multiname index:u16>
    eLexicalPostDec,    // <multiname index:u16>
    eLexicalPreInc,     // <multiname index:u16>
    eLexicalPreDec,     // <multiname index:u16>
    eDotPostInc,        // <multiname index:u16>
    eDotPostDec,        // <multiname index:u16>
    eDotPreInc,         // <multiname index:u16>
    eDotPreDec,         // <multiname index:u16>
    eBracketPostInc,
    eBracketPostDec,
    eBracketPreInc,
    eBracketPreDec,
    eSlotPostInc,       // <slot index:u16>
    eSlotPostDec,       // <slot index:u16>
    eSlotPreInc,        // <slot index:u16>
    eSlotPreDec,        // <slot index:u16>


    eNop

};


class Frame;
class NonWithFrame;
class ParameterFrame;
class Environment;

class JS2Object;
class JS2Metadata;
class BytecodeContainer;
class JS2Class;

int getStackEffect(JS2Op op);
void dumpBytecode(BytecodeContainer *bCon);

class JS2Engine {
public:

    JS2Engine(JS2Metadata *meta, World &world);
    ~JS2Engine();

    js2val interpret(Phase execPhase, BytecodeContainer *targetbCon, Environment *env);
    js2val interpreterLoop();

    // Use the pc map in the current bytecode container to get a source offset
    size_t errorPos();

    static int32 float64toInt32(float64 f);
    static int64 float64toInt64(float64 f);
    static uint32 float64toUInt32(float64 f);
    static uint16 float64toUInt16(float64 f);
    static float64 truncateFloat64(float64 d);
    

    int64 checkInteger(js2val x);

    JS2Metadata *meta;                  // engine needs access to 'meta' for the string 'world'
                                        // the environment, error reporter and forIterator object

    // Current engine execution state
    uint8 *pc;
    BytecodeContainer *bCon;
    Phase phase;

    // Handy f.p. values
    js2val nanValue;
    js2val posInfValue;
    js2val negInfValue;

    js2val allocString(const String *s)                                 { return STRING_TO_JS2VAL(allocStringPtr(s)); }
    js2val allocString(const String &s)                                 { return allocString(&s); }
    js2val allocString(const char *s)                                   { return STRING_TO_JS2VAL(allocStringPtr(s)); }
    js2val allocString(const char16 *s, uint32 length)                  { return STRING_TO_JS2VAL(allocStringPtr(s, length)); }
    js2val allocString(const String *s, uint32 index, uint32 length)    { return STRING_TO_JS2VAL(allocStringPtr(s, index, length)); }
    String *allocStringPtr(const String *s);
    String *allocStringPtr(const char *s);
    String *allocStringPtr(const char16 *s, uint32 length);
    String *allocStringPtr(const String *s, uint32 index, uint32 length);
    String *concatStrings(const String *s1, const String *s2);

    String *numberToString(float64 *number);    // non-static since they need access to meta
    String *numberToString(int32 i);
    String *numberToString(uint32 i);

    StringAtom &numberToStringAtom(float64 *number);
    StringAtom &numberToStringAtom(int32 i);
    StringAtom &numberToStringAtom(uint32 i);

    js2val allocFloat(float32 x); 
    js2val pushFloat(float32 x)         { js2val retval = allocFloat(x); push(retval); return retval; }

    js2val allocNumber(float64 x); 
    js2val pushNumber(float64 x)        { js2val retval = allocNumber(x); push(retval); return retval; }

    js2val allocULong(uint64 x); 
    js2val pushULong(uint64 x)          { js2val retval = allocULong(x); push(retval); return retval; }

    js2val allocLong(int64 x); 
    js2val pushLong(int64 x)            { js2val retval = allocLong(x); push(retval); return retval; }

private:
    // A cache of f.p. values (XXX experimentally trying to reduce # of double pointers XXX)
    float64 *float64Table[256];
    float64 *newDoubleValue(float64 x);

    js2val retval;
public:


    // Cached StringAtoms for handy access
    // These are all engine-allocated, so can be used as JS2VAL's
    const StringAtom &true_StringAtom;
    const StringAtom &false_StringAtom;
    const StringAtom &null_StringAtom;
    const StringAtom &undefined_StringAtom;
    const StringAtom &public_StringAtom;
    const StringAtom &private_StringAtom;
    const StringAtom &Function_StringAtom;
    const StringAtom &Object_StringAtom;
    const StringAtom &object_StringAtom;
    const StringAtom &Empty_StringAtom;
    const StringAtom &Dollar_StringAtom;
    const StringAtom &prototype_StringAtom;
    const StringAtom &length_StringAtom;
    const StringAtom &toString_StringAtom;
    const StringAtom &valueOf_StringAtom;

    // The activation stack, when it's empty and a return is executed, the
    // interpreter quits
#define MAX_ACTIVATION_STACK (20)
    struct ActivationFrame {
        uint8 *pc;
        BytecodeContainer *bCon;
        Phase phase;
        js2val retval;
        uint32 execStackBase;
        Environment *env;
        Environment *newEnv;
        Frame *topFrame;
        NonWithFrame *localFrame;
        ParameterFrame *parameterFrame;
        js2val *parameterSlots;
        uint32 parameterCount;
		bool superConstructorCalled;
    };
    void jsr(Phase execPhase, BytecodeContainer *bCon, uint32 stackBase, js2val returnVal, Environment *env);
    bool activationStackEmpty() { return (activationStackTop == activationStack); }
    void rts();
    ActivationFrame *activationStack;
    ActivationFrame *activationStackTop;
    
    js2val typeofString(js2val a);
    
    // The execution stack for expression evaluation, should be empty
    // between statements.
#define INITIAL_EXEC_STACK (80)

    js2val *execStackLimit;
    js2val *execStack;
    js2val *sp;
    
    void push(js2val x)         { ASSERT(sp < execStackLimit); *sp++ = x; }
    js2val pop()                { ASSERT(sp > execStack); return *--sp; }

    // return the value at the stack top
    js2val top()                { return *(sp - 1); }

    // return the value 'n' items down the stack
    js2val top(int n)           { return *(sp - n); }

    // return the address of the value 'n' items down the stack
    js2val *base(int n)         { return (sp - n); }
    
    // pop 'n' things off the stack
    void pop(int n)             { sp -= n; ASSERT(sp >= execStack); }

    // insert 'x' before the top 'count' stack items
    void insert(js2val x, int count);

    struct HandlerData {
        HandlerData(uint8 *pc, uint32 stackTop, ActivationFrame *curAct, Frame *frame) 
            : mPC(pc), mStackTop(stackTop), mActivation(curAct), mFrame(frame) { }

        uint8 *mPC;
        uint32 mStackTop;
        ActivationFrame *mActivation;
        Frame *mFrame;
    };

    std::stack<HandlerData *> mTryStack;
    std::stack<uint8 *> finallyStack;

    // For frame slot references:
    NonWithFrame *packageFrame;
    NonWithFrame *localFrame;

    ParameterFrame *parameterFrame;
    js2val *parameterSlots;             // just local copies of paramterFrame->argSlots
    uint32 parameterCount;              // ... and parameterFrame->argCount
    bool superConstructorCalled;

    void pushHandler(uint8 *pc);
    void popHandler();

    void mark();

    bool traceInstructions;     // emit trace of each instruction executed

    static js2val defaultConstructor(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc);


};

#ifdef TRACE_DEBUG
uint8 *printInstruction(uint8 *pc, uint8 *start, BytecodeContainer *bCon, JS2Engine *engine);
#endif

}
}



#endif

