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

#include "interpreter.h"
#include "jsclasses.h"
#include "world.h"
#include "parser.h"
#include "jsmath.h"

#include <assert.h>

namespace JavaScript {
namespace Interpreter {

// operand access macros.
#define op1(i) (i->o1())
#define op2(i) (i->o2())
#define op3(i) (i->o3())
#define op4(i) (i->o4())

// mnemonic names for operands.
#define dst(i)  op1(i)
#define src1(i) op2(i)
#define src2(i) op3(i)
#define ofs(i)  (i->getOffset())
#define val2(i)  op2(i)
#define val3(i)  op3(i)
#define val4(i)  op4(i)

using namespace ICG;
using namespace JSTypes;
using namespace JSClasses;
using namespace JSMathClass;
    
// These classes are private to the JS interpreter.

/**
* 
*/
struct Handler: public gc_base {
    Handler(Label *catchLabel, Label *finallyLabel)
        : catchTarget(catchLabel), finallyTarget(finallyLabel) {}
    Label *catchTarget;
    Label *finallyTarget;
};
typedef std::vector<Handler *> CatchStack;


/**
 * Represents the current function's invocation state.
 */
struct Activation : public gc_base {
    JSValues mRegisters;
    ICodeModule* mICode;
    CatchStack catchStack;
        
    Activation(ICodeModule* iCode, const JSValues& args)
        : mRegisters(iCode->itsMaxRegister + 1), mICode(iCode)
    {
        // copy arg list to initial registers.
        JSValues::iterator dest = mRegisters.begin();
        for (JSValues::const_iterator src = args.begin(), 
                 end = args.end(); src != end; ++src, ++dest) {
            *dest = *src;
        }
    }

    Activation(ICodeModule* iCode, Activation* caller, const JSValue thisArg,
                 const ArgumentList& list)
        : mRegisters(iCode->itsMaxRegister + 1), mICode(iCode)
    {
        // copy caller's parameter list to initial registers.
        JSValues::iterator dest = mRegisters.begin();
        *dest++ = thisArg;
        const JSValues& params = caller->mRegisters;
        for (ArgumentList::const_iterator src = list.begin(), 
                 end = list.end(); src != end; ++src, ++dest) {
            Register r = (*src).first.first;
            if (r != NotARegister)
                *dest = params[r];
            else
                *dest = JSValue(JSValue::uninitialized_tag);
        }
    }

    // calling a binary operator, no 'this'
    Activation(ICodeModule* iCode, const JSValue arg1, const JSValue arg2)
        : mRegisters(iCode->itsMaxRegister + 1), mICode(iCode)
    {
        mRegisters[1] = arg1;
        mRegisters[2] = arg2;
    }

    // calling a getter function, no arguments
    Activation(ICodeModule* iCode)
        : mRegisters(iCode->itsMaxRegister + 1), mICode(iCode)
    {
    }

    // calling a setter function, 1 argument
    Activation(ICodeModule* iCode, const JSValue arg)
        : mRegisters(iCode->itsMaxRegister + 1), mICode(iCode)
    {
        mRegisters[1] = arg;
    }

};

/**
 * exception-safe class to save off values.
 */
template <typename T>
class autosaver {
    T& mRef;
    T mOld;
public:
    autosaver(T& ref, T val = T()) : mRef(ref), mOld(ref) { ref = val; }
    ~autosaver() { mRef = mOld; }
};

ICodeModule* Context::compileFunction(const String &source)
{
    Arena a;
    String filename = widenCString("Some source source");
    Parser p(getWorld(), a, source, filename);
    ExprNode* e = p.parseExpression(false);
    ICodeGenerator icg(&getWorld(), getGlobalObject());
    ASSERT(e->getKind() == ExprNode::functionLiteral);
    FunctionExprNode* f = static_cast<FunctionExprNode*>(e);
    icg.allocateParameter(getWorld().identifiers["this"]);   // always parameter #0
    VariableBinding* v = f->function.parameters;
    while (v) {
        if (v->name && (v->name->getKind() == ExprNode::identifier))
            icg.allocateParameter((static_cast<IdentifierExprNode*>(v->name))->name);
        v = v->next;
    }
    icg.genStmt(f->function.body);
    ICodeModule* result = icg.complete();
    result->setFileName(filename);
    return result;
}

JSValue Context::readEvalFile(FILE* in, const String& fileName)
{
    String buffer;
    string line;
    LineReader inReader(in);
    JSValues emptyArgs;
    JSValue result;
        
    // save off important member variables, to enable recursive call to interpret.
    // this is a little stinky, but should be exception-safe.
    autosaver<Activation*> activation(mActivation, 0);
    autosaver<Linkage*> linkage(mLinkage, 0);
    autosaver<InstructionIterator> pc(mPC);

    while (inReader.readLine(line) != 0) {
        appendChars(buffer, line.data(), line.size());
        try {
            Arena a;
            Parser p(getWorld(), a, buffer, fileName);
            StmtNode *parsedStatements = p.parseProgram();
			ASSERT(p.lexer.peek(true).hasKind(Token::end));
            {
            	PrettyPrinter f(stdOut, 30);
            	{
            		PrettyPrinter::Block b(f, 2);
                	f << "Program =";
                	f.linearBreak(1);
                	StmtNode::printStatements(f, parsedStatements);
            	}
            	f.end();
            }
    	    stdOut << '\n';

	    // Generate code for parsedStatements, which is a linked 
            // list of zero or more statements
            ICodeModule* icm = genCode(parsedStatements, fileName);
            if (icm) {
                result = interpret(icm, emptyArgs);
                delete icm;
            }

            clear(buffer);
        } catch (Exception &e) {
            /* If we got a syntax error on the end of input,
             * then wait for a continuation
             * of input rather than printing the error message. */
            if (!(e.hasKind(Exception::syntaxError) &&
                  e.lineNum && e.pos == buffer.size() &&
                  e.sourceFile == fileName)) {
                stdOut << '\n' << e.fullMessage();
                clear(buffer);
            }
        }
    }
    return result;
}

ICodeModule* Context::genCode(StmtNode *p, const String &fileName)
{
    ICodeGenerator icg(&getWorld(), getGlobalObject());
    
    TypedRegister ret(NotARegister, &None_Type);
    while (p) {
        ret = icg.genStmt(p);
        p = p->next;
    }
    icg.returnStmt(ret);

    ICodeModule *icm = icg.complete();
    icm->setFileName (fileName);
    return icm;
}


JSValues& Context::getRegisters()    { return mActivation->mRegisters; }
ICodeModule* Context::getICode()     { return mActivation->mICode; }

/**
 * Stores saved state from the *previous* activation, the current
 * activation is alive and well in locals of the interpreter loop.
 */
struct Linkage : public Context::Frame, public gc_base {
    Linkage*            mNext;              // next linkage in linkage stack.
    InstructionIterator mReturnPC;
    Activation*         mActivation;        // caller's activation.
    JSScope*            mScope;
    TypedRegister       mResult;            // the desired target register for the return value

