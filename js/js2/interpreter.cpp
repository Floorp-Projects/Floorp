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
#include "vmtypes.h"

namespace JavaScript {
namespace JSTypes {

static JSObject kUndefinedObject;
const JSValue kUndefinedValue(&kUndefinedObject);

} // namespace JSTypes
} // namespace JavaScript

namespace JavaScript {
namespace Interpreter {

// operand access macros.
#define op1(i) (i->o1())
#define op2(i) (i->o2())
#define op3(i) (i->o3())

// mnemonic names for operands.
#define dst(i)  op1(i)
#define src1(i) op2(i)
#define src2(i) op3(i)
#define ofs(i)  (i->getOffset())

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
};

/**
 * Stores saved state from the *previous* activation, the current
 * activation is alive and well in locals of the interpreter loop.
 */
struct Linkage : public Context::Frame, public gc_base {
    Linkage*            mNext;              // next linkage in linkage stack.
    InstructionIterator mReturnPC;
    InstructionIterator mBasePC;
    Activation*         mActivation;        // caller's activation.
    Register            mResult;            // the desired target register for the return value

    Linkage(Linkage* linkage, InstructionIterator returnPC, InstructionIterator basePC,
            Activation* activation, Register result) 
        :   mNext(linkage), mReturnPC(returnPC), mBasePC(basePC),
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

JSValue Context::interpret(ICodeModule* iCode, const JSValues& args)
{
    // initial activation.
    Activation* activation = new Activation(iCode, args);
    JSValues* registers = &activation->mRegisters;

    InstructionIterator begin_pc = iCode->its_iCode->begin();
    InstructionIterator pc = begin_pc;

    // stack of all catch/finally handlers available for the current activation
    std::stack<InstructionIterator> subroutineStack;    // to implement jsr/rts for finally code
    while (true) {
        try {
            // tell any listeners about the current execution state.
            // XXX should only do this if we're single stepping/tracing.
            if (mListeners.size()) {
                broadcast(pc, registers, activation->mICode);
            }
            Instruction* instruction = *pc;
            switch (instruction->op()) {
            case CALL:
                {
                    Call* call = static_cast<Call*>(instruction);
                    mLinkage = new Linkage(mLinkage, ++pc, begin_pc, activation,
                                            op1(call));
                    ICodeModule* target = 
                        (*registers)[op2(call)].function->getICode();
                    activation = new Activation(target, activation, op3(call));
                    registers = &activation->mRegisters;
                    begin_pc = pc = target->its_iCode->begin();
                }
                continue;

            case RETURN_VOID:
                {
                    JSValue result;
                    Linkage *linkage = mLinkage;
                    if (!linkage)
                        return result;
                    mLinkage = linkage->mNext;
                    activation = linkage->mActivation;
                    registers = &activation->mRegisters;
                    (*registers)[linkage->mResult] = result;
                    pc = linkage->mReturnPC;
                    begin_pc = linkage->mBasePC;
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
                        return result;
                    mLinkage = linkage->mNext;
                    activation = linkage->mActivation;
                    registers = &activation->mRegisters;
                    (*registers)[linkage->mResult] = result;
                    pc = linkage->mReturnPC;
                    begin_pc = linkage->mBasePC;
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
                    (*registers)[dst(ln)] = mGlobal->getProperty(*src1(ln));
                }
                break;
            case SAVE_NAME:
                {
                    SaveName* sn = static_cast<SaveName*>(instruction);
                    mGlobal->setProperty(*dst(sn), (*registers)[src1(sn)]);
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
            case BRANCH:
                {
                    GenericBranch* bra =
                        static_cast<GenericBranch*>(instruction);
                    pc = begin_pc + ofs(bra);
                    continue;
                }
                break;
            case BRANCH_LT:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    if ((*registers)[src1(bc)].i32 < 0) {
                        pc = begin_pc + ofs(bc);
                        continue;
                    }
                }
                break;
            case BRANCH_LE:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    if ((*registers)[src1(bc)].i32 <= 0) {
                        pc = begin_pc + ofs(bc);
                        continue;
                    }
                }
                break;
            case BRANCH_EQ:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    if ((*registers)[src1(bc)].i32 == 0) {
                        pc = begin_pc + ofs(bc);
                        continue;
                    }
                }
                break;
            case BRANCH_NE:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    if ((*registers)[src1(bc)].i32 != 0) {
                        pc = begin_pc + ofs(bc);
                        continue;
                    }
                }
                break;
            case BRANCH_GE:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    if ((*registers)[src1(bc)].i32 >= 0) {
                        pc = begin_pc + ofs(bc);
                        continue;
                    }
                }
                break;
            case BRANCH_GT:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    if ((*registers)[src1(bc)].i32 > 0) {
                        pc = begin_pc + ofs(bc);
                        continue;
                    }
                }
                break;
            case ADD:
                {
                    // could get clever here with Functional forms.
                    Arithmetic* add = static_cast<Arithmetic*>(instruction);
                    (*registers)[dst(add)] = 
                        JSValue((*registers)[src1(add)].f64 +
                                (*registers)[src2(add)].f64);
                }
                break;
            case SUBTRACT:
                {
                    Arithmetic* sub = static_cast<Arithmetic*>(instruction);
                    (*registers)[dst(sub)] = 
                        JSValue((*registers)[src1(sub)].f64 -
                                (*registers)[src2(sub)].f64);
                }
                break;
            case MULTIPLY:
                {
                    Arithmetic* mul = static_cast<Arithmetic*>(instruction);
                    (*registers)[dst(mul)] =
                        JSValue((*registers)[src1(mul)].f64 *
                                (*registers)[src2(mul)].f64);
                }
                break;
            case DIVIDE:
                {
                    Arithmetic* div = static_cast<Arithmetic*>(instruction);
                    (*registers)[dst(div)] = 
                        JSValue((*registers)[src1(div)].f64 /
                                (*registers)[src2(div)].f64);
                }
                break;
            case COMPARE_LT:
            case COMPARE_LE:
            case COMPARE_EQ:
            case COMPARE_NE:
            case COMPARE_GT:
            case COMPARE_GE:
                {
                    Arithmetic* cmp = static_cast<Arithmetic*>(instruction);
                    float64 diff = 
                        ((*registers)[src1(cmp)].f64 - 
                         (*registers)[src2(cmp)].f64);
                    (*registers)[dst(cmp)] = 
                        JSValue(int32(diff == 0.0 ? 0 : (diff > 0.0 ? 1 : -1)));
                }
                break;
            case NOT:
                {
                    Not* nt = static_cast<Not*>(instruction);
                    (*registers)[dst(nt)] = JSValue(int32(!(*registers)[src1(nt)].i32));
                }
                break;
            
