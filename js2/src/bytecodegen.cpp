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

#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif

#include "parser.h"
#include "js2runtime.h"
#include "bytecodegen.h"
#include "numerics.h"
#include "formatter.h"

#include <string.h>

// this is the IdentifierList passed to the name lookup routines
#define CURRENT_ATTR    mNamespaceList

namespace JavaScript {    
namespace JS2Runtime {


void Reference::emitTypeOf(ByteCodeGen *bcg)
{
    emitCodeSequence(bcg); 
    bcg->addOp(TypeOfOp);
}

void Reference::emitDelete(ByteCodeGen *bcg)
{
    bcg->addOp(LoadConstantFalseOp);
}

void AccessorReference::emitCodeSequence(ByteCodeGen * /*bcg*/) 
{ 
    ASSERT(false);      // NYI
//    bcg->addOp(InvokeOp); 
//    bcg->addPointer(mFunction);
}

void LocalVarReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    if (mAccess == Read)
        bcg->addOp(GetLocalVarOp);
    else
        bcg->addOp(SetLocalVarOp);
    bcg->addLong(mIndex); 
}

void ParameterReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    if (mAccess == Read)
        bcg->addOp(GetArgOp);
    else
        bcg->addOp(SetArgOp);
    bcg->addLong(mIndex); 
}

void ClosureVarReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    if (mAccess == Read)
        bcg->addOp(GetClosureVarOp);
    else
        bcg->addOp(SetClosureVarOp);
    bcg->addLong(mDepth); 
    bcg->addLong(mIndex); 
}

void StaticFieldReference::emitImplicitLoad(ByteCodeGen *bcg) 
{
    bcg->addOp(LoadTypeOp);
    bcg->addPointer(mClass);
}

void StaticFieldReference::emitDelete(ByteCodeGen *bcg)
{
    bcg->addOp(DeleteOp);
    bcg->addStringRef(mName); 
}



void FieldReference::emitImplicitLoad(ByteCodeGen *bcg) 
{
    bcg->addOp(LoadThisOp);
}

void FieldReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    if (mAccess == Read)
        bcg->addOp(GetFieldOp);
    else
        bcg->addOp(SetFieldOp);
    bcg->addLong(mIndex); 
}


void StaticFieldReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    if (mAccess == Read)
        bcg->addOp(GetPropertyOp);
    else
        bcg->addOp(SetPropertyOp);
    bcg->addStringRef(mName); 
}

void StaticFieldReference::emitInvokeSequence(ByteCodeGen *bcg) 
{
    bcg->addOp(GetPropertyOp);
    bcg->addStringRef(mName); 
}



void MethodReference::emitImplicitLoad(ByteCodeGen *bcg) 
{
    bcg->addOp(LoadThisOp);
}

void MethodReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    bcg->addOp(GetMethodRefOp);
    bcg->addLong(mIndex); 
}

void MethodReference::emitInvokeSequence(ByteCodeGen *bcg) 
{
    bcg->addOp(GetMethodOp);
    bcg->addLong(mIndex); 
}

void GetterMethodReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    bcg->addOp(GetMethodOp);
    bcg->addLong(mIndex); 
    bcg->addOpAdjustDepth(InvokeOp, -1);    // function, 'this' --> result
    bcg->addLong(0);
    bcg->addByte(Explicit);
}

void SetterMethodReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    bcg->addOpAdjustDepth(InvokeOp, -2);    // leaves value on stack
    bcg->addLong(1);
    bcg->addByte(Explicit);
}

bool SetterMethodReference::emitPreAssignment(ByteCodeGen *bcg) 
{
    bcg->addOp(GetMethodOp);
    bcg->addLong(mIndex);
    return true;
}

void FunctionReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    bcg->addOp(LoadFunctionOp);
    bcg->addPointer(mFunction);
}

void GetterFunctionReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    bcg->addOp(LoadFunctionOp);
    bcg->addPointer(mFunction);
    bcg->addOpAdjustDepth(InvokeOp, -1);
    bcg->addLong(0);
    bcg->addByte(Explicit);
}

void SetterFunctionReference::emitImplicitLoad(ByteCodeGen *bcg) 
{
    bcg->addOp(LoadFunctionOp);
    bcg->addPointer(mFunction);
}

void SetterFunctionReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    bcg->addOpAdjustDepth(InvokeOp, -1);
    bcg->addLong(1);
    bcg->addByte(Explicit);
}

void NameReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    if (mAccess == Read)
        bcg->addOp(GetNameOp);
    else
        bcg->addOp(SetNameOp);
    bcg->addStringRef(mName); 
}

void NameReference::emitTypeOf(ByteCodeGen *bcg)
{
    bcg->addOp(GetTypeOfNameOp);
    bcg->addStringRef(mName); 
    bcg->addOp(TypeOfOp);
}

void NameReference::emitDelete(ByteCodeGen *bcg)
{
    bcg->addOp(DeleteNameOp);
    bcg->addStringRef(mName); 
}


void PropertyReference::emitImplicitLoad(ByteCodeGen *bcg) 
{
    bcg->addOp(LoadThisOp);
}

void PropertyReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    if (mAccess == Read)
        bcg->addOp(GetPropertyOp);
    else
        bcg->addOp(SetPropertyOp);
    bcg->addStringRef(mName); 
}

void PropertyReference::emitInvokeSequence(ByteCodeGen *bcg) 
{
    bcg->addOp(GetInvokePropertyOp);
    bcg->addStringRef(mName); 
}

void PropertyReference::emitDelete(ByteCodeGen *bcg)
{
    bcg->addOp(DeleteOp);
    bcg->addStringRef(mName); 
}

void ElementReference::emitCodeSequence(ByteCodeGen *bcg) 
{
    if (mAccess == Read)
        bcg->addOpAdjustDepth(GetElementOp, -mDepth);
    else
        bcg->addOpAdjustDepth(SetElementOp, -(mDepth + 1));
    bcg->addShort(mDepth);
}

void ElementReference::emitDelete(ByteCodeGen *bcg) 
{
    bcg->addOpAdjustDepth(DeleteElementOp, -(mDepth - 1));
    bcg->addShort(mDepth);
}


ByteCodeData gByteCodeData[OpCodeCount] = {
{ 1,        "LoadConstantUndefined", },
{ 1,        "LoadConstantTrue", },
{ 1,        "LoadConstantFalse", },
{ 1,        "LoadConstantNull", },
{ 1,        "LoadConstantZero", },
{ 1,        "LoadConstantNumber", },
{ 1,        "LoadConstantString", },
{ 1,        "LoadThis", },
{ 1,        "LoadFunction", },
{ 1,        "LoadType", },
{ -128,     "Invoke", },
{ 0,        "GetType", },
{ -1,       "Cast", },
{ 0,        "DoUnary", },
{ -1,       "DoOperator", },
{ 1,        "PushNull", },
{ 1,        "PushInt", },
{ 1,        "PushNum", },
{ 1,        "PushString", },
{ 1,        "PushType", },
{ -128,     "Return", },
{ -128,     "ReturnVoid", },
{ 0,        "GetConstructor", },
{ 1,        "NewObject", },
{ -1,       "NewThis", },
{ -128,     "NewInstance", },
{ 0,        "Delete", },
{ 1,        "DeleteName", },
{ 0,        "TypeOf", },
{ -1,       "InstanceOf", },
{ -1,       "As", },
{ -1,       "Is", },
{ 0,        "ToBoolean", },
{ -1,       "JumpFalse", },
{ -1,       "JumpTrue", },
{ 0,        "Jump", },
{ 0,        "Try", },
{ 0,        "Jsr", },
{ 0,        "Rts", },
{ -1,       "Within", },
{ 0,        "Without", },
{ -128,     "Throw", },
{ 0,        "Handler", },
{ -3,       "LogicalXor", },
{ 0,        "LogicalNot", },
{ 0,        "Swap", },
{ 1,        "Dup", },
{ 1,        "DupInsert", },
{ -128,     "DupN", },
{ -128,     "DupInsertN", },
{ -1,       "Pop", },
{ -128,     "PopN", },
{ 0,        "GetField", },
{ -1,       "SetField", },
{ 1,        "GetMethod", },
{ 0,        "GetMethodRef", },
{ 1,        "GetArg", },
{ 0,        "SetArg", },
{ 1,        "GetLocalVar", },
{ 0,        "SetLocalVar", },
{ 1,        "GetClosureVar", },
{ 0,        "SetClosureVar", },
{ -128,     "GetElement", },
{ -128,     "SetElement", },
{ -128,     "DeleteElement" },
{ 0,        "GetProperty", },
{ 1,        "GetInvokeProperty", },
{ -1,       "SetProperty", },
{ 1,        "GetName", },
{ 1,        "GetTypeOfName", },
{ 0,        "SetName", },
{ 1,        "LoadGlobalObject", },
{ 0,        "PushScope", },
{ 0,        "PopScope", },
{ 0,        "NewClosure" },
{ 0,        "Class" },
{ -1,       "Juxtapose" },
{ -1,       "NamedArgument" },

};