    Linkage(Linkage* linkage, InstructionIterator returnPC,
            Activation* activation, JSScope* scope, TypedRegister result) 
        :   mNext(linkage), mReturnPC(returnPC),
            mActivation(activation), mScope(scope), mResult(result)
    {
    }
    
    Context::Frame* getNext() { return mNext; }
    
    void getState(InstructionIterator& pc, JSValues*& registers, ICodeModule*& iCode)
    {
        pc = mReturnPC;
        registers = &mActivation->mRegisters;
        iCode = mActivation->mICode;
    }
};

static JSValue shiftLeft_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toUInt32());
    return JSValue(num1.i32 << (num2.u32 & 0x1F));
}
static JSValue shiftRight_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toUInt32());
    return JSValue(num1.i32 >> (num2.u32 & 0x1F));
}
static JSValue UshiftRight_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toUInt32());
    JSValue num2(r2.toUInt32());
    return JSValue(num1.u32 >> (num2.u32 & 0x1F));
}
static JSValue and_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toInt32());
    return JSValue(num1.i32 & num2.i32);
}
static JSValue or_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toInt32());
    return JSValue(num1.i32 | num2.i32);
}
static JSValue xor_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toInt32());
    JSValue num2(r2.toInt32());
    return JSValue(num1.i32 ^ num2.i32);
}
static JSValue add_Default(const JSValue& r1, const JSValue& r2)
{
    //
    // could also handle these as separate entries in the override table for add
    // by specifying add(String, Any), add(Any, String), add(String, String)
    //
    if (r1.isString() || r2.isString()) {
        JSString& str1 = *r1.toString().string;
        JSString& str2 = *r2.toString().string;
        return JSValue(new JSString(str1 + str2));
    }
    else {
        JSValue num1(r1.toNumber());
        JSValue num2(r2.toNumber());
        return JSValue(num1.f64 + num2.f64);
    }
}

/*
static JSValue add_String1(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(num1.f64 + num2.f64);
}
*/

static JSValue subtract_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(num1.f64 - num2.f64);
}
static JSValue multiply_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(num1.f64 * num2.f64);
}
static JSValue divide_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(num1.f64 / num2.f64);
}
static JSValue remainder_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue num1(r1.toNumber());
    JSValue num2(r2.toNumber());
    return JSValue(fmod(num1.f64, num2.f64));
}
static JSValue less_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue lv = r1.toPrimitive(JSValue::Number);
    JSValue rv = r2.toPrimitive(JSValue::Number);
    if (lv.isString() && rv.isString()) {
        return JSValue(bool(lv.string->compare(*rv.string) < 0));
    }
    else {
        lv = lv.toNumber();
        rv = rv.toNumber();
        if (lv.isNaN() || rv.isNaN())
            return JSValue();
        else
            return JSValue(lv.f64 < rv.f64);
    }
}
static JSValue lessOrEqual_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue lv = r1.toPrimitive(JSValue::Number);
    JSValue rv = r2.toPrimitive(JSValue::Number);
    if (lv.isString() && rv.isString()) {
        return JSValue(bool(lv.string->compare(*rv.string) <= 0));
    }
    else {
        lv = lv.toNumber();
        rv = rv.toNumber();
        if (lv.isNaN() || rv.isNaN())
            return JSValue();
        else
            return JSValue(lv.f64 <= rv.f64);
    }
}
static JSValue equal_Default(const JSValue& r1, const JSValue& r2)
{
    JSValue lv = r1.toPrimitive(JSValue::Number);
    JSValue rv = r2.toPrimitive(JSValue::Number);
    if (lv.isString() && rv.isString()) {
        return JSValue(bool(lv.string->compare(*rv.string) == 0));
    }
    else {
        lv = lv.toNumber();
        rv = rv.toNumber();
        if (lv.isNaN() || rv.isNaN())
            return JSValue();
        else
            return JSValue(lv.f64 == rv.f64);
    }
}
static JSValue identical_Default(const JSValue& r1, const JSValue& r2)
{
    if (r1.getType() != r2.getType())
        return kFalseValue;
    if (r1.isUndefined() )
        return kTrueValue;
    if (r1.isNull())
        return kTrueValue;
    if (r1.isNumber()) {
        if (r1.isNaN())
            return kFalseValue;
        if (r2.isNaN())
            return kFalseValue;
        return JSValue(r1.f64 == r2.f64);
    }
    else {
        if (r1.isString())
            return JSValue(bool(r1.string->compare(*r2.string) == 0));
        if (r1.isBoolean())
            return JSValue(bool(r1.boolean == r2.boolean));
        if (r1.isObject())
            return JSValue(bool(r1.object == r2.object));
        return kFalseValue;
    }
}


static JSValue defineAdd(Context *cx, const JSValues& argv)
{
    // should be three args, first two are types, third is a function.
    ASSERT(argv[0].isType());
    ASSERT(argv[1].isType());
    ASSERT(argv[2].isFunction());

// XXX need to prove that argv[2].function takes T1 and T2 as args and returns Boolean for the relational operators ?

    cx->addBinaryOperator(BinaryOperator::Add, new BinaryOperator(argv[0].type, argv[1].type, argv[2].function));

    return kUndefinedValue;
}

