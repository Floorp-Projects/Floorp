
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
* Communications Corporation.   Portions created by Netscape are
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
#include "msvc_pragma.h"
#endif

#include <algorithm>
#include <assert.h>
#include <map>
#include <list>
#include <stack>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "jslong.h"
#include "numerics.h"
#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"


namespace JavaScript {
namespace MetaData {


    js2val JS2Metadata::readEvalString(const String &str, const String& fileName)
    {
        js2val result = JS2VAL_VOID;

        Arena a;
        Pragma::Flags flags = Pragma::js1;
        Parser p(world, a, flags, str, fileName);
        CompilationData *oldData = NULL;
        try {
            StmtNode *parsedStatements = p.parseProgram();
            ASSERT(p.lexer.peek(true).hasKind(Token::end));
            if (showTrees)
            {
                PrettyPrinter f(stdOut, 80);
                {
                    PrettyPrinter::Block b(f, 2);
                    f << "Program =";
                    f.linearBreak(1);
                    StmtNode::printStatements(f, parsedStatements);
                }
                f.end();
                stdOut << '\n';
            }
            if (parsedStatements) {
                oldData = startCompilationUnit(NULL, str, fileName);
                ValidateStmtList(parsedStatements);
                result = ExecuteStmtList(RunPhase, parsedStatements);
            }
        }
        catch (Exception &x) {
            if (oldData)
                restoreCompilationUnit(oldData);
            throw x;
        }
        if (oldData)
            restoreCompilationUnit(oldData);
        return result;
    }

    js2val JS2Metadata::readEvalFile(const String& fileName)
    {
        String buffer;
        int ch;

        js2val result = JS2VAL_VOID;

        std::string str(fileName.length(), char());
        std::transform(fileName.begin(), fileName.end(), str.begin(), narrow);
        FILE* f = fopen(str.c_str(), "r");
        if (f) {
            while ((ch = getc(f)) != EOF)
                buffer += static_cast<char>(ch);
            fclose(f);
            result = readEvalString(buffer, fileName);
        }
        return result;
    }

    js2val JS2Metadata::readEvalFile(const char *fileName)
    {
        String buffer;
        int ch;

        js2val result = JS2VAL_VOID;

        FILE* f = fopen(fileName, "r");
        if (f) {
            while ((ch = getc(f)) != EOF)
                buffer += static_cast<char>(ch);
            fclose(f);
            result = readEvalString(buffer, widenCString(fileName));
        }
        return result;
    }

    /*
     * Evaluate the linked list of statement nodes beginning at 'p' 
     * (generate bytecode and then execute that bytecode
     */
    js2val JS2Metadata::ExecuteStmtList(Phase phase, StmtNode *p)
    {
        size_t lastPos = p->pos;
        while (p) {
            SetupStmt(env, phase, p);
            lastPos = p->pos;
            p = p->next;
        }
        bCon->emitOp(eReturnVoid, lastPos);
        uint8 *savePC = engine->pc;
        engine->pc = NULL;
        js2val retval = engine->interpret(phase, bCon, env);
        engine->pc = savePC;
        return retval;
    }

    /*
     * Evaluate an expression 'p' AND execute the associated bytecode
     */
    js2val JS2Metadata::EvalExpression(Environment *env, Phase phase, ExprNode *p)
    {
        js2val retval;
        uint8 *savePC = NULL;
        JS2Class *exprType;

        CompilationData *oldData = startCompilationUnit(NULL, bCon->mSource, bCon->mSourceLocation);
        try {
            Reference *r = SetupExprNode(env, phase, p, &exprType);
            if (r) r->emitReadBytecode(bCon, p->pos);
            bCon->emitOp(eReturn, p->pos);
            savePC = engine->pc;
            engine->pc = NULL;
            retval = engine->interpret(phase, bCon, env);
        }
        catch (Exception &x) {
            engine->pc = savePC;
            restoreCompilationUnit(oldData);
            throw x;
        }
        engine->pc = savePC;
        restoreCompilationUnit(oldData);
        return retval;
    }