ByteCodeModule::ByteCodeModule(ByteCodeGen *bcg, JSFunction *f)
{
    mFunction = f;
    mLength = bcg->mBuffer->size();
    mCodeBase = new uint8[mLength];
    memcpy(mCodeBase, bcg->mBuffer->begin(), mLength);

    mCodeMapLength = bcg->mPC_Map->size();
    mCodeMap = new PC_Position[mCodeMapLength];
    memcpy(mCodeMap, bcg->mPC_Map->begin(), mCodeMapLength * sizeof(PC_Position));

    mLocalsCount = bcg->mScopeChain->countVars();
    mStackDepth = toUInt32(bcg->mStackMax);
}

ByteCodeModule::~ByteCodeModule()
{
    delete[] mCodeBase;
    delete[] mCodeMap;
}

size_t ByteCodeModule::getPositionForPC(uint32 pc)
{
    if (mCodeMapLength == 0)
        return 0;

    for (uint32 i = 0; i < (mCodeMapLength - 1); i++) {
        uint32 pos1 = mCodeMap[i].first;
        uint32 pos2 = mCodeMap[i + 1].first;
        if ((pc >= pos1) && (pc < pos2))
            return mCodeMap[i].second;
    }
    return mCodeMap[mCodeMapLength - 1].second;
}





uint32 ByteCodeGen::getLabel()
{
    uint32 result = mLabelList.size();
    mLabelList.push_back(Label());
    return result;
}

uint32 ByteCodeGen::getLabel(Label::LabelKind kind)
{
    uint32 result = mLabelList.size();
    mLabelList.push_back(Label(kind));
    return result;
}

uint32 ByteCodeGen::getLabel(LabelStmtNode *lbl)
{
    uint32 result = mLabelList.size();
    mLabelList.push_back(Label(lbl));
    return result;
}

uint32 ByteCodeGen::getTopLabel(Label::LabelKind kind, const StringAtom *name, StmtNode *p)
{
    uint32 result = uint32(-1);
    bool foundLabel = false;
    for (std::vector<uint32>::reverse_iterator i = mLabelStack.rbegin(),
                        end = mLabelStack.rend();
                        (i != end); i++)
    {
        // find the appropriate kind of label
        if (mLabelList[*i].matches(kind)) {
            result = *i;
            foundLabel = true;
        }
        else // and return it when we get the name
            if (mLabelList[*i].matches(name)) {
                if (foundLabel)
                    return result;
            }
    }
    m_cx->reportError(Exception::syntaxError, "label not found", p->pos);
    return false;
}

uint32 ByteCodeGen::getTopLabel(Label::LabelKind kind, StmtNode *p)
{
    for (std::vector<uint32>::reverse_iterator i = mLabelStack.rbegin(),
                        end = mLabelStack.rend();
                        (i != end); i++)
    {
        if (mLabelList[*i].matches(kind))
            return *i;
    }
    m_cx->reportError(Exception::syntaxError, "label not found", p->pos);
    return false;
}

void ByteCodeGen::addOp(uint8 op)        
{ 
    addByte(op);
    ASSERT(gByteCodeData[op].stackImpact != -128);
    mStackTop += gByteCodeData[op].stackImpact;
    if (mStackTop > mStackMax)
        mStackMax = mStackTop; 
    ASSERT(mStackTop >= 0);
}

void ByteCodeGen::addNumberRef(float64 f)
{
    addFloat64(f);
/*
    NumberPool::iterator i = mNumberPool.find(f);
    if (i != mNumberPool.end())
        addLong(i->second);
    else {
        addLong(mNumberPoolContents.size());
        mNumberPool[f] = mNumberPoolContents.size();
        mNumberPoolContents.push_back(f);
    }
*/
}

void ByteCodeGen::addStringRef(const String &str)
{
    const StringAtom &s = m_cx->mWorld.identifiers[str];
    addPointer((void *)(&s));

/*
    StringPool::iterator i = mStringPool.find(str);
    if (i != mStringPool.end())
        addLong(i->second);
    else {
        addLong(mStringPoolContents.size());
        mStringPool[str] = mStringPoolContents.size();
        mStringPoolContents.push_back(str);
    }
*/
}








void Label::setLocation(ByteCodeGen *bcg, uint32 location)
{
    mHasLocation = true;
    mLocation = location;
    for (std::vector<uint32>::iterator i = mFixupList.begin(), end = mFixupList.end(); 
                    (i != end); i++)
    {
        uint32 branchLocation = *i;
        bcg->setOffset(branchLocation, int32(mLocation - branchLocation)); 
    }
}

void Label::addFixup(ByteCodeGen *bcg, uint32 branchLocation) 
{ 
    if (mHasLocation)
        bcg->addOffset(int32(mLocation - branchLocation));
    else {
        mFixupList.push_back(branchLocation); 
        bcg->addLong(0);
    }
}



void ByteCodeGen::genCodeForFunction(FunctionDefinition &f, size_t pos, JSFunction *fnc, bool isConstructor, JSType *topClass)
{
    mScopeChain->addScope(fnc->mParameterBarrel);
    mScopeChain->addScope(&fnc->mActivation);
    // OPT - no need to push the parameter and function
    // scopes if the function doesn't contain any 'eval'
    // calls, all other references to the variables mapped
    // inside these scopes will have been turned into
    // localVar references.
/*
    addByte(PushScopeOp);
    addPointer(fnc->mParameterBarrel);
    addByte(PushScopeOp);   
    addPointer(&fnc->mActivation);
*/

#ifdef DEBUG
    if (f.name) {
//      const StringAtom& name = checked_cast<IdentifierExprNode *>(f.name)->name;
//      stdOut << "gencode for " << name << "\n";
    }
#endif

    if (isConstructor) {
        //
        // add a code sequence to create a new empty instance if the
        // incoming 'this' is null
        //
        addOp(LoadTypeOp);
        addPointer(topClass);
        addOp(NewThisOp);
        //
        //  Invoke the super class constructor if there isn't an explicit
        // statement to do so.
        //  
        if (topClass->mSuperType) {
            JSType *superClass = topClass->mSuperType;            
            bool foundSuperCall = false;
            BlockStmtNode *b = f.body;
            if (b && b->statements) {
                if (b->statements->getKind() == StmtNode::expression) {
                    ExprStmtNode *e = checked_cast<ExprStmtNode *>(b->statements);
                    if (e->expr->getKind() == ExprNode::call) {
                        InvokeExprNode *i = checked_cast<InvokeExprNode *>(e->expr);
                        if (i->op->getKind() == ExprNode::dot) {
                            // check for 'this.m()'
                            BinaryExprNode *b = checked_cast<BinaryExprNode *>(i->op);
                            if ((b->op1->getKind() == ExprNode::This) && (b->op2->getKind() == ExprNode::identifier)) {
    //                            IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(b->op2);
                                // XXX verify that i->name is a constructor in the superclass
                                foundSuperCall = true;
                                i->isSuperInvoke = true;
                            }
                        }
                        else {
                            // look for calls to 'this()'
                            if (i->op->getKind() == ExprNode::This) {
                                foundSuperCall = true;
                                i->isSuperInvoke = true;
                            }
                            else {
                                // otherwise, look for calls to the superclass by name
                                if (i->op->getKind() == ExprNode::identifier) {
                                    const StringAtom &name = checked_cast<IdentifierExprNode *>(i->op)->name;
                                    if (superClass->mClassName->compare(name) == 0)
                                        foundSuperCall = true;
                                    i->isSuperInvoke = true;
                                }                            
                            }
                        }
                    }
                    else {
                        if (e->expr->getKind() == ExprNode::superStmt) {
                            foundSuperCall = true;
                        }
                    }
                }
            }

            if (!foundSuperCall) { // invoke the default superclass constructor
                if (superClass) {
            // Make sure there's a default constructor with 0 (required) parameters
                    JSFunction *superConstructor = superClass->getDefaultConstructor();
                    if (superConstructor) {
                        if (superConstructor->getRequiredArgumentCount() > 0)
                            m_cx->reportError(Exception::typeError, "Super class default constructor must be called explicitly - it has required parameters that must be specified", pos);
                        addOp(LoadThisOp);
                        addOp(LoadFunctionOp);
                        addPointer(superConstructor);
                        addOpAdjustDepth(InvokeOp, -1);
                        addLong(0);
                        addByte(Explicit);
                        addOp(PopOp);
                    }
                }
            }
        }
    }

    bool hasReturn = false;
    if (f.body)
        hasReturn = genCodeForStatement(f.body, NULL, NotALabel);
    
/*
    // OPT - see above
    addByte(PopScopeOp);
    addByte(PopScopeOp);
*/  
    if (isConstructor) {
        ASSERT(!hasReturn);     // is this useful? Won't the semantics have done it?
        addOp(LoadThisOp);
        ASSERT(mStackTop == 1);
        addOpSetDepth(ReturnOp, 0);
    }
    else {
        if (!hasReturn) {
            addOp(LoadConstantUndefinedOp);
            ASSERT(mStackTop == 1);
            addOpSetDepth(ReturnOp, 0);
        }
    }

    VariableBinding *v = f.parameters;
    uint32 index = 0;
    while (v) {
        if (v->initializer) {        
            // this code gets executed if the function is called without
            // an argument for this parameter. 
            fnc->setArgumentInitializer(index, currentOffset());
            genExpr(v->initializer);
            addOpSetDepth(ReturnOp, 0);
        }
        index++;
        v = v->next;
    }

    ByteCodeModule *bcm = new ByteCodeModule(this, fnc);
    if (m_cx->mReader)
        bcm->setSource(m_cx->mReader->source, m_cx->mReader->sourceLocation);
    fnc->setByteCode(bcm);        

    mScopeChain->popScope();
    mScopeChain->popScope();
}


