// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.

#include "interpreter.h"
#include "world.h"

#include <map>

namespace JavaScript {

using std::map;
using std::less;
using std::pair;

/**
 * Private representation of a JavaScript object.
 * This will change over time, so it is treated as an opaque
 * type everywhere else but here.
 */
#ifdef WIN32
    class JSObject : public map<String, JSValue, less<String>, gc_allocator<JSValue> > 
#else
    class JSObject : public map<String, JSValue, less<String>, gc_allocator<pair<const String, JSValue> > > 
#endif
        {
public:
    void* operator new(size_t) { return alloc.allocate(1, 0); }
    void operator delete(void* /* ptr */) {}
private:
    static gc_allocator<JSObject> alloc;
};

// static allocator (required when gc_allocator<T> is allocator<T>.
gc_allocator<JSObject> JSObject::alloc;

// operand access macros.
#define op1(i) (i->itsOperand1)
#define op2(i) (i->itsOperand2)
#define op3(i) (i->itsOperand3)

JSValue interpret(InstructionStream& iCode, const JSValues& args)
{
	JSValue result;
	JSValues frame(args);
	JSValues registers(32);
	static JSObject globals;
	
	InstructionIterator pc = iCode.begin();
    while (pc != iCode.end()) {
        Instruction* instruction = *pc;
	    switch (instruction->opcode()) {
		case MOVE_TO:
			{
				Move* mov = static_cast<Move*>(instruction);
				registers[op1(mov)] = registers[op2(mov)];
			}
			break;
                case LOAD_NAME:
			{
				LoadName* ln = static_cast<LoadName*>(instruction);
                                registers[op1(ln)] = globals[*op2(ln)];
			}
			break;
		case SAVE_NAME:
			{
				SaveName* sn = static_cast<SaveName*>(instruction);
				globals[*op1(sn)] = registers[op2(sn)];
			}
			break;
		case NEW_OBJECT:
		        {
		                NewObject* no = static_cast<NewObject*>(instruction);
		                registers[op1(no)].obj = new JSObject();
		        }
		        break;
		case GET_PROP:
		        {
		                GetProp* gp = static_cast<GetProp*>(instruction);
		                JSObject* obj = registers[op2(gp)].obj;
		                registers[op1(gp)] = (*obj)[*op3(gp)];
                        }
		        break;
		case SET_PROP:
		        {
		                SetProp* sp = static_cast<SetProp*>(instruction);
		                JSObject* obj = registers[op2(sp)].obj;
                                (*obj)[*op1(sp)] = registers[op3(sp)];
                        }
		        break;
		case LOAD_IMMEDIATE:
			{
				LoadImmediate* li = static_cast<LoadImmediate*>(instruction);
				registers[op1(li)] = JSValue(op2(li));
			}
			break;
		case LOAD_VAR:
			{
				LoadVar* lv = static_cast<LoadVar*>(instruction);
				registers[op1(lv)] = frame[op2(lv)];
			}
			break;
		case SAVE_VAR:
			{
				SaveVar* sv = static_cast<SaveVar*>(instruction);
				frame[op1(sv)] = registers[op2(sv)];
			}
			break;
		case BRANCH:
			{
				ResolvedBranch* bra = static_cast<ResolvedBranch*>(instruction);
				pc = iCode.begin() + op1(bra);
				continue;
			}
			break;
		case BRANCH_LT:
			{
				ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
				if (registers[op2(bc)].i32 < 0) {
					pc = iCode.begin() + op1(bc);
					continue;
				}
			}
			break;
		case BRANCH_LE:
			{
				ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
				if (registers[op2(bc)].i32 <= 0) {
					pc = iCode.begin() + op1(bc);
					continue;
				}
			}
			break;
		case BRANCH_EQ:
			{
				ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
				if (registers[op2(bc)].i32 == 0) {
					pc = iCode.begin() + op1(bc);
					continue;
				}
			}
			break;
		case BRANCH_NE:
			{
				ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
				if (registers[op2(bc)].i32 != 0) {
					pc = iCode.begin() + op1(bc);
					continue;
				}
			}
			break;
		case BRANCH_GE:
			{
				ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
				if (registers[op2(bc)].i32 >= 0) {
					pc = iCode.begin() + op1(bc);
					continue;
				}
			}
			break;
		case BRANCH_GT:
			{
				ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
				if (registers[op2(bc)].i32 > 0) {
					pc = iCode.begin() + op1(bc);
					continue;
				}
			}
			break;
		case ADD:
			{
				// could get clever here with Functional forms.
				Arithmetic* add = static_cast<Arithmetic*>(instruction);
				registers[op1(add)] = JSValue(registers[op2(add)].f64 + registers[op3(add)].f64);
			}
			break;
		case SUBTRACT:
			{
				Arithmetic* sub = static_cast<Arithmetic*>(instruction);
				registers[op1(sub)] = JSValue(registers[op2(sub)].f64 - registers[op3(sub)].f64);
			}
			break;
		case MULTIPLY:
			{
				Arithmetic* mul = static_cast<Arithmetic*>(instruction);
				registers[op1(mul)] = JSValue(registers[op2(mul)].f64 * registers[op3(mul)].f64);
			}
			break;
		case DIVIDE:
			{
				Arithmetic* div = static_cast<Arithmetic*>(instruction);
				registers[op1(div)] = JSValue(registers[op2(div)].f64 / registers[op3(div)].f64);
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
				float64 diff = (registers[op2(cmp)].f64 - registers[op3(cmp)].f64);
				registers[op1(cmp)].i32 = (diff == 0.0 ? 0 : (diff > 0.0 ? 1 : -1));
			}
			break;
		case NOT:
			{
				Move* nt = static_cast<Move*>(instruction);
				registers[op1(nt)].i32 = !registers[op2(nt)].i32;
			}
			break;
                case RETURN:
                        {
                                Return* ret = static_cast<Return*>(instruction);
                                result = registers[op1(ret)];
                                return result;
                        }
                        break;
                default:
                        break;
    	}
    	
    	// increment the program counter.
    	++pc;
    }

	return result;
}

}
