/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "parser.h"
#include "numerics.h"
#include "js2runtime.h"
#include "bytecodegen.h"

#include "jsstring.h"
#include "jsarray.h"
#include "jsmath.h"
#include "hash.h"

#include "fdlibm_ns.h"


namespace JavaScript {    
namespace JS2Runtime {


inline char narrow(char16 ch) { return char(ch); }

js2val Context::readEvalString(const String &str, const String& fileName, ScopeChain *scopeChain, const js2val thisValue)
{
    js2val result = kUndefinedValue;
    NamespaceList *useOnceNamespace = NULL;

    Arena a;
    Parser p(mWorld, a, mFlags, str, fileName);
    Reader *oldReader = mReader;
    setReader(&p.lexer.reader);
    try {
        StmtNode *parsedStatements = p.parseProgram();
        ASSERT(p.lexer.peek(true).hasKind(Token::end));
        if (mDebugFlag)
        {
            PrettyPrinter f(stdOut, 30);
            {
                PrettyPrinter::Block b(f, 2);
                f << "Program =";
                f.linearBreak(1);
                StmtNode::printStatements(f, parsedStatements);
            }
            f.end();
            stdOut << '\n';
        }
        buildRuntime(parsedStatements);
        JS2Runtime::ByteCodeModule* bcm = genCode(parsedStatements, fileName);
        if (bcm) {
            setReader(NULL);
            bcm->setSource(str, fileName);
            result = interpret(bcm, 0, scopeChain, thisValue, NULL, 0);
            delete bcm;
        }
    }
    catch (Exception &x) {
        setReader(oldReader);
        throw x;
    }
    setReader(oldReader);
    return result;
}

js2val Context::readEvalFile(const String& fileName)
{
    String buffer;
    int ch;

    js2val result = kUndefinedValue;

    std::string str(fileName.length(), char());
    std::transform(fileName.begin(), fileName.end(), str.begin(), narrow);
    FILE* f = fopen(str.c_str(), "r");
    if (f) {
        while ((ch = getc(f)) != EOF)
            buffer += static_cast<char>(ch);
        fclose(f);
        result = readEvalString(buffer, fileName, NULL, JSValue::newObject(getGlobalObject()));
    }
    return result;
}

// Given an operator op, and two operand types - dispatch to the
// appropriate operator. The operands are still on the execution stack.
// Return result indicates whether interpreter loop has to begin
// execution of new function.
bool Context::executeOperator(Operator op, JSType *t1, JSType *t2)
{
    // look in the operator table for applicable operators
    OperatorList applicableOperators;

    for (OperatorList::iterator oi = mBinaryOperatorTable[op].begin(),
                end = mBinaryOperatorTable[op].end();
                    (oi != end); oi++) 
    {
        if ((*oi)->isApplicable(t1, t2)) {
            applicableOperators.push_back(*oi);
        }
    }
    if (applicableOperators.size() == 0)
        reportError(Exception::runtimeError, "No applicable operators found");

    OperatorList::iterator candidate = applicableOperators.begin();
    for (OperatorList::iterator aoi = applicableOperators.begin() + 1,
                aend = applicableOperators.end();
                    (aoi != aend); aoi++) 
    {
        if ((*aoi)->mType1->derivesFrom((*candidate)->mType1)
                || ((*aoi)->mType2->derivesFrom((*candidate)->mType2)))
            candidate = aoi;
    }
    // XXX how to complain if there is more than one best fit?

    JSFunction *target = (*candidate)->mImp;

    js2val newThis = kNullValue;
    if (target->isNative()) {
        js2val result = target->invokeNativeCode(this, newThis, getBase(stackSize() - 2), 2);
        resizeStack(stackSize() - 2);
        pushValue(result);
        return false;
    }
    else {
        mActivationStack.push(new Activation(mLocals, mStack, mStackTop - 2, mScopeChain,
                                                mArgumentBase, mThis, mPC, mCurModule, mNamespaceList));
        mThis = newThis;
        mCurModule = target->getByteCode();
        mArgumentBase = getBase(stackSize() - 2);
        mScopeChain = target->getScopeChain();
        return true;
    }
}


// Invokes either the native or bytecode implementation. Causes another interpreter loop
// to begin execution, and does nothing to clean up the incoming arguments (which need
// not even be on the execution stack).
js2val Context::invokeFunction(JSFunction *target, const js2val thisValue, js2val *argv, uint32 argc)
{
    if (target->isNative())
        return target->invokeNativeCode(this, thisValue, argv, argc);
    else
        return interpret(target->getByteCode(), 0, target->getScopeChain(), thisValue, argv, argc);
}

js2val Context::interpret(JS2Runtime::ByteCodeModule *bcm, int offset, ScopeChain *scopeChain, const js2val thisValue, js2val *argv, uint32 /*argc*/)
{ 
    Activation *prev = new Activation(mLocals, mStack, mStackTop, mScopeChain,
                                            mArgumentBase, mThis, NULL, mCurModule, mNamespaceList); // use NULL pc value to force interpret loop to exit
    uint32 activationHeight = mActivationStack.size();
    mActivationStack.push(prev);   
    mThis = thisValue;
    if (scopeChain)
        mScopeChain = scopeChain;
    else {
        mScopeChain = new ScopeChain(this, mWorld);
        mScopeChain->addScope(getGlobalObject());
    }
    
//    if (JSValue::isObject(mThis))
//        mScopeChain->addScope(mThis.object);
//    mScopeChain->addScope(mActivationStack.top());

    mCurModule = bcm;
    uint8 *pc = mCurModule->mCodeBase + offset;
    uint8 *endPC = mCurModule->mCodeBase + mCurModule->mLength;
    mArgumentBase = argv;
    mLocals = new js2val[mCurModule->mLocalsCount];
    mStack = new js2val[mCurModule->mStackDepth];
    mStackMax = mCurModule->mStackDepth;
    mStackTop = 0;

    js2val result;
    try {
        result = interpret(pc, endPC);
    }
    catch (Exception &jsx) {
        while (mActivationStack.size() != activationHeight)
            mActivationStack.pop();
         
        // the following (delete's) are a bit iffy - depends on whether
        // a closure capturing the contents has come along...
//        if (JSValue::isObject(mThis))
//          mScopeChain->popScope();
        js2val x;
        if (jsx.hasKind(Exception::userException)) 
            x = popValue();

        delete[] mStack;
        delete[] mLocals;
        if (scopeChain == NULL)
            delete mScopeChain;

        mNamespaceList = prev->mNamespaceList;
        mCurModule = prev->mModule;
        mStack = prev->mStack;
        mStackTop = 0;      // we're processing an exception, no need to preserve the stack
        

        if (mCurModule) {
            mStackMax = mCurModule->mStackDepth;
            if (jsx.hasKind(Exception::userException)) 
                pushValue(x);
        }
        mLocals = prev->mLocals;
        mArgumentBase = prev->mArgumentBase;
        mThis = prev->mThis;
        mScopeChain = prev->mScopeChain;
        delete prev;
        throw jsx;
    }
    ASSERT(prev == mActivationStack.top());
    mActivationStack.pop();

    // the following (delete's) are a bit iffy - depends on whether
    // a closure capturing the contents has come along...
//    if (JSValue::isObject(mThis))
//        mScopeChain->popScope();
    delete[] mStack;
    delete[] mLocals;
    if (scopeChain == NULL)
        delete mScopeChain;

    mNamespaceList = prev->mNamespaceList;
    mCurModule = prev->mModule;
    mStack = prev->mStack;
    mStackTop = prev->mStackTop;
    if (mCurModule)
        mStackMax = mCurModule->mStackDepth;
    mLocals = prev->mLocals;
    mArgumentBase = prev->mArgumentBase;
    mThis = prev->mThis;
    mScopeChain = prev->mScopeChain;
    delete prev;
    return result;
}

// Assumes arguments are on the top of stack. argCount is the number that were pushed by user code
js2val *Context::buildArgumentBlock(JSFunction *target, uint32 &argCount)
{
    js2val *argBase;
    uint32 maxExpectedParameterCount = target->getRequiredParameterCount() + target->getOptionalParameterCount() + target->getNamedParameterCount();
    if (target->isChecked()) {
        if (argCount < target->getRequiredParameterCount())
            reportError(Exception::referenceError, "Insufficient quantity of arguments");
    
        if ((argCount > maxExpectedParameterCount) && !target->hasRestParameter())
            reportError(Exception::referenceError, "Oversufficient quantity of arguments");
    }

    uint32 i;
    uint32 argBlockSize = max(argCount, maxExpectedParameterCount) + (target->hasRestParameter() ? 1 : 0);
                                                    // room for all required,optional & named arguments
                                                    // plus the rest parameter if it exists.
    argBase = new js2val[argBlockSize];     

/*
* If the first parameter is required and no positional argument has been supplied for it, then raise an error unless the function 
*   is unchecked, in which case let undefined be the first parameter’s value.
*
* If the first parameter is optional and there is a positional argument remaining, use the value of the positional argument 
*    and raise an error if there is also a named argument with a matching argument name. If there are no remaining positional 
*    arguments, then if there is a named argument with a matching argument name, use the value of that named argument. Otherwise, 
*   evaluate the first parameter’s AssignmentExpression and let it be the first parameter’s value.
* 
* Implicitly coerce the argument (or default) value to type t and bind the parameter’s Identifier to the result.
*/    
    uint32 inArgIndex = 0;
    uint32 argStart = stackSize() - argCount;
    bool *argUsed = new bool[argCount];
    for (i = 0; i < argCount; i++)
        argUsed[i] = false;    
    uint32 restParameterIndex;

    uint32 pCount = maxExpectedParameterCount + (target->hasRestParameter() ? 1 : 0);
    for (uint32 pIndex = 0; pIndex < pCount; pIndex++) {        
        bool needDefault = true;
        if (target->parameterIsRequired(pIndex)) {
            if (inArgIndex < argCount) {
                js2val v = getValue(inArgIndex + argStart);
                bool isPositionalArg = !JSValue::isNamedArg(v);
                // if the next argument is named, then we've run out of positional args
                if (!isPositionalArg) {
                    if (!target->isChecked())
                        needDefault = false; // the undefined value is already assigned in the arg block
                }
                else {
                    argBase[pIndex] = getValue(inArgIndex + argStart);
                    argUsed[inArgIndex++] = true;
                    needDefault = false;
                }
            }
            if (needDefault && target->isChecked())
                reportError(Exception::referenceError, "missing required argument");
        }
        else {
            if (target->parameterIsOptional(pIndex)) {
                bool tookPositionalArg = false;
                if (inArgIndex < argCount) {
                    js2val v = getValue(inArgIndex + argStart);
                    if (!JSValue::isNamedArg(v)) {
                        argBase[pIndex] = v;
                        argUsed[inArgIndex++] = true;
                        needDefault = false;
                    }
                }
            }
            else {
                if (target->parameterIsNamed(pIndex)) {
                    const String *parameterName = target->getParameterName(pIndex);
                    for (uint32 i = inArgIndex; i < argCount; i++) {
                        if (!argUsed[i]) {
                            js2val v = getValue(i + argStart);
                            if (JSValue::isNamedArg(v)) {
                                if (JSValue::namedArg(v)->mName == parameterName) {
                                    argBase[pIndex] = JSValue::namedArg(v)->mValue;
                                    argUsed[i] = true;
                                    needDefault = false;
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                    restParameterIndex = pIndex;
            }
        }
        if (needDefault)
            if (target->parameterHasInitializer(pIndex))
                argBase[pIndex] = target->runParameterInitializer(this, pIndex, mThis, argBase, maxExpectedParameterCount);
    }

    // now find a place for any left-overs:
/*
* If there is a RestParameter with a type that does not allow the dynamic addition of named properties and one or more of the
*    remaining arguments is named, raise an error.
*
* If there is a RestParameter with an Identifier, bind that Identifier to an array of the remaining positional and/or named
*    arguments. The remaining positional arguments are assigned indices starting from 0.
*
* If there is no RestParameter and any arguments remain, raise an error unless the function is unchecked.
*/

    js2val restArgument;
    bool haveRestArg = false;
    
    if (target->hasRestParameter() && target->getParameterName(restParameterIndex)) {
        restArgument = target->getParameterType(restParameterIndex)->newInstance(this);
        argBase[restParameterIndex] = restArgument;
        haveRestArg = true;
    }
    inArgIndex = 0;    // re-number the non-named arguments that end up in the rest arg
    for (i = 0; i < argCount; i++) {
        if (!argUsed[i]) {
            js2val v = getValue(i + argStart);
            bool isNamedArg = JSValue::isNamedArg(v);
            if (isNamedArg) {
                NamedArgument *arg = JSValue::namedArg(v);
                if (haveRestArg)
                    JSValue::object(restArgument)->setProperty(this, *arg->mName, (NamespaceList *)(NULL), arg->mValue);
                else
                    reportError(Exception::referenceError, "Unused named argument, no rest argument");                                
            }
            else {
                if (haveRestArg) {
                    const String *id = numberToString(inArgIndex++);
                    JSValue::object(restArgument)->setProperty(this, *id, (NamespaceList *)(NULL), v);
                }
                else {
                    if (target->isChecked())
                       reportError(Exception::referenceError, "Extra argument, no rest argument"); 
                    else {
                        if (isNamedArg)
                            argBase[i] = JSValue::namedArg(v)->mValue;
                        else
                            argBase[i] = v;
                    }
                }
            }
        }
    }
    
    argCount = argBlockSize;
    return argBase;
}

js2val Context::interpret(uint8 *pc, uint8 *endPC)
{
    bool useOnceNamespace = false;
    js2val result = kUndefinedValue;
    while (pc != endPC) {
        mPC = pc;
        try {
            if (mDebugFlag) {
                FunctionName *fnName;
                uint32 x = mScopeChain->mScopeStack.size();
                if (mCurModule->mFunction && (fnName = mCurModule->mFunction->getFunctionName())) {
                    StringFormatter s;
                    PrettyPrinter pp(s);
                    fnName->print(pp);
                    const String &fnStr = s.getString();
                    std::string str(fnStr.length(), char());
                    std::transform(fnStr.begin(), fnStr.end(), str.begin(), narrow);
                    uint32 len = strlen(str.c_str());
                    printFormat(stdOut, "%.30s+%.4d%*c%d %d        ", str.c_str(), (pc - mCurModule->mCodeBase), (len > 30) ? 0 : (len - 30), ' ', stackSize(), x);
                }
                else
                    printFormat(stdOut, "+%.4d%*c%d %d        ", (pc - mCurModule->mCodeBase), 30, ' ', stackSize(), x);
                printInstruction(stdOut, toUInt32(pc - mCurModule->mCodeBase), *mCurModule);
            }
            switch ((ByteCodeOp)(*pc++)) {
            case PopOp:
                {
                    result = popValue();
                }
                break;
            case VoidPopOp:
                {
                    popValue(); // XXX just decrement top
                }
                break;
            case PopNOp:
                {
                    uint16 count = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    resizeStack(mStackTop - count);
                }
                break;
            case DupOp:
                {
                    js2val v = topValue();
                    pushValue(v);
                }
                break;
            case DupInsertOp:   // XXX something more efficient than pop/push?
                {
                    js2val v1 = popValue();
                    js2val v2 = popValue();
                    pushValue(v1);
                    pushValue(v2);
                    pushValue(v1);
                }
                break;
            case DupNOp:
                {
                    uint16 count = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    js2val *vp = getBase(stackSize() - count);
                    while (count--)
                        pushValue(*vp++);
                }
                break;
            // <N things> <object2> --> <object2> <N things> <object2>
            case DupInsertNOp:
                {
                    js2val v2 = topValue();
                    uint16 count = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    insertValue(v2, mStackTop - (count + 1));
                }
                break;
            case SwapOp:   // XXX something more efficient than pop/push?
                {
                    js2val v1 = popValue();
                    js2val v2 = popValue();
                    pushValue(v1);
                    pushValue(v2);
                }
                break;
            case LogicalXorOp:
                {
                    js2val v2 = popValue();
                    ASSERT(JSValue::isBool(v2));
                    js2val v1 = popValue();
                    ASSERT(JSValue::isBool(v1));

                    if (JSValue::boolean(v1)) {
                        if (JSValue::boolean(v2)) {
                            popValue();
                            popValue();
                            pushValue(kFalseValue);
                        }
                        else
                            popValue();
                    }
                    else {
                        if (JSValue::boolean(v1)) {
                            popValue();
                            popValue();
                            pushValue(kFalseValue);
                        }
                        else {
                            js2val t = topValue();
                            popValue();
                            popValue();
                            pushValue(t);
                        }
                    }
                }
                break;
            case LogicalNotOp:
                {
                    js2val v = popValue();
                    ASSERT(JSValue::isBool(v));
                    if (JSValue::isTrue(v))
                        pushValue(kFalseValue);
                    else
                        pushValue(kTrueValue);
                }
                break;
            case JumpOp:
                {
                    uint32 offset = *((uint32 *)pc);
                    pc += offset;
                }
                break;
            case ToBooleanOp:
                {
                    js2val v = popValue();
                    pushValue(JSValue::toBoolean(this, v));
                }
                break;
            case JumpFalseOp:
                {
                    js2val v = popValue();
                    ASSERT(JSValue::isBool(v));
                    if (!JSValue::boolean(v)) {
                        uint32 offset = *((uint32 *)pc);
                        pc += offset;
                    }
                    else
                        pc += sizeof(uint32);
                }
                break;
            case JumpTrueOp:
                {
                    js2val v = popValue();
                    ASSERT(JSValue::isBool(v));
                    if (JSValue::boolean(v)) {
                        uint32 offset = *((uint32 *)pc);
                        pc += offset;
                    }
                    else
                        pc += sizeof(uint32);
                }
                break;
            case NamedArgOp:
                {
                    js2val name = popValue();
                    if (!JSValue::isString(name))
                        reportError(Exception::typeError, "String needed for argument name");
                    js2val value = popValue();
                    pushValue(JSValue::newNamedArg(new NamedArgument(value, JSValue::string(name))));
                }
                break;
            case InvokeOp:
                {
                    uint32 argCount = *((uint32 *)pc); 
                    uint32 cleanUp = argCount;
                    pc += sizeof(uint32);
                    CallFlag callFlags = (CallFlag)(*pc++);
                    mPC = pc;

                    js2val *targetValue = getBase(stackSize() - (argCount + 1));
                    JSFunction *target;
                    js2val oldThis = mThis;
                    switch (callFlags & ThisFlags) {
                    case NoThis:
                        mThis = kNullValue; 
                        break;
                    case Explicit:
                        mThis = getValue(stackSize() - (argCount + 2));
                        cleanUp++;
                        break;
                    default:
                        NOT_REACHED("bad bytecode");
                    }

                    if (!JSValue::isFunction(*targetValue)) {
                        if (JSValue::isType(*targetValue)) {
                            // how to distinguish between a cast and an invocation of
                            // the superclass constructor from a constructor????
                            // XXX help
                            if ((callFlags & SuperInvoke) == SuperInvoke) {
                                // in this case, calling the constructor requires passing the 'this' value
                                // through.
                                target = JSValue::type(*targetValue)->getDefaultConstructor();
                                mThis = oldThis;
                            }
                            else {
                                // "  Type()  "
                                // - it's a cast expression, we call the
                                // default constructor, overriding the supplied 'this'.
                                //
                            
                                // XXX Note that this is different behaviour from JS1.5, where
                                // (e.g.) Array(2) is an invocation of the constructor.
                                
                                target = JSValue::type(*targetValue)->getTypeCastFunction();
                                if (target == NULL) {
                                    if ((argCount > 1) || ((callFlags & ThisFlags) != NoThis))
                                        reportError(Exception::referenceError, "Type cast can only take one argument");
                                    js2val v;
                                    if (argCount > 0)
                                        v = popValue();
                                    popValue();             // don't need the target anymore
                                    pushValue(mapValueToType(v, JSValue::type(*targetValue)));
                                    mThis = oldThis;
                                    break;      // all done
                                }
                            }
                        }
                        else
                            reportError(Exception::referenceError, "Not a function");
                    }
                    else {
                        target = JSValue::function(*targetValue);

                        // an invocation of the super constructor has to pass thru the existing 'this'
                        if (target->isConstructor() && (callFlags & SuperInvoke))
                            mThis = oldThis;
                        else
                            if (target->hasBoundThis())   // then we use it instead of the expressed version
                                mThis = target->getThisValue();
                    }
                    if (target->isPrototype() && JSValue::isNull(mThis))
                        mThis = JSValue::newObject(getGlobalObject());
                                        
                    if (!target->isNative()) {
                        js2val *argBase = buildArgumentBlock(target, argCount);
                        // XXX Optimize the more typical case in which there are no named arguments, no optional arguments
                        // and the appropriate number of arguments have been supplied.

                        mActivationStack.push(new Activation(mLocals, mStack, mStackTop - (cleanUp + 1),
                                                                    mScopeChain,
                                                                    mArgumentBase, oldThis,
                                                                    pc, mCurModule, mNamespaceList));
                        mScopeChain = target->getScopeChain();
                        mScopeChain->addScope(target->getParameterBarrel());
                        mScopeChain->addScope(target->getActivation());

                        if (!target->isChecked()) {
                            js2val args = Array_Type->newInstance(this);
                            for (uint32 i = 0; i < argCount; i++) 
                                JSValue::instance(args)->setProperty(this, *numberToString(i), NULL, argBase[i]);
                            target->getActivation()->setProperty(this, Arguments_StringAtom, NULL, args);
                        }

                        mCurModule = target->getByteCode();
                        pc = mCurModule->mCodeBase;
                        endPC = mCurModule->mCodeBase + mCurModule->mLength;
                        mArgumentBase = argBase;
                        mLocals = new js2val[mCurModule->mLocalsCount];
                        mStack = new js2val[mCurModule->mStackDepth];
                        mStackMax = mCurModule->mStackDepth;
                        mStackTop = 0;
                    }
                    else {
                        js2val *argBase = getBase(stackSize() - argCount);
                        // native functions may still need access to information
                        // about the currently executing function.
                        mActivationStack.push(new Activation(mLocals, mStack, mStackTop - (cleanUp + 1),
                                                                    mScopeChain,
                                                                    mArgumentBase, oldThis,
                                                                    pc, mCurModule, mNamespaceList));
                        js2val result = target->invokeNativeCode(this, mThis, argBase, argCount);
                        Activation *prev = mActivationStack.top();
                        delete prev;
                        mActivationStack.pop();

                        mThis = oldThis;
                        resizeStack(stackSize() - (cleanUp + 1));
                        pushValue(result);
                    }

                }
                break;
            case ReturnVoidOp:
                {                    
                    if (mActivationStack.empty())
                        return result;
                    Activation *prev = mActivationStack.top();
                    if (prev->mPC == NULL) {   // NULL is used to indicate that we want the loop to exit
                                               // (even though there is more activation stack to go
                        return result;         // - used to implement Xetters from XProperty ops. e.g.)
                                        
                    }
                    mActivationStack.pop();
                    delete[] mLocals;
                    delete[] mStack;

                    mScopeChain->popScope();
                    mScopeChain->popScope();

                    mNamespaceList = prev->mNamespaceList;
                    mCurModule = prev->mModule;
                    pc = prev->mPC;
                    endPC = mCurModule->mCodeBase + mCurModule->mLength;
                    mStack = prev->mStack;
                    mStackTop = prev->mStackTop;
                    mStackMax = mCurModule->mStackDepth;
                    mLocals = prev->mLocals;
                    mArgumentBase = prev->mArgumentBase;
                    mThis = prev->mThis;
                    mScopeChain = prev->mScopeChain;       
                    pushValue(kUndefinedValue);
                    delete prev;
                }
                break;
            case ReturnOp:
                {
                    js2val result = popValue();                    
                    if (mActivationStack.empty())
                        return result;                    
                    Activation *prev = mActivationStack.top();
                    if (prev->mPC == NULL) {
                        return result;
                    }
                    mActivationStack.pop();
                    delete[] mLocals;
                    delete[] mStack;                    

                    mScopeChain->popScope();
                    mScopeChain->popScope();

                    mNamespaceList = prev->mNamespaceList;
                    mCurModule = prev->mModule;
                    pc = prev->mPC;
                    endPC = mCurModule->mCodeBase + mCurModule->mLength;
                    mStack = prev->mStack;
                    mStackTop = prev->mStackTop;
                    mStackMax = mCurModule->mStackDepth;
                    mLocals = prev->mLocals;
                    mArgumentBase = prev->mArgumentBase;
                    mThis = prev->mThis;
                    mScopeChain = prev->mScopeChain;
                    pushValue(result);
                    delete prev;
                }
                break;
            case LoadTypeOp:
                {
                    JSType *t = *((JSType **)pc);
                    pc += sizeof(JSType *);
                    pushValue(JSValue::newType(t));
                }
                break;
            case LoadFunctionOp:
                {
                    JSFunction *f = *((JSFunction **)pc);
                    pc += sizeof(JSFunction *);
                    pushValue(JSValue::newFunction(f));
                }
                break;
            case LoadConstantStringOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(JSValue::newString(mCurModule->getString(index)));
                }
                break;
            case LoadConstantNumberOp:
                {
                    pushValue(JSValue::newNumber(mCurModule->getNumber(pc)));
                    pc += sizeof(float64);
                }
                break;
            case LoadConstantUndefinedOp:
                pushValue(kUndefinedValue);
                break;
            case LoadConstantTrueOp:
                pushValue(kTrueValue);
                break;
            case LoadConstantFalseOp:
                pushValue(kFalseValue);
                break;
            case LoadConstantNullOp:
                pushValue(kNullValue);
                break;
            case LoadConstantZeroOp:
                pushValue(kPositiveZero);
                break;
            case LoadConstantRegExpOp:
                {
                    JSInstance *t = *((JSInstance **)pc);
                    pc += sizeof(JSInstance *);
                    pushValue(JSValue::newInstance(t));
                }
                break;
            case DeleteNameOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const StringAtom &name = *mCurModule->getString(index);
                    if (mScopeChain->deleteName(this, name, mNamespaceList))
                        pushValue(kTrueValue);
                    else
                        pushValue(kFalseValue);
                }
                break;
            case DeleteOp:
                {
                    js2val base = popValue();
                    JSObject *obj = JSValue::toObjectValue(this, base);
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const StringAtom &name = *mCurModule->getString(index);
                    PropertyIterator it;
                    if (obj->hasOwnProperty(this, name, mNamespaceList, Read, &it))
                        if (obj->deleteProperty(this, name, mNamespaceList))
                            pushValue(kTrueValue);
                        else
                            pushValue(kFalseValue);
                    else
                        pushValue(kTrueValue);
                }
                break;
            case TypeOfOp:
                {
                    js2val v = popValue();
                    if (JSValue::isUndefined(v))
                        pushValue(JSValue::newString(&Undefined_StringAtom));
                    else
                    if (JSValue::isNull(v))
                        pushValue(JSValue::newString(&Object_StringAtom));
                    else
                    if (JSValue::isBool(v))
                        pushValue(JSValue::newString(&Boolean_StringAtom));
                    else
                    if (JSValue::isNumber(v))
                        pushValue(JSValue::newString(&Number_StringAtom));
                    else
                    if (JSValue::isString(v))
                        pushValue(JSValue::newString(&String_StringAtom));
                    else
                    if (JSValue::isFunction(v))
                        pushValue(JSValue::newString(&Function_StringAtom));
                    else
                        pushValue(JSValue::newString(&Object_StringAtom));
                }
                break;
            case AsOp:
                {
                    js2val t = popValue();
                    js2val v = popValue();
                    if (JSValue::isType(t)) {
                        if (JSValue::isInstance(v) 
                            && (JSValue::instance(v)->getType() == JSValue::type(t)))
                            pushValue(v);
                        else
                            pushValue(kNullValue);   // XXX or throw an exception if 
                                                            // NULL is not a member of type t
                    }
                    else
                        reportError(Exception::typeError, "As needs type");
                }
                break;
            case IsOp:
                {
                    js2val t = popValue();
                    js2val v = popValue();
                    if (JSValue::isType(t)) {
                        if (JSValue::isNull(v))
                            if (JSValue::type(t) == Object_Type)
                                pushValue(kTrueValue);
                            else
                                pushValue(kFalseValue);
                        else
                            if (JSValue::isInstance(v) 
                                && ((JSValue::instance(v)->getType() == JSValue::type(t))
                                        || (JSValue::instance(v)->getType()->derivesFrom(JSValue::type(t)))))
                                pushValue(kTrueValue);
                            else {
                                if (JSValue::getType(v) == JSValue::type(t))
                                    pushValue(kTrueValue);
                                else
                                    pushValue(kFalseValue);
                            }
                    }
                    else {  // behave like instanceof
                        if (JSValue::isObject(t) && JSValue::isFunction(t)) {                            
                            // XXX prove that t->function["prototype"] is on t.object->mPrototype chain
                            pushValue(kTrueValue);
                        }
                        else
                            reportError(Exception::typeError, "InstanceOf needs object");
                    }
                }
                break;
            case InstanceOfOp:  
                {
                    js2val t = popValue();
                    js2val v = popValue();
                    if (JSValue::isFunction(t)) {
                        JSFunction *obj = JSValue::function(t);                        
                        PropertyIterator i;
                        JSFunction *target = NULL;
                        if (obj->hasProperty(this, HasInstance_StringAtom, mNamespaceList, Read, &i)) {
                            js2val hi = obj->getPropertyValue(i);
                            if (JSValue::isFunction(hi))
                                target = JSValue::function(hi);
                        }
                        if (target)
                            pushValue(invokeFunction(target, t, &v, 1));
                        else
                            reportError(Exception::typeError, "InstanceOf couldn't find [[hasInstance]]");
                    }
                    else {
                        // XXX hack for <= ECMA 3 compatibility
                        if (JSValue::isType(t)) {
                            if ((JSValue::getType(v) == JSValue::type(t))
                                || JSValue::getType(v)->derivesFrom(JSValue::type(t)))
                                pushValue(kTrueValue);
                            else
                                pushValue(kFalseValue);
                        }
                        else
                            reportError(Exception::typeError, "InstanceOf needs function object");
                    }
                }
                break;
            case GetNameOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const StringAtom &name = *mCurModule->getString(index);
                    JSObject *parent = mScopeChain->getNameValue(this, name, mNamespaceList);
                    js2val result = topValue();
                    if (JSValue::isFunction(result) && !JSValue::function(result)->hasBoundThis()) {
                        popValue();
                        if (JSValue::function(result)->isConstructor())
                            // A constructor has to be called with a NULL 'this' in order to prompt it
                            // to construct the instance object.
                            pushValue(JSValue::newFunction((JSFunction *)(new JSBoundFunction(this, JSValue::function(result), NULL))));
                        else
                            pushValue(JSValue::newFunction((JSFunction *)(new JSBoundFunction(this, JSValue::function(result), parent))));
                    }
                    if (useOnceNamespace) {
                        NamespaceList *t = mNamespaceList->mNext;
                        delete mNamespaceList;
                        useOnceNamespace = false;
                        mNamespaceList = t;
                    }                        
                }
                break;
            case SetNameOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const StringAtom &name = *mCurModule->getString(index);
                    js2val v = topValue();
                    if (JSValue::isFunction(v) && JSValue::function(v)->hasBoundThis() && !JSValue::function(v)->isMethod()) {
                        popValue();
                        pushValue(JSValue::newFunction(JSValue::function(v)->getFunction()));
                    }
                    mScopeChain->setNameValue(this, name, mNamespaceList);
                    if (useOnceNamespace) {
                        NamespaceList *t = mNamespaceList->mNext;
                        delete mNamespaceList;
                        useOnceNamespace = false;
                        mNamespaceList = t;
                    }                        
                }
                break;
            case GetTypeOfNameOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const StringAtom &name = *mCurModule->getString(index);
                    if (mScopeChain->hasNameValue(this, name, mNamespaceList)) {
                        mScopeChain->getNameValue(this, name, mNamespaceList);
                    }
                    else
                        pushValue(kUndefinedValue);
                    if (useOnceNamespace) {
                        NamespaceList *t = mNamespaceList->mNext;
                        delete mNamespaceList;
                        useOnceNamespace = false;
                        mNamespaceList = t;
                    }                        
                }
                break;
            case GetElementOp:
                {
                    uint32 dimCount = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    mPC = pc;

                    js2val *base = getBase(stackSize() - (dimCount + 1));

                    // Use the type of the base to dispatch on...
                    JSObject *obj = JSValue::toObjectValue(this, *base);
                    JSFunction *target = getUnaryOperator(JSValue::getType(*base), Index);

                    if (target) {
                        js2val result;
                        if (target->isNative()) {
                            js2val *argBase = new js2val[dimCount + 1];
                            for (uint32 i = 0; i < (dimCount + 1); i++)
                                argBase[i] = base[i];
                            resizeStack(stackSize() - (dimCount + 1));
                            result = target->invokeNativeCode(this, argBase[0], argBase, dimCount + 1);
                            delete[] argBase;
                        }
                        else {
                            uint32 argCount = dimCount + 1;
                            js2val *argBase = buildArgumentBlock(target, argCount);
                            resizeStack(stackSize() - (dimCount + 1));
                            try {
                                result = interpret(target->getByteCode(), 0, target->getScopeChain(), argBase[0], argBase, argCount);
                            }
                            catch (Exception &x) {
                                delete[] argBase;
                                throw x;
                            }
                        }
                        pushValue(result);
                    }
                    else {  // XXX or should this be implemented in Object_Type as operator "[]" ?
                        if (dimCount != 1)
                            reportError(Exception::typeError, "too many indices");
                        js2val index = popValue();
                        popValue();     // discard base
                        const String *name = JSValue::string(JSValue::toString(this, index));
                        obj->getProperty(this, *name, mNamespaceList);
                    }
                    // if the result is a method of some kind, bind
                    // the base object to it
                    js2val result = topValue();
                    if (JSValue::isFunction(result)) {
                        popValue();
                        if (JSValue::function(result)->isConstructor())
                            // A constructor has to be called with a NULL 'this' in order to prompt it
                            // to construct the instance object.
                            pushValue(JSValue::newFunction((JSFunction *)(new JSBoundFunction(this, JSValue::function(result), NULL))));
                        else
                            pushValue(JSValue::newFunction((JSFunction *)(new JSBoundFunction(this, JSValue::function(result), obj))));
                    }
                }
                break;
            case SetElementOp:
                {
                    uint32 dimCount = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    mPC = pc;

                    js2val *base = getBase(stackSize() - (dimCount + 2));     // +1 for assigned value

                    // Use the type of the base to dispatch on...
                    JSObject *obj = JSValue::toObjectValue(this, *base);
                    JSFunction *target = getUnaryOperator(JSValue::getType(*base), IndexEqual);

                    if (target) {
                        js2val v = popValue();     // need to have this sitting right above the base value
                        if (JSValue::isFunction(v) && JSValue::function(v)->hasBoundThis() && !JSValue::function(v)->isMethod()) {
                            v = JSValue::newFunction(JSValue::function(v)->getFunction());
                        }
                        insertValue(v, mStackTop - dimCount);

                        js2val result;
                        if (target->isNative()) {
                            js2val *argBase = new js2val[dimCount + 2];
                            for (uint32 i = 0; i < (dimCount + 2); i++)
                                argBase[i] = base[i];
                            resizeStack(stackSize() - (dimCount + 2));
                            result = target->invokeNativeCode(this, *base, base, (dimCount + 2));
                            delete[] argBase;
                        }
                        else {
                            uint32 argCount = dimCount + 2;
                            js2val *argBase = buildArgumentBlock(target, argCount);
                            resizeStack(stackSize() - (dimCount + 2));
                            try {
                                result = interpret(target->getByteCode(), 0, target->getScopeChain(), *base, argBase, argCount);
                            }
                            catch (Exception &x) {
                                delete[] argBase;
                                throw x;
                            }
                        }
                        pushValue(result);
                    }
                    else {  // XXX or should this be implemented in Object_Type as operator "[]=" ?
                        if (dimCount != 1)
                            reportError(Exception::typeError, "too many indices");
                        js2val v = popValue();
                        if (JSValue::isFunction(v) && JSValue::function(v)->hasBoundThis() && !JSValue::function(v)->isMethod()) {
                            v = JSValue::newFunction(JSValue::function(v)->getFunction());
                        }
                        js2val index = popValue();
                        popValue();     // discard base
                        const String *name = JSValue::string(JSValue::toString(this, index));
                        obj->setProperty(this, *name, mNamespaceList, v);
                        pushValue(v);                
                    }
                }
                break;
            case DeleteElementOp:
                {
                    uint32 dimCount = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    mPC = pc;

                    js2val *base = getBase(stackSize() - (dimCount + 1));

                    // Use the type of the base to dispatch on...
                    JSObject *obj = JSValue::toObjectValue(this, *base);
                    JSFunction *target = getUnaryOperator(JSValue::getType(*base), DeleteIndex);

                    if (target) {
                        js2val result;
                        if (target->isNative()) {
                            js2val *argBase = new js2val[dimCount + 1];
                            for (uint32 i = 0; i < (dimCount + 1); i++)
                                argBase[i] = base[i];
                            resizeStack(stackSize() - (dimCount + 1));
                            result = target->invokeNativeCode(this, argBase[0], argBase, dimCount + 1);
                            delete[] argBase;
                        }
                        else {
                            uint32 argCount = dimCount + 1;
                            js2val *argBase = buildArgumentBlock(target, argCount);
                            resizeStack(stackSize() - (dimCount + 1));
                            try {
                                result = interpret(target->getByteCode(), 0, target->getScopeChain(), kNullValue, argBase, argCount);
                            }
                            catch (Exception &x) {
                                delete[] argBase;
                                throw x;
                            }
                        }
                        pushValue(result);
                    }
                    else {  // XXX or should this be implemented in Object_Type as operator "delete[]" ?
                        if (dimCount != 1)
                            reportError(Exception::typeError, "too many indices");
                        js2val index = popValue();
                        popValue();     // discard base
                        const String *name = JSValue::string(JSValue::toString(this, index));
                        PropertyIterator it;
                        if (obj->hasOwnProperty(this, *name, mNamespaceList, Read, &it))
                            obj->deleteProperty(this, *name, mNamespaceList);
                        pushValue(kTrueValue);
                    }
                }
                break;
            case GetPropertyOp:
                {
                    js2val base = popValue();
                    JSObject *obj = JSValue::toObjectValue(this, base);
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    mPC = pc;
                    const StringAtom &name = *mCurModule->getString(index);
                    obj->getProperty(this, name, mNamespaceList);
                    // if the result is a method of some kind, bind
                    // the base object to it
                    js2val result = topValue();
                    if (JSValue::isFunction(result)) {
                        popValue();
                        if (JSValue::function(result)->isConstructor())
                            // A constructor has to be called with a NULL 'this' in order to prompt it
                            // to construct the instance object.
                            pushValue(JSValue::newFunction((JSFunction *)(new JSBoundFunction(this, JSValue::function(result), NULL))));
                        else
                            pushValue(JSValue::newFunction((JSFunction *)(new JSBoundFunction(this, JSValue::function(result), obj))));
                    }
                    if (useOnceNamespace) {
                        NamespaceList *t = mNamespaceList->mNext;
                        delete mNamespaceList;
                        useOnceNamespace = false;
                        mNamespaceList = t;
                    }                        
                }
                break;
            case GetInvokePropertyOp:
                {
                    js2val base = topValue();
                    js2val baseObject = JSValue::toObject(this, base);
                    // XXX really only need to pop/push if the base got modified
                    popValue();
                    pushValue(baseObject); // want the "toObject'd" version of base

                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    mPC = pc;
                    
                    const StringAtom &name = *mCurModule->getString(index);
                    JSValue::getObjectValue(baseObject)->getProperty(this, name, mNamespaceList);
                    if (useOnceNamespace) {
                        NamespaceList *t = mNamespaceList->mNext;
                        delete mNamespaceList;
                        useOnceNamespace = false;
                        mNamespaceList = t;
                    }                        
                }
                break;
            case SetPropertyOp:
                {
                    js2val v = popValue();
                    if (JSValue::isFunction(v) && JSValue::function(v)->hasBoundThis() && !JSValue::function(v)->isMethod()) {
                        v = JSValue::newFunction(JSValue::function(v)->getFunction());
                    }
                    js2val base = popValue();
                    JSObject *obj = JSValue::toObjectValue(this, base);
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    mPC = pc;
                    const StringAtom &name = *mCurModule->getString(index);
                    obj->setProperty(this, name, mNamespaceList, v);
                    pushValue(v);
                    if (useOnceNamespace) {
                        NamespaceList *t = mNamespaceList->mNext;
                        delete mNamespaceList;
                        useOnceNamespace = false;
                        mNamespaceList = t;
                    }                        
                }
                break;
            case DoUnaryOp:
                {
                    Operator op = (Operator)(*pc++);
                    mPC = pc;
                    js2val v = topValue();
                    JSFunction *target = getUnaryOperator(JSValue::getType(v), op);
                    if (target) {                    
                        uint32 argBase = stackSize() - 1;
                        js2val newThis = kNullValue;
                        if (!target->isNative()) {
                            // lie about argCount to the activation since it
                            // would normally expect to clean the function pointer
                            // off the stack as well.
                            mActivationStack.push(new Activation(mLocals, mStack, mStackTop - 1, 
                                                                    mScopeChain,
                                                                    mArgumentBase, mThis,
                                                                    pc, mCurModule, mNamespaceList));
                            mThis = newThis;
                            mCurModule = target->getByteCode();
                            pc = mCurModule->mCodeBase;
                            endPC = mCurModule->mCodeBase + mCurModule->mLength;
                            mArgumentBase = getBase(argBase);
                            mLocals = new js2val[mCurModule->mLocalsCount];
                            mStack = new js2val[mCurModule->mStackDepth];
                            mStackMax = mCurModule->mStackDepth;
                            mStackTop = 0;
                        }
                        else {
                            js2val result = target->invokeNativeCode(this, newThis, getBase(argBase), 0);
                            resizeStack(stackSize() -  1);
                            pushValue(result);
                        }
                        break;
                    }                    

                    switch (op) {
                    default:
                        NOT_REACHED("bad unary op");
                    case Negate:
                        {
                            popValue();
                            js2val n = JSValue::toNumber(this, v);
                            if (JSValue::isNaN(n))
                                pushValue(n);
                            else
                                pushValue(JSValue::newNumber(-JSValue::f64(n)));
                        }
                        break;
                    case Posate:
                        {
                            popValue();
                            js2val n = JSValue::toNumber(this, v);
                            pushValue(n);
                        }
                        break;
                    case Complement:
                        {
                            popValue();
                            js2val n = JSValue::toInt32(this, v);
                            pushValue(JSValue::newNumber((float64)(~(int32)JSValue::f64(n))));
                        }
                        break;
                    }
                }
                break;
            case DoOperatorOp:
                {
                    Operator op = (Operator)(*pc++);
                    mPC = pc;
                    js2val v1 = getValue(stackSize() - 2);
                    js2val v2 = getValue(stackSize() - 1);
                    if (executeOperator(op, JSValue::getType(v1), JSValue::getType(v2))) {
                        // need to invoke
                        pc = mCurModule->mCodeBase;
                        endPC = mCurModule->mCodeBase + mCurModule->mLength;
                        mLocals = new js2val[mCurModule->mLocalsCount];
                        mStack = new js2val[mCurModule->mStackDepth];
                        mStackMax = mCurModule->mStackDepth;
                        mStackTop = 0;
                    }
                }
                break;
            case GetConstructorOp:
                {
                    js2val v = popValue();
                    ASSERT(JSValue::isType(v));
                    pushValue(JSValue::newFunction(JSValue::type(v)->getDefaultConstructor()));
                }
                break;
            case NewInstanceOp:
                {
                    uint32 argCount = *((uint32 *)pc); 
                    pc += sizeof(uint32);
                    uint32 cleanUp = argCount;
                    js2val *argBase = getBase(stackSize() - argCount);
                    bool isPrototypeFunctionCall = false;

                    JSFunction *target = NULL;
                    js2val newThis = kNullValue;
                    js2val *typeValue = getBase(stackSize() - (argCount + 1));
                    if (!JSValue::isType(*typeValue)) {                        
                        if (JSValue::isFunction(*typeValue) && JSValue::function(*typeValue)->isPrototype()) {
                            isPrototypeFunctionCall = true;
                            target = JSValue::function(*typeValue);
                            newThis = Object_Type->newInstance(this);
                            PropertyIterator i;
                            if (target->hasProperty(this, Prototype_StringAtom, mNamespaceList, Read, &i)) {
                                js2val v = target->getPropertyValue(i);
                                JSValue::object(newThis)->mPrototype = JSValue::newObject(JSValue::toObjectValue(this, v));
                                JSValue::object(newThis)->setProperty(this, UnderbarPrototype_StringAtom, (NamespaceList *)NULL, JSValue::object(newThis)->mPrototype);
                            }
                        }
                        else            
                            reportError(Exception::referenceError, "Not a type or a prototype function");
                    }
                    else {
                        // if the type has an operator "new" use that, 
                        // otherwise use the default constructor (and pass NULL
                        // for the this value)
                        target = getUnaryOperator(JSValue::type(*typeValue), New);
                        if (target)
                            newThis = JSValue::type(*typeValue)->newInstance(this);
                        else {
                            newThis = kNullValue;
                            target = JSValue::type(*typeValue)->getDefaultConstructor();
                        }
                    }                    
                    ASSERT(target);

                    js2val result;
                    if (target->isNative()) {
                        js2val *tArgBase = new js2val[argCount];
                        for (uint32 i = 0; i < argCount; i++)
                            tArgBase[i] = argBase[i];
                        resizeStack(stackSize() - cleanUp);
                        result = target->invokeNativeCode(this, newThis, tArgBase, argCount);
                    }
                    else {
                        argBase = buildArgumentBlock(target, argCount);
                        resizeStack(stackSize() - cleanUp);
                        if (!target->isChecked()) {
                            js2val args = Array_Type->newInstance(this);
                            for (uint32 i = 0; i < argCount; i++) 
                                JSValue::instance(args)->setProperty(this, *numberToString(i), NULL, argBase[i]);
                            target->getActivation()->setProperty(this, Arguments_StringAtom, NULL, args);
                        }
                        try {
                            result = interpret(target->getByteCode(), 0, target->getScopeChain(), newThis, argBase, argCount);
                        }
                        catch (Exception &x) {
                            delete[] argBase;
                            throw x;
                        }
                    }
                    
                    if (isPrototypeFunctionCall) {
                        // If it's a prototype function, the return value is only
                        // interesting if it's not a primitive, in which case it
                        // overrides the newly constructed object. Weird, huh.
                        if (!JSValue::isPrimitive(result))
                            newThis = result;
                    }
                    else
                        // otherwise, constructor has potentially made the 'this', so retain it
                        newThis = result;

                    popValue();             // don't need the type anymore
                    pushValue(newThis);
                 }
                break;
            case NewThisOp:
                {
                    js2val v = popValue();
                    if (JSValue::isNull(mThis)) {
                        ASSERT(JSValue::isType(v));
                        mThis = JSValue::type(v)->newInstance(this);
                    }
                }
                break;
            case NewObjectOp:
                {
                    pushValue(Object_Type->newInstance(this));
                }
                break;
            case GetLocalVarOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(mLocals[index]);
                }
                break;
            case SetLocalVarOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    js2val v = topValue();
                    if (JSValue::isFunction(v) && JSValue::function(v)->hasBoundThis() && !JSValue::function(v)->isMethod()) {
                        v = JSValue::newFunction(JSValue::function(v)->getFunction());
                    }
                    mLocals[index] = v;
                }
                break;
            case GetClosureVarOp:
                {
                    uint32 depth = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
//                    pushValue(mScopeChain->getClosureVar(depth, index));                    
                }
                break;
            case SetClosureVarOp:
                {
                    js2val v = topValue();
                    if (JSValue::isFunction(v) && JSValue::function(v)->hasBoundThis() && !JSValue::function(v)->isMethod()) {
                        v = JSValue::newFunction(JSValue::function(v)->getFunction());
                    }
                    uint32 depth = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
//                    mScopeChain->setClosureVar(depth, index, topValue()));
                }
                break;
            case NewClosureOp:
                {
                }
                break;
            case LoadThisOp:
                {
                    pushValue(mThis);
                }
                break;
            case GetArgOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(mArgumentBase[index]);
                }
                break;
            case SetArgOp:
                {
                    js2val v = topValue();
                    if (JSValue::isFunction(v) && JSValue::function(v)->hasBoundThis() && !JSValue::function(v)->isMethod()) {
                        v = JSValue::newFunction(JSValue::function(v)->getFunction());
                    }
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    mArgumentBase[index] = v;
                }
                break;
            case GetMethodOp:
                {
                    js2val base = topValue();
                    ASSERT(JSValue::isInstance(base));
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(JSValue::newInstance(JSValue::instance(base)->mType->mMethods[index]));
                }
                break;
            case GetMethodRefOp:
                {
                    js2val base = popValue();
                    if (!JSValue::isInstance(base))
                        reportError(Exception::semanticError, "Illegal method reference");
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(JSValue::newFunction(new JSBoundFunction(this, JSValue::instance(base)->mType->mMethods[index], JSValue::object(base))));
                }
                break;
            case GetFieldOp:
                {
                    js2val base = popValue();
                    ASSERT(JSValue::isInstance(base));
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(JSValue::instance(base)->mInstanceValues[index]);
                }
                break;
            case SetFieldOp:
                {
                    js2val v = popValue();
                    if (JSValue::isFunction(v) && JSValue::function(v)->hasBoundThis() && !JSValue::function(v)->isMethod()) {
                        v = JSValue::newFunction(JSValue::function(v)->getFunction());
                    }
                    js2val base = popValue();
                    ASSERT(JSValue::isInstance(base));
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    JSValue::instance(base)->mInstanceValues[index] = v;
                    pushValue(v);
                }
                break;
            case WithinOp:
                {
                    js2val base = popValue();
                    mScopeChain->addScope(JSValue::toObjectValue(this, base));
                }
                break;
            case WithoutOp:
                {
                    mScopeChain->popScope();
                }
                break;
            case PushScopeOp:
                {
                    JSObject *obj = *((JSObject **)pc);
                    mScopeChain->addScope(obj);
                    pc += sizeof(JSObject *);
                }
                break;
            case PopScopeOp:
                {
                    mScopeChain->popScope();
                }
                break;
            case LoadGlobalObjectOp:
                {
                    pushValue(JSValue::newObject(getGlobalObject()));
                }
                break;
            case JsrOp:
                {
                    uint32 offset = *((uint32 *)pc);
                    mSubStack.push(pc + sizeof(uint32));
                    pc += offset;
                }
                break;
            case RtsOp:
                {
                    pc = mSubStack.top();
                    mSubStack.pop();
                }
                break;

            case TryOp:
                {
                    Activation *curAct = (mActivationStack.size() > 0) ? mActivationStack.top() : NULL;
                    uint32 handler = *((uint32 *)pc);
                    if (handler != toUInt32(-1))
                        mTryStack.push(new HandlerData(pc + handler, stackSize(), curAct));
                    pc += sizeof(uint32);
                    handler = *((uint32 *)pc);
                    if (handler != toUInt32(-1))
                        mTryStack.push(new HandlerData(pc + handler, stackSize(), curAct));
                    pc += sizeof(uint32);
                }
                break;
            case HandlerOp:
                {
                    HandlerData *hndlr = (HandlerData *)mTryStack.top();
                    mTryStack.pop();
                    delete hndlr;
                }
                break;
            case ThrowOp:
                {   
                    throw Exception(Exception::userException, "");
                }
                break;
            case JuxtaposeOp:
                {
                    js2val v2 = popValue();
                    js2val v1 = popValue();

                    ASSERT(JSValue::isAttribute(v1));
                    ASSERT(JSValue::isAttribute(v2));

                    Attribute *a1 = JSValue::attribute(v1);
                    Attribute *a2 = JSValue::attribute(v2);

                    if ((a1->mTrueFlags & a2->mFalseFlags) != 0)
                        reportError(Exception::semanticError, "Mismatched attributes"); // XXX could supply more detail
                    if ((a1->mFalseFlags & a2->mTrueFlags) != 0)
                        reportError(Exception::semanticError, "Mismatched attributes");

                    
                    // Now build the result attribute and set it's values
                    Attribute *x = new Attribute(a1->mTrueFlags | a2->mTrueFlags, a1->mFalseFlags | a2->mFalseFlags);

                    NamespaceList *t = a1->mNamespaceList;
                    while (t) {
                        x->mNamespaceList = new NamespaceList(t->mName, x->mNamespaceList);
                        t = t->mNext;
                    }
                    t = a2->mNamespaceList;
                    while (t) {
                        x->mNamespaceList = new NamespaceList(t->mName, x->mNamespaceList);
                        t = t->mNext;
                    }
                    if (a1->mExtendArgument)
                        x->mExtendArgument = a1->mExtendArgument;
                    else
                        if (a2->mExtendArgument)
                            x->mExtendArgument = a2->mExtendArgument;
                    
                    pushValue(JSValue::newAttribute(x));
                }
                break;
            case CastOp:
                {
                    js2val t = popValue();
                    ASSERT(JSValue::isType(t));
                    JSType *toType = JSValue::type(t);
                    js2val v = popValue();

                    pushValue(mapValueToType(v, toType));
                }
                break;
            case UseOnceOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const StringAtom &nameSpace = *mCurModule->getString(index);
                    ASSERT(useOnceNamespace == false);
                    useOnceNamespace = true;
                    mNamespaceList = new NamespaceList(nameSpace, mNamespaceList);
                }
                break;
            case UseOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const StringAtom &nameSpace = *mCurModule->getString(index);
                    mNamespaceList = new NamespaceList(nameSpace, mNamespaceList);
                }
                break;