ByteCodeModule *ByteCodeGen::genCodeForScript(StmtNode *p)
{
    while (p) {
        genCodeForStatement(p, NULL, NotALabel);
        p = p->next;
    }
    return new ByteCodeModule(this, NULL);
}

ByteCodeModule *ByteCodeGen::genCodeForExpression(ExprNode *p)
{
    genExpr(p);
    addOp(PopOp);
    return new ByteCodeModule(this, NULL);
}


// emit bytecode for the single statement p. Return true if that statement
// was a return statement (or contained only paths leading to a return statement)
bool ByteCodeGen::genCodeForStatement(StmtNode *p, ByteCodeGen *static_cg, uint32 finallyLabel)
{
    bool result = false;    
    addPosition(p->pos);
    switch (p->getKind()) {
    case StmtNode::Class:
        {
            ClassStmtNode *classStmt = checked_cast<ClassStmtNode *>(p);
            JSType *thisClass = classStmt->mType;

            mScopeChain->addScope(thisClass);
            if (classStmt->body) {
                ByteCodeGen static_cg(m_cx, mScopeChain);       // this will capture the static initializations
                ByteCodeGen bcg(m_cx, mScopeChain);             // this will capture the instance initializations
                StmtNode* s = classStmt->body->statements;
                while (s) {
                    bcg.genCodeForStatement(s, &static_cg, finallyLabel);
                    s = s->next;
                }
                JSFunction *f = NULL;
                if (static_cg.hasContent()) {
                    // build a function to be invoked 
                    // when the class is loaded
                    f = new JSFunction(m_cx, Void_Type, mScopeChain);
                    ByteCodeModule *bcm = new ByteCodeModule(&static_cg, NULL);
                    if (m_cx->mReader)
                        bcm->setSource(m_cx->mReader->source, m_cx->mReader->sourceLocation);
                    f->setByteCode(bcm);
                }
                thisClass->setStaticInitializer(m_cx, f);
                f = NULL;
                if (bcg.hasContent()) {
                    // execute this function now to form the initial instance
                    f = new JSFunction(m_cx, Void_Type, mScopeChain);
                    ByteCodeModule *bcm = new ByteCodeModule(&bcg, NULL);
                    if (m_cx->mReader)
                        bcm->setSource(m_cx->mReader->source, m_cx->mReader->sourceLocation);
                    f->setByteCode(bcm);
                }
                thisClass->setInstanceInitializer(m_cx, f);
            }
            mScopeChain->popScope();
        }
        break;
    case StmtNode::Const:
    case StmtNode::Var:
        {
            VariableStmtNode *vs = checked_cast<VariableStmtNode *>(p);
            VariableBinding *v = vs->bindings;
            bool isStatic = (vs->attributeValue->mTrueFlags & Property::Static) == Property::Static;
            while (v)  {
                Reference *ref = mScopeChain->getName(*v->name, vs->attributeValue->mNamespaceList, Write);
                ASSERT(ref);    // must have been added previously by collectNames
                if (v->initializer) {
                    if (isStatic && (static_cg != NULL)) {
                        ref->emitImplicitLoad(static_cg);
                        static_cg->genExpr(v->initializer);
                        ref->emitCodeSequence(static_cg);
                        static_cg->addOp(PopOp);
                    }
                    else {
                        ref->emitImplicitLoad(this);
                        genExpr(v->initializer);
                        ref->emitCodeSequence(this);
                        addOp(PopOp);
                    }
                }
                else {
                    // initialize the variable with an appropriate value
                    JSValue uiv = ref->mType->getUninitializedValue();
                    if (!uiv.isUndefined()) {
                        ref->emitImplicitLoad(this);
                        if (uiv.isNull())
                            addOp(LoadConstantNullOp);
                        else
                        if (uiv.isPositiveZero())
                            addOp(LoadConstantZeroOp);
                        else
                        if (uiv.isFalse())
                            addOp(LoadConstantFalseOp);
                        else
                            NOT_REACHED("Any more??");
                        ref->emitCodeSequence(this);
                        addOp(PopOp);
                    }
                }
                delete ref;
                v = v->next;
            }
        }
        break;
    case StmtNode::Function:
        {
            FunctionStmtNode *f = checked_cast<FunctionStmtNode *>(p);
            bool isConstructor = (f->attributeValue->mTrueFlags & Property::Constructor) == Property::Constructor;
            JSFunction *fnc = f->mFunction;

            ASSERT(f->function.name);
            if (mScopeChain->topClass() && (mScopeChain->topClass()->mClassName->compare(*f->function.name) == 0))
                isConstructor = true;
            ByteCodeGen bcg(m_cx, mScopeChain);
            bcg.genCodeForFunction(f->function, f->pos, fnc, isConstructor, mScopeChain->topClass());

            if (mScopeChain->isNestedFunction()) {
                addOp(LoadFunctionOp);
                addPointer(fnc);
                addOp(NewClosureOp);
                addOp(SetNameOp);
                addStringRef(*f->function.name);
                addOp(PopOp);
            }
        
        }
        break;
    case StmtNode::While:
        {
            UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
            addOp(JumpOp);
            uint32 labelAtTestCondition = getLabel(Label::ContinueLabel); 
            addFixup(labelAtTestCondition);
            uint32 labelAtTopOfBlock = getLabel();
            setLabel(labelAtTopOfBlock);
            uint32 breakLabel = getLabel(Label::BreakLabel);

            mLabelStack.push_back(breakLabel);
            mLabelStack.push_back(labelAtTestCondition);
            genCodeForStatement(w->stmt, static_cg, finallyLabel);
            mLabelStack.pop_back();
            mLabelStack.pop_back();
            
            setLabel(labelAtTestCondition);
            genExpr(w->expr);
            addOp(ToBooleanOp);
            addOp(JumpTrueOp);
            addFixup(labelAtTopOfBlock);
            setLabel(breakLabel);
        }
        break;
    case StmtNode::DoWhile:
        {
            UnaryStmtNode *d = checked_cast<UnaryStmtNode *>(p);
            uint32 breakLabel = getLabel(Label::BreakLabel);
            uint32 labelAtTopOfBlock = getLabel();
            uint32 labelAtTestCondition = getLabel(Label::ContinueLabel); 
            setLabel(labelAtTopOfBlock);

            mLabelStack.push_back(breakLabel);
            mLabelStack.push_back(labelAtTestCondition);
            genCodeForStatement(d->stmt, static_cg, finallyLabel);
            mLabelStack.pop_back();
            mLabelStack.pop_back();

            setLabel(labelAtTestCondition);
            genExpr(d->expr);
            addOp(ToBooleanOp);
            addOp(JumpTrueOp);
            addFixup(labelAtTopOfBlock);
            setLabel(breakLabel);
        }
        break;
    case StmtNode::ForIn:
        {
            ForStmtNode *f = checked_cast<ForStmtNode *>(p);

            uint32 breakLabel = getLabel(Label::BreakLabel);
            uint32 labelAtTopOfBlock = getLabel();
            uint32 labelAtIncrement = getLabel(Label::ContinueLabel); 
            uint32 labelAtTestCondition = getLabel(); 
            uint32 labelAtEnd = getLabel(); 
/*
            iterator = object.forin()
            goto test
            top:
                v = iterator.value
                <statement body>
            continue:
                iterator = object.next(iterator)
            test:
                if (iterator == null)
                    goto end
                goto top                    
            break:
                object.done(iterator)
            end:
*/

            // acquire a local from the scopechain, and copy the target object
            // into it.
            Reference *objectReadRef, *objectWriteRef;
            Reference *iteratorReadRef, *iteratorWriteRef;
            mScopeChain->defineTempVariable(m_cx, objectReadRef, objectWriteRef, Object_Type);
            mScopeChain->defineTempVariable(m_cx, iteratorReadRef, iteratorWriteRef, Object_Type);


                genExpr(f->expr2);
                objectWriteRef->emitCodeSequence(this);
                addOp(GetInvokePropertyOp);
//              addIdentifierRef(widenCString("Iterator"), widenCString("forin"));
                addStringRef(m_cx->Forin_StringAtom);
                addOpAdjustDepth(InvokeOp, -1);
                addLong(0);
                addByte(Explicit);
                iteratorWriteRef->emitCodeSequence(this);
                addOp(PopOp);
            
                addOp(JumpOp);
                addFixup(labelAtTestCondition);

            setLabel(labelAtTopOfBlock);

                Reference *value = NULL;
                if (f->initializer->getKind() == StmtNode::Var) {
                    VariableStmtNode *vs = checked_cast<VariableStmtNode *>(f->initializer);
                    VariableBinding *v = vs->bindings;
                    value = mScopeChain->getName(*v->name, CURRENT_ATTR, Write);
                }
                else {
                    if (f->initializer->getKind() == StmtNode::expression) {
                        ExprStmtNode *e = checked_cast<ExprStmtNode *>(f->initializer);
                        value = genReference(e->expr, Write);
                    }
                    else
                        NOT_REACHED("what else??");
                }            
                if (value == NULL)
                    m_cx->reportError(Exception::referenceError, "Invalid for..in target");
                iteratorReadRef->emitCodeSequence(this);
                addOp(GetPropertyOp);
                addStringRef(m_cx->Value_StringAtom);
                value->emitCodeSequence(this);
                addOp(PopOp);

                mLabelStack.push_back(breakLabel);
                mLabelStack.push_back(labelAtIncrement);
                genCodeForStatement(f->stmt, static_cg, finallyLabel);
                mLabelStack.pop_back();
                mLabelStack.pop_back();

            setLabel(labelAtIncrement);
                objectReadRef->emitCodeSequence(this);
                addOp(GetInvokePropertyOp);
                addStringRef(m_cx->Next_StringAtom);
                iteratorReadRef->emitCodeSequence(this);
                addOpAdjustDepth(InvokeOp, -2);
                addLong(1);
                addByte(Explicit);
                iteratorWriteRef->emitCodeSequence(this);
                addOp(PopOp);

            setLabel(labelAtTestCondition);
                iteratorReadRef->emitCodeSequence(this);
                addOp(LoadConstantNullOp);
                addOp(DoOperatorOp);
                addByte(Equal);
                addOp(JumpTrueOp);
                addFixup(labelAtEnd);
                addOp(JumpOp);
                addFixup(labelAtTopOfBlock);

            setLabel(breakLabel);
                objectReadRef->emitCodeSequence(this);
                addOp(GetInvokePropertyOp);
                addStringRef(m_cx->Done_StringAtom);
                iteratorReadRef->emitCodeSequence(this);
                addOpAdjustDepth(InvokeOp, -2);
                addLong(1);
                addByte(Explicit);
                addOp(PopOp);

            setLabel(labelAtEnd);
/*
            if (valueBaseDepth) {
                if (valueBaseDepth > 1) {
                    addOpAdjustDepth(PopNOp, -valueBaseDepth);
                    addShort(valueBaseDepth);
                }
                else
                    addOp(PopOp);
            }
*/
            delete objectReadRef;
            delete objectWriteRef;
            delete iteratorReadRef;
            delete iteratorWriteRef;
        }
        break;
    case StmtNode::For:
        {
            ForStmtNode *f = checked_cast<ForStmtNode *>(p);
            uint32 breakLabel = getLabel(Label::BreakLabel);
            uint32 labelAtTopOfBlock = getLabel();
            uint32 labelAtIncrement = getLabel(Label::ContinueLabel); 
            uint32 labelAtTestCondition = getLabel(); 

            if (f->initializer) 
                genCodeForStatement(f->initializer, static_cg, finallyLabel);
            addOp(JumpOp);
            addFixup(labelAtTestCondition);

            setLabel(labelAtTopOfBlock);

            mLabelStack.push_back(breakLabel);
            mLabelStack.push_back(labelAtIncrement);
            genCodeForStatement(f->stmt, static_cg, finallyLabel);
            mLabelStack.pop_back();
            mLabelStack.pop_back();

            setLabel(labelAtIncrement);
            if (f->expr3) {
                genExpr(f->expr3);
                addOp(PopOp);
            }

            setLabel(labelAtTestCondition);
            if (f->expr2) {
                genExpr(f->expr2);
                addOp(ToBooleanOp);
                addOp(JumpTrueOp);
                addFixup(labelAtTopOfBlock);
            }
            else {
                addOp(JumpOp);
                addFixup(labelAtTopOfBlock);
            }

            setLabel(breakLabel);
        }
        break;
    case StmtNode::label:
        {
            LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
            mLabelStack.push_back(getLabel(l));
            uint32 breakLabel = getLabel(Label::BreakLabel);
            mLabelStack.push_back(breakLabel);
            genCodeForStatement(l->stmt, static_cg, finallyLabel);
            setLabel(breakLabel);
            mLabelStack.pop_back();
            mLabelStack.pop_back();
        }
        break;
    case StmtNode::Break:
        {
            if (finallyLabel != NotALabel) {
                addOp(JsrOp);
                addFixup(finallyLabel);
            }

            GoStmtNode *g = checked_cast<GoStmtNode *>(p);
            addOp(JumpOp);
            if (g->name)
                addFixup(getTopLabel(Label::BreakLabel, g->name, p));
            else
                addFixup(getTopLabel(Label::BreakLabel, p));
        }
        break;
    case StmtNode::Continue:
        {
            GoStmtNode *g = checked_cast<GoStmtNode *>(p);
            addOp(JumpOp);
            if (g->name)
                addFixup(getTopLabel(Label::ContinueLabel, g->name, p));
            else
                addFixup(getTopLabel(Label::ContinueLabel, p));
        }
        break;
    case StmtNode::Switch:
        {
/*
            <swexpr>        
            SetVarOp    <switchTemp>
            Pop

        // test sequence in source order except 
        // the default is moved to end.

            GetVarOp    <switchTemp>
            <case1expr>
            Equal
            JumpTrue --> case1StmtLabel
            GetVarOp    <switchTemp>
            <case2expr>
            Equal
            JumpTrue --> case2StmtLabel
            Jump --> default, if there is one, or break label

    case1StmtLabel:
            <stmt>
    case2StmtLabel:
            <stmt>
    defaultLabel:
            <stmt>
    case3StmtLabel:
            <stmt>
            ..etc..     // all in source order
    
    breakLabel:
*/
            uint32 breakLabel = getLabel(Label::BreakLabel);
            uint32 defaultLabel = toUInt32(-1);

            Reference *switchTempReadRef, *switchTempWriteRef;
            mScopeChain->defineTempVariable(m_cx, switchTempReadRef, switchTempWriteRef, Object_Type);

            SwitchStmtNode *sw = checked_cast<SwitchStmtNode *>(p);
            genExpr(sw->expr);
            switchTempWriteRef->emitCodeSequence(this);
            addOp(PopOp);

            StmtNode *s = sw->statements;
            while (s) {
                if (s->getKind() == StmtNode::Case) {
                    ExprStmtNode *c = checked_cast<ExprStmtNode *>(s);
                    c->label = getLabel();
                    if (c->expr) {
                        switchTempReadRef->emitCodeSequence(this);
                        genExpr(c->expr);
                        addOp(DoOperatorOp);
                        addByte(SpittingImage);
                        addOp(JumpTrueOp);
                        addFixup(c->label);
                    }
                    else
                        defaultLabel = c->label;
                }
                s = s->next;
            }
            addOp(JumpOp);
            if (defaultLabel != toUInt32(-1))
                addFixup(defaultLabel);
            else
                addFixup(breakLabel);          
            
            s = sw->statements;
            mLabelStack.push_back(breakLabel);
            while (s) {
                if (s->getKind() == StmtNode::Case) {
                    ExprStmtNode *c = checked_cast<ExprStmtNode *>(s);
                    setLabel(c->label);
                }
                else
                    genCodeForStatement(s, static_cg, finallyLabel);
                s = s->next;
            }
            mLabelStack.pop_back();
            setLabel(breakLabel);

            delete switchTempReadRef;
            delete switchTempWriteRef;
        }
        break;
    case StmtNode::If:
        {
            UnaryStmtNode *i = checked_cast<UnaryStmtNode *>(p);
            genExpr(i->expr);
            addOp(ToBooleanOp);
            addOp(JumpFalseOp);
            uint32 label = getLabel(); 
            addFixup(label);
            genCodeForStatement(i->stmt, static_cg, finallyLabel);
            setLabel(label);
        }
        break;
    case StmtNode::IfElse:
        {
            BinaryStmtNode *i = checked_cast<BinaryStmtNode *>(p);
            genExpr(i->expr);
            addOp(ToBooleanOp);
            addOp(JumpFalseOp);
            uint32 elseStatementLabel = getLabel(); 
            addFixup(elseStatementLabel);
            result = genCodeForStatement(i->stmt, static_cg, finallyLabel);
            addOp(JumpOp);
            uint32 branchAroundElselabel = getLabel(); 
            addFixup(branchAroundElselabel);
            setLabel(elseStatementLabel);
            result &= genCodeForStatement(i->stmt2, static_cg, finallyLabel);
            setLabel(branchAroundElselabel);
        }
        break;
    case StmtNode::block:
        {
            BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
            StmtNode *s = b->statements;
            while (s) {
                result = genCodeForStatement(s, static_cg, finallyLabel);
                s = s->next;
            }            
        }
        break;
    case StmtNode::Return:
        {
            JSFunction *container = mScopeChain->getContainerFunction();
            if (!container)                    
                m_cx->reportError(Exception::syntaxError, "return statement outside of function", p->pos);
            ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
            if (e->expr) {
                genExpr(e->expr);
                if (container->getResultType() != Object_Type) {
                    addOp(LoadTypeOp);
                    addPointer(container->getResultType());
                    addOp(CastOp);
                }
                ASSERT(mStackTop == 1);
                addOpSetDepth(ReturnOp, 0);
            }
            else {
                ASSERT(mStackTop == 0);
                addOpSetDepth(ReturnVoidOp, 0);
            }
            result = true;
        }
        break;
    case StmtNode::expression:
        {
            ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
            genExpr(e->expr);
            addOp(PopOp);
        }
        break;
    case StmtNode::empty:
        /* nada */
        break;
    case StmtNode::Throw:
        {
            ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
            genExpr(e->expr);
            addOpSetDepth(ThrowOp, 0);
        }
        break;
    case StmtNode::With:
        {
            UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
            JSType *objType = genExpr(w->expr);
            addOp(WithinOp);
            mScopeChain->addScope(objType);
            genCodeForStatement(w->stmt, static_cg, finallyLabel);
            mScopeChain->popScope();
            addOp(WithoutOp);
        }
        break;
    case StmtNode::Try:
        {
/*

            try {   //  [catch,finally] handler labels are pushed on try stack
                    <tryblock>
                }   //  catch handler label is popped off try stack
                jsr finally
                jump-->finished                 

            finally:        // finally handler label popped off
                {           // a throw from in here goes to the 'next' handler
                }
                rts

            finallyInvoker:              <---
                push exception              |
                jsr finally                 |--- the handler labels 
                throw exception             | 
                                            |
            catchLabel:                  <---
                catch (exception) { // catch handler label popped off
                        // any throw from in here must jump to the finallyInvoker
                        // (i.e. not the catch handler!)
                    the incoming exception
                    is on the top of the stack. it
                    get stored into the variable
                    we've associated with the catch clause

                }
                // 'normal' fall thru from catch
                jsr finally

            finished:
*/
            TryStmtNode *t = checked_cast<TryStmtNode *>(p);

            uint32 catchClauseLabel;
            uint32 finallyInvokerLabel;
            uint32 t_finallyLabel;
            addOp(TryOp);
            if (t->finally) {
                finallyInvokerLabel = getLabel();
                addFixup(finallyInvokerLabel);            
                t_finallyLabel = getLabel(); 
            }
            else {
                finallyInvokerLabel = NotALabel;
                addLong(NotALabel);
                t_finallyLabel = NotALabel;
            }
            if (t->catches) {
                catchClauseLabel = getLabel();
                addFixup(catchClauseLabel);            
            }
            else {
                catchClauseLabel = NotALabel;
                addLong(NotALabel);
            }
            uint32 finishedLabel = getLabel();

            genCodeForStatement(t->stmt, static_cg, t_finallyLabel);

            if (t->finally) {
                addOp(JsrOp);
                addFixup(t_finallyLabel);
                addOp(JumpOp);
                addFixup(finishedLabel);
                setLabel(t_finallyLabel);
                addOp(HandlerOp);
                genCodeForStatement(t->finally, static_cg, finallyLabel);
                addOp(RtsOp);

                setLabel(finallyInvokerLabel);
                // the exception object is on the top of the stack already
                addOp(JsrOp);
                addFixup(t_finallyLabel);
                addOpSetDepth(ThrowOp, 0);

            }
            else {
                addOp(JumpOp);
                addFixup(finishedLabel);
            }

            if (t->catches) {
                setLabel(catchClauseLabel);
                addOp(HandlerOp);
                CatchClause *c = t->catches;
                ASSERT(mStackTop == 0);
                mStackTop = 1;
                if (mStackMax < 1) mStackMax = 1;
                while (c) {
                    Reference *ref = mScopeChain->getName(c->name, CURRENT_ATTR, Write);
                    ref->emitImplicitLoad(this);
                    ref->emitCodeSequence(this);
                    delete ref;
                    genCodeForStatement(c->stmt, static_cg, t_finallyLabel);
                    c = c->next;
                    if (c) {
                        mStackTop = 1;
                        Reference *ref = mScopeChain->getName(c->name, CURRENT_ATTR, Read);
                        ref->emitCodeSequence(this);
                        delete ref;
                    }
                }
                addOp(PopOp);       // the exception object has persisted 
                                    // on the top of the stack until here
                if (t->finally) {
                    addOp(JsrOp);
                    addFixup(t_finallyLabel);
                }
            }
            setLabel(finishedLabel);
        }
        break;
    case StmtNode::Use:
        {
            UseStmtNode *u = checked_cast<UseStmtNode *>(p);
            ExprList *eList = u->namespaces;
            while (eList) {
                ExprNode *e = eList->expr;
                if (e->getKind() == ExprNode::identifier) {
                    NOT_REACHED("implement me");
                    // ***** What is this supposed to do?  id is not used anywhere.
                    // AttributeList *id = new(m_cx->mArena) AttributeList(e);
                    // id->next = CURRENT_ATTR;
                }
                else
                    NOT_REACHED("implement me");
                eList = eList->next;
            }

        }
        break;
    case StmtNode::Namespace:
        {
            // do anything at bytecodegen?
        }
        break;
    default:
        NOT_REACHED("Not Implemented Yet");
    }
    return result;
}

