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
    
    JSValue
    Context::interpret(ICodeModule* iCode, const JSValues& args)
    {
        // stack of JSFrames.
        // XXX is a linked list of activation's sufficient?
        JSFrameStack frames;

        // initial activation.
        JSActivation* activation = new JSActivation(iCode, args);
        JSValues* registers = &activation->mRegisters;

        InstructionIterator begin_pc = iCode->its_iCode->begin();
        InstructionIterator pc = begin_pc;

        std::vector<InstructionIterator> catchStack;     // <-- later will need to restore scope, other 'global' values
        while (true) {
            try {
                Instruction* instruction = *pc;
                switch (instruction->op()) {
                case CALL:
                    {
                        Call* call = static_cast<Call*>(instruction);
                        frames.push(new JSFrame(++pc, begin_pc, activation,
                                                op1(call)));
                        ICodeModule* target = 
                            (*registers)[op2(call)].function->getICode();
                        activation = new JSActivation(target, activation, op3(call));
                        registers = &activation->mRegisters;
                        begin_pc = pc = target->its_iCode->begin();
                    }
                    continue;

                case RETURN_VOID:
                    {
                        JSValue result(NotARegister);
                        if (frames.empty())
                            return result;
                        JSFrame *frame = frames.top();
                        frames.pop();
                        activation = frame->itsActivation;
                        registers = &activation->mRegisters;
                        (*registers)[frame->itsResult] = result;
                        pc = frame->itsReturnPC;
                        begin_pc = frame->itsBasePC;
                    }
                    continue;

                case RETURN:
                    {
                        Return* ret = static_cast<Return*>(instruction);
                        JSValue result(NotARegister);
                        if (op1(ret) != NotARegister) 
                            result = (*registers)[op1(ret)];
                        if (frames.empty())
                            return result;
                        JSFrame *frame = frames.top();
                        frames.pop();
                        activation = frame->itsActivation;
                        registers = &activation->mRegisters;
                        (*registers)[frame->itsResult] = result;
                        pc = frame->itsReturnPC;
                        begin_pc = frame->itsBasePC;
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
                        (*registers)[dst(ln)] = mGlobal[*src1(ln)];
                    }
                    break;
                case SAVE_NAME:
                    {
                        SaveName* sn = static_cast<SaveName*>(instruction);
                        mGlobal[*dst(sn)] = (*registers)[src1(sn)];
                    }
                    break;
                case NEW_OBJECT:
                    {
                        NewObject* no = static_cast<NewObject*>(instruction);
                        (*registers)[dst(no)].object = new JSObject();
                    }
                    break;
                case NEW_ARRAY:
                    {
                        NewArray* na = static_cast<NewArray*>(instruction);
                        (*registers)[dst(na)].array = new JSArray();
                    }
                    break;
                case GET_PROP:
                    {
                        GetProp* gp = static_cast<GetProp*>(instruction);
                        JSObject* object = (*registers)[src1(gp)].object;
                        (*registers)[dst(gp)] = (*object)[*src2(gp)];
                    }
                    break;
                case SET_PROP:
                    {
                        SetProp* sp = static_cast<SetProp*>(instruction);
                        JSObject* object = (*registers)[dst(sp)].object;
                        (*object)[*src1(sp)] = (*registers)[src2(sp)];
                    }
                    break;
                case GET_ELEMENT:
                    {
                        GetElement* ge = static_cast<GetElement*>(instruction);
                        JSArray* array = (*registers)[src1(ge)].array;
                        (*registers)[dst(ge)] = (*array)[(*registers)[src2(ge)]];
                    }
                    break;
                case SET_ELEMENT:
                    {
                        SetElement* se = static_cast<SetElement*>(instruction);
                        JSArray* array = (*registers)[dst(se)].array;
                        (*array)[(*registers)[src1(se)]] = (*registers)[src2(se)];
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
                        (*registers)[dst(cmp)].i32 = 
                            (diff == 0.0 ? 0 : (diff > 0.0 ? 1 : -1));
                    }
                    break;
                case NOT:
                    {
                        Move* nt = static_cast<Move*>(instruction);
                        (*registers)[dst(nt)].i32 = !(*registers)[src1(nt)].i32;
                    }
                    break;
                
                case THROW :
                    {
                        throw new JS_Exception();
                    }
                    
                case TRY:
                    {       // push the catch handler address onto the try stack
                            // why did Rhino interpreter also have a finally stack?
                        Try* tri = static_cast<Try*>(instruction);
                        catchStack.push_back(begin_pc + ofs(tri));
                    }   
                    break;
                case ENDTRY :
                    {
                        catchStack.pop_back();
                    }
                    break;

                default:
                    NOT_REACHED("bad opcode");
                    break;
                }
        
                // increment the program counter.
                ++pc;
            }
            catch (JS_Exception ) {
                ASSERT(!catchStack.empty());
                pc = catchStack.back();
                catchStack.pop_back();
            }
        }
    } /* interpret */

} /* namespace JavaScript */
