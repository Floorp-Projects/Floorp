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
            JS2Class *exprType;
            meta->ValidateExpression(&meta->cxt, meta->env, fnExpr);
            meta->SetupExprNode(meta->env, RunPhase, fnExpr, &exprType);
            ASSERT(fnExpr);
            return OBJECT_TO_JS2VAL(fnExpr->obj);
        }
        else {  // construct an empty function wrapper
            js2val thatValue = OBJECT_TO_JS2VAL(new FunctionInstance(meta, meta->functionClass->prototype, meta->functionClass));
            FunctionInstance *fnInst = checked_cast<FunctionInstance *>(JS2VAL_TO_OBJECT(thatValue));
            fnInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true));
            fnInst->fWrap->bCon->emitOp(eReturnVoid, meta->engine->errorPos());
            fnInst->writeProperty(meta, meta->engine->length_StringAtom, INT_TO_JS2VAL(0), DynamicPropertyValue::READONLY);
            return thatValue;
        }
    }
        
    static js2val Function_toString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != PrototypeInstanceKind)
                || ((checked_cast<PrototypeInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->functionClass))
            meta->reportError(Exception::typeError, "Function.toString called on something other than a function thing", meta->engine->errorPos());
//        FunctionInstance *fnInst = checked_cast<FunctionInstance *>(JS2VAL_TO_OBJECT(thisValue));
        return STRING_TO_JS2VAL(meta->engine->Function_StringAtom);
    }

    static js2val Function_valueOf(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != PrototypeInstanceKind)
                || ((checked_cast<PrototypeInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->functionClass))
            meta->reportError(Exception::typeError, "Function.valueOf called on something other than a function thing", meta->engine->errorPos());
//        FunctionInstance *nfInst = checked_cast<FunctionInstance *>(JS2VAL_TO_OBJECT(thisValue));
        return STRING_TO_JS2VAL(meta->engine->Function_StringAtom);
    }

    void initFunctionObject(JS2Metadata *meta)
    {
        FunctionData prototypeFunctions[] =
        {
            { "toString",            0, Function_toString },
            { "valueOf",             0, Function_valueOf  },
            { NULL }
        };

        meta->functionClass->prototype = new FunctionInstance(meta, meta->objectClass->prototype, meta->functionClass);
        meta->initBuiltinClass(meta->functionClass, &prototypeFunctions[0], NULL, Function_Constructor, Function_Constructor);
    }

}
}