            case THROW :
                {
                    Throw* thrw = static_cast<Throw*>(instruction);
                    throw new JSException((*registers)[op1(thrw)]);
                }
                
            case TRY:
                {       // push the catch handler address onto the try stack
                    Try* tri = static_cast<Try*>(instruction);
                    activation->catchStack.push_back(new Handler(op1(tri), op2(tri)));
                }   
                break;
            case ENDTRY :
                {
                    Handler *h = activation->catchStack.back();
                    activation->catchStack.pop_back();
                    delete h;
                }
                break;
            case JSR :
                {
                    subroutineStack.push(++pc);
                    Jsr* jsr = static_cast<Jsr*>(instruction);
                    uint32 offset = ofs(jsr);
                    pc = begin_pc + offset;
                    continue;
                }
            case RTS :
                {
                    ASSERT(!subroutineStack.empty());
                    pc = subroutineStack.top();
                    subroutineStack.pop();
                    continue;
                }
            default:
                NOT_REACHED("bad opcode");
                break;

            }
    
            // increment the program counter.
            ++pc;
        }
        catch (JSException x) {
            if (mLinkage) {
                if (activation->catchStack.empty()) {
                    Linkage *pLinkage = mLinkage;
                    for (; pLinkage != NULL; pLinkage = pLinkage->mNext) {
                        if (!pLinkage->mActivation->catchStack.empty()) {
                            activation = pLinkage->mActivation;
                            Handler *h = activation->catchStack.back();
                            registers = &activation->mRegisters;
                            begin_pc = pLinkage->mBasePC;
                            if (h->catchTarget) {
                                pc = begin_pc + h->catchTarget->mOffset;
                            }
                            else {
                                ASSERT(h->finallyTarget);
                                pc = begin_pc + h->finallyTarget->mOffset;
                            }
                            mLinkage = pLinkage;
                            break;
                        }
                    }
                    if (pLinkage)
                        continue;
                }
                else {
                    Handler *h = activation->catchStack.back();
                    if (h->catchTarget) {
                        pc = begin_pc + h->catchTarget->mOffset;
                    }
                    else {
                        ASSERT(h->finallyTarget);
                        pc = begin_pc + h->finallyTarget->mOffset;
                    }
                    continue;
                }
            }
            return x.value;            
        }
    }
} /* interpret */

void Context::addListener(Listener* listener)
{
    mListeners.push_back(listener);
}

typedef std::vector<Context::Listener*>::iterator ListenerIterator;

void Context::removeListener(Listener* listener)
{   
    for (ListenerIterator i = mListeners.begin(), e = mListeners.end(); i != e; ++i) {
        if (*i == listener) {
            mListeners.erase(i);
            break;
        }
    }
}

void Context::broadcast(InstructionIterator pc, JSValues* registers, ICodeModule* iCode)
{
    for (ListenerIterator i = mListeners.begin(), e = mListeners.end(); i != e; ++i) {
        Listener* listener = *i;
        listener->listen(this, pc, registers, iCode);
    }
}

Context::Frame* Context::getFrames()
{
    return mLinkage;
}

} /* namespace Interpreter */
} /* namespace JavaScript */