Reference *ByteCodeGen::genReference(ExprNode *p, Access acc)
{
    switch (p->getKind()) {
    case ExprNode::parentheses:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            return genReference(u->op, acc);
        }
    case ExprNode::index:
        {
            InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
            genExpr(i->op);
            ExprPairList *p = i->pairs;
            uint16 dimCount = 0;
            while (p) {
                genExpr(p->value);
                dimCount++;
                p = p->next;
            }
            Reference *ref = new ElementReference(acc, dimCount);
            return ref;
        }
    case ExprNode::identifier:
        {
            const StringAtom &name = checked_cast<IdentifierExprNode *>(p)->name;
            Reference *ref = mScopeChain->getName(name, CURRENT_ATTR, acc);            
            if (ref == NULL)
                ref = new NameReference(name, acc);
            ref->emitImplicitLoad(this);
            return ref;
        }
    case ExprNode::dot:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            
            JSType *lType = NULL;

            // Optimize for ClassName.identifier. If we don't
            // do this we simply generate a getProperty op
            // against the Type_Type object the leftside has found.
            //
            // If we find it, emit the code to 'load' the class
            // (which loads the static instance) and the name 
            // lookup can then proceed against the static type.
            //

            if (b->op1->getKind() == ExprNode::identifier) {
                const StringAtom &name = checked_cast<IdentifierExprNode *>(b->op1)->name;
                JSValue v = mScopeChain->getCompileTimeValue(name, NULL);
                if (v.isType()) {
                    lType = v.type;
                    genExpr(b->op1);
                }
            }

            if (lType == NULL)
                lType = genExpr(b->op1);    // generate code for leftside of dot
            if (b->op2->getKind() == ExprNode::qualify) {
                QualifyExprNode *qe = checked_cast<QualifyExprNode *>(b->op2);
                ASSERT(qe->qualifier->getKind() == ExprNode::identifier);   // XXX handle more complex...

                const StringAtom &fieldName = checked_cast<IdentifierExprNode *>(b->op2)->name;
                const StringAtom &qualifierName = checked_cast<IdentifierExprNode *>(qe->qualifier)->name;

                NamespaceList *oldNS = mNamespaceList;
                mNamespaceList = new NamespaceList(&qualifierName, mNamespaceList);

                Reference *ref = lType->genReference(true, fieldName, mNamespaceList, acc, Property::NoAttribute);
                if (ref == NULL)
                    ref = new PropertyReference(fieldName, acc, Object_Type, 0);

                delete mNamespaceList;
                mNamespaceList = oldNS;

                return ref;
            }
            else {
                ASSERT(b->op2->getKind() == ExprNode::identifier);
                const StringAtom &fieldName = checked_cast<IdentifierExprNode *>(b->op2)->name;
                Reference *ref = lType->genReference(true, fieldName, CURRENT_ATTR, acc, 0);
                if (ref == NULL)
                    ref = new PropertyReference(fieldName, acc, Object_Type, Property::NoAttribute);
                return ref;
            }

        }
    default:    // return NULL here rather than throwing an exception, 
                // for (e.g.) typeof foo(), we use the NULL reference to signal
                // the distinction between lvalue & rvalue cases.
        return NULL;
    }
    return NULL;
}

