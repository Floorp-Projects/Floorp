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
#endif

#include <algorithm>
#include "numerics.h"
#include "world.h"
#include "vmtypes.h"
#include "jstypes.h"
#include "jsclasses.h"
#include "icodegenerator.h"
#include "interpreter.h"
#include "exception.h"
#include "icodeasm.h"

#include <stdexcept>
#include <stdio.h>

namespace JavaScript {
namespace ICG {

using namespace VM;
using namespace JSTypes;
using namespace JSClasses;
using namespace Interpreter;
using namespace ICodeASM;

inline char narrow(char16 ch) { return char(ch); }

/************************************************************************/


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

static bool hasAttribute(const IdentifierList* identifiers, StringAtom &name)
{
    while (identifiers) {
        if (identifiers->name == name)
            return true;
        identifiers = identifiers->next;
    }
    return false;
}


/************************************************************************/

// Returns whether the tree at p will have inherently produced a
// boolean result. If it didn't we need to emit a 'Test' instruction.
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

static bool isSlotName(JSClass *c, const StringAtom &name, Reference &ref, bool lvalue)
{
    while (c) {
        if (c->hasSlot(name)) {
            const JSSlot &s = c->getSlot(name);
            if (lvalue) {
                if (s.mActual || s.mSetter) {
                    ref.mClass = c;
                    ref.mKind = Slot;
                    ref.mSlotIndex = s.mIndex;
                    ref.mType = s.mType;
                    return true;
                }
            }
            else {
                if (s.mActual || s.mGetter) {
                    ref.mKind = Slot;
                    ref.mSlotIndex = s.mIndex;
                    ref.mType = s.mType;
                    return true;
                }
            }
            return false;
        }
        c = c->getSuperClass();
    }
    return false;
}

static bool isMethodName(JSClass *c, const StringAtom &name, Reference &ref)
{
    if (c && c->hasMethod(name, ref.mSlotIndex)) {
        ref.mKind = Method;
        return true;
    }
    return false;
}


static bool isStaticName(JSClass *c, const StringAtom &name, Reference &ref)
{
    do {
        bool isConstructor = false;
        if (c->hasStatic(name, ref.mType, isConstructor)) {
            ref.mClass = c;
            ref.mKind = (isConstructor) ? Constructor : Static;
            return true;
        }
        c = c->getSuperClass();
    } while (c);
    return false;
}

bool ICodeGenerator::getVariableByName(const StringAtom &name, Reference &ref)
{
    TypedRegister v;
    v = variableList->findVariable(name);
    if (v.first == NotARegister)
        v = parameterList->findVariable(name);
    if (v.first != NotARegister) {
        ref.mKind = Var;
        ref.mBase = v;
        ref.mType = v.second;
        return true;
    }
    return false;
}

bool ICodeGenerator::scanForVariable(const StringAtom &name, Reference &ref)
{
    if (getVariableByName(name, ref))
        return true;
    
    uint32 count = 0;
    ICodeGenerator *upper = mContainingFunction;
    while (upper) {
        if (upper->getVariableByName(name, ref)) {
            ref.mKind = ClosureVar;
            ref.mSlotIndex = ref.mBase.first;
            ref.mBase = getClosure(count);
            return true;
        }
        count++;
        upper = upper->mContainingFunction;
    }
    return false;
}

// find 'name' (unqualified) in the current context.
// for local variable, returns v.first = register number
// for slot/method, returns slotIndex and sets base appropriately
// (note closure vars also get handled this way)
// v.second is set to the type regardless
bool ICodeGenerator::resolveIdentifier(const StringAtom &name, Reference &ref, bool lvalue)
{
    if (!isWithinWith()) {
        if (scanForVariable(name, ref))
            return true;
        else {
            if (mClass) {   // we're compiling a method of a class
                // look for static references first
                if (isStaticName(mClass, name, ref)) {
                    return true;
                }
                // then instance methods (if we're in a instance member function)
                // 'this' is always in register 0
                if (!isStaticMethod()) {
                    if (isSlotName(mClass, name, ref, lvalue)) {
                        ref.mBase = TypedRegister(0, mClass);
                        return true;
                    }
                    if (isMethodName(mClass, name, ref)) {
                        ref.mBase = TypedRegister(0, mClass);
                        return true;
                    }
                }
            }
            // last chance - if it's a generic name in the global scope, try to get a type for it
            ref.mKind = Name;
            ref.mType = mContext->getGlobalObject()->getType(name);
            return true;
        }
    }
    // all bet's off, generic name & type
    ref.mKind = Name;
    ref.mType = &Object_Type;
    return true;
}

Reference ICodeGenerator::genReference(ExprNode *p)
{
    switch (p->getKind()) {
    case ExprNode::identifier:
        {
            const StringAtom &name = static_cast<IdentifierExprNode *>(p)->name;
            Reference result(name);
            resolveIdentifier(name, result, true);
            return result;
        }
        break;
    case ExprNode::dotParen:
    case ExprNode::dot:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister lhs = genExpr(b->op1);    // generate code for leftside of dot
            if (b->op2->getKind() != ExprNode::identifier) {
                Reference result(mContext->getWorld().identifiers["irritating damn stringatom concept"]);
                result.mKind = Field;
                result.mBase = lhs;
                result.mField = genExpr(b->op2);
            }
            else {
                // we have <lhs>.<fieldname>
                const StringAtom &fieldName = static_cast<IdentifierExprNode *>(b->op2)->name;
                Reference result(fieldName);

                // default result..
                result.mKind = Property;
                result.mBase = lhs;
                
                // try to do better..
                if (lhs.second == &Type_Type) {     // then look for a static field
                    // special case for <Classname>, rather than an arbitrary expression
                    if ((b->op1->getKind() == ExprNode::identifier) && !isWithinWith()) {
                        const StringAtom &baseName = (static_cast<IdentifierExprNode *>(b->op1))->name;
                        const JSValue &v = mContext->getGlobalObject()->getVariable(baseName);
                        ASSERT(v.isType());
                        JSClass *c = dynamic_cast<JSClass*>(v.type);
                        if (c && isStaticName(c, fieldName, result)) {
                            return result;
                        }
                    }
                }
                else {  // if the type is a class, look for methods/fields
                    JSClass *c = dynamic_cast<JSClass*>(lhs.second);
                    if (c) {
                        if (isSlotName(c, fieldName, result, true)) {
                            return result;
                        }
                        if (isMethodName(c, fieldName, result)) {
                            return result;
                        }
                    }
                }
                return result;
            }                
        }
        break;
    default:
        NOT_REACHED("bad kind in reference expression");
        break;
    }
    return Reference(mContext->getWorld().identifiers["irritating damn stringatom concept"]);
}

TypedRegister Reference::getValue(ICodeGenerator *icg)
{
    TypedRegister result;
    switch (mKind) {
    case Var:
        result = mBase;
        break;
    case Name:
        result = icg->loadName(mName, mType);
        break;
    case Slot:
        result = icg->getSlot(mBase, mSlotIndex);
        break;
    case Static:
    case Constructor:
        result = icg->getStatic(mClass, mName);
        break;
    case Method:
        result = icg->getMethod(mBase, mSlotIndex);
        break;
    case Property:
        result = icg->getProperty(mBase, mName);
        break;
    case Field:
        result = icg->getField(mBase, mField);
        break;
    default:
        NOT_REACHED("Bad lvalue kind");
    }
    return result;
}

TypedRegister Reference::getCallTarget(ICodeGenerator *icg)
{
    TypedRegister result;
    switch (mKind) {
    case Var:
        result = mBase;
        break;
    case Name:
        result = icg->loadName(mName, mType);
        break;
    case Slot:
        result = icg->getSlot(mBase, mSlotIndex);
        break;
    case Static:
        result = icg->getStatic(mClass, mName);
        break;
    case Constructor:
        result = icg->bindThis(icg->newClass(mClass), icg->getStatic(mClass, mName));
        break;
    case Method:
        result = icg->getMethod(mBase, mSlotIndex);
        break;
    case Property:
        result = icg->bindThis(mBase, icg->getProperty(mBase, mName));
        break;
    case Field:
        result = icg->bindThis(mBase, icg->getField(mBase, mField));
        break;
    default:
        NOT_REACHED("Bad lvalue kind");
    }
    return result;
}

void Reference::setValue(ICodeGenerator *icg, TypedRegister value)
{
    switch (mKind) {
    case Var:
        icg->move(mBase, value);
        break;
    case Name:
        icg->saveName(mName, value);
        break;
    case Slot:
        icg->setSlot(mBase, mSlotIndex, value);
        break;
    case Constructor:       // XXX really ????
    case Static:
        icg->setStatic(mClass, mName, value);
        break;
    case Property:
        icg->setProperty(mBase, mName, value);
        break;
    case Field:
        icg->setField(mBase, mField, value);
        break;
    default:
        NOT_REACHED("Bad lvalue kind");
    }
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
    ICodeOp dblOp;
    JSTypes::Operator op;
    TypedRegister ret(NotARegister, &None_Type);
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
    case ExprNode::Null:
        ret = loadNull();
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
            ArgumentList *args = new ArgumentList();
            ExprPairList *p = i->pairs;
            StringFormatter s;
            while (p) {
                if (p->field && (p->field->getKind() == ExprNode::identifier))
                    args->push_back(Argument(genExpr(p->value), &(static_cast<IdentifierExprNode *>(p->field))->name));
                else {
                    if (p->field && (p->field->getKind() == ExprNode::string))
                        args->push_back(Argument(genExpr(p->value), &mContext->getWorld().identifiers[(static_cast<StringExprNode *>(p->field))->str]));
                    else {
                        s << (uint32)args->size();
                        args->push_back(Argument(genExpr(p->value), &mContext->getWorld().identifiers[s.getString()] ));
                        s.clear();
                    }
                }
                p = p->next;
            }


            ret = genericNew(genExpr(i->op), args);


#if 0
            if (i->op->getKind() == ExprNode::identifier) {
                const StringAtom &className = static_cast<IdentifierExprNode *>(i->op)->name;
                const JSValue& value = mContext->getGlobalObject()->getVariable(className);
                if (value.isType()) {
                    JSClass* clazz = dynamic_cast<JSClass*>(value.type);
                    if (clazz) {
                        ret = newClass(clazz);
                        ret = call(bindThis(ret, getStatic(clazz, className)), args);
                    }
                    else {
/*
                        //
                        // like 'new Boolean()' - see if the type has a constructor
                        //
                        JSFunction *f = value.type->getConstructor();
                        if (f)
                            ret = directCall(f, args);
                        else
*/
                            NOT_REACHED("new <name>, where <name> is not a new-able type (whatever that means)");   // XXX Runtime error.
                    }
                }
                else {
                    if (value.isFunction()) {
                        TypedRegister f = loadName(className, value.type);
                        ret = newObject(f);
                        ret = call(bindThis(ret, f), args);
                    }
                    else
                        NOT_REACHED("new <name>, where <name> is not a function");   // XXX Runtime error.
                }
            }                
            else
                ret = newObject(TypedRegister(NotARegister, &Object_Type));  // XXX more ?
#endif
        }
        break;
    case ExprNode::Delete:
        {
            Reference ref = genReference(p);
            if (ref.mKind == Property) {
                ret = deleteProperty(ref.mBase, ref.mName);
            }
            else
                if (ref.mKind == Name) {
                    NOT_REACHED("need DeleteName icode for global object deletes");
                }
                else
                    NOT_REACHED("bad deletee");
        }
        break;
    case ExprNode::call : 
        {
            InvokeExprNode *i = static_cast<InvokeExprNode *>(p);
            ArgumentList *args = new ArgumentList();
            ExprPairList *p = i->pairs;
            StringFormatter s;
            while (p) {
                if (p->field && (p->field->getKind() == ExprNode::identifier))
                    args->push_back(Argument(genExpr(p->value), &(static_cast<IdentifierExprNode *>(p->field))->name));
                else {
                    if (p->field && (p->field->getKind() == ExprNode::string))
                        args->push_back(Argument(genExpr(p->value), &mContext->getWorld().identifiers[(static_cast<StringExprNode *>(p->field))->str]));
                    else {
                        s << (uint32)args->size();
                        args->push_back(Argument(genExpr(p->value), &mContext->getWorld().identifiers[s.getString()] ));
                        s.clear();
                    }
                }
                p = p->next;
            }

            Reference ref = genReference(i->op);
            ret = invokeCallOp(ref.getCallTarget(this), args);
        }
        break;
    case ExprNode::index :
        {
            InvokeExprNode *i = static_cast<InvokeExprNode *>(p);
            TypedRegister base = genExpr(i->op);
            JSClass *clazz = dynamic_cast<JSClass*>(base.second);
            if (clazz) {
                // look for operator [] and invoke it
            }
            TypedRegister index = genExpr(i->pairs->value);     // FIXME, only taking first index
            ret = getElement(base, index);
        }
        break;
    case ExprNode::dotClass:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister lhs = genExpr(b->op1);
            ret = dotClass(lhs);
        }
        break;
    case ExprNode::dotParen:
    case ExprNode::dot :
        {
            Reference ref = genReference(p);
            ret = ref.getValue(this);
        }
        break;
    case ExprNode::This :
        {
            ret = TypedRegister(0, mClass ? mClass : &Object_Type);
        }
        break;
    case ExprNode::identifier :
        {
            Reference ref = genReference(p);
            ret = ref.getValue(this);
        }
        break;
    case ExprNode::number :
        ret = loadImmediate((static_cast<NumberExprNode *>(p))->value);
        break;
    case ExprNode::string :
        ret = loadString(mContext->getWorld().identifiers[(static_cast<StringExprNode *>(p))->str]);
        break;
    case ExprNode::preDecrement: 
        op = Decrement;
        goto GenericPreXcrement;
    case ExprNode::preIncrement:
        op = Increment;
        goto GenericPreXcrement;
