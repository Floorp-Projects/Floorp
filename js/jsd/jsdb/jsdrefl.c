/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
* Reflects JSD api into JavaScript
*/

#include "jsdbpriv.h"

/***************************************************************************/
/* Non-const Properties */

static const char _str_ThreadStateHandle[]  = "ThreadStateHandle";
static const char _str_InterruptSet[]       = "InterruptSet";
static const char _str_Evaluating[]         = "Evaluating";
static const char _str_DebuggerDepth[]      = "DebuggerDepth";
static const char _str_ReturnExpression[]   = "ReturnExpression";

static jsval _valFalse = JSVAL_FALSE;
static jsval _valTrue  = JSVAL_TRUE;
static jsval _valNull  = JSVAL_NULL;


enum jsd_prop_ids
{
    JSD_PROP_ID_THREADSTATE_HANDLE  = -1,
    JSD_PROP_ID_INTERRUPT_SET       = -2,
    JSD_PROP_ID_EVALUATING          = -3,
    JSD_PROP_ID_DEBUGGER_DEPTH      = -4,
    JSD_PROP_ID_RETURN_EXPRESSION   = -5,
    COMPLETLY_IGNORED_ID            = 0 /* just to avoid tracking the comma */
};

static JSPropertySpec jsd_properties[] = {
    {_str_ThreadStateHandle, JSD_PROP_ID_THREADSTATE_HANDLE, JSPROP_ENUMERATE|JSPROP_PERMANENT},
    {_str_InterruptSet,      JSD_PROP_ID_INTERRUPT_SET,      JSPROP_ENUMERATE|JSPROP_PERMANENT},
    {_str_Evaluating,        JSD_PROP_ID_EVALUATING,         JSPROP_ENUMERATE|JSPROP_PERMANENT},
    {_str_DebuggerDepth,     JSD_PROP_ID_DEBUGGER_DEPTH,     JSPROP_ENUMERATE|JSPROP_PERMANENT},
    {_str_ReturnExpression,  JSD_PROP_ID_RETURN_EXPRESSION,  JSPROP_ENUMERATE|JSPROP_PERMANENT},
    {0}
};

static JSBool
_defineNonConstProperties(JSDB_Data* data)
{
    JSContext* cx = data->cxDebugger;
    JSObject*  ob = data->jsdOb;
    jsval val;
    JSBool ignored;

    if(!JS_DefineProperties(cx, ob, jsd_properties))
        return JS_FALSE;

    /* set initial state of some of the properties */

    if(!JS_SetProperty(cx, ob, _str_ThreadStateHandle, &_valNull))
        return JS_FALSE;

    if(!JS_SetProperty(cx, ob, _str_InterruptSet, &_valFalse))
        return JS_FALSE;

    if(!JS_SetProperty(cx, ob, _str_Evaluating, &_valFalse))
        return JS_FALSE;

    val = INT_TO_JSVAL(data->debuggerDepth);
    if(!JS_SetProperty(cx, ob, _str_DebuggerDepth, &val))
        return JS_FALSE;
    if(!JS_SetPropertyAttributes(cx, ob, _str_DebuggerDepth,
                                 JSPROP_READONLY |
                                 JSPROP_ENUMERATE |
                                 JSPROP_PERMANENT,
                                 &ignored))
        return JS_FALSE;

    if(!JS_SetProperty(cx, ob, _str_ReturnExpression, &_valNull))
        return JS_FALSE;

    return JS_TRUE;
}

JSBool
jsdb_SetThreadState(JSDB_Data* data, JSDThreadState* jsdthreadstate)
{
    jsval val;

    data->jsdthreadstate = jsdthreadstate;
    if(jsdthreadstate)
        val = P2H_THREADSTATE(data->cxDebugger, jsdthreadstate);
    else
        val = JSVAL_NULL;

    return JS_SetProperty(data->cxDebugger, data->jsdOb,
                          _str_ThreadStateHandle, &val);
}

/***************************************************************************/
/*
*  System to store JSD_xxx handles in jsvals. This supports tracking the
*  handle's type - both for debugging and for automatic 'dropping' of
*  reference counted handle types (e.g. JSDValue).
*/

typedef struct JSDBHandle
{
    void* ptr;
    JSDBHandleType type;
} JSDBHandle;

static const char _str_HandleNameString[]   = "handle_name";

JS_STATIC_DLL_CALLBACK(void)
handle_finalize(JSContext *cx, JSObject *obj)
{
    JSDBHandle* p;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    p = (JSDBHandle*) JS_GetPrivate(cx, obj);
    if(p)
    {
        switch(p->type)
        {
          case JSDB_VALUE:
            JSD_DropValue(data->jsdcTarget, (JSDValue*) p->ptr);
            break;
          case JSDB_PROPERTY:
            JSD_DropProperty(data->jsdcTarget, (JSDProperty*) p->ptr);
            break;
          default:
            break;
        }
        free(p);
    }
    else
        JS_ASSERT(0);
}

JSClass jsdb_HandleClass = {
    "JSDHandle",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   handle_finalize
};

JS_STATIC_DLL_CALLBACK(JSBool)
handle_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if(!JS_InstanceOf(cx, obj, &jsdb_HandleClass, argv) ||
       !JS_GetProperty(cx, obj, _str_HandleNameString, rval))
    {
        JS_ASSERT(0);
        return JS_FALSE;
    }
    return JS_TRUE;
}

static JSFunctionSpec handle_methods[] = {
    {"toString",   handle_toString,        0},
    {0}
};

void*
jsdb_HandleValToPointer(JSContext *cx, jsval val, JSDBHandleType type)
{
    JSDBHandle* p;
    JSObject *obj;

    if(!JSVAL_IS_OBJECT(val) ||
       !(obj = JSVAL_TO_OBJECT(val)) ||
       !JS_InstanceOf(cx, obj, &jsdb_HandleClass, NULL) ||
       !(p = (JSDBHandle*) JS_GetPrivate(cx, obj)))
    {
/*         JS_ASSERT(0); */
        return NULL;
    }
    JS_ASSERT(p->ptr);
    JS_ASSERT(p->type == type);
    return p->ptr;
}

