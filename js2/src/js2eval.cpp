
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


   JSRuntime *gJavaScriptRuntime;
   JSContext *gJavaScriptContext;
   JSObject *gJavaScriptGlobalObject;
   JSClass gJavaScriptGlobalClass = { "MyClass", 0, JS_PropertyStub, JS_PropertyStub,
                                       JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
                                       JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub };


    void JS2Metadata::initializeMonkey( )
    {
        gJavaScriptRuntime = JS_NewRuntime( 1000000L );
        if (gJavaScriptRuntime) {
            gJavaScriptContext = JS_NewContext( gJavaScriptRuntime, 8192 );
            if (gJavaScriptContext) {
                gJavaScriptGlobalObject = JS_NewObject(gJavaScriptContext, &gJavaScriptGlobalClass, NULL, NULL);
                if (gJavaScriptGlobalObject)
                    JS_SetGlobalObject(gJavaScriptContext, gJavaScriptGlobalObject);
            }
        }    
    }
    
    jsval JS2Metadata::execute(String *str)
    {
        jsval retval;
        JS_EvaluateUCScript(gJavaScriptContext, gJavaScriptGlobalObject, str->c_str(), str->length(), "file", 1, &retval);
        return retval;
    }

}; // namespace MetaData
}; // namespace Javascript