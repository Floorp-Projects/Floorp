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
#include <list>
#include <map>
#include <stack>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "numerics.h"
#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

namespace JavaScript {    
namespace MetaData {


    js2val Function_Constructor(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
    {   
        if (argc) {
            const String &srcLoc = widenCString("Function constructor source");
            const String *bodyStr = meta->toString(argv[argc - 1]);
            String functionExpr(widenCString("("));
            if (argc > 1) {
                for (uint32 i = 0; i < (argc - 1); i++) {
                    functionExpr += *meta->toString(argv[i]);
                    if (i < (argc - 2))
                        functionExpr += ",";
                }                                       
            }
            functionExpr += widenCString("){") + *bodyStr + widenCString("}");

            Arena a;
            Pragma::Flags flags = Pragma::js1;          // XXX get flags from meta/context ?
            Parser parser(meta->world, a, flags, functionExpr, srcLoc);
            FunctionExprNode *fnExpr = parser.parseFunctionExpression(meta->engine->errorPos());
            ASSERT(parser.lexer.peek(true).hasKind(Token::end));
            ASSERT(fnExpr);     // otherwise, an exception would have been thrown out of here
            fnExpr->function.fn = NULL;
            DEFINE_ROOTKEEPER(meta, rk, fnExpr->function.fn);
            meta->ValidateExpression(&meta->cxt, meta->env, fnExpr);
            Arena *oldArena = meta->referenceArena;
            meta->referenceArena = new Arena;
            try {
                FunctionInstance *fnInst = fnExpr->function.fn;
                ASSERT(fnInst);
                CompilationData *oldData = meta->startCompilationUnit(fnInst->fWrap->bCon, *bodyStr, srcLoc);
                meta->env->addFrame(fnInst->fWrap->compileFrame);
                meta->SetupStmt(meta->env, RunPhase, fnExpr->function.body);
                fnInst->fWrap->bCon->emitOp(eReturnVoid, meta->engine->errorPos());
                meta->env->removeTopFrame();
                meta->restoreCompilationUnit(oldData);
            }
            catch (Exception &x) {
                meta->referenceArena->clear();
                meta->referenceArena = oldArena;
                throw x;
            }
            meta->referenceArena->clear();
            meta->referenceArena = oldArena;
            ASSERT(fnExpr);
            return OBJECT_TO_JS2VAL(fnExpr->function.fn);
        }
        else {  // construct an empty function wrapper
            js2val thatValue = OBJECT_TO_JS2VAL(new (meta) FunctionInstance(meta, meta->functionClass->prototype, meta->functionClass));
            FunctionInstance *fnInst = checked_cast<FunctionInstance *>(JS2VAL_TO_OBJECT(thatValue));
            DEFINE_ROOTKEEPER(meta, rk, fnInst);
            fnInst->fWrap = new FunctionWrapper(meta, true, new (meta) ParameterFrame(JS2VAL_INACCESSIBLE, true), meta->env);
            fnInst->fWrap->length = 0;
            fnInst->fWrap->bCon->emitOp(eReturnVoid, meta->engine->errorPos());
            meta->createDynamicProperty(fnInst, meta->engine->length_StringAtom, INT_TO_JS2VAL(0), ReadAccess, true, false);
            if (meta->cxt.E3compatibility)
                meta->createDynamicProperty(fnInst, meta->world.identifiers["arguments"], JS2VAL_NULL, ReadAccess, true, false);
            return thatValue;
        }
    }
        
    static js2val Function_toString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->functionClass))
            meta->reportError(Exception::typeError, "Function.toString called on something other than a function thing", meta->engine->errorPos());
        FunctionInstance *fnInst = checked_cast<FunctionInstance *>(JS2VAL_TO_OBJECT(thisValue));
		if (fnInst->sourceText)
			return STRING_TO_JS2VAL(fnInst->sourceText);
		else
			return meta->engine->allocString(meta->engine->Function_StringAtom);
    }

    static js2val Function_call(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->functionClass))
            meta->reportError(Exception::typeError, "Function.call called on something other than a function thing", meta->engine->errorPos());
        FunctionInstance *fnInst = checked_cast<FunctionInstance *>(JS2VAL_TO_OBJECT(thisValue));

		js2val callThis = argv[0];
		DEFINE_ROOTKEEPER(meta, rk0, callThis);
		if (JS2VAL_IS_NULL(argv[0]) || JS2VAL_IS_UNDEFINED(argv[0]))
			callThis = OBJECT_TO_JS2VAL(meta->glob);
		else
			callThis = meta->toObject(callThis);
    
		if (argc > 1)
			return meta->invokeFunction(fnInst, callThis, &argv[1], argc - 1, NULL);
		else
			return meta->invokeFunction(fnInst, callThis, NULL, 0, NULL);
	}

    static js2val Function_apply(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->functionClass))
            meta->reportError(Exception::typeError, "Function.apply called on something other than a function thing", meta->engine->errorPos());
        FunctionInstance *fnInst = checked_cast<FunctionInstance *>(JS2VAL_TO_OBJECT(thisValue));
		js2val callThis = argv[0];
		DEFINE_ROOTKEEPER(meta, rk0, callThis);
		if (JS2VAL_IS_NULL(argv[0]) || JS2VAL_IS_UNDEFINED(argv[0]))
			callThis = OBJECT_TO_JS2VAL(meta->glob);
		else
			callThis = meta->toObject(callThis);

		if ((argc > 1) && !JS2VAL_IS_NULL(argv[1]) && !JS2VAL_IS_UNDEFINED(argv[1])) {
			if (JS2VAL_IS_OBJECT(argv[1]) 
					&& (JS2VAL_TO_OBJECT(argv[1])->kind == SimpleInstanceKind)) {
				SimpleInstance *obj = checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(argv[1]));
				if (obj->type == meta->arrayClass) {
					ArrayInstance *arrInst = checked_cast<ArrayInstance *>(obj);
					uint32 length = getLength(meta, arrInst);
					js2val *argArray = new js2val[length];
					DEFINE_ARRAYROOTKEEPER(meta, rk, argArray, length);
					for (uint32 i = 0; i < length; i++)
						meta->arrayClass->ReadPublic(meta, &argv[1], meta->engine->numberToStringAtom(i), RunPhase, &argArray[i]);
					return meta->invokeFunction(fnInst, callThis, argArray, length, NULL);
				}
				else
					if (obj->type == meta->argumentsClass) {
						ArgumentsInstance *argInst = checked_cast<ArgumentsInstance *>(obj);
						uint32 length = getLength(meta, argInst);
						js2val *argArray = new js2val[length];
						DEFINE_ARRAYROOTKEEPER(meta, rk, argArray, length);
						for (uint32 i = 0; i < length; i++)
							meta->argumentsClass->ReadPublic(meta, &argv[1], meta->engine->numberToStringAtom(i), RunPhase, &argArray[i]);
						return meta->invokeFunction(fnInst, callThis, argArray, length, NULL);
					}
			}
			meta->reportError(Exception::typeError, "Function.apply passed a non-array or argument value", meta->engine->errorPos());
			return JS2VAL_VOID;
		}
		else
			return meta->invokeFunction(fnInst, callThis, NULL, 0, NULL);        
    }

    void initFunctionObject(JS2Metadata *meta)
    {
        FunctionData prototypeFunctions[] =
        {
            { "toString",            0, Function_toString },
            { "call",                1, Function_call  },
            { "apply",               2, Function_apply  },
            { NULL }
        };

        FunctionInstance *fnInst = new (meta) FunctionInstance(meta, meta->objectClass->prototype, meta->functionClass);
        DEFINE_ROOTKEEPER(meta, rk, fnInst);
        fnInst->fWrap = new FunctionWrapper(meta, true, new (meta) ParameterFrame(JS2VAL_INACCESSIBLE, true), meta->env);
        fnInst->fWrap->length = 0;
        fnInst->fWrap->bCon->emitOp(eReturnVoid, 0);

        meta->initBuiltinClass(meta->functionClass, NULL, Function_Constructor, Function_Constructor);
        meta->functionClass->prototype = OBJECT_TO_JS2VAL(fnInst);
        meta->initBuiltinClassPrototype(meta->functionClass, &prototypeFunctions[0]);
    }

}
}


