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


ICodeGenerator::ICodeGenerator(Context *cx, 
                               ICodeGenerator *containingFunction, 
                               JSClass *aClass, 
                               ICodeGeneratorFlags flags,
                               JSType *resultType)
    :   mTopRegister(0), 
        mExceptionRegister(TypedRegister(NotARegister, &None_Type)),
        variableList(new VariableList()),
        parameterList(new ParameterList()),
        mContext(cx),
        mInstructionMap(new InstructionMap()),
        mClass(aClass),
        mFlags(flags),
        pLabels(NULL),
        mInitName(cx->getWorld().identifiers["__init__"]),
        mContainingFunction(containingFunction),
        mResultType(resultType)
{ 
    iCode = new InstructionStream();
    iCodeOwner = true;
}

/*
    -Called to allocate parameter and variable registers, aka 'permanent' registers.
    -mTopRegister is the current high-water mark.
    -mPermanentRegister marks those registers given to variables/parameters.
    -Theoretically, mPermanentRegister[n] can be become false when a scope ends and
     the registers allocated to contained variables are then available for re-use.
    -Mostly the need is to handle overlapping allocation of temps & permanents as the
     variables' declarations are encountered. This wouldn't be necessary if a function
     presented a list of all variables, or a pre-pass executed to discover same.
*/
TypedRegister ICodeGenerator::allocateRegister(JSType *type) 
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
    mTopRegister = ++r;
    return result;
}

TypedRegister ICodeGenerator::allocateVariable(const StringAtom& name, const StringAtom& typeName)
{
    return allocateVariable(name, mContext->findType(typeName));
}

TypedRegister ICodeGenerator::allocateParameter(const StringAtom& name, bool isOptional, const StringAtom& typeName)
{
    return allocateParameter(name, isOptional, mContext->findType(typeName));
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
    ICodeModule* module = new ICodeModule(*this);
    if (pLabels) {
        uint32 i;
        uint32 parameterInits = pLabels->size() - 1;        // there's an extra label at the end for the actual entryPoint
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
    if (value) 
        iCode->push_back(new LoadTrue(dest));
    else
        iCode->push_back(new LoadFalse(dest));
    return dest;
}

TypedRegister ICodeGenerator::loadNull()
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    iCode->push_back(new LoadNull(dest));
    return dest;
}

TypedRegister ICodeGenerator::loadType(JSType *type)
{
    TypedRegister dest(getTempRegister(), type);
    iCode->push_back(new LoadType(dest, type));
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

TypedRegister ICodeGenerator::newFunction(ICodeModule *icm)
{
    TypedRegister dest(getTempRegister(), &Function_Type);
    NewFunction *instr = new NewFunction(dest, icm);
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


static const JSSlot& getStaticSlot(JSClass *&c, const String &name)
{
    if (c->hasStatic(name))
        return c->getStatic(name);
    c = c->getSuperClass();
    ASSERT(c);
    return getStaticSlot(c, name);
}

TypedRegister ICodeGenerator::getStatic(JSClass *base, const String &name)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    const JSSlot& slot = getStaticSlot(base, name);
    GetStatic *instr = new GetStatic(dest, base, slot.mIndex);
    iCode->push_back(instr);
    return dest;
}

void ICodeGenerator::setStatic(JSClass *base, const StringAtom &name,
                                 TypedRegister value)
{
    const JSSlot& slot = getStaticSlot(base, name);
    SetStatic *instr = new SetStatic(base, slot.mIndex, value);
    iCode->push_back(instr);
}

TypedRegister ICodeGenerator::staticXcr(JSClass *base, const StringAtom &name, ICodeOp /*op*/)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    const JSSlot& slot = getStaticSlot(base, name);
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
        GenericBinaryOP *instr = new GenericBinaryOP(dest, mapICodeOpToExprNode(op), source1, source2);
        iCode->push_back(instr);
    }
    return dest;
} 
    