void ByteCodeGen::genReferencePair(ExprNode *p, Reference *&readRef, Reference *&writeRef)
{
    switch (p->getKind()) {
    case ExprNode::identifier:
        {
            const StringAtom &name = checked_cast<IdentifierExprNode *>(p)->name;
            readRef = mScopeChain->getName(name, CURRENT_ATTR, Read);            
            if (readRef == NULL)
                readRef = new NameReference(name, Read);
            writeRef = mScopeChain->getName(name, CURRENT_ATTR, Write);            
            if (writeRef == NULL)
                writeRef = new NameReference(name, Write);
            readRef->emitImplicitLoad(this);
        }
        break;
    case ExprNode::index:
        {
            InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
            genExpr(i->op);
            ExprPairList *p = i->pairs;
            uint16 dimCount = 0;
            while (p) {
                genExpr(p->value);
                dimCount++;
                p = p->next;
            }
            readRef = new ElementReference(Read, dimCount);
            writeRef = new ElementReference(Write, dimCount);
        }
        break;
    case ExprNode::dot:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            
            JSType *lType = NULL;

            if (b->op1->getKind() == ExprNode::identifier) {
                const StringAtom &name = checked_cast<IdentifierExprNode *>(b->op1)->name;
                JSValue v = mScopeChain->getCompileTimeValue(name, NULL);
                if (v.isType()) {
                    lType = v.type;
                    genExpr(b->op1);
                }
            }

            if (lType == NULL)
                lType = genExpr(b->op1);    // generate code for leftside of dot
            if (b->op2->getKind() != ExprNode::identifier) {
                // this is where we handle n.q::id
            }
            else {
                const StringAtom &fieldName = checked_cast<IdentifierExprNode *>(b->op2)->name;
                readRef = lType->genReference(true, fieldName, CURRENT_ATTR, Read, 0);
                if (readRef == NULL)
                    readRef = new PropertyReference(fieldName, Read, Object_Type, Property::NoAttribute);
                writeRef = lType->genReference(true, fieldName, CURRENT_ATTR, Write, 0);
                if (writeRef == NULL)
                    writeRef = new PropertyReference(fieldName, Write, Object_Type, Property::NoAttribute);
            }
        }
        break;
    default:
        NOT_REACHED("Bad genReferencePair op");
    }
}