#define DEFINE_OBO(NAME)                                                            \
    static JSValue define##NAME(Context *cx, const JSValues& argv)                  \
    {                                                                               \
        ASSERT(argv[0].isType());                                                   \
        ASSERT(argv[1].isType());                                                   \
        ASSERT(argv[2].isFunction());                                               \
        cx->addBinaryOperator(BinaryOperator::##NAME,                               \
            new BinaryOperator(argv[0].type, argv[1].type, argv[2].function));      \
        return kUndefinedValue;                                                     \
    }                                                                               \

DEFINE_OBO(Subtract)
DEFINE_OBO(Multiply)
DEFINE_OBO(Divide)
DEFINE_OBO(Remainder)
DEFINE_OBO(LeftShift)
DEFINE_OBO(RightShift)
DEFINE_OBO(LogicalRightShift)
DEFINE_OBO(BitwiseOr)
DEFINE_OBO(BitwiseXor)
DEFINE_OBO(BitwiseAnd)
DEFINE_OBO(Less)
DEFINE_OBO(LessOrEqual)
DEFINE_OBO(Equal)
DEFINE_OBO(Identical)

void Context::initOperatorsPackage()
{
// hack - the following should be available only after importing the 'Operators' package
// (hmm, how will that work - the import needs to connect the functions into this mechanism
//   do we watch for the specific package name???)

    struct OBO {
        char *name;
        JSNativeFunction::JSCode fun;
    } OBOs[] = {
        { "defineAdd",              defineAdd },
        { "defineSubtract",         defineSubtract },
        { "defineMultiply",         defineMultiply },
        { "defineDivide",           defineDivide },
        { "defineRemainder",        defineRemainder },
        { "defineLeftShift",        defineLeftShift },
        { "defineRightShift",       defineRightShift },
        { "defineLogicalRightShift",defineLogicalRightShift },
        { "defineBitwiseOr",        defineBitwiseOr },
        { "defineBitwiseXor",       defineBitwiseXor },
        { "defineBitwiseAnd",       defineBitwiseAnd },
        { "defineLess",             defineLess },
        { "defineLessOrEqual",      defineLessOrEqual },
        { "defineEqual",            defineEqual },
        { "defineIdentical",        defineIdentical },
    };

    for (uint i = 0; i < sizeof(OBOs) / sizeof(struct OBO); i++)
        mGlobal->defineNativeFunction(mWorld.identifiers[widenCString(OBOs[i].name)], OBOs[i].fun);

    mHasOperatorsPackageLoaded = true;
}

void Context::initContext()
{
// if global has a parent, assume it's been initialized already.
    if (mGlobal->getParent())
        return;

// predefine the umm, predefined types;
    struct PDT {
        char *name;
        JSType *type;
    } PDTs[] = {
        { "any", &Any_Type },
        { "Integer", &Integer_Type },
        { "Number", &Number_Type },
        { "Character", &Character_Type },
        { "String", &String_Type },
        { "Function", &Function_Type },
        { "Array", &Array_Type },
        { "Type", &Type_Type },
        { "Boolean", &Boolean_Type },
        { "Null", &Null_Type },
        { "Void", &Void_Type },
        { "none", &None_Type }
    };

    for (uint i = 0; i < sizeof(PDTs) / sizeof(struct PDT); i++)
        mGlobal->defineVariable(widenCString(PDTs[i].name), &Type_Type, JSValue(PDTs[i].type));

    // set up the correct [[Class]] for the global object (matching SpiderMonkey)
    mGlobal->setClass(new JSString("global"));


    // add (XXX some) of the global object properties
    mGlobal->setProperty(widenCString("NaN"), kNaNValue);
    mGlobal->setProperty(widenCString("undefined"), kUndefinedValue);

    
    // 'Object', 'Date', 'RegExp', 'Array' etc are all (constructor) properties of the global object
    // Some of these overlap with the predefined types above. The way we handle this is to set a 
    // 'constructor' function for types. When new <typename> is encountered, the type is queried for
    // a 'constructor' function to be invoked.
    JSObject::initObjectObject(mGlobal);
    
    JSFunction::initFunctionObject(mGlobal);
    JSBoolean::initBooleanObject(mGlobal);
    
    // the 'Math' object just has some useful properties 
    JSMath::initMathObject(mGlobal);


    // This initializes the state of the binary operator overload mechanism.
    // One could argue that it is unneccessary to do this until the 'Operators'
    // package is loaded and that all (un-typed) binary operators should use a 
    // form of icode that performed the inline operation instead.
    JSBinaryOperator::JSBinaryCode defaultFunction[] = {
        add_Default,
        subtract_Default,
        multiply_Default,
        divide_Default,
        remainder_Default,
        shiftLeft_Default,
        shiftRight_Default,
        UshiftRight_Default,
        or_Default,
        xor_Default,
        and_Default,
        less_Default,
        lessOrEqual_Default,
        equal_Default,
        identical_Default
    };

    for (BinaryOperator::BinaryOp b = BinaryOperator::BinaryOperatorFirst; 
         b < BinaryOperator::BinaryOperatorCount; 
         b = (BinaryOperator::BinaryOp)(b + 1) )            
        addBinaryOperator(b, new BinaryOperator(&Any_Type, &Any_Type, new JSBinaryOperator(defaultFunction[b])));

}

const JSValue Context::findBinaryOverride(JSValue &operand1, JSValue &operand2, BinaryOperator::BinaryOp op)
{
    int32 bestDist1 = JSType::NoRelation;
    int32 bestDist2 = JSType::NoRelation;
    BinaryOperatorList::iterator candidate = NULL;

    for (BinaryOperatorList::iterator i = mBinaryOperators[op].begin();
            i != mBinaryOperators[op].end(); i++) 
    {
        int32 dist1 = operand1.getType()->distance((*i)->t1);
        int32 dist2 = operand2.getType()->distance((*i)->t2);

        if ((dist1 < bestDist1) && (dist2 < bestDist2)) {
            bestDist1 = dist1;
            bestDist2 = dist2;
            candidate = i;
        }
    }
    ASSERT(candidate);
    return JSValue((*candidate)->function);
}


bool Context::hasNamedArguments(ArgumentList &args)
{
    Argument* e = args.end();
    for (ArgumentList::iterator r = args.begin(); r != e; r++) {
        if ((*r).second) return true;
    }
    return false;
}