    // Execute an expression and return the result, which must be a type
    JS2Class *JS2Metadata::EvalTypeExpression(Environment *env, Phase phase, ExprNode *p)
    {
        js2val retval = EvalExpression(env, phase, p);
        if (JS2VAL_IS_PRIMITIVE(retval))
            reportError(Exception::badValueError, "Type expected", p->pos);
        JS2Object *obj = JS2VAL_TO_OBJECT(retval);
        if (obj->kind != ClassKind)
            reportError(Exception::badValueError, "Type expected", p->pos);
        return checked_cast<JS2Class *>(obj);        
    }

    // Invoke the named function on the thisValue object (it is an object)
    // Returns false if no callable function exists. Otherwise return the
    // function result value.
    bool JS2Metadata::invokeFunctionOnObject(js2val thisValue, const String *fnName, js2val &result)
    {
        Multiname mn(fnName, publicNamespace);
        LookupKind lookup(true, JS2VAL_NULL);   // XXX using lexical lookup since we want readProperty to fail
                                                // if the function isn't defined
        js2val fnVal;

        if (readProperty(&thisValue, &mn, &lookup, RunPhase, &fnVal)) {
            if (JS2VAL_IS_OBJECT(fnVal)) {
                JS2Object *fnObj = JS2VAL_TO_OBJECT(fnVal);
                result = invokeFunction(fnObj, thisValue, NULL, 0);
                return true;
/*
                FunctionWrapper *fWrap = NULL;
                if ((fnObj->kind == SimpleInstanceKind)
                            && (objectType(fnVal) == functionClass)) {
                    fWrap = (checked_cast<SimpleInstance *>(fnObj))->fWrap;
                }
                else
                if ((fnObj->kind == PrototypeInstanceKind)
                        && ((checked_cast<PrototypeInstance *>(fnObj))->type == functionClass)) {
                    fWrap = (checked_cast<FunctionInstance *>(fnObj))->fWrap;
                }
                else
                if (fnObj->kind == MethodClosureKind) {
                    // XXX here we ignore the bound this, can that be right?
                    MethodClosure *mc = checked_cast<MethodClosure *>(fnObj);
                    fWrap = mc->method->fInst->fWrap;
                }
                if (fWrap) {
                    if (fWrap->code) {
                        result = (fWrap->code)(this, thisValue, NULL, 0);
                        return true;
                    }
                    else {
                        uint8 *savePC = NULL;
                        BytecodeContainer *bCon = fWrap->bCon;

                        CompilationData *oldData = startCompilationUnit(bCon, bCon->mSource, bCon->mSourceLocation);
                        ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                        runtimeFrame->instantiate(env);
                        runtimeFrame->thisObject = thisValue;
                        Frame *oldTopFrame = env->getTopFrame();
                        env->addFrame(runtimeFrame);
                        try {
                            savePC = engine->pc;
                            engine->pc = NULL;
                            result = engine->interpret(RunPhase, bCon);
                        }
                        catch (Exception &x) {
                            engine->pc = savePC;
                            restoreCompilationUnit(oldData);
                            env->setTopFrame(oldTopFrame);
                            throw x;
                        }
                        engine->pc = savePC;
                        restoreCompilationUnit(oldData);
                        env->setTopFrame(oldTopFrame);
                        return true;
                    }
                }
*/
            }
        }
        return false;
    }

