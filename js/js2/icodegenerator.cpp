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

#include "numerics.h"
#include "world.h"
#include "utilities.h"
#include "icodegenerator.h"

#include <iomanip>

namespace JS = JavaScript;
using namespace JavaScript;

ostream &JS::operator<<(ostream &s, ICodeGenerator &i)
{
    return i.print(s);
}

//
// ICodeGenerator
//


Register ICodeGenerator::loadVariable(int32 frameIndex)
{
    Register dest = getRegister();
    Instruction_2<int32, Register> *instr = new Instruction_2<int32, Register>(LOAD_VAR, frameIndex, dest);
    iCode->push_back(instr);
    return dest;
}

Register ICodeGenerator::loadImmediate(double value)
{
    Register dest = getRegister();
    Instruction_2<double, Register> *instr = new Instruction_2<double, Register>(LOAD_IMMEDIATE, value, dest);
    iCode->push_back(instr);
    return dest;
}

Register ICodeGenerator::loadName(StringAtom &name)
{
    Register dest = getRegister();
    Instruction_2<StringAtom &, Register> *instr = new Instruction_2<StringAtom &, Register>(LOAD_NAME, name, dest);
    iCode->push_back(instr);
    return dest;
}

Register ICodeGenerator::getProperty(StringAtom &name, Register base)
{
    Register dest = getRegister();
    Instruction_3<StringAtom &, Register, Register> *instr = new Instruction_3<StringAtom &, Register, Register>(GET_PROP, name, base, dest);
    iCode->push_back(instr);
    return dest;
}

void ICodeGenerator::saveVariable(int32 frameIndex, Register value)
{
    Instruction_2<int32, Register> *instr = new Instruction_2<int32, Register>(SAVE_VAR, frameIndex, value);
    iCode->push_back(instr);
}

Register ICodeGenerator::op(ICodeOp op, Register source)
{
    Register dest = getRegister();
    Instruction_2<Register, Register> *instr = new Instruction_2<Register, Register>(op, source, dest);
    iCode->push_back(instr);
    return dest;
}

Register ICodeGenerator::op(ICodeOp op, Register source1, Register source2)
{
    Register dest = getRegister();
    Instruction_3<Register, Register, Register> *instr = new Instruction_3<Register, Register, Register>(op, source1, source2, dest);
    iCode->push_back(instr);
    return dest;
}

void ICodeGenerator::branch(int32 label)
{
    Instruction_1<int32> *instr = new Instruction_1<int32>(BRANCH, label);
    iCode->push_back(instr);
}

void ICodeGenerator::branchConditional(int32 label, Register condition)
{
    Instruction_2<int32, Register> *instr = new Instruction_2<int32, Register>(BRANCH_COND, label, condition);
    iCode->push_back(instr);
}


/***********************************************************************************************/

int32 ICodeGenerator::getLabel()
{
    int32 result = labels.size();
    labels.push_back(new Label(NULL, -1));
    return result;
}

void ICodeGenerator::setLabel(int32 label)
{
    labels.at(label)->itsBase = iCode;
    labels.at(label)->itsOffset = iCode->size();
}


/***********************************************************************************************/

void ICodeGenerator::beginWhileStatement(const SourcePosition &pos)
{
    resetTopRegister();
  
    // insert a branch to the while condition, which we're 
    // moving to follow the while block
    int32 whileConditionTop = getLabel();
    int32 whileBlockStart = getLabel();
    branch(whileConditionTop);
    
    // save off the current stream while we gen code for the condition
    stitcher.push(new WhileCodeState(iCode, labels.size(), whileConditionTop, whileBlockStart));

    iCode = new InstructionStream();
}

void ICodeGenerator::endWhileExpression(Register condition)
{
    WhileCodeState *ics = (WhileCodeState *)(stitcher.top());

    branchConditional(ics->whileBodyLabel, condition);
    resetTopRegister();
    // stash away the condition expression and switch 
    // back to the main stream
    iCode = stitcher.top()->swap_iCode(iCode);
    setLabel(ics->whileBodyLabel);         // mark the start of the while block
}
  
void ICodeGenerator::endWhileStatement()
{
    // recover the while stream
    WhileCodeState *ics = dynamic_cast<WhileCodeState *>(stitcher.top());
    stitcher.pop();

    // mark the start of the condition code
    // and re-attach it to the main stream
    setLabel(ics->whileConditionLabel);

    if (ics->itsTopLabel < labels.size()) { 
            // labels (might) have been allocated in this stream
            // we need to adjust their position relative to the 
            // size of the stream we're joining

        // XXXX how do I start at 'ics->itsTopLabel' ???
        //
        for (LabelList::iterator i = labels.begin(); i != labels.end(); i++) {
            if ((*i)->itsBase == ics->its_iCode) {
                (*i)->itsBase = iCode;
                (*i)->itsOffset += iCode->size();
            }
        }
    }

    for (InstructionIterator i = ics->its_iCode->begin(); i != ics->its_iCode->end(); i++)
        iCode->push_back(*i);

    delete ics->its_iCode;
    delete ics;

    resetTopRegister();
}


