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

#include "numerics.h"
#include "world.h"
#include "vmtypes.h"
#include "jstypes.h"
#include "icodegenerator.h"

#include <iomanip>
#include <stdexcept>


namespace JavaScript {
namespace ICG {

    using namespace VM;

        
    Formatter& operator<<(Formatter &f, ICodeGenerator &i)
    {
        return i.print(f);
    }
        
    //
    // ICodeGenerator
    //

    ICodeGenerator::ICodeGenerator(World *world, bool hasTryStatement, uint32 switchStatementNesting)
        :   topRegister(0), 
            registerBase(0), 
            maxRegister(0), 
            parameterCount(0),
            exceptionRegister(NotARegister),
            switchRegister(NotARegister),
            variableList(new VariableList())
    { 
        iCode = new InstructionStream(); 
        if (hasTryStatement) 
            exceptionRegister = allocateVariable(world->identifiers[widenCString("__exceptionObject__")]);
        for (uint i = 0; i < switchStatementNesting; i++) {
            String s = widenCString("__switchControlVariable__");
            char num[8]; 
            sprintf(num, "%.2d", i);
            appendChars(s, num, strlen(num));
            if (switchRegister == NotARegister)
                switchRegister = allocateVariable(world->identifiers[s]);
            else
                allocateVariable(world->identifiers[s]);
        }
        labelSet = new StatementLabels();
    }


    ICodeModule *ICodeGenerator::complete()
    {
#ifdef DEBUG
        ASSERT(stitcher.empty());
        // ASSERT(statementLabels.empty());
        for (LabelList::iterator i = labels.begin();
             i != labels.end(); i++) {
            ASSERT((*i)->mBase == iCode);
            ASSERT((*i)->mOffset <= iCode->size());
        }
#endif
        /*
        for (InstructionIterator ii = iCode->begin(); 
             ii != iCode->end(); ii++) {            
            if ((*ii)->op() == BRANCH) {
                Instruction *t = *ii;
                *ii = new ResolvedBranch(static_cast<Branch *>(*ii)->operand1->itsOffset);
                delete t;
            }
            else 
                if ((*ii)->itsOp >= BRANCH_LT && (*ii)->itsOp <= BRANCH_GT) {
                    Instruction *t = *ii;
                    *ii = new ResolvedBranchCond((*ii)->itsOp, 
                                                 static_cast<BranchCond *>(*ii)->itsOperand1->itsOffset,
                                                 static_cast<BranchCond *>(*ii)->itsOperand2);
                    delete t;
                }
        }
        */
        markMaxRegister();            
        return new ICodeModule(iCode, maxRegister);
    }
    
    TryCodeState::TryCodeState(Label *catchLabel, Label *finallyLabel, ICodeGenerator *icg) 
        : ICodeState(Try_state, icg), 
                catchHandler(catchLabel), 
                finallyHandler(finallyLabel),
                finallyInvoker(NULL),
                beyondCatch(NULL)
    { 
        if (catchHandler) {
            beyondCatch = icg->getLabel();
            if (finallyLabel)
                finallyInvoker = icg->getLabel();
        }
    }


    /********************************************************************/

    Register ICodeGenerator::loadImmediate(double value)
    {
        Register dest = getRegister();
        LoadImmediate *instr = new LoadImmediate(dest, value);
        iCode->push_back(instr);
        return dest;
    }

    Register ICodeGenerator::newObject()
    {
        Register dest = getRegister();
        NewObject *instr = new NewObject(dest);
        iCode->push_back(instr);
        return dest;
    }

    Register ICodeGenerator::newArray()
    {
        Register dest = getRegister();
        NewArray *instr = new NewArray(dest);
        iCode->push_back(instr);
        return dest;
    }

    Register ICodeGenerator::loadName(StringAtom &name)
    {
        Register dest = getRegister();
        LoadName *instr = new LoadName(dest, &name);
        iCode->push_back(instr);
        return dest;
    }
    
    void ICodeGenerator::saveName(StringAtom &name, Register value)
    {
        SaveName *instr = new SaveName(&name, value);
        iCode->push_back(instr);
    }

    Register ICodeGenerator::getProperty(Register base, StringAtom &name)
    {
        Register dest = getRegister();
        GetProp *instr = new GetProp(dest, base, &name);
        iCode->push_back(instr);
        return dest;
    }

    void ICodeGenerator::setProperty(Register base, StringAtom &name,
                                     Register value)
    {
        SetProp *instr = new SetProp(base, &name, value);
        iCode->push_back(instr);
    }