    // Invoke the named function, no args, by looking it up
    // the current scope. (Do this by emitting and executing the
    // appropriate bytecode sequence)
    js2val JS2Metadata::invokeFunction(const char *fname)
    {
        js2val retval;
        uint8 *savePC = NULL;

        CompilationData *oldData = startCompilationUnit(NULL, bCon->mSource, bCon->mSourceLocation);
        try {
            LexicalReference rVal(&world.identifiers[widenCString(fname)], false);
            rVal.emitReadForInvokeBytecode(bCon, 0);
            bCon->emitOp(eCall, 0, -(0 + 2) + 1);    // pop argCount args, the base & function, and push a result
            bCon->addShort(0);
            bCon->emitOp(eReturn, 0);
            savePC = engine->pc;
            engine->pc = NULL;
            retval = engine->interpret(RunPhase, bCon, env);
        }
        catch (Exception &x) {
            engine->pc = savePC;
            restoreCompilationUnit(oldData);
            throw x;
        }
        engine->pc = savePC;
        restoreCompilationUnit(oldData);
        return retval;
    }

    js2val JS2Metadata::invokeFunction(JS2Object *fnObj, js2val thisValue, js2val *argv, uint32 argc)
    {
        js2val result = JS2VAL_UNDEFINED;

        FunctionWrapper *fWrap = NULL;
        if ((fnObj->kind == SimpleInstanceKind)
                && ((checked_cast<SimpleInstance *>(fnObj))->type == functionClass)) {
            fWrap = (checked_cast<SimpleInstance *>(fnObj))->fWrap;
        }
        else
        if ((fnObj->kind == PrototypeInstanceKind)
                && ((checked_cast<PrototypeInstance *>(fnObj))->type == functionClass)) {
            fWrap = (checked_cast<FunctionInstance *>(fnObj))->fWrap;
        }
        else
        if (fnObj->kind == MethodClosureKind) {
            // XXX here we ignore the bound this, can that be right?
            MethodClosure *mc = checked_cast<MethodClosure *>(fnObj);
            fWrap = mc->method->fInst->fWrap;
        }
        if (fWrap) {
            if (fWrap->code) {
                result = (fWrap->code)(this, thisValue, argv, argc);
            }
            else {
                uint8 *savePC = NULL;
                BytecodeContainer *bCon = fWrap->bCon;

                CompilationData *oldData = startCompilationUnit(bCon, bCon->mSource, bCon->mSourceLocation);
                ParameterFrame *runtimeFrame = new ParameterFrame(fWrap->compileFrame);
                RootKeeper rk(&runtimeFrame);
                runtimeFrame->instantiate(env);
                runtimeFrame->thisObject = thisValue;
                runtimeFrame->assignArguments(this, fnObj, argv, argc);
                Frame *oldTopFrame = env->getTopFrame();
                env->addFrame(runtimeFrame);
                try {
                    savePC = engine->pc;
                    engine->pc = NULL;
                    result = engine->interpret(RunPhase, bCon, env);
                }
                catch (Exception &x) {
                    engine->pc = savePC;
                    restoreCompilationUnit(oldData);
                    env->setTopFrame(oldTopFrame);
                    throw x;
                }
                engine->pc = savePC;
                restoreCompilationUnit(oldData);
                env->setTopFrame(oldTopFrame);
            }
        }
        return result;
    }

    // Save off info about the current compilation and begin a
    // new one - using the given parser.
    CompilationData *JS2Metadata::startCompilationUnit(BytecodeContainer *newBCon, const String &source, const String &sourceLocation)
    {
        CompilationData *result = new CompilationData();
        result->compilation_bCon = bCon;
        result->execution_bCon = engine->bCon;
        bConList.push_back(bCon);

        if (newBCon)
            bCon = newBCon;
        else
            bCon = new BytecodeContainer();
        bCon->mSource = source;
        bCon->mSourceLocation = sourceLocation;
        engine->bCon = bCon;

        return result;
    }

    // Restore the compilation data, and then delete the cached copy.
    void JS2Metadata::restoreCompilationUnit(CompilationData *oldData)
    {
        BytecodeContainer *xbCon = bConList.back();
        ASSERT(oldData->compilation_bCon == xbCon);
        bConList.pop_back();

        bCon = oldData->compilation_bCon;
        engine->bCon = oldData->execution_bCon;

        delete oldData;
    }