JSValue Context::interpret(ICodeModule* iCode, const JSValues& args)
{
    assert(mActivation == 0); /* recursion == bad */
    
    JSValue rv;
    
    // when invoked with empty args, make sure that 'this' is 
    // going to be the global object.

    mActivation = new Activation(iCode, args);
    JSValues* registers = &mActivation->mRegisters;
    if (args.size() == 0) (*registers)[0] = mGlobal;

    mPC = mActivation->mICode->its_iCode->begin();
    InstructionIterator endPC = mActivation->mICode->its_iCode->end();

    // stack of all catch/finally handlers available for the current activation
    // to implement jsr/rts for finally code
    std::stack<InstructionIterator> subroutineStack;

    while (true) {
        try {
            // tell any listeners about the current execution state.
            // XXX should only do this if we're single stepping/tracing.
            if (mListeners.size())
                broadcast(EV_STEP);

            assert(mPC != endPC);
            Instruction* instruction = *mPC;
            switch (instruction->op()) {
            case CAST:
                {
                    Cast* c = static_cast<Cast*>(instruction);
                    JSType *toType = op3(c);
                    (*registers)[dst(c).first] = (*registers)[src1(c).first].convert(toType);
                }
                break;
            case SUPER:
                {
                    Super* su = static_cast<Super*>(instruction);
                    ASSERT((*registers)[0].isObject());         // should be scope of current class
                    JSScope *s = static_cast<JSScope *>((*registers)[0].object->getPrototype());
                    (*registers)[dst(su).first] = s;
                }
                break;

            case GET_METHOD:
                {
                    GetMethod* gm = static_cast<GetMethod*>(instruction);
                    JSValue base = (*registers)[src1(gm).first];
                    ASSERT(base.isObject());        // XXX runtime error
                    JSClass *theClass = dynamic_cast<JSClass*>(base.object->getType());
                    ASSERT(theClass);
                    (*registers)[dst(gm).first] = theClass->getMethod(src2(gm));
                }
                break;

            case CALL:
                {
                    Call* call = static_cast<Call*>(instruction);
                    JSValue v = (*registers)[op2(call).first];
                    JSFunction *target = NULL;
                    if (v.isFunction())
                        target = v.function;
                    else
                        if (v.isObject()) {
                            JSType *t = dynamic_cast<JSType*>(v.object);
                            if (t)
                                target = t->getInvokor();
                        }
                    if (!target)
                        throw new JSException("Call to non callable object");
                    if (target->isNative()) {
                        ArgumentList &params = op4(call);
                        JSValues argv(params.size() + 1);
                        argv[0] = (*registers)[op3(call).first];
                        JSValues::size_type i = 1;
                        for (ArgumentList::const_iterator src = params.begin(), end = params.end();
                                        src != end; ++src, ++i) {
                            argv[i] = (*registers)[src->first.first];
                        }
                        JSValue result = static_cast<JSNativeFunction*>(target)->mCode(this, argv);
                        if (op1(call).first != NotARegister)
                            (*registers)[op1(call).first] = result;
                        break;
                    }
                    else {
                        ICodeModule *icm = target->getICode();
                        ArgumentList &args = op4(call);

                        // mParameterCount includes 'this' and also 1 for a named rest parameter
                        //
                        uint32 pCount = icm->mParameterCount - 1;
                        ArgumentList callArgs(pCount, Argument(TypedRegister(NotARegister, &Null_Type), NULL));
                        if (icm->mHasNamedRestParameter) pCount--;


                        // walk along the given arguments, handling each.
                        // might be good to optimize the case of calls without named arguments and/or rest parameters
                        JSArray *restArg = NULL;
                        uint32 restIndex = 0;
                        uint32 i;
                        for (i = 0; i < args.size(); i++) {
                            if (args[i].second) {       // a named argument
                                TypedRegister r = icm->itsVariables->findVariable(*(args[i].second));
                                bool isParameter = false;
                                if (r.first != NotABanana) {   // we found the name in the target's list of variables
                                    if (r.first < icm->mParameterCount) { // make sure we didn't match a local var                                            
                                        ASSERT(r.first <= callArgs.size());
                                        // the named argument is arriving in slot i, but needs to be r instead
                                        // r.first is the intended target register, we subtract 1 since the callArgs array doesn't include 'this'
                                        // here's where we could detect over-writing a positional arg with a named one if that is illegal
                                        // if (callArgs[r.first - 1].first.first != NotARegister)...
                                        (*registers)[args[i].first.first] = (*registers)[args[i].first.first].convert(r.second);
                                        callArgs[r.first - 1] = Argument(args[i].first, NULL);   // no need to copy the name through?
                                        isParameter = true;
                                    }
                                }
                                if (!isParameter) {     // wasn't a parameter, make it a property of the rest parameter (if there is one) 
                                    if (icm->mHasRestParameter) {
                                        if (icm->mHasNamedRestParameter) {
                                            if (restArg == NULL) {
                                                restArg = new JSArray();
                                                restArg->setProperty(*args[i].second, (*registers)[args[i].first.first]);
                                                (*registers)[args[i].first.first] = restArg;
                                                callArgs[pCount] = Argument(TypedRegister(args[i].first.first, &Array_Type), NULL);
                                            }
                                            else
                                                restArg->setProperty(*args[i].second, (*registers)[args[i].first.first]);

                                        }
                                        // else just throw it away 
                                    }
                                    else 
                                        throw new JSException("Named argument doesn't match parameter name in call with no rest parameter");
                                        // what about matching the named rest parameter ? aaarrggh!
                                }
                            }
                            else {
                                if (i >= pCount) {  // more args than expected
                                    if (icm->mHasRestParameter) {
                                        if (icm->mHasNamedRestParameter) {
                                            if (restArg == NULL) {
                                                restArg = new JSArray();
                                                (*restArg)[restIndex++] = (*registers)[args[i].first.first];
                                                (*registers)[args[i].first.first] = restArg;
                                                callArgs[pCount] = Argument(TypedRegister(args[i].first.first, &Array_Type), NULL);
                                            }
                                            else
                                                (*restArg)[restIndex++] = (*registers)[args[i].first.first];
                                        }
                                        // else just throw it away 
                                    }
                                    else
                                        throw new JSException("Too many arguments in call");
                                }
                                else {
                                    TypedRegister r = icm->itsVariables->getRegister(i + 1);    // the variable list includes 'this'
                                    (*registers)[args[i].first.first] = (*registers)[args[i].first.first].convert(r.second);
                                    callArgs[i] = args[i];  // it's a positional, just slap it in place
                                }
                            }
                        }
                        uint32 contiguousArgs = 0;
                        for (i = 0; i < args.size(); i++) {
                            Argument &arg = args[i];
                            if (arg.first.first == NotARegister) break;
                            contiguousArgs++;
                        }
                        if ((contiguousArgs + 1) < icm->mNonOptionalParameterCount) // there's always a 'this' in R0 (even though it might be null)
                            throw new JSException("Too few arguments in call");
                        
                        mLinkage = new Linkage(mLinkage, ++mPC, mActivation, mGlobal, op1(call));
                        mActivation = new Activation(icm, mActivation, (*registers)[op3(call).first], callArgs);
                        registers = &mActivation->mRegisters;
                        mPC = mActivation->mICode->its_iCode->begin();
                        endPC = mActivation->mICode->its_iCode->end();
                        continue;
                    }
                }

            case DIRECT_CALL:
                {
                    DirectCall* call = static_cast<DirectCall*>(instruction);
                    JSFunction *target = op2(call);
                    if (target->isNative()) {
                        ArgumentList &params = op3(call);
                        JSValues argv(params.size() + 1);
                        JSValues::size_type i = 1;
                        for (ArgumentList::const_iterator src = params.begin(), end = params.end();
                                        src != end; ++src, ++i) {
                            argv[i] = (*registers)[src->first.first];
                        }
                        JSValue result = static_cast<JSNativeFunction*>(target)->mCode(this, argv);
                        if (op1(call).first != NotARegister)
                            (*registers)[op1(call).first] = result;
                        break;
                    }
                    else {
                        mLinkage = new Linkage(mLinkage, ++mPC,
                                               mActivation, mGlobal, op1(call));
                        mActivation = new Activation(target->getICode(), mActivation, kNullValue, op3(call));
                        registers = &mActivation->mRegisters;
                        mPC = mActivation->mICode->its_iCode->begin();
                        endPC = mActivation->mICode->its_iCode->end();
                        continue;
                    }
                }

            case RETURN_VOID:
                {
                    Linkage *linkage = mLinkage;
                    if (!linkage)
                    {
                        // let the garbage collector free activations.
                        mActivation = 0;
                        return kUndefinedValue;
                    }
                    mLinkage = linkage->mNext;
                    mActivation = linkage->mActivation;
                    mGlobal = linkage->mScope;
                    registers = &mActivation->mRegisters;
                    if (linkage->mResult.first != NotARegister)
                        (*registers)[linkage->mResult.first] = kUndefinedValue;
                    mPC = linkage->mReturnPC;
                    endPC = mActivation->mICode->its_iCode->end();
                }
                continue;

            case RETURN:
                {
                    Return* ret = static_cast<Return*>(instruction);
                    JSValue result;
                    if (op1(ret).first != NotARegister) 
                        result = (*registers)[op1(ret).first];
                    Linkage* linkage = mLinkage;
                    if (!linkage)
                    {
                        // let the garbage collector free activations.
                        mActivation = 0;
                        return result;
                    }
                    mLinkage = linkage->mNext;
                    mActivation = linkage->mActivation;
                    mGlobal = linkage->mScope;
                    registers = &mActivation->mRegisters;
                    if (linkage->mResult.first != NotARegister)
                        (*registers)[linkage->mResult.first] = result;
                    mPC = linkage->mReturnPC;
                    endPC = mActivation->mICode->its_iCode->end();
                }
                continue;
            case MOVE:
                {
                    Move* mov = static_cast<Move*>(instruction);
                    (*registers)[dst(mov).first] = (*registers)[src1(mov).first];
                }
                break;
            case LOAD_NAME:
                {
                    LoadName* ln = static_cast<LoadName*>(instruction);                    
                    JSFunction *getter = mGlobal->getter(*src1(ln));
                    if (getter) {
                        ASSERT(!getter->isNative());
                        mLinkage = new Linkage(mLinkage, ++mPC, mActivation, mGlobal, dst(ln));
                        mActivation = new Activation(getter->getICode());
                        registers = &mActivation->mRegisters;
                        mPC = mActivation->mICode->its_iCode->begin();
                        endPC = mActivation->mICode->its_iCode->end();
                        continue;
                    }
                    else
                        (*registers)[dst(ln).first] = mGlobal->getVariable(*src1(ln));
                }
                break;
            case SAVE_NAME:
                {
                    SaveName* sn = static_cast<SaveName*>(instruction);
                    JSFunction *setter = mGlobal->setter(*dst(sn));
                    if (setter) {
                        ASSERT(!setter->isNative());
                        mLinkage = new Linkage(mLinkage, ++mPC, mActivation, mGlobal, TypedRegister(NotARegister, &Null_Type));
                        mActivation = new Activation(setter->getICode(), (*registers)[src1(sn).first]);
                        registers = &mActivation->mRegisters;
                        mPC = mActivation->mICode->its_iCode->begin();
                        endPC = mActivation->mICode->its_iCode->end();
                        continue;
                    }
                    else
                        mGlobal->setVariable(*dst(sn), (*registers)[src1(sn).first]);
                }
                break;
            case NEW_OBJECT:
                {
                    NewObject* no = static_cast<NewObject*>(instruction);
                    if (src1(no).first != NotARegister)
                        (*registers)[dst(no).first] = new JSObject((*registers)[src1(no).first]);
                    else
                        (*registers)[dst(no).first] = new JSObject();
                }
                break;
            case NEW_CLASS:
                {
                    NewClass* nc = static_cast<NewClass*>(instruction);
                    JSClass* thisClass = src1(nc);
                    JSInstance* thisInstance = new(thisClass) JSInstance(thisClass);
                    (*registers)[dst(nc).first] = thisInstance;
                }
                break;
            case NEW_FUNCTION:
                {
                    NewFunction* nf = static_cast<NewFunction*>(instruction);
                    (*registers)[dst(nf).first] = new JSFunction(src1(nf));
                }
                break;
            case NEW_ARRAY:
                {
                    NewArray* na = static_cast<NewArray*>(instruction);
                    (*registers)[dst(na).first] = new JSArray();
                }
                break;
            case DELETE_PROP:
                {
                    DeleteProp* dp = static_cast<DeleteProp*>(instruction);
                    JSValue& value = (*registers)[src1(dp).first];
                    if (value.isObject() && !value.isType()) {
                        (*registers)[dst(dp).first] = value.object->deleteProperty(*src2(dp));
                    }
                }
                break;
            case GET_PROP:
                {
                    GetProp* gp = static_cast<GetProp*>(instruction);
                    JSValue& value = (*registers)[src1(gp).first];
                    if (value.isObject()) {
                        if (value.isType()) {
                            // REVISIT: should signal error if slot doesn't exist.
                            NOT_REACHED("tell me I'm wrong");
                            JSClass* thisClass = dynamic_cast<JSClass*>(value.type);
                            if (thisClass && thisClass->hasStatic(*src2(gp))) {
                                const JSSlot& slot = thisClass->getStatic(*src2(gp));
                                (*registers)[dst(gp).first] = (*thisClass)[slot.mIndex];
                            }
                            else
                                (*registers)[dst(gp).first] = value.object->getProperty(*src2(gp));
                        } else {
                            (*registers)[dst(gp).first] = value.object->getProperty(*src2(gp));
                        }
                    }
                    // XXX runtime error
                }
                break;
            case SET_PROP:
                {
                    SetProp* sp = static_cast<SetProp*>(instruction);
                    JSValue& value = (*registers)[dst(sp).first];
                    if (value.isObject()) {
                        if (value.isType()) {
                            // REVISIT: should signal error if slot doesn't exist.
                            NOT_REACHED("tell me I'm wrong");
                            JSClass* thisClass = dynamic_cast<JSClass*>(value.object);
                            if (thisClass && thisClass->hasStatic(*src1(sp))) {
                                const JSSlot& slot = thisClass->getStatic(*src1(sp));
                                (*thisClass)[slot.mIndex] = (*registers)[src2(sp).first];
                            }
                            else
                                value.object->setProperty(*src1(sp), (*registers)[src2(sp).first]);
                        } else {
                            value.object->setProperty(*src1(sp), (*registers)[src2(sp).first]);
                        }
                    }
                }
                break;
            case GET_STATIC:
                {
                    GetStatic* gs = static_cast<GetStatic*>(instruction);
                    JSClass* c = src1(gs);
                    (*registers)[dst(gs).first] = (*c)[src2(gs)];
                }
                break;
            case SET_STATIC:
                {
                    SetStatic* ss = static_cast<SetStatic*>(instruction);
                    JSClass* c = dst(ss);
                    (*c)[src1(ss)] = (*registers)[src2(ss).first];
                }
                break;
/*
In 1.5, there is no 'array' type really, the index operation
turns into a get_property call like this, only we need to be
using JSString throughout.
            case GET_ELEMENT:
                {
                    GetElement* ge = static_cast<GetElement*>(instruction);
                    JSValue& base = (*registers)[src1(ge)];
                    JSValue index = (*registers)[src2(ge)].toString();
                    if (base.tag == JSValue::object_tag) {
                        JSObject* object = base.object;
                        (*registers)[dst(ge)] = object->getProperty(*index.string);
                    }
                }
                break;
*/
            case GET_ELEMENT:
                {
                    GetElement* ge = static_cast<GetElement*>(instruction);
                    JSValue& value = (*registers)[src1(ge).first];
                    if (value.tag == JSValue::array_tag) {
                        JSArray* array = value.array;
                        (*registers)[dst(ge).first] = (*array)[(*registers)[src2(ge).first]];
                    }
                }
                break;
            case SET_ELEMENT:
                {
                    SetElement* se = static_cast<SetElement*>(instruction);
                    JSValue& value = (*registers)[dst(se).first];
                    if (value.tag == JSValue::array_tag) {
                        JSArray* array = value.array;
                        (*array)[(*registers)[src1(se).first]] = (*registers)[src2(se).first];
                    }
                }
                break;

            case GET_SLOT:
                {
                    GetSlot* gs = static_cast<GetSlot*>(instruction);
                    JSValue& value = (*registers)[src1(gs).first];
                    if (value.isObject()) {
                        JSInstance* inst = static_cast<JSInstance *>(value.object);
                        (*registers)[dst(gs).first] = (*inst)[src2(gs)];
                    }
                    // XXX runtime error
                }
                break;
            case SET_SLOT:
                {
                    SetSlot* ss = static_cast<SetSlot*>(instruction);
                    JSValue& value = (*registers)[dst(ss).first];
                    if (value.isObject()) {
                        JSInstance* inst = static_cast<JSInstance *>(value.object);
                        (*inst)[src1(ss)] = (*registers)[src2(ss).first];
                    }
                }
                break;

            case LOAD_IMMEDIATE:
                {
                    LoadImmediate* li = static_cast<LoadImmediate*>(instruction);
                    (*registers)[dst(li).first] = src1(li);
                }
                break;
            case LOAD_STRING:
                {
                    LoadString* ls = static_cast<LoadString*>(instruction);
                    (*registers)[dst(ls).first] = src1(ls);
                }
                break;
            case LOAD_BOOLEAN:
                {
                    LoadBoolean* lb = static_cast<LoadBoolean*>(instruction);
                    (*registers)[dst(lb).first] = src1(lb);
                }
                break;
            case BRANCH:
                {
                    GenericBranch* bra =
                        static_cast<GenericBranch*>(instruction);
                    mPC = mActivation->mICode->its_iCode->begin() + ofs(bra);
                    continue;
                }
                break;
            case BRANCH_TRUE:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    ASSERT((*registers)[src1(bc).first].isBoolean());
                    if ((*registers)[src1(bc).first].boolean) {
                        mPC = mActivation->mICode->its_iCode->begin() + ofs(bc);
                        continue;
                    }
                }
                break;
            case BRANCH_FALSE:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    ASSERT((*registers)[src1(bc).first].isBoolean());
                    if (!(*registers)[src1(bc).first].boolean) {
                        mPC = mActivation->mICode->its_iCode->begin() + ofs(bc);
                        continue;
                    }
                }
                break;
            case BRANCH_INITIALIZED:
                {
                    GenericBranch* bc =
                        static_cast<GenericBranch*>(instruction);
                    if ((*registers)[src1(bc).first].isInitialized()) {
                        mPC = mActivation->mICode->its_iCode->begin() + ofs(bc);
                        continue;
                    }
                }
                break;
            case GENERIC_BINARY_OP: 
                {
                    GenericBinaryOP* gbo = static_cast<GenericBinaryOP*>(instruction);
                    JSValue& dest = (*registers)[dst(gbo).first];
                    JSValue& r1 = (*registers)[val3(gbo).first];
                    JSValue& r2 = (*registers)[val4(gbo).first];
                    const JSValue ovr = findBinaryOverride(r1, r2, val2(gbo));
                    JSFunction *target = ovr.function;
                    if (target->isNative()) {
                        JSValues argv(2);
                        argv[0] = r1;
                        argv[1] = r2;
                        dest = static_cast<JSBinaryOperator*>(target)->mCode(r1, r2);
                        break;
                    }
                    else {
                        mLinkage = new Linkage(mLinkage, ++mPC,
                                               mActivation, mGlobal, dst(gbo));
                        mActivation = new Activation(target->getICode(), r1, r2);
                        registers = &mActivation->mRegisters;
                        mPC = mActivation->mICode->its_iCode->begin();
                        endPC = mActivation->mICode->its_iCode->end();
                        continue;
                    }
                }
                break;

            case SHIFTLEFT:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    JSValue num1(r1.toInt32());
                    JSValue num2(r2.toUInt32());
                    dest = JSValue(num1.i32 << (num2.u32 & 0x1F));
                }
                break;
            case SHIFTRIGHT:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    JSValue num1(r1.toInt32());
                    JSValue num2(r2.toUInt32());
                    dest = JSValue(num1.i32 >> (num2.u32 & 0x1F));
                }
                break;
            case USHIFTRIGHT:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    JSValue num1(r1.toUInt32());
                    JSValue num2(r2.toUInt32());
                    dest = JSValue(num1.u32 >> (num2.u32 & 0x1F));
                }
                break;
            case AND:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    JSValue num1(r1.toInt32());
                    JSValue num2(r2.toInt32());
                    dest = JSValue(num1.i32 & num2.i32);
                }
                break;
            case OR:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    JSValue num1(r1.toInt32());
                    JSValue num2(r2.toInt32());
                    dest = JSValue(num1.i32 | num2.i32);
                }
                break;
            case XOR:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    JSValue num1(r1.toInt32());
                    JSValue num2(r2.toInt32());
                    dest = JSValue(num1.i32 ^ num2.i32);
                }
                break;
            case ADD:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    ASSERT(r1.isNumber());
                    ASSERT(r2.isNumber());
                    dest = JSValue(r1.f64 + r2.f64);
                }
                break;
            case SUBTRACT:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    ASSERT(r1.isNumber());
                    ASSERT(r2.isNumber());
                    dest = JSValue(r1.f64 - r2.f64);
                }
                break;
            case MULTIPLY:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    ASSERT(r1.isNumber());
                    ASSERT(r2.isNumber());
                    dest = JSValue(r1.f64 * r2.f64);
                }
                break;
            case DIVIDE:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    ASSERT(r1.isNumber());
                    ASSERT(r2.isNumber());
                    dest = JSValue(r1.f64 / r2.f64);
                }
                break;
            case REMAINDER:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    ASSERT(r1.isNumber());
                    ASSERT(r2.isNumber());
                    dest = JSValue(fmod(r1.f64, r2.f64));
                }
                break;
            case COMPARE_LT:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    ASSERT(r1.isNumber());
                    ASSERT(r2.isNumber());
                    if (r1.isNaN() || r2.isNaN())
                        dest = JSValue();
                    else
                        dest = JSValue(r1.f64 < r2.f64);
                }
                break;
            case COMPARE_LE:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    ASSERT(r1.isNumber());
                    ASSERT(r2.isNumber());
                    if (r1.isNaN() || r2.isNaN())
                        dest = JSValue();
                    else
                        dest = JSValue(r1.f64 <= r2.f64);
                }
                break;
            case COMPARE_EQ:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    ASSERT(r1.isNumber());
                    ASSERT(r2.isNumber());
                    if (r1.isNaN() || r2.isNaN())
                        dest = JSValue();
                    else
                        dest = JSValue(r1.f64 == r2.f64);
                }
                break;
            case STRICT_EQ:
                {
                    Arithmetic* a = static_cast<Arithmetic*>(instruction);
                    JSValue& dest = (*registers)[dst(a).first];
                    JSValue& r1 = (*registers)[src1(a).first];
                    JSValue& r2 = (*registers)[src2(a).first];
                    ASSERT(r1.isNumber());
                    ASSERT(r2.isNumber());
                    if (r1.isNaN() || r2.isNaN())
                        dest = kFalseValue;
                    else
                        dest = JSValue(r1.f64 == r2.f64);
                }
                break;

            case VAR_XCR:
                {
                    VarXcr *vx = static_cast<VarXcr*>(instruction);
                    JSValue& dest = (*registers)[dst(vx).first];
                    JSValue r = (*registers)[src1(vx).first].toNumber();
                    dest = r;
                    r.f64 += val3(vx);
                    (*registers)[src1(vx).first] = r;
                }
                break;

            case PROP_XCR:
                {
                    PropXcr *px = static_cast<PropXcr*>(instruction);
                    JSValue& dest = (*registers)[dst(px).first];
                    JSValue& base = (*registers)[src1(px).first];
                    JSObject *object = base.object;
                    JSValue r = object->getProperty(*src2(px)).toNumber();
                    dest = r;
                    r.f64 += val4(px);
                    object->setProperty(*src2(px), r);
                }
                break;

            case SLOT_XCR:
                {
                    SlotXcr *sx = static_cast<SlotXcr*>(instruction);
                    JSValue& dest = (*registers)[dst(sx).first];
                    JSValue& base = (*registers)[src1(sx).first];
                    JSInstance *inst = static_cast<JSInstance*>(base.object);
                    JSValue r = (*inst)[src2(sx)].toNumber();
                    dest = r;
                    r.f64 += val4(sx);
                    (*inst)[src2(sx)] = r;
                }
                break;
            
            case STATIC_XCR:
                {
                    StaticXcr *sx = static_cast<StaticXcr*>(instruction);
                    JSValue& dest = (*registers)[dst(sx).first];
                    JSClass* thisClass = src1(sx);
                    JSValue r = (*thisClass)[src2(sx)].toNumber();
                    dest = r;
                    r.f64 += val4(sx);
                    (*thisClass)[src2(sx)] = r;
                }
                break;

            case NAME_XCR:
                {
                    NameXcr *nx = static_cast<NameXcr*>(instruction);
                    JSValue& dest = (*registers)[dst(nx).first];
                    JSValue r = mGlobal->getVariable(*src1(nx)).toNumber();
                    dest = r;
                    r.f64 += val3(nx);
                    mGlobal->setVariable(*src1(nx), r);
                }
                break;

            case TEST:
                {
                    Test* tst = static_cast<Test*>(instruction);
                    (*registers)[dst(tst).first] = (*registers)[src1(tst).first].toBoolean();
                }
                break;
            case NEGATE:
                {
                    Negate* neg = static_cast<Negate*>(instruction);
                    (*registers)[dst(neg).first] = -(*registers)[src1(neg).first].toNumber().f64;
                }
                break;
            case POSATE:
                {
                    Posate* pos = static_cast<Posate*>(instruction);
                    (*registers)[dst(pos).first] = (*registers)[src1(pos).first].toNumber();
                }
                break;
            case BITNOT:
                {
                    Bitnot* bn = static_cast<Bitnot*>(instruction);
                    (*registers)[dst(bn).first] = ~(*registers)[src1(bn).first].toInt32().i32;
                }
                break;

            case NOT:
                {
                    Not* nt = static_cast<Not*>(instruction);
                    ASSERT((*registers)[src1(nt).first].isBoolean());
                    (*registers)[dst(nt).first] = !(*registers)[src1(nt).first].boolean;
                }
                break;

            case THROW:
                {
                    Throw* thrw = static_cast<Throw*>(instruction);
                    throw new JSException((*registers)[op1(thrw).first]);
                }
                
            case TRYIN:
                {       // push the catch handler address onto the try stack
                    Tryin* tri = static_cast<Tryin*>(instruction);
                    mActivation->catchStack.push_back(new Handler(op1(tri),
                                                                  op2(tri)));
                }   
                break;
            case TRYOUT:
                {
                    Handler *h = mActivation->catchStack.back();
                    mActivation->catchStack.pop_back();
                    delete h;
                }
                break;
            case JSR:
                {
                    subroutineStack.push(++mPC);
                    Jsr* jsr = static_cast<Jsr*>(instruction);
                    uint32 offset = ofs(jsr);
                    mPC = mActivation->mICode->its_iCode->begin() + offset;
                    continue;
                }
            case RTS:
                {
                    ASSERT(!subroutineStack.empty());
                    mPC = subroutineStack.top();
                    subroutineStack.pop();
                    continue;
                }
            case WITHIN:
                {
                    Within* within = static_cast<Within*>(instruction);
                    JSValue& value = (*registers)[op1(within).first];
                    assert(value.tag == JSValue::object_tag);
                    mGlobal = new JSScope(mGlobal, value.object);
                }
                break;
            case WITHOUT:
                {
                    // Without* without = static_cast<Without*>(instruction);
                    mGlobal = mGlobal->getParent();
                }
                break;
            case DEBUGGER:
                {          
                    if (mListeners.size())
                        broadcast(EV_DEBUG);
                    break;
                }
            default:
                NOT_REACHED("bad opcode");
                break;
            }
    
            // increment the program counter.
            ++mPC;
        }
        catch (JSException *x) {
            if (mLinkage) {
                if (mActivation->catchStack.empty()) {
                    Linkage *pLinkage = mLinkage;
                    for (; pLinkage != NULL; pLinkage = pLinkage->mNext) {
                        if (!pLinkage->mActivation->catchStack.empty()) {
                            mActivation = pLinkage->mActivation;
                            Handler *h = mActivation->catchStack.back();
                            registers = &mActivation->mRegisters;
                            if (h->catchTarget) {
                                mPC = mActivation->mICode->its_iCode->begin() + h->catchTarget->mOffset;
                            }
                            else {
                                ASSERT(h->finallyTarget);
                                mPC = mActivation->mICode->its_iCode->begin() + h->finallyTarget->mOffset;
                            }
                            mLinkage = pLinkage;
                            break;
                        }
                    }
                    if (pLinkage)
                        continue;
                }
                else {
                    Handler *h = mActivation->catchStack.back();
                    if (h->catchTarget) {
                        mPC = mActivation->mICode->its_iCode->begin() + h->catchTarget->mOffset;
                    }
                    else {
                        ASSERT(h->finallyTarget);
                        mPC = mActivation->mICode->its_iCode->begin() + h->finallyTarget->mOffset;
                    }
                    continue;
                }
            }
            rv = x->value;
            break;
        }

    }
    mActivation = 0;
    return rv;
} /* interpret */

void Context::addListener(Listener* listener)
{
    mListeners.push_back(listener);
}

void Context::removeListener(Listener* listener)
{
    ListenerIterator e = mListeners.end();
    ListenerIterator l = std::find(mListeners.begin(), e, listener);
    if (l != e) mListeners.erase(l);
}

void Context::broadcast(Event event)
{
    for (ListenerIterator i = mListeners.begin(), e = mListeners.end();
         i != e; ++i) {
        Listener* listener = *i;
        listener->listen(this, event);
    }
}

Context::Frame* Context::getFrames()
{
    return mLinkage;
}

} /* namespace Interpreter */
} /* namespace JavaScript */
