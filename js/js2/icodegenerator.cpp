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
#include "jsclasses.h"
#include "icodegenerator.h"
#include "interpreter.h"

#include <stdexcept>
#include <stdio.h>

namespace JavaScript {
namespace ICG {

using namespace VM;
using namespace JSTypes;
using namespace JSClasses;
using namespace Interpreter;

inline char narrow(char16 ch) { return char(ch); }


uint32 ICodeModule::sMaxID = 0;
    
Formatter& operator<<(Formatter &f, ICodeGenerator &i)
{
    return i.print(f);
}

Formatter& operator<<(Formatter &f, ICodeModule &i)
{
    return i.print(f);
}

//
// ICodeGenerator
//


ICodeGenerator::ICodeGenerator(World *world, JSScope *global, JSClass *aClass, ICodeGeneratorFlags flags)
    :   mTopRegister(0), 
        mParameterCount(0),
        mExceptionRegister(TypedRegister(NotARegister, &None_Type)),
        variableList(new VariableList()),
        mWorld(world),
        mGlobal(global),
        mInstructionMap(new InstructionMap()),
        mClass(aClass),
        mFlags(flags),
        pLabels(NULL),
        mHasRestParameter(false),
        mHasNamedRestParameter(false)
{ 
    iCode = new InstructionStream();
    iCodeOwner = true;
}

JSType *ICodeGenerator::findType(const StringAtom& typeName) 
{
    const JSValue& type = mGlobal->getVariable(typeName);
    if (type.isType())
        return type.type;
    return &Any_Type;
}

TypedRegister ICodeGenerator::allocateRegister(const StringAtom& name, JSType *type) 
{
    Register r = mTopRegister;
    while (r < mPermanentRegister.size())
        if (!mPermanentRegister[r]) 
            break;
        else
            ++r;
    if (r == mPermanentRegister.size())
        mPermanentRegister.resize(r + 1);
    mPermanentRegister[r] = true;

    TypedRegister result(r, type); 
    (*variableList)[name] = result; 
    mTopRegister = ++r;
    return result;
}


TypedRegister ICodeGenerator::allocateVariable(const StringAtom& name, const StringAtom& typeName) 
{ 
    if (mExceptionRegister.first == NotARegister) {
        mExceptionRegister = allocateRegister(mWorld->identifiers["__exceptionObject__"], &Any_Type);
    }
    return allocateRegister(name, findType(typeName)); 
}

TypedRegister ICodeGenerator::allocateVariable(const StringAtom& name) 
{ 
    if (mExceptionRegister.first == NotARegister) {
        mExceptionRegister = allocateRegister(mWorld->identifiers["__exceptionObject__"], &Any_Type);
    }
    return allocateRegister(name, &Any_Type); 
}

ICodeModule *ICodeGenerator::complete()
{
#ifdef DEBUG
    for (LabelList::iterator i = labels.begin();
         i != labels.end(); i++) {
        ASSERT((*i)->mBase == iCode);
        ASSERT((*i)->mOffset <= iCode->size());
    }
#endif


    if (iCode->size()) {
        ICodeOp lastOp = (*iCode)[iCode->size() - 1]->op();
        if ((lastOp != RETURN) && (lastOp != RETURN_VOID))
            returnStmt();
    }
    else
        returnStmt();
     

    /*
    XXX FIXME
    I wanted to do the following rather than have to have the label set hanging around as well
    as the ICodeModule. Branches have since changed, but the concept is still good and should
    be re-introduced at some point.

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
    ICodeModule* module = new ICodeModule(iCode, 
                                            variableList, 
                                            mPermanentRegister.size(), 
                                            mParameterCount, 
                                            mInstructionMap, 
                                            mHasRestParameter, 
                                            mHasNamedRestParameter);
    if (pLabels) {
        uint32 i;
        uint32 parameterInits = pLabels->size() - 1;        // there's an extra label at the end for the actual entryPoint
        module->mNonOptionalParameterCount -= parameterInits;
        module->mParameterInit = new uint32[parameterInits];
        for (i = 0; i < parameterInits; i++) {
            module->mParameterInit[i] = (*pLabels)[i]->mOffset;
        }
        module->mEntryPoint = (*pLabels)[i]->mOffset;
    }
    iCodeOwner = false;   // give ownership to the module.
    return module;
}


/********************************************************************/

TypedRegister ICodeGenerator::loadImmediate(double value)
{
    TypedRegister dest(getTempRegister(), &Number_Type);
    LoadImmediate *instr = new LoadImmediate(dest, value);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::loadString(const String &value)
{
    TypedRegister dest(getTempRegister(), &String_Type);
    LoadString *instr = new LoadString(dest, new JSString(value));
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::loadString(const StringAtom &value)
{
    TypedRegister dest(getTempRegister(), &String_Type);
    LoadString *instr = new LoadString(dest, new JSString(value));
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::loadBoolean(bool value)
{
    TypedRegister dest(getTempRegister(), &Boolean_Type);
    LoadBoolean *instr = new LoadBoolean(dest, value);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::newObject(TypedRegister constructor)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    NewObject *instr = new NewObject(dest, constructor);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::newClass(JSClass *clazz)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    NewClass *instr = new NewClass(dest, clazz);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::newFunction(ICodeModule *icm, FunctionDefinition *def)
{
    TypedRegister dest(getTempRegister(), &Function_Type);
    NewFunction *instr = new NewFunction(dest, icm, def);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::newArray()
{
    TypedRegister dest(getTempRegister(), &Array_Type);
    NewArray *instr = new NewArray(dest);
    iCode->push_back(instr);
    return dest;
}



TypedRegister ICodeGenerator::loadName(const StringAtom &name, JSType *t)
{
    TypedRegister dest(getTempRegister(), t);
    LoadName *instr = new LoadName(dest, &name);
    iCode->push_back(instr);
    return dest;
}

void ICodeGenerator::saveName(const StringAtom &name, TypedRegister value)
{
    SaveName *instr = new SaveName(&name, value);
    iCode->push_back(instr);
}

TypedRegister ICodeGenerator::nameXcr(const StringAtom &name, ICodeOp op)
{
    TypedRegister dest(getTempRegister(), &Number_Type);
    NameXcr *instr = new NameXcr(dest, &name, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}



TypedRegister ICodeGenerator::varXcr(TypedRegister var, ICodeOp op)
{
    TypedRegister dest(getTempRegister(), &Number_Type);
    VarXcr *instr = new VarXcr(dest, var, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}




TypedRegister ICodeGenerator::deleteProperty(TypedRegister base, const StringAtom &name)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    DeleteProp *instr = new DeleteProp(dest, base, &name);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::getProperty(TypedRegister base, const StringAtom &name)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    GetProp *instr = new GetProp(dest, base, &name);
    iCode->push_back(instr);
    return dest;
}

void ICodeGenerator::setProperty(TypedRegister base, const StringAtom &name,
                                 TypedRegister value)
{
    SetProp *instr = new SetProp(base, &name, value);
    iCode->push_back(instr);
}

TypedRegister ICodeGenerator::propertyXcr(TypedRegister base, const StringAtom &name, ICodeOp op)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    PropXcr *instr = new PropXcr(dest, base, &name, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}





TypedRegister ICodeGenerator::getStatic(JSClass *base, const String &name)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    const JSSlot& slot = base->getStatic(name);
    GetStatic *instr = new GetStatic(dest, base, slot.mIndex);
    iCode->push_back(instr);
    return dest;
}

void ICodeGenerator::setStatic(JSClass *base, const StringAtom &name,
                                 TypedRegister value)
{
    const JSSlot& slot = base->getStatic(name);
    SetStatic *instr = new SetStatic(base, slot.mIndex, value);
    iCode->push_back(instr);
}

TypedRegister ICodeGenerator::staticXcr(JSClass *base, const StringAtom &name, ICodeOp /*op*/)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    const JSSlot& slot = base->getStatic(name);
    StaticXcr *instr = new StaticXcr(dest, base, slot.mIndex, 1.0);
    iCode->push_back(instr);
    return dest;
}




TypedRegister ICodeGenerator::getSlot(TypedRegister base, uint32 slot)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    GetSlot *instr = new GetSlot(dest, base, slot);
    iCode->push_back(instr);
    return dest;
}

void ICodeGenerator::setSlot(TypedRegister base, uint32 slot,
                                 TypedRegister value)
{
    SetSlot *instr = new SetSlot(base, slot, value);
    iCode->push_back(instr);
}

TypedRegister ICodeGenerator::slotXcr(TypedRegister base, uint32 slot, ICodeOp op)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    SlotXcr *instr = new SlotXcr(dest, base, slot, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}




TypedRegister ICodeGenerator::getElement(TypedRegister base, TypedRegister index)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    GetElement *instr = new GetElement(dest, base, index);
    iCode->push_back(instr);
    return dest;
}

void ICodeGenerator::setElement(TypedRegister base, TypedRegister index,
                                TypedRegister value)
{
    SetElement *instr = new SetElement(base, index, value);
    iCode->push_back(instr);
}

TypedRegister ICodeGenerator::elementXcr(TypedRegister base, TypedRegister index, ICodeOp op)
{
    TypedRegister dest(getTempRegister(), &Number_Type);
    ElemXcr *instr = new ElemXcr(dest, base, index, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}



TypedRegister ICodeGenerator::op(ICodeOp op, TypedRegister source)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    ASSERT(source.first != NotARegister);    
    Unary *instr = new Unary (op, dest, source);
    iCode->push_back(instr);
    return dest;
}
    
    
void ICodeGenerator::move(TypedRegister destination, TypedRegister source)
{
    ASSERT(destination.first != NotARegister);    
    ASSERT(source.first != NotARegister);    
    Move *instr = new Move(destination, source);
    iCode->push_back(instr);
}

TypedRegister ICodeGenerator::logicalNot(TypedRegister source)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    Not *instr = new Not(dest, source);
    iCode->push_back(instr);
    return dest;
} 

TypedRegister ICodeGenerator::test(TypedRegister source)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    Test *instr = new Test(dest, source);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::op(ICodeOp op, TypedRegister source1, 
                            TypedRegister source2)
{
    ASSERT(source1.first != NotARegister);    
    ASSERT(source2.first != NotARegister);    
    TypedRegister dest(getTempRegister(), &Any_Type);
    Arithmetic *instr = new Arithmetic(op, dest, source1, source2);
    iCode->push_back(instr);
    return dest;
} 
    
TypedRegister ICodeGenerator::binaryOp(ICodeOp op, TypedRegister source1, 
                            TypedRegister source2)
{
    ASSERT(source1.first != NotARegister);    
    ASSERT(source2.first != NotARegister);
    TypedRegister dest(getTempRegister(), &Any_Type);
    
    if ((source1.second == &Number_Type) && (source2.second == &Number_Type)) {
        Arithmetic *instr = new Arithmetic(op, dest, source1, source2);
        iCode->push_back(instr);
    }
    else {
        GenericBinaryOP *instr = new GenericBinaryOP(dest, BinaryOperator::mapICodeOp(op), source1, source2);
        iCode->push_back(instr);
    }
    return dest;
} 
    
TypedRegister ICodeGenerator::call(TypedRegister target, TypedRegister thisArg, ArgumentList *args)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    Call *instr = new Call(dest, target, thisArg, *args);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::directCall(JSFunction *target, ArgumentList *args)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    DirectCall *instr = new DirectCall(dest, target, *args);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::getMethod(TypedRegister thisArg, uint32 slotIndex)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    GetMethod *instr = new GetMethod(dest, thisArg, slotIndex);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::super()
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    Super *instr = new Super(dest);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::cast(TypedRegister arg, JSType *toType)
{
    TypedRegister dest(getTempRegister(), toType);
    Cast *instr = new Cast(dest, arg, toType);
    iCode->push_back(instr);
    return dest;
}

void ICodeGenerator::branch(Label *label)
{
    Branch *instr = new Branch(label);
    iCode->push_back(instr);
}

GenericBranch *ICodeGenerator::branchTrue(Label *label, TypedRegister condition)
{
    GenericBranch *instr = new GenericBranch(BRANCH_TRUE, label, condition);
    iCode->push_back(instr);
    return instr;
}

GenericBranch *ICodeGenerator::branchFalse(Label *label, TypedRegister condition)
{
    GenericBranch *instr = new GenericBranch(BRANCH_FALSE, label, condition);
    iCode->push_back(instr);
    return instr;
}

void ICodeGenerator::returnStmt(TypedRegister r)
{
    iCode->push_back(new Return(r));
}

void ICodeGenerator::returnStmt()
{
    iCode->push_back(new ReturnVoid());
}


/********************************************************************/

Label *ICodeGenerator::getLabel()
{
    labels.push_back(new Label(NULL));
    return labels.back();
}

Label *ICodeGenerator::setLabel(Label *l)
{
    l->mBase = iCode;
    l->mOffset = iCode->size();
    return l;
}

/************************************************************************/




ICodeOp ICodeGenerator::mapExprNodeToICodeOp(ExprNode::Kind kind)
{
    // can be an array later, when everything has settled down
    switch (kind) {
    // binary
    case ExprNode::add:
    case ExprNode::addEquals:
        return ADD;
    case ExprNode::subtract:
    case ExprNode::subtractEquals:
        return SUBTRACT;
    case ExprNode::multiply:
    case ExprNode::multiplyEquals:
        return MULTIPLY;
    case ExprNode::divide:
    case ExprNode::divideEquals:
        return DIVIDE;
    case ExprNode::modulo:
    case ExprNode::moduloEquals:
        return REMAINDER;
    case ExprNode::leftShift:
    case ExprNode::leftShiftEquals:
        return SHIFTLEFT;
    case ExprNode::rightShift:
    case ExprNode::rightShiftEquals:
        return SHIFTRIGHT;
    case ExprNode::logicalRightShift:
    case ExprNode::logicalRightShiftEquals:
        return USHIFTRIGHT;
    case ExprNode::bitwiseAnd:
    case ExprNode::bitwiseAndEquals:
        return AND;
    case ExprNode::bitwiseXor:
    case ExprNode::bitwiseXorEquals:
        return XOR;
    case ExprNode::bitwiseOr:
    case ExprNode::bitwiseOrEquals:
        return OR;
    // unary
    case ExprNode::plus:
        return POSATE;
    case ExprNode::minus:
        return NEGATE;
    case ExprNode::complement:
        return BITNOT;

   // relational
    case ExprNode::In:
        return COMPARE_IN;
    case ExprNode::Instanceof:
        return INSTANCEOF;

    case ExprNode::equal:
        return COMPARE_EQ;
    case ExprNode::lessThan:
        return COMPARE_LT;
    case ExprNode::lessThanOrEqual:
        return COMPARE_LE;
    case ExprNode::identical:
        return STRICT_EQ;

  // these get reversed by the generator
    case ExprNode::notEqual:        
        return COMPARE_EQ;
    case ExprNode::greaterThan:
        return COMPARE_LT;
    case ExprNode::greaterThanOrEqual:
        return COMPARE_LE;
    case ExprNode::notIdentical:
        return STRICT_EQ;


    default:
        NOT_REACHED("Unimplemented kind");
        return NOP;
    }
}


static bool generatedBoolean(ExprNode *p)
{
    switch (p->getKind()) {
    case ExprNode::parentheses: 
        {
            UnaryExprNode *u = static_cast<UnaryExprNode *>(p);
            return generatedBoolean(u->op);
        }
    case ExprNode::True:
    case ExprNode::False:
    case ExprNode::equal:
    case ExprNode::notEqual:
    case ExprNode::lessThan:
    case ExprNode::lessThanOrEqual:
    case ExprNode::greaterThan:
    case ExprNode::greaterThanOrEqual:
    case ExprNode::identical:
    case ExprNode::notIdentical:
    case ExprNode::In:
    case ExprNode::Instanceof:
    case ExprNode::logicalAnd:
    case ExprNode::logicalXor:
    case ExprNode::logicalOr:
        return true;
    default:
        break;
    }
    return false;
}

static bool isSlotName(JSType *t, const StringAtom &name, uint32 &slotIndex, JSType *&type)
{
    JSClass* c = dynamic_cast<JSClass*>(t);
    while (c) {
        if (c->hasSlot(name)) {
            const JSSlot &s = c->getSlot(name);
            slotIndex = s.mIndex;
            type = s.mType;
            return true;
        }
        c = c->getSuperClass();
    }
    return false;
}

static bool isMethodName(JSType *t, const StringAtom &name, uint32 &slotIndex)
{
    JSClass* c = dynamic_cast<JSClass*>(t);
    while (c) {
        if (c->hasMethod(name, slotIndex))
            return true;
        c = c->getSuperClass();
    }
    return false;
}


ICodeGenerator::LValueKind ICodeGenerator::resolveIdentifier(const StringAtom &name, TypedRegister &v, uint32 &slotIndex)
{
    if (!isWithinWith()) {
        v = findVariable(name);
        if (v.first != NotARegister)
            return Var;
        else {
            if (mClass) {   // we're compiling a method of a class
                if (!isStaticMethod()) {
                    if (isSlotName(mClass, name, slotIndex, v.second))
                        return Slot;
                    if (isMethodName(mClass, name, slotIndex))
                        return Method;
                }
                bool isConstructor = false;
                if (mClass->hasStatic(name, v.second, isConstructor)) {
                    return (isConstructor) ? Constructor : Static;
                }
            }
            v.second = mGlobal->getType(name);
            return Name;
        }
    }
    v.second = &Any_Type;
    return Name;
}

TypedRegister ICodeGenerator::handleIdentifier(IdentifierExprNode *p, ExprNode::Kind use, ICodeOp xcrementOp, TypedRegister ret, ArgumentList *args)
{
    ASSERT(p->getKind() == ExprNode::identifier);

    /*JSType *vType = &Any_Type;*/
    uint32 slotIndex;
    TypedRegister v;

    const StringAtom &name = (static_cast<IdentifierExprNode *>(p))->name;
    LValueKind lValueKind = resolveIdentifier(name, v, slotIndex);

    TypedRegister thisBase = TypedRegister(0, mClass ? mClass : &Any_Type);

    switch (use) {
    case ExprNode::addEquals:
    case ExprNode::subtractEquals:
    case ExprNode::multiplyEquals:
    case ExprNode::divideEquals:
    case ExprNode::moduloEquals:
    case ExprNode::leftShiftEquals:
    case ExprNode::rightShiftEquals:
    case ExprNode::logicalRightShiftEquals:
    case ExprNode::bitwiseAndEquals:
    case ExprNode::bitwiseXorEquals:
    case ExprNode::bitwiseOrEquals:
        switch (lValueKind) {
        case Var:
            break;
        case Name:
            v = loadName(name, v.second);
            break;
        case Slot:
            v = getSlot(thisBase, slotIndex);
            break;
        case Static:
        case Constructor:
            v = getStatic(mClass, name);
            break;
        default:
            NOT_REACHED("Bad lvalue kind");
        }
        ret = binaryOp(mapExprNodeToICodeOp(use), v, ret);
        // fall thru...
    case ExprNode::assignment:
        switch (lValueKind) {
        case Var:
            move(v, ret);
            break;
        case Name:
            saveName(name, ret);
            break;
        case Slot:
            setSlot(thisBase, slotIndex, ret);
            break;
        case Static:
        case Constructor:
            setStatic(mClass, name, ret);
            break;
        default:
            NOT_REACHED("Bad lvalue kind");
        }
        break;
    case ExprNode::identifier: 
        switch (lValueKind) {
        case Var:
            ret = v;
            break;
        case Name:
            ret = loadName(name, v.second);
            break;
        case Slot:
            ret = getSlot(thisBase, slotIndex);
            break;
        case Static:
        case Constructor:
            ret = getStatic(mClass, name);
            break;
        default:
            NOT_REACHED("Bad lvalue kind");
        }
        break;
    case ExprNode::preDecrement: 
    case ExprNode::preIncrement:
        switch (lValueKind) {
        case Var:
            ret = binaryOp(xcrementOp, v, loadImmediate(1.0));
            break;
        case Name:
            ret = loadName(name, v.second);
            ret = binaryOp(xcrementOp, ret, loadImmediate(1.0));
            saveName(name, ret);
            break;
        case Slot:
            ret = binaryOp(xcrementOp, getSlot(thisBase, slotIndex), loadImmediate(1.0));
            setSlot(thisBase, slotIndex, ret);
            break;
        case Static:
        case Constructor:
            ret = binaryOp(xcrementOp, getStatic(mClass, name), loadImmediate(1.0));
            setStatic(mClass, name, ret);
            break;
        default:
            NOT_REACHED("Bad lvalue kind");
        }
        break;
    case ExprNode::postDecrement: 
    case ExprNode::postIncrement:
        switch (lValueKind) {
        case Var:
            ret = varXcr(v, xcrementOp);
            break;
        case Name:
            ret = nameXcr(name, xcrementOp);
            break;
        case Slot:
            ret = slotXcr(thisBase, slotIndex, xcrementOp);
            break;
        case Static:
        case Constructor:
            ret = staticXcr(mClass, name, xcrementOp);
            break;
        default:
            NOT_REACHED("Bad lvalue kind");
        }
        break;
    case ExprNode::call: 
        {
            switch (lValueKind) {
            case Var:
                ret = call(v, TypedRegister(NotARegister, &Null_Type), args);
                break;
            case Name:
                ret = call(loadName(name), TypedRegister(NotARegister, &Null_Type), args);
                break;
            case Method:
                ret = call(getMethod(thisBase, slotIndex), thisBase, args);
                break;
            case Static:
                ret = call(getStatic(mClass, name), TypedRegister(NotARegister, &Null_Type), args);
                break;
            case Constructor:
                ret = newClass(mClass);
                call(getStatic(mClass, name), ret, args);
                break;
            default:
                NOT_REACHED("Bad lvalue kind");
            }
        }
        break;
    default:
        NOT_REACHED("Bad use kind");
    }
    return ret;

}

TypedRegister ICodeGenerator::handleDot(BinaryExprNode *b, ExprNode::Kind use, ICodeOp xcrementOp, TypedRegister ret, ArgumentList *args)
{   
    ASSERT(b->getKind() == ExprNode::dot);

    LValueKind lValueKind = Property;

    if (b->op2->getKind() != ExprNode::identifier) {
        NOT_REACHED("Implement me");    // turns into a getProperty (but not via any overloaded [] )
    }
    else {
        const StringAtom &fieldName = static_cast<IdentifierExprNode *>(b->op2)->name;
        TypedRegister base;
        JSClass *clazz = NULL;
        JSType *fieldType = &Any_Type;
        uint32 slotIndex;
        if ((b->op1->getKind() == ExprNode::identifier) && !isWithinWith()) {
            const StringAtom &baseName = (static_cast<IdentifierExprNode *>(b->op1))->name;
            resolveIdentifier(baseName, base, slotIndex);
            
            //
            // handle <class name>.<static field>
            //
            if (base.second == &Type_Type) {
                const JSValue &v = mGlobal->getVariable(baseName);
                bool isConstructor;
                ASSERT(v.isType());     // there's no other way that base.second could be &Type_Type, right?
                clazz = dynamic_cast<JSClass*>(v.type);
                if (clazz && clazz->hasStatic(fieldName, fieldType, isConstructor)) {
                    lValueKind = (isConstructor) ? Constructor : Static;
                }
            }
            if (lValueKind == Property) {
                if (isSlotName(base.second, fieldName, slotIndex, fieldType))
                    lValueKind = Slot;
                else
                    if (isMethodName(base.second, fieldName, slotIndex))
                        lValueKind = Method;
                    else {
                        bool isConstructor;
                        clazz = dynamic_cast<JSClass*>(base.second);
                        if (clazz && clazz->hasStatic(fieldName, fieldType, isConstructor))
                            lValueKind = (isConstructor) ? Constructor : Static;
                    }
            }
            if ((lValueKind == Property) || (base.first == NotARegister))
                base = loadName(baseName, base.second);
        }
        else {
            base = genExpr(b->op1);
            if (isSlotName(base.second, fieldName, slotIndex, fieldType))
                lValueKind = Slot;
            else
                if (isMethodName(base.second, fieldName, slotIndex))
                    lValueKind = Method;
                else {
                    bool isConstructor;
                    clazz = dynamic_cast<JSClass*>(base.second);
                    if (clazz && clazz->hasStatic(fieldName, fieldType, isConstructor))
                        lValueKind = (isConstructor) ? Constructor : Static;
                }
        }
        TypedRegister v;
        switch (use) {
        case ExprNode::call:
            switch (lValueKind) {
            case Static:
                ret = call(getStatic(clazz, fieldName), TypedRegister(NotARegister, &Null_Type), args);
                break;
            case Constructor:
                ret = newClass(clazz);
                call(getStatic(clazz, fieldName), ret, args);
                break;
            case Property:
                ret = call(getProperty(base, fieldName), base, args);
                break;
            case Method:
                ret = call(getMethod(base, slotIndex), base, args);
                break;
            default:
                NOT_REACHED("Bad lvalue kind");
            }
            break;
        case ExprNode::dot:
        case ExprNode::addEquals:
        case ExprNode::subtractEquals:
        case ExprNode::multiplyEquals:
        case ExprNode::divideEquals:
        case ExprNode::moduloEquals:
        case ExprNode::leftShiftEquals:
        case ExprNode::rightShiftEquals:
        case ExprNode::logicalRightShiftEquals:
        case ExprNode::bitwiseAndEquals:
        case ExprNode::bitwiseXorEquals:
        case ExprNode::bitwiseOrEquals:
            switch (lValueKind) {
            case Constructor:
            case Static:
                v = getStatic(clazz, fieldName);
                break;
            case Property:
                v = getProperty(base, fieldName);
                break;
            case Slot:
                v = getSlot(base, slotIndex);
                break;
            default:
                NOT_REACHED("Bad lvalue kind");
            }
            if (use == ExprNode::dot) {
                ret = v;
                break;
            }
            ret = binaryOp(mapExprNodeToICodeOp(use), v, ret);
            // fall thru...
        case ExprNode::assignment:
            switch (lValueKind) {
            case Constructor:
            case Static:
                setStatic(clazz, fieldName, ret);
                break;
            case Property:
                setProperty(base, fieldName, ret);
                break;
            case Slot:
                setSlot(base, slotIndex, ret);
                break;
            default:
                NOT_REACHED("Bad lvalue kind");
            }
            break;
        case ExprNode::postDecrement: 
        case ExprNode::postIncrement:
            {
//                JSClass *clss = dynamic_cast<JSClass*>(fieldType);
//                if (clss) {
//                    clss->findOverloadedOperator(use);
//                }
                switch (lValueKind) {
                case Constructor:
                case Static:
                    ret = staticXcr(clazz, fieldName, xcrementOp);
                    break;
                case Property:
                    ret = propertyXcr(base, fieldName, xcrementOp);
                    break;
                case Slot:
                    ret = slotXcr(base, slotIndex, xcrementOp);
                    break;
                default:
                    NOT_REACHED("Bad lvalue kind");
                }
            }
            break;
        case ExprNode::preDecrement: 
        case ExprNode::preIncrement:
            switch (lValueKind) {
            case Constructor:
            case Static:
                ret = binaryOp(xcrementOp, getStatic(clazz, fieldName), loadImmediate(1.0));
                setStatic(clazz, fieldName, ret);
                break;
            case Property:
                ret = binaryOp(xcrementOp, getProperty(base, fieldName), loadImmediate(1.0));
                setProperty(base, fieldName, ret);
                break;
            case Slot:
                ret = binaryOp(xcrementOp, getSlot(base, slotIndex), loadImmediate(1.0));
                setSlot(base, slotIndex, ret);
                break;
            default:
                NOT_REACHED("Bad lvalue kind");
            }
            break;
        case ExprNode::Delete:
            if (lValueKind == Property) {
                ret = deleteProperty(base, fieldName);
            }
            break;
        default:
            NOT_REACHED("unexpected use node");
        }
        ret.second = fieldType;
    }
    return ret;
}



/*
    if trueBranch OR falseBranch are not null, the sub-expression should generate
    a conditional branch to the appropriate target. If either branch is NULL, it
    indicates that the label is immediately forthcoming.
*/
TypedRegister ICodeGenerator::genExpr(ExprNode *p, 
                                    bool needBoolValueInBranch, 
                                    Label *trueBranch, 
                                    Label *falseBranch)
{
    TypedRegister ret(NotARegister, &None_Type);
    ICodeOp xcrementOp = ADD;
    switch (p->getKind()) {
    case ExprNode::True:
        if (trueBranch || falseBranch) {
            if (needBoolValueInBranch)
                ret = loadBoolean(true);
            if (trueBranch)
                branch(trueBranch);
        }
        else
            ret = loadBoolean(true);
        break;
    case ExprNode::False:
        if (trueBranch || falseBranch) {
            if (needBoolValueInBranch)
                ret = loadBoolean(false);
            if (falseBranch)
                branch(falseBranch);
        }
        else
            ret = loadBoolean(false);
        break;
    case ExprNode::parentheses:
        {
            UnaryExprNode *u = static_cast<UnaryExprNode *>(p);
            ret = genExpr(u->op, needBoolValueInBranch, trueBranch, falseBranch);
        }
        break;
    case ExprNode::New:
        {
            InvokeExprNode *i = static_cast<InvokeExprNode *>(p);
            ArgumentList args;
            ExprPairList *p = i->pairs;
            while (p) {
                if (p->field && (p->field->getKind() == ExprNode::identifier))
                    args.push_back(Argument(genExpr(p->value), &(static_cast<IdentifierExprNode *>(p->field))->name));
                else
                    args.push_back(Argument(genExpr(p->value), NULL ));
                p = p->next;
            }
            if (i->op->getKind() == ExprNode::identifier) {
                const StringAtom &className = static_cast<IdentifierExprNode *>(i->op)->name;
                const JSValue& value = mGlobal->getVariable(className);
                if (value.isType()) {
                    JSClass* clazz = dynamic_cast<JSClass*>(value.type);
                    if (clazz) {
                        ret = newClass(clazz);
                        ret = call(getStatic(clazz, className), ret, &args);
                    }
                    else {
                        //
                        // like 'new Boolean()' - see if the type has a constructor
                        //
                        JSFunction *f = value.type->getConstructor();
                        if (f)
                            ret = directCall(f, &args);
                        else
                            NOT_REACHED("new <name>, where <name> is not a new-able type (whatever that means)");   // XXX Runtime error.
                    }
                }
                else
                    if (value.isFunction()) {
                        TypedRegister f = loadName(className, value.type);
                        ret = newObject(f);
                        ret = call(f, ret, &args);
                    }
                    else
                        NOT_REACHED("new <name>, where <name> is not a function");   // XXX Runtime error.
            }                
            else
                ret = newObject(TypedRegister(NotARegister, &Any_Type));  // XXX more ?
        }
        break;
    case ExprNode::Delete:
        {
            UnaryExprNode *d = static_cast<UnaryExprNode *>(p);
            ASSERT(d->op->getKind() == ExprNode::dot);
            ret = handleDot(static_cast<BinaryExprNode *>(d->op), p->getKind(), xcrementOp, ret, NULL);
            // rather than getProperty(), need to do a deleteProperty().
        }
        break;
    case ExprNode::call : 
        {
            InvokeExprNode *i = static_cast<InvokeExprNode *>(p);
            ArgumentList args;
            ExprPairList *p = i->pairs;
            while (p) {
                if (p->field && (p->field->getKind() == ExprNode::identifier))
                    args.push_back(Argument(genExpr(p->value), &(static_cast<IdentifierExprNode *>(p->field))->name));
                else
                    args.push_back(Argument(genExpr(p->value), NULL ));
                p = p->next;
            }

            if (i->op->getKind() == ExprNode::dot) {
                BinaryExprNode *b = static_cast<BinaryExprNode *>(i->op);
                ret = handleDot(b, ExprNode::call, xcrementOp, ret, &args);
            }
            else 
                if (i->op->getKind() == ExprNode::identifier) {
                    ret = handleIdentifier(static_cast<IdentifierExprNode *>(i->op), ExprNode::call, xcrementOp, ret, &args);
                }
                else 
                    if (i->op->getKind() == ExprNode::index) {
                        BinaryExprNode *b = static_cast<BinaryExprNode *>(i->op);
                        TypedRegister base = genExpr(b->op1);
                        ret = call(getElement(base, genExpr(b->op2)), base, &args);
                    }
                    else
                        ASSERT("WAH!");
        }
        break;
    case ExprNode::index :
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister base = genExpr(b->op1);
            JSClass *clazz = dynamic_cast<JSClass*>(base.second);
            if (clazz) {
                // look for operator [] and invoke it
            }
            TypedRegister index = genExpr(b->op2);
            ret = getElement(base, index);
        }
        break;
    case ExprNode::dot :
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            ret = handleDot(b, p->getKind(), xcrementOp, ret, NULL);
        }
        break;
    case ExprNode::This :
        {
            ret = TypedRegister(0, mClass ? mClass : &Any_Type);
        }
        break;
    case ExprNode::identifier :
        {
            ret = handleIdentifier(static_cast<IdentifierExprNode *>(p), ExprNode::identifier, xcrementOp, ret, NULL);
        }
        break;
    case ExprNode::number :
        ret = loadImmediate((static_cast<NumberExprNode *>(p))->value);
        break;
    case ExprNode::string :
        ret = loadString(mWorld->identifiers[(static_cast<StringExprNode *>(p))->str]);
        break;
    case ExprNode::preDecrement: 
        xcrementOp = SUBTRACT;
    case ExprNode::preIncrement:
       {
            UnaryExprNode *u = static_cast<UnaryExprNode *>(p);
            if (u->op->getKind() == ExprNode::dot) {
                ret = handleDot(static_cast<BinaryExprNode *>(u->op), p->getKind(), xcrementOp, ret, NULL);
            }
            else
                if (u->op->getKind() == ExprNode::identifier) {
                    ret = handleIdentifier(static_cast<IdentifierExprNode *>(u->op), p->getKind(), xcrementOp, ret, NULL);
                }
                else
                    if (u->op->getKind() == ExprNode::index) {
                        BinaryExprNode *b = static_cast<BinaryExprNode *>(u->op);
                        TypedRegister base = genExpr(b->op1);
                        TypedRegister index = genExpr(b->op2);
                        ret = getElement(base, index);
                        ret = binaryOp(xcrementOp, ret, loadImmediate(1.0));
                        setElement(base, index, ret);
                    }
                    else
                        ASSERT("WAH!");
        }
        break;
    case ExprNode::postDecrement: 
        xcrementOp = SUBTRACT;
    case ExprNode::postIncrement: 
        {
            UnaryExprNode *u = static_cast<UnaryExprNode *>(p);
            if (u->op->getKind() == ExprNode::dot) {
                ret = handleDot(static_cast<BinaryExprNode *>(u->op), p->getKind(), xcrementOp, ret, NULL);
            }
            else
                if (u->op->getKind() == ExprNode::identifier) {
                    ret = handleIdentifier(static_cast<IdentifierExprNode *>(u->op), p->getKind(), xcrementOp, ret, NULL);
                }
                else
                    if (u->op->getKind() == ExprNode::index) {
                        BinaryExprNode *b = static_cast<BinaryExprNode *>(u->op);
                        TypedRegister base = genExpr(b->op1);
                        TypedRegister index = genExpr(b->op2);
                        ret = elementXcr(base, index, xcrementOp);
                    }
                    else
                        ASSERT("WAH!");
        }
        break;

    case ExprNode::plus:
    case ExprNode::minus:
    case ExprNode::complement:
        {
            UnaryExprNode *u = static_cast<UnaryExprNode *>(p);
            TypedRegister r = genExpr(u->op);
            ret = op(mapExprNodeToICodeOp(p->getKind()), r);
        }
        break;
    case ExprNode::add:
    case ExprNode::subtract:
    case ExprNode::multiply:
    case ExprNode::divide:
    case ExprNode::modulo:
    case ExprNode::leftShift:
    case ExprNode::rightShift:
    case ExprNode::logicalRightShift:
    case ExprNode::bitwiseAnd:
    case ExprNode::bitwiseXor:
    case ExprNode::bitwiseOr:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister r1 = genExpr(b->op1);
            TypedRegister r2 = genExpr(b->op2);
            ret = binaryOp(mapExprNodeToICodeOp(p->getKind()), r1, r2);
        }
        break;
    case ExprNode::assignment:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            ret = genExpr(b->op2);
            if (b->op1->getKind() == ExprNode::identifier) {
                ret = handleIdentifier(static_cast<IdentifierExprNode *>(b->op1), p->getKind(), xcrementOp, ret, NULL);
            }
            else
                if (b->op1->getKind() == ExprNode::dot) {
                    BinaryExprNode *lb = static_cast<BinaryExprNode *>(b->op1);
                    ret = handleDot(lb, p->getKind(), xcrementOp, ret, NULL);
                }
                else
                    if (b->op1->getKind() == ExprNode::index) {
                        BinaryExprNode *lb = static_cast<BinaryExprNode *>(b->op1);
                        TypedRegister base = genExpr(lb->op1);
                        TypedRegister index = genExpr(lb->op2);
                        setElement(base, index, ret);
                    }
                    else
                        ASSERT("WAH!");
        }
        break;
    case ExprNode::addEquals:
    case ExprNode::subtractEquals:
    case ExprNode::multiplyEquals:
    case ExprNode::divideEquals:
    case ExprNode::moduloEquals:
    case ExprNode::leftShiftEquals:
    case ExprNode::rightShiftEquals:
    case ExprNode::logicalRightShiftEquals:
    case ExprNode::bitwiseAndEquals:
    case ExprNode::bitwiseXorEquals:
    case ExprNode::bitwiseOrEquals:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            ret = genExpr(b->op2);
            if (b->op1->getKind() == ExprNode::identifier) {
                ret = handleIdentifier(static_cast<IdentifierExprNode *>(b->op1), p->getKind(), xcrementOp, ret, NULL);
            }
            else
                if (b->op1->getKind() == ExprNode::dot) {
                    BinaryExprNode *lb = static_cast<BinaryExprNode *>(b->op1);
                    ret = handleDot(lb, p->getKind(), xcrementOp, ret, NULL);
                }
                else
                    if (b->op1->getKind() == ExprNode::index) {
                        BinaryExprNode *lb = static_cast<BinaryExprNode *>(b->op1);
                        TypedRegister base = genExpr(lb->op1);
                        TypedRegister index = genExpr(lb->op2);
                        TypedRegister v = getElement(base, index);
                        ret = binaryOp(mapExprNodeToICodeOp(p->getKind()), v, ret);
                        setElement(base, index, ret);
                    }
                    else
                        ASSERT("WAH!");
        }
        break;
    case ExprNode::equal:
    case ExprNode::lessThan:
    case ExprNode::lessThanOrEqual:
    case ExprNode::identical:
    case ExprNode::In:
    case ExprNode::Instanceof:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister r1 = genExpr(b->op1);
            TypedRegister r2 = genExpr(b->op2);
            ret = binaryOp(mapExprNodeToICodeOp(p->getKind()), r1, r2);
            if (trueBranch || falseBranch) {
                if (trueBranch == NULL)
                    branchFalse(falseBranch, ret);
                else {
                    branchTrue(trueBranch, ret);
                    if (falseBranch)
                        branch(falseBranch);
                }
            }
        }
        break;
    case ExprNode::greaterThan:
    case ExprNode::greaterThanOrEqual:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister r1 = genExpr(b->op1);
            TypedRegister r2 = genExpr(b->op2);
            ret = binaryOp(mapExprNodeToICodeOp(p->getKind()), r2, r1);   // will return reverse case
            if (trueBranch || falseBranch) {
                if (trueBranch == NULL)
                    branchFalse(falseBranch, ret);
                else {
                    branchTrue(trueBranch, ret);
                    if (falseBranch)
                        branch(falseBranch);
                }
            }
        }
        break;

    case ExprNode::notEqual:
    case ExprNode::notIdentical:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister r1 = genExpr(b->op1);
            TypedRegister r2 = genExpr(b->op2);
            ret = binaryOp(mapExprNodeToICodeOp(p->getKind()), r1, r2);
            if (trueBranch || falseBranch) {
                if (trueBranch == NULL)
                    branchFalse(falseBranch, ret);
                else {
                    branchTrue(trueBranch, ret);
                    if (falseBranch)
                        branch(falseBranch);
                }
            }
            else
                ret = logicalNot(ret);

        }
        break;
    
    case ExprNode::logicalAnd:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            if (trueBranch || falseBranch) {
                genExpr(b->op1, needBoolValueInBranch, NULL, falseBranch);
                genExpr(b->op2, needBoolValueInBranch, trueBranch, falseBranch);
            }
            else {
                Label *fBranch = getLabel();
                TypedRegister r1 = genExpr(b->op1, true, NULL, fBranch);
                if (!generatedBoolean(b->op1)) {
                    r1 = test(r1);
                    branchFalse(fBranch, r1);
                }
                TypedRegister r2 = genExpr(b->op2);
                if (!generatedBoolean(b->op2)) {
                    r2 = test(r2);
                }
                if (r1 != r2)       // FIXME, need a way to specify a dest???
                    move(r1, r2);
                setLabel(fBranch);
                ret = r1;
            }
        }
        break;
    case ExprNode::logicalOr:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            if (trueBranch || falseBranch) {
                genExpr(b->op1, needBoolValueInBranch, trueBranch, NULL);
                genExpr(b->op2, needBoolValueInBranch, trueBranch, falseBranch);
            }
            else {
                Label *tBranch = getLabel();
                TypedRegister r1 = genExpr(b->op1, true, tBranch, NULL);
                if (!generatedBoolean(b->op1)) {
                    r1 = test(r1);
                    branchTrue(tBranch, r1);
                }
                TypedRegister r2 = genExpr(b->op2);
                if (!generatedBoolean(b->op2)) {
                    r2 = test(r2);
                }
                if (r1 != r2)       // FIXME, need a way to specify a dest???
                    move(r1, r2);
                setLabel(tBranch);
                ret = r1;
            }
        }
        break;

    case ExprNode::conditional:
        {
            TernaryExprNode *t = static_cast<TernaryExprNode *>(p);
            Label *fBranch = getLabel();
            Label *beyondBranch = getLabel();
            TypedRegister c = genExpr(t->op1, false, NULL, fBranch);
            if (!generatedBoolean(t->op1))
                branchFalse(fBranch, test(c));
            TypedRegister r1 = genExpr(t->op2);
            branch(beyondBranch);
            setLabel(fBranch);
            TypedRegister r2 = genExpr(t->op3);
            if (r1 != r2)       // FIXME, need a way to specify a dest???
                move(r1, r2);
            setLabel(beyondBranch);
            ret = r1;
        }
        break;

    case ExprNode::objectLiteral:
        {
            ret = newObject(TypedRegister(NotARegister, &Any_Type));
            PairListExprNode *plen = static_cast<PairListExprNode *>(p);
            ExprPairList *e = plen->pairs;
            while (e) {
                if (e->field && e->value && (e->field->getKind() == ExprNode::identifier))
                    setProperty(ret, (static_cast<IdentifierExprNode *>(e->field))->name, genExpr(e->value));
                e = e->next;
            }
        }
        break;

    case ExprNode::functionLiteral:
        {
            FunctionExprNode *f = static_cast<FunctionExprNode *>(p);
            ICodeGenerator icg(mWorld, mGlobal);
            icg.allocateParameter(mWorld->identifiers["this"]);   // always parameter #0
            VariableBinding *v = f->function.parameters;
            while (v) {
                if (v->name && (v->name->getKind() == ExprNode::identifier))
                    icg.allocateParameter((static_cast<IdentifierExprNode *>(v->name))->name);
                v = v->next;
            }
            icg.genStmt(f->function.body);
            //stdOut << icg;
            ICodeModule *icm = icg.complete();
            ret = newFunction(icm, &f->function);
        }
        break;

    case ExprNode::at:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            // for now, just handle simple identifiers on the rhs.
            ret = genExpr(b->op1);
            if (b->op2->getKind() == ExprNode::identifier) {
                TypedRegister t;
                /*uint32 slotIndex;*/
                const StringAtom &name = (static_cast<IdentifierExprNode *>(b->op2))->name;
                /*LValueKind lvk = resolveIdentifier(name, t, slotIndex);*/
                ASSERT(t.second == &Type_Type);
                const JSValue &v = mGlobal->getVariable(name);
                ASSERT(v.isType());
                JSClass *clazz = dynamic_cast<JSClass*>(v.type);
                if (clazz)
                    ret = cast(ret, clazz);
                else
                    ret = cast(ret, t.second);
            }
            else
                NOT_REACHED("Anything more complex than <expr>@<name> is not implemented");
        }
        break;

    
    default:
        {
            NOT_REACHED("Unsupported ExprNode kind");
        }   
    }
    return ret;
}

