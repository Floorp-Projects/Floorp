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
    eRegExp,
    eUInt64,
    eInt64,
    eString,            // <string pointer:u32>
    eThis,
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
    eFrameSlotWrite,    // <slot index:u16>

    eLexicalRead,       // <multiname index:u16>
    eLexicalWrite,      // <multiname index:u16>
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
    eBranchFalse,       // <branch displacement:s32> XXX save space with short and long versions instead ?
    eBranchTrue,        // <branch displacement:s32>
    eBranch,            // <branch displacement:s32>
    eNew,               // <argCount:u16>
    eCall,              // <argCount:u16>
    eTypeof,
    eIs,

    ePopv,
    ePop,
    eDup,
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

};


class Frame;
class JS2Object;
class JS2Metadata;
class BytecodeContainer;
class JS2Class;

int getStackEffect(JS2Op op);
void dumpBytecode(BytecodeContainer *bCon);

class JS2Engine {
public:

    JS2Engine(World &world);

    js2val interpret(Phase execPhase, BytecodeContainer *targetbCon);
    js2val interpreterLoop();

    // Use the pc map in the current bytecode container to get a source offset
    size_t errorPos();

    static int32 float64toInt32(float64 f);
    static uint32 float64toUInt32(float64 f);
    static uint16 float64toUInt16(float64 f);


    int64 checkInteger(js2val x);

    JS2Metadata *meta;

    // Current engine execution state
    uint8 *pc;
    BytecodeContainer *bCon;
    Phase phase;

    // Handy f.p. values
    js2val nanValue;
    js2val posInfValue;
    js2val negInfValue;

    js2val allocString(const String *s)       { return STRING_TO_JS2VAL(allocStringPtr(s)); }
    js2val allocString(const String &s)       { return allocString(&s); }
    js2val allocString(const char *s)         { return STRING_TO_JS2VAL(allocStringPtr(s)); }
    String *allocStringPtr(const String *s);
    String *allocStringPtr(const char *s);

    String *numberToString(float64 *number);
    String *numberToString(int32 i);

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

    js2val a,b;
    js2val baseVal,indexVal;


public:


    // Cached StringAtoms for handy access
    // These are all engine-allocated, so can be used as JS2VAL's
    const String *true_StringAtom;
    const String *false_StringAtom;
    const String *null_StringAtom;
    const String *undefined_StringAtom;
    const String *public_StringAtom;
    const String *private_StringAtom;
    const String *Function_StringAtom;
    const String *object_StringAtom;
    const String *Empty_StringAtom;
    const String *Dollar_StringAtom;
    const String *prototype_StringAtom;
    const String *length_StringAtom;
    const String *toString_StringAtom;
    const String *valueOf_StringAtom;

    // The activation stack, when it's empty and a return is executed, the
    // interpreter quits
#define MAX_ACTIVATION_STACK (20)
    struct ActivationFrame {
        uint8 *pc;
        BytecodeContainer *bCon;
        Frame *topFrame;
        Phase phase;
        js2val retval;
        js2val *execStackBase;
    };
    void jsr(Phase execPhase, BytecodeContainer *bCon, js2val *stackBase, js2val returnVal);
    bool activationStackEmpty() { return (activationStackTop == activationStack); }
    void rts();
    ActivationFrame *activationStack;
    ActivationFrame *activationStackTop;
    
    
    // The execution stack for expression evaluation, should be empty
    // between statements.
#define INITIAL_EXEC_STACK (40)

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
        HandlerData(uint8 *pc, js2val *stackTop, ActivationFrame *curAct) 
            : mPC(pc), mStackTop(stackTop), mActivation(curAct) { }

        uint8 *mPC;
        js2val *mStackTop;
        ActivationFrame *mActivation;
    };

    std::stack<HandlerData *> mTryStack;
    std::stack<uint8 *> finallyStack;

    void pushHandler(uint8 *pc);
    void popHandler();

    void mark();

    bool traceInstructions;     // emit trace of each instruction executed

    static js2val defaultConstructor(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc);


};

#ifdef DEBUG
uint8 *printInstruction(uint8 *pc, uint8 *start, BytecodeContainer *bCon, JS2Engine *engine);
#endif

}
}



#endif