    // x is not a String
    const String *JS2Metadata::convertValueToString(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x))
            return engine->undefined_StringAtom;
        if (JS2VAL_IS_NULL(x))
            return engine->null_StringAtom;
        if (JS2VAL_IS_BOOLEAN(x))
            return (JS2VAL_TO_BOOLEAN(x)) ? engine->true_StringAtom : engine->false_StringAtom;
        if (JS2VAL_IS_INT(x))
            return engine->numberToString(JS2VAL_TO_INT(x));
        if (JS2VAL_IS_LONG(x)) {
            float64 d;
            JSLL_L2D(d, *JS2VAL_TO_LONG(x));
            return engine->numberToString(&d);
        }
        if (JS2VAL_IS_ULONG(x)) {
            float64 d;
            JSLL_UL2D(d, *JS2VAL_TO_ULONG(x));
            return engine->numberToString(&d);
        }
        if (JS2VAL_IS_FLOAT(x)) {
            float64 d = *JS2VAL_TO_FLOAT(x);
            return engine->numberToString(&d);
        }
        if (JS2VAL_IS_DOUBLE(x))
            return engine->numberToString(JS2VAL_TO_DOUBLE(x));
        return toString(toPrimitive(x, StringHint));
    }

    // x is not a primitive (it is an object and not null)
    js2val JS2Metadata::convertValueToPrimitive(js2val x, Hint hint)
    {
        // return [[DefaultValue]] --> get property 'toString' and invoke it, 
        // if not available or result is not primitive then try property 'valueOf'
        // if that's not available or returns a non primitive, throw a TypeError

        if (hint == StringHint) {
            js2val result;
            if (invokeFunctionOnObject(x, engine->toString_StringAtom, result)) {
                if (JS2VAL_IS_PRIMITIVE(result))
                    return result;
            }
            if (invokeFunctionOnObject(x, engine->valueOf_StringAtom, result)) {
                if (JS2VAL_IS_PRIMITIVE(result))
                    return result;
            }
            reportError(Exception::typeError, "DefaultValue failure", engine->errorPos());
        }
        else {
            js2val result;
            if (invokeFunctionOnObject(x, engine->valueOf_StringAtom, result)) {
                if (JS2VAL_IS_PRIMITIVE(result))
                    return result;
            }
            if (invokeFunctionOnObject(x, engine->toString_StringAtom, result)) {
                if (JS2VAL_IS_PRIMITIVE(result))
                    return result;
            }
            reportError(Exception::typeError, "DefaultValue failure", engine->errorPos());
        }
        return JS2VAL_VOID;
    }

    // x is not a number
    float64 JS2Metadata::convertValueToDouble(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x))
            return nan;
        if (JS2VAL_IS_NULL(x))
            return 0;
        if (JS2VAL_IS_BOOLEAN(x))
            return (JS2VAL_TO_BOOLEAN(x)) ? 1.0 : 0.0;
        if (JS2VAL_IS_STRING(x)) {
            String *str = JS2VAL_TO_STRING(x);
            uint32 length = str->length();
            if (length == 0)
                return 0.0;
            const char16 *numEnd;
            // if the string begins with '0X' or '0x' (after white space), then
            // read it as a hex integer.
            const char16 *strStart = str->data();
            const char16 *strEnd = strStart + length;
            const char16 *str1 = skipWhiteSpace(strStart, strEnd);
            if ((*str1 == '0') && ((str1[1] == 'x') || (str1[1] == 'X')))
                return stringToInteger(str1, strEnd, numEnd, 16);
            else {
                float64 d = stringToDouble(str1, strEnd, numEnd);
                if (numEnd == str1)
                    return nan;
                return d;
            }
        }
        if (JS2VAL_IS_INACCESSIBLE(x))
            reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
        if (JS2VAL_IS_UNINITIALIZED(x))
            reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
        return toFloat64(toPrimitive(x, NumberHint));
    }

    // x is not a number, convert it to one
    js2val JS2Metadata::convertValueToGeneralNumber(js2val x)
    {
        // XXX Assuming convert to float64, rather than long/ulong
        return engine->allocNumber(toFloat64(x));
    }

    // x is not an Object, it needs to be wrapped in one
    js2val JS2Metadata::convertValueToObject(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x) || JS2VAL_IS_NULL(x) || JS2VAL_IS_SPECIALREF(x))
            reportError(Exception::typeError, "Can't convert to Object", engine->errorPos());
        if (JS2VAL_IS_STRING(x))
            return String_Constructor(this, JS2VAL_NULL, &x, 1);
        if (JS2VAL_IS_BOOLEAN(x))
            return Boolean_Constructor(this, JS2VAL_NULL, &x, 1);
        if (JS2VAL_IS_NUMBER(x))
            return Number_Constructor(this, JS2VAL_NULL, &x, 1);
        NOT_REACHED("unsupported value type");
        return JS2VAL_VOID;
    }
    
    // x is any js2val
    float64 JS2Metadata::toFloat64(js2val x)
    { 
        if (JS2VAL_IS_INT(x)) 
            return JS2VAL_TO_INT(x); 
        else
        if (JS2VAL_IS_DOUBLE(x)) 
            return *JS2VAL_TO_DOUBLE(x); 
        else
        if (JS2VAL_IS_LONG(x)) {
            float64 d;
            JSLL_L2D(d, *JS2VAL_TO_LONG(x));
            return d;
        }
        else
        if (JS2VAL_IS_ULONG(x)) {
            float64 d;
            JSLL_UL2D(d, *JS2VAL_TO_ULONG(x));
            return d; 
        }
        else
        if (JS2VAL_IS_FLOAT(x))
            return *JS2VAL_TO_FLOAT(x);
        else 
            return convertValueToDouble(x); 
    }

    // x is not a bool
    bool JS2Metadata::convertValueToBoolean(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x))
            return false;
        if (JS2VAL_IS_NULL(x))
            return false;
        if (JS2VAL_IS_INT(x))
            return (JS2VAL_TO_INT(x) != 0);
        if (JS2VAL_IS_LONG(x) || JS2VAL_IS_ULONG(x))
            return (!JSLL_IS_ZERO(x));
        if (JS2VAL_IS_FLOAT(x)) {
            float64 xd = *JS2VAL_TO_FLOAT(x);
            return ! (JSDOUBLE_IS_POSZERO(xd) || JSDOUBLE_IS_NEGZERO(xd) || JSDOUBLE_IS_NaN(xd));
        }
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 xd = *JS2VAL_TO_DOUBLE(x);
            return ! (JSDOUBLE_IS_POSZERO(xd) || JSDOUBLE_IS_NEGZERO(xd) || JSDOUBLE_IS_NaN(xd));
        }
        if (JS2VAL_IS_STRING(x)) {
            String *str = JS2VAL_TO_STRING(x);
            return (str->length() != 0);
        }
        return true;
    }

    // x is not an int
    int32 JS2Metadata::convertValueToInteger(js2val x)
    {
        int32 i;
        if (JS2VAL_IS_LONG(x)) {
            JSLL_L2I(i, *JS2VAL_TO_LONG(x));
            return i;
        }
        if (JS2VAL_IS_ULONG(x)) {
            JSLL_UL2I(i, *JS2VAL_TO_ULONG(x));
            return i;
        }
        if (JS2VAL_IS_FLOAT(x)) {
            float64 f = *JS2VAL_TO_FLOAT(x);
            return JS2Engine::float64toInt32(f);
        }
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 d = *JS2VAL_TO_DOUBLE(x);
            return JS2Engine::float64toInt32(d);
        }
        float64 d = convertValueToDouble(x);
        return JS2Engine::float64toInt32(d);
    }


}; // namespace MetaData
}; // namespace Javascript



