/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
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

#include "interpreter.h"
#include "world.h"

#include <assert.h>

namespace JavaScript {
namespace Interpreter {

// operand access macros.
#define op1(i) (i->o1())
#define op2(i) (i->o2())
#define op3(i) (i->o3())
#define op4(i) (i->o4())

// mnemonic names for operands.
#define dst(i)  op1(i)
#define src1(i) op2(i)
#define src2(i) op3(i)
#define ofs(i)  (i->getOffset())
#define val3(i)  op3(i)
#define val4(i)  op4(i)

using namespace ICG;
using namespace JSTypes;
    
// These classes are private to the JS interpreter.

/**
* 
*/
struct Handler: public gc_base {
    Handler(Label *catchLabel, Label *finallyLabel)
        : catchTarget(catchLabel), finallyTarget(finallyLabel) {}
    Label *catchTarget;
    Label *finallyTarget;
};
typedef std::vector<Handler *> CatchStack;


/**
 * Represents the current function's invocation state.
 */
struct Activation : public gc_base {
    JSValues mRegisters;
    ICodeModule* mICode;
    CatchStack catchStack;
        
    Activation(ICodeModule* iCode, const JSValues& args)
        : mRegisters(iCode->itsMaxRegister + 1), mICode(iCode)
    {
        // copy arg list to initial registers.
        JSValues::iterator dest = mRegisters.begin();
        for (JSValues::const_iterator src = args.begin(), 
                 end = args.end(); src != end; ++src, ++dest) {
            *dest = *src;
        }
    }

    Activation(ICodeModule* iCode, Activation* caller, 
                 const RegisterList& list)
        : mRegisters(iCode->itsMaxRegister + 1), mICode(iCode)
    {
        // copy caller's parameter list to initial registers.
        JSValues::iterator dest = mRegisters.begin();
        const JSValues& params = caller->mRegisters;
        for (RegisterList::const_iterator src = list.begin(), 
                 end = list.end(); src != end; ++src, ++dest) {
            *dest = params[*src];
        }
    }

    Activation(ICodeModule* iCode, const JSValue arg)
        : mRegisters(iCode->itsMaxRegister + 1), mICode(iCode)
    {
        mRegisters[0] = arg;
    }

    Activation(ICodeModule* iCode, const JSValue arg1, const JSValue arg2)
        : mRegisters(iCode->itsMaxRegister + 1), mICode(iCode)
    {
        mRegisters[0] = arg1;
        mRegisters[1] = arg2;
    }

};

JSValues& Context::getRegisters()    { return mActivation->mRegisters; }
ICodeModule* Context::getICode()     { return mActivation->mICode; }

/**
 * Stores saved state from the *previous* activation, the current
 * activation is alive and well in locals of the interpreter loop.
 */
struct Linkage : public Context::Frame, public gc_base {
    Linkage*            mNext;              // next linkage in linkage stack.
    InstructionIterator mReturnPC;
    Activation*         mActivation;        // caller's activation.
    Register            mResult;            // the desired target register for the return value