jsval
jsdb_PointerToNewHandleVal(JSContext *cx, void* ptr, JSDBHandleType type)
{
    JSDBHandle* p;
    JSObject* obj;
    char* name;
    char* type_name;
    JSString* name_str;

    /* OK to fail silently on NULL pointer */
    if(!ptr)
        return JSVAL_NULL;

    if(!(p = (JSDBHandle*)malloc(sizeof(JSDBHandle))))
    {
        JS_ASSERT(0);
        return JSVAL_NULL;
    }
    JS_ASSERT(!(((int)p) & 1));    /* must be 2-byte-aligned */

    p->ptr = ptr;
    p->type = type;

    if(!(obj = JS_NewObject(cx, &jsdb_HandleClass, NULL, NULL)) ||
       !JS_SetPrivate(cx, obj, p))
    {
        JS_ASSERT(0);
        return JSVAL_NULL;
    }

    switch(type)
    {
      case JSDB_GENERIC:
        type_name = "GENERIC";
        break;
      case JSDB_CONTEXT:
        type_name = "CONTEXT";
        break;
      case JSDB_SCRIPT:
        type_name = "SCRIPT";
        break;
      case JSDB_SOURCETEXT:
        type_name = "SOURCETEXT";
        break;
      case JSDB_THREADSTATE:
        type_name = "THREADSTATE";
        break;
      case JSDB_STACKFRAMEINFO:
        type_name = "STACKFRAMEINFO";
        break;
      case JSDB_VALUE:
        type_name = "VALUE";
        break;
      case JSDB_PROPERTY:
        type_name = "PROPERTY";
        break;
      case JSDB_OBJECT:
        type_name = "OBJECT";
        break;
      default:
        JS_ASSERT(0);
        type_name = "BOGUS";
        break;
    }

    name = JS_smprintf("%s:%x", type_name, ptr);
    if(!name ||
       !(name_str = JS_NewString(cx, name, strlen(name))) ||
       !JS_DefineProperty(cx, obj,
                          _str_HandleNameString,
                          STRING_TO_JSVAL(name_str),
                          NULL, NULL,
                          JSPROP_READONLY|JSPROP_PERMANENT))
    {
        JS_ASSERT(0);
        return JSVAL_NULL;
    }
    return OBJECT_TO_JSVAL(obj);
}

JSBool _initHandleSystem(JSDB_Data* data)
{
    return (JSBool) JS_InitClass(data->cxDebugger, data->globDebugger,
                                 NULL, &jsdb_HandleClass, NULL, 0,
                                 NULL, handle_methods, NULL, NULL);
}

/***************************************************************************/

JSBool
jsdb_EvalReturnExpression(JSDB_Data* data, jsval* rval)
{
    JSDStackFrameInfo* frame;
    jsval expressionVal;
    JSString* expressionString;
    JSContext* cx = data->cxDebugger;
    JSBool result = JS_FALSE;

    frame = JSD_GetStackFrame(data->jsdcTarget, data->jsdthreadstate);
    if(!frame)
        return JS_FALSE;

    if(!JS_GetProperty(cx, data->jsdOb, _str_ReturnExpression, &expressionVal))
        return JS_FALSE;

    if(!(expressionString = JS_ValueToString(cx, expressionVal)))
        return JS_FALSE;

    JS_SetProperty(cx, data->jsdOb, _str_Evaluating, &_valTrue);
    if(JSD_EvaluateScriptInStackFrame(data->jsdcTarget,
                                      data->jsdthreadstate,
                                      frame,
                                      JS_GetStringBytes(expressionString),
                                      JS_GetStringLength(expressionString),
                                      "ReturnExpressionEval", 1, rval))
    {
        result = JS_TRUE;
    }
    JS_SetProperty(cx, data->jsdOb, _str_Evaluating, &_valFalse);
    JS_SetProperty(cx, data->jsdOb, _str_ReturnExpression, &_valNull);
    return result;
}

/***************************************************************************/
/* High Level calls */

JS_STATIC_DLL_CALLBACK(JSBool)
GetMajorVersion(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = INT_TO_JSVAL(JSD_GetMajorVersion());
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetMinorVersion(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = INT_TO_JSVAL(JSD_GetMinorVersion());
    return JS_TRUE;
}

/***************************************************************************/
/* Script functions */

JS_STATIC_DLL_CALLBACK(JSBool)
LockScriptSubsystem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);
    JSD_LockScriptSubsystem(data->jsdcTarget);
    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