            default:
                reportError(Exception::internalError, "Bad Opcode");
            }
        }
        catch (Exception &jsx) {
            if (mTryStack.size() > 0) {
                HandlerData *hndlr = (HandlerData *)mTryStack.top();
                Activation *curAct = (mActivationStack.size() > 0) ? mActivationStack.top() : NULL;
                
                js2val x;
                if (curAct != hndlr->mActivation) {
                    ASSERT(mActivationStack.size() > 0);
                    Activation *prev;// = mActivationStack.top();
                    do {
                        prev = curAct;
                        if (prev->mPC == NULL) {
                            // Yikes! the exception is getting thrown across a re-invocation
                            // of the interpreter loop.
                            throw jsx;
                        }
                        mActivationStack.pop();
                        curAct = mActivationStack.top();                            
                    } while (hndlr->mActivation != curAct);
                    if (jsx.hasKind(Exception::userException))  // snatch the exception before the stack gets clobbered
                        x = popValue();
                    mNamespaceList = prev->mNamespaceList;
                    mCurModule = prev->mModule;
                    endPC = mCurModule->mCodeBase + mCurModule->mLength;
                    mLocals = prev->mLocals;
                    mStack = prev->mStack;
                    mStackMax = mCurModule->mStackDepth;
                    mArgumentBase = prev->mArgumentBase;
                    mThis = prev->mThis;
                }
                else {
                    if (jsx.hasKind(Exception::userException))
                        x = popValue();
                }

                // make sure there's a JS object for the catch clause to work with
                if (!jsx.hasKind(Exception::userException)) {
                    js2val argv[1];
                    argv[0] = JSValue::newString(new String(jsx.fullMessage()));
                    switch (jsx.kind) {
                    case Exception::syntaxError:
                        x = SyntaxError_Constructor(this, kNullValue, argv, 1);
                        break;
                    case Exception::referenceError:
                        x = ReferenceError_Constructor(this, kNullValue, argv, 1);
                        break;
                    case Exception::typeError:
                        x = TypeError_Constructor(this, kNullValue, argv, 1);
                        break;
                    case Exception::rangeError:
                        x = RangeError_Constructor(this, kNullValue, argv, 1);
                        break;
                    default:
                        x = Error_Constructor(this, kNullValue, argv, 1);
                        break;
                    }
                }
                
                resizeStack(hndlr->mStackSize);
                pc = hndlr->mPC;
                pushValue(x);
            }
            else
                throw jsx; //reportError(Exception::uncaughtError, "No handler for throw");
        }
    }
    return result;
}