JSType *ByteCodeGen::genExpr(ExprNode *p)
{
    Operator op;

    switch (p->getKind()) {
    case ExprNode::boolean:
        addOp(checked_cast<BooleanExprNode *>(p)->value ? LoadConstantTrueOp : LoadConstantFalseOp);
        return Boolean_Type;
    case ExprNode::Null:
        addOp(LoadConstantNullOp);
        return Object_Type;
    case ExprNode::number :
        addOp(LoadConstantNumberOp);
        addNumberRef(checked_cast<NumberExprNode *>(p)->value);
        return Number_Type;
    case ExprNode::string :
        addOp(LoadConstantStringOp);
        addStringRef(checked_cast<StringExprNode *>(p)->str);
        return String_Type;

    case ExprNode::add:
        op = Plus;
        goto BinaryOperator;
    case ExprNode::subtract:
        op = Minus;
        goto BinaryOperator;
    case ExprNode::multiply:
        op = Multiply;
        goto BinaryOperator;
    case ExprNode::divide:
        op = Divide;
        goto BinaryOperator;
    case ExprNode::modulo:
        op = Remainder;
        goto BinaryOperator;
    case ExprNode::leftShift:
        op = ShiftLeft;
        goto BinaryOperator;
    case ExprNode::rightShift:
        op = ShiftRight;
        goto BinaryOperator;
    case ExprNode::logicalRightShift:
        op = UShiftRight;
        goto BinaryOperator;
    case ExprNode::bitwiseAnd:
        op = BitAnd;
        goto BinaryOperator;
    case ExprNode::bitwiseXor:
        op = BitXor;
        goto BinaryOperator;
    case ExprNode::bitwiseOr:
        op = BitOr;
        goto BinaryOperator;
    case ExprNode::lessThan:
        op = Less;
        goto BinaryOperator;
    case ExprNode::lessThanOrEqual:
        op = LessEqual;
        goto BinaryOperator;
    case ExprNode::In:
        op = In;
        goto BinaryOperator;
    case ExprNode::equal:
        op = Equal;
        goto BinaryOperator;
    case ExprNode::identical:
        op = SpittingImage;
        goto BinaryOperator;

BinaryOperator:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            genExpr(b->op1);
            genExpr(b->op2);
            addOp(DoOperatorOp);
            addByte(op);
            return Object_Type;
        }

    case ExprNode::notEqual:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            genExpr(b->op1);
            genExpr(b->op2);
            addOp(DoOperatorOp);
            addByte(Equal);
            addOp(LogicalNotOp);
            return Object_Type;
        }
    case ExprNode::greaterThan:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            genExpr(b->op1);
            genExpr(b->op2);
            addOp(SwapOp);
            addOp(DoOperatorOp);
            addByte(Less);
            return Object_Type;
        }
    case ExprNode::greaterThanOrEqual:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            genExpr(b->op1);
            genExpr(b->op2);
            addOp(SwapOp);
            addOp(DoOperatorOp);
            addByte(LessEqual);
            return Object_Type;
        }
    case ExprNode::notIdentical:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            genExpr(b->op1);
            genExpr(b->op2);
            addOp(DoOperatorOp);
            addByte(SpittingImage);
            addOp(LogicalNotOp);
            return Object_Type;
        }

    case ExprNode::minus:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            genExpr(u->op);
            addOp(DoUnaryOp);
            addByte(Negate);
            return Object_Type;
        }

    case ExprNode::plus:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            genExpr(u->op);
            addOp(DoUnaryOp);
            addByte(Posate);
            return Object_Type;
        }

    case ExprNode::complement:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            genExpr(u->op);
            addOp(DoUnaryOp);
            addByte(Complement);
            return Object_Type;
        }

    case ExprNode::preIncrement:
//        op = Increment;
        op = Plus;
        goto PreXcrement;
    case ExprNode::preDecrement:
//        op = Decrement;
        op = Minus;
        goto PreXcrement;

PreXcrement:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            Reference *readRef;
            Reference *writeRef;            
            genReferencePair(u->op, readRef, writeRef);
            uint16 baseDepth = readRef->baseExpressionDepth();
            if (baseDepth) {         // duplicate the base expression
                if (baseDepth > 1) {
                    addOpAdjustDepth(DupNOp, baseDepth);
                    addShort(baseDepth);
                }
                else
                    addOp(DupOp);
            }
            readRef->emitCodeSequence(this);
            addOp(DoUnaryOp);       
            addByte(Posate);
            addOp(LoadConstantNumberOp);
            addNumberRef(1.0);
            addOp(DoOperatorOp);
            addByte(op);
            writeRef->emitCodeSequence(this);
            delete readRef;
            delete writeRef;
            return Object_Type;
        }

    case ExprNode::postIncrement:
//        op = Increment;
        op = Plus;
        goto PostXcrement;
    case ExprNode::postDecrement:
//        op = Decrement;
        op = Minus;
        goto PostXcrement;

PostXcrement:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            Reference *readRef;
            Reference *writeRef;
            genReferencePair(u->op, readRef, writeRef);
            uint16 baseDepth = readRef->baseExpressionDepth();
            if (baseDepth) {         // duplicate the base expression
                if (baseDepth > 1) {
                    addOpAdjustDepth(DupNOp, baseDepth);
                    addShort(baseDepth);
                }
                else
                    addOp(DupOp);
                readRef->emitCodeSequence(this);
                   // duplicate the value and bury it
                if (baseDepth > 1) {
                    addOpAdjustDepth(DupInsertNOp, baseDepth);
                    addShort(baseDepth);
                }
                else
                    addOp(DupInsertOp);
            }
            else {
                readRef->emitCodeSequence(this);
                addOp(DupOp);
            }
            addOp(DoUnaryOp);       
            addByte(Posate);
            addOp(LoadConstantNumberOp);
            addNumberRef(1.0);
            addOp(DoOperatorOp);
            addByte(op);
            writeRef->emitCodeSequence(this);
            addOp(PopOp);     // because the SetXXX will propogate the new value
            delete readRef;
            delete writeRef;
            return Object_Type;
        }

    case ExprNode::addEquals:
        op = Plus;
        goto BinaryOpEquals;
    case ExprNode::subtractEquals:
        op = Minus;
        goto BinaryOpEquals;
    case ExprNode::multiplyEquals:
        op = Multiply;
        goto BinaryOpEquals;
    case ExprNode::divideEquals:
        op = Divide;
        goto BinaryOpEquals;
    case ExprNode::moduloEquals:
        op = Remainder;
        goto BinaryOpEquals;
    case ExprNode::leftShiftEquals:
        op = ShiftLeft;
        goto BinaryOpEquals;
    case ExprNode::rightShiftEquals:
        op = ShiftRight;
        goto BinaryOpEquals;
    case ExprNode::logicalRightShiftEquals:
        op = UShiftRight;
        goto BinaryOpEquals;
    case ExprNode::bitwiseAndEquals:
        op = BitAnd;
        goto BinaryOpEquals;
    case ExprNode::bitwiseXorEquals:
        op = BitXor;
        goto BinaryOpEquals;
    case ExprNode::bitwiseOrEquals:
        op = BitOr;
        goto BinaryOpEquals;