    Linkage(Linkage* linkage, InstructionIterator returnPC,
            Activation* activation, Register result) 
        :   mNext(linkage), mReturnPC(returnPC),
            mActivation(activation), mResult(result)
    {
    }
    
Context::Frame* getNext() { return mNext; }
    
void getState(InstructionIterator& pc, JSValues*& registers, ICodeModule*& iCode)
    {
        pc = mReturnPC;
        registers = &mActivation->mRegisters;
        iCode = mActivation->mICode;
    }
};
/*
void Context::doCall(JSFunction *target, Instruction *pc)
{
    if (target->isNative()) {
        RegisterList &params = op3(call);
        JSValues argv(params.size());
        JSValues::size_type i = 0;
        for (RegisterList::const_iterator src = params.begin(), end = params.end();
                        src != end; ++src, ++i) {
            argv[i] = (*registers)[*src];
        }
        if (op2(call) != NotARegister)
            (*registers)[op2(call)] = static_cast<JSNativeFunction*>(target)->mCode(argv);
        return pc;
    }
    else {
        mLinkage = new Linkage(mLinkage, ++mPC,
                               mActivation, op1(call));
        iCode = target->getICode();
        mActivation = new Activation(iCode, mActivation, op3(call));
        registers = &mActivation->mRegisters;
        continue;
    }
}
*/

static JSValue shiftLeft_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toUInt32());
    return JSValue(num1.i32 << (num2.u32 & 0x1F));
}
static JSValue shiftRight_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toUInt32());
    return JSValue(num1.i32 >> (num2.u32 & 0x1F));
}
static JSValue UshiftRight_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toUInt32());
    JSValue num2(r2.toUInt32());
    return JSValue(num1.u32 >> (num2.u32 & 0x1F));
}
static JSValue and_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toInt32());
    return JSValue(num1.i32 & num2.i32);
}
static JSValue or_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toInt32());
    return JSValue(num1.i32 | num2.i32);
}
static JSValue xor_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toInt32());
    return JSValue(num1.i32 ^ num2.i32);
}
static JSValue add_Default(const JSValue& r1, const JSValue& r2)
{
    //
    // could also handle these as separate entries in the override table for add
    // by specifying add(String, Any), add(Any, String), add(String, String)
    //
    if (r1.isString() || r2.isString()) {
        JSValue r = r1.toString();
        JSString& str1 = *r.string;
        JSString& str2 = *r2.toString().string;
        str1 += str2;
        return r;
    }
    else {
        JSValue num1(r1.toNumber());
        JSValue num2(r2.toNumber());
        return JSValue(num1.f64 + num2.f64);
    }
}
static JSValue add_String1(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(num1.f64 + num2.f64);
}
static JSValue subtract_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(num1.f64 - num2.f64);
}
static JSValue multiply_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(num1.f64 * num2.f64);
}
static JSValue divide_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(num1.f64 / num2.f64);
}
static JSValue remainder_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(fmod(num1.f64, num2.f64));
}
static JSValue less_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue lv = r1.toPrimitive(JSValue::Number);
    JSValue rv = r2.toPrimitive(JSValue::Number);
    if (lv.isString() && rv.isString()) {
        // XXX FIXME urgh, call w_strcmp ??? on a JSString ???
        return JSValue();
    }
    else {
        lv = lv.toNumber();
        rv = rv.toNumber();
        if (lv.isNaN() || rv.isNaN())
            return JSValue();
        else
            return JSValue(lv.f64 < rv.f64);
    }
}
static JSValue lessEqual_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue lv = r1.toPrimitive(JSValue::Number);
    JSValue rv = r2.toPrimitive(JSValue::Number);
    if (lv.isString() && rv.isString()) {
        // XXX FIXME urgh, call w_strcmp ??? on a JSString ???
        return JSValue();
    }
    else {
        lv = lv.toNumber();
        rv = rv.toNumber();
        if (lv.isNaN() || rv.isNaN())
            return JSValue();
        else
            return JSValue(lv.f64 <= rv.f64);
    }
}
static JSValue equal_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue lv = r1.toPrimitive(JSValue::Number);
    JSValue rv = r2.toPrimitive(JSValue::Number);
    if (lv.isString() && rv.isString()) {
        // XXX FIXME urgh, call w_strcmp ??? on a JSString ???
        return JSValue();
    }
    else {
        lv = lv.toNumber();
        rv = rv.toNumber();
        if (lv.isNaN() || rv.isNaN())
            return JSValue();
        else
            return JSValue(lv.f64 == rv.f64);
    }
}
static JSValue identical_Default(const JSValue& r1, const JSValue& r2)
{
    if (r1.getType() != r2.getType())
        return kFalse;
    if (r1.isUndefined() )
        return kTrue;
    if (r1.isNull())
        return kTrue;
    if (r1.isNumber()) {
        if (r1.isNaN())
            return kFalse;
        if (r2.isNaN())
            return kFalse;
        return JSValue(r1.f64 == r2.f64);
    }
    else {
        if (r1.isString())
            return kFalse;     // XXX implement me!! w_strcmp??
        if (r1.isBoolean())
            return JSValue(r1.boolean == r2.boolean);
        if (r1.isObject())
            return JSValue(r1.object == r2.object);
        return kFalse;
    }
}


class BinaryOperator {
public:

    // Wah, here's a third enumeration of opcodes - ExprNode, ICodeOp and now here, this can't be right??
    typedef enum { 
        Add, Subtract, Multiply, Divide,
        Remainder, LeftShift, RightShift, LogicalRightShift,
        BitwiseOr, BitwiseXor, BitwiseAnd, Less, LessEqual,
        Equal, Identical
    } BinaryOp;