// called on a JSValue to return a base object. The value may be a type, function, instance
// or generic object
JSObject *JSValue::getObjectValue(js2val v)
{
    ASSERT(isObject(v));
    return object(v);
}

// called on a JSValue with type == Number_Type, which could be either a tagged
// value or a Number object (actually an instance). Return the raw value in either case.
float64 JSValue::getNumberValue(js2val v)
{
    if (JSValue::isNumber(v))
        return JSValue::f64(v);
    ASSERT(JSValue::isInstance(v) && (JSValue::getType(v) == Number_Type));
    JSNumberInstance *numInst = checked_cast<JSNumberInstance *>(JSValue::instance(v));
    return numInst->mValue;
}

// called on a JSValue with type == String_Type, which could be either a tagged
// value or a String object (actually an instance). Return the raw value in either case.
const String *JSValue::getStringValue(js2val v)
{
    if (JSValue::isString(v))
        return JSValue::string(v);
    ASSERT(JSValue::isInstance(v) && (JSValue::getType(v) == String_Type));
    JSStringInstance *strInst = checked_cast<JSStringInstance *>(JSValue::instance(v));
    return strInst->mValue;
}

// called on a JSValue with type == Boolean_Type, which could be either a tagged
// value or a Boolean object (actually an instance). Return the raw value in either case.
bool JSValue::getBoolValue(js2val v)
{
    if (JSValue::isBool(v))
        return JSValue::boolean(v);
    ASSERT(JSValue::isInstance(v) && (JSValue::getType(v) == Boolean_Type));
    JSBooleanInstance *boolInst = checked_cast<JSBooleanInstance *>(JSValue::instance(v));
    return boolInst->mValue;
}

