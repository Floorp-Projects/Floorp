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
#include <iostream>

namespace JavaScript {

    typedef uint32 Register;

    enum ICodeOp {
                        //      Operand1                Operand2                Operand3
        NOP,

        MOVE_TO,        // Source Register          Destination Register

        LOAD_VAR,       // index of frame slot      Destination Register
        SAVE_VAR,       // index of frame slot      Source Register

        LOAD_IMMEDIATE, // immediate (double)       Destination Register

        LOAD_NAME,      // StringAtom &             Destination Register
        SAVE_NAME,      // StringAtom &             Source Register

        NEW_OBJECT,     // Destination Register

        GET_PROP,       // StringAtom &             Base Register               Destination Register
        SET_PROP,       // StringAtom &             Base Register               Source Register

        ADD,            // Source Register 1        Source Register 2           Destination Register
        SUBTRACT,
        MULTIPLY,
        DIVIDE,
        
        // maintain contiguity                     
        COMPARE_LT,     // Source Register 1        Source Register 2           Destination Register
        COMPARE_LE,
        COMPARE_EQ,
        COMPARE_NE,
        COMPARE_GE,
        COMPARE_GT,

        NOT,            // Source Register          Destination Register

        BRANCH,         // Target label

        BRANCH_LT,      // Target label             Condition Register
        BRANCH_LE,
        BRANCH_EQ,
        BRANCH_NE,
        BRANCH_GE,
        BRANCH_GT,
        
        RETURN          // Source Register
    };

    class Instruction {
    public:
        Instruction(ICodeOp op) : itsOp(op) { }

#ifdef DEBUG
        // provide a virtual destructor, so we can see dynamic type in debugger.
        virtual ~Instruction() { }
#endif

        ICodeOp itsOp;
        
        ICodeOp getBranchOp()   { return ((itsOp >= COMPARE_LT) && (itsOp <= COMPARE_GT)) ? (ICodeOp)(BRANCH_LT + (itsOp - COMPARE_LT)) : NOP;  }
        ICodeOp opcode()        { return itsOp; }
    };

    template <typename Operand1>
        class Instruction_1 : public Instruction {
        public:
            Instruction_1(ICodeOp op, Operand1 operand1) :
                            Instruction(op), itsOperand1(operand1) {  }
            Operand1 itsOperand1;
            
            Operand1& o1() { return itsOperand1; }
        };

    template <typename Operand1, typename Operand2>
        class Instruction_2 : public Instruction {
        public:
            Instruction_2(ICodeOp op, Operand1 operand1, Operand2 operand2) :
                            Instruction(op), itsOperand1(operand1), itsOperand2(operand2) {  }
            Operand1 itsOperand1;
            Operand2 itsOperand2;

            Operand1& o1() { return itsOperand1; }
            Operand2& o2() { return itsOperand2; }
        };

    template <typename Operand1, typename Operand2, typename Operand3>
        class Instruction_3 : public Instruction {
        public:
            Instruction_3(ICodeOp op, Operand1 operand1, Operand2 operand2, Operand3 operand3) :
                            Instruction(op), itsOperand1(operand1), itsOperand2(operand2), itsOperand3(operand3) {  }
            Operand1 itsOperand1;
            Operand2 itsOperand2;
            Operand3 itsOperand3;

            Operand1& o1() { return itsOperand1; }
            Operand2& o2() { return itsOperand2; }
            Operand3& o3() { return itsOperand3; }
        };

    typedef Instruction_3<StringAtom*, Register, Register> GetProp, SetProp;
    typedef Instruction_2<StringAtom*, Register> LoadName, SaveName;
    typedef Instruction_2<float64, Register> LoadImmediate;
    typedef Instruction_2<uint32, Register> LoadVar, SaveVar;
    typedef Instruction_1<int32> Branch;
    typedef Instruction_2<int32, Register> BranchCond;
    typedef Instruction_3<Register, Register, Register> Arithmetic;
    typedef Instruction_3<Register, Register, Register> Compare;
    typedef Instruction_2<Register, Register> Move;

    class Return : public Instruction_1<Register> {
    public:
        Return(Register source) : Instruction_1<Register>(RETURN, source) {}
    };

    class NewObject : public Instruction_1<Register> {
    public:
        NewObject(Register dest) : Instruction_1<Register>(NEW_OBJECT, dest) {}
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