    BinaryOperator(const JSType *t1, const JSType *t2, JSBinaryOperator *function) :
        t1(t1), t2(t2), function(function) { }

    static BinaryOp mapICodeOp(ICodeOp op);

    const JSType *t1;
    const JSType *t2;
    JSBinaryOperator *function;

};

BinaryOperator::BinaryOp BinaryOperator::mapICodeOp(ICodeOp op) {
    // a table later... or maybe we need a grand opcode re-unification 
    switch (op) {
    case ADD        : return Add;
    case SUBTRACT   : return Subtract;
    case MULTIPLY   : return Multiply;
    case DIVIDE     : return Divide;
    case REMAINDER  : return Remainder;
    case SHIFTLEFT  : return LeftShift;
    case SHIFTRIGHT : return RightShift;
    case USHIFTRIGHT: return LogicalRightShift;

    case AND        : return BitwiseAnd;
    case OR         : return BitwiseOr;
    case XOR        : return BitwiseXor;
    
    case COMPARE_LT : return Less;
    case COMPARE_LE : return LessEqual;
    case COMPARE_EQ : return Equal;
    case STRICT_EQ  : return Identical;
    default :
        NOT_REACHED("Unsupported binary op");
        return (BinaryOp)-1;
    }
}



typedef std::vector<BinaryOperator *> BinaryOperatorList;
BinaryOperatorList binaryOperators[15];

        

class InitBinaryOperators {
public:
    InitBinaryOperators() {
        binaryOperators[BinaryOperator::Add].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(add_Default)));
        binaryOperators[BinaryOperator::Subtract].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(subtract_Default)));
        binaryOperators[BinaryOperator::Multiply].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(multiply_Default)));
        binaryOperators[BinaryOperator::Divide].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(divide_Default)));
        binaryOperators[BinaryOperator::Remainder].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(remainder_Default)));
        binaryOperators[BinaryOperator::LeftShift].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(shiftLeft_Default)));
        binaryOperators[BinaryOperator::RightShift].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(shiftRight_Default)));
        binaryOperators[BinaryOperator::LogicalRightShift].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(UshiftRight_Default)));
        binaryOperators[BinaryOperator::BitwiseOr].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(or_Default)));
        binaryOperators[BinaryOperator::BitwiseXor].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(xor_Default)));
        binaryOperators[BinaryOperator::BitwiseAnd].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(and_Default)));
        binaryOperators[BinaryOperator::Less].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(less_Default)));
        binaryOperators[BinaryOperator::LessEqual].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(lessEqual_Default)));
        binaryOperators[BinaryOperator::Equal].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(equal_Default)));
        binaryOperators[BinaryOperator::Identical].push_back(new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(identical_Default)));
    }
} initializer = InitBinaryOperators();


static const JSValue findBinaryOverride(JSValue &operand1, JSValue &operand2, BinaryOperator::BinaryOp op)
{
    int32 bestDist1 = JSType::NoRelation;
    int32 bestDist2 = JSType::NoRelation;
    BinaryOperatorList::iterator candidate = NULL;

    for (BinaryOperatorList::iterator i = binaryOperators[op].begin();
            i != binaryOperators[op].end(); i++) 
    {
        int32 dist1 = operand1.getType()->distance((*i)->t1);
        int32 dist2 = operand2.getType()->distance((*i)->t2);

        if ((dist1 < bestDist1) && (dist2 < bestDist2)) {
            bestDist1 = dist1;
            bestDist2 = dist2;
            candidate = i;
        }
    }
    ASSERT(candidate);
    return JSValue((*candidate)->function);
}