GenericPreXcrement:
        {
            UnaryExprNode *u = static_cast<UnaryExprNode *>(p);
            Reference ref = genReference(u->op);
            ret = xcrementOp(op, ref.getValue(this));
        }
        break;
    case ExprNode::postDecrement: 
        op = Decrement;
        goto GenericPostXcrement;
    case ExprNode::postIncrement: 
        op = Increment;
        goto GenericPostXcrement;
GenericPostXcrement:
        {
            UnaryExprNode *u = static_cast<UnaryExprNode *>(p);
            Reference ref = genReference(u->op);
            ret = ref.getValue(this);
            ref.setValue(this, xcrementOp(op, ret));
        }
        break;

    case ExprNode::plus:
        op = JSTypes::Posate;
        goto GenericUnary;
    case ExprNode::minus:
        op = JSTypes::Negate;
        goto GenericUnary;
    case ExprNode::complement:
        op = JSTypes::Complement;
        goto GenericUnary;
GenericUnary:
        {
            UnaryExprNode *u = static_cast<UnaryExprNode *>(p);
            TypedRegister r = genExpr(u->op);
            ret = unaryOp(op, r);
        }
        break;
    case ExprNode::add:
        dblOp = ADD; op = JSTypes::Plus;
        goto GenericBinary;
    case ExprNode::subtract:
        dblOp = SUBTRACT; op = JSTypes::Minus;
        goto GenericBinary;
    case ExprNode::multiply:
        dblOp = MULTIPLY; op = JSTypes::Multiply;
        goto GenericBinary;
    case ExprNode::divide:
        dblOp = DIVIDE; op = JSTypes::Divide;
        goto GenericBinary;
    case ExprNode::modulo:
        dblOp = REMAINDER; op = JSTypes::Remainder;
        goto GenericBinary;
    case ExprNode::leftShift:
        dblOp = SHIFTLEFT; op = JSTypes::ShiftLeft;
        goto GenericBinary;
    case ExprNode::rightShift:
        dblOp = SHIFTRIGHT; op = JSTypes::ShiftRight;
        goto GenericBinary;
    case ExprNode::logicalRightShift:
        dblOp = USHIFTRIGHT; op = JSTypes::UShiftRight;
        goto GenericBinary;
    case ExprNode::bitwiseAnd:
        dblOp = AND; op = JSTypes::BitAnd;
        goto GenericBinary;
    case ExprNode::bitwiseXor:
        dblOp = XOR; op = JSTypes::BitXor;
        goto GenericBinary;
    case ExprNode::bitwiseOr:
        dblOp = OR; op = JSTypes::BitOr;
        goto GenericBinary;