    class ICodeGenerator;   // forward declaration

    enum StateKind { While_state, If_state, Do_state, Switch_state, For_state };

    class ICodeState {
    public :
        ICodeState(StateKind kind, ICodeGenerator *icg);        // inline below
        virtual ~ICodeState()   { }

        virtual int32 getBreakLabel(ICodeGenerator *)       { ASSERT(false); return 0;}
        virtual int32 getContinueLabel(ICodeGenerator *)    { ASSERT(false); return 0;}

        StateKind stateKind;
        Register registerBase;
        int32 breakLabel;
        int32 continueLabel;
    };

    /****************************************************************/
        
    // An ICodeGenerator provides the interface between the parser and the interpreter.
    // The parser constructs one of these for each function/script, adds statements and
    // expressions to it and then converts it into an ICodeModule, ready for execution.

    class ICodeGenerator {
      private:
        InstructionStream *iCode;

        LabelList labels;

        std::vector<ICodeState *> stitcher;

        Register topRegister;
        Register getRegister()      { return topRegister++; }
        void resetTopRegister()     { topRegister = stitcher.empty() ? 0 : stitcher.back()->registerBase; }

        ICodeOp getBranchOp()       { ASSERT(!iCode->empty()); return iCode->back()->getBranchOp(); }

      public:
        int32 getLabel();
      private:
        void setLabel(int32 label);
        void setLabel(InstructionStream *stream, int32 label);

        void branch(int32 label);
        void branchConditional(int32 label, Register condition);
    
      public:
        ICodeGenerator() : topRegister(0) { iCode = new InstructionStream(); }

        void mergeStream(InstructionStream *sideStream);
        
        InstructionStream *complete();

        std::ostream &print(std::ostream &s);

        Register op(ICodeOp op, Register source);
        Register op(ICodeOp op, Register source1, Register source2);

        Register compare(ICodeOp op, Register source1, Register source2);
 
        Register loadVariable(uint32 frameIndex);
        Register loadImmediate(double value);

        void saveVariable(uint32 frameIndex, Register value);

        Register newObject();

        Register loadName(StringAtom &name);
        void saveName(StringAtom &name, Register value);
        
        Register getProperty(StringAtom &name, Register base);
        void setProperty(StringAtom &name, Register base, Register value);

        Register getRegisterBase()                          { return topRegister; }
        InstructionStream *get_iCode()                      { return iCode; }


    // Rather than have the ICG client maniplate labels and branches, it
    // uses the following calls to describe the high level looping constructs
    // being generated. The functions listed below are expected to be called
    // in the order listed for each construct, (internal error otherwise).
    // The ICG will enforce correct nesting and closing.

        // expression statements
        void beginStatement(uint32 /*pos*/)                 { resetTopRegister(); }
    
        void returnStatement(Register result);

        void beginWhileStatement(uint32 pos);
        void endWhileExpression(Register condition);
        void endWhileStatement();

    
        void beginDoStatement(uint32 pos);
        void endDoStatement();
        void endDoExpression(Register condition);


        void beginIfStatement(uint32 pos, Register condition);
        void beginElseStatement(bool hasElse);           // required, regardless of existence of else clause
        void endIfStatement();


        // for ( ... in ...) statements get turned into generic for statements by the parser (ok?)
        void beginForStatement(uint32 pos);            // for initialization is emitted prior to this call
        void forCondition(Register condition);         // required
        void forIncrement();                           // required
        void endForStatement();


        void beginSwitchStatement(uint32 pos, Register expression);

        void endCaseCondition(Register expression);
    
        void beginCaseStatement();
        void endCaseStatement();
        
        // optionally
        void beginDefaultStatement();
        void endDefaultStatement();

        void endSwitchStatement();


        void labelStatement(const StringAtom &label);           // adds to label set for next statement, 
                                                                // removed when that statement is finished
        void continueStatement();
        void breakStatement();

        void continueStatement(const StringAtom &label);
        void breakStatement(const StringAtom &label);


        void throwStatement(Register expression);

        void beginCatchStatement();
        void endCatchExpression(Register expression);
        void endCatchStatement();

    };