    Register ICodeGenerator::getElement(Register base, Register index)
    {
        Register dest = getRegister();
        GetElement *instr = new GetElement(dest, base, index);
        iCode->push_back(instr);
        return dest;
    }

    void ICodeGenerator::setElement(Register base, Register index,
                                    Register value)
    {
        SetElement *instr = new SetElement(base, index, value);
        iCode->push_back(instr);
    }

    Register ICodeGenerator::op(ICodeOp op, Register source)
    {
        Register dest = getRegister();
        ASSERT(source != NotARegister);    
        Compare *instr = new Compare (op, dest, source);
        iCode->push_back(instr);
        return dest;
    }
        
    void ICodeGenerator::move(Register destination, Register source)
    {
        ASSERT(destination != NotARegister);    
        ASSERT(source != NotARegister);    
        Move *instr = new Move(destination, source);
        iCode->push_back(instr);
    } 

    void ICodeGenerator::complement(Register destination, Register source)
    {
        Not *instr = new Not(destination, source);
        iCode->push_back(instr);
    } 

    Register ICodeGenerator::op(ICodeOp op, Register source1, 
                                Register source2)
    {
        ASSERT(source1 != NotARegister);    
        ASSERT(source2 != NotARegister);    
        Register dest = getRegister();
        Arithmetic *instr = new Arithmetic(op, dest, source1, source2);
        iCode->push_back(instr);
        return dest;
    } 
        
    Register ICodeGenerator::call(Register target, RegisterList args)
    {
        Register dest = getRegister();
        Call *instr = new Call(dest, target, args);
        iCode->push_back(instr);
        return dest;
    }

    void ICodeGenerator::branch(Label *label)
    {
        Branch *instr = new Branch(label);
        iCode->push_back(instr);
    }

    void ICodeGenerator::branchConditional(Label *label, Register condition)
    {
        ICodeOp branchOp = getBranchOp();
        if (branchOp == NOP) {
            // XXXX emit convert to boolean / Test / ...
            branchOp = BRANCH_NE;
        }
        GenericBranch *instr = new GenericBranch(branchOp, label, condition);
        iCode->push_back(instr);
    }

    void ICodeGenerator::branchNotConditional(Label *label, Register condition)
    {
        ICodeOp branchOp = getBranchOp();
        if (branchOp == NOP) {
            // XXXX emit convert to boolean / Test / ...
            branchOp = BRANCH_NE;
        }
        switch (branchOp) {
            case BRANCH_EQ : branchOp = BRANCH_NE; break;
            case BRANCH_GE : branchOp = BRANCH_LT; break;
            case BRANCH_GT : branchOp = BRANCH_LE; break;
            case BRANCH_LE : branchOp = BRANCH_GT; break;
            case BRANCH_LT : branchOp = BRANCH_GE; break;
            case BRANCH_NE : branchOp = BRANCH_EQ; break;
            default : NOT_REACHED("Expected a branch op"); break;
        }
        GenericBranch *instr = new GenericBranch(branchOp, label, condition);
        iCode->push_back(instr);
    }

    /********************************************************************/

    Label *ICodeGenerator::getLabel()
    {
        labels.push_back(new Label(NULL));
        return labels.back();
    }

    void ICodeGenerator::setLabel(Label *l)
    {
        l->mBase = iCode;
        l->mOffset = iCode->size();
    }

    void ICodeGenerator::setLabel(InstructionStream *stream, Label *l)
    {
        l->mBase = stream;
        l->mOffset = stream->size();
    }

    /********************************************************************/

    void ICodeGenerator::mergeStream(InstructionStream *sideStream)
    {
        // change InstructionStream to be a class that also remembers
        // if it contains any labels (maybe even remembers the labels
        // themselves?) in order to avoid running this loop unnecessarily.
        for (LabelList::iterator i = labels.begin();
             i != labels.end(); i++) {
            if ((*i)->mBase == sideStream) {
                (*i)->mBase = iCode;
                (*i)->mOffset += iCode->size();
            }
        }

        for (InstructionIterator ii = sideStream->begin();
             ii != sideStream->end(); ii++) {
            iCode->push_back(*ii);
        }

    }

    /********************************************************************/

    void ICodeGenerator::beginWhileStatement(uint32)
    {
        WhileCodeState *ics = new WhileCodeState(this);
        addStitcher(ics);

        // insert a branch to the while condition, which we're 
        // moving to follow the while block
        branch(ics->whileCondition);

        iCode = new InstructionStream();
    }

