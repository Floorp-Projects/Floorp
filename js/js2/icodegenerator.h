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
#include "jsclasses.h"


namespace JavaScript {
namespace ICG {
    
    using namespace VM;
    using namespace JSTypes;
    using namespace JSClasses;
    
    typedef std::map<String, TypedRegister, std::less<String> > VariableList;
    typedef std::map<uint32, uint32, std::less<uint32> > InstructionMap;
   

    class ICodeModule {
    public:
        ICodeModule(InstructionStream *iCode, VariableList *variables,
                    uint32 maxRegister, uint32 maxParameter,
                    InstructionMap *instructionMap) :
            its_iCode(iCode), itsVariables(variables),
            itsParameterCount(maxParameter), itsMaxRegister(maxRegister),
            mID(++sMaxID), mInstructionMap(instructionMap) { }
        ~ICodeModule()
        {
            delete its_iCode;
            delete itsVariables;
            delete mInstructionMap;
        }

        Formatter& print(Formatter& f);
        void setFileName (String aFileName) { mFileName = aFileName; }
        String getFileName () { return mFileName; }
        
        InstructionStream *its_iCode;
        VariableList *itsVariables;
        uint32 itsParameterCount;
        uint32 itsMaxRegister;
        uint32 mID;
        InstructionMap *mInstructionMap;
        String mFileName;

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
    public:
        typedef enum { kNoFlags = 0, kIsTopLevel = 0x01, kIsStaticMethod = 0x02, kIsWithinWith = 0x04 } ICodeGeneratorFlags;
    private:
        InstructionStream *iCode;
        bool iCodeOwner;
        LabelList labels;
        
        Register topRegister;           // highest (currently) alloacated register
        Register registerBase;          // start of registers available for expression temps
        uint32   maxRegister;           // highest (ever) allocated register
        uint32   parameterCount;        // number of parameters declared for the function
                                        // these must come before any variables declared.
        TypedRegister exceptionRegister;// reserved to carry the exception object.
        VariableList *variableList;     // name|register pair for each variable

        World *mWorld;                  // used to register strings
        JSScope *mGlobal;               // the scope for compiling within
        LabelStack mLabelStack;         // stack of LabelEntry objects, one per nested looping construct
                                        // maps source position to instruction index
        InstructionMap *mInstructionMap;

        JSClass *mClass;                // enclosing class when generating code for methods
        ICodeGeneratorFlags mFlags;     // assorted flags

        void markMaxRegister()
        { if (topRegister > maxRegister) maxRegister = topRegister; }
        
        Register getRegister()
        { return topRegister++; }

        void resetTopRegister()
        { markMaxRegister(); topRegister = registerBase; }
        

        void setLabel(Label *label);
        
        void jsr(Label *label)  { iCode->push_back(new Jsr(label)); }
        void rts()              { iCode->push_back(new Rts()); }
        void branch(Label *label);
        GenericBranch *branchTrue(Label *label, TypedRegister condition);
        GenericBranch *branchFalse(Label *label, TypedRegister condition);
        
        void beginTry(Label *catchLabel, Label *finallyLabel)
            { iCode->push_back(new Tryin(catchLabel, finallyLabel)); }
        void endTry()
            { iCode->push_back(new Tryout()); }

        void beginWith(TypedRegister obj)
            { iCode->push_back(new Within(obj)); }
        void endWith()
            { iCode->push_back(new Without()); }
        
        void resetStatement() { resetTopRegister(); }

        void setRegisterForVariable(const StringAtom& name, TypedRegister r) 
        { (*variableList)[name] = r; }

        void startStatement(uint32 pos)
        { (*mInstructionMap)[iCode->size()] = pos; }

        ICodeOp mapExprNodeToICodeOp(ExprNode::Kind kind);

        TypedRegister grabRegister(const StringAtom& name, JSType *type) 
        {
            TypedRegister result(getRegister(), type); 
            (*variableList)[name] = result; 
            registerBase = topRegister;
            return result;
        }


        bool isTopLevel()       { return (mFlags & kIsTopLevel) != 0; }
        bool isWithinWith()     { return (mFlags & kIsWithinWith) != 0; }
        bool isStaticMethod()   { return (mFlags & kIsStaticMethod) != 0; }

        void setFlag(uint32 flag, bool v) { mFlags = (ICodeGeneratorFlags)((v) ? mFlags | flag : mFlags & ~flag); }

        JSType *findType(const StringAtom& typeName);
        TypedRegister handleDot(BinaryExprNode *b, ExprNode::Kind use, ICodeOp xcrementOp, TypedRegister ret, RegisterList *args);
        ICodeModule *genFunction(FunctionStmtNode *f);
    