    std::ostream &operator<<(std::ostream &s, ICodeGenerator &i);
    std::ostream &operator<<(std::ostream &s, StringAtom &str);

    class WhileCodeState : public ICodeState {
    public:
        WhileCodeState(int32 conditionLabel, int32 bodyLabel, ICodeGenerator *icg);         // inline below
        InstructionStream *swapStream(InstructionStream *iCode) { InstructionStream *t = whileExpressionStream; whileExpressionStream = iCode; return t; }

        virtual int32 getBreakLabel(ICodeGenerator *icg)    { if (breakLabel == -1) breakLabel = icg->getLabel(); return breakLabel; }
        virtual int32 getContinueLabel(ICodeGenerator *)    { return whileCondition; }

        int32 whileCondition;
        int32 whileBody;
        InstructionStream *whileExpressionStream;
    };

    class ForCodeState : public ICodeState {
    public:
        ForCodeState(int32 conditionLabel, int32 bodyLabel, ICodeGenerator *icg);        // inline below
        InstructionStream *swapStream(InstructionStream *iCode) { InstructionStream *t = forConditionStream; forConditionStream = iCode; return t; }
        InstructionStream *swapStream2(InstructionStream *iCode) { InstructionStream *t = forIncrementStream; forIncrementStream = iCode; return t; }
        
        virtual int32 getBreakLabel(ICodeGenerator *icg)    { if (breakLabel == -1) breakLabel = icg->getLabel(); return breakLabel; }
        virtual int32 getContinueLabel(ICodeGenerator *icg) { icg; ASSERT(false); return 0; /*if (continueLabel == -1) continueLabel = icg->getLabel(); return whileCondition;*/ }

        int32 forCondition;
        int32 forBody;
        InstructionStream *forConditionStream;
        InstructionStream *forIncrementStream;
    };

    class IfCodeState : public ICodeState {
    public:
        IfCodeState(int32 a, int32 b, ICodeGenerator *icg) 
                    : ICodeState(If_state, icg), elseLabel(a), beyondElse(b) { }
        int32 elseLabel;
        int32 beyondElse;
    };

    class DoCodeState : public ICodeState {
    public:
        DoCodeState(int32 bodyLabel, int32 conditionLabel, ICodeGenerator *icg) 
                    : ICodeState(Do_state, icg), doBody(bodyLabel), doCondition(conditionLabel) { }

        virtual int32 getBreakLabel(ICodeGenerator *icg)    { if (breakLabel == -1) breakLabel = icg->getLabel(); return breakLabel; }
        virtual int32 getContinueLabel(ICodeGenerator *)    { return doCondition; }

        int32 doBody;
        int32 doCondition;
    };

    class SwitchCodeState : public ICodeState {
    public:
        SwitchCodeState(Register control, ICodeGenerator *icg);        // inline below
        InstructionStream *swapStream(InstructionStream *iCode) { InstructionStream *t = caseStatementsStream; caseStatementsStream = iCode; return t; }
        
        virtual int32 getBreakLabel(ICodeGenerator *icg)    { if (breakLabel == -1) breakLabel = icg->getLabel(); return breakLabel; }

        Register controlExpression;
        InstructionStream *caseStatementsStream;
        int32 defaultLabel;
    };

    inline ICodeState::ICodeState(StateKind kind, ICodeGenerator *icg) 
                    : stateKind(kind), breakLabel(-1), continueLabel(-1), registerBase(icg->getRegisterBase()) { }

    inline SwitchCodeState::SwitchCodeState(Register control, ICodeGenerator *icg)
                    : ICodeState(Switch_state, icg), controlExpression(control), defaultLabel(-1), caseStatementsStream(icg->get_iCode()) {}

    inline WhileCodeState::WhileCodeState(int32 conditionLabel, int32 bodyLabel, ICodeGenerator *icg) 
                    : ICodeState(While_state, icg), whileCondition(conditionLabel), whileBody(bodyLabel),
                            whileExpressionStream(icg->get_iCode()) { }

    inline ForCodeState::ForCodeState(int32 conditionLabel, int32 bodyLabel, ICodeGenerator *icg) 
                    : ICodeState(For_state, icg), forCondition(conditionLabel), forBody(bodyLabel), 
                            forConditionStream(icg->get_iCode()), forIncrementStream(icg->get_iCode()) { }
}
#endif