BinaryOpEquals:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            Reference *readRef;
            Reference *writeRef;
            genReferencePair(b->op1, readRef, writeRef);

            uint16 baseDepth = readRef->baseExpressionDepth();
            if (baseDepth) {         // duplicate the base expression
                if (baseDepth > 1) {
                    addOpAdjustDepth(DupNOp, baseDepth);
                    addShort(baseDepth);
                }
                else
                    addOp(DupOp);
            }
            if (writeRef->emitPreAssignment(this))
                addOp(SwapOp);
            readRef->emitCodeSequence(this);
            genExpr(b->op2);
            addOp(DoOperatorOp);
            addByte(op);

            if (writeRef->mType != Object_Type) {
                addOp(LoadTypeOp);
                addPointer(writeRef->mType);
                addOp(CastOp);
            }

            writeRef->emitCodeSequence(this);
            delete readRef;
            delete writeRef;
            return Object_Type;
        }


    case ExprNode::logicalAndEquals:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            Reference *readRef;
            Reference *writeRef;
            genReferencePair(b->op1, readRef, writeRef);

            uint16 baseDepth = readRef->baseExpressionDepth();
            if (baseDepth) {         // duplicate the base expression
                if (baseDepth > 1) {
                    addOpAdjustDepth(DupNOp, baseDepth);
                    addShort(baseDepth);
                }
                else
                    addOp(DupOp);
            }

            uint32 labelAfterSecondExpr = getLabel();
            if (writeRef->emitPreAssignment(this))
                addOp(SwapOp);
            readRef->emitCodeSequence(this);
            addOp(DupOp);
            addOp(ToBooleanOp);
            addOp(JumpFalseOp);
            addFixup(labelAfterSecondExpr);
            addOp(PopOp);
            genExpr(b->op2);
            setLabel(labelAfterSecondExpr);    

            if (writeRef->mType != Object_Type) {
                addOp(LoadTypeOp);
                addPointer(writeRef->mType);
                addOp(CastOp);
            }

            writeRef->emitCodeSequence(this);
            delete readRef;
            delete writeRef;
            return Object_Type;
        }

    case ExprNode::logicalOrEquals:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            Reference *readRef;
            Reference *writeRef;
            genReferencePair(b->op1, readRef, writeRef);

            uint16 baseDepth = readRef->baseExpressionDepth();
            if (baseDepth) {         // duplicate the base expression
                if (baseDepth > 1) {
                    addOpAdjustDepth(DupNOp, baseDepth);
                    addShort(baseDepth);
                }
                else
                    addOp(DupOp);
            }

            uint32 labelAfterSecondExpr = getLabel();
            if (writeRef->emitPreAssignment(this))
                addOp(SwapOp);
            readRef->emitCodeSequence(this);
            addOp(DupOp);
            addOp(ToBooleanOp);
            addOp(JumpTrueOp);
            addFixup(labelAfterSecondExpr);
            addOp(PopOp);
            genExpr(b->op2);
            setLabel(labelAfterSecondExpr);    

            if (writeRef->mType != Object_Type) {
                addOp(LoadTypeOp);
                addPointer(writeRef->mType);
                addOp(CastOp);
            }

            writeRef->emitCodeSequence(this);
            delete readRef;
            delete writeRef;
            return Object_Type;
        }

    case ExprNode::logicalXorEquals:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            Reference *readRef;
            Reference *writeRef;
            genReferencePair(b->op1, readRef, writeRef);

            uint16 baseDepth = readRef->baseExpressionDepth();
            if (baseDepth) {         // duplicate the base expression
                if (baseDepth > 1) {
                    addOpAdjustDepth(DupNOp, baseDepth);
                    addShort(baseDepth);
                }
                else
                    addOp(DupOp);
            }

            if (writeRef->emitPreAssignment(this))
                addOp(SwapOp);
            readRef->emitCodeSequence(this);
            genExpr(b->op2);
            addOp(LogicalXorOp);

            if (writeRef->mType != Object_Type) {
                addOp(LoadTypeOp);
                addPointer(writeRef->mType);
                addOp(CastOp);
            }

            writeRef->emitCodeSequence(this);
            delete readRef;
            delete writeRef;
            return Object_Type;
        }

    case ExprNode::logicalAnd:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            uint32 labelAfterSecondExpr = getLabel();
            genExpr(b->op1);
            addOp(DupOp);
            addOp(ToBooleanOp);
            addOp(JumpFalseOp);
            addFixup(labelAfterSecondExpr);
            addOp(PopOp);
            genExpr(b->op2);
            setLabel(labelAfterSecondExpr);            
            return Object_Type;
        }
    case ExprNode::logicalXor:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            genExpr(b->op1);
            addOp(DupOp);
            addOp(ToBooleanOp);
            genExpr(b->op2);
            addOp(DupInsertOp);
            addOp(ToBooleanOp);            
            addOp(LogicalXorOp);
            return Object_Type;
        }
    case ExprNode::logicalOr:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            uint32 labelAfterSecondExpr = getLabel();
            genExpr(b->op1);
            addOp(DupOp);
            addOp(ToBooleanOp);
            addOp(JumpTrueOp);
            addFixup(labelAfterSecondExpr);
            addOp(PopOp);
            genExpr(b->op2);
            setLabel(labelAfterSecondExpr);            
            return Object_Type;
        }
        
    case ExprNode::logicalNot:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            genExpr(u->op);
            addOp(ToBooleanOp);
            addOp(LogicalNotOp);
            return Boolean_Type;
        }

    case ExprNode::assignment:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            Reference *ref = genReference(b->op1, Write);
            if (ref == NULL)
                m_cx->reportError(Exception::semanticError, "incomprehensible assignment designate (and error message)", p->pos);
            if (ref->isConst())
                m_cx->reportError(Exception::semanticError, "assignment to const not allowed", p->pos);

            ref->emitPreAssignment(this);
            genExpr(b->op2);

            if (ref->mType != Object_Type) {
                addOp(LoadTypeOp);
                addPointer(ref->mType);
                addOp(CastOp);
            }

            ref->emitCodeSequence(this);
            delete ref;
            return Object_Type;
        }
    case ExprNode::identifier:
        {
            Reference *ref = genReference(p, Read);
            ref->emitCodeSequence(this);
            JSType *type = ref->mType;
            delete ref;
            return type;
        }
    case ExprNode::This:
        {
            JSFunction *f = mScopeChain->getContainerFunction();
            JSType *theClass;
			if (f) 
				theClass = f->getClass();
			else
				theClass = mScopeChain->topClass();            
            // 'this' is legal in prototype functions
            // and at the script top-level
            if ( ((f == NULL) && theClass)
                    || ((theClass == NULL) && f && !f->isPrototype()) )
                m_cx->reportError(Exception::referenceError, "Illegal use of 'this'", p->pos);
            addOp(LoadThisOp);
            if (theClass)
                return theClass;
            else
                return Object_Type;
        }
    case ExprNode::dot:
        {
            Reference *ref = genReference(p, Read);
            ref->emitCodeSequence(this);
            JSType *type = ref->mType;
            delete ref;
            return type;
        }
    case ExprNode::Delete:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            Reference *ref = genReference(u->op, Read);
            if (ref == NULL)
                addOp(LoadConstantTrueOp);
            else {
                ref->emitDelete(this);
                delete ref;
            }
            return Boolean_Type;
        }
    case ExprNode::Typeof:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            Reference *ref = genReference(u->op, Read);
            if (ref == NULL) {
                genExpr(u->op);
                addOp(TypeOfOp);
            }
            else {
                ref->emitTypeOf(this);
                delete ref;
            }
            return String_Type;
        }
    case ExprNode::New:
        {
            InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
            JSType *type = genExpr(i->op);

            ExprPairList *p = i->pairs;
            int32 argCount = 0;
            while (p) {
                genExpr(p->value);
                argCount++;
                p = p->next;
            }
            // A NewInstanceOp actually ends up adding one more
            // value onto the stack, before removing the arguments
            // and type value
            stretchStack(1);
            addOpAdjustDepth(NewInstanceOp, -argCount);
            addLong(toUInt32(argCount));
            return type;
        }
    case ExprNode::index:
        {
            Reference *ref = genReference(p, Read);
            ref->emitCodeSequence(this);
            delete ref;
            return Object_Type;
        }
    case ExprNode::call:
        {
            InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
            Reference *ref = genReference(i->op, Read);
            if (ref == NULL)
                m_cx->reportError(Exception::referenceError, "Unrecogizable call target", p->pos);

            ref->emitInvokeSequence(this);

            uint8 callFlags = 0;

            ExprPairList *p = i->pairs;
            int32 argCount = 0;
            while (p) {
                genExpr(p->value);
                if (p->field) {
                    callFlags |= NamedArguments;
                    genExpr(p->field);
                    addOp(NamedArgOp);
                }
                argCount++;
                p = p->next;
            }

            if (ref->needsThis()) {
                addOpAdjustDepth(InvokeOp, -(argCount + 1));
                addLong(toUInt32(argCount));
                callFlags |= Explicit;
            }
            else {
                addOpAdjustDepth(InvokeOp, -argCount);
                addLong(toUInt32(argCount));
                callFlags |= NoThis;
            }
            if (i->isSuperInvoke)
                callFlags |= SuperInvoke;
            addByte(callFlags);
            JSType *type = ref->mType;
            delete ref;
            return type;
        }
    case ExprNode::parentheses:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            return genExpr(u->op);
        }
    case ExprNode::conditional:
        {
            uint32 falseConditionExpression = getLabel();
            uint32 labelAtBottom = getLabel();

            TernaryExprNode *c = checked_cast<TernaryExprNode *>(p);
            genExpr(c->op1);
            addOp(ToBooleanOp);
            addOp(JumpFalseOp);
            addFixup(falseConditionExpression);
            genExpr(c->op2);
            addOp(JumpOp);
            addFixup(labelAtBottom);
            setLabel(falseConditionExpression);
            adjustStack(-1);        // the true case will leave a stack entry pending
                                    // but we can discard it since only path will be taken.
            genExpr(c->op3);
            setLabel(labelAtBottom);
            return Object_Type;
        }
    case ExprNode::objectLiteral:
        {
            addOp(NewObjectOp);
            PairListExprNode *plen = checked_cast<PairListExprNode *>(p);
            ExprPairList *e = plen->pairs;
            while (e) {
                if (e->field && e->value) {
                    addOp(DupOp);
                    genExpr(e->value);
                    addOp(SetPropertyOp);
                    switch (e->field->getKind()) {
                    case ExprNode::identifier:
                        addStringRef(checked_cast<IdentifierExprNode *>(e->field)->name);
                        break;
                    case ExprNode::string:
                        addStringRef(checked_cast<StringExprNode *>(e->field)->str);
                        break;
                    case ExprNode::number:
                        addStringRef(*numberToString(checked_cast<NumberExprNode *>(e->field)->value));
                        break;
                    default:
                        NOT_REACHED("bad field name");
                    }
                    addOp(PopOp);
                }
                e = e->next;
            }
        }
        break;
    case ExprNode::arrayLiteral:
        {
            addOp(LoadTypeOp);
            addPointer(Array_Type);
            addOpAdjustDepth(NewInstanceOp, 0);
            addLong(0);
            PairListExprNode *plen = checked_cast<PairListExprNode *>(p);
            ExprPairList *e = plen->pairs;
            int index = 0;
            while (e) {
                if (e->value) {
                    addOp(DupOp);
                    addOp(LoadConstantNumberOp);
                    addNumberRef(index);
                    genExpr(e->value);
                    addOpAdjustDepth(SetElementOp, -2);
                    addShort(1);
                    addOp(PopOp);
                }
                index++;
                e = e->next;
            }
        }
        break;
    case ExprNode::Is:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            genExpr(b->op1);
            genExpr(b->op2);
            addOp(IsOp);
        }
        break;
    case ExprNode::Instanceof:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            genExpr(b->op1);
            genExpr(b->op2);
            addOp(InstanceOfOp);
        }
        break;
    case ExprNode::As:
        {
            BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
            genExpr(b->op1);
            genExpr(b->op2);
            addOp(AsOp);
        }
        break;
    case ExprNode::numUnit:
        {
            // turn the unit string into a function call into the
            // Unit package, passing the number literal arguments
            // Requires winding up a new lexer/parser chunk. For
            // now we'll handle single units only
            NumUnitExprNode *n = checked_cast<NumUnitExprNode *>(p);

            addOp(LoadTypeOp);
            addPointer(Unit_Type);
            addOp(GetInvokePropertyOp);
            addStringRef(n->str);
            addOp(LoadConstantNumberOp);
            addNumberRef(n->num);
            addOp(LoadConstantStringOp);
            addStringRef(n->numStr);
            addOpAdjustDepth(InvokeOp, -2);
            addLong(2);
            addOp(NoThis);
        }
        break;
    case ExprNode::functionLiteral:
        {
            FunctionExprNode *f = checked_cast<FunctionExprNode *>(p);
            JSFunction *fnc = new JSFunction(m_cx, NULL, mScopeChain);

            uint32 reqArgCount = 0;
            uint32 optArgCount = 0;

            VariableBinding *b = f->function.parameters;
            while ((b != f->function.optParameters) && (b != f->function.restParameter)) {
                reqArgCount++;
                b = b->next;
            }
            while (b != f->function.restParameter) {
                optArgCount++;
                b = b->next;
            }
            fnc->setArgCounts(m_cx, reqArgCount, optArgCount, (f->function.restParameter != NULL));

	    if (mScopeChain->isPossibleUncheckedFunction(&f->function))
		fnc->setIsPrototype(true);

            m_cx->buildRuntimeForFunction(f->function, fnc);
            ByteCodeGen bcg(m_cx, mScopeChain);
            bcg.genCodeForFunction(f->function, f->pos, fnc, false, NULL);
            addOp(LoadFunctionOp);
            addPointer(fnc);
            addOp(NewClosureOp);
            // XXX more needed!!! how does this function get access to the
            // local variables/parameters of all the functions above on the
            // scope chain?
            // Build a list of those functions now, then at 'NewClosureOp' time
            // acquire a link for each function on the list to the current
            // activation. That activation object has to survive when those functions
            // terminate.
        }
        break;
    case ExprNode::superStmt:
        {
            // we're in a class constructor - turn this into
            // an invocation of the default constructor of the
            // superclass
            InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
            JSType *currentClass = mScopeChain->topClass();
            ASSERT(currentClass);

            addOp(LoadFunctionOp);
            addPointer(currentClass->mSuperType->getDefaultConstructor());

            ExprPairList *p = i->pairs;
            int32 argCount = 1;
            addOp(LoadThisOp);
            while (p) {
                genExpr(p->value);
                argCount++;
                p = p->next;
            }

            addOpAdjustDepth(InvokeOp, -argCount);
            addLong(toUInt32(argCount));
            addByte(Explicit | SuperInvoke);
            return currentClass;
        }
        break;
    case ExprNode::dotClass:
        {
            UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
            JSType *uType = genExpr(u->op);
            addByte(ClassOp);
            return uType;
        }
        break;
    case ExprNode::juxtapose:
        {
            BinaryExprNode *j = checked_cast<BinaryExprNode *>(p);
            genExpr(j->op1);
            genExpr(j->op2);
            addOp(JuxtaposeOp);
        }
        break;
    case ExprNode::comma:
        {
            BinaryExprNode *c = checked_cast<BinaryExprNode *>(p);
            genExpr(c->op1);
            addOp(PopOp);
            return genExpr(c->op2);
        }
        break;
    case ExprNode::Void:
        {
            UnaryExprNode *v = checked_cast<UnaryExprNode *>(p);
            genExpr(v->op);
            addOp(PopOp);
            addOp(LoadConstantUndefinedOp);
            return Object_Type;
        }
        break;
    default:
        NOT_REACHED("Not Implemented Yet");
    }
    return NULL;
}

