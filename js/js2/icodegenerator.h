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
    
    class ICodeState {
    public :
        ICodeState(StateKind kind, ICodeGenerator *icg); // inline below
        virtual ~ICodeState()   { }
        
        virtual Label *getBreakLabel(ICodeGenerator *) \
        { ASSERT(false); return NULL; }
        virtual Label *getContinueLabel(ICodeGenerator *) \
        { ASSERT(false); return NULL;}
        
        StateKind stateKind;
        Register registerBase;
        uint32 statementLabelBase;
        Label *breakLabel;
        Label *continueLabel;
    };
    
    class ICodeModule {
    public:
        ICodeModule(InstructionStream *iCode, uint32 maxRegister) :
            its_iCode(iCode), itsMaxRegister(maxRegister) { }
        
        InstructionStream *its_iCode;
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
        std::vector<const StringAtom *> statementLabels;
        
        void markMaxRegister() \
        { if (topRegister > maxRegister) maxRegister = topRegister; }
        
        Register getRegister() \
        { return topRegister++; }
        void resetTopRegister() \
        { markMaxRegister(); topRegister = stitcher.empty() ? registerBase :
            stitcher.back()->registerBase; }
        
        void addStitcher(ICodeState *ics) \
	{ stitcher.push_back(ics); statementLabelBase = statementLabels.size(); }
	
	ICodeOp getBranchOp() \
        { return (iCode->empty()) ? NOP : iCode->back()->getBranchOp(); }

        Register topRegister;
        Register registerBase;        
        uint32   maxRegister;
	    uint32   statementLabelBase;
        
        void setLabel(Label *label);
        void setLabel(InstructionStream *stream, Label *label);
        
        void branch(Label *label);
        void branchConditional(Label *label, Register condition);
        void branchNotConditional(Label *label, Register condition);
        
        void beginTry(Label *catchLabel);

    public:
        ICodeGenerator() : topRegister(0), registerBase(0), maxRegister(0), statementLabelBase(0)
        { iCode = new InstructionStream(); }
        
        virtual ~ICodeGenerator() { if (iCode) delete iCode; }
        
        void mergeStream(InstructionStream *sideStream);
        
        ICodeModule *complete();

        Register allocateVariable(StringAtom& /*name*/) 
        { Register result = getRegister(); registerBase = topRegister; return result; }
        
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
        
        Register getRegisterBase()      { return topRegister; }
        InstructionStream *get_iCode()  { return iCode; }
        uint32 getStatementLabelBase()  { return statementLabelBase; }
        
        Label *getLabel();
        
        
        // Rather than have the ICG client maniplate labels and branches, it
        // uses the following calls to describe the high level looping
        // constructs being generated. The functions listed below are
        // expected to be called in the order listed for each construct,
        // (internal error otherwise).
        // The ICG will enforce correct nesting and closing.
        
        // expression statements
        void beginStatement(uint32 /*pos*/) { resetTopRegister(); }
        
        void returnStatement() { iCode->push_back(new ReturnVoid()); }
        void returnStatement(Register result) \
            { iCode->push_back(new Return(result)); }
        
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
        { statementLabels.push_back(&label); }
        void endLabelStatement() { statementLabels.pop_back(); }

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
        void endCatchExpression(Register expression);
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

        Register controlExpression;
        Label *defaultLabel;
        InstructionStream *caseStatementsStream;
    };

    class TryCodeState : public ICodeState {
    public:
        TryCodeState(Label *catchLabel, Label *finallyLabel, ICodeGenerator *icg) 
            : ICodeState(Try_state, icg), 
                    catchHandler(catchLabel), 
                    finallyHandler(finallyLabel),
                    beyondCatch(NULL) 
                { if (catchHandler != NULL) beyondCatch = icg->getLabel(); }
        Label *catchHandler;
        Label *finallyHandler;
        Label *beyondCatch;
    };

    inline ICodeState::ICodeState(StateKind kind, ICodeGenerator *icg) 
        : stateKind(kind), registerBase(icg->getRegisterBase()),
          statementLabelBase(icg->getStatementLabelBase()), breakLabel(NULL),
          continueLabel(NULL) { }

    inline SwitchCodeState::SwitchCodeState(Register control,
                                            ICodeGenerator *icg)
        : ICodeState(Switch_state, icg), controlExpression(control),
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