UnlockScriptSubsystem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IterateScripts(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *iterp = NULL;
    JSDScript *script;
    JSFunction *fun;
    jsval argv0;
    int count = 0;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(fun = JS_ValueToFunction(cx, argv[0])))
    {
        JS_ReportError(cx, "IterateScripts requires a function param");
        return JS_FALSE;
    }

    /* We pass along any additional args, so patch and leverage argv */
    argv0 = argv[0];
    JSD_LockScriptSubsystem(data->jsdcTarget);
    while(NULL != (script = JSD_IterateScripts(data->jsdcTarget, &iterp)))
    {
        jsval retval;
        JSBool another;
        argv[0] = P2H_SCRIPT(cx, script);

        if(!JS_CallFunction(cx, NULL, fun, argc, argv, &retval))
            break;
        count++ ;
        if(!JS_ValueToBoolean(cx, retval, &another) || !another)
            break;
    }
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    argv[0] = argv0;

    *rval = INT_TO_JSVAL(count);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsActiveScript(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *jsdscript;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 1 || !(jsdscript = H2P_SCRIPT(cx, argv[0])))
    {
        JS_ReportError(cx, "IsActiveScript requires script handle");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);

    *rval = active ? JSVAL_TRUE : JSVAL_FALSE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetScriptFilename(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *jsdscript;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 1 || !(jsdscript = H2P_SCRIPT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetScriptFilename requires script handle");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    if(! active )
    {
        printf("%d\n", (int)jsdscript);
        JS_ReportError(cx, "GetScriptFilename passed inactive script");
        return JS_FALSE;
    }

    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,
                JSD_GetScriptFilename(data->jsdcTarget, jsdscript)));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetScriptFunctionName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *jsdscript;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 1 || !(jsdscript = H2P_SCRIPT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetScriptFunctionName requires script handle");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    if(! active )
    {
        JS_ReportError(cx, "GetScriptFunctionName passed inactive script");
        return JS_FALSE;
    }

    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,
                JSD_GetScriptFunctionName(data->jsdcTarget, jsdscript)));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetScriptBaseLineNumber(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *jsdscript;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 1 || !(jsdscript = H2P_SCRIPT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetScriptBaseLineNumber requires script handle");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    if(! active )
    {
        JS_ReportError(cx, "GetScriptBaseLineNumber passed inactive script");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetScriptBaseLineNumber(data->jsdcTarget, jsdscript));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetScriptLineExtent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *jsdscript;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 1 || !(jsdscript = H2P_SCRIPT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetScriptLineExtent requires script handle");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    if(! active )
    {
        JS_ReportError(cx, "GetScriptLineExtent passed inactive script");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetScriptLineExtent(data->jsdcTarget, jsdscript));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
SetScriptHook(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval oldHook;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 ||
       !(JSVAL_IS_NULL(argv[0]) ||
         NULL != JS_ValueToFunction(cx, argv[0])))
    {
        JS_ReportError(cx, "SetScriptHook requires a function or null param");
        return JS_FALSE;
    }

    oldHook = data->jsScriptHook;
    data->jsScriptHook = argv[0];

    *rval = oldHook;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetScriptHook(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    *rval = data->jsScriptHook;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetClosestPC(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int32 line;
    JSDScript *jsdscript;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 2 || !(jsdscript = H2P_SCRIPT(cx, argv[0])) ||
       ! JS_ValueToInt32(cx, argv[1], &line))
    {
        JS_ReportError(cx, "GetClosestPC requires script handle and a line number");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    if(! active )
    {
        JS_ReportError(cx, "GetClosestPC passed inactive script");
        return JS_FALSE;
    }

    *rval = P2H_GENERIC(cx, (void*)
                        JSD_GetClosestPC(data->jsdcTarget, jsdscript, line));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetClosestLine(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *jsdscript;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 2 || !(jsdscript = H2P_SCRIPT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetClosestLine requires script handle and a line number");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    if(! active );
    {
        JS_ReportError(cx, "GetClosestLine passed inactive script");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetClosestLine(data->jsdcTarget, jsdscript,
                                 (jsuword) H2P_GENERIC(cx, argv[1])));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
SetExecutionHook(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval oldHook;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 ||
       !(JSVAL_IS_NULL(argv[0]) ||
         NULL != JS_ValueToFunction(cx, argv[0])))
    {
        JS_ReportError(cx, "SetExecutionHook requires a function or null param");
        return JS_FALSE;
    }

    oldHook = data->jsExecutionHook;
    data->jsExecutionHook = argv[0];

    *rval = oldHook;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetExecutionHook(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    *rval = data->jsExecutionHook;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
SendInterrupt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval val = JSVAL_TRUE;
    JSBool success;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);


    success = JSD_SetInterruptHook(data->jsdcTarget, jsdb_ExecHookHandler, data);
    if(success)
        success = JS_SetProperty(data->cxDebugger, data->jsdOb,
                                 _str_InterruptSet, &val);
    return success;
}

JS_STATIC_DLL_CALLBACK(JSBool)
ClearInterrupt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval val = JSVAL_FALSE;
    JSBool success;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    success = JSD_ClearInterruptHook(data->jsdcTarget);
    if(success)
        success = JS_SetProperty(data->cxDebugger, data->jsdOb,
                                 _str_InterruptSet, &val);
    return success;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetCountOfStackFrames(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    *rval = INT_TO_JSVAL(JSD_GetCountOfStackFrames(data->jsdcTarget,
                                                   data->jsdthreadstate));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetStackFrame(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    *rval = P2H_STACKFRAMEINFO(cx, JSD_GetStackFrame(data->jsdcTarget,
                                                     data->jsdthreadstate));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetCallingStackFrame(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDStackFrameInfo* jsdframe;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdframe = H2P_STACKFRAMEINFO(cx, argv[0])))
    {
        JS_ReportError(cx, "GetCallingStackFrame requires stackframe handle");
        return JS_FALSE;
    }

    *rval = P2H_STACKFRAMEINFO(cx, JSD_GetCallingStackFrame(data->jsdcTarget,
                                                            data->jsdthreadstate,
                                                            jsdframe));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetScriptForStackFrame(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDStackFrameInfo* jsdframe;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdframe = H2P_STACKFRAMEINFO(cx, argv[0])))
    {
        JS_ReportError(cx, "GetScriptForStackFrame requires stackframe handle");
        return JS_FALSE;
    }

    *rval = P2H_SCRIPT(cx, JSD_GetScriptForStackFrame(data->jsdcTarget,
                                                      data->jsdthreadstate,
                                                      jsdframe));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetPCForStackFrame(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDStackFrameInfo* jsdframe;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdframe = H2P_STACKFRAMEINFO(cx, argv[0])))
    {
        JS_ReportError(cx, "GetPCForStackFrame requires stackframe handle");
        return JS_FALSE;
    }

    *rval = P2H_GENERIC(cx, (void*) JSD_GetPCForStackFrame(data->jsdcTarget,
                                                           data->jsdthreadstate,
                                                           jsdframe));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
EvaluateScriptInStackFrame(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    static char default_filename[] = "jsdb_show";
    JSDStackFrameInfo* jsdframe;
    jsval foreignAnswerVal;
    JSString* foreignAnswerString;
    JSString* textJSString;
    char* filename;
    int32 lineno;
    JSBool retVal = JS_FALSE;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdframe = H2P_STACKFRAMEINFO(cx, argv[0])))
    {
        JS_ReportError(cx, "EvaluateScriptInStackFrame requires stackframe handle");
        return JS_FALSE;
    }

    if(argc < 2 || !(textJSString = JS_ValueToString(cx, argv[1])))
    {
        JS_ReportError(cx, "EvaluateScriptInStackFrame requires source text as a second param");
        return JS_FALSE;
    }

    if(argc < 3)
        filename = default_filename;
    else
    {
        JSString* filenameJSString;
        if(!(filenameJSString = JS_ValueToString(cx, argv[2])))
        {
            JS_ReportError(cx, "EvaluateScriptInStackFrame passed non-string filename as 3rd param");
            return JS_FALSE;
        }
        filename = JS_GetStringBytes(filenameJSString);
    }

    if(argc < 4)
        lineno = 1;
    else
    {
        if(!JS_ValueToInt32(cx, argv[3], &lineno))
        {
            JS_ReportError(cx, "EvaluateScriptInStackFrame passed non-int lineno as 4th param");
            return JS_FALSE;
        }
    }

    JS_SetProperty(cx, data->jsdOb, _str_Evaluating, &_valTrue);

    if(JSD_EvaluateScriptInStackFrame(data->jsdcTarget,
                                      data->jsdthreadstate,
                                      jsdframe,
                                      JS_GetStringBytes(textJSString),
                                      JS_GetStringLength(textJSString),
                                      filename, lineno, &foreignAnswerVal))
    {
        if(NULL != (foreignAnswerString =
            JSD_ValToStringInStackFrame(data->jsdcTarget,
                                        data->jsdthreadstate,
                                        jsdframe,
                                        foreignAnswerVal)))
        {
            *rval = STRING_TO_JSVAL(
                        JS_NewStringCopyZ(cx,
                                      JS_GetStringBytes(foreignAnswerString)));
            retVal = JS_TRUE;
        }

    }

    JS_SetProperty(cx, data->jsdOb, _str_Evaluating, &_valFalse);

    return retVal;
}


JS_STATIC_DLL_CALLBACK(JSBool)
EvaluateScriptInStackFrameToValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    static char default_filename[] = "jsdb_show";
    JSDStackFrameInfo* jsdframe;
    jsval foreignAnswerVal;
    JSString* textJSString;
    char* filename;
    int32 lineno;
    JSBool retVal = JS_FALSE;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdframe = H2P_STACKFRAMEINFO(cx, argv[0])))
    {
        JS_ReportError(cx, "EvaluateScriptInStackFrameToValue requires stackframe handle");
        return JS_FALSE;
    }

    if(argc < 2 || !(textJSString = JS_ValueToString(cx, argv[1])))
    {
        JS_ReportError(cx, "EvaluateScriptInStackFrameToValue requires source text as a second param");
        return JS_FALSE;
    }

    if(argc < 3)
        filename = default_filename;
    else
    {
        JSString* filenameJSString;
        if(!(filenameJSString = JS_ValueToString(cx, argv[2])))
        {
            JS_ReportError(cx, "EvaluateScriptInStackFrameToValue passed non-string filename as 3rd param");
            return JS_FALSE;
        }
        filename = JS_GetStringBytes(filenameJSString);
    }

    if(argc < 4)
        lineno = 1;
    else
    {
        if(!JS_ValueToInt32(cx, argv[3], &lineno))
        {
            JS_ReportError(cx, "EvaluateScriptInStackFrameToValue passed non-int lineno as 4th param");
            return JS_FALSE;
        }
    }

    JS_SetProperty(cx, data->jsdOb, _str_Evaluating, &_valTrue);

    if(JSD_EvaluateScriptInStackFrame(data->jsdcTarget,
                                      data->jsdthreadstate,
                                      jsdframe,
                                      JS_GetStringBytes(textJSString),
                                      JS_GetStringLength(textJSString),
                                      filename, lineno, &foreignAnswerVal))
    {
        *rval = P2H_VALUE(cx, JSD_NewValue(data->jsdcTarget, foreignAnswerVal));
        retVal = JS_TRUE;
    }

    JS_SetProperty(cx, data->jsdOb, _str_Evaluating, &_valFalse);

    return retVal;
}