    void ICodeGenerator::endWhileExpression(Register condition)
    {
        WhileCodeState *ics = static_cast<WhileCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == While_state);

        branchConditional(ics->whileBody, condition);
        resetTopRegister();
        // stash away the condition expression and switch 
        // back to the main stream
        iCode = ics->swapStream(iCode);
        // mark the start of the while block
        setLabel(ics->whileBody);
    }
  
    void ICodeGenerator::endWhileStatement()
    {
        // recover the while stream
        WhileCodeState *ics = static_cast<WhileCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == While_state);
        stitcher.pop_back();

            // mark the start of the condition code
            // which is where continues will target
        setLabel(ics->whileCondition);
        
        // and re-attach it to the main stream
        mergeStream(ics->whileExpressionStream);

        if (ics->breakLabel != NULL)
            setLabel(ics->breakLabel);

        delete ics;

        resetStatement();
    }

    /********************************************************************/

    void ICodeGenerator::beginForStatement(uint32)
    {
        ForCodeState *ics = new ForCodeState(this);
        addStitcher(ics);
        branch(ics->forCondition);

        // begin the stream for collecting the condition expression
        iCode = new InstructionStream();
        setLabel(ics->forCondition);

        resetTopRegister();
    }

    void ICodeGenerator::forCondition(Register condition)
    {
        ForCodeState *ics = static_cast<ForCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == For_state);

        // finsh off the test expression by adding the branch to the body
        branchConditional(ics->forBody, condition);

        // switch back to main stream
        iCode = ics->swapStream(iCode);
        // begin the stream for collecting the increment expression
        iCode = new InstructionStream();

        ics->continueLabel = getLabel();
        // can't lazily insert this since we haven't seen the body yet
        // ??? could just remember the offset
        setLabel(ics->continueLabel);       

        resetTopRegister();
    }

    void ICodeGenerator::forIncrement()
    {
        ForCodeState *ics = static_cast<ForCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == For_state);

        // now switch back to the main stream
        iCode = ics->swapStream2(iCode);
        setLabel(ics->forBody);
    
        resetTopRegister();
    }

    void ICodeGenerator::endForStatement()
    {
        ForCodeState *ics = static_cast<ForCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == For_state);
        stitcher.pop_back();

        mergeStream(ics->forIncrementStream);
        mergeStream(ics->forConditionStream);

        if (ics->breakLabel != NULL)
            setLabel(ics->breakLabel);

        delete ics;
        resetStatement();
    }

    /********************************************************************/

    void ICodeGenerator::beginDoStatement(uint32)
    {
        DoCodeState *ics = new DoCodeState(this);
        addStitcher(ics);
  
        // mark the top of the loop body
        setLabel(ics->doBody);
    }

    void ICodeGenerator::endDoStatement()
    {
        DoCodeState *ics = static_cast<DoCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Do_state);

        // mark the start of the do conditional
        setLabel(ics->doCondition);
        if (ics->continueLabel != NULL)
            setLabel(ics->continueLabel);

        resetTopRegister();
    }
  
    void ICodeGenerator::endDoExpression(Register condition)
    {
        DoCodeState *ics = static_cast<DoCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Do_state);
        stitcher.pop_back();

        // add branch to top of do block
        branchConditional(ics->doBody, condition);
        if (ics->breakLabel != NULL)
            setLabel(ics->breakLabel);

        delete ics;

        resetStatement();
    }

    /********************************************************************/

    void ICodeGenerator::beginSwitchStatement(uint32, Register expression)
    {
        // stash the control expression value

        // hmmm, need to track depth of nesting here....
        move(switchRegister, expression);

        // build an instruction stream for the case statements, the case
        // expressions are generated into the main stream directly, the
        // case statements are then added back in afterwards.
        InstructionStream *x = new InstructionStream();
        SwitchCodeState *ics = new SwitchCodeState(switchRegister++, this);
        ics->swapStream(x);
        addStitcher(ics);
    }

    void ICodeGenerator::endCaseCondition(Register expression)
    {
        SwitchCodeState *ics =
            static_cast<SwitchCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Switch_state);

        Label *caseLabel = getLabel();
        Register r = op(COMPARE_EQ, expression, ics->controlValue);
        branchConditional(caseLabel, r);

        // mark the case in the Case Statement stream 
        setLabel(ics->caseStatementsStream, caseLabel);
        resetTopRegister();
    }

    void ICodeGenerator::beginCaseStatement(uint32 /* pos */)
    {
        SwitchCodeState *ics = 
            static_cast<SwitchCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Switch_state);
        // switch to Case Statement stream
        iCode = ics->swapStream(iCode);
    }

    void ICodeGenerator::endCaseStatement()
    {
        SwitchCodeState *ics = 
            static_cast<SwitchCodeState *>(stitcher.back());
        // do more to guarantee correct blocking?
        ASSERT(ics->stateKind == Switch_state);
        // switch back to Case Conditional stream
        iCode = ics->swapStream(iCode);
        resetTopRegister();
    }

    void ICodeGenerator::beginDefaultStatement(uint32 /* pos */)
    {
        SwitchCodeState *ics = 
            static_cast<SwitchCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Switch_state);
        ASSERT(ics->defaultLabel == NULL);
        ics->defaultLabel = getLabel();
        setLabel(ics->caseStatementsStream, ics->defaultLabel);
        // switch to Case Statement stream
        iCode = ics->swapStream(iCode);
    }

    void ICodeGenerator::endDefaultStatement()
    {
        SwitchCodeState *ics = 
            static_cast<SwitchCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Switch_state);
        // do more to guarantee correct blocking?
        ASSERT(ics->defaultLabel != NULL);
        // switch to Case Statement stream
        iCode = ics->swapStream(iCode);
        resetTopRegister();
    }

    void ICodeGenerator::endSwitchStatement()
    {
        SwitchCodeState *ics = 
            static_cast<SwitchCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Switch_state);
        stitcher.pop_back();

        // ground out the case chain at the default block or fall thru
        // to the break label
        if (ics->defaultLabel != NULL)       
            branch(ics->defaultLabel);
        else {
            if (ics->breakLabel == NULL)
                ics->breakLabel = getLabel();
            branch(ics->breakLabel);
        }
    
        // dump all the case statements into the main stream
        mergeStream(ics->caseStatementsStream);

        if (ics->breakLabel != NULL)
            setLabel(ics->breakLabel);

        delete ics;
        
        --switchRegister;

        resetStatement();
    }


    /********************************************************************/

    void ICodeGenerator::beginIfStatement(uint32, Register condition)
    {
        IfCodeState *ics = new IfCodeState(this);
        addStitcher(ics);

        branchNotConditional(ics->elseLabel, condition);

        resetTopRegister();
    }

    void ICodeGenerator::beginElseStatement(bool hasElse)
    {
        IfCodeState *ics = static_cast<IfCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == If_state);

        if (hasElse) {
            Label *beyondElse = getLabel();
            ics->beyondElse = beyondElse;
            branch(beyondElse);
        }
        setLabel(ics->elseLabel);
        resetTopRegister();
    }

    void ICodeGenerator::endIfStatement()
    {
        IfCodeState *ics = static_cast<IfCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == If_state);
        stitcher.pop_back();

        if (ics->beyondElse != NULL) {   // had an else
            setLabel(ics->beyondElse);   // the beyond else label
        }
    
        delete ics;
        resetStatement();
    }

    /************************************************************************/

    void ICodeGenerator::breakStatement(uint32 /* pos */)
    {
        for (std::vector<ICodeState *>::reverse_iterator p =
                 stitcher.rbegin(); p != stitcher.rend(); p++) {
            if ((*p)->breakLabel != NULL) {
                branch((*p)->breakLabel);
                return;
            }
            if (((*p)->stateKind == While_state)
                || ((*p)->stateKind == Do_state)
                || ((*p)->stateKind == For_state)
                || ((*p)->stateKind == Switch_state)) {
                (*p)->breakLabel = getLabel();
                branch((*p)->breakLabel);
                return;
            }
        }
        NOT_REACHED("no break target available");
    }

    void ICodeGenerator::breakStatement(uint32 /* pos */,
                                        const StringAtom &label)
    {
        for (std::vector<ICodeState *>::reverse_iterator p =
                    stitcher.rbegin(); p != stitcher.rend(); p++) {
            if ((*p)->labelSet) {
                for (StatementLabels::iterator i = (*p)->labelSet->begin();
                            i != (*p)->labelSet->end(); i++) {
                    if ((*i) == &label) {
		                if ((*p)->breakLabel == NULL)
			                (*p)->breakLabel = getLabel();
                        branch((*p)->breakLabel);
                        return;
                    }
                }
            }
        }
        NOT_REACHED("no break target available");
    }

    void ICodeGenerator::continueStatement(uint32 /* pos */)
    {
        for (std::vector<ICodeState *>::reverse_iterator p =
                 stitcher.rbegin(); p != stitcher.rend(); p++) {
            if ((*p)->continueLabel != NULL) {
                branch((*p)->continueLabel);
                return;
            }
            if (((*p)->stateKind == While_state)
                || ((*p)->stateKind == Do_state)
                || ((*p)->stateKind == For_state)) {
                (*p)->continueLabel = getLabel();
                branch((*p)->continueLabel);
                return;
            }
        }
        NOT_REACHED("no continue target available");
    }    

    void ICodeGenerator::continueStatement(uint32 /* pos */,
                                           const StringAtom &label)
    {
        for (std::vector<ICodeState *>::reverse_iterator p =
                    stitcher.rbegin(); p != stitcher.rend(); p++) {
            if ((*p)->labelSet) {
                for (StatementLabels::iterator i = (*p)->labelSet->begin();
                            i != (*p)->labelSet->end(); i++) {
                    if ((*i) == &label) {
        		        if ((*p)->continueLabel == NULL)
		            	    (*p)->continueLabel = getLabel();
                        branch((*p)->continueLabel);
                        return;
                    }
                }
            }
        }
        NOT_REACHED("no continue target available");
    }
    /********************************************************************/

    void ICodeGenerator::beginTryStatement(uint32 /* pos */,
                                           bool hasCatch, bool hasFinally)
    {
        ASSERT(exceptionRegister != NotARegister);
        TryCodeState *ics = new TryCodeState((hasCatch) ? getLabel() : NULL,
                                     (hasFinally) ? getLabel() : NULL, this);
        addStitcher(ics);
        beginTry(ics->catchHandler, ics->finallyInvoker);
    }

    void ICodeGenerator::endTryBlock()
    {
        TryCodeState *ics = static_cast<TryCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Try_state);

        endTry();
        if (ics->finallyHandler)
            jsr(ics->finallyHandler);
        if (ics->beyondCatch)
            branch(ics->beyondCatch);
    }

    void ICodeGenerator::endTryStatement()
    {
        TryCodeState *ics = static_cast<TryCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Try_state);
        stitcher.pop_back();
        if (ics->beyondCatch)
            setLabel(ics->beyondCatch);
        resetStatement();
    }

    void ICodeGenerator::beginCatchStatement(uint32 /* pos */)
    {
        TryCodeState *ics = static_cast<TryCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Try_state);
        ASSERT(ics->catchHandler);
        setLabel(ics->catchHandler);
    }

    void ICodeGenerator::endCatchExpression(Register exceptionId)
    {
        TryCodeState *ics = static_cast<TryCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Try_state);
        move(exceptionRegister, exceptionId);
    }
    
    void ICodeGenerator::endCatchStatement()
    {
        TryCodeState *ics = static_cast<TryCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Try_state);
        if (ics->finallyHandler)
            jsr(ics->finallyHandler);
        throwStatement(0, exceptionRegister);
    }

    void ICodeGenerator::beginFinallyStatement(uint32 /* pos */)
    {
        TryCodeState *ics = static_cast<TryCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Try_state);
        ASSERT(ics->finallyHandler);
        if (ics->finallyInvoker) {
            setLabel(ics->finallyInvoker);
            jsr(ics->finallyHandler);
            throwStatement(0, exceptionRegister);
        }
        setLabel(ics->finallyHandler);
    }

    void ICodeGenerator::endFinallyStatement()
    {
        TryCodeState *ics = static_cast<TryCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Try_state);
        rts();
    }

    /************************************************************************/

    Formatter& ICodeGenerator::print(Formatter& f)
    {
        f << "ICG! " << (uint32)iCode->size() << "\n";
        for (InstructionIterator i = iCode->begin(); 
             i != iCode->end(); i++) {
            bool isLabel = false;

            for (LabelList::iterator k = labels.begin(); 
                 k != labels.end(); k++)
                if ((ptrdiff_t)(*k)->mOffset == (i - iCode->begin())) {
                    f << "#" << (uint32)(i - iCode->begin()) << "\t";
                    isLabel = true;
                    break;
                }

            if (!isLabel)
                f << "\t";
                
            f << **i << "\n";
        }

        return f;
    }
        
} // namespace ICG
    
} // namespace JavaScript