    public:

        ICodeGenerator(World *world, JSScope *global, JSClass *aClass = NULL, ICodeGeneratorFlags flags = kIsTopLevel);
        
        ~ICodeGenerator()
        {
            if (iCodeOwner) {
                delete iCode;
                delete mInstructionMap;
            }
        }
                
        ICodeModule *complete();

        TypedRegister genExpr(ExprNode *p, 
                    bool needBoolValueInBranch = false, 
                    Label *trueBranch = NULL, 
                    Label *falseBranch = NULL);
        void preprocess(StmtNode *p);
        TypedRegister genStmt(StmtNode *p, LabelSet *currentLabelSet = NULL);

        void returnStmt(TypedRegister r);
        void returnStmt();
        void throwStmt(TypedRegister r)
            { iCode->push_back(new Throw(r)); }
        void debuggerStmt()
            { iCode->push_back(new Debugger()); }

        TypedRegister allocateVariable(const StringAtom& name);
        TypedRegister allocateVariable(const StringAtom& name, const StringAtom& typeName);

        TypedRegister findVariable(const StringAtom& name)
            { VariableList::iterator i = variableList->find(name);
        return (i == variableList->end()) ? TypedRegister(NotARegister, &None_Type) : (*i).second; }
        
        TypedRegister allocateParameter(const StringAtom& name) 
            { parameterCount++; return grabRegister(name, &Any_Type); }
        TypedRegister allocateParameter(const StringAtom& name, const StringAtom& typeName) 
            { parameterCount++; return grabRegister(name, findType(typeName)); }
        TypedRegister allocateParameter(const StringAtom& name, JSType *type) 
            { parameterCount++; return grabRegister(name, type); }
        
        Formatter& print(Formatter& f);
        
        TypedRegister op(ICodeOp op, TypedRegister source);
        TypedRegister op(ICodeOp op, TypedRegister source1, TypedRegister source2);
        TypedRegister call(TypedRegister target, const StringAtom &name, RegisterList args);
        TypedRegister methodCall(TypedRegister targetBase, TypedRegister targetValue, RegisterList args);
        TypedRegister staticCall(JSClass *c, const StringAtom &name, RegisterList args);

        void move(TypedRegister destination, TypedRegister source);
        TypedRegister logicalNot(TypedRegister source);
        TypedRegister test(TypedRegister source);
        
        TypedRegister loadBoolean(bool value);
        TypedRegister loadImmediate(double value);
        TypedRegister loadString(String &value);
        TypedRegister loadString(const StringAtom &name);
                
        TypedRegister newObject();
        TypedRegister newArray();
        TypedRegister newFunction(ICodeModule *icm);
        TypedRegister newClass(const StringAtom &name);
        
        TypedRegister loadName(const StringAtom &name);
        void saveName(const StringAtom &name, TypedRegister value);
        TypedRegister nameXcr(const StringAtom &name, ICodeOp op);
       
        TypedRegister deleteProperty(TypedRegister base, const StringAtom &name);
        TypedRegister getProperty(TypedRegister base, const StringAtom &name);
        void setProperty(TypedRegister base, const StringAtom &name, TypedRegister value);
        TypedRegister propertyXcr(TypedRegister base, const StringAtom &name, ICodeOp op);
        
        TypedRegister getStatic(JSClass *base, const StringAtom &name);
        void setStatic(JSClass *base, const StringAtom &name, TypedRegister value);
        TypedRegister staticXcr(JSClass *base, const StringAtom &name, ICodeOp op);
        
        TypedRegister getElement(TypedRegister base, TypedRegister index);
        void setElement(TypedRegister base, TypedRegister index, TypedRegister value);
        TypedRegister elementXcr(TypedRegister base, TypedRegister index, ICodeOp op);

        TypedRegister getSlot(TypedRegister base, uint32 slot);
        void setSlot(TypedRegister base, uint32 slot, TypedRegister value);
        TypedRegister slotXcr(TypedRegister base, uint32 slot, ICodeOp op);

        TypedRegister varXcr(TypedRegister var, ICodeOp op);
        
        Register getRegisterBase()                  { return topRegister; }
        InstructionStream *getICode()               { return iCode; }
        
        Label *getLabel();

    };

    Formatter& operator<<(Formatter &f, ICodeGenerator &i);
    Formatter& operator<<(Formatter &f, ICodeModule &i);
    /*
      std::ostream &operator<<(std::ostream &s, ICodeGenerator &i);
      std::ostream &operator<<(std::ostream &s, StringAtom &str);
    */



} /* namespace IGC */
} /* namespace JavaScript */

#endif /* icodegenerator_h */
