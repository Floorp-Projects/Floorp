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

    ICodeModule *ICodeGenerator::complete()
    {
#ifdef DEBUG
        ASSERT(stitcher.empty());
        for (LabelList::iterator i = labels.begin();
             i != labels.end(); i++) {
            ASSERT((*i)->base == iCode);
            ASSERT((*i)->offset <= iCode->size());
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
            
        return new ICodeModule(iCode, maxRegister, maxVariable);
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
        Compare *instr = new Compare (op, dest, source);
        iCode->push_back(instr);
        return dest;
    }
        
    void ICodeGenerator::move(Register destination, Register source)
    {
         Move *instr = new Move(destination, source);
         iCode->push_back(instr);
    } 

    Register ICodeGenerator::op(ICodeOp op, Register source1, 
                                Register source2)
    {
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

    /********************************************************************/

    Label *ICodeGenerator::getLabel()
    {
        labels.push_back(new Label(NULL));
        return labels.back();
    }

    void ICodeGenerator::setLabel(Label *l)
    {
        l->base = iCode;
        l->offset = static_cast<int32>(iCode->size());
    }

    void ICodeGenerator::setLabel(InstructionStream *stream, Label *l)
    {
        l->base = stream;
        l->offset = static_cast<int32>(stream->size());
    }

    /********************************************************************/

    void ICodeGenerator::mergeStream(InstructionStream *sideStream)
    {
        // change InstructionStream to be a class that also remembers
        // if it contains any labels (maybe even remembers the labels
        // themselves?) in order to avoid running this loop unnecessarily.
        for (LabelList::iterator i = labels.begin();
             i != labels.end(); i++) {
            if ((*i)->base == sideStream) {
                (*i)->base = iCode;
                (*i)->offset += iCode->size();
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
        resetTopRegister();
  
        // insert a branch to the while condition, which we're 
        // moving to follow the while block
        Label *whileConditionTop = getLabel();
        Label *whileBlockStart = getLabel();
        branch(whileConditionTop);
    
        // save off the current stream while we gen code for the condition
        stitcher.push_back(new WhileCodeState(whileConditionTop, 
                                              whileBlockStart, this));

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

        resetTopRegister();
    }

    /********************************************************************/

    void ICodeGenerator::beginForStatement(uint32)
    {
        Label *forCondition = getLabel();

        ForCodeState *ics = new ForCodeState(forCondition, getLabel(), this);

        branch(forCondition);

        stitcher.push_back(ics);

        // begin the stream for collecting the condition expression
        iCode = new InstructionStream();
        setLabel(forCondition);

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
    }

    /********************************************************************/

    void ICodeGenerator::beginDoStatement(uint32)
    {
        resetTopRegister();
  
        // mark the top of the loop body
        // and reserve a label for the condition
        Label *doBlock = getLabel();
        Label *doCondition = getLabel();
        setLabel(doBlock);
    
        stitcher.push_back(new DoCodeState(doBlock, doCondition, this));

        iCode = new InstructionStream();
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

        resetTopRegister();
    }

    /********************************************************************/

    void ICodeGenerator::beginSwitchStatement(uint32, Register expression)
    {
        // stash the control expression value
        resetTopRegister();
        Register control = op(MOVE, expression);
        // build an instruction stream for the case statements, the case
        // expressions are generated into the main stream directly, the
        // case statements are then added back in afterwards.
        InstructionStream *x = new InstructionStream();
        SwitchCodeState *ics = new SwitchCodeState(control, this);
        ics->swapStream(x);
        stitcher.push_back(ics);
    }

    void ICodeGenerator::endCaseCondition(Register expression)
    {
        SwitchCodeState *ics =
            static_cast<SwitchCodeState *>(stitcher.back());
        ASSERT(ics->stateKind == Switch_state);

        Label *caseLabel = getLabel();
        Register r = op(COMPARE_EQ, expression, ics->controlExpression);
        branchConditional(caseLabel, r);

        // mark the case in the Case Statement stream 
        setLabel(ics->caseStatementsStream, caseLabel);
        resetTopRegister();
    }

    void ICodeGenerator::beginCaseStatement()
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

    void ICodeGenerator::beginDefaultStatement()
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
    }


    /********************************************************************/

    void ICodeGenerator::beginIfStatement(uint32, Register condition)
    {
        Label *elseLabel = getLabel();
    
        stitcher.push_back(new IfCodeState(elseLabel, NULL, this));

        Register notCond = op(NOT, condition);
        branchConditional(elseLabel, notCond);

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
        resetTopRegister();
    }

    /************************************************************************/

    void ICodeGenerator::breakStatement()
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

    void ICodeGenerator::continueStatement()
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

    Formatter& ICodeGenerator::print(Formatter& f)
    {
        f << "ICG! " << (uint32)iCode->size() << "\n";
        for (InstructionIterator i = iCode->begin(); 
             i != iCode->end(); i++) {
            bool isLabel = false;

            for (LabelList::iterator k = labels.begin(); 
                 k != labels.end(); k++)
                if ((ptrdiff_t)(*k)->offset == (i - iCode->begin())) {
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
    
}; // namespace JavaScript
