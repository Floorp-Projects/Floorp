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

#ifndef icodegenerator_h
#define icodegenerator_h

#include <vector>
#include <stack>
#include <iostream>

#include "utilities.h"
#include "parser.h"
#include "vmtypes.h"


#define NS_JSICG JavaScript::IGC

namespace JavaScript {
namespace ICG {
    
    using namespace VM;
    
    class ICodeGenerator; // forward declaration
    
    enum StateKind {
        While_state,
        If_state,
        Do_state,
        Switch_state,
        For_state,
        Try_state
    };

    typedef std::vector<const StringAtom *> StatementLabels;
    
    class ICodeState {
    public :
        ICodeState(StateKind kind, ICodeGenerator *icg); // inline below
        virtual ~ICodeState()   { }
        
        virtual Label *getBreakLabel(ICodeGenerator *) \
        { ASSERT(false); return NULL; }
        virtual Label *getContinueLabel(ICodeGenerator *) \
        { ASSERT(false); return NULL;}
        
        StateKind stateKind;
        uint32 statementLabelBase;
        Label *breakLabel;
        Label *continueLabel;
        StatementLabels *labelSet;
    };

    typedef std::map<String, Register, std::less<String> > VariableList;
    

    class ICodeModule {
    public:
        ICodeModule(InstructionStream *iCode, uint32 maxRegister) :
            its_iCode(iCode), itsMaxRegister(maxRegister) { }
        
        InstructionStream *its_iCode;
        VariableList *itsVariables;
        uint32  itsParameterCount;
        uint32  itsMaxRegister;
    };
    
    /****************************************************************/
    
    // An ICodeGenerator provides the interface between the parser and the
    // interpreter. The parser constructs one of these for each
    // function/script, adds statements and expressions to it and then
    // converts it into an ICodeModule, ready for execution.
    
    class ICodeGenerator {
    private:
        InstructionStream *iCode;
        LabelList labels;
        std::vector<ICodeState *> stitcher;
        StatementLabels *labelSet;
        
        Register topRegister;           // highest (currently) alloacated register
        Register registerBase;          // start of registers available for expression temps
        uint32   maxRegister;           // highest (ever) allocated register
        uint32   parameterCount;        // number of parameters declared for the function
                                        // these must come before any variables declared.
        Register exceptionRegister;     // reserved to carry the exception object, only has a value
                                        // for functions that contain try/catch statements.
        Register switchRegister;        // register containing switch control value for most 
                                        // recently in progress switch statement.
        VariableList *variableList;     // name|register pair for each variable
        
        
        
        void markMaxRegister() \
        { if (topRegister > maxRegister) maxRegister = topRegister; }
        
        Register getRegister() \
        { return topRegister++; }

        void resetTopRegister() \
        { markMaxRegister(); topRegister = registerBase; }
        
        void addStitcher(ICodeState *ics) \
	    { stitcher.push_back(ics); }
	
	    ICodeOp getBranchOp() \
        { return (iCode->empty()) ? NOP : iCode->back()->getBranchOp(); }


        void setLabel(Label *label);
        void setLabel(InstructionStream *stream, Label *label);
        
        void jsr(Label *label)  { iCode->push_back(new Jsr(label)); }
        void rts()              { iCode->push_back(new Rts()); }
        void branch(Label *label);
        void branchConditional(Label *label, Register condition);
        void branchNotConditional(Label *label, Register condition);
        
        void beginTry(Label *catchLabel, Label *finallyLabel)
            { iCode->push_back(new Try(catchLabel, finallyLabel)); }
        void endTry()
            { iCode->push_back(new Endtry()); }
        
        void resetStatement()
        { if (labelSet) { delete labelSet; labelSet = NULL; } resetTopRegister(); }

    public:
        ICodeGenerator(World *world = NULL, 
                            bool hasTryStatement = false, 
                            uint32 switchStatementNesting = 0);
        
        virtual ~ICodeGenerator() { if (iCode) delete iCode; }
        
        void mergeStream(InstructionStream *sideStream);
        
        ICodeModule *complete();