bool LabelEntry::containsLabel(const StringAtom *label)
{
    if (labelSet) {
        for (LabelSet::iterator i = labelSet->begin(); i != labelSet->end(); i++)
            if ( (*i) == label )
                return true;
    }
    return false;
}

static bool hasAttribute(const IdentifierList* identifiers, Token::Kind tokenKind)
{
    while (identifiers) {
        if (identifiers->name.tokenKind == tokenKind)
            return true;
        identifiers = identifiers->next;
    }
    return false;
}

JSType *ICodeGenerator::extractType(ExprNode *t)
{
    JSType* type = &Any_Type;
    // FIXME:  need to do code generation for type expressions.
    if (t && (t->getKind() == ExprNode::identifier)) {
        IdentifierExprNode* typeExpr = static_cast<IdentifierExprNode*>(t);
        type = findType(typeExpr->name);
    }
    return type;
}

ICodeModule *ICodeGenerator::genFunction(FunctionStmtNode *f, bool isConstructor, JSClass *superclass)
{
    bool isStatic = hasAttribute(f->attributes, Token::Static);
    ICodeGeneratorFlags flags = (isStatic) ? kIsStaticMethod : kNoFlags;
    
    ICodeGenerator icg(mWorld, mGlobal, mClass, flags);
    icg.allocateParameter(mWorld->identifiers["this"], (mClass) ? mClass : &Any_Type);   // always parameter #0
    VariableBinding *v = f->function.parameters;
    while (v) {
        if (v->name && (v->name->getKind() == ExprNode::identifier)) {
            JSType *pType;
            if ((v == f->function.restParameter) && (v->type == NULL))
                pType = &Array_Type;
            else
                pType = extractType(v->type);
            icg.allocateParameter((static_cast<IdentifierExprNode *>(v->name))->name, pType);
        }
        else
            NOT_REACHED("qualified or un-named parameters not handled; bugger off.");
        v = v->next;
    }
    v = f->function.optParameters;
    if (v) {
        while (v) {    // include the rest parameter, as it may have an initializer
            if (v->name && (v->name->getKind() == ExprNode::identifier)) {
                icg.addParameterLabel(icg.setLabel(icg.getLabel()));
                if (v->initializer) {           // might be NULL when we get to the restParameter
                    TypedRegister p = icg.genExpr(v->name);
                    icg.move(p, icg.genExpr(v->initializer));
                }
            }
            v = v->next;
        }
        icg.addParameterLabel(icg.setLabel(icg.getLabel()));    // to provide the entry-point for the default case
    }
    v = f->function.restParameter;
    if (v) {
        icg.mHasRestParameter = true;
        if (v->name && (v->name->getKind() == ExprNode::identifier)) {
            icg.mHasNamedRestParameter = true;
        }
    }
    if (isConstructor) {
        /*
           See if the first statement is an expression statement consisting
           of a call to super(). If not we need to add a call to the default 
           superclass constructor ourselves.
        */
        TypedRegister thisValue = TypedRegister(0, mClass);
        ArgumentList args;
        if (superclass) {
            bool foundSuperCall = false;
            BlockStmtNode *b = f->function.body;
            if (b && b->statements && (b->statements->getKind() == StmtNode::expression)) {
                ExprStmtNode *e = static_cast<ExprStmtNode *>(b->statements);
                if (e->expr->getKind() == ExprNode::call) {
                    InvokeExprNode *i = static_cast<InvokeExprNode *>(e->expr);
                    if (i->op->getKind() == ExprNode::dot) {
                        BinaryExprNode *b = static_cast<BinaryExprNode *>(i->op);
                        if ((b->op1->getKind() == ExprNode::This) && (b->op2->getKind() == ExprNode::qualify)) {
                            BinaryExprNode *q = static_cast<BinaryExprNode *>(b->op2);
                            if (q->op1->getKind() == ExprNode::Super) {
                                // XXX verify that q->op2 is either the superclass name or a constructor for it
                                foundSuperCall = true;
                            }
                        }
                    }
                }
            }
            if (!foundSuperCall) {         // invoke the default superclass constructor
                icg.call(icg.getStatic(superclass, superclass->getName()), thisValue, &args);
            }
        }
        const StringAtom &initName = mWorld->identifiers["__init__"];     // XXXXXXX
        icg.call(icg.getStatic(mClass, initName), thisValue, &args);   // ok, so it's mis-named
    }
    if (f->function.body)
        icg.genStmt(f->function.body);
    if (isConstructor) {
        TypedRegister thisValue = TypedRegister(0, mClass);
        icg.returnStmt(thisValue);
    }
    return icg.complete();

}