GenericBinary:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister r1 = genExpr(b->op1);
            TypedRegister r2 = genExpr(b->op2);
            ret = binaryOp(dblOp, op, r1, r2);
        }
        break;
    case ExprNode::assignment:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            Reference ref = genReference(b->op1);
            ret = genExpr(b->op2);
            ref.setValue(this, ret);
        }
        break;
    case ExprNode::addEquals:
        dblOp = ADD; op = JSTypes::Plus;
        goto GenericBinaryEquals;
    case ExprNode::subtractEquals:
        dblOp = SUBTRACT; op = JSTypes::Minus;
        goto GenericBinaryEquals;
    case ExprNode::multiplyEquals:
        dblOp = MULTIPLY; op = JSTypes::Multiply;
        goto GenericBinaryEquals;
    case ExprNode::divideEquals:
        dblOp = DIVIDE; op = JSTypes::Divide;
        goto GenericBinaryEquals;
    case ExprNode::moduloEquals:
        dblOp = REMAINDER; op = JSTypes::Remainder;
        goto GenericBinaryEquals;
    case ExprNode::leftShiftEquals:
        dblOp = SHIFTLEFT; op = JSTypes::ShiftLeft;
        goto GenericBinaryEquals;
    case ExprNode::rightShiftEquals:
        dblOp = SHIFTRIGHT; op = JSTypes::ShiftRight;
        goto GenericBinaryEquals;
    case ExprNode::logicalRightShiftEquals:
        dblOp = USHIFTRIGHT; op = JSTypes::UShiftRight;
        goto GenericBinaryEquals;
    case ExprNode::bitwiseAndEquals:
        dblOp = AND; op = JSTypes::BitAnd;
        goto GenericBinaryEquals;
    case ExprNode::bitwiseXorEquals:
        dblOp = XOR; op = JSTypes::BitXor;
        goto GenericBinaryEquals;
    case ExprNode::bitwiseOrEquals:
        dblOp = OR; op = JSTypes::BitOr;
        goto GenericBinaryEquals;