        Register allocateVariable(StringAtom& name) 
        { Register result = getRegister(); (*variableList)[name] = result; 
            registerBase = topRegister; return result; }

        Register allocateParameter(StringAtom& name) 
        { parameterCount++; return allocateVariable(name); }
        
        Formatter& print(Formatter& f);
        
        Register op(ICodeOp op, Register source);
        Register op(ICodeOp op, Register source1, Register source2);
        Register call(Register target, RegisterList args);

        void move(Register destination, Register source);
        void complement(Register destination, Register source);
        
        Register compare(ICodeOp op, Register source1, Register source2);
        
        Register loadImmediate(double value);
                
        Register newObject();
        Register newArray();
        
        Register loadName(StringAtom &name);
        void saveName(StringAtom &name, Register value);
        
        Register getProperty(Register base, StringAtom &name);
        void setProperty(Register base, StringAtom &name, Register value);
        
        Register getElement(Register base, Register index);
        void setElement(Register base, Register index, Register value);
        
        Register getRegisterBase()                  { return topRegister; }
        InstructionStream *get_iCode()              { return iCode; }
        StatementLabels *getStatementLabels()       { return labelSet; labelSet = NULL; }

        
        Label *getLabel();
        
        
        // Rather than have the ICG client maniplate labels and branches, it
        // uses the following calls to describe the high level looping
        // constructs being generated. The functions listed below are
        // expected to be called in the order listed for each construct,
        // (internal error otherwise).
        // The ICG will enforce correct nesting and closing.
        
        // expression statements
        void beginStatement(uint32 /*pos*/) { }
        void endStatement() { resetStatement(); }
        
        void returnStatement() { iCode->push_back(new ReturnVoid()); }
        void returnStatement(Register result) \
            { iCode->push_back(new Return(result)); resetStatement(); }
        
        void beginWhileStatement(uint32 pos);
        void endWhileExpression(Register condition);
        void endWhileStatement();
        
        void beginDoStatement(uint32 pos);
        void endDoStatement();
        void endDoExpression(Register condition);
        
        void beginIfStatement(uint32 pos, Register condition);
        void beginElseStatement(bool hasElse); // required, regardless of
        // existence of else clause
        void endIfStatement();
        
        // for ( ... in ...) statements get turned into generic for
        // statements by the parser (ok?)
        void beginForStatement(uint32 pos); // for initialization is
        // emitted prior to this call
        void forCondition(Register condition); // required
        void forIncrement();                   // required
        void endForStatement();
        
        void beginSwitchStatement(uint32 pos, Register expression);

        void endCaseCondition(Register expression);
    
        void beginCaseStatement(uint32 pos);
        void endCaseStatement();
        
        // optionally
        void beginDefaultStatement(uint32 pos);
        void endDefaultStatement();

        void endSwitchStatement();

        void beginLabelStatement(uint32 /* pos */, const StringAtom &label)
        { labelSet->push_back(&label); }
        void endLabelStatement() { labelSet->pop_back(); }

        void continueStatement(uint32 pos);
        void breakStatement(uint32 pos);

        void continueStatement(uint32 pos, const StringAtom &label);
        void breakStatement(uint32 pos, const StringAtom &label);

        void throwStatement(uint32 /* pos */, Register expression)
            { iCode->push_back(new Throw(expression)); }

        void beginTryStatement(uint32 pos, bool hasCatch, bool hasFinally);
        void endTryBlock();
        void endTryStatement();

        void beginCatchStatement(uint32 pos);
        void endCatchExpression(Register exceptionId);
        void endCatchStatement();

        void beginFinallyStatement(uint32 pos);
        void endFinallyStatement();

    };

    Formatter& operator<<(Formatter &f, ICodeGenerator &i);
    /*
      std::ostream &operator<<(std::ostream &s, ICodeGenerator &i);
      std::ostream &operator<<(std::ostream &s, StringAtom &str);
    */

    class WhileCodeState : public ICodeState {
    public:
        WhileCodeState(ICodeGenerator *icg); // inline below
        InstructionStream *swapStream(InstructionStream *iCode) \
        { InstructionStream *t = whileExpressionStream; \
        whileExpressionStream = iCode; return t; }