// Implementation of '+' for two number types
static js2val numberPlus(Context *, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    return JSValue::newNumber(JSValue::getNumberValue(argv[0]) + JSValue::getNumberValue(argv[1]));
}

// Implementation of '+' for two integer types
static js2val integerPlus(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    return JSValue::newNumber(JSValue::getNumberValue(argv[0]) + JSValue::getNumberValue(argv[1]));
}

// Implementation of '+' for two 'generic' objects
static js2val objectPlus(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val &r1 = argv[0];
    js2val &r2 = argv[1];
    JSType *t1 = JSValue::getType(r1);
    JSType *t2 = JSValue::getType(r2);

    if ((t1 == Number_Type) && (t2 == Number_Type)) {        
        return JSValue::newNumber(JSValue::getNumberValue(r1) + JSValue::getNumberValue(r2));
    }

    if (t1 == String_Type) {
        if (t2 == String_Type)
            return JSValue::newString(new String(*JSValue::getStringValue(r1) + *JSValue::getStringValue(r2)));
        else
            return JSValue::newString(new String(*JSValue::getStringValue(r1) + *JSValue::string(JSValue::toString(cx, r2))));
    }
    else {
        if (t2 == String_Type) 
            return JSValue::newString(new String(*JSValue::string(JSValue::toString(cx, r1)) + *JSValue::getStringValue(r2)));
        else {
            js2val r1p = JSValue::toPrimitive(cx, r1);
            js2val r2p = JSValue::toPrimitive(cx, r2);
            // gar-on-teed tagged values now
            if (JSValue::isString(r1p))
                if (JSValue::isString(r2p))
                    return JSValue::newString(new String(*JSValue::string(r1p) + *JSValue::string(r2p)));
                else
                    return JSValue::newString(new String(*JSValue::string(r1p) + *JSValue::string(JSValue::toString(cx, r2p))));
            else
                if (JSValue::isString(r2p))
                    return JSValue::newString(new String(*JSValue::string(JSValue::toString(cx, r1p)) + *JSValue::string(r2p)));
                else {
                    js2val num1(JSValue::toNumber(cx, r1));
                    js2val num2(JSValue::toNumber(cx, r2));
                    return JSValue::newNumber(JSValue::f64(num1) + JSValue::f64(num2));
                }
        }
    }
}



