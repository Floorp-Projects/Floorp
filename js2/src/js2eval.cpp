
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
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif


#include "js2metadata.h"


#include "jsstddef.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsarena.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsscope.h"
#include "jsscript.h"

namespace JavaScript {
namespace MetaData {


    JSRuntime *gMonkeyRuntime;
    JSContext *gMonkeyContext;
    JSObject *gMonkeyGlobalObject;
    JSClass gMonkeyGlobalClass = { "MyClass", 0, JS_PropertyStub, JS_PropertyStub,
                                       JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
                                       JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub };

    JSClass gMonkeyMultinameClass = 
            { "Multiname", 0, JS_PropertyStub, JS_PropertyStub,
               JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
               JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub };

    // forward ref.
    static void LexicalReference_finalize(JSContext *cx, JSObject *obj);

    JSClass gMonkeyLexicalReferenceClass =
            { "LexicalReference", JSCLASS_HAS_PRIVATE, 
               JS_PropertyStub, JS_PropertyStub,
               JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
               JS_ResolveStub, JS_ConvertStub, LexicalReference_finalize };

    // member functions at global scope
    JSFunctionSpec jsfGlobal [] =
    {
	{ 0 }
    };

    // The Monkey error handler, simply throws back to JS2MetaData
    void MonkeyError(JSContext *cx, const char *message, JSErrorReport *report)
    {
        throw message;
    }



    /******************************************************************************
     LexicalReference
    ******************************************************************************/

    // finish constructing a LexicalReference
    static JSBool
    LexicalReference_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        ExecutionState *eState = static_cast<ExecutionState *>(JS_GetContextPrivate(cx));

        ASSERT(argc == 2);
        ASSERT(JSVAL_IS_BOOLEAN(argv[0]));  // the 'strict' flag (XXX what's that for?)
        ASSERT(JSVAL_IS_OBJECT(argv[1]));   // the multiname object
        
        JSObject *multiNameObj = JSVAL_TO_OBJECT(argv[1]);
        ASSERT(OBJ_GET_CLASS(cx, multiNameObj) == &gMonkeyMultinameClass); 
        Multiname *mName = static_cast<Multiname *>(JS_GetPrivate(cx, multiNameObj));

        if (!JS_SetPrivate(cx, obj, new LexicalReference(mName, eState->env, (JSVAL_TO_BOOLEAN(argv[0])) == JS_TRUE) ))
            return JS_FALSE;

        return JS_TRUE;
    }

    // finalize a LexicalReference - called by Monkey gc
    static void
    LexicalReference_finalize(JSContext *cx, JSObject *obj)
    {
        ASSERT(OBJ_GET_CLASS(cx, obj) == &gMonkeyLexicalReferenceClass);
        LexicalReference *lRef = static_cast<LexicalReference *>(JS_GetPrivate(cx, obj));
        if (lRef) delete lRef;
    }

    // Given a LexicalReference, read it's contents
    static JSBool
    readLexicalReference(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        ASSERT(OBJ_GET_CLASS(cx, obj) == &gMonkeyLexicalReferenceClass);
        ExecutionState *eState = static_cast<ExecutionState *>(JS_GetContextPrivate(cx));
        LexicalReference *lRef = static_cast<LexicalReference *>(JS_GetPrivate(cx, obj));

        eState->env->lexicalRead(eState, lRef->variableMultiname, RunPhase);
        return JS_TRUE;
    }

    // Write a value into a LexicalReference
    static JSBool
    writeLexicalReference(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        ASSERT(OBJ_GET_CLASS(cx, obj) == &gMonkeyLexicalReferenceClass);
        ExecutionState *eState = static_cast<ExecutionState *>(JS_GetContextPrivate(cx));
        LexicalReference *lRef = static_cast<LexicalReference *>(JS_GetPrivate(cx, obj));


        return JS_TRUE;
    }

    // member functions in a LexicalReference
    JSFunctionSpec jsfLexicalReference [] =
    {
        { "readReference",    readLexicalReference,    0, 0, 0 },
        { "writeReference",   writeLexicalReference,   0, 0, 0 },
	{ 0 }
    };



    /******************************************************************************
     Multiname
    ******************************************************************************/

    // finish constructing a Multiname
    static JSBool
    Multiname_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        ASSERT(argc >= 1);      // could be just the base name
        ASSERT(OBJ_GET_CLASS(cx, obj) == &gMonkeyMultinameClass);

        ASSERT(JSVAL_IS_STRING(argv[0]));

        // XXX use reserved slots instead
        if (!JS_SetProperty(cx, obj, "name", &argv[0]))
            return JS_FALSE;

        jsval qualifierVal = JSVAL_NULL;
        if (argc > 1) {
            JSObject *qualArray = JS_NewArrayObject(cx, argc - 1, &argv[1]);
            if (!qualArray)
                return JS_FALSE;
            qualifierVal = OBJECT_TO_JSVAL(qualArray);
        }
        if (!JS_SetProperty(cx, obj, "qualifiers", &qualifierVal))
            return JS_FALSE;

        return JS_TRUE;
    }

    // member functions in a Multiname
    JSFunctionSpec jsfMultiname [] =
    {
	{ 0 }
    };







    // Initialize the SpiderMonkey engine
    void JS2Metadata::initializeMonkey()
    {
        gMonkeyRuntime = JS_NewRuntime( 1000000L );
        if (!gMonkeyRuntime)
            throw "Monkey start failure";

        gMonkeyContext = JS_NewContext( gMonkeyRuntime, 8192 );
        if (!gMonkeyContext)
            throw "Monkey start failure";


        gMonkeyGlobalObject = JS_NewObject(gMonkeyContext, &gMonkeyGlobalClass, NULL, NULL);
        if (!gMonkeyGlobalObject)
            throw "Monkey start failure";

        JS_SetErrorReporter(gMonkeyContext, MonkeyError);

        JS_InitStandardClasses(gMonkeyContext, gMonkeyGlobalObject);

        JS_InitClass(gMonkeyContext, gMonkeyGlobalObject, NULL,
                     &gMonkeyLexicalReferenceClass, LexicalReference_constructor, 0,
                     NULL, jsfLexicalReference, NULL, NULL);

        JS_InitClass(gMonkeyContext, gMonkeyGlobalObject, NULL,
                     &gMonkeyMultinameClass, Multiname_constructor, 0,
                     NULL, jsfMultiname, NULL, NULL);

        JS_DefineFunctions(gMonkeyContext, gMonkeyGlobalObject, jsfGlobal);

    }

    // Execute a JS string against the given environment
    // Errors are thrown back to C++ by the error handler
    jsval JS2Metadata::execute(String *str, Environment *env, JS2Metadata *meta, size_t pos)
    {
        jsval retval;

        ExecutionState eState(gMonkeyContext, env, meta, pos);
        JS_SetContextPrivate(gMonkeyContext, &eState);

        JS_EvaluateUCScript(gMonkeyContext, gMonkeyGlobalObject, str->c_str(), str->length(), "file", 1, &retval);
        
        return retval;
    }

}; // namespace MetaData
}; // namespace Javascript