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

#ifndef icodegenerator_h
#define icodegenerator_h

#include "utilities.h"
#include "parser.h"

#include <vector>
#include <stack>
#include <algorithm>

namespace JavaScript {

    typedef uint32 Register;

    enum ICodeOp {
                        //      Operand1                Operand2                Operand3

        LOAD_VAR,       // index of frame slot      Destination Register
        SAVE_VAR,       // index of frame slot      Source Register

        LOAD_IMMEDIATE, // immediate (double)       Destination Register

        LOAD_NAME,      // StringAtom &             Destination Register
        SAVE_NAME,      // StringAtom &             Source Register

        GET_PROP,       // StringAtom &             Base Register               Destination Register
        SET_PROP,       // StringAtom &             Base Register               Source Register

        ADD,            // Source Register 1        Source Register 2           Destination Register
        NOT,            // Source Register          Destination Register

        BRANCH,         // Target label
        BRANCH_COND,    // Target label             Condition Register
    };

    class Instruction {
    public:
        Instruction(ICodeOp op) : itsOp(op) { }
        ICodeOp itsOp;
    };

    template <typename Operand1>
        class Instruction_1 : public Instruction {
        public:
            Instruction_1(ICodeOp op, Operand1 operand1) :
                            Instruction(op), itsOperand1(operand1) {  }
            Operand1 itsOperand1;
        };

    template <typename Operand1, typename Operand2>
        class Instruction_2 : public Instruction {
        public:
            Instruction_2(ICodeOp op, Operand1 operand1, Operand2 operand2) :
                            Instruction(op), itsOperand1(operand1), itsOperand2(operand2) {  }
            Operand1 itsOperand1;
            Operand2 itsOperand2;
        };

    template <typename Operand1, typename Operand2, typename Operand3>
        class Instruction_3 : public Instruction {
        public:
            Instruction_3(ICodeOp op, Operand1 operand1, Operand2 operand2, Operand3 operand3) :
                            Instruction(op), itsOperand1(operand1), itsOperand2(operand2), itsOperand3(operand3) {  }
            Operand1 itsOperand1;
            Operand2 itsOperand2;
            Operand3 itsOperand3;
        };

    typedef std::vector<Instruction *> InstructionStream;
    typedef InstructionStream::iterator InstructionIterator;

    /****************************************************************/

    class Label {
    public:
        Label(InstructionStream *base, int32 offset) : itsBase(base), itsOffset(offset) { }

        InstructionStream *itsBase;
        int32 itsOffset;
    };

    typedef std::vector<Label *> LabelList;
    typedef LabelList::iterator LabelIterator;

    /****************************************************************/
    /****************************************************************/

    enum StateKind { While_State, If_State };

    class ICodeState { 
    public :
        ICodeState(StateKind kind) : stateKind(kind) { }
        StateKind stateKind;
    };

    // an ICodeState that handles switching to a new InstructionStream 
    // and then re-combining the streams later
    class MultiPathICodeState : public ICodeState {
    public:
        MultiPathICodeState(StateKind kind,InstructionStream *iCode, int32 topLabel)
                            : ICodeState(kind), its_iCode(iCode), itsTopLabel(topLabel) {}

        InstructionStream *swapStream(InstructionStream *iCode) { InstructionStream *t = its_iCode; its_iCode = iCode; return t; }

        InstructionStream *its_iCode;
        int32 itsTopLabel;      // set to the highest label allocated when this stream
                                // was created. If that value changes, this stream may
                                // contain labels that will need to be adjusted when
                                // the streams are merged.
        void mergeStream(InstructionStream *mainStream, LabelList &labels);
    };

    class WhileCodeState : public MultiPathICodeState {
    public:
        WhileCodeState(InstructionStream *iCode, int32 topLabel, int32 a, int32 b) 
                    : MultiPathICodeState(While_State, iCode, topLabel), whileConditionLabel(a), whileBodyLabel(b) { }
        int32 whileConditionLabel;
        int32 whileBodyLabel;
    };

    class IfCodeState : public ICodeState {
    public:
        IfCodeState(int32 a, int32 b) : ICodeState(If_State), elseLabel(a), beyondElse(b) { }
        int32 elseLabel;
        int32 beyondElse;
    };

    /****************************************************************/
        
    // An ICodeGenerator provides the interface between the parser and the interpreter.
    // The parser constructs one of these for each function/script, adds statements and
    // expressions to it and then converts it into an ICodeModule, ready for execution.

    class ICodeGenerator {
    private:
           
        InstructionStream *iCode;
        
        LabelList labels;

        std::stack<ICodeState *> stitcher;

        Register topRegister;
        Register getRegister()      { return topRegister++; }
        void resetTopRegister()     { topRegister = 0; }

        int32 getLabel();
        void setLabel(int32 label);

        void branch(int32 label);
        void branchConditional(int32 label, Register condition);
    
    public:
        ICodeGenerator() { iCode = new InstructionStream(); }

        InstructionStream *complete();

        ostream &print(ostream &s);

        Register op(ICodeOp op, Register source);
        Register op(ICodeOp op, Register source1, Register source2);

        Register loadVariable(int32 frameIndex);
        Register loadImmediate(double value);

        void saveVariable(int32 frameIndex, Register value);

        Register loadName(StringAtom &name);
        Register getProperty(StringAtom &name, Register base);


        void beginStatement(const SourcePosition &pos)        { resetTopRegister(); }


    // Rather than have the ICG client maniplate labels and branches, it
    // uses the following calls to describe the high level looping constructs
    // being generated. The functions listed below are expected to be called
    // in the order listed for each construct, (internal error otherwise).
    // The ICG will enforce correct nesting and closing.

    
        void beginWhileStatement(const SourcePosition &pos);
        void endWhileExpression(Register condition);
        void endWhileStatement();

    
        void beginDoStatement();
        void endDoStatement();
        void endDoExpression(Register condition);


        void beginIfStatement(const SourcePosition &pos, Register condition);
        void beginElseStatement(bool hasElse);           // required, regardless of existence of else clause
        void endIfStatement();


        // for ( ... in ...) statements get turned into generic for statements by the parser (ok?)
        void beginForStatement();                      // for initialization is emitted prior to this call
        void forCondition(Register condition);         // required with optional <operand>
        void forIncrement(Register expression);        // required with optional <operand>
        void endForStatement();


        void beginSwitchStatement(Register expression);

        // sequences of the next three follow for each case clause
        void beginCaseStatement();
        void endCaseCondition(Register expression);
        void endCaseStatement();             // corresponds to a break and may be omitted
    
        // optionally
        void beginDefaultStatement();
        void endDefaultStatement();          // the break for the default clause, may be omitted

        void endSwitchStatement();


        void labelStatement(const StringAtom &label);         // adds to label set for next statement, 
                                                         // removed when that statement is finished
        void continueStatement(const StringAtom &label);


        void throwStatement(Register expression);


        void returnStatement(Register expression);     // optional <operand>


        void beginCatchStatement();
        void endCatchExpression(Register expression);
        void endCatchStatement();

    };

    ostream &operator<<(ostream &s, ICodeGenerator &i);
    ostream &operator<<(ostream &s, StringAtom &str);
}
#endif