        virtual Label *getBreakLabel(ICodeGenerator *icg) \
        { if (breakLabel == NULL) breakLabel = icg->getLabel(); \
        return breakLabel; }
        virtual Label *getContinueLabel(ICodeGenerator *) \
        { return whileCondition; }

        Label *whileCondition;
        Label *whileBody;
        InstructionStream *whileExpressionStream;
    };

    class ForCodeState : public ICodeState {
    public:
        ForCodeState(ICodeGenerator *icg); // inline below
        InstructionStream *swapStream(InstructionStream *iCode) \
        { InstructionStream *t = forConditionStream; \
        forConditionStream = iCode; return t; }
        InstructionStream *swapStream2(InstructionStream *iCode) \
        { InstructionStream *t = forIncrementStream; \
        forIncrementStream = iCode; return t; }
        
        virtual Label *getBreakLabel(ICodeGenerator *icg) \
        { if (breakLabel == NULL) breakLabel = icg->getLabel(); \
        return breakLabel; }
        virtual Label *getContinueLabel(ICodeGenerator *) \
        { ASSERT(continueLabel); return continueLabel; }

        Label *forCondition;
        Label *forBody;
        InstructionStream *forConditionStream;
        InstructionStream *forIncrementStream;
    };

    class IfCodeState : public ICodeState {
    public:
        IfCodeState(ICodeGenerator *icg) 
            : ICodeState(If_state, icg), elseLabel(icg->getLabel()), beyondElse(NULL) { }
        Label *elseLabel;
        Label *beyondElse;
    };

    class DoCodeState : public ICodeState {
    public:
        DoCodeState(ICodeGenerator *icg) 
            : ICodeState(Do_state, icg), doBody(icg->getLabel()),
              doCondition(icg->getLabel()) { }

        virtual Label *getBreakLabel(ICodeGenerator *icg) \
        { if (breakLabel == NULL) breakLabel = icg->getLabel();
        return breakLabel; }
        virtual Label *getContinueLabel(ICodeGenerator *) \
        { return doCondition; }

        Label *doBody;
        Label *doCondition;
    };

    class SwitchCodeState : public ICodeState {
    public:
        SwitchCodeState(Register control, ICodeGenerator *icg); // inline below
        InstructionStream *swapStream(InstructionStream *iCode) \
        { InstructionStream *t = caseStatementsStream; \
        caseStatementsStream = iCode; return t; }
        
        virtual Label *getBreakLabel(ICodeGenerator *icg) \
        { if (breakLabel == NULL) breakLabel = icg->getLabel(); 
        return breakLabel; }

        Register controlValue;
        Label *defaultLabel;
        InstructionStream *caseStatementsStream;
    };

    class TryCodeState : public ICodeState {
    public:
        TryCodeState(Label *catchLabel, Label *finallyLabel, ICodeGenerator *icg);
        Label *catchHandler;
        Label *finallyHandler;
        Label *finallyInvoker;
        Label *beyondCatch;
    };

    inline ICodeState::ICodeState(StateKind kind, ICodeGenerator *icg) 
        : stateKind(kind),         
          breakLabel(NULL), continueLabel(NULL), 
          labelSet(icg->getStatementLabels()) { }

    inline SwitchCodeState::SwitchCodeState(Register control,
                                            ICodeGenerator *icg)
        : ICodeState(Switch_state, icg), controlValue(control),
          defaultLabel(NULL), caseStatementsStream(icg->get_iCode()) {}

    inline WhileCodeState::WhileCodeState(ICodeGenerator *icg) 
        : ICodeState(While_state, icg), whileCondition(icg->getLabel()),
          whileBody(icg->getLabel()), whileExpressionStream(icg->get_iCode()) {}

    inline ForCodeState::ForCodeState(ICodeGenerator *icg) 
        : ICodeState(For_state, icg), forCondition(icg->getLabel()),
          forBody(icg->getLabel()), forConditionStream(icg->get_iCode()),
          forIncrementStream(icg->get_iCode()) {}


} /* namespace IGC */
} /* namespace JavaScript */

#endif /* icodegenerator_h */