/***********************************************************************************************/

void ICodeGenerator::beginIfStatement(const SourcePosition &pos, Register condition)
{
    int32 elseLabel = getLabel();
    
    // save off the current stream while we gen code for the condition
    stitcher.push(new IfCodeState(iCode, labels.size(), elseLabel, -1));

    Register notCond = op(NOT, condition);
    branchConditional(elseLabel, notCond);

    resetTopRegister();
}

void ICodeGenerator::beginElseStatement(bool hasElse)
{
    IfCodeState *ics = (IfCodeState *)(stitcher.top());

    if (hasElse) {
        int32 beyondElse = getLabel();
        ics->beyondElse = beyondElse;
        branch(beyondElse);
    }
    setLabel(ics->elseLabel);
    resetTopRegister();
}

void ICodeGenerator::endIfStatement()
{
    IfCodeState *ics = (IfCodeState *)(stitcher.top());

    if (ics->beyondElse != -1) {     // had an else
        setLabel(ics->beyondElse);   // the beyond else label
    }
    stitcher.pop();
    resetTopRegister();
}


/***********************************************************************************************/


char *opcodeName[] = {
        "load_var",
        "save_var",

        "load_imm",

        "load_name",
        "save_name",

        "get_prop",
        "set_prop", 

        "add",
        "not",

        "branch",
        "branch_cond",
};

ostream &JS::operator<<(ostream &s, StringAtom &str)
{
    for (String::iterator i = str.begin(); i != str.end(); i++)
        s << (char)*i;
    return s;
}

ostream &ICodeGenerator::print(ostream &s)
{
    s << "ICG! " << iCode->size() << "\n";
    for (InstructionIterator i = iCode->begin(); i != iCode->end(); i++) {
        for (LabelList::iterator k = labels.begin(); k != labels.end(); k++)
            if ((*k)->itsOffset == (i - iCode->begin())) {
                s << "label #" << (k - labels.begin()) << ":\n";
            }
        
        Instruction *instr = *i;
        s << "\t"<< std::setiosflags( std::ostream::left ) << std::setw(16) << opcodeName[instr->itsOp];
        switch (instr->itsOp) {
            case LOAD_NAME :
                {
                    Instruction_2<StringAtom &, Register> *t = static_cast<Instruction_2<StringAtom &, Register> * >(instr);
                    s << "\"" << t->itsOperand1 <<  "\", R" << t->itsOperand2;
                }
                break;
            case LOAD_IMMEDIATE :
                {
                    Instruction_2<double, Register> *t = static_cast<Instruction_2<double, Register> * >(instr);
                    s << t->itsOperand1 << ", R" << t->itsOperand2;
                }
                break;
            case LOAD_VAR :
            case SAVE_VAR :
                {
                    Instruction_2<int, Register> *t = static_cast<Instruction_2<int, Register> * >(instr);
                    s << "Variable #" << t->itsOperand1 << ", R" << t->itsOperand2;
                }
                break;
            case BRANCH :
                {
                    Instruction_1<int32> *t = static_cast<Instruction_1<int32> * >(instr);
                    s << "target #" << t->itsOperand1;
                }
                break;
            case BRANCH_COND :
                {
                    Instruction_2<int32, Register> *t = static_cast<Instruction_2<int32, Register> * >(instr);
                    s << "target #" << t->itsOperand1 << ", R" << t->itsOperand2;
                }
                break;
            case ADD :
                {
                     Instruction_3<Register, Register, Register> *t = static_cast<Instruction_3<Register, Register, Register> * >(instr);
                     s << "R" << t->itsOperand1 << ", R" << t->itsOperand2 << ", R" << t->itsOperand3;
                }
                break;
            case NOT :
                {
                     Instruction_3<Register, Register, Register> *t = static_cast<Instruction_3<Register, Register, Register> * >(instr);
                     s << "R" << t->itsOperand1 << ", R" << t->itsOperand2;
                }
                break;
        }
        s << "\n";
    }
    for (LabelList::iterator k = labels.begin(); k != labels.end(); k++)
        if ((*k)->itsOffset == (iCode->end() - iCode->begin())) {
            s << "label #" << (k - labels.begin()) << ":\n";
        }
    return s;
}










