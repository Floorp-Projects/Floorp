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

// these should probably be in icodegenerator.h.
typedef Instruction_2<StringAtom&, Register> LoadName, SaveName;
typedef Instruction_2<float64, Register> LoadImmediate;
typedef Instruction_2<int32, Register> LoadVar, SaveVar;
typedef Instruction_1<int32> Branch;
typedef Instruction_2<int32, Register> BranchCond;
typedef Instruction_3<Register, Register, Register> Arithmetic;
typedef Instruction_2<Register, Register> Move;

#define op1(i) (i->itsOperand1)
#define op2(i) (i->itsOperand2)
#define op3(i) (i->itsOperand3)

JSValue interpret(InstructionStream& iCode, LabelList& labels, const JSValues& args)
{
	JSValue result;
	JSValues frame(args);
	JSValues registers(32);
	static std::map<String, JSValue> globals;
	
    for (InstructionIterator pc = iCode.begin(); pc != iCode.end(); ++pc) {
        Instruction* instruction = *pc;
	    switch (instruction->opcode()) {
        case LOAD_NAME:
			{
				LoadName* i = static_cast<LoadName*>(instruction);
				registers[op2(i)] = globals[op1(i)];
			}
			break;
		case SAVE_NAME:
			{
				SaveName* i = static_cast<SaveName*>(instruction);
				globals[op1(i)] = registers[op2(i)];
			}
			break;
		case LOAD_IMMEDIATE:
			{
				LoadImmediate* i = static_cast<LoadImmediate*>(instruction);
				registers[op2(i)] = JSValue(op1(i));
			}
			break;
		case LOAD_VAR:
			{
				LoadVar* i = static_cast<LoadVar*>(instruction);
				registers[op2(i)] = frame[op1(i)];
			}
			break;
		case SAVE_VAR:
			{
				SaveVar* i = static_cast<SaveVar*>(instruction);
				frame[op1(i)] = registers[op2(i)];
			}
			break;
		case BRANCH:
			{
				Branch* i = static_cast<Branch*>(instruction);
				pc = iCode.begin() + labels[op1(i)]->itsOffset;
			}
			break;
		case BRANCH_COND:
			{
				BranchCond* i = static_cast<BranchCond*>(instruction);
				// s << "target #" << t->itsOperand1 << ", R" << t->itsOperand2;
				if (registers[op2(i)].i32)
					pc = iCode.begin() + labels[op1(i)]->itsOffset;
			}
			break;
		case ADD:
			{
				// could get clever here with Functional forms.
				Arithmetic* i = static_cast<Arithmetic*>(instruction);
				registers[op3(i)] = JSValue(registers[op1(i)].f64 + registers[op2(i)].f64);
			}
			break;
		case COMPARE:
			{
				Arithmetic* i = static_cast<Arithmetic*>(instruction);
				registers[op3(i)].i32 = registers[op1(i)].f64 == registers[op2(i)].f64;
			}
			break;
		case MOVE_TO:
			{
				Move* i = static_cast<Move*>(instruction);
				registers[op2(i)] = registers[op1(i)];
			}
			break;
		case NOT:
			{
				Move* i = static_cast<Move*>(instruction);
				registers[op2(i)].i32 = !registers[op1(i)].i32;
			}
			break;
    	}
    }

	return result;
}

}
