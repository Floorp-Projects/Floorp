/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
		BytecodeContainer *new_bCon = NULL;
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
				new_bCon = bCon;
                ValidateStmtList(parsedStatements);
                result = ExecuteStmtList(RunPhase, parsedStatements);
            }
        }
        catch (Exception &x) {
            if (oldData)
                restoreCompilationUnit(oldData);
			if (new_bCon)
				delete new_bCon;
            throw x;
        }
        if (oldData)
            restoreCompilationUnit(oldData);
		if (new_bCon)
			delete new_bCon;
        return result;
    }

    js2val JS2Metadata::readEvalFile(const String& fileName)
    {
        js2val result = JS2VAL_VOID;

        std::string str(fileName.length(), char());
        std::transform(fileName.begin(), fileName.end(), str.begin(), narrow);
        FILE* f = fopen(str.c_str(), "rb");

        if (f) {
			fseek(f, 0, SEEK_END);
			long fsize = ftell(f);
			fseek(f, 0, SEEK_SET);
			char *buf = new char[fsize];
			fread(buf, fsize, 1, f);
			fclose(f);
			String buffer(fsize, uni::null);
			for (long i = 0; i < fsize; i++) {
				if (!buf[i]) {
					buffer.resize(i);
					break;
				}
				buffer[i] = widen(buf[i]);
			}
			delete [] buf;
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
        Arena *oldArena = referenceArena;
        referenceArena = new Arena;
        size_t lastPos;
        try {
            lastPos = p->pos;
            while (p) {
                SetupStmt(env, phase, p);
                lastPos = p->pos;
                p = p->next;
            }
        }
        catch (Exception &x) {
            referenceArena->clear();
            delete referenceArena;
            referenceArena = oldArena;
            throw x;
        }
        referenceArena->clear();
        delete referenceArena;
        referenceArena = oldArena;

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

        Arena *oldArena = referenceArena;
        referenceArena = new Arena;
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
            referenceArena->clear();
            delete referenceArena;
            referenceArena = oldArena;
            engine->pc = savePC;
            restoreCompilationUnit(oldData);
            throw x;
        }
        referenceArena->clear();
        delete referenceArena;
        referenceArena = oldArena;
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
    bool JS2Metadata::invokeFunctionOnObject(js2val thisValue, const StringAtom &fnName, js2val &result)
    {
        js2val fnVal;

        JS2Class *limit = objectType(thisValue);
        if (limit->ReadPublic(this, &thisValue, fnName, RunPhase, &fnVal)) {
            if (JS2VAL_IS_OBJECT(fnVal)) {
                JS2Object *fnObj = JS2VAL_TO_OBJECT(fnVal);
                if ((fnObj->kind == SimpleInstanceKind)
                        && ((checked_cast<SimpleInstance *>(fnObj))->type == functionClass)) {
                    result = invokeFunction(fnObj, thisValue, NULL, 0, NULL);
                    return true;
                }
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
            LexicalReference rVal(new (this) Multiname(world.identifiers[widenCString(fname)]), false, bCon);
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

    // Invoke the constructor function for a class
    void JS2Metadata::invokeInit(JS2Class *c, js2val thisValue, js2val *argv, uint32 argc)
    {
        FunctionInstance *init = NULL;
        if (c) init = c->init;
        if (init) {
            ParameterFrame *runtimeFrame;
            DEFINE_ROOTKEEPER(this, rk, runtimeFrame);
            runtimeFrame = init->fWrap->compileFrame;
            if (!init->fWrap->compileFrame->callsSuperConstructor) {
                invokeInit(c->super, thisValue, NULL, 0);
                runtimeFrame->superConstructorCalled = true;
            }
            invokeFunction(init, thisValue, argv, argc, runtimeFrame);
            if (!runtimeFrame->superConstructorCalled)
                reportError(Exception::uninitializedError, "The superconstructor must be called before returning normally from a constructor", engine->errorPos());
        }
        else
            if (argc)
                reportError(Exception::argumentsError, "The default constructor does not take any arguments", engine->errorPos());
    }

    js2val JS2Metadata::invokeFunction(JS2Object *fnObj, js2val thisValue, js2val *argv, uint32 argc, ParameterFrame *runtimeFrame)
    {
        js2val result = JS2VAL_UNDEFINED;

        FunctionWrapper *fWrap = NULL;
        if ((fnObj->kind == SimpleInstanceKind)
                && ((checked_cast<SimpleInstance *>(fnObj))->type == functionClass)) {
            FunctionInstance *fInst = checked_cast<FunctionInstance *>(fnObj);
            fWrap = fInst->fWrap;
            if (fInst->isMethodClosure) {
                // XXX ignoring fInst->thisObject;
            }
            if (fWrap->code) {
                result = (fWrap->code)(this, thisValue, argv, argc);
            }
            else {
                uint8 *savePC = NULL;
                BytecodeContainer *bCon = fWrap->bCon;
                BytecodeContainer *oldbcon = this->bCon;
                DEFINE_ROOTKEEPER(this, rk, runtimeFrame);
                if (runtimeFrame == NULL)
                    runtimeFrame = fWrap->compileFrame;
                runtimeFrame->thisObject = thisValue;
                runtimeFrame->assignArguments(this, fnObj, argv, argc);
                Frame *oldTopFrame = fWrap->env->getTopFrame();
                if (fInst->isMethodClosure)
                    fWrap->env->addFrame(objectType(thisValue));
                fWrap->env->addFrame(runtimeFrame);
                ParameterFrame *oldPFrame = engine->parameterFrame;
                js2val *oldPSlots = engine->parameterSlots;
                try {
                    savePC = engine->pc;
                    engine->pc = NULL;
                    engine->parameterFrame = runtimeFrame;
                    engine->parameterSlots = runtimeFrame->argSlots;
                    engine->parameterCount = runtimeFrame->argCount;
                    result = engine->interpret(RunPhase, bCon, fWrap->env);
                }
                catch (Exception &x) {
                    engine->pc = savePC;
                    fWrap->env->setTopFrame(oldTopFrame);
                    engine->parameterFrame = oldPFrame;
                    engine->parameterSlots = oldPSlots;
                    if (engine->parameterFrame)
                        engine->parameterFrame->argSlots = engine->parameterSlots;
                    this->bCon = oldbcon;
                    runtimeFrame->releaseArgs();
                    throw x;
                }
                engine->pc = savePC;
                fWrap->env->setTopFrame(oldTopFrame);
                engine->parameterFrame = oldPFrame;
                engine->parameterSlots = oldPSlots;
                if (engine->parameterFrame)
                    engine->parameterFrame->argSlots = engine->parameterSlots;
                this->bCon = oldbcon;
                runtimeFrame->releaseArgs();
            }
        }
        return result;
    }

    // Save off info about the current compilation and begin a
    // new one - using the given source information.
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
            return engine->allocStringPtr(&engine->undefined_StringAtom);
        if (JS2VAL_IS_NULL(x))
            return engine->allocStringPtr(&engine->null_StringAtom);
        if (JS2VAL_IS_BOOLEAN(x))
            if (JS2VAL_TO_BOOLEAN(x)) 
				return engine->allocStringPtr(&engine->true_StringAtom);
			else
				return engine->allocStringPtr(&engine->false_StringAtom);
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

        JS2Object *obj = JS2VAL_TO_OBJECT(x);
        DEFINE_ROOTKEEPER(this, rk1, obj);
        if (obj->kind == ClassKind)    // therefore, not an E3 object, so just return
            return engine->allocString("Function");// engine->typeofString(x);     // the 'typeof' string

        if (hint == NoHint) {
            if ((obj->kind == SimpleInstanceKind)
                    && ((checked_cast<SimpleInstance *>(obj))->type == dateClass))
                hint = StringHint;
            else
                hint = NumberHint;
        }

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

    float64 JS2Metadata::convertStringToDouble(const String *str)
    {
        bool neg = false;
        uint32 length = str->length();
        if (length == 0)
            return 0.0;
        const char16 *numEnd;
        // if the string begins with '0X' or '0x' (after white space), then
        // read it as a hex integer.
        const char16 *strStart = str->data();
        const char16 *strEnd = strStart + length;
        const char16 *str1 = skipWhiteSpace(strStart, strEnd);
        if (str1 == strEnd)
            return 0.0;
        if (*str1 == '-') {
            neg = true;
            str1++;
        }
        float64 d;
        if ((*str1 == '0') && ((str1[1] == 'x') || (str1[1] == 'X')))
            d = stringToInteger(str1, strEnd, numEnd, 16);
        else {
            d = stringToDouble(str1, strEnd, numEnd);
            if (numEnd == str1)
                return nan;
            if (skipWhiteSpace(numEnd, strEnd) != strEnd)
                return nan;
        }
        return (neg) ? -d : d;
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
            return convertStringToDouble(str);
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
    float64 JS2Metadata::valToFloat64(js2val x)
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
    float64 JS2Metadata::convertValueToInteger(js2val x)
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
            return JS2Engine::truncateFloat64(f);
        }
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 d = *JS2VAL_TO_DOUBLE(x);
            return JS2Engine::truncateFloat64(d);
        }
        float64 d = convertValueToDouble(x);
        return JS2Engine::truncateFloat64(d);
    }
    
    // x is not an int
    int32 JS2Metadata::convertValueToInt32(js2val x)
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
    
    // x is not an int
    uint32 JS2Metadata::convertValueToUInt32(js2val x)
    {
        uint32 i;
        if (JS2VAL_IS_LONG(x)) {
            JSLL_L2UI(i, *JS2VAL_TO_LONG(x));
            return i;
        }
        if (JS2VAL_IS_ULONG(x)) {
            JSLL_UL2UI(i, *JS2VAL_TO_ULONG(x));
            return i;
        }
        if (JS2VAL_IS_FLOAT(x)) {
            float64 f = *JS2VAL_TO_FLOAT(x);
            return JS2Engine::float64toUInt32(f);
        }
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 d = *JS2VAL_TO_DOUBLE(x);
            return JS2Engine::float64toUInt32(d);
        }
        float64 d = convertValueToDouble(x);
        return JS2Engine::float64toUInt32(d);
    }


    bool JS2Class::Read(JS2Metadata *meta, js2val *base, Multiname *multiname, Environment *env, Phase phase, js2val *rval)
    {
        InstanceMember *mBase = meta->findBaseInstanceMember(this, multiname, ReadAccess);
        if (mBase)
            return meta->readInstanceMember(*base, this, mBase, phase, rval);
        if (this != meta->objectType(*base))
            return false;

        Member *m = meta->findCommonMember(base, multiname, ReadAccess, false);
        if (m == NULL) {
            if ((env == NULL) 
                    && JS2VAL_IS_OBJECT(*base) 
                    && (( (JS2VAL_TO_OBJECT(*base)->kind == SimpleInstanceKind) && !checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(*base))->sealed)
                        || ( (JS2VAL_TO_OBJECT(*base)->kind == PackageKind) && !checked_cast<Package *>(JS2VAL_TO_OBJECT(*base))->sealed) ) ) {
                if (phase == CompilePhase)
                    meta->reportError(Exception::compileExpressionError, "Inappropriate compile time expression", meta->engine->errorPos());
                else {
                    *rval = JS2VAL_UNDEFINED;
                    return true;
                }
            }
            else
                return false;
        }
        switch (m->memberKind) {
        case Member::FrameVariableMember:
            return meta->readLocalMember(checked_cast<LocalMember *>(m), phase, rval, checked_cast<Frame *>(JS2VAL_TO_OBJECT(*base)));
        case Member::ForbiddenMember:
        case Member::DynamicVariableMember:
        case Member::VariableMember:
        case Member::ConstructorMethodMember:
        case Member::SetterMember:
        case Member::GetterMember:
            return meta->readLocalMember(checked_cast<LocalMember *>(m), phase, rval, NULL);
        case Member::InstanceVariableMember:
        case Member::InstanceMethodMember:
        case Member::InstanceGetterMember:
        case Member::InstanceSetterMember:
            {
                if (!JS2VAL_IS_OBJECT(*base) || (JS2VAL_TO_OBJECT(*base)->kind != ClassKind) || (env == NULL))
                    meta->reportError(Exception::referenceError, "Can't read an instance member without supplying an instance", meta->engine->errorPos());
                js2val thisVal = env->readImplicitThis(meta);
                return meta->readInstanceMember(thisVal, meta->objectType(thisVal), checked_cast<InstanceMember *>(m), phase, rval);
            }
        default:
            NOT_REACHED("bad member kind");
            return false;
        }
    }

    bool JS2Class::ReadPublic(JS2Metadata *meta, js2val *base, const StringAtom &name, Phase phase, js2val *rval)
    {
        // XXX could speed up by pushing knowledge of single namespace?
        Multiname *mn = new (meta) Multiname(name, meta->publicNamespace);
        DEFINE_ROOTKEEPER(meta, rk, mn);
        return Read(meta, base, mn, NULL, phase, rval);
    }

    bool JS2Class::DeletePublic(JS2Metadata *meta, js2val base, const StringAtom &name, bool *result)
    {
        // XXX could speed up by pushing knowledge of single namespace?
        Multiname *mn = new (meta) Multiname(name, meta->publicNamespace);
        DEFINE_ROOTKEEPER(meta, rk, mn);
        return Delete(meta, base, mn, NULL, result);
    }

    bool JS2Class::WritePublic(JS2Metadata *meta, js2val base, const StringAtom &name, bool createIfMissing, js2val newValue)
    {
        // XXX could speed up by pushing knowledge of single namespace?
        Multiname *mn = new (meta) Multiname(name, meta->publicNamespace);
        DEFINE_ROOTKEEPER(meta, rk, mn);
        return Write(meta, base, mn, NULL, createIfMissing, newValue, false);
    }

    bool JS2Class::BracketRead(JS2Metadata *meta, js2val *base, js2val indexVal, Phase phase, js2val *rval)
    {
        const String *indexStr = meta->toString(indexVal);
        DEFINE_ROOTKEEPER(meta, rk, indexStr);
        Multiname *mn = new (meta) Multiname(meta->world.identifiers[*indexStr], meta->publicNamespace);
        DEFINE_ROOTKEEPER(meta, rk1, mn);
        return Read(meta, base, mn, NULL, phase, rval);
    }

	bool isValidIndex(const StringAtom &name, uint32 &index)
	{
        const char16 *numEnd;        
        float64 f = stringToDouble(name.data(), name.data() + name.length(), numEnd);
        index = JS2Engine::float64toUInt32(f);
        char buf[dtosStandardBufferSize];
        const char *chrp = doubleToStr(buf, dtosStandardBufferSize, index, dtosStandard, 0);
        return (widenCString(chrp) == name);
	}

    bool JS2ArrayClass::Read(JS2Metadata *meta, js2val *base, Multiname *multiname, Environment *env, Phase phase, js2val *rval)
    {
        if ((multiname->name == meta->engine->length_StringAtom) && (multiname->nsList->size() == 1) && (multiname->nsList->back() == meta->publicNamespace)) {
            *rval = Array_lengthGet(meta, *base, NULL, 0);
            return true;
        }
        else
            return JS2Class::Read(meta, base, multiname, env, phase, rval);
    }

    bool JS2ArrayClass::Write(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool createIfMissing, js2val newValue, bool initFlag)
    {
        ASSERT(JS2VAL_IS_OBJECT(base));
        JS2Object *obj = JS2VAL_TO_OBJECT(base);
        
        bool result;
        if ((multiname->name == meta->engine->length_StringAtom) && (multiname->nsList->size() == 1) && (multiname->nsList->back() == meta->publicNamespace)) {
            Array_lengthSet(meta, base, &newValue, 1);
            result = true;
        }
        else {
            result = JS2Class::Write(meta, base, multiname, env, createIfMissing, newValue, false);
            if (result && (multiname->nsList->size() == 1) && (multiname->nsList->back() == meta->publicNamespace)) {
				uint32 index;
                if (isValidIndex(multiname->name, index)) {
					ArrayInstance *arrInst = checked_cast<ArrayInstance *>(obj);
                    if (index >= arrInst->length)
                        setLength(meta, obj, index + 1);
                }
            }
        }
        return result;
    }    

    bool JS2ArrayClass::Delete(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool *result)
    {
        if ((multiname->name == meta->engine->length_StringAtom) && (multiname->nsList->size() == 1) && (multiname->nsList->back() == meta->publicNamespace)) {
            *result = false;
            return true;
        }
        else
            return JS2Class::Delete(meta, base, multiname, env, result);
    }

    bool JS2ArrayClass::BracketWrite(JS2Metadata *meta, js2val base, js2val indexVal, js2val newValue)
	{
		const String *indexStr = meta->toString(indexVal);
		DEFINE_ROOTKEEPER(meta, rk, indexStr);
		Multiname *mn = new (meta) Multiname(meta->world.identifiers[*indexStr], meta->publicNamespace);
		DEFINE_ROOTKEEPER(meta, rk1, mn);

		ASSERT(JS2VAL_IS_OBJECT(base));
		JS2Object *obj = JS2VAL_TO_OBJECT(base);

		bool result;
		if (!JS2VAL_IS_INT(indexVal)) {
			if (mn->name == meta->engine->length_StringAtom) {
				Array_lengthSet(meta, base, &newValue, 1);
				return true;
			}
		}
		result = JS2Class::Write(meta, base, mn, NULL, true, newValue, false);
		uint32 index;
		if (result) {
			if (JS2VAL_IS_INT(indexVal))
				index = JS2VAL_TO_INT(indexVal);
			else {
                if (!isValidIndex(mn->name, index))
					return true;	// not a valid index, don't need to set length
			}
		}
        ArrayInstance *arrInst = checked_cast<ArrayInstance *>(obj);
		if (index >= arrInst->length) {
			js2val newLength = meta->engine->allocNumber(index + 1);
			Array_lengthSet(meta, base, &newLength, 1);
		}
		return result;
	}

    bool JS2ArrayClass::BracketDelete(JS2Metadata *meta, js2val base, js2val indexVal, bool *result)
	{
		const String *indexStr = meta->toString(indexVal);
		DEFINE_ROOTKEEPER(meta, rk, indexStr);
		Multiname *mn = new (meta) Multiname(meta->world.identifiers[*indexStr], meta->publicNamespace);
		DEFINE_ROOTKEEPER(meta, rk1, mn);
		return Delete(meta, base, mn, NULL, result);
	}

    bool JS2ArgumentsClass::Read(JS2Metadata *meta, js2val *base, Multiname *multiname, Environment *env, Phase phase, js2val *rval)
    {
        ASSERT(JS2VAL_IS_OBJECT(*base));
        JS2Object *obj = JS2VAL_TO_OBJECT(*base);
        ArgumentsInstance *args = checked_cast<ArgumentsInstance *>(obj);

        uint32 index;
        if ((multiname->nsList->size() == 1) 
                && (multiname->nsList->back() == meta->publicNamespace) 
                && isValidIndex(multiname->name, index)
                && (index < args->count)) {
            if (args->mSplit[index])
                *rval = args->mSplitValue[index];
            else
                *rval = args->mSlots[index];
            return true;
        }
        else
            return JS2Class::Read(meta, base, multiname, env, phase, rval);
    }

    bool JS2ArgumentsClass::Write(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool createIfMissing, js2val newValue, bool initFlag)
    {
        ASSERT(JS2VAL_IS_OBJECT(base));
        JS2Object *obj = JS2VAL_TO_OBJECT(base);
        ArgumentsInstance *args = checked_cast<ArgumentsInstance *>(obj);

        uint32 index;
        if ((multiname->nsList->size() == 1) 
                && (multiname->nsList->back() == meta->publicNamespace) 
                && isValidIndex(multiname->name, index)
                && (index < args->count)) {
            if (args->mSplit[index])
                args->mSplitValue[index] = newValue;
            else
                args->mSlots[index] = newValue;
            return true;
        }
        else
            return JS2Class::Write(meta, base, multiname, env, createIfMissing, newValue, false);
    }    

    bool JS2ArgumentsClass::Delete(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool *result)
    {
        ASSERT(JS2VAL_IS_OBJECT(base));
        JS2Object *obj = JS2VAL_TO_OBJECT(base);
        ArgumentsInstance *args = checked_cast<ArgumentsInstance *>(obj);

        uint32 index;
        if ((multiname->nsList->size() == 1) 
                && (multiname->nsList->back() == meta->publicNamespace) 
                && isValidIndex(multiname->name, index)
                && (index < args->count)) {
            if (!args->mSplitValue)
                args->mSplitValue = new js2val[slotCount];
            args->mSplit[index] = true;
            args->mSplitValue[index] = JS2VAL_VOID;
            return true;
        }
        else
            return JS2Class::Delete(meta, base, multiname, env, result);
    }

    bool JS2ArrayClass::BracketRead(JS2Metadata *meta, js2val *base, js2val indexVal, Phase phase, js2val *rval)
	{
		const String *indexStr = meta->toString(indexVal);
		DEFINE_ROOTKEEPER(meta, rk, indexStr);
		Multiname *mn = new (meta) Multiname(meta->world.identifiers[*indexStr], meta->publicNamespace);
		DEFINE_ROOTKEEPER(meta, rk1, mn);
		return Read(meta, base, mn, NULL, phase, rval);
	}

    bool JS2Class::BracketWrite(JS2Metadata *meta, js2val base, js2val indexVal, js2val newValue)
    {
        const String *indexStr = meta->toString(indexVal);
        DEFINE_ROOTKEEPER(meta, rk, indexStr);
        Multiname *mn = new (meta) Multiname(meta->world.identifiers[*indexStr], meta->publicNamespace);
        DEFINE_ROOTKEEPER(meta, rk1, mn);
        return Write(meta, base, mn, NULL, true, newValue, false);
    }

    bool JS2Class::BracketDelete(JS2Metadata *meta, js2val base, js2val indexVal, bool *result)
    {
        const String *indexStr = meta->toString(indexVal);
        DEFINE_ROOTKEEPER(meta, rk, indexStr);
        Multiname *mn = new (meta) Multiname(meta->world.identifiers[*indexStr], meta->publicNamespace);
        DEFINE_ROOTKEEPER(meta, rk1, mn);
        return Delete(meta, base, mn, NULL, result);
    }

    bool JS2Class::Write(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool createIfMissing, js2val newValue, bool initFlag)
    {
        InstanceMember *mBase = meta->findBaseInstanceMember(this, multiname, WriteAccess);
        if (mBase) {
            meta->writeInstanceMember(base, this, mBase, newValue);
            return true;
        }
        if (this != meta->objectType(base))
            return false;

        Member *m = meta->findCommonMember(&base, multiname, WriteAccess, true);
        if (m == NULL) {
            // XXX E3 compatibility...
            JS2Object *baseObj = NULL;
            DEFINE_ROOTKEEPER(meta, rk, baseObj);
            if (JS2VAL_IS_PRIMITIVE(base)) {
                if (meta->cxt.E3compatibility)
                    baseObj = JS2VAL_TO_OBJECT(meta->toObject(base));   
            }
            else
                baseObj = JS2VAL_TO_OBJECT(base);
            if (createIfMissing 
                    && baseObj
                    && ( ((baseObj->kind == SimpleInstanceKind) && !checked_cast<SimpleInstance *>(baseObj)->sealed)
                            || ( (baseObj->kind == PackageKind) && !checked_cast<Package *>(baseObj)->sealed)) ) {
                QualifiedName *qName = multiname->selectPrimaryName(meta);
				ASSERT(qName);
                Multiname *mn = new (meta) Multiname(*qName);
				delete qName;
                DEFINE_ROOTKEEPER(meta, rk, mn);
                if ( (meta->findBaseInstanceMember(this, mn, ReadAccess) == NULL)
                        && (meta->findCommonMember(&base, mn, ReadAccess, true) == NULL) ) {
                    meta->createDynamicProperty(baseObj, mn->name, newValue, ReadWriteAccess, false, true);
                    return true;
                }
            }
            return false;
        }
        switch (m->memberKind) {
        case Member::FrameVariableMember:
			ASSERT(JS2VAL_IS_OBJECT(base));
            return meta->writeLocalMember(checked_cast<LocalMember *>(m), newValue, initFlag, checked_cast<Frame *>(JS2VAL_TO_OBJECT(base)));
        case Member::ForbiddenMember:
        case Member::DynamicVariableMember:
        case Member::VariableMember:
        case Member::ConstructorMethodMember:
        case Member::SetterMember:
        case Member::GetterMember:
            return meta->writeLocalMember(checked_cast<LocalMember *>(m), newValue, initFlag, NULL);
        case Member::InstanceVariableMember:
        case Member::InstanceMethodMember:
        case Member::InstanceGetterMember:
        case Member::InstanceSetterMember:
            {
                if ( !JS2VAL_IS_OBJECT(base) || (JS2VAL_TO_OBJECT(base)->kind != ClassKind) || (env == NULL))
                    meta->reportError(Exception::referenceError, "Can't write an instance member withoutsupplying an instance", meta->engine->errorPos());
                js2val thisVal = env->readImplicitThis(meta);
                meta->writeInstanceMember(thisVal, meta->objectType(thisVal), checked_cast<InstanceMember *>(m), newValue);
                return true;
            }
        default:
            NOT_REACHED("bad member kind");
            return false;
        }
    }

    bool JS2Class::Delete(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool *result)
    {
        InstanceMember *mBase = meta->findBaseInstanceMember(this, multiname, ReadWriteAccess);
        if (mBase) {
            *result = false;
            return true;
        }
        if (this != meta->objectType(base))
            return false;

        Member *m = meta->findCommonMember(&base, multiname, ReadWriteAccess, false);
        if (m == NULL) {
            *result = true;
            return true;
		}
        switch (m->memberKind) {
        case Member::ForbiddenMember:
            meta->reportError(Exception::propertyAccessError, "It is forbidden", meta->engine->errorPos());
            return false;
        case Member::FrameVariableMember:
            {
                if (checked_cast<FrameVariable *>(m)->sealed) {
                    *result = false;
                    return true;
                }
                goto VariableMemberCommon;
            }
        case Member::DynamicVariableMember:
            {
                if (checked_cast<DynamicVariable *>(m)->sealed) {
                    *result = false;
                    return true;
                }
VariableMemberCommon:
                // XXX if findCommonMember returned the Binding instead, we wouldn't have to rediscover it here...
                JS2Object *container = JS2VAL_TO_OBJECT(meta->toObject(base));
                LocalBindingMap *lMap;
                if (container->kind == SimpleInstanceKind)
                    lMap = &checked_cast<SimpleInstance *>(container)->localBindings;
                else
                    lMap = &checked_cast<NonWithFrame *>(container)->localBindings;

                LocalBindingEntry *lbeP = (*lMap)[multiname->name];
                if (lbeP) {
                    while (true) {
                        bool deletedOne = false;
                        for (LocalBindingEntry::NS_Iterator i = lbeP->begin(), end = lbeP->end(); (i != end); i++) {
                            LocalBindingEntry::NamespaceBinding &ns = *i;
                            if (multiname->listContains(ns.first)) {
                                lbeP->bindingList.erase(i);
                                deletedOne = true;
                                if (ns.second->content->release())
                                    delete ns.second->content;
                                break;
                            }
                        }
                        if (!deletedOne)
                            break;
                    }
                }
                *result = true;
                return true;
            }
        case Member::VariableMember:
        case Member::ConstructorMethodMember:
        case Member::SetterMember:
        case Member::GetterMember:
            *result = false;
            return true;
        case Member::InstanceVariableMember:
        case Member::InstanceMethodMember:
        case Member::InstanceGetterMember:
        case Member::InstanceSetterMember:
            if ( (!JS2VAL_IS_OBJECT(base) || (JS2VAL_TO_OBJECT(base)->kind != ClassKind)) || (env == NULL)) {
                *result = false;
                return true;
            }
            env->readImplicitThis(meta);
            *result = false;
            return true;
        default:
            NOT_REACHED("bad member kind");
            return false;
        }
    }

    js2val JS2Class::ImplicitCoerce(JS2Metadata *meta, js2val newValue)
    {
        if (JS2VAL_IS_NULL(newValue) || meta->objectType(newValue)->isAncestor(this) )
            return newValue;
        meta->reportError(Exception::badValueError, "Illegal coercion", meta->engine->errorPos());
        return JS2VAL_VOID;
    }

    js2val JS2IntegerClass::ImplicitCoerce(JS2Metadata *meta, js2val newValue)
    {
        if (JS2VAL_IS_UNDEFINED(newValue))
            return JS2VAL_ZERO;
        if (JS2VAL_IS_NUMBER(newValue)) {
            int64 x = meta->engine->checkInteger(newValue);
            if (JSLL_IS_INT32(x)) {
                int32 i = 0;
                JSLL_L2I(i, x);
                return INT_TO_JS2VAL(i);
            }
        }        
        meta->reportError(Exception::badValueError, "Illegal coercion", meta->engine->errorPos());
        return JS2VAL_VOID;
    }

    js2val JS2Class::Is(JS2Metadata *meta, js2val newValue)
    {
        return BOOLEAN_TO_JS2VAL(meta->objectType(newValue) == this);
    }

    js2val JS2IntegerClass::Is(JS2Metadata *meta, js2val newValue)
    {
        bool result = false;
        if (JS2VAL_IS_NUMBER(newValue)) {
            float64 f = meta->toFloat64(newValue);
            if (JSDOUBLE_IS_FINITE(f)) {
                
            }
        }        
        return BOOLEAN_TO_JS2VAL(result);
    }

    bool JS2StringClass::BracketRead(JS2Metadata *meta, js2val *base, js2val indexVal, Phase phase, js2val *rval)
    {
        if (JS2VAL_IS_INT(indexVal)) {
            const String *str = NULL;
            if (JS2VAL_IS_STRING(*base)) {
                str = JS2VAL_TO_STRING(*base);
            }
            else {
                ASSERT(JS2VAL_IS_OBJECT(*base));
                JS2Object *obj = JS2VAL_TO_OBJECT(*base);
                ASSERT((obj->kind == SimpleInstanceKind) && (checked_cast<SimpleInstance *>(obj)->type == meta->stringClass));
                StringInstance *a = checked_cast<StringInstance *>(obj);
                str = a->mValue;
            }
            int32 i = JS2VAL_TO_INT(indexVal);
            if ((i >= 0) && ((uint32)(i) < str->length())) {
                jschar c = (*str)[i];
                *rval = meta->engine->allocString(&c, 1);
            }
            else
                *rval = JS2VAL_UNDEFINED;
            return true;
        }
        else
            return BracketRead(meta, base, indexVal, phase, rval);
    }



}; // namespace MetaData
}; // namespace Javascript