GenericBinaryEquals:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            Reference ref = genReference(b->op1);
            ret = genExpr(b->op2);            
            ret = binaryOp(dblOp, op, ref.getValue(this), ret);
            ref.setValue(this, ret);
        }
        break;
    case ExprNode::equal:
        dblOp = COMPARE_EQ; op = JSTypes::Equal;
        goto GenericConditionalBranch;
    case ExprNode::lessThan:
        dblOp = COMPARE_LT; op = JSTypes::Less;
        goto GenericConditionalBranch;
    case ExprNode::lessThanOrEqual:
        dblOp = COMPARE_LE; op = JSTypes::LessEqual;
        goto GenericConditionalBranch;
    case ExprNode::identical:
        dblOp = STRICT_EQ; op = JSTypes::SpittingImage;
        goto GenericConditionalBranch;
    case ExprNode::In:
        dblOp = COMPARE_IN; op = JSTypes::In;
        goto GenericConditionalBranch;
GenericConditionalBranch:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister r1 = genExpr(b->op1);
            TypedRegister r2 = genExpr(b->op2);
            ret = binaryOp(dblOp, op, r1, r2);
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
    case ExprNode::Instanceof:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister r1 = genExpr(b->op1);
            TypedRegister r2 = genExpr(b->op2);
            ret = instanceOf(r1, r2);
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
        dblOp = COMPARE_LE; op = JSTypes::LessEqual;
        goto GenericReverseBranch;
    case ExprNode::greaterThanOrEqual:
        dblOp = COMPARE_LT; op = JSTypes::Less;
        goto GenericReverseBranch;
GenericReverseBranch:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister r1 = genExpr(b->op1);
            TypedRegister r2 = genExpr(b->op2);
            ret = binaryOp(dblOp, op, r2, r1);
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
        dblOp = COMPARE_EQ; op = JSTypes::Equal;
        goto GenericNotBranch;
    case ExprNode::notIdentical:
        dblOp = STRICT_EQ; op = JSTypes::SpittingImage;
        goto GenericNotBranch;
