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

namespace JavaScript {
namespace ICG {

using namespace VM;
using namespace JSTypes;
using namespace JSClasses;

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

ICodeGenerator::ICodeGenerator(World *world, JSScope *global, JSClass *aClass, bool atTopLevel)
    :   topRegister(0), 
        registerBase(0), 
        maxRegister(0), 
        parameterCount(0),
        exceptionRegister(TypedRegister(NotARegister, &None_Type)),
        variableList(new VariableList()),
        mWorld(world),
        mGlobal(global),
        mInstructionMap(new InstructionMap()),
        mWithinWith(false),
        mAtTopLevel(atTopLevel),
        mClass(aClass)

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

TypedRegister ICodeGenerator::allocateVariable(const StringAtom& name, const StringAtom& typeName) 
{ 
    if (exceptionRegister.first == NotARegister) {
        exceptionRegister = grabRegister(mWorld->identifiers[widenCString("__exceptionObject__")], &Any_Type);
    }
    return grabRegister(name, findType(typeName)); 
}

TypedRegister ICodeGenerator::allocateVariable(const StringAtom& name) 
{ 
    if (exceptionRegister.first == NotARegister) {
        exceptionRegister = grabRegister(mWorld->identifiers[widenCString("__exceptionObject__")], &Any_Type);
    }
    return grabRegister(name, &Any_Type); 
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
    markMaxRegister();
    ICodeModule* module = new ICodeModule(iCode, variableList, maxRegister, 0, mInstructionMap);
    iCodeOwner = false;   // give ownership to the module.
    return module;
}


/********************************************************************/

TypedRegister ICodeGenerator::loadImmediate(double value)
{
    TypedRegister dest(getRegister(), &Number_Type);
    LoadImmediate *instr = new LoadImmediate(dest, value);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::loadString(String &value)
{
    TypedRegister dest(getRegister(), &String_Type);
    LoadString *instr = new LoadString(dest, new JSString(value));
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::loadString(const StringAtom &value)
{
    TypedRegister dest(getRegister(), &String_Type);
    LoadString *instr = new LoadString(dest, new JSString(value));
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::loadBoolean(bool value)
{
    TypedRegister dest(getRegister(), &Boolean_Type);
    LoadBoolean *instr = new LoadBoolean(dest, value);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::newObject()
{
    TypedRegister dest(getRegister(), &Any_Type);
    NewObject *instr = new NewObject(dest);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::newClass(const StringAtom &name)
{
    TypedRegister dest(getRegister(), &Any_Type);
    NewClass *instr = new NewClass(dest, &name);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::newFunction(ICodeModule *icm)
{
    TypedRegister dest(getRegister(), &Function_Type);
    NewFunction *instr = new NewFunction(dest, icm);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::newArray()
{
    TypedRegister dest(getRegister(), &Array_Type);
    NewArray *instr = new NewArray(dest);
    iCode->push_back(instr);
    return dest;
}



TypedRegister ICodeGenerator::loadName(const StringAtom &name)
{
    TypedRegister dest(getRegister(), &Any_Type);
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
    TypedRegister dest(getRegister(), &Number_Type);
    NameXcr *instr = new NameXcr(dest, &name, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}



TypedRegister ICodeGenerator::varXcr(TypedRegister var, ICodeOp op)
{
    TypedRegister dest(getRegister(), &Number_Type);
    VarXcr *instr = new VarXcr(dest, var, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}




TypedRegister ICodeGenerator::getProperty(TypedRegister base, const StringAtom &name)
{
    TypedRegister dest(getRegister(), &Any_Type);
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
    TypedRegister dest(getRegister(), &Any_Type);
    PropXcr *instr = new PropXcr(dest, base, &name, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}





TypedRegister ICodeGenerator::getStatic(JSClass *base, const StringAtom &name)
{
    TypedRegister dest(getRegister(), &Any_Type);
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

TypedRegister ICodeGenerator::staticXcr(JSClass *base, const StringAtom &name, ICodeOp op)
{
    TypedRegister dest(getRegister(), &Any_Type);
    const JSSlot& slot = base->getStatic(name);
    StaticXcr *instr = new StaticXcr(dest, base, slot.mIndex, 1.0);
    iCode->push_back(instr);
    return dest;
}




TypedRegister ICodeGenerator::getSlot(TypedRegister base, uint32 slot)
{
    TypedRegister dest(getRegister(), &Any_Type);
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
    TypedRegister dest(getRegister(), &Any_Type);
    SlotXcr *instr = new SlotXcr(dest, base, slot, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}




TypedRegister ICodeGenerator::getElement(TypedRegister base, TypedRegister index)
{
    TypedRegister dest(getRegister(), &Any_Type);
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
    TypedRegister dest(getRegister(), &Number_Type);
    ElemXcr *instr = new ElemXcr(dest, base, index, (op == ADD) ? 1.0 : -1.0);
    iCode->push_back(instr);
    return dest;
}



TypedRegister ICodeGenerator::op(ICodeOp op, TypedRegister source)
{
    TypedRegister dest(getRegister(), &Any_Type);
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
    TypedRegister dest(getRegister(), &Any_Type);
    Not *instr = new Not(dest, source);
    iCode->push_back(instr);
    return dest;
} 

TypedRegister ICodeGenerator::test(TypedRegister source)
{
    TypedRegister dest(getRegister(), &Any_Type);
    Test *instr = new Test(dest, source);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::op(ICodeOp op, TypedRegister source1, 
                            TypedRegister source2)
{
    ASSERT(source1.first != NotARegister);    
    ASSERT(source2.first != NotARegister);    
    TypedRegister dest(getRegister(), &Any_Type);
    Arithmetic *instr = new Arithmetic(op, dest, source1, source2);
    iCode->push_back(instr);
    return dest;
} 
    
TypedRegister ICodeGenerator::call(TypedRegister target, const StringAtom &name, RegisterList args)
{
    TypedRegister dest(getRegister(), &Any_Type);
    Call *instr = new Call(dest, target, &name, args);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::methodCall(TypedRegister targetBase, TypedRegister targetValue, RegisterList args)
{
    TypedRegister dest(getRegister(), &Any_Type);
    MethodCall *instr = new MethodCall(dest, targetBase, targetValue, args);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::staticCall(JSClass *c, const StringAtom &name, RegisterList args)
{
    TypedRegister dest(getRegister(), &Any_Type);
    StaticCall *instr = new StaticCall(dest, c, &name, args);
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

void ICodeGenerator::setLabel(Label *l)
{
    l->mBase = iCode;
    l->mOffset = iCode->size();
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
        NOT_REACHED("I shouldn't be here."); /* quiet linux warnings */
    }
    return false;
}

static bool isSlotName(JSType *t, const StringAtom &name, uint32 &slotIndex)
{
    JSClass* c = dynamic_cast<JSClass*>(t);
    while (c) {
        if (c->hasSlot(name)) {
            const JSSlot &s = c->getSlot(name);
            slotIndex = s.mIndex;
            return true;
        }
        c = c->getSuperClass();
    }
    return false;
}

TypedRegister ICodeGenerator::handleDot(BinaryExprNode *b, ExprNode::Kind use, ICodeOp xcrementOp, TypedRegister ret)
{
    enum {Property, Slot, Static} lValueKind;
    
    ASSERT(b->getKind() == ExprNode::dot);

    if (b->op2->getKind() != ExprNode::identifier) {
        NOT_REACHED("Implement me");    // turns into a getProperty (but not via any overloaded [] )
    }
    else {
        const StringAtom &name = static_cast<IdentifierExprNode *>(b->op2)->name;
        TypedRegister base;
        JSClass* c = NULL;
        uint32 slotIndex;
        if ((b->op1->getKind() == ExprNode::identifier) && !mWithinWith) {
            const StringAtom &baseName = (static_cast<IdentifierExprNode *>(b->op1))->name;
            JSType *baseType = &Any_Type;
            base = findVariable(baseName);

            if (base.first != NotARegister)
                baseType = base.second;
            else {
                // implicit 'this' has a big impact here
                baseType = mGlobal->getType(baseName);
            }
            lValueKind = Property;
            if (baseType == &Type_Type) {
                const JSValue &v = mGlobal->getVariable(baseName);
                c = dynamic_cast<JSClass*>(v.type);
                if (c && c->hasStatic(name))
                    lValueKind = Static;
            }
            if (base.first == NotARegister)
                base = loadName(baseName);
        }
        else {
            base = genExpr(b->op1);
            if (isSlotName(base.second, name, slotIndex))
                lValueKind = Slot;
            else {
                c = dynamic_cast<JSClass*>(base.second);
                if (c && c->hasStatic(name))
                    lValueKind = Static;
                else
                    lValueKind = Property;
            }
        }
        TypedRegister v;
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
            case Static:
                v = getStatic(c, name);
                break;
            case Property:
                v = getProperty(base, name);
                break;
            case Slot:
                v = getSlot(base, slotIndex);
                break;
            }
            ret = op(mapExprNodeToICodeOp(use), v, ret);
            // fall thru...
        case ExprNode::assignment:
            switch (lValueKind) {
            case Static:
                setStatic(c, name, ret);
                break;
            case Property:
                setProperty(base, name, ret);
                break;
            case Slot:
                setSlot(base, slotIndex, ret);
                break;
            }
            break;
        case ExprNode::postDecrement: 
        case ExprNode::postIncrement: 
            switch (lValueKind) {
            case Static:
                ret = staticXcr(c, name, xcrementOp);
                break;
            case Property:
                ret = propertyXcr(base, name, xcrementOp);
                break;
            case Slot:
                ret = slotXcr(base, slotIndex, xcrementOp);
            }
            break;
        case ExprNode::preDecrement: 
        case ExprNode::preIncrement:
            switch (lValueKind) {
            case Static:
                ret = op(xcrementOp, getStatic(c, name), loadImmediate(1.0));
                setStatic(c, name, ret);
                break;
            case Property:
                ret = op(xcrementOp, getProperty(base, name), loadImmediate(1.0));
                setProperty(base, name, ret);
                break;
            case Slot:
                ret = op(xcrementOp, getSlot(base, slotIndex), loadImmediate(1.0));
                setSlot(base, slotIndex, ret);
                break;
            }
            break;
        }
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
            if (i->op->getKind() == ExprNode::identifier) {
                ret = newClass(static_cast<IdentifierExprNode *>(i->op)->name);
            }                
            else
                ret = newObject();  // XXX more
        }
        break;
    case ExprNode::call : 
        {
            InvokeExprNode *i = static_cast<InvokeExprNode *>(p);
            RegisterList args;
            ExprPairList *p = i->pairs;
            while (p) {
                args.push_back(genExpr(p->value));
                p = p->next;
            }

            if (i->op->getKind() == ExprNode::dot) {
                BinaryExprNode *b = static_cast<BinaryExprNode *>(i->op);
                TypedRegister base = genExpr(b->op1);
                // might be Class.<method>()
                const StringAtom &name = static_cast<IdentifierExprNode *>(b->op2)->name;
                if ((base.second == &Type_Type)
                        && (b->op1->getKind() == ExprNode::identifier)) {
                    const StringAtom &className = (static_cast<IdentifierExprNode *>(b->op1))->name;
                    const JSValue &v = mGlobal->getVariable(className);
                    JSClass* c = dynamic_cast<JSClass*>(v.type);
                    if (c && c->getScope()->hasProperty(name)) {
                        ret = staticCall(c, name, args);
                        break;
                    }
                }
                ret = methodCall(base, loadString(name), args);
            }
            else 
                if (i->op->getKind() == ExprNode::identifier) {
                    if (!mWithinWith) {
                        TypedRegister v = findVariable((static_cast<IdentifierExprNode *>(i->op))->name);
                        if (v.first != NotARegister)
                            ret = call(v, static_cast<IdentifierExprNode *>(i->op)->name, args);
                        else
                            ret = call(TypedRegister(NotARegister, &Null_Type), static_cast<IdentifierExprNode *>(i->op)->name, args);
                    }
                    else
                        ret = methodCall(TypedRegister(NotARegister, &Null_Type), loadString(static_cast<IdentifierExprNode *>(i->op)->name), args);
                }
                else 
                    if (i->op->getKind() == ExprNode::index) {
                        BinaryExprNode *b = static_cast<BinaryExprNode *>(i->op);
                        ret = methodCall(genExpr(b->op1), genExpr(b->op2), args);
                    }
                    else
                        ASSERT("WAH!");
        }
        break;
    case ExprNode::index :
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister base = genExpr(b->op1);     
            TypedRegister index = genExpr(b->op2);
            ret = getElement(base, index);
        }
        break;
    case ExprNode::dot :
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            if (b->op2->getKind() != ExprNode::identifier) {
                NOT_REACHED("Implement me");    // turns into a getProperty (but not via any overloaded [] )
            }
            else {
                const StringAtom &name = static_cast<IdentifierExprNode *>(b->op2)->name;
                TypedRegister base = genExpr(b->op1);
                uint32 slotIndex;
                if (isSlotName(base.second, name, slotIndex))
                    ret = getSlot(base, slotIndex);
                else {
                    //
                    // look for <class name>.m
                    //
                    // XXX redundant work done by genExpr(b->op1), seems a pity
                    if ((base.second == &Type_Type)
                            && (b->op1->getKind() == ExprNode::identifier)) {
                        const StringAtom &className = (static_cast<IdentifierExprNode *>(b->op1))->name;
                        const JSValue &v = mGlobal->getVariable(className);
                        JSClass* c = dynamic_cast<JSClass*>(v.type);
                        if (c && c->hasStatic(name)) {
                            ret = getStatic(c, name);
                            break;
                        }
                    }
                }
                ret = getProperty(base, name);
            }
        }
        break;
    case ExprNode::This :
        {
            ret = TypedRegister(0, mClass ? mClass : &Any_Type);
        }
        break;
    case ExprNode::identifier :
        {
            const StringAtom &name = (static_cast<IdentifierExprNode *>(p))->name;
            if (!mWithinWith) {
                TypedRegister v = findVariable(name);
                if (v.first != NotARegister)
                    ret = v;
                else {
                    // When we support implicit this AND we're in a method (instance or static), look
                    // to see if the class has a slot(for an instance field) or property(static field) 
                    // by this name.
                    ret = loadName(name);
                    ret.second = mGlobal->getType(name);
                }
            }
            else
                ret = loadName(name);
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
                BinaryExprNode *b = static_cast<BinaryExprNode *>(u->op);
                ret = handleDot(b, p->getKind(), xcrementOp, ret);
            }
            else
                if (u->op->getKind() == ExprNode::identifier) {
                    if (!mWithinWith) {
                        TypedRegister v = findVariable((static_cast<IdentifierExprNode *>(u->op))->name);
                        if (v.first != NotARegister)
                            ret = op(xcrementOp, ret, loadImmediate(1.0));
                        else {
                            ret = loadName((static_cast<IdentifierExprNode *>(u->op))->name);
                            ret = op(xcrementOp, ret, loadImmediate(1.0));
                            saveName((static_cast<IdentifierExprNode *>(u->op))->name, ret);
                        }
                    }
                    else {
                        ret = loadName((static_cast<IdentifierExprNode *>(u->op))->name);
                        ret = op(xcrementOp, ret, loadImmediate(1.0));
                        saveName((static_cast<IdentifierExprNode *>(u->op))->name, ret);
                    }
                }
                else
                    if (u->op->getKind() == ExprNode::index) {
                        BinaryExprNode *b = static_cast<BinaryExprNode *>(u->op);
                        TypedRegister base = genExpr(b->op1);
                        TypedRegister index = genExpr(b->op2);
                        ret = getElement(base, index);
                        ret = op(xcrementOp, ret, loadImmediate(1.0));
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
                BinaryExprNode *b = static_cast<BinaryExprNode *>(u->op);
                ret = handleDot(b, p->getKind(), xcrementOp, ret);
            }
            else
                if (u->op->getKind() == ExprNode::identifier) {
                    if (!mWithinWith) {
                        TypedRegister v = findVariable((static_cast<IdentifierExprNode *>(u->op))->name);
                        if (v.first != NotARegister)
                            ret = varXcr(v, xcrementOp);
                        else
                            ret = nameXcr((static_cast<IdentifierExprNode *>(u->op))->name, xcrementOp);
                    }
                    else
                        ret = nameXcr((static_cast<IdentifierExprNode *>(u->op))->name, xcrementOp);
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
            ret = op(mapExprNodeToICodeOp(p->getKind()), r1, r2);
        }
        break;
    case ExprNode::assignment:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            ret = genExpr(b->op2);
            if (b->op1->getKind() == ExprNode::identifier) {
                if (!mWithinWith) {
                    TypedRegister v = findVariable((static_cast<IdentifierExprNode *>(b->op1))->name);
                    if (v.first != NotARegister)
                        move(v, ret);
                    else
                        saveName((static_cast<IdentifierExprNode *>(b->op1))->name, ret);
                }
                else
                    saveName((static_cast<IdentifierExprNode *>(b->op1))->name, ret);
            }
            else
                if (b->op1->getKind() == ExprNode::dot) {
                    BinaryExprNode *lb = static_cast<BinaryExprNode *>(b->op1);
                    ret = handleDot(lb, p->getKind(), xcrementOp, ret);
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
                if (!mWithinWith) {
                    TypedRegister v = findVariable((static_cast<IdentifierExprNode *>(b->op1))->name);
                    if (v.first != NotARegister) {
                        ret = op(mapExprNodeToICodeOp(p->getKind()), v, ret);
                        move(v, ret);
                    }
                    else {
                        v = loadName((static_cast<IdentifierExprNode *>(b->op1))->name);
                        ret = op(mapExprNodeToICodeOp(p->getKind()), v, ret);
                        saveName((static_cast<IdentifierExprNode *>(b->op1))->name, ret);
                    }
                }
                else {
                    TypedRegister v = loadName((static_cast<IdentifierExprNode *>(b->op1))->name);
                    ret = op(mapExprNodeToICodeOp(p->getKind()), v, ret);
                    saveName((static_cast<IdentifierExprNode *>(b->op1))->name, ret);
                }
            }
            else
                if (b->op1->getKind() == ExprNode::dot) {
                    BinaryExprNode *lb = static_cast<BinaryExprNode *>(b->op1);
                    ret = handleDot(lb, p->getKind(), xcrementOp, ret);
                }
                else
                    if (b->op1->getKind() == ExprNode::index) {
                        BinaryExprNode *lb = static_cast<BinaryExprNode *>(b->op1);
                        TypedRegister base = genExpr(lb->op1);
                        TypedRegister index = genExpr(lb->op2);
                        TypedRegister v = getElement(base, index);
                        ret = op(mapExprNodeToICodeOp(p->getKind()), v, ret);
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
            ret = op(mapExprNodeToICodeOp(p->getKind()), r1, r2);
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
            ret = op(mapExprNodeToICodeOp(p->getKind()), r2, r1);   // will return reverse case
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
            ret = op(mapExprNodeToICodeOp(p->getKind()), r1, r2);
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
            ret = newObject();
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
            icg.allocateParameter(mWorld->identifiers[widenCString("this")]);   // always parameter #0
            VariableBinding *v = f->function.parameters;
            while (v) {
                if (v->name && (v->name->getKind() == ExprNode::identifier))
                    icg.allocateParameter((static_cast<IdentifierExprNode *>(v->name))->name);
                v = v->next;
            }
            icg.preprocess(f->function.body);
            icg.genStmt(f->function.body);
            //stdOut << icg;
            ICodeModule *icm = icg.complete();
            ret = newFunction(icm);
        }
        break;

    default:
        {
            NOT_REACHED("Unsupported ExprNode kind");
        }   
    }
    return ret;
}

/*
 need pre-pass to find: 
    variable & function definitions,
    contains 'with' or 'eval'
    contains 'try {} catch {} finally {}'
*/


bool LabelEntry::containsLabel(const StringAtom *label)
{
    if (labelSet) {
        for (LabelSet::iterator i = labelSet->begin(); i != labelSet->end(); i++)
            if ( (*i) == label )
                return true;
    }
    return false;
}

void ICodeGenerator::preprocess(StmtNode *p)
{
    switch (p->getKind()) {
    case StmtNode::block:
        {
            BlockStmtNode *b = static_cast<BlockStmtNode *>(p);
            StmtNode *s = b->statements;
            while (s) {
                preprocess(s);
                s = s->next;
            }            
        }
        break;
    case StmtNode::Var:
        {
            VariableStmtNode *vs = static_cast<VariableStmtNode *>(p);
            VariableBinding *v = vs->bindings;
            while (v)  {
                if (v->name && (v->name->getKind() == ExprNode::identifier))
                    if (v->type && (v->type->getKind() == ExprNode::identifier))
                        if (mAtTopLevel)
                            mGlobal->defineVariable((static_cast<IdentifierExprNode *>(v->name))->name,
                                            findType((static_cast<IdentifierExprNode *>(v->type))->name));
                        else
                            allocateVariable((static_cast<IdentifierExprNode *>(v->name))->name,
                                                (static_cast<IdentifierExprNode *>(v->type))->name);
                    else 
                        allocateVariable((static_cast<IdentifierExprNode *>(v->name))->name);
                v = v->next;
            }
        }
        break;
    case StmtNode::DoWhile:
        {
            UnaryStmtNode *d = static_cast<UnaryStmtNode *>(p);
            preprocess(d->stmt);
        }
        break;
    case StmtNode::While:
        {
            UnaryStmtNode *w = static_cast<UnaryStmtNode *>(p);
            preprocess(w->stmt);
        }
        break;
    case StmtNode::For:
        {
            ForStmtNode *f = static_cast<ForStmtNode *>(p);
            if (f->initializer) 
                preprocess(f->initializer);
            preprocess(f->stmt);
        }
        break;
    case StmtNode::If:
        {
            UnaryStmtNode *i = static_cast<UnaryStmtNode *>(p);
            preprocess(i->stmt);
        }
        break;
    case StmtNode::IfElse:
        {
            BinaryStmtNode *i = static_cast<BinaryStmtNode *>(p);
            preprocess(i->stmt);
            preprocess(i->stmt2);
        }
        break;
    case StmtNode::With:
        {
            UnaryStmtNode *w = static_cast<UnaryStmtNode *>(p);
            preprocess(w->stmt);
        }
        break;
    case StmtNode::Switch:
        {
            SwitchStmtNode *sw = static_cast<SwitchStmtNode *>(p);
            StmtNode *s = sw->statements;
            while (s) {
                if (s->getKind() != StmtNode::Case)
                    preprocess(s);
                s = s->next;
            }
        }
        break;
    case StmtNode::label:
        {
            LabelStmtNode *l = static_cast<LabelStmtNode *>(p);
            preprocess(l->stmt);
        }
        break;
    case StmtNode::Try:
        {
            TryStmtNode *t = static_cast<TryStmtNode *>(p);
            genStmt(t->stmt);
            if (t->catches) {
                CatchClause *c = t->catches;
                while (c) {
                    preprocess(c->stmt);
                    c = c->next;
                }
            }
            if (t->finally)
                genStmt(t->finally);
        }
        break;

    default:
        break;
        
   }
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

TypedRegister ICodeGenerator::genStmt(StmtNode *p, LabelSet *currentLabelSet)
{
    TypedRegister ret(NotARegister, &None_Type);

    startStatement(p->pos);
    if (exceptionRegister.first == NotARegister) {
        exceptionRegister = grabRegister(mWorld->identifiers[widenCString("__exceptionObject__")], &Any_Type);
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
            if (classStmt->body) {
                JSScope* thisScope = thisClass->getScope();
                ICodeGenerator ccg(mWorld, thisScope, thisClass, false);   // constructor code generator.
                ccg.allocateParameter(mWorld->identifiers[widenCString("this")], thisClass);   // always parameter #0
                // XXX
                // The only use for mcg is to establish a new container class ('thisClass' instead of 'mClass')
                // and set up a new scope, maybe we could just do that inline?
                // XXX
                ICodeGenerator mcg(mWorld, thisScope, thisClass, false);   // method code generator.
                ICodeGenerator scg(mWorld, thisScope, thisClass, false);   // static initializer code generator.
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
                                    JSType* type = &Any_Type;
                                    // FIXME:  need to do code generation for type expressions.
                                    if (v->type && v->type->getKind() == ExprNode::identifier) {
                                        IdentifierExprNode* typeExpr = static_cast<IdentifierExprNode*>(v->type);
                                        type = findType(typeExpr->name);
                                    }
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
                    case StmtNode::Function:
                        {
                            mcg.preprocess(s);
                            mcg.genStmt(s);
                        }
                        break;
                    default:
                        NOT_REACHED("unimplemented class member statement");
                        break;
                    }
                    s = s->next;
                }
                // freeze the class.
                thisClass->complete();
                if (ccg.getICode()->size())
                    thisClass->setConstructor(ccg.complete());
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
            bool isStatic = hasAttribute(f->attributes, Token::Static);
            ICodeGenerator icg(mWorld, mGlobal, mClass, false);
            icg.allocateParameter(mWorld->identifiers[widenCString("this")], (mClass) ? mClass : &Any_Type);   // always parameter #0
            VariableBinding *v = f->function.parameters;
            while (v) {
                if (v->name && (v->name->getKind() == ExprNode::identifier))
                    icg.allocateParameter((static_cast<IdentifierExprNode *>(v->name))->name);
                v = v->next;
            }
            icg.preprocess(f->function.body);
            icg.genStmt(f->function.body);
            //stdOut << icg;
            ICodeModule *icm = icg.complete();
            if (f->function.name->getKind() == ExprNode::identifier)
                // XXX what to do with static methods? 
                // if (isStatic && mClass)...
                mGlobal->defineFunction((static_cast<IdentifierExprNode *>(f->function.name))->name, icm);
        }
        break;
    case StmtNode::Var:
        {
            VariableStmtNode *vs = static_cast<VariableStmtNode *>(p);
            VariableBinding *v = vs->bindings;
            while (v)  {
                if (v->name && v->initializer) {
                    if (v->name->getKind() == ExprNode::identifier) {
                        if (!mWithinWith && !mAtTopLevel) {
                            TypedRegister r = genExpr(v->name);
                            TypedRegister val = genExpr(v->initializer);
                            move(r, val);
                        }
                        else {
                            TypedRegister val = genExpr(v->initializer);
                            saveName((static_cast<IdentifierExprNode *>(v->name))->name, val);
                        }
                    }
                    // XXX what's the else case here, when does a VariableBinding NOT have an identifier child?
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
            bool withinWith = mWithinWith;
            mWithinWith = true;
            beginWith(o);
            genStmt(w->stmt);
            endWith();
            mWithinWith = withinWith;
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
                        TypedRegister eq = op(COMPARE_EQ, r, sc);
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
                    setRegisterForVariable(c->name, exceptionRegister);

                    genStmt(c->stmt);
                    if (finallyLabel)
                        jsr(finallyLabel);
                    throwStmt(exceptionRegister);
                    c = c->next;
                }
            }
            if (finallyLabel) {
                setLabel(finallyInvoker);
                jsr(finallyLabel);
                throwStmt(exceptionRegister);

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
    f << "ICM! " << (uint32)its_iCode->size() << "\n";
    return VM::operator<<(f, *its_iCode);
}

    
} // namespace ICG

} // namespace JavaScript