// Implementation of '-' for two integer types
static js2val integerMinus(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    return JSValue::newNumber(JSValue::getNumberValue(argv[0]) - JSValue::getNumberValue(argv[1]));
}

// Implementation of '-' for two number types
static js2val numberMinus(Context *, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    return JSValue::newNumber(JSValue::getNumberValue(argv[0]) - JSValue::getNumberValue(argv[1]));
}

// Implementation of '-' for two 'generic' objects
static js2val objectMinus(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toNumber(cx, argv[0]);
    js2val r2 = JSValue::toNumber(cx, argv[1]);
    return JSValue::newNumber(JSValue::f64(r1) - JSValue::f64(r2));
}



// Implementation of '*' for two integer types
static js2val integerMultiply(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    return JSValue::newNumber(JSValue::getNumberValue(argv[0]) * JSValue::getNumberValue(argv[1]));
}

// Implementation of '*' for two 'generic' objects
static js2val objectMultiply(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toNumber(cx, argv[0]);
    js2val r2 = JSValue::toNumber(cx, argv[1]);
    return JSValue::newNumber(JSValue::f64(r1) * JSValue::f64(r2));
}


// Implementation of '/' for two integer types
static js2val integerDivide(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    float64 f1 = JSValue::getNumberValue(argv[0]);
    float64 f2 = JSValue::getNumberValue(argv[1]);
    float64 d = f1 / f2;
    bool neg = (d < 0);
    d = fd::floor(neg ? -d : d);
    d = neg ? -d : d;
    return JSValue::newNumber(d);
}