JS_STATIC_DLL_CALLBACK(JSBool)
SetTrap(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *jsdscript;
    jsuword pc;
    JSBool success;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 1 || !(jsdscript = H2P_SCRIPT(cx, argv[0])))
    {
        JS_ReportError(cx, "SetTrap requires script handle as first arg");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    if(! active )
    {
        JS_ReportError(cx, "SetTrap passed inactive script");
        return JS_FALSE;
    }

    if(argc < 2 || 0 == (pc = (jsuword) H2P_GENERIC(cx, argv[1])))
    {
        JS_ReportError(cx, "SetTrap requires pc handle as second arg");
        return JS_FALSE;
    }

    success = JSD_SetExecutionHook(data->jsdcTarget, jsdscript, pc,
                                   jsdb_ExecHookHandler, data);
    *rval = success ? JSVAL_TRUE : JSVAL_FALSE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
ClearTrap(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *jsdscript;
    jsuword pc;
    JSBool success;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 1 || !(jsdscript = H2P_SCRIPT(cx, argv[0])))
    {
        JS_ReportError(cx, "ClearTrap requires script handle as first arg");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    if(! active )
    {
        JS_ReportError(cx, "ClearTrap passed inactive script");
        return JS_FALSE;
    }

    if(argc < 2 || 0 == (pc = (jsuword) H2P_GENERIC(cx, argv[1])))
    {
        JS_ReportError(cx, "ClearTrap requires pc handle as second arg");
        return JS_FALSE;
    }

    success = JSD_ClearExecutionHook(data->jsdcTarget, jsdscript, pc);
    *rval = success ? JSVAL_TRUE : JSVAL_FALSE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
ClearAllTrapsForScript(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDScript *jsdscript;
    JSBool success;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JSBool active;

    JS_ASSERT(data);

    if(argc < 1 || !(jsdscript = H2P_SCRIPT(cx, argv[0])))
    {
        JS_ReportError(cx, "ClearAllTrapsForScript script handle as first arg");
        return JS_FALSE;
    }

    JSD_LockScriptSubsystem(data->jsdcTarget);
    active = JSD_IsActiveScript(data->jsdcTarget, jsdscript);
    JSD_UnlockScriptSubsystem(data->jsdcTarget);
    if(! active )
    {
        JS_ReportError(cx, "GetClosestLine passed inactive script");
        return JS_FALSE;
    }

    success = JSD_ClearAllExecutionHooksForScript(data->jsdcTarget, jsdscript);
    *rval = success ? JSVAL_TRUE : JSVAL_FALSE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
ClearAllTraps(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool success;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    success = JSD_ClearAllExecutionHooks(data->jsdcTarget);
    *rval = success ? JSVAL_TRUE : JSVAL_FALSE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
SetErrorReporterHook(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval oldHook;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 ||
       !(JSVAL_IS_NULL(argv[0]) ||
         NULL != JS_ValueToFunction(cx, argv[0])))
    {
        JS_ReportError(cx, "SetErrorReporterHook requires a function or null param");
        return JS_FALSE;
    }

    oldHook = data->jsErrorReporterHook;
    data->jsErrorReporterHook = argv[0];

    *rval = oldHook;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetException(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    *rval = P2H_VALUE(cx, JSD_GetException(data->jsdcTarget,
                                           data->jsdthreadstate));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
SetException(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "SetException requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_SetException(data->jsdcTarget,
                                              data->jsdthreadstate,
                                              jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
ClearException(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    *rval = BOOLEAN_TO_JSVAL(JSD_SetException(data->jsdcTarget,
                                              data->jsdthreadstate,
                                              NULL));
    return JS_TRUE;
}

/***************************************************************************/
/* Source Text functions */

JS_STATIC_DLL_CALLBACK(JSBool)
LockSourceTextSubsystem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);
    JSD_LockSourceTextSubsystem(data->jsdcTarget);
    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
UnlockSourceTextSubsystem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);
    JSD_UnlockSourceTextSubsystem(data->jsdcTarget);
    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IterateSources(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDSourceText *iterp = NULL;
    JSDSourceText *jsdsrc;
    JSFunction *fun;
    jsval argv0;
    int count = 0;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(fun = JS_ValueToFunction(cx, argv[0])))
    {
        JS_ReportError(cx, "IterateSources requires a function param");
        return JS_FALSE;
    }

    /* We pass along any additional args, so patch and leverage argv */
    argv0 = argv[0];
    while(NULL != (jsdsrc = JSD_IterateSources(data->jsdcTarget, &iterp)))
    {
        jsval retval;
        JSBool another;

        argv[0] = P2H_SOURCETEXT(cx, jsdsrc);

        if(!JS_CallFunction(cx, NULL, fun, argc, argv, &retval))
            break;
        count++ ;
        if(!JS_ValueToBoolean(cx, retval, &another) || !another)
            break;
    }
    argv[0] = argv0;

    *rval = INT_TO_JSVAL(count);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
FindSourceForURL(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString* jsstr;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsstr = JS_ValueToString(cx, argv[0])))
    {
        JS_ReportError(cx, "FindSourceForURL requires a URL (filename) string");
        return JS_FALSE;
    }
    *rval = P2H_SOURCETEXT(cx, JSD_FindSourceForURL(data->jsdcTarget,
                                                    JS_GetStringBytes(jsstr)));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetSourceURL(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDSourceText* jsdsrc;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdsrc = H2P_SOURCETEXT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetSourceURL requires sourcetext handle");
        return JS_FALSE;
    }

    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,
                JSD_GetSourceURL(data->jsdcTarget, jsdsrc)));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetSourceText(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDSourceText* jsdsrc;
    const char* ptr;
    int len;

    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdsrc = H2P_SOURCETEXT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetSourceText requires sourcetext handle");
        return JS_FALSE;
    }

    if(JSD_GetSourceText(data->jsdcTarget, jsdsrc, &ptr, &len))
        *rval = STRING_TO_JSVAL(JS_NewStringCopyN(cx, ptr, len));
    else
        *rval = JSVAL_NULL;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetSourceStatus(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDSourceText* jsdsrc;

    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdsrc = H2P_SOURCETEXT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetSourceStatus requires sourcetext handle");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetSourceStatus(data->jsdcTarget, jsdsrc));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetSourceAlterCount(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDSourceText* jsdsrc;

    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdsrc = H2P_SOURCETEXT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetSourceAlterCount requires sourcetext handle");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetSourceAlterCount(data->jsdcTarget, jsdsrc));
    return JS_TRUE;
}
/***************************************************************************/
/* Value and Property functions */

JS_STATIC_DLL_CALLBACK(JSBool)
GetCallObjectForStackFrame(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDStackFrameInfo* jsdframe;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdframe = H2P_STACKFRAMEINFO(cx, argv[0])))
    {
        JS_ReportError(cx, "GetCallObjectForStackFrame requires stackframe handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetCallObjectForStackFrame(data->jsdcTarget,
                                                         data->jsdthreadstate,
                                                         jsdframe));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetScopeChainForStackFrame(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDStackFrameInfo* jsdframe;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdframe = H2P_STACKFRAMEINFO(cx, argv[0])))
    {
        JS_ReportError(cx, "GetScopeChainForStackFrame requires stackframe handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetScopeChainForStackFrame(data->jsdcTarget,
                                                         data->jsdthreadstate,
                                                         jsdframe));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetThisForStackFrame(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDStackFrameInfo* jsdframe;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdframe = H2P_STACKFRAMEINFO(cx, argv[0])))
    {
        JS_ReportError(cx, "GetThisForStackFrame requires stackframe handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetThisForStackFrame(data->jsdcTarget,
                                                   data->jsdthreadstate,
                                                   jsdframe));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
RefreshValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "RefreshValue requires value handle");
        return JS_FALSE;
    }

    JSD_RefreshValue(data->jsdcTarget, jsdval);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueObject(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueObject requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueObject(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueNumber(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueNumber requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueNumber(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueInt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueInt requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueInt(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueDouble(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueDouble requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueDouble(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueString requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueString(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueBoolean(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueBoolean requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueBoolean(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueNull(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueNull requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueNull(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueVoid(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueVoid requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueVoid(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValuePrimitive(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValuePrimitive requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValuePrimitive(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueFunction(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueFunction requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueFunction(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IsValueNative(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IsValueNative requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_IsValueNative(data->jsdcTarget, jsdval));
    return JS_TRUE;
}


JS_STATIC_DLL_CALLBACK(JSBool)
GetValueBoolean(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueBoolean requires value handle");
        return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(JSD_GetValueBoolean(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValueInt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueInt requires value handle");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetValueInt(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValueDouble(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueDouble requires value handle");
        return JS_FALSE;
    }

    *rval = DOUBLE_TO_JSVAL(JSD_GetValueDouble(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValueString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueString requires value handle");
        return JS_FALSE;
    }

    *rval = STRING_TO_JSVAL(JSD_GetValueString(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValueFunctionName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueFunctionName requires value handle");
        return JS_FALSE;
    }

    *rval = STRING_TO_JSVAL(JSD_GetValueFunctionName(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetCountOfProperties(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetCountOfProperties requires value handle");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetCountOfProperties(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IterateProperties(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDProperty* iterp = NULL;
    JSDProperty* jsdprop;
    JSDValue* jsdval;
    JSFunction *fun;
    jsval argv1;
    int count = 0;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "IterateProperties requires value handle first param");
        return JS_FALSE;
    }

    if(argc < 2 || !(fun = JS_ValueToFunction(cx, argv[1])))
    {
        JS_ReportError(cx, "IterateProperties requires a function second param");
        return JS_FALSE;
    }

    /* We pass along any additional args, so patch and leverage argv */
    argv1 = argv[1];
    while(NULL != (jsdprop = JSD_IterateProperties(data->jsdcTarget, jsdval, &iterp)))
    {
        jsval retval;
        JSBool another;

        argv[1] = P2H_PROPERTY(cx, jsdprop);

        if(!JS_CallFunction(cx, NULL, fun, argc, argv, &retval))
            break;
        count++ ;
        if(!JS_ValueToBoolean(cx, retval, &another) || !another)
            break;
    }
    argv[1] = argv1;

    *rval = INT_TO_JSVAL(count);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValueProperty(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSString* name;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueProperty requires value handle first param");
        return JS_FALSE;
    }

    if(argc < 2 || !(name = JS_ValueToString(cx, argv[1])))
    {
        JS_ReportError(cx, "GetValueProperty requires name string second param");
        return JS_FALSE;
    }

    *rval = P2H_PROPERTY(cx, JSD_GetValueProperty(data->jsdcTarget, jsdval, name));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValuePrototype(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValuePrototype requires value handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetValuePrototype(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValueParent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueParent requires value handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetValueParent(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValueConstructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueConstructor requires value handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetValueConstructor(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValueClassName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    const char* name;
    JSString* nameStr;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueClassName requires value handle");
        return JS_FALSE;
    }

    if(!(name = JSD_GetValueClassName(data->jsdcTarget, jsdval)) ||
       !(nameStr = JS_NewStringCopyZ(cx, name)))
        *rval = JSVAL_NULL;
    else
        *rval = STRING_TO_JSVAL(nameStr);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetPropertyName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDProperty* jsdprop;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdprop = H2P_PROPERTY(cx, argv[0])))
    {
        JS_ReportError(cx, "GetPropertyName requires property handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetPropertyName(data->jsdcTarget, jsdprop));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetPropertyValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDProperty* jsdprop;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdprop = H2P_PROPERTY(cx, argv[0])))
    {
        JS_ReportError(cx, "GetPropertyValue requires property handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetPropertyValue(data->jsdcTarget, jsdprop));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetPropertyAlias(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDProperty* jsdprop;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdprop = H2P_PROPERTY(cx, argv[0])))
    {
        JS_ReportError(cx, "GetPropertyAlias requires property handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetPropertyAlias(data->jsdcTarget, jsdprop));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetPropertyFlags(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDProperty* jsdprop;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdprop = H2P_PROPERTY(cx, argv[0])))
    {
        JS_ReportError(cx, "GetPropertyFlags requires property handle");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetPropertyFlags(data->jsdcTarget, jsdprop));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetPropertyVarArgSlot(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDProperty* jsdprop;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdprop = H2P_PROPERTY(cx, argv[0])))
    {
        JS_ReportError(cx, "GetPropertyVarArgSlot requires property handle");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetPropertyVarArgSlot(data->jsdcTarget, jsdprop));
    return JS_TRUE;
}
/***************************************************************************/
/* Object functions */

JS_STATIC_DLL_CALLBACK(JSBool)
LockObjectSubsystem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);
    JSD_LockObjectSubsystem(data->jsdcTarget);
    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
UnlockObjectSubsystem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);
    JSD_UnlockObjectSubsystem(data->jsdcTarget);
    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
IterateObjects(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDObject* iterp = NULL;
    JSDObject* jsdobj;
    JSFunction *fun;
    jsval argv0;
    int count = 0;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(fun = JS_ValueToFunction(cx, argv[0])))
    {
        JS_ReportError(cx, "IterateObjects requires a function param");
        return JS_FALSE;
    }

    /* We pass along any additional args, so patch and leverage argv */
    argv0 = argv[0];
    while(NULL != (jsdobj = JSD_IterateObjects(data->jsdcTarget, &iterp)))
    {
        jsval retval;
        JSBool another;
        argv[0] = P2H_OBJECT(cx, jsdobj);

        if(!JS_CallFunction(cx, NULL, fun, argc, argv, &retval))
            break;
        count++ ;
        if(!JS_ValueToBoolean(cx, retval, &another) || !another)
            break;
    }
    argv[0] = argv0;

    *rval = INT_TO_JSVAL(count);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetObjectNewURL(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDObject* jsdobj;
    const char* str;
    JSString* strStr;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdobj = H2P_OBJECT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetObjectNewURL requires object handle");
        return JS_FALSE;
    }

    if(!(str = JSD_GetObjectNewURL(data->jsdcTarget, jsdobj)) ||
       !(strStr = JS_NewStringCopyZ(cx, str)))
        *rval = JSVAL_NULL;
    else
        *rval = STRING_TO_JSVAL(strStr);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetObjectNewLineNumber(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDObject* jsdobj;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdobj = H2P_OBJECT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetObjectNewLineNumber requires object handle");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetObjectNewLineNumber(data->jsdcTarget, jsdobj));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetObjectConstructorURL(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDObject* jsdobj;
    const char* str;
    JSString* strStr;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdobj = H2P_OBJECT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetObjectConstructorURL requires object handle");
        return JS_FALSE;
    }

    if(!(str = JSD_GetObjectConstructorURL(data->jsdcTarget, jsdobj)) ||
       !(strStr = JS_NewStringCopyZ(cx, str)))
        *rval = JSVAL_NULL;
    else
        *rval = STRING_TO_JSVAL(strStr);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetObjectConstructorLineNumber(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDObject* jsdobj;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdobj = H2P_OBJECT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetObjectConstructorLineNumber requires object handle");
        return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(JSD_GetObjectConstructorLineNumber(data->jsdcTarget, jsdobj));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetObjectConstructorName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDObject* jsdobj;
    const char* str;
    JSString* strStr;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdobj = H2P_OBJECT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetObjectConstructorName requires object handle");
        return JS_FALSE;
    }

    if(!(str = JSD_GetObjectConstructorName(data->jsdcTarget, jsdobj)) ||
       !(strStr = JS_NewStringCopyZ(cx, str)))
        *rval = JSVAL_NULL;
    else
        *rval = STRING_TO_JSVAL(strStr);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetObjectForValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDValue* jsdval;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdval = H2P_VALUE(cx, argv[0])))
    {
        JS_ReportError(cx, "GetObjectForValue requires value handle");
        return JS_FALSE;
    }

    *rval = P2H_OBJECT(cx, JSD_GetObjectForValue(data->jsdcTarget, jsdval));
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
GetValueForObject(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSDObject* jsdobj;
    JSDB_Data* data = (JSDB_Data*) JS_GetContextPrivate(cx);
    JS_ASSERT(data);

    if(argc < 1 || !(jsdobj = H2P_OBJECT(cx, argv[0])))
    {
        JS_ReportError(cx, "GetValueForObject requires object handle");
        return JS_FALSE;
    }

    *rval = P2H_VALUE(cx, JSD_GetValueForObject(data->jsdcTarget, jsdobj));
    return JS_TRUE;
}

/***************************************************************************/

/* expands to {"x",x,n JSPROP_ENUMERATE},
 * e.g....
 * FUN_SPEC(GetMajorVersion,0) ->
 *      {"GetMajorVersion",GetMajorVersion,0,JSPROP_ENUMERATE},
 */
#define FUN_SPEC(x,n) {#x,x,n,JSPROP_ENUMERATE},

static JSFunctionSpec jsd_functions[] = {
/* High Level calls */
    FUN_SPEC(GetMajorVersion, 0)
    FUN_SPEC(GetMinorVersion, 0)
/* Script functions */
    FUN_SPEC(LockScriptSubsystem, 0)
    FUN_SPEC(UnlockScriptSubsystem, 0)
    FUN_SPEC(IterateScripts, 1)
    FUN_SPEC(IsActiveScript, 1)
    FUN_SPEC(GetScriptFilename, 1)
    FUN_SPEC(GetScriptFunctionName, 1)
    FUN_SPEC(GetScriptBaseLineNumber, 1)
    FUN_SPEC(GetScriptLineExtent, 1)
    FUN_SPEC(SetScriptHook, 1)
    FUN_SPEC(GetScriptHook, 0)
    FUN_SPEC(GetClosestPC, 2)
    FUN_SPEC(GetClosestLine, 2)
/* Execution/Interrupt Hook functions */
    FUN_SPEC(SetExecutionHook, 1)
    FUN_SPEC(GetExecutionHook, 0)
    FUN_SPEC(SendInterrupt, 0)
    FUN_SPEC(ClearInterrupt, 0)
    FUN_SPEC(GetCountOfStackFrames, 0)
    FUN_SPEC(GetStackFrame, 0)
    FUN_SPEC(GetCallingStackFrame, 1)
    FUN_SPEC(GetScriptForStackFrame, 1)
    FUN_SPEC(GetPCForStackFrame, 1)
    FUN_SPEC(GetCallObjectForStackFrame, 1)
    FUN_SPEC(GetScopeChainForStackFrame, 1)
    FUN_SPEC(GetThisForStackFrame, 1)
    FUN_SPEC(EvaluateScriptInStackFrame, 4)
    FUN_SPEC(EvaluateScriptInStackFrameToValue, 4)
    FUN_SPEC(SetTrap, 2)
    FUN_SPEC(ClearTrap, 2)
    FUN_SPEC(ClearAllTrapsForScript, 1)
    FUN_SPEC(ClearAllTraps, 0)
    FUN_SPEC(SetErrorReporterHook, 1)
    FUN_SPEC(GetException, 0)
    FUN_SPEC(SetException, 1)
    FUN_SPEC(ClearException, 0)
/* Source Text functions */
    FUN_SPEC(LockSourceTextSubsystem, 0)
    FUN_SPEC(UnlockSourceTextSubsystem, 0)
    FUN_SPEC(IterateSources, 1)
    FUN_SPEC(FindSourceForURL, 1)
    FUN_SPEC(GetSourceURL, 1)
    FUN_SPEC(GetSourceText, 1)
    FUN_SPEC(GetSourceStatus, 1)
    FUN_SPEC(GetSourceAlterCount, 1)
/* Value and Property functions */
    FUN_SPEC(RefreshValue, 1)
    FUN_SPEC(IsValueObject, 1)
    FUN_SPEC(IsValueNumber, 1)
    FUN_SPEC(IsValueInt, 1)
    FUN_SPEC(IsValueDouble, 1)
    FUN_SPEC(IsValueString, 1)
    FUN_SPEC(IsValueBoolean, 1)
    FUN_SPEC(IsValueNull, 1)
    FUN_SPEC(IsValueVoid, 1)
    FUN_SPEC(IsValuePrimitive, 1)
    FUN_SPEC(IsValueFunction, 1)
    FUN_SPEC(IsValueNative, 1)
    FUN_SPEC(GetValueBoolean, 1)
    FUN_SPEC(GetValueInt, 1)
    FUN_SPEC(GetValueDouble, 1)
    FUN_SPEC(GetValueString, 1)
    FUN_SPEC(GetValueFunctionName, 1)
    FUN_SPEC(GetCountOfProperties, 1)
    FUN_SPEC(IterateProperties, 2)
    FUN_SPEC(GetValueProperty, 2)
    FUN_SPEC(GetValuePrototype, 1)
    FUN_SPEC(GetValueParent, 1)
    FUN_SPEC(GetValueConstructor, 1)
    FUN_SPEC(GetValueClassName, 1)
    FUN_SPEC(GetPropertyName, 1)
    FUN_SPEC(GetPropertyValue, 1)
    FUN_SPEC(GetPropertyAlias, 1)
    FUN_SPEC(GetPropertyFlags, 1)
    FUN_SPEC(GetPropertyVarArgSlot, 1)
/* Object functions */
    FUN_SPEC(LockObjectSubsystem, 0)
    FUN_SPEC(UnlockObjectSubsystem, 0)
    FUN_SPEC(IterateObjects, 1)
    FUN_SPEC(GetObjectNewURL, 1)
    FUN_SPEC(GetObjectNewLineNumber, 1)
    FUN_SPEC(GetObjectConstructorURL, 1)
    FUN_SPEC(GetObjectConstructorLineNumber, 1)
    FUN_SPEC(GetObjectConstructorName, 1)
    FUN_SPEC(GetObjectForValue, 1)
    FUN_SPEC(GetValueForObject, 1)
    {0}
};

/***************************************************************************/
/***************************************************************************/
/* Constant Properties */
typedef struct ConstProp
{
    char* name;
    int   val;
} ConstProp;

/* expands to {"x",x}, e.g. {"JSD_HOOK_INTERRUPTED",JSD_HOOK_INTERRUPTED}, */
#define CONST_PROP(x) {#x,x},

/* these are used for constants defined in jsdebug.h - they must match! */
static ConstProp const_props[] = {

    CONST_PROP(JSD_HOOK_INTERRUPTED)
    CONST_PROP(JSD_HOOK_BREAKPOINT)
    CONST_PROP(JSD_HOOK_DEBUG_REQUESTED)
    CONST_PROP(JSD_HOOK_DEBUGGER_KEYWORD)
    CONST_PROP(JSD_HOOK_THROW)

    CONST_PROP(JSD_HOOK_RETURN_HOOK_ERROR)
    CONST_PROP(JSD_HOOK_RETURN_CONTINUE)
    CONST_PROP(JSD_HOOK_RETURN_ABORT)
    CONST_PROP(JSD_HOOK_RETURN_RET_WITH_VAL)
    CONST_PROP(JSD_HOOK_RETURN_THROW_WITH_VAL)
    CONST_PROP(JSD_HOOK_RETURN_CONTINUE_THROW)

    CONST_PROP(JSD_SOURCE_INITED)
    CONST_PROP(JSD_SOURCE_PARTIAL)
    CONST_PROP(JSD_SOURCE_COMPLETED)
    CONST_PROP(JSD_SOURCE_ABORTED)
    CONST_PROP(JSD_SOURCE_FAILED)
    CONST_PROP(JSD_SOURCE_CLEARED)

    CONST_PROP(JSD_ERROR_REPORTER_PASS_ALONG)
    CONST_PROP(JSD_ERROR_REPORTER_RETURN)
    CONST_PROP(JSD_ERROR_REPORTER_DEBUG)
    CONST_PROP(JSD_ERROR_REPORTER_CLEAR_RETURN)

    CONST_PROP(JSDPD_ENUMERATE)
    CONST_PROP(JSDPD_READONLY)
    CONST_PROP(JSDPD_PERMANENT)
    CONST_PROP(JSDPD_ALIAS)
    CONST_PROP(JSDPD_ARGUMENT)
    CONST_PROP(JSDPD_VARIABLE)
    CONST_PROP(JSDPD_HINTED)

    {0}
};

JSBool
_defineConstProperties(JSDB_Data* data)
{
    int i;
    for(i = 0; const_props[i].name;i++)
    {
        if(!JS_DefineProperty(data->cxDebugger, data->jsdOb,
                              const_props[i].name,
                              INT_TO_JSVAL(const_props[i].val),
                              NULL, NULL,
                              JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT))
            return JS_FALSE;
    }
    return JS_TRUE;
}
/**************************************/

static JSClass jsd_class = {
    "JSD", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

/***************************************************************************/

JSBool
jsdb_ReflectJSD(JSDB_Data* data)
{
    if(!(data->jsdOb = JS_DefineObject(data->cxDebugger, data->globDebugger,
                   "jsd", &jsd_class, NULL,
                    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)))
        return JS_FALSE;

    if(!JS_DefineFunctions(data->cxDebugger, data->jsdOb, jsd_functions))
        return JS_FALSE;

    if (!JS_DefineProperties(data->cxDebugger, data->jsdOb, jsd_properties))
        return JS_FALSE;

    if(!_defineConstProperties(data))
        return JS_FALSE;

    if(!_defineNonConstProperties(data))
        return JS_FALSE;

    if(!_initHandleSystem(data))
        return JS_FALSE;

    return JS_TRUE;
}