TypedRegister ICodeGenerator::genStmt(StmtNode *p, LabelSet *currentLabelSet)
{
    TypedRegister ret(NotARegister, &None_Type);

    startStatement(p->pos);
    if (mExceptionRegister.first == NotARegister) {
        mExceptionRegister = allocateRegister(mWorld->identifiers["__exceptionObject__"], &Any_Type);
    }

    switch (p->getKind()) {
    case StmtNode::Class:
        {
            // FIXME:  need a semantic check to make sure a class isn't being redefined(?)
            ClassStmtNode *classStmt = static_cast<ClassStmtNode *>(p);
            ASSERT(classStmt->name->getKind() == ExprNode::identifier);
            IdentifierExprNode* nameExpr = static_cast<IdentifierExprNode*>(classStmt->name);
            JSClass* superclass = 0;
            if (classStmt->superclass) {
                ASSERT(classStmt->superclass->getKind() == ExprNode::identifier);
                IdentifierExprNode* superclassExpr = static_cast<IdentifierExprNode*>(classStmt->superclass);
                const JSValue& superclassValue = mGlobal->getVariable(superclassExpr->name);
                ASSERT(superclassValue.isObject() && !superclassValue.isNull());
                superclass = static_cast<JSClass*>(superclassValue.object);
            }
            JSClass* thisClass = new JSClass(mGlobal, nameExpr->name, superclass);
            // is it ok for a partially defined class to appear in global scope? this is needed
            // to handle recursive types, such as linked list nodes.
            mGlobal->defineVariable(nameExpr->name, &Type_Type, JSValue(thisClass));

            // Have to have this declared ahead of time so that it's slot can
            // be discoverd when compiling constructors in the loop below. Could
            // do a pre-processing loop to discover whether it is in fact empty
            // and then pass that info through to the genFunction() call for each
            // constructor.
            const StringAtom &initName = mWorld->identifiers["__init__"];
            thisClass->defineStatic(initName, &Function_Type);
            
            bool hasDefaultConstructor = false;
            if (classStmt->body) {
                JSScope* thisScope = thisClass->getScope();
                ICodeGenerator ccg(mWorld, thisScope, thisClass, kNoFlags);   // constructor code generator.
                ccg.allocateParameter(mWorld->identifiers["this"], thisClass);   // always parameter #0
                ICodeGenerator scg(mWorld, thisScope, thisClass, kIsStaticMethod);   // static initializer code generator.
                StmtNode* s = classStmt->body->statements;
                while (s) {
                    switch (s->getKind()) {
                    case StmtNode::Const:
                    case StmtNode::Var:
                        {
                            // FIXME:  should preprocess all variable declarations, to prepare for method codegen.
                            VariableStmtNode *vs = static_cast<VariableStmtNode *>(s);
                            bool isStatic = hasAttribute(vs->attributes, Token::Static);
                            VariableBinding *v = vs->bindings;
                            TypedRegister thisRegister = TypedRegister(0, thisClass);
                            while (v)  {
                                if (v->name) {
                                    ASSERT(v->name->getKind() == ExprNode::identifier);
                                    IdentifierExprNode* idExpr = static_cast<IdentifierExprNode*>(v->name);
                                    JSType* type = extractType(v->type);
                                    if (isStatic) {
                                        thisClass->defineStatic(idExpr->name, type);
                                        if (v->initializer) {
                                            scg.setStatic(thisClass, idExpr->name, scg.genExpr(v->initializer));
                                            scg.resetStatement();
                                        }
                                     } else {
                                        const JSSlot& slot = thisClass->defineSlot(idExpr->name, type);
                                        if (v->initializer) {
                                            // generate code for the default constructor, which initializes the slots.
                                            ccg.setSlot(thisRegister, slot.mIndex, ccg.genExpr(v->initializer));
                                            ccg.resetStatement();
                                        }
                                    }
                                }
                                v = v->next;
                            }
                        }
                        break;
                    case StmtNode::Constructor:
                    case StmtNode::Function:
                        {
                            FunctionStmtNode *f = static_cast<FunctionStmtNode *>(s);
                            bool isStatic = hasAttribute(f->attributes, Token::Static);
                            bool isConstructor = (s->getKind() == StmtNode::Constructor);
                            ICodeGeneratorFlags flags = (isStatic) ? kIsStaticMethod : kNoFlags;

                            ICodeGenerator mcg(mWorld, thisScope, thisClass, flags);   // method code generator.
                            ICodeModule *icm = mcg.genFunction(f, isConstructor, superclass);
                            if (f->function.name->getKind() == ExprNode::identifier) {
                                const StringAtom& name = (static_cast<IdentifierExprNode *>(f->function.name))->name;
                                if (isConstructor) {
                                    if (name == nameExpr->name)
                                        hasDefaultConstructor = true;
                                    thisClass->defineConstructor(name);
                                    scg.setStatic(thisClass, name, scg.newFunction(icm, &f->function));
                                }
                                else
                                    if (isStatic) {
                                        thisClass->defineStatic(name, &Function_Type);
                                        scg.setStatic(thisClass, name, scg.newFunction(icm, &f->function));
                                    }
                                    else
                                        thisClass->defineMethod(name, new JSFunction(icm, &f->function));
                            }
                        }                        
                        break;
                    default:
                        NOT_REACHED("unimplemented class member statement");
                        break;
                    }
                    s = s->next;
                }

                // add the instance initializer
                scg.setStatic(thisClass, initName, scg.newFunction(ccg.complete(), NULL)); 
                // invent a default constructor if necessary, it just calls the 
                //   initializer and the superclass default constructor
                if (!hasDefaultConstructor) {
                    TypedRegister thisValue = TypedRegister(0, thisClass);
                    ArgumentList args;
                    ICodeGenerator icg(mWorld, thisScope, thisClass, kIsStaticMethod);
                    icg.allocateParameter(mWorld->identifiers["this"], thisClass);   // always parameter #0
                    if (superclass)
                        icg.call(icg.getStatic(superclass, superclass->getName()), thisValue, &args);
                    icg.call(icg.getStatic(thisClass, initName), thisValue, &args);
                    icg.returnStmt(thisValue);
                    thisClass->defineConstructor(nameExpr->name);
                    scg.setStatic(thisClass, nameExpr->name, scg.newFunction(icg.complete(), NULL));
                }
                // freeze the class.
                thisClass->complete();


                // REVISIT:  using the scope of the class to store both methods and statics.
                if (scg.getICode()->size()) {
                    Interpreter::Context cx(*mWorld, thisScope);
                    ICodeModule* clinit = scg.complete();
                    cx.interpret(clinit, JSValues());
                    delete clinit;
                }
            }
        }
        break;
    case StmtNode::Function:
        {
            FunctionStmtNode *f = static_cast<FunctionStmtNode *>(p);
            ICodeModule *icm = genFunction(f, false, NULL);
            if (f->function.name->getKind() == ExprNode::identifier) {
                const StringAtom& name = (static_cast<IdentifierExprNode *>(f->function.name))->name;
                mGlobal->defineFunction(name, icm, &f->function);
            }
        }
        break;
    case StmtNode::Import:
        {
            ImportStmtNode *i = static_cast<ImportStmtNode *>(p);
            String *fileName = i->bindings->packageName.str;
            if (fileName) { /// if not, build one from the idList instead
                std::string str(fileName->length(), char());
                std::transform(fileName->begin(), fileName->end(), str.begin(), narrow);
                FILE* f = fopen(str.c_str(), "r");
                if (f) {
                    Context cx(*mWorld, mGlobal);
                    JSValue result = cx.readEvalFile(f, *fileName);
                    fclose(f);
                }
            }
        }
        break;
    case StmtNode::Var:
        {
            VariableStmtNode *vs = static_cast<VariableStmtNode *>(p);
            VariableBinding *v = vs->bindings;
            while (v)  {
                if (v->name && (v->name->getKind() == ExprNode::identifier)) {
                    if (v->type && (v->type->getKind() == ExprNode::identifier)) {
                        if (isTopLevel())
                            mGlobal->defineVariable((static_cast<IdentifierExprNode *>(v->name))->name,
                                            findType((static_cast<IdentifierExprNode *>(v->type))->name));
                        else
                            allocateVariable((static_cast<IdentifierExprNode *>(v->name))->name,
                                                (static_cast<IdentifierExprNode *>(v->type))->name);
                    }
                    else 
                        allocateVariable((static_cast<IdentifierExprNode *>(v->name))->name);
                    if (v->initializer) {
                        if (!isTopLevel() && !isWithinWith()) {
                            TypedRegister r = genExpr(v->name);
                            TypedRegister val = genExpr(v->initializer);
                            move(r, val);
                        }
                        else {
                            TypedRegister val = genExpr(v->initializer);
                            saveName((static_cast<IdentifierExprNode *>(v->name))->name, val);
                        }
                    }
                }
                v = v->next;
            }
        }
        break;
    case StmtNode::expression:
        {
            ExprStmtNode *e = static_cast<ExprStmtNode *>(p);
            ret = genExpr(e->expr);
        }
        break;
    case StmtNode::Throw:
        {
            ExprStmtNode *e = static_cast<ExprStmtNode *>(p);
            throwStmt(genExpr(e->expr));
        }
        break;
    case StmtNode::Debugger:
        {
            debuggerStmt();
        }
        break;
    case StmtNode::Return:
        {
            ExprStmtNode *e = static_cast<ExprStmtNode *>(p);
            if (e->expr)
                returnStmt(ret = genExpr(e->expr));
            else
                returnStmt(TypedRegister(NotARegister, &Void_Type));
        }
        break;
    case StmtNode::If:
        {
            Label *falseLabel = getLabel();
            UnaryStmtNode *i = static_cast<UnaryStmtNode *>(p);
            TypedRegister c = genExpr(i->expr, false, NULL, falseLabel);
            if (!generatedBoolean(i->expr))
                branchFalse(falseLabel, test(c));
            genStmt(i->stmt);
            setLabel(falseLabel);
        }
        break;
    case StmtNode::IfElse:
        {
            Label *falseLabel = getLabel();
            Label *beyondLabel = getLabel();
            BinaryStmtNode *i = static_cast<BinaryStmtNode *>(p);
            TypedRegister c = genExpr(i->expr, false, NULL, falseLabel);
            if (!generatedBoolean(i->expr))
                branchFalse(falseLabel, test(c));
            genStmt(i->stmt);
            branch(beyondLabel);
            setLabel(falseLabel);
            genStmt(i->stmt2);
            setLabel(beyondLabel);
        }
        break;
    case StmtNode::With:
        {
            UnaryStmtNode *w = static_cast<UnaryStmtNode *>(p);
            TypedRegister o = genExpr(w->expr);
            bool withinWith = isWithinWith();
            setFlag(kIsWithinWith, true);
            beginWith(o);
            genStmt(w->stmt);
            endWith();
            setFlag(kIsWithinWith, withinWith);
        }
        break;
    case StmtNode::Switch:
        {
            Label *defaultLabel = NULL;
            LabelEntry *e = new LabelEntry(currentLabelSet, getLabel());
            mLabelStack.push_back(e);
            SwitchStmtNode *sw = static_cast<SwitchStmtNode *>(p);
            TypedRegister sc = genExpr(sw->expr);
            StmtNode *s = sw->statements;
            // ECMA requires case & default statements to be immediate children of switch
            // unlike C where they can be arbitrarily deeply nested in other statements.    
            Label *nextCaseLabel = NULL;
            GenericBranch *lastBranch = NULL;
            while (s) {
                if (s->getKind() == StmtNode::Case) {
                    ExprStmtNode *c = static_cast<ExprStmtNode *>(s);
                    if (c->expr) {
                        if (nextCaseLabel)
                            setLabel(nextCaseLabel);
                        nextCaseLabel = getLabel();
                        TypedRegister r = genExpr(c->expr);
                        TypedRegister eq = binaryOp(COMPARE_EQ, r, sc);
                        lastBranch = branchFalse(nextCaseLabel, eq);
                    }
                    else {
                        defaultLabel = getLabel();
                        setLabel(defaultLabel);
                    }
                }
                else
                    genStmt(s);
                s = s->next;
            }
            if (nextCaseLabel)
                setLabel(nextCaseLabel);
            if (defaultLabel && lastBranch)
                lastBranch->setTarget(defaultLabel);

            setLabel(e->breakLabel);
            mLabelStack.pop_back();
        }
        break;
    case StmtNode::DoWhile:
        {
            LabelEntry *e = new LabelEntry(currentLabelSet, getLabel(), getLabel());
            mLabelStack.push_back(e);
            UnaryStmtNode *d = static_cast<UnaryStmtNode *>(p);
            Label *doBodyTopLabel = getLabel();
            setLabel(doBodyTopLabel);
            genStmt(d->stmt);
            setLabel(e->continueLabel);
            TypedRegister c = genExpr(d->expr, false, doBodyTopLabel, NULL);
            if (!generatedBoolean(d->expr))
                branchTrue(doBodyTopLabel, test(c));
            setLabel(e->breakLabel);
            mLabelStack.pop_back();
        }
        break;
    case StmtNode::While:
        {
            LabelEntry *e = new LabelEntry(currentLabelSet, getLabel(), getLabel());
            mLabelStack.push_back(e);
            branch(e->continueLabel);
            
            UnaryStmtNode *w = static_cast<UnaryStmtNode *>(p);

            Label *whileBodyTopLabel = getLabel();
            setLabel(whileBodyTopLabel);
            genStmt(w->stmt);

            setLabel(e->continueLabel);
            TypedRegister c = genExpr(w->expr, false, whileBodyTopLabel, NULL);
            if (!generatedBoolean(w->expr))
                branchTrue(whileBodyTopLabel, test(c));

            setLabel(e->breakLabel);
            mLabelStack.pop_back();
        }
        break;
    case StmtNode::For:
        {
            LabelEntry *e = new LabelEntry(currentLabelSet, getLabel(), getLabel());
            mLabelStack.push_back(e);

            ForStmtNode *f = static_cast<ForStmtNode *>(p);
            if (f->initializer) 
                genStmt(f->initializer);
            Label *forTestLabel = getLabel();
            branch(forTestLabel);

            Label *forBlockTop = getLabel();
            setLabel(forBlockTop);
            genStmt(f->stmt);
            
            setLabel(e->continueLabel);
            if (f->expr3) {
                (*mInstructionMap)[iCode->size()] = f->expr3->pos;
                genExpr(f->expr3);
            }
            
            setLabel(forTestLabel);
            if (f->expr2) {
                (*mInstructionMap)[iCode->size()] = f->expr2->pos;
                TypedRegister c = genExpr(f->expr2, false, forBlockTop, NULL);
                if (!generatedBoolean(f->expr2))
                    branchTrue(forBlockTop, test(c));
            }
            
            setLabel(e->breakLabel);

            mLabelStack.pop_back();
        }
        break;
    case StmtNode::block:
        {
            BlockStmtNode *b = static_cast<BlockStmtNode *>(p);
            StmtNode *s = b->statements;
            while (s) {
                genStmt(s);
                s = s->next;
            }            
        }
        break;

    case StmtNode::label:
        {
            LabelStmtNode *l = static_cast<LabelStmtNode *>(p);
            // ok, there's got to be a cleverer way of doing this...
            if (currentLabelSet == NULL) {
                currentLabelSet = new LabelSet();
                currentLabelSet->push_back(&l->name);
                genStmt(l->stmt, currentLabelSet);
                delete currentLabelSet;
            }
            else {
                currentLabelSet->push_back(&l->name);
                genStmt(l->stmt, currentLabelSet);
                currentLabelSet->pop_back();
            }
        }
        break;

    case StmtNode::Break:
        {
            GoStmtNode *g = static_cast<GoStmtNode *>(p);
            if (g->label) {
                LabelEntry *e = NULL;
                for (LabelStack::reverse_iterator i = mLabelStack.rbegin(); i != mLabelStack.rend(); i++) {
                    e = (*i);
                    if (e->containsLabel(g->name))
                        break;
                }
                if (e) {
                    ASSERT(e->breakLabel);
                    branch(e->breakLabel);
                }
                else
                    NOT_REACHED("break label not in label set");
            }
            else {
                ASSERT(!mLabelStack.empty());
                LabelEntry *e = mLabelStack.back();
                ASSERT(e->breakLabel);
                branch(e->breakLabel);
            }
        }
        break;
    case StmtNode::Continue:
        {
            GoStmtNode *g = static_cast<GoStmtNode *>(p);
            if (g->label) {
                LabelEntry *e = NULL;
                for (LabelStack::reverse_iterator i = mLabelStack.rbegin(); i != mLabelStack.rend(); i++) {
                    e = (*i);
                    if (e->containsLabel(g->name))
                        break;
                }
                if (e) {
                    ASSERT(e->continueLabel);
                    branch(e->continueLabel);
                }
                else
                    NOT_REACHED("continue label not in label set");
            }
            else {
                ASSERT(!mLabelStack.empty());
                LabelEntry *e = mLabelStack.back();
                ASSERT(e->continueLabel);
                branch(e->continueLabel);
            }
        }
        break;

    case StmtNode::Try:
        {
            /*
                The finallyInvoker is a little stub used by the interpreter to
                invoke the finally handler on the (exceptional) way out of the
                try block assuming there are no catch clauses.
            */
            /*Register ex = NotARegister;*/
            TryStmtNode *t = static_cast<TryStmtNode *>(p);
            Label *catchLabel = (t->catches) ? getLabel() : NULL;
            Label *finallyInvoker = (t->finally) ? getLabel() : NULL;
            Label *finallyLabel = (t->finally) ? getLabel() : NULL;
            Label *beyondLabel = getLabel();
            beginTry(catchLabel, finallyLabel);
            genStmt(t->stmt);
            endTry();
            if (finallyLabel)
                jsr(finallyLabel);
            branch(beyondLabel);
            if (catchLabel) {
                setLabel(catchLabel);
                CatchClause *c = t->catches;
                while (c) {
                    // Bind the incoming exception ...
                    setRegisterForVariable(c->name, mExceptionRegister);

                    genStmt(c->stmt);
                    if (finallyLabel)
                        jsr(finallyLabel);
                    throwStmt(mExceptionRegister);
                    c = c->next;
                }
            }
            if (finallyLabel) {
                setLabel(finallyInvoker);
                jsr(finallyLabel);
                throwStmt(mExceptionRegister);

                setLabel(finallyLabel);
                genStmt(t->finally);
                rts();
            }
            setLabel(beyondLabel);
        }
        break;

    case StmtNode::empty:
        /* nada */
        break;
        
    default:
        NOT_REACHED("unimplemented statement kind");
    }
    resetStatement();
    return ret;
}


/************************************************************************/


Formatter& ICodeGenerator::print(Formatter& f)
{
    f << "ICG! " << (uint32)iCode->size() << "\n";
    VM::operator<<(f, *iCode);
    f << "  Src  :  Instr" << "\n";
    for (InstructionMap::iterator i = mInstructionMap->begin(); i != mInstructionMap->end(); i++)
    {
        printDec( f, (*i).first, 6);
        f << " : ";
        printDec( f, (*i).second, 6);
        f << "\n";
//        f << (*i)->first << " : " << (*i)->second << "\n";
    }
    return f;
}

Formatter& ICodeModule::print(Formatter& f)
{
    f << "ICM[" << mID << "] from source at '" << mFileName << "' " <<
        (uint32)its_iCode->size() << " bytecodes\n";
    return VM::operator<<(f, *its_iCode);
}

    
} // namespace ICG

} // namespace JavaScript