// Implementation of '/' for two 'generic' objects
static js2val objectDivide(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toNumber(cx, argv[0]);
    js2val r2 = JSValue::toNumber(cx, argv[1]);
    return JSValue::newNumber(JSValue::f64(r1) / JSValue::f64(r2));
}


// Implementation of '%' for two integer types
static js2val integerRemainder(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    float64 f1 = JSValue::getNumberValue(argv[0]);
    float64 f2 = JSValue::getNumberValue(argv[1]);
    float64 d = fd::fmod(f1, f2);
    bool neg = (d < 0);
    d = fd::floor(neg ? -d : d);
    d = neg ? -d : d;
    return JSValue::newNumber(d);
}

// Implementation of '%' for two 'generic' objects
static js2val objectRemainder(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toNumber(cx, argv[0]);
    js2val r2 = JSValue::toNumber(cx, argv[1]);

    float64 f1 = JSValue::f64(r1);
    float64 f2 = JSValue::f64(r2);

#ifdef XP_PC
    /* Workaround MS fmod bug where 42 % (1/0) => NaN, not 42. */
    if (JSDOUBLE_IS_FINITE(f1) && JSDOUBLE_IS_INFINITE(f2))
        return JSValue::newNumber(f1);
#endif

    return JSValue::newNumber(fd::fmod(f1, f2));
}


// Implementation of '<<' for two integer types
static js2val integerShiftLeft(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    float64 f1 = JSValue::getNumberValue(argv[0]);
    float64 f2 = JSValue::getNumberValue(argv[1]);
    return JSValue::newNumber((float64)( (int32)(f1) << ( (uint32)(f2) & 0x1F)) );
}

// Implementation of '<<' for two 'generic' objects
static js2val objectShiftLeft(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toInt32(cx, argv[0]);
    js2val r2 = JSValue::toUInt32(cx, argv[1]);
    return JSValue::newNumber((float64)( (int32)(JSValue::f64(r1)) << ( (uint32)(JSValue::f64(r2)) & 0x1F)) );
}



// Implementation of '>>' for two integer types
static js2val integerShiftRight(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    float64 f1 = JSValue::getNumberValue(argv[0]);
    float64 f2 = JSValue::getNumberValue(argv[1]);
    return JSValue::newNumber((float64) ( (int32)(f1) >> ( (uint32)(f2) & 0x1F)) );
}

// Implementation of '>>' for two 'generic' objects
static js2val objectShiftRight(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toInt32(cx, argv[0]);
    js2val r2 = JSValue::toUInt32(cx, argv[1]);
    return JSValue::newNumber((float64) ( (int32)(JSValue::f64(r1)) >> ( (uint32)(JSValue::f64(r2)) & 0x1F)) );
}



// Implementation of '>>>' for two integer types
static js2val integerUShiftRight(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    float64 f1 = JSValue::getNumberValue(argv[0]);
    float64 f2 = JSValue::getNumberValue(argv[1]);
    return JSValue::newNumber((float64) ( (uint32)(f1) >> ( (uint32)(f2) & 0x1F)) );
}

// Implementation of '>>>' for two 'generic' objects
static js2val objectUShiftRight(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toUInt32(cx, argv[0]);
    js2val r2 = JSValue::toUInt32(cx, argv[1]);
    return JSValue::newNumber((float64) ( (uint32)(JSValue::f64(r1)) >> ( (uint32)(JSValue::f64(r2)) & 0x1F)) );
}


// Implementation of '&' for two integer types
static js2val integerBitAnd(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    float64 f1 = JSValue::getNumberValue(argv[0]);
    float64 f2 = JSValue::getNumberValue(argv[1]);
    return JSValue::newNumber((float64)( (int32)(f1) & (int32)(f2) ));
}

// Implementation of '&' for two 'generic' objects
static js2val objectBitAnd(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toInt32(cx, argv[0]);
    js2val r2 = JSValue::toInt32(cx, argv[1]);
    return JSValue::newNumber((float64)( (int32)(JSValue::f64(r1)) & (int32)(JSValue::f64(r2)) ));
}



// Implementation of '^' for two integer types
static js2val integerBitXor(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    float64 f1 = JSValue::getNumberValue(argv[0]);
    float64 f2 = JSValue::getNumberValue(argv[1]);
    return JSValue::newNumber((float64)( (int32)(f1) ^ (int32)(f2) ));
}

// Implementation of '^' for two 'generic' objects
static js2val objectBitXor(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toInt32(cx, argv[0]);
    js2val r2 = JSValue::toInt32(cx, argv[1]);
    return JSValue::newNumber((float64)( (int32)(JSValue::f64(r1)) ^ (int32)(JSValue::f64(r2)) ));
}



// Implementation of '|' for two integer types
static js2val integerBitOr(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    float64 f1 = JSValue::getNumberValue(argv[0]);
    float64 f2 = JSValue::getNumberValue(argv[1]);
    return JSValue::newNumber((float64)( (int32)(f1) | (int32)(f2) ));
}

// Implementation of '|' for two 'generic' objects
static js2val objectBitOr(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = JSValue::toInt32(cx, argv[0]);
    js2val r2 = JSValue::toInt32(cx, argv[1]);
    return JSValue::newNumber((float64)( (int32)(JSValue::f64(r1)) | (int32)(JSValue::f64(r2)) ));
}



//
// implements r1 < r2, returning true or false or undefined
//
static js2val objectCompare(Context *cx, js2val &r1, js2val &r2)
{
    js2val r1p = JSValue::toPrimitive(cx, r1, JSValue::NumberHint);
    js2val r2p = JSValue::toPrimitive(cx, r2, JSValue::NumberHint);

    if (JSValue::isString(r1p) && JSValue::isString(r2p))
        return JSValue::newBoolean(bool(JSValue::string(r1p)->compare(*JSValue::string(r2p)) < 0));
    else {
        js2val r1n = JSValue::toNumber(cx, r1p);
        js2val r2n = JSValue::toNumber(cx, r2p);
        if (JSValue::isNaN(r1n) || JSValue::isNaN(r2n))
            return kUndefinedValue;
        else
            return JSValue::newNumber(JSValue::f64(r1n) < JSValue::f64(r2n));
    }

}

static js2val objectLess(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val result = objectCompare(cx, argv[0], argv[1]);
    if (JSValue::isUndefined(result))
        return kFalseValue;
    else
        return result;
}

static js2val objectLessEqual(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val result = objectCompare(cx, argv[1], argv[0]);
    if (JSValue::isUndefined(result) || JSValue::isTrue(result))
        return kFalseValue;
    else
        return kTrueValue;
}

