
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

    JSClass gMonkeyLexicalReferenceClass = { "LexicalReference", 0, JS_PropertyStub, JS_PropertyStub,
                                       JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
                                       JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub };

    static JSBool
    LexicalReference_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        ASSERT(argc == 3);
        ASSERT(JSVAL_IS_STRING(argv[0]));
        ASSERT(JSVAL_IS_BOOLEAN(argv[1]));
        ASSERT(JSVAL_IS_NULL(argv[2]) || JSVAL_IS_OBJECT(argv[2]));
        
        JSString *str = JSVAL_TO_STRING(argv[0]);
        if (!str)
            return JS_FALSE;
        
        if (!JS_SetProperty(cx, obj, "name", argv[0]))
            return JS_FALSE;


        return JS_TRUE;
    }

    static JSBool
    readReference(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        // for this reference, use the
        return JS_TRUE;
    }

    static JSBool
    writeReference(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
    {
        return JS_TRUE;
    }

    JSFunctionSpec jsfLexicalReference [] =
    {
        { "readReference",    readReference,    0, 0, 0 },
        { "writeReference",   writeReference,   0, 0, 0 },
	{ 0 }
    };

    JSFunctionSpec jsfGlobal [] =
    {
	{ 0 }
    };

    void MonkeyError(JSContext *cx, const char *message, JSErrorReport *report)
    {
        throw message;
    }

    void JS2Metadata::initializeMonkey( )
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
                     NULL, jsfLexicalReference,
                     NULL, NULL);

        JS_DefineFunctions(gMonkeyContext, gMonkeyGlobalObject, jsfGlobal);

    }

    
    jsval JS2Metadata::execute(String *str)
    {
        jsval retval;
        JS_EvaluateUCScript(gMonkeyContext, gMonkeyGlobalObject, str->c_str(), str->length(), "file", 1, &retval);
        
        return retval;
    }

}; // namespace MetaData
}; // namespace Javascript