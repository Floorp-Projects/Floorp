
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


    JSClass gMonkeyGlobalClass = { "MyClass", 0, JS_PropertyStub, JS_PropertyStub,
                                       JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
                                       JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub };

    // forward refs.
    static void Multiname_finalize(JSContext *cx, JSObject *obj);
    static void LexicalReference_finalize(JSContext *cx, JSObject *obj);
    static void Namespace_finalize(JSContext *cx, JSObject *obj);
    static void JS2Object_finalize(JSContext *cx, JSObject *obj);

    JSClass gMonkeyMultinameClass = 
            { "Multiname", JSCLASS_HAS_PRIVATE, 
               JS_PropertyStub, JS_PropertyStub,
               JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
               JS_ResolveStub, JS_ConvertStub, Multiname_finalize };


    JSClass gMonkeyLexicalReferenceClass =
            { "LexicalReference", JSCLASS_HAS_PRIVATE, 
               JS_PropertyStub, JS_PropertyStub,
               JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
               JS_ResolveStub, JS_ConvertStub, LexicalReference_finalize };

    JSClass gMonkeyNamespaceClass =
            { "Namespace", JSCLASS_HAS_PRIVATE, 
               JS_PropertyStub, JS_PropertyStub,
               JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
               JS_ResolveStub, JS_ConvertStub, Namespace_finalize };

    JSClass gMonkeyJS2ObjectClass =
            { "JS2Object", JSCLASS_HAS_PRIVATE, 
               JS_PropertyStub, JS_PropertyStub,
               JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
               JS_ResolveStub, JS_ConvertStub, JS2Object_finalize };

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
        JS2Metadata *meta = static_cast<JS2Metadata *>(JS_GetContextPrivate(cx));

        ASSERT(argc == 1);                  // just the base name
        ASSERT(JSVAL_IS_STRING(argv[0]));

        JSString *str = JSVAL_TO_STRING(argv[0]);
        Multiname *mName = new Multiname(meta->world.identifiers[String(JS_GetStringChars(str), JS_GetStringLength(str))]);
        mName->addNamespace(&meta->cxt);

        if (!JS_SetPrivate(cx, obj, new LexicalReference(mName, &meta->env, meta->cxt.strict)))
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
        JS2Metadata *meta = static_cast<JS2Metadata *>(JS_GetContextPrivate(cx));
        LexicalReference *lRef = static_cast<LexicalReference *>(JS_GetPrivate(cx, obj));

        ASSERT(argc == 0);

        *rval = meta->env.lexicalRead(meta, lRef->variableMultiname, RunPhase);
        return JS_TRUE;
    }

    // Write a value into a LexicalReference
    static JSBool
    writeLexicalReference(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        ASSERT(OBJ_GET_CLASS(cx, obj) == &gMonkeyLexicalReferenceClass);
        JS2Metadata *meta = static_cast<JS2Metadata *>(JS_GetContextPrivate(cx));
        LexicalReference *lRef = static_cast<LexicalReference *>(JS_GetPrivate(cx, obj));

        ASSERT(argc == 1);

        meta->env.lexicalWrite(meta, lRef->variableMultiname, argv[0], !meta->cxt.strict, RunPhase);
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
        JS2Metadata *meta = static_cast<JS2Metadata *>(JS_GetContextPrivate(cx));

        ASSERT(argc == 1);              // just the base name

        ASSERT(JSVAL_IS_STRING(argv[0]));
        JSString *str = JSVAL_TO_STRING(argv[0]);

        Multiname *mName = new Multiname(meta->world.identifiers[String(JS_GetStringChars(str), JS_GetStringLength(str))]);
        if (!JS_SetPrivate(cx, obj, mName))
            return JS_FALSE;
        mName->addNamespace(&meta->cxt);

        return JS_TRUE;
    }

    // finalize a Multiname - called by Monkey gc
    static void
    Multiname_finalize(JSContext *cx, JSObject *obj)
    {
        ASSERT(OBJ_GET_CLASS(cx, obj) == &gMonkeyMultinameClass);
        Multiname *mName = static_cast<Multiname *>(JS_GetPrivate(cx, obj));
        if (mName) delete mName;
    }


    /******************************************************************************
     Namespace
    ******************************************************************************/

    // finish constructing a Namespace
    static JSBool
    Namespace_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        JS2Metadata *meta = static_cast<JS2Metadata *>(JS_GetContextPrivate(cx));
        ASSERT(argc == 1);
        ASSERT(JSVAL_IS_STRING(argv[0]));
        JSString *str = JSVAL_TO_STRING(argv[0]);

        Namespace *ns = new Namespace(meta->world.identifiers[String(JS_GetStringChars(str), JS_GetStringLength(str))]);
        if (!JS_SetPrivate(cx, obj, ns))
            return JS_FALSE;
        return JS_TRUE;
    }

    // finalize a Namespace - called by Monkey gc
    static void
    Namespace_finalize(JSContext *cx, JSObject *obj)
    {
        ASSERT(OBJ_GET_CLASS(cx, obj) == &gMonkeyNamespaceClass);
        Namespace *ns = static_cast<Namespace *>(JS_GetPrivate(cx, obj));
        if (ns) delete ns;
    }


   /******************************************************************************
     JS2Object
    ******************************************************************************/

    // finish constructing a JS2Object
    static JSBool
    JS2Object_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        JS2Metadata *meta = static_cast<JS2Metadata *>(JS_GetContextPrivate(cx));
        if (!JS_SetPrivate(cx, obj, meta->objectClass->construct()))
            return JS_FALSE;
        return JS_TRUE;
    }

    // finalize a Namespace - called by Monkey gc
    static void
    JS2Object_finalize(JSContext *cx, JSObject *obj)
    {
        ASSERT(OBJ_GET_CLASS(cx, obj) == &gMonkeyJS2ObjectClass);
        DynamicInstance *di = static_cast<DynamicInstance *>(JS_GetPrivate(cx, obj));
        if (di) delete di;
    }





    // Initialize the SpiderMonkey engine
    void JS2Metadata::initializeMonkey()
    {
        monkeyRuntime = JS_NewRuntime(1000000L);
        if (!monkeyRuntime)
            throw "Monkey start failure";

        monkeyContext = JS_NewContext(monkeyRuntime, 8192);
        if (!monkeyContext)
            throw "Monkey start failure";


        monkeyGlobalObject = JS_NewObject(monkeyContext, &gMonkeyGlobalClass, NULL, NULL);
        if (!monkeyGlobalObject)
            throw "Monkey start failure";

        JS_SetErrorReporter(monkeyContext, MonkeyError);

        if (!JS_InitStandardClasses(monkeyContext, monkeyGlobalObject))
            throw "Monkey start failure";

        JS_InitClass(monkeyContext, monkeyGlobalObject, NULL,
                     &gMonkeyLexicalReferenceClass, LexicalReference_constructor, 0,
                     NULL, jsfLexicalReference, NULL, NULL);

        JS_InitClass(monkeyContext, monkeyGlobalObject, NULL,
                     &gMonkeyMultinameClass, Multiname_constructor, 0,
                     NULL, NULL, NULL, NULL);

        JS_InitClass(monkeyContext, monkeyGlobalObject, NULL,
                     &gMonkeyNamespaceClass, Namespace_constructor, 0,
                     NULL, NULL, NULL, NULL);

        JS_InitClass(monkeyContext, monkeyGlobalObject, NULL,
                     &gMonkeyJS2ObjectClass, JS2Object_constructor, 0,
                     NULL, NULL, NULL, NULL);

        if (!JS_DefineFunctions(monkeyContext, monkeyGlobalObject, jsfGlobal))
            throw "Monkey start failure";
    }

    // Execute a JS string against the given environment
    // Errors are thrown back to C++ by the error handler
    jsval JS2Metadata::execute(String *str, size_t pos)
    {
        jsval retval;

        errorPos = pos;
        JS_SetContextPrivate(monkeyContext, this);

        JS_EvaluateUCScript(monkeyContext, monkeyGlobalObject, str->c_str(), str->length(), "file", 1, &retval);
        
        return retval;
    }

}; // namespace MetaData
}; // namespace Javascript