// Compare two values for '=='
static js2val compareEqual(Context *cx, js2val r1, js2val r2)
{
    JSType *t1 = JSValue::getType(r1);
    JSType *t2 = JSValue::getType(r2);

    if (t1 != t2) {
        if (JSValue::isNull(r1) && JSValue::isUndefined(r2))
            return kTrueValue;
        if (JSValue::isUndefined(r1) && JSValue::isNull(r2))
            return kTrueValue;
        if ((t1 == Number_Type) && (t2 == String_Type))
            return compareEqual(cx, r1, JSValue::toNumber(cx, r2));
        if ((t1 == String_Type) && (t2 == Number_Type))
            return compareEqual(cx, JSValue::toNumber(cx, r1), JSValue::toString(cx, r2));
        if (t1 == Boolean_Type)
            return compareEqual(cx, JSValue::toNumber(cx, r1), r2);
        if (t2 == Boolean_Type)
            return compareEqual(cx, r1, JSValue::toNumber(cx, r1));
        if ( ((t1 == String_Type) || (t1 == Number_Type)) && JSValue::isObject(r2) )
            return compareEqual(cx, r1, JSValue::toPrimitive(cx, r2));
        if ( (JSValue::isObject(r1)) && ((t2 == String_Type) || (t2 == Number_Type)) )
            return compareEqual(cx, JSValue::toPrimitive(cx, r1), r2);
        return kFalseValue;
    }
    else {
        if (JSValue::isUndefined(r1))
            return kTrueValue;
        if (JSValue::isNull(r1))
            return kTrueValue;
        if (JSValue::isInstance(r1) && JSValue::isInstance(r2)) // because new Boolean()->getType() == Boolean_Type
            return JSValue::newBoolean(JSValue::instance(r1) == JSValue::instance(r2));
        if (JSValue::isObject(r1) && JSValue::isObject(r2))
            return JSValue::newBoolean(JSValue::object(r1) == JSValue::object(r2));
        if (JSValue::isPackage(r1) && JSValue::isPackage(r2))
            return JSValue::newBoolean(JSValue::package(r1) == JSValue::package(r2));
        if (JSValue::isType(r1))
            return JSValue::newBoolean(JSValue::type(r1) == JSValue::type(r2));
        if (JSValue::isFunction(r1))
            return JSValue::newBoolean(JSValue::function(r1)->isEqual(JSValue::function(r2)));
        if (t1 == Number_Type) {
            
            float64 f1 = JSValue::getNumberValue(r1);
            float64 f2 = JSValue::getNumberValue(r2);

            if (JSDOUBLE_IS_NaN(f1))
                return kFalseValue;
            if (JSDOUBLE_IS_NaN(f2))
                return kFalseValue;

            return JSValue::newBoolean(JSValue::f64(r1) == JSValue::f64(r2));
        }
        else {
            if (t1 == String_Type)
                return JSValue::newBoolean(bool(JSValue::getStringValue(r1)->compare(*JSValue::getStringValue(r2)) == 0));
            if (t1 == Boolean_Type)
                return JSValue::newBoolean(JSValue::getBoolValue(r1) == JSValue::getBoolValue(r2));
            NOT_REACHED("unhandled type");
            return kFalseValue;
        }
    }
}

static js2val objectEqual(Context *cx, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    return compareEqual(cx, argv[0], argv[1]);
}

static js2val objectSpittingImage(Context * /*cx*/, const js2val /*thisValue*/, js2val argv[], uint32 /*argc*/)
{
    js2val r1 = argv[0];
    js2val r2 = argv[1];
    
    JSType *t1 = JSValue::getType(r1);
    JSType *t2 = JSValue::getType(r2);

    if ((t1 != t2) || (JSValue::isObject(r1) != JSValue::isObject(r2))) {
        return kFalseValue;
    }
    else {
        if (JSValue::isUndefined(r1))
            return kTrueValue;
        if (JSValue::isNull(r1))
            return kTrueValue;
        if (JSValue::isObject(r1) && JSValue::isObject(r2)) // because new Boolean()->getType() == Boolean_Type
            return JSValue::newBoolean(JSValue::object(r1) == JSValue::object(r2));
        if (JSValue::isType(r1))
            return JSValue::newBoolean(JSValue::type(r1) == JSValue::type(r2));
        if (JSValue::isFunction(r1))
            return JSValue::newBoolean(JSValue::function(r1)->isEqual(JSValue::function(r2)));
        if (t1 == Number_Type) {
            
            float64 f1 = JSValue::getNumberValue(r1);
            float64 f2 = JSValue::getNumberValue(r2);

            if (JSDOUBLE_IS_NaN(f1))
                return kFalseValue;
            if (JSDOUBLE_IS_NaN(f2))
                return kFalseValue;

            return JSValue::newBoolean(JSValue::f64(r1) == JSValue::f64(r2));
        }
        else {
            if (t1 == String_Type)
                return JSValue::newBoolean(bool(JSValue::getStringValue(r1)->compare(*JSValue::getStringValue(r2)) == 0));
            if (JSValue::isBool(r1) && JSValue::isBool(r2))
                return JSValue::newBoolean(JSValue::getBoolValue(r1) == JSValue::getBoolValue(r2));
            return kFalseValue;
        }
    }
}


// Build the default state of the binary operator table
void Context::initOperators()
{
    struct OpTableEntry {
        Operator which;
        JSType *op1;
        JSType *op2;
        JSFunction::NativeCode *imp;
        JSType *resType;
    } OpTable[] = {
        { Plus,  Object_Type, Object_Type, objectPlus,  Object_Type },
        { Plus,  Number_Type, Number_Type, numberPlus,  Number_Type },
        { Plus,  Integer_Type, Integer_Type, integerPlus,  Integer_Type },

        { Minus, Object_Type, Object_Type, objectMinus, Number_Type },
        { Minus, Number_Type, Number_Type, numberMinus, Number_Type },
        { Minus,  Integer_Type, Integer_Type, integerMinus,  Integer_Type },

        { ShiftLeft, Integer_Type, Integer_Type, integerShiftLeft, Integer_Type },
        { ShiftLeft, Object_Type, Object_Type, objectShiftLeft, Number_Type },

        { ShiftRight, Integer_Type, Integer_Type, integerShiftRight, Integer_Type },
        { ShiftRight, Object_Type, Object_Type, objectShiftRight, Number_Type },

        { UShiftRight, Integer_Type, Integer_Type, integerUShiftRight, Integer_Type },
        { UShiftRight, Object_Type, Object_Type, objectUShiftRight, Number_Type },
        
        { BitAnd, Integer_Type, Integer_Type, integerBitAnd, Integer_Type },
        { BitAnd, Object_Type, Object_Type, objectBitAnd, Number_Type },
        
        { BitXor, Integer_Type, Integer_Type, integerBitXor, Integer_Type },
        { BitXor, Object_Type, Object_Type, objectBitXor, Number_Type },

        { BitOr, Integer_Type, Integer_Type, integerBitOr, Integer_Type },
        { BitOr, Object_Type, Object_Type, objectBitOr, Number_Type },

        { Multiply, Integer_Type, Integer_Type, integerMultiply, Integer_Type },
        { Multiply, Object_Type, Object_Type, objectMultiply, Number_Type },
        
        { Divide, Integer_Type, Integer_Type, integerDivide, Integer_Type },
        { Divide, Object_Type, Object_Type, objectDivide, Number_Type },

        { Remainder, Integer_Type, Integer_Type, integerRemainder, Integer_Type },
        { Remainder, Object_Type, Object_Type, objectRemainder, Number_Type },

        { Less, Object_Type, Object_Type, objectLess, Boolean_Type },
        { LessEqual, Object_Type, Object_Type, objectLessEqual, Boolean_Type },

        { Equal, Object_Type, Object_Type, objectEqual, Boolean_Type },

        { SpittingImage, Object_Type, Object_Type, objectSpittingImage, Boolean_Type }

    };

    for (uint32 i = 0; i < sizeof(OpTable) / sizeof(OpTableEntry); i++) {
        JSFunction *f = new JSFunction(this, OpTable[i].imp, OpTable[i].resType, NULL);
        OperatorDefinition *op = new OperatorDefinition(OpTable[i].op1, OpTable[i].op2, f);
        mBinaryOperatorTable[OpTable[i].which].push_back(op);
    }
}

// Convert a primitive value into an object value, this is a no-op for
// values that are already object-like.

// XXX for Waldemar - is it possible to modify behaviour for Number, Boolean etc
// so that the behaviour here of constructing a new instance of those types would
// be different?