GenericNotBranch:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            TypedRegister r1 = genExpr(b->op1);
            TypedRegister r2 = genExpr(b->op2);
            ret = binaryOp(dblOp, op, r1, r2);
            if (trueBranch || falseBranch) {
                if (trueBranch == NULL)
                    branchTrue(falseBranch, ret);
                else {
                    branchFalse(trueBranch, ret);
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
            ret = newObject(TypedRegister(NotARegister, &Object_Type));
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
            ICodeModule *icm = genFunction(f->function, false, false, NULL);
            ret = newClosure(icm);
        }
        break;

    case ExprNode::at:
        {
            BinaryExprNode *b = static_cast<BinaryExprNode *>(p);
            // for now, just handle simple identifiers on the rhs.
            ret = genExpr(b->op1);
            if (b->op2->getKind() == ExprNode::identifier) {
                TypedRegister t;
                const StringAtom &name = (static_cast<IdentifierExprNode *>(b->op2))->name;
                ASSERT(t.second == &Type_Type);
                const JSValue &v = mContext->getGlobalObject()->getVariable(name);
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



ICodeModule *ICodeGenerator::genFunction(FunctionDefinition &function, bool isStatic, bool isConstructor, JSClass *superclass)
{
    ICodeGeneratorFlags flags = (isStatic) ? kIsStaticMethod : kNoFlags;
    
    ICodeGenerator icg(mContext, this, mClass, flags, mContext->extractType(function.resultType));
    icg.allocateParameter(mContext->getWorld().identifiers["this"], false, (mClass) ? mClass : &Object_Type);   // always parameter #0
    VariableBinding *v = function.parameters;
    bool unnamed = true;
    uint32 positionalCount = 0;
    StringFormatter s;
    while (v) {
        if (unnamed && (v == function.namedParameters)) { // Track when we hit the first named parameter.
            icg.parameterList->setPositionalCount(positionalCount);
            unnamed = false;
        }

        // The rest parameter is ignored in this processing - we push it to the end of the list.
        // But we need to track whether it comes before or after the |
        if (v == function.restParameter) {
            icg.parameterList->setRestParameter( (unnamed) ? ParameterList::HasRestParameterBeforeBar : ParameterList::HasRestParameterAfterBar );
        }
        else {            
            if (v->name && (v->name->getKind() == ExprNode::identifier)) {
                JSType *pType = mContext->extractType(v->type);
                TypedRegister r = icg.allocateParameter((static_cast<IdentifierExprNode *>(v->name))->name, (v->initializer != NULL), pType);
                IdentifierList *a = v->aliases;
                while (a) {
                    icg.parameterList->add(a->name, r, (v->initializer != NULL));
                    a = a->next;
                }
                // every unnamed parameter is also named with it's positional name
                if (unnamed) {
                    positionalCount++;
                    s << r.first - 1;  // the first positional parameter is '0'
                    icg.parameterList->add(mContext->getWorld().identifiers[s.getString()], r, (v->initializer != NULL));
                    s.clear();
                }
            }
        }
        v = v->next;
    }
    if (unnamed) icg.parameterList->setPositionalCount(positionalCount);

    // now allocate the rest parameter
    if (function.restParameter) {
        v = function.restParameter;
        JSType *pType = (v->type == NULL) ? &Array_Type : mContext->extractType(v->type);
        if (v->name && (v->name->getKind() == ExprNode::identifier))
            icg.allocateParameter((static_cast<IdentifierExprNode *>(v->name))->name, (v->initializer != NULL), pType);
        else
            icg.parameterList->setRestParameter(ParameterList::HasUnnamedRestParameter);
    }

    // generate the code for optional initializers
    v = function.optParameters;
    if (v) {
        while (v) {    // include the rest parameter, as it may have an initializer
            if (v->name && (v->name->getKind() == ExprNode::identifier)) {
                icg.addParameterLabel(icg.setLabel(icg.getLabel()));
                TypedRegister p = icg.genExpr(v->name);
                if (v->initializer) {           // might be NULL when we get to the restParameter
                    Label *l = icg.getLabel();
                    icg.branchInitialized(l, p);
                    icg.move(p, icg.genExpr(v->initializer));
                    icg.setLabel(l);
                }
                else {  // an un-initialized rest parameter is still an empty array
                    if (v == function.restParameter) {
                        Label *l = icg.getLabel();
                        icg.branchInitialized(l, p);
                        icg.move(p, icg.newArray());
                        icg.setLabel(l);
                    }
                }
            }
            v = v->next;
        }
        icg.addParameterLabel(icg.setLabel(icg.getLabel()));    // to provide the entry-point for the default case
    }

    if (isConstructor) {
        /*
           See if the first statement is an expression statement consisting
           of a call to super(). If not we need to add a call to the default 
           superclass constructor ourselves.
        */
        TypedRegister thisValue = TypedRegister(0, mClass);
        ArgumentList *args = new ArgumentList();
        if (superclass) {
            bool foundSuperCall = false;
            BlockStmtNode *b = function.body;
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
                icg.call(icg.bindThis(thisValue, icg.getStatic(superclass, superclass->getName())), args);
            }
        }
        if (mClass->hasStatic(mInitName))
            icg.call(icg.bindThis(thisValue, icg.getStatic(mClass, mInitName)), args);   // ok, so it's mis-named

    }
    if (function.body)
        icg.genStmt(function.body);
    if (isConstructor) {
        TypedRegister thisValue = TypedRegister(0, mClass);
        icg.returnStmt(thisValue);
    }
    return icg.complete();
}


JSTypes::Operator simpleLookup[ExprNode::kindsEnd] = {
   JSTypes::None,                    // none,
   JSTypes::None,                    // identifier,
   JSTypes::None,                    // number,
   JSTypes::None,                    // string,
   JSTypes::None,                    // regExp
   JSTypes::None,                    // Null,
   JSTypes::None,                    // True,
   JSTypes::None,                    // False,
   JSTypes::None,                    // This,
   JSTypes::None,                    // Super,
   JSTypes::None,                    // parentheses,
   JSTypes::None,                    // numUnit,
   JSTypes::None,                    // exprUnit,
   JSTypes::None,                    // qualify,
   JSTypes::None,                    // objectLiteral,
   JSTypes::None,                    // arrayLiteral,
   JSTypes::None,                    // functionLiteral,
   JSTypes::None,                    // call,
   JSTypes::None,                    // New,
   JSTypes::None,                    // index,
   JSTypes::None,                    // dot,
   JSTypes::None,                    // dotClass,
   JSTypes::None,                    // dotParen,
   JSTypes::None,                    // at,
   JSTypes::None,                    // Delete,
   JSTypes::None,                    // Typeof,
   JSTypes::None,                    // Eval,
   JSTypes::None,                    // preIncrement,
   JSTypes::None,                    // preDecrement,
   JSTypes::None,                    // postIncrement,
   JSTypes::None,                    // postDecrement,
   JSTypes::None,                    // plus,
   JSTypes::None,                    // minus,
   JSTypes::Complement,              // complement,
   JSTypes::None,                    // logicalNot,
   JSTypes::None,                    // add,
   JSTypes::None,                    // subtract,
   JSTypes::Multiply,                // multiply,
   JSTypes::Divide,                  // divide,
   JSTypes::Remainder,               // modulo,
   JSTypes::ShiftLeft,               // leftShift,
   JSTypes::ShiftRight,              // rightShift,
   JSTypes::UShiftRight,             // logicalRightShift,
   JSTypes::BitAnd,                  // bitwiseAnd,
   JSTypes::BitXor,                  // bitwiseXor,
   JSTypes::BitOr,                   // bitwiseOr,
   JSTypes::None,                    // logicalAnd,
   JSTypes::None,                    // logicalXor,
   JSTypes::None,                    // logicalOr,
   JSTypes::Equal,                   // equal,
   JSTypes::None,                    // notEqual,
   JSTypes::Less,                    // lessThan,
   JSTypes::LessEqual,               // lessThanOrEqual,
   JSTypes::None,                    // greaterThan,
   JSTypes::None,                    // greaterThanOrEqual,
   JSTypes::SpittingImage,           // identical,
   JSTypes::None,                    // notIdentical,
   JSTypes::In,                      // In,
   JSTypes::None,                    // Instanceof,
   JSTypes::None,                    // assignment,
   JSTypes::None,                    // addEquals,
   JSTypes::None,                    // subtractEquals,
   JSTypes::None,                    // multiplyEquals,
   JSTypes::None,                    // divideEquals,
   JSTypes::None,                    // moduloEquals,
   JSTypes::None,                    // leftShiftEquals,
   JSTypes::None,                    // rightShiftEquals,
   JSTypes::None,                    // logicalRightShiftEquals,
   JSTypes::None,                    // bitwiseAndEquals,
   JSTypes::None,                    // bitwiseXorEquals,
   JSTypes::None,                    // bitwiseOrEquals,
   JSTypes::None,                    // logicalAndEquals,
   JSTypes::None,                    // logicalXorEquals,
   JSTypes::None,                    // logicalOrEquals,
   JSTypes::None,                    // conditional,
   JSTypes::None,                    // comma,
};

JSTypes::Operator ICodeGenerator::getOperator(uint32 parameterCount, String &name)
{

    Lexer operatorLexer(mContext->getWorld(), name, widenCString("Operator name"), 0); // XXX get source and line number from function ???
    
    const Token &t = operatorLexer.get(false);  // XXX what's correct for preferRegExp parameter ???

    JSTypes::Operator op = simpleLookup[t.getKind()];
    if (op != JSTypes::None)
        return op;
    else {
        switch (t.getKind()) {
            default:
                NOT_REACHED("Illegal operator name");

            case Token::plus:
                if (parameterCount == 1)
                    return JSTypes::Posate;
                else
                    return JSTypes::Plus;
            case Token::minus:
                if (parameterCount == 1)
                    return JSTypes::Negate;
                else
                    return JSTypes::Minus;

            case Token::openParenthesis:
                return JSTypes::Call;
            
            case Token::New:
                if (parameterCount > 1)
                    return JSTypes::NewArgs;
                else
                    return JSTypes::New;
            
            case Token::openBracket:
                {
                    const Token &t2 = operatorLexer.get(false);
                    ASSERT(t2.getKind() == Token::closeBracket);
                    const Token &t3 = operatorLexer.get(false);
                    if (t3.getKind() == Token::equal)
                        return JSTypes::IndexEqual;
                    else
                        return JSTypes::Index;
                }
                
            case Token::Delete:
                return JSTypes::DeleteIndex;
        }
    }
    return JSTypes::None;
    
}


TypedRegister ICodeGenerator::genStmt(StmtNode *p, LabelSet *currentLabelSet)
{
    TypedRegister ret(NotARegister, &None_Type);

    startStatement(p->pos);

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
                const JSValue& superclassValue = mContext->getGlobalObject()->getVariable(superclassExpr->name);
                ASSERT(superclassValue.isObject() && !superclassValue.isNull());
                superclass = static_cast<JSClass*>(superclassValue.object);
            }
            JSClass* thisClass = new JSClass(mContext->getGlobalObject(), nameExpr->name, superclass);
            // is it ok for a partially defined class to appear in global scope? this is needed
            // to handle recursive types, such as linked list nodes.
            mContext->getGlobalObject()->defineVariable(nameExpr->name, &Type_Type, JSValue(thisClass));


/*
    Pre-pass to declare all the methods & fields
*/
            bool needsInstanceInitializer = false;
            TypedRegister thisRegister = TypedRegister(0, thisClass);
            if (classStmt->body) {
                StmtNode* s = classStmt->body->statements;
                while (s) {
                    switch (s->getKind()) {
                    case StmtNode::Const:
                    case StmtNode::Var:
                        {
                            VariableStmtNode *vs = static_cast<VariableStmtNode *>(s);
                            bool isStatic = hasAttribute(vs->attributes, Token::Static);
                            VariableBinding *v = vs->bindings;
                            while (v)  {
                                if (v->name) {
                                    ASSERT(v->name->getKind() == ExprNode::identifier);
                                    IdentifierExprNode* idExpr = static_cast<IdentifierExprNode*>(v->name);
                                    JSType* type = mContext->extractType(v->type);
                                    if (isStatic)
                                        thisClass->defineStatic(idExpr->name, type);
                                    else {
                                        if (hasAttribute(vs->attributes, mContext->getWorld().identifiers["virtual"]))
                                            thisClass->defineSlot(idExpr->name, type, JSSlot::kIsVirtual);
                                        else
                                            thisClass->defineSlot(idExpr->name, type);
                                        if (v->initializer)
                                            needsInstanceInitializer = true;
                                    }
                                }
                                v = v->next;
                            }
                        }
                        break;
                    case StmtNode::Function:
                        {
                            FunctionStmtNode *f = static_cast<FunctionStmtNode *>(s);
                            bool isStatic = hasAttribute(f->attributes, Token::Static);
                            bool isConstructor = hasAttribute(f->attributes, mContext->getWorld().identifiers["constructor"]);
                            bool isOperator = hasAttribute(f->attributes, mContext->getWorld().identifiers["operator"]);
                            if (isOperator) {
                                ASSERT(f->function.name->getKind() == ExprNode::string);
                                Operator op = getOperator(mContext->getParameterCount(f->function),
                                                                (static_cast<StringExprNode *>(f->function.name))->str);
                                thisClass->defineOperator(op, mContext->getParameterType(f->function, 0), 
                                                              mContext->getParameterType(f->function, 1), NULL);
                            }
                            else
                                if (f->function.name->getKind() == ExprNode::identifier) {
                                    const StringAtom& name = (static_cast<IdentifierExprNode *>(f->function.name))->name;
                                    if (isConstructor)
                                        thisClass->defineConstructor(name);
                                    else
                                        if (isStatic)
                                            thisClass->defineStatic(name, &Function_Type);
                                        else {
                                            switch (f->function.prefix) {
                                            case FunctionName::Get:
                                                thisClass->setGetter(name, NULL, mContext->extractType(f->function.resultType));
                                                break;
                                            case FunctionName::Set:
                                                thisClass->setSetter(name, NULL, mContext->extractType(f->function.resultType));
                                                break;
                                            case FunctionName::normal:
                                                thisClass->defineMethod(name, NULL);
                                                break;
                                            default:
                                                NOT_REACHED("unexpected prefix");
                                                break;
                                            }
                                        }
                                }
                        }                        
                        break;
                    default:
                        NOT_REACHED("unimplemented class member statement");
                        break;
                    }
                    s = s->next;
                }
            }
            if (needsInstanceInitializer)
                thisClass->defineStatic(mInitName, &Function_Type);
/*  
    Now gen code for each
*/
            
            bool hasDefaultConstructor = false;
            if (classStmt->body) {
                JSScope* thisScope = thisClass->getScope();
                ICodeGenerator *ccg = NULL;
                Context *classContext = new Context(mContext->getWorld(), thisScope);
                if (needsInstanceInitializer) {
                      // constructor code generator. Slot variable
                      // initializers get added to this function.
                    ccg = new ICodeGenerator(classContext, NULL, thisClass, kNoFlags, &Void_Type);   
                    ccg->allocateParameter(mContext->getWorld().identifiers["this"], false, thisClass);   // always parameter #0
                }
                
                 // static initializer code generator.
                 // static field inits, plus code to initialize
                 // static method slots.
                ICodeGenerator scg(classContext, NULL, thisClass, kIsStaticMethod, &Void_Type);
                StmtNode* s = classStmt->body->statements;
                while (s) {
                    switch (s->getKind()) {
                    case StmtNode::Const:
                    case StmtNode::Var:
                        {
                            VariableStmtNode *vs = static_cast<VariableStmtNode *>(s);
                            bool isStatic = hasAttribute(vs->attributes, Token::Static);
                            VariableBinding *v = vs->bindings;
                            while (v)  {
                                if (v->name) {
                                    ASSERT(v->name->getKind() == ExprNode::identifier);
                                    if (v->initializer) {
                                        IdentifierExprNode* idExpr = static_cast<IdentifierExprNode*>(v->name);
                                        if (isStatic) {
                                            scg.setStatic(thisClass, idExpr->name, scg.genExpr(v->initializer));
                                            scg.resetStatement();
                                        } else {
                                            const JSSlot& slot = thisClass->getSlot(idExpr->name);
                                            ccg->setSlot(thisRegister, slot.mIndex, ccg->genExpr(v->initializer));
                                            ccg->resetStatement();
                                        }
                                    }
                                }
                                v = v->next;
                            }
                        }
                        break;
                    case StmtNode::Function:   
                        {
                            FunctionStmtNode *f = static_cast<FunctionStmtNode *>(s);
                            bool isStatic = hasAttribute(f->attributes, Token::Static);
                            bool isConstructor = hasAttribute(f->attributes, mContext->getWorld().identifiers["constructor"]);
                            bool isOperator = hasAttribute(f->attributes, mContext->getWorld().identifiers["operator"]);
                            ICodeGeneratorFlags flags = (isStatic) ? kIsStaticMethod : kNoFlags;

                            ICodeGenerator mcg(classContext, NULL, thisClass, flags);   // method code generator.
                            ICodeModule *icm = mcg.genFunction(f->function, isStatic, isConstructor, superclass);
                            if (isOperator) {
                                ASSERT(f->function.name->getKind() == ExprNode::string);
                                Operator op = getOperator(mContext->getParameterCount(f->function),
                                                                (static_cast<StringExprNode *>(f->function.name))->str);
                                thisClass->defineOperator(op, mContext->getParameterType(f->function, 0), 
                                                              mContext->getParameterType(f->function, 1),  new JSFunction(icm));
                            }
                            else
                                if (f->function.name->getKind() == ExprNode::identifier) {
                                    const StringAtom& name = (static_cast<IdentifierExprNode *>(f->function.name))->name;
                                    if (isConstructor) {
                                        if (name == nameExpr->name)
                                            hasDefaultConstructor = true;
                                        scg.setStatic(thisClass, name, scg.newFunction(icm));
                                    }
                                    else
                                        if (isStatic)
                                            scg.setStatic(thisClass, name, scg.newFunction(icm));
                                        else {
                                            switch (f->function.prefix) {
                                            case FunctionName::Get:
                                                thisClass->setGetter(name, new JSFunction(icm), icm->mResultType);
                                                break;
                                            case FunctionName::Set:
                                                thisClass->setSetter(name, new JSFunction(icm), icm->mResultType);
                                                break;
                                            case FunctionName::normal:
                                                thisClass->defineMethod(name, new JSFunction(icm));
                                                break;
                                            default:
                                                NOT_REACHED("unexpected prefix");
                                                break;
                                            }
                                        }
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
                if (ccg) {
                    scg.setStatic(thisClass, mInitName, scg.newFunction(ccg->complete()));
                    delete ccg;
                }
                // invent a default constructor if necessary, it just calls the 
                //   initializer and the superclass default constructor
                if (!hasDefaultConstructor) {
                    TypedRegister thisValue = TypedRegister(0, thisClass);
                    ArgumentList *args = new ArgumentList();
                    ICodeGenerator icg(classContext, NULL, thisClass, kIsStaticMethod, &Void_Type);
                    icg.allocateParameter(mContext->getWorld().identifiers["this"], false, thisClass);   // always parameter #0
                    if (superclass)
                        icg.call(icg.bindThis(thisValue, icg.getStatic(superclass, superclass->getName())), args);
                    if (thisClass->hasStatic(mInitName))
                        icg.call(icg.bindThis(thisValue, icg.getStatic(thisClass, mInitName)), args);
                    icg.returnStmt(thisValue);
                    thisClass->defineConstructor(nameExpr->name);
                    scg.setStatic(thisClass, nameExpr->name, scg.newFunction(icg.complete()));
                }
                // freeze the class.
                thisClass->complete();


                // REVISIT:  using the scope of the class to store both methods and statics.
                if (scg.getICode()->size()) {
                    ICodeModule* clinit = scg.complete();
                    classContext->interpret(clinit, JSValues());
                    delete clinit;
                }
                delete classContext;
            }
        }
        break;
    case StmtNode::Function:
        {
            FunctionStmtNode *f = static_cast<FunctionStmtNode *>(p);
            bool isStatic = hasAttribute(f->attributes, Token::Static);
            ICodeModule *icm = genFunction(f->function, isStatic, false, NULL);
            JSType *resultType = mContext->extractType(f->function.resultType);
            if (f->function.name->getKind() == ExprNode::identifier) {
                const StringAtom& name = (static_cast<IdentifierExprNode *>(f->function.name))->name;
                switch (f->function.prefix) {
                case FunctionName::Get:
                    if (isTopLevel()) {
                        mContext->getGlobalObject()->defineVariable(name, resultType);
                        mContext->getGlobalObject()->setGetter(name, new JSFunction(icm));
                    }
                    else {
                        // is this legal - a nested getter?
                        NOT_REACHED("Better check with Waldemar");
                        //allocateVariable(name, resultType);
                    }
                    break;
                case FunctionName::Set:
                    if (isTopLevel()) {
                        mContext->getGlobalObject()->defineVariable(name, resultType);
                        mContext->getGlobalObject()->setSetter(name, new JSFunction(icm));
                    }
                    else {
                        // is this legal - a nested setter?
                        NOT_REACHED("Better check with Waldemar");
                        //allocateVariable(name, resultType);
                    }
                    break;
                case FunctionName::normal:
                    mContext->getGlobalObject()->defineFunction(name, icm);
                    break;
                default:
                    NOT_REACHED("unexpected prefix");
                    break;
                }
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
                    (void)mContext->readEvalFile(f, *fileName);
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
                    JSType *type = mContext->extractType(v->type);
                    if (isTopLevel())
                        mContext->getGlobalObject()->defineVariable((static_cast<IdentifierExprNode *>(v->name))->name, type);
                    else
                        allocateVariable((static_cast<IdentifierExprNode *>(v->name))->name, type);
                    if (v->initializer) {
                        if (!isTopLevel() && !isWithinWith()) {
                            TypedRegister r = genExpr(v->name);
                            TypedRegister val = genExpr(v->initializer);
                            if (type != &Object_Type)
                                val = cast(val, type);
                            move(r, val);
                        }
                        else {
                            TypedRegister val = genExpr(v->initializer);
                            if (type != &Object_Type)
                                val = cast(val, type);
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
            Label *trueLabel = getLabel();
            Label *beyondLabel = getLabel();
            BinaryStmtNode *i = static_cast<BinaryStmtNode *>(p);
            TypedRegister c = genExpr(i->expr, false, trueLabel, falseLabel);
            if (!generatedBoolean(i->expr))
                branchFalse(falseLabel, test(c));
            setLabel(trueLabel);
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
                        TypedRegister eq = binaryOp(COMPARE_EQ, JSTypes::Equal, r, sc);
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
                    if (mExceptionRegister.first == NotABanana)
                        mExceptionRegister = allocateRegister(&Object_Type);
                    allocateVariable(c->name, mExceptionRegister);

                    genStmt(c->stmt);
                    if (finallyLabel)
                        jsr(finallyLabel);
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

    
} // namespace ICG

} // namespace JavaScript