JSValue Context::interpret(ICodeModule* iCode, const JSValues& args)
{
    assert(mActivation == 0); /* recursion == bad */
    
    JSValue rv;
    
    mActivation = new Activation(iCode, args);
    JSValues* registers = &mActivation->mRegisters;

    mPC = mActivation->mICode->its_iCode->begin();

    // stack of all catch/finally handlers available for the current activation
    // to implement jsr/rts for finally code
    std::stack<InstructionIterator> subroutineStack;

    while (true) {
        try {
            // tell any listeners about the current execution state.
            // XXX should only do this if we're single stepping/tracing.
            if (mListeners.size())
                broadcast(EV_STEP);

            Instruction* instruction = *mPC;
            switch (instruction->op()) {
            case CALL:
                {
                    Call* call = static_cast<Call*>(instruction);
                    JSFunction *target = (*registers)[op2(call)].function;
                    if (target->isNative()) {
                        RegisterList &params = op3(call);
                        JSValues argv(params.size());
                        JSValues::size_type i = 0;
                        for (RegisterList::const_iterator src = params.begin(), end = params.end();
                                        src != end; ++src, ++i) {
                            argv[i] = (*registers)[*src];
                        }
                        if (op2(call) != NotARegister)
                            (*registers)[op2(call)] = static_cast<JSNativeFunction*>(target)->mCode(argv);
                        break;
                    }
                    else {
                        mLinkage = new Linkage(mLinkage, ++mPC,
                                               mActivation, op1(call));
                        mActivation = new Activation(target->getICode(), mActivation, op3(call));
                        registers = &mActivation->mRegisters;
                        mPC = mActivation->mICode->its_iCode->begin();
                        continue;
                    }
                }

            case RETURN_VOID:
                {
                    JSValue result;
                    Linkage *linkage = mLinkage;
                    if (!linkage)
                    {
                        // let the garbage collector free activations.
                        mActivation = 0;
                        return result;
                    }
                    mLinkage = linkage->mNext;
                    mActivation = linkage->mActivation;
                    registers = &mActivation->mRegisters;
                    (*registers)[linkage->mResult] = result;
                    mPC = linkage->mReturnPC;
                }
                continue;

            case RETURN:
                {
                    Return* ret = static_cast<Return*>(instruction);
                    JSValue result;
                    if (op1(ret) != NotARegister) 
                        result = (*registers)[op1(ret)];
                    Linkage* linkage = mLinkage;
                    if (!linkage)
                    {
                        // let the garbage collector free activations.
                        mActivation = 0;
                        return result;
                    }
                    mLinkage = linkage->mNext;
                    mActivation = linkage->mActivation;
                    registers = &mActivation->mRegisters;
                    (*registers)[linkage->mResult] = result;
                    mPC = linkage->mReturnPC;
                }
                continue;
            case MOVE:
                {
                    Move* mov = static_cast<Move*>(instruction);
                    (*registers)[dst(mov)] = (*registers)[src1(mov)];
                }
                break;
            case LOAD_NAME:
                {
                    LoadName* ln = static_cast<LoadName*>(instruction);
                    (*registers)[dst(ln)] = mGlobal->getVariable(*src1(ln));
                }
                break;
            case SAVE_NAME:
                {
                    SaveName* sn = static_cast<SaveName*>(instruction);
                    mGlobal->setVariable(*dst(sn), (*registers)[src1(sn)]);
                }
                break;
            case NEW_OBJECT:
                {
                    NewObject* no = static_cast<NewObject*>(instruction);
                    (*registers)[dst(no)] = JSValue(new JSObject());
                }
                break;
            case NEW_ARRAY:
                {
                    NewArray* na = static_cast<NewArray*>(instruction);
                    (*registers)[dst(na)] = JSValue(new JSArray());
                }
                break;
            case GET_PROP:
                {
                    GetProp* gp = static_cast<GetProp*>(instruction);
                    JSValue& value = (*registers)[src1(gp)];
                    if (value.tag == JSValue::object_tag) {
                        JSObject* object = value.object;
                        (*registers)[dst(gp)] = object->getProperty(*src2(gp));
                    }
                }
                break;
            case SET_PROP:
                {
                    SetProp* sp = static_cast<SetProp*>(instruction);
                    JSValue& value = (*registers)[dst(sp)];
                    if (value.tag == JSValue::object_tag) {
                        JSObject* object = value.object;
                        object->setProperty(*src1(sp), (*registers)[src2(sp)]);
                    }
                }
                break;
/*
In 1.5, there is no 'array' type really, the index operation
turns into a get_property call like this, only we need to be
using JSString throughout.
            case GET_ELEMENT:
                {
                    GetElement* ge = static_cast<GetElement*>(instruction);
                    JSValue& base = (*registers)[src1(ge)];
                    JSValue index = (*registers)[src2(ge)].toString();
                    if (base.tag == JSValue::object_tag) {
                        JSObject* object = base.object;
                        (*registers)[dst(ge)] = object->getProperty(*index.string);
                    }
                }
                break;
*/
            case GET_ELEMENT:
                {
                    GetElement* ge = static_cast<GetElement*>(instruction);
                    JSValue& value = (*registers)[src1(ge)];
                    if (value.tag == JSValue::array_tag) {
                        JSArray* array = value.array;
                        (*registers)[dst(ge)] = (*array)[(*registers)[src2(ge)]];
                    }
                }
                break;
            case SET_ELEMENT:
                {
                    SetElement* se = static_cast<SetElement*>(instruction);
                    JSValue& value = (*registers)[dst(se)];
                    if (value.tag == JSValue::array_tag) {
                        JSArray* array = value.array;
                        (*array)[(*registers)[src1(se)]] = (*registers)[src2(se)];
                    }
                }
                break;

            case LOAD_IMMEDIATE:
                {
                    LoadImmediate* li = static_cast<LoadImmediate*>(instruction);
                    (*registers)[dst(li)] = JSValue(src1(li));
                }
                break;
            case LOAD_STRING:
                {
                    LoadString* ls = static_cast<LoadString*>(instruction);
                    (*registers)[dst(ls)] = JSValue(src1(ls));
                }
                break;
            case LOAD_VALUE:
                {
                    LoadValue* lv = static_cast<LoadValue*>(instruction);
                    (*registers)[dst(lv)] = src1(lv);
                }
                break;
            case BRANCH:
                {
                    GenericBranch* bra =
                        static_cast<GenericBranch*>(instruction);
                    mPC = mActivation->mICode->its_iCode->begin() + ofs(bra);
                    continue;
                }
                break;
            case BRANCH_TRUE:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    ASSERT((*registers)[src1(bc)].isBoolean());
                    if ((*registers)[src1(bc)].boolean) {
                        mPC = mActivation->mICode->its_iCode->begin() + ofs(bc);
                        continue;
                    }
                }
                break;
            case BRANCH_FALSE:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    ASSERT((*registers)[src1(bc)].isBoolean());
                    if (!(*registers)[src1(bc)].boolean) {
                        mPC = mActivation->mICode->its_iCode->begin() + ofs(bc);
                        continue;
                    }
                }
                break;
            case SHIFTLEFT:
            case SHIFTRIGHT:
            case USHIFTRIGHT:
            case AND:
            case OR:
            case XOR:
            case ADD:
            case SUBTRACT:
            case MULTIPLY:
            case DIVIDE:
            case REMAINDER:
            case COMPARE_LT:
            case COMPARE_LE:
            case COMPARE_EQ:
            case STRICT_EQ:
                {
                    Arithmetic* mul = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(mul)];
                    JSValue& r1 = (*registers)[src1(mul)];
                    JSValue& r2 = (*registers)[src2(mul)];
                    const JSValue ovr = findBinaryOverride(r1, r2, BinaryOperator::mapICodeOp(instruction->op()));
                    JSFunction *target = ovr.function;
                    if (target->isNative()) {
                        JSValues argv(2);
                        argv[0] = r1;
                        argv[1] = r2;
                        dest = static_cast<JSBinaryOperator*>(target)->mCode(r1, r2);
                        break;
                    }
                    else {
                        mLinkage = new Linkage(mLinkage, ++mPC,
                                               mActivation, dst(mul));
                        mActivation = new Activation(target->getICode(), r1, r2);
                        registers = &mActivation->mRegisters;
                        mPC = mActivation->mICode->its_iCode->begin();
                        continue;
                    }
                }
                break;

            case PROP_XCR:
                {
                    PropXcr *px = static_cast<PropXcr*>(instruction);
                    JSValue& dest = (*registers)[dst(px)];
                    JSValue& base = (*registers)[src1(px)];
                    JSObject *object = base.object;
                    JSValue r = object->getProperty(*src2(px)).toNumber();
                    dest = r;
                    r.f64 += val4(px);
                    object->setProperty(*src2(px), r);
                }
                break;

            case NAME_XCR:
                {
                    NameXcr *nx = static_cast<NameXcr*>(instruction);
                    JSValue& dest = (*registers)[dst(nx)];
                    JSValue r = mGlobal->getVariable(*src1(nx)).toNumber();
                    dest = r;
                    r.f64 += val3(nx);
                    mGlobal->setVariable(*src1(nx), r);
                }
                break;

            case TEST:
                {
                    Test* tst = static_cast<Test*>(instruction);
                    (*registers)[dst(tst)] = (*registers)[src1(tst)].toBoolean();
                }
                break;
            case NEGATE:
                {
                    Negate* neg = static_cast<Negate*>(instruction);
                    (*registers)[dst(neg)] = JSValue(-(*registers)[src1(neg)].toNumber().f64);
                }
                break;
            case POSATE:
                {
                    Posate* pos = static_cast<Posate*>(instruction);
                    (*registers)[dst(pos)] = (*registers)[src1(pos)].toNumber();
                }
                break;
            case BITNOT:
                {
                    Bitnot* bn = static_cast<Bitnot*>(instruction);
                    (*registers)[dst(bn)] = JSValue(~(*registers)[src1(bn)].toInt32().i32);
                }
                break;
            case NOT:
                {
                    Not* nt = static_cast<Not*>(instruction);
                    ASSERT((*registers)[src1(nt)].isBoolean());
                    (*registers)[dst(nt)] = JSValue(!(*registers)[src1(nt)].boolean);
                }
                break;
            case THROW:
                {
                    Throw* thrw = static_cast<Throw*>(instruction);
                    throw new JSException((*registers)[op1(thrw)]);
                }
                
            case TRYIN:
                {       // push the catch handler address onto the try stack
                    Tryin* tri = static_cast<Tryin*>(instruction);
                    mActivation->catchStack.push_back(new Handler(op1(tri),
                                                                  op2(tri)));
                }   
                break;
            case TRYOUT:
                {
                    Handler *h = mActivation->catchStack.back();
                    mActivation->catchStack.pop_back();
                    delete h;
                }
                break;
            case JSR:
                {
                    subroutineStack.push(++mPC);
                    Jsr* jsr = static_cast<Jsr*>(instruction);
                    uint32 offset = ofs(jsr);
                    mPC = mActivation->mICode->its_iCode->begin() + offset;
                    continue;
                }
            case RTS:
                {
                    ASSERT(!subroutineStack.empty());
                    mPC = subroutineStack.top();
                    subroutineStack.pop();
                    continue;
                }
            case WITHIN:
                {
                    Within* within = static_cast<Within*>(instruction);
                    JSValue& value = (*registers)[op1(within)];
                    assert(value.tag == JSValue::object_tag);
                    mGlobal = new JSScope(mGlobal, value.object);
                }
                break;
            case WITHOUT:
                {
                    // Without* without = static_cast<Without*>(instruction);
                    mGlobal = mGlobal->getParent();
                }
                break;
            default:
                NOT_REACHED("bad opcode");
                break;
            }
    
            // increment the program counter.
            ++mPC;
        }
        catch (JSException x) {
            if (mLinkage) {
                if (mActivation->catchStack.empty()) {
                    Linkage *pLinkage = mLinkage;
                    for (; pLinkage != NULL; pLinkage = pLinkage->mNext) {
                        if (!pLinkage->mActivation->catchStack.empty()) {
                            mActivation = pLinkage->mActivation;
                            Handler *h = mActivation->catchStack.back();
                            registers = &mActivation->mRegisters;
                            if (h->catchTarget) {
                                mPC = mActivation->mICode->its_iCode->begin() + h->catchTarget->mOffset;
                            }
                            else {
                                ASSERT(h->finallyTarget);
                                mPC = mActivation->mICode->its_iCode->begin() + h->finallyTarget->mOffset;
                            }
                            mLinkage = pLinkage;
                            break;
                        }
                    }
                    if (pLinkage)
                        continue;
                }
                else {
                    Handler *h = mActivation->catchStack.back();
                    if (h->catchTarget) {
                        mPC = mActivation->mICode->its_iCode->begin() + h->catchTarget->mOffset;
                    }
                    else {
                        ASSERT(h->finallyTarget);
                        mPC = mActivation->mICode->its_iCode->begin() + h->finallyTarget->mOffset;
                    }
                    continue;
                }
            }
            rv = x.value;
        }
    }

    return rv;
} /* interpret */

void Context::addListener(Listener* listener)
{
    mListeners.push_back(listener);
}

void Context::removeListener(Listener* listener)
{
    ListenerIterator e = mListeners.end();
    ListenerIterator l = std::find(mListeners.begin(), e, listener);
    if (l != e) mListeners.erase(l);
}

void Context::broadcast(Event event)
{
    for (ListenerIterator i = mListeners.begin(), e = mListeners.end();
         i != e; ++i) {
        Listener* listener = *i;
        listener->listen(this, event);
    }
}

Context::Frame* Context::getFrames()
{
    return mLinkage;
}

} /* namespace Interpreter */
} /* namespace JavaScript */