uint32 printInstruction(Formatter &f, uint32 i, const ByteCodeModule& bcm)
{
    int32 offset;
    uint8 op = bcm.mCodeBase[i];
    f << gByteCodeData[op].opName << " ";
    i++;
    switch (op) {

    case LoadConstantUndefinedOp:
    case LoadConstantTrueOp:
    case LoadConstantFalseOp:
    case LoadConstantNullOp:
    case LoadConstantZeroOp:
    case LoadThisOp:
    case GetTypeOp:
    case CastOp:
    case ReturnOp:
    case ReturnVoidOp:
    case GetConstructorOp:
    case NewObjectOp:
    case NewThisOp:
    case TypeOfOp:
    case InstanceOfOp:
    case AsOp:
    case IsOp:
    case ToBooleanOp:
    case RtsOp:
    case WithinOp:
    case WithoutOp:
    case ThrowOp:
    case HandlerOp:
    case LogicalXorOp:
    case LogicalNotOp:
    case SwapOp:
    case DupOp:
    case DupInsertOp:
    case PopOp:
    case LoadGlobalObjectOp:
    case NewClosureOp:
    case ClassOp:
    case JuxtaposeOp:
    case NamedArgOp:
        break;

    case DeleteElementOp:
    case GetElementOp:
    case SetElementOp:
        {
            uint16 u = bcm.getShort(i);
            printFormat(f, "%hu", u);
            i += 2;
        }
        break;

    case DoUnaryOp:
    case DoOperatorOp:
        f << bcm.mCodeBase[i];
        i++;
        break;
    
    case DupNOp:
    case PopNOp:
    case DupInsertNOp:
        {
            uint16 u = bcm.getShort(i);
            printFormat(f, "%hu", u);
            i += 2;
        }
        break;

    case JumpOp:
    case JumpTrueOp:
    case JumpFalseOp:
        offset = bcm.getOffset(i);
        f << offset << " --> " << (i) + offset;
        i += 4;
        break;

    case InvokeOp:
        f << bcm.getLong(i) << " " << bcm.mCodeBase[i + 4];
        i += 5;
        break;

    case GetLocalVarOp:
    case SetLocalVarOp:
    case GetArgOp:
    case SetArgOp:
    case GetMethodOp:
    case GetMethodRefOp:
    case GetFieldOp:
    case SetFieldOp:
    case NewInstanceOp:
        f << bcm.getLong(i);
        i += 4;
        break;

    case GetClosureVarOp:
    case SetClosureVarOp:
        f << bcm.getLong(i);
        i += 4;
        f << " " << bcm.getLong(i);
        i += 4;
        break;

    case GetNameOp:
    case GetTypeOfNameOp:
    case SetNameOp:
    case GetPropertyOp:
    case GetInvokePropertyOp:
    case SetPropertyOp:
    case LoadConstantStringOp:
    case DeleteOp:
    case DeleteNameOp:
        f << *bcm.getString(bcm.getLong(i));
        i += 4;
        break;

    case LoadConstantNumberOp:
        f << bcm.getNumber(&bcm.mCodeBase[i]);
        i += 8;
        break;

    case LoadTypeOp:
    case LoadFunctionOp:
    case PushScopeOp:
        printFormat(f, "0x%X", bcm.getLong(i));
        i += 4;
        break;

    case JsrOp:
        offset = bcm.getOffset(i);
        f << offset << " --> " << i + offset;
        i += 4;
        break;

    case TryOp:
        offset = bcm.getOffset(i);
        if (offset == -1)
            f << "no finally; ";
        else
            f << "(finally) " << offset << " --> " << i + offset << "; ";
        i += 4;
        offset = bcm.getOffset(i);
        if (offset == -1)
            f << "no catch;";
        else
            f << "(catch) " << offset << " --> " << i + offset;
        i += 4;
        break;
    default:
        printFormat(f, "Unknown Opcode 0x%X", bcm.mCodeBase[i]);
        i++;
        break;
    }
    f << "\n";
    return i;
}

Formatter& operator<<(Formatter& f, const ByteCodeModule& bcm)
{
    uint32 i = 0;
    while (i < bcm.mLength) {
        printFormat(f, "%.4d ", i);
        i = printInstruction(f, i, bcm);
    }
    return f;
}


}
}