js2val JSValue::valueToObject(Context *cx, const js2val value)
{
    switch (tag(value)) {
    case JS2VAL_DOUBLE:
        {
            js2val result = Number_Type->newInstance(cx);
            ((JSNumberInstance *)JSValue::instance(result))->mValue = JSValue::f64(value);
            return result;
        }
    case JS2VAL_BOOLEAN:
        {
            js2val result = Boolean_Type->newInstance(cx);
            ((JSBooleanInstance *)JSValue::instance(result))->mValue = JSValue::boolean(value);
            return result;
        }
    case JS2VAL_STRING: 
        {
            js2val result = String_Type->newInstance(cx);
            ((JSStringInstance *)JSValue::instance(result))->mValue = JSValue::string(value);
            return result;
        }
    case JS2VAL_OBJECT:
        if (isNull(value))
            cx->reportError(Exception::typeError, "converting null to object");
        return value;
    case JS2VAL_INT:
        cx->reportError(Exception::typeError, "converting undefined to object");
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
}

// Convert a value to an object, without constructing a new value
// unless absolutely necessary, which would be the case for using
// valueToObject().getObjectValue() instead.

// XXX for Waldemar - is it possible to modify behaviour for Number, Boolean etc
// so that the behaviour here of constructing a new instance of those types would
// be different?


JSObject *JSValue::toObjectValue(Context *cx, const js2val v)
{
    switch (tag(v)) {
    case JS2VAL_DOUBLE:
        {
            js2val result = Number_Type->newInstance(cx);
            ((JSNumberInstance *)JSValue::instance(result))->mValue = JSValue::f64(v);
            return JSValue::instance(result);
        }
    case JS2VAL_BOOLEAN:
        {
            js2val result = Boolean_Type->newInstance(cx);
            ((JSBooleanInstance *)JSValue::instance(result))->mValue = JSValue::boolean(v);
            return JSValue::instance(result);
        }
    case JS2VAL_STRING: 
        {
            js2val result = String_Type->newInstance(cx);
            ((JSStringInstance *)JSValue::instance(result))->mValue = JSValue::string(v);
            return JSValue::instance(result);
        }
    case JS2VAL_OBJECT:
        if (isNull(v))
            cx->reportError(Exception::typeError, "converting null to object");
        return JSValue::object(v);
    case JS2VAL_INT:
        cx->reportError(Exception::typeError, "converting undefined to object");
    default:
        NOT_REACHED("Bad tag");
        return NULL;
    }
}

// convert string to double - a wrapper around the numerics routine
// to handle hex literals as well
float64 stringToNumber(const String *string)
{
    const char16 *numEnd;
    const char16 *sBegin = string->begin();
    if (sBegin)
        if ((sBegin[0] == '0') && ((sBegin[1] & ~0x20) == 'X'))
            return stringToInteger(sBegin, string->end(), numEnd, 16);
        else {
            float64 result = stringToDouble(sBegin, string->end(), numEnd);
            if (numEnd != string->end()) {
                const char16 *sEnd = string->end();
                while (numEnd != sEnd) {
                    if (!isSpace(*numEnd++))
                        return nan;
                }
                return result;
            }
            else
                return result;
        }
    else
        return 0.0;
}

// Convert a JSValue to number-type
js2val JSValue::valueToNumber(Context *cx, const js2val value)
{
    switch (tag(value)) {
    case JS2VAL_DOUBLE:
        return value;
    case JS2VAL_STRING: 
        return JSValue::newNumber(stringToNumber(JSValue::string(value)));
    case JS2VAL_OBJECT:
        if (isNull(value))
            return kPositiveZero;
        else
            return JSValue::toNumber(cx, JSValue::toPrimitive(cx, value, NumberHint));
    case JS2VAL_BOOLEAN:
        return JSValue::newNumber((JSValue::boolean(value)) ? 1.0 : 0.0);
    case JS2VAL_INT:
        return kNaNValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
}

// Convert a double to a string representation (base-10)
const String *numberToString(float64 number)
{
    char buf[dtosStandardBufferSize];
    const char *chrp = doubleToStr(buf, dtosStandardBufferSize, number, dtosStandard, 0);
    return new JavaScript::String(widenCString(chrp));
}

// Convert a JSValue to a string (ECMA rules)
js2val JSValue::valueToString(Context *cx, const js2val value)
{
    const String *strp = NULL;
    JSObject *obj = NULL;
    switch (tag(value)) {
    case JS2VAL_DOUBLE:
        return JSValue::newString(numberToString(JSValue::f64(value)));
    case JS2VAL_OBJECT:
        if (isNull(value))
            strp = &cx->Null_StringAtom;
        else
            obj = JSValue::object(value);
        break;
    case JS2VAL_STRING:
        return value;
    case JS2VAL_BOOLEAN:
        strp = (JSValue::boolean(value)) ? &cx->True_StringAtom : &cx->False_StringAtom;
        break;
    case JS2VAL_INT:
        strp = &cx->Undefined_StringAtom;
        break;
    default:
        NOT_REACHED("Bad tag");
    }
    if (obj) {
        JSFunction *target = NULL;
        PropertyIterator i;
        if (obj->hasProperty(cx, cx->ToString_StringAtom, NULL, Read, &i)) {
            js2val v = obj->getPropertyValue(i);
            if (JSValue::isFunction(v))
                target = JSValue::function(v);
        }
        if (target == NULL) {
            if (obj->hasProperty(cx, cx->ValueOf_StringAtom, NULL, Read, &i)) {
                js2val v = obj->getPropertyValue(i);
                if (JSValue::isFunction(v))
                    target = JSValue::function(v);
            }
        }
        if (target)
            return cx->invokeFunction(target, value, NULL, 0);
        cx->reportError(Exception::runtimeError, "toString");    // XXX
        return kUndefinedValue; // keep compilers happy
    }
    else
        return JSValue::newString(strp);

}

// Convert a JSValue to a primitive type
js2val JSValue::toPrimitive(Context *cx, const js2val v, Hint hint)
{
    JSObject *obj;
    switch (tag(v)) {
    case JS2VAL_DOUBLE:
    case JS2VAL_STRING:
    case JS2VAL_BOOLEAN:
    case JS2VAL_INT:
        return v;

    case JS2VAL_OBJECT:
        obj = JSValue::object(v);
        if ((hint == NoHint) && (getType(v) == Date_Type))
            hint = StringHint;
        break;

    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }

    // The following is [[DefaultValue]]
    //
    ASSERT(obj);
    JSFunction *target = NULL;
    js2val result;
    PropertyIterator i;

    StringAtom *first = &cx->ValueOf_StringAtom;
    StringAtom *second = &cx->ToString_StringAtom;

    if (hint == StringHint) {
        first = &cx->ToString_StringAtom;
        second = &cx->ValueOf_StringAtom;
    }


    if (obj->hasProperty(cx, *first, NULL, Read, &i)) {
        js2val v = obj->getPropertyValue(i);
        if (JSValue::isFunction(v)) {
            target = JSValue::function(v);
            if (target) {
                result = cx->invokeFunction(target, v, NULL, 0);
                if (JSValue::isPrimitive(result))
                    return result;
            }
        }
    }
    if (obj->hasProperty(cx, *second, NULL, Read, &i)) {
        js2val v = obj->getPropertyValue(i);
        if (JSValue::isFunction(v)) {
            target = JSValue::function(v);
            if (target) {
                result = cx->invokeFunction(target, v, NULL, 0);
                if (JSValue::isPrimitive(result))
                    return result;
            }
        }
    }
    cx->reportError(Exception::runtimeError, "toPrimitive");    // XXX
    return kUndefinedValue;
}

/*
int JSValue::operator==(const JSValue& value) const
{
    if (this->tag == value.tag) {
#       define CASE(T) case T##_tag: return (this->T == value.T)
        switch (tag) {
        CASE(f64);
        CASE(object);
        CASE(boolean);
        #undef CASE
        // question:  are all undefined values equal to one another?
        case undefined_tag: return 1;
        default:
            NOT_REACHED("Broken compiler?");            
        }
    }
    return 0;
}
*/
float64 JSValue::float64ToInteger(float64 d)
{
    if (JSDOUBLE_IS_NaN(d))
        return 0.0;
    else
        return (d >= 0.0) ? fd::floor(d) : -fd::floor(-d);
}

js2val JSValue::valueToInteger(Context *cx, const js2val value)
{
    js2val v = valueToNumber(cx, value);
    return JSValue::newNumber(float64ToInteger(JSValue::f64(v)));
}

int32 JSValue::float64ToInt32(float64 d)
{
    d = fd::fmod(d, two32);
    d = (d >= 0) ? d : d + two32;
    if (d >= two31)
        return (int32)(d - two32);
    else
        return (int32)(d);    
}

uint32 JSValue::float64ToUInt32(float64 d)
{
    bool neg = (d < 0);
    d = fd::floor(neg ? -d : d);
    d = neg ? -d : d;
    d = fd::fmod(d, two32);
    d = (d >= 0) ? d : d + two32;
    return (uint32)d;
}

js2val JSValue::valueToInt32(Context *, const js2val value)
{
    float64 d;
    switch (tag(value)) {
    case JS2VAL_DOUBLE:
        d = JSValue::f64(value);
        break;
    case JS2VAL_STRING: 
        {
            const char16 *numEnd;
            const String *str = JSValue::string(value);
            d = stringToDouble(str->begin(), str->end(), numEnd);
        }
        break;
    case JS2VAL_BOOLEAN:
        return JSValue::newNumber((float64)((JSValue::boolean(value)) ? 1 : 0));
    case JS2VAL_OBJECT:
    case JS2VAL_INT:
        // toNumber(toPrimitive(hint Number))
        return kUndefinedValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
    if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d) )
        return JSValue::newNumber((float64)0);
    return JSValue::newNumber((float64)float64ToInt32(d));
}

js2val JSValue::valueToUInt32(Context *, const js2val value)
{
    float64 d;
    switch (tag(value)) {
    case JS2VAL_DOUBLE:
        d = JSValue::f64(value);
        break;
    case JS2VAL_STRING: 
        {
            const char16 *numEnd;
            const String *str = JSValue::string(value);
            d = stringToDouble(str->begin(), str->end(), numEnd);
        }
        break;
    case JS2VAL_BOOLEAN:
        return JSValue::newNumber((float64)((JSValue::boolean(value)) ? 1 : 0));
    case JS2VAL_OBJECT:
    case JS2VAL_INT:
        // toNumber(toPrimitive(hint Number))
        return kUndefinedValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
    if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d))
        return JSValue::newNumber((float64)0);
    return JSValue::newNumber((float64)float64ToUInt32(d));
}

js2val JSValue::valueToUInt16(Context *, const js2val value)
{
    float64 d;
    switch (tag(value)) {
    case JS2VAL_DOUBLE:
        d = JSValue::f64(value);
        break;
    case JS2VAL_STRING: 
        {
            const char16 *numEnd;
            const String *str = JSValue::string(value);
            d = stringToDouble(str->begin(), str->end(), numEnd);
        }
        break;
    case JS2VAL_BOOLEAN:
        return JSValue::newNumber((float64)((JSValue::boolean(value)) ? 1 : 0));
    case JS2VAL_OBJECT:
    case JS2VAL_INT:
        // toNumber(toPrimitive(hint Number))
        return kUndefinedValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
    if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d))
        return JSValue::newNumber((float64)0);
    bool neg = (d < 0);
    d = fd::floor(neg ? -d : d);
    d = neg ? -d : d;
    d = fd::fmod(d, two16);
    d = (d >= 0) ? d : d + two16;
    return JSValue::newNumber((float64)d);
}

js2val JSValue::valueToBoolean(Context * /*cx*/, const js2val value)
{
    switch (tag(value)) {
    case JS2VAL_DOUBLE:
        if (isNaN(value))
            return kFalseValue;
        if (f64(value) == 0.0)
            return kFalseValue;
        return kTrueValue;
    case JS2VAL_STRING: 
        return JSValue::newBoolean(JSValue::string(value)->length() != 0);
    case JS2VAL_BOOLEAN:
        return value;
    case JS2VAL_OBJECT:
        if (isNull(value))
            return kFalseValue;
        else
            return kTrueValue;
    case JS2VAL_INT:
        return kFalseValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
}


// See if 'v' can be represented as a 't' - works for
// the built-ins by falling back to the ECMA3 behaviour
js2val Context::mapValueToType(js2val v, JSType *t)
{
    // user conversions?
    if (JSValue::getType(v) == t)
        return v;
    
    if (t == Number_Type) {
        return JSValue::toNumber(this, v);
    }
    else
    if (t == Integer_Type) {
        js2val fv = JSValue::toNumber(this, v);
        float64 d = JSValue::f64(fv);
        bool neg = (d < 0);
        d = fd::floor(neg ? -d : d);
        d = neg ? -d : d;
        return JSValue::newNumber(d);
    }
    else
    if (t == String_Type) {
        return JSValue::toString(this, v);
    }
    else
    if (t == Boolean_Type) {
        return JSValue::toBoolean(this, v);
    }

    if (JSValue::isUndefined(v))
        return v;

    if (JSValue::getType(v)->derivesFrom(t))
        return v;

    reportError(Exception::typeError, "Invalid type cast");
    return kUndefinedValue;
}

JSType *JSValue::getType(const js2val v) {
    switch (tag(v)) {
    case JS2VAL_DOUBLE:     return Number_Type;
    case JS2VAL_BOOLEAN:    return Boolean_Type;
    case JS2VAL_STRING:     return (JSType *)String_Type;
    case JS2VAL_OBJECT:     if (isNull(v))
                                return Null_Type;
                            else
                                if (isInstance(v))
                                    return JSValue::instance(v)->getType();
                                else
                                    return Object_Type;
    case JS2VAL_INT:        return Void_Type;
    default: 
        NOT_REACHED("bad type"); 
        return NULL;
    }
}


JSFunction *Context::getUnaryOperator(JSType *dispatchType, Operator which)
{
    // look in the operator table for applicable operators
    OperatorList applicableOperators;

    for (OperatorList::iterator oi = mUnaryOperatorTable[which].begin(),
                end = mUnaryOperatorTable[which].end(); 
                    (oi != end); oi++) {        
        if ((*oi)->isApplicable(dispatchType)) {
            applicableOperators.push_back(*oi);
        }
    }
    if (applicableOperators.size() == 0)
        return NULL;   
        
    OperatorList::iterator candidate = applicableOperators.begin();
    for (OperatorList::iterator aoi = applicableOperators.begin() + 1,
                aend = applicableOperators.end();
                    (aoi != aend); aoi++) 
    {
        if ((*aoi)->mType1->derivesFrom((*candidate)->mType1) )
            candidate = aoi;
    }
    
    return (*candidate)->mImp;
}

    
}
}

