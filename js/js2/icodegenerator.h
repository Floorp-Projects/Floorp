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

#include "utilities.h"
#include "parser.h"
#include "vmtypes.h"


namespace JavaScript {
namespace ICG {
    
    using namespace VM;
    
    
    typedef std::map<String, Register, std::less<String> > VariableList;
    

    class ICodeModule {
    public:
        ICodeModule(InstructionStream *iCode, VariableList *variables,
                    uint32 maxRegister, uint32 maxParameter) :
            its_iCode(iCode), itsVariables(variables),
            itsParameterCount(maxParameter), itsMaxRegister(maxRegister),
            mID(++sMaxID) { }
        ~ICodeModule()
        {
            delete its_iCode;
            delete itsVariables;
        }

        Formatter& print(Formatter& f);
        
        InstructionStream *its_iCode;
        VariableList *itsVariables;
        uint32 itsParameterCount;
        uint32 itsMaxRegister;
        uint32 mID;
        static uint32 sMaxID;
        
    };

    typedef std::vector<const StringAtom *> LabelSet;
    class LabelEntry {
    public:
        LabelEntry(LabelSet *labelSet, Label *breakLabel) 
            : labelSet(labelSet), breakLabel(breakLabel), continueLabel(NULL) { }
        LabelEntry(LabelSet *labelSet, Label *breakLabel, Label *continueLabel) 
            : labelSet(labelSet), breakLabel(breakLabel), continueLabel(continueLabel) { }

        bool containsLabel(const StringAtom *label);

        LabelSet *labelSet;
        Label *breakLabel;
        Label *continueLabel;
    };
    typedef std::vector<LabelEntry *> LabelStack;

    Formatter& operator<<(Formatter &f, ICodeModule &i);

    /****************************************************************/
    
    // An ICodeGenerator provides the interface between the parser and the
    // interpreter. The parser constructs one of these for each
    // function/script, adds statements and expressions to it and then
    // converts it into an ICodeModule, ready for execution.
    
    class ICodeGenerator {
    private:
        InstructionStream *iCode;
        bool iCodeOwner;
        LabelList labels;
        
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

        World *mWorld;                  // used to register strings
        
        LabelStack mLabelStack;
        
        void markMaxRegister() \
        { if (topRegister > maxRegister) maxRegister = topRegister; }
        
        Register getRegister() \
        { return topRegister++; }

        void resetTopRegister() \
        { markMaxRegister(); topRegister = registerBase; }
        
	    ICodeOp getBranchOp() \
        { return (iCode->empty()) ? NOP : iCode->back()->getBranchOp(); }


        void setLabel(Label *label);
        
        void jsr(Label *label)  { iCode->push_back(new Jsr(label)); }
        void rts()              { iCode->push_back(new Rts()); }
        void branch(Label *label);
        void branchConditional(Label *label, Register condition);
        void branchNotConditional(Label *label, Register condition);
        void branchTrue(Label *label, Register condition);
        void branchFalse(Label *label, Register condition);
        
        void beginTry(Label *catchLabel, Label *finallyLabel)
            { iCode->push_back(new Tryin(catchLabel, finallyLabel)); }
        void endTry()
            { iCode->push_back(new Tryout()); }
        
        void resetStatement() { resetTopRegister(); }

        ICodeOp mapExprNodeToICodeOp(ExprNode::Kind kind);
    
    public:
        ICodeGenerator(World *world = NULL, 
                            bool hasTryStatement = false, 
                            uint32 switchStatementNesting = 0);
        
        ~ICodeGenerator()
        {
            if (iCodeOwner)
                delete iCode;
        }
                
        ICodeModule *complete();

        Register genExpr(ExprNode *p, bool needBoolValueInBranch = false,
                    Label *trueBranch = NULL, 
                    Label *falseBranch = NULL);
        Register genStmt(StmtNode *p, LabelSet *currentLabelSet = NULL);


        void addInstruction(Instruction *i) { iCode->push_back(i); }


        Register allocateVariable(const StringAtom& name) 
        { Register result = getRegister(); (*variableList)[name] = result; 
            registerBase = topRegister; return result; }

        Register findVariable(const StringAtom& name)
        { VariableList::iterator i = variableList->find(name);
        return (i == variableList->end()) ? NotARegister : (*i).second; }
        
        Register allocateParameter(const StringAtom& name) 
        { parameterCount++; return allocateVariable(name); }
        
        Formatter& print(Formatter& f);
        
        Register op(ICodeOp op, Register source);
        Register op(ICodeOp op, Register source1, Register source2);
        Register call(Register target, RegisterList args);
        void callVoid(Register target, RegisterList args);

        void move(Register destination, Register source);
        Register logicalNot(Register source);
        Register test(Register source);
        
        Register loadValue(JSValue value);
        Register loadImmediate(double value);
        Register loadString(String &value);
                
        Register newObject();
        Register newArray();
        
        Register loadName(const StringAtom &name);
        void saveName(const StringAtom &name, Register value);
        Register nameInc(const StringAtom &name);
        Register nameDec(const StringAtom &name);
        
        Register getProperty(Register base, const StringAtom &name);
        void setProperty(Register base, const StringAtom &name, Register value);
        Register propertyInc(Register base, const StringAtom &name);
        Register propertyDec(Register base, const StringAtom &name);
        
        Register getElement(Register base, Register index);
        void setElement(Register base, Register index, Register value);
        Register elementInc(Register base, Register index);
        Register elementDec(Register base, Register index);
       
        Register getRegisterBase()                  { return topRegister; }
        InstructionStream *get_iCode()              { return iCode; }
        
        Label *getLabel();

    };

    Formatter& operator<<(Formatter &f, ICodeGenerator &i);
    /*
      std::ostream &operator<<(std::ostream &s, ICodeGenerator &i);
      std::ostream &operator<<(std::ostream &s, StringAtom &str);
    */



} /* namespace IGC */
} /* namespace JavaScript */

#endif /* icodegenerator_h */