TypedRegister ICodeGenerator::call(TypedRegister target, ArgumentList *args)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    Call *instr = new Call(dest, target, args);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::directCall(JSFunction *target, ArgumentList *args)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    DirectCall *instr = new DirectCall(dest, target, args);
    iCode->push_back(instr);
    return dest;
}

TypedRegister ICodeGenerator::bindThis(TypedRegister thisArg, TypedRegister target)
{
    TypedRegister dest(getTempRegister(), &Function_Type);
    iCode->push_back(new BindThis(dest, thisArg, target));
    return dest;
}


TypedRegister ICodeGenerator::getClosure(uint32 count)
{
    TypedRegister dest(getTempRegister(), &Any_Type);
    iCode->push_back(new GetClosure(dest, count));
    return dest;
}

TypedRegister ICodeGenerator::newClosure(ICodeModule *icm)
{
    TypedRegister dest(getTempRegister(), &Function_Type);
    iCode->push_back(new NewClosure(dest, icm));
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
    Cast *instr = new Cast(dest, arg, loadType(toType));
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

GenericBranch *ICodeGenerator::branchInitialized(Label *label, TypedRegister condition)
{
    GenericBranch *instr = new GenericBranch(BRANCH_INITIALIZED, label, condition);
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

/*************************************************************************/

Formatter& operator<<(Formatter &f, string &s)
{
    f << s.c_str();
    return f;
}

ICodeModule *ICodeGenerator::readFunction(XMLNode *element, JSClass *thisClass)
{
    ICodeModule *result = NULL;

    String resultTypeName;
    element->getValue(widenCString("type"), resultTypeName);
    ParameterList *theParameterList = new ParameterList();
    theParameterList->add(mContext->getWorld().identifiers["this"], TypedRegister(0, thisClass), false);
    uint32 pCount = 1;
    StringFormatter s;
    XMLNodeList &parameters = element->children();
    for (XMLNodeList::const_iterator k = parameters.begin(); k != parameters.end(); k++) {
        XMLNode *parameter = *k;
        if (parameter->name().compare(widenCString("parameter")) == 0) {                    
            String parameterName;
            String parameterTypeName;
            element->getValue(widenCString("name"), parameterName);
            element->getValue(widenCString("type"), parameterTypeName);
            JSType *parameterType = mContext->findType(mContext->getWorld().identifiers[parameterTypeName]);
            theParameterList->add(mContext->getWorld().identifiers[parameterName], TypedRegister(pCount, parameterType), false);
            s << pCount - 1;
            theParameterList->add(mContext->getWorld().identifiers[s.getString()], TypedRegister(pCount, parameterType), false);
            s.clear();
            pCount++;
        }
    }
    theParameterList->setPositionalCount(pCount);

    JSType *resultType = mContext->findType(mContext->getWorld().identifiers[resultTypeName]);
    String &body = element->body();
    if (body.length()) {
        std::string str(body.length(), char());
        std::transform(body.begin(), body.end(), str.begin(), narrow);
        ICodeParser icp(mContext);

        stdOut << "Calling ICodeParser with :\n" << str << "\n";

        icp.parseSourceFromString(str);

        result = new ICodeModule(icp.mInstructions, 
                                            NULL,                   /* VariableList *variables */
                                            theParameterList,       /* ParameterList *parameters */
                                            icp.mMaxRegister, 
                                            NULL,                   /* InstructionMap *instructionMap */
                                            resultType, 
                                            NotABanana);            /* exception register */
    }
    return result;
}

ICodeModule *ICodeGenerator::readICode(const char *fileName)
{
    ICodeModule *result = NULL;

    XMLParser xp(fileName);
    XMLNode *top = xp.parseDocument();
    stdOut << *top;

    XMLNodeList &kids = top->children();
    for (XMLNodeList::const_iterator i = kids.begin(); i != kids.end(); i++) {
        XMLNode *node = *i;

        if (node->name().compare(widenCString("class")) == 0) {
            String className;
            String superName;
            JSClass* superclass = 0;

            node->getValue(widenCString("name"), className);
            if (node->getValue(widenCString("super"), superName)) {
                const JSValue& superclassValue = mContext->getGlobalObject()->getVariable(superName);
                superclass = static_cast<JSClass*>(superclassValue.object);
            }
            JSClass* thisClass = new JSClass(mContext->getGlobalObject(), className, superclass);
            JSScope* thisScope = thisClass->getScope();
            Context *classContext = new Context(mContext->getWorld(), thisScope);
            ICodeGenerator scg(classContext, NULL, thisClass, kIsStaticMethod, &Void_Type);
            ICodeGenerator ccg(classContext, NULL, thisClass, kNoFlags, &Void_Type);
            ccg.allocateParameter(mContext->getWorld().identifiers["this"], false, thisClass);
            thisClass->defineStatic(mInitName, &Function_Type);

            mContext->getGlobalObject()->defineVariable(className, &Type_Type, JSValue(thisClass));

            XMLNodeList &elements = node->children();
            for (XMLNodeList::const_iterator j = elements.begin(); j != elements.end(); j++) {
                XMLNode *element = *j;
                bool isConstructor = (element->name().compare(widenCString("constructor")) == 0);

                if (isConstructor || (element->name().compare(widenCString("method")) == 0)) {
                    String methodName;
                    node->getValue(widenCString("name"), methodName);
                    ICodeModule *icm = readFunction(element, thisClass);
                    if (icm) {
                        if (isConstructor) {
                            thisClass->defineConstructor(methodName);
                            scg.setStatic(thisClass, mContext->getWorld().identifiers[methodName], scg.newFunction(icm));
                        }
                        else
                            thisClass->defineMethod(methodName, new JSFunction(icm));
                    }
                }
                else {
                    if (element->name().compare(widenCString("field")) == 0) {
                        String fieldName;
                        String fieldType;

                        element->getValue(widenCString("name"), fieldName);
                        element->getValue(widenCString("type"), fieldType);
                        JSType *type = mContext->findType(mContext->getWorld().identifiers[fieldType]);

                        if (element->hasAttribute(widenCString("static")))
                            thisClass->defineStatic(fieldName, type);
                        else
                            thisClass->defineSlot(fieldName, type);
                    }
                }
            }
            scg.setStatic(thisClass, mInitName, scg.newFunction(ccg.complete())); 
            thisClass->complete();
        
            if (scg.getICode()->size()) {
                ICodeModule* clinit = scg.complete();
                classContext->interpret(clinit, JSValues());
                delete clinit;
            }
            delete classContext;
        }
        else {
            if (node->name().compare(widenCString("script")) == 0) {
                String &body = node->body();
                if (body.length()) {
                    std::string str(body.length(), char());
                    std::transform(body.begin(), body.end(), str.begin(), narrow);
                    ICodeParser icp(mContext);

                    stdOut << "(script) Calling ICodeParser with :\n" << str << "\n";

                    icp.parseSourceFromString(str);

                    result = new ICodeModule(icp.mInstructions, 
                                                        NULL,                   /* VariableList *variables */
                                                        NULL,                   /* ParameterList *parameters */
                                                        icp.mMaxRegister, 
                                                        NULL,                   /* InstructionMap *instructionMap */
                                                        &Void_Type, 
                                                        NotABanana);            /* exception register */
                }                        
            }
            else {
                if (node->name().compare(widenCString("function")) == 0) {
                    String functionName;
                    node->getValue(widenCString("name"), functionName);
                    ICodeModule *icm = readFunction(node, NULL);
                    mContext->getGlobalObject()->defineFunction(functionName, icm);
                }
                else {
                    if (node->name().compare(widenCString("instance")) == 0) {
                        // find the appropriate class and initialize the fields
                    }
                    else {
                        if (node->name().compare(widenCString("object")) == 0) {
                            // an object literal
                        }
                    }
                }
            }
        }

    }
    return result;
}
    

    
} // namespace ICG

} // namespace JavaScript
