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

#ifndef jsdbgapi_h___
#define jsdbgapi_h___
/*
 * JS debugger API.
 */
#include "jsapi.h"
#include "jsopcode.h"
#include "jsprvtd.h"

PR_BEGIN_EXTERN_C

typedef enum JSTrapStatus {
    JSTRAP_ERROR,
    JSTRAP_CONTINUE,
    JSTRAP_RETURN,
    JSTRAP_LIMIT
} JSTrapStatus;

typedef JSTrapStatus
(*JSTrapHandler)(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
		 void *closure);

extern void
js_PatchOpcode(JSContext *cx, JSScript *script, jsbytecode *pc, JSOp op);

PR_EXTERN(JSBool)
JS_SetTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
	   JSTrapHandler handler, void *closure);

PR_EXTERN(JSOp)
JS_GetTrapOpcode(JSContext *cx, JSScript *script, jsbytecode *pc);

PR_EXTERN(void)
JS_ClearTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
	     JSTrapHandler *handlerp, void **closurep);

PR_EXTERN(void)
JS_ClearScriptTraps(JSContext *cx, JSScript *script);

PR_EXTERN(void)
JS_ClearAllTraps(JSContext *cx);

PR_EXTERN(JSTrapStatus)
JS_HandleTrap(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval);

PR_EXTERN(JSBool)
JS_SetInterrupt(JSRuntime *rt, JSTrapHandler handler, void *closure);

PR_EXTERN(JSBool)
JS_ClearInterrupt(JSRuntime *rt, JSTrapHandler *handlerp, void **closurep);

/************************************************************************/

typedef JSBool
(*JSWatchPointHandler)(JSContext *cx, JSObject *obj, jsval id,
		       jsval old, jsval *newp, void *closure);

PR_EXTERN(JSBool)
JS_SetWatchPoint(JSContext *cx, JSObject *obj, jsval id,
		 JSWatchPointHandler handler, void *closure);

PR_EXTERN(void)
JS_ClearWatchPoint(JSContext *cx, JSObject *obj, jsval id,
		   JSWatchPointHandler *handlerp, void **closurep);

PR_EXTERN(void)
JS_ClearWatchPointsForObject(JSContext *cx, JSObject *obj);

PR_EXTERN(void)
JS_ClearAllWatchPoints(JSContext *cx);

/************************************************************************/

PR_EXTERN(uintN)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc);

PR_EXTERN(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, uintN lineno);

PR_EXTERN(JSScript *)
JS_GetFunctionScript(JSContext *cx, JSFunction *fun);

PR_EXTERN(JSPrincipals *)
JS_GetScriptPrincipals(JSContext *cx, JSScript *script);

/*
 * Stack Frame Iterator
 *
 * Used to iterate through the JS stack frames to extract
 * information from the frames.
 */

PR_EXTERN(JSStackFrame *)
JS_FrameIterator(JSContext *cx, JSStackFrame **iteratorp);

PR_EXTERN(JSScript *)
JS_GetFrameScript(JSContext *cx, JSStackFrame *fp);

PR_EXTERN(jsbytecode *)
JS_GetFramePC(JSContext *cx, JSStackFrame *fp);

PR_EXTERN(JSBool)
JS_IsNativeFrame(JSContext *cx, JSStackFrame *fp);

PR_EXTERN(void *)
JS_GetFrameAnnotation(JSContext *cx, JSStackFrame *fp);

PR_EXTERN(void)
JS_SetFrameAnnotation(JSContext *cx, JSStackFrame *fp, void *annotation);

PR_EXTERN(void *)
JS_GetFramePrincipalArray(JSContext *cx, JSStackFrame *fp);

PR_EXTERN(JSObject *)
JS_GetFrameObject(JSContext *cx, JSStackFrame *fp);

PR_EXTERN(JSObject *)
JS_GetFrameThis(JSContext *cx, JSStackFrame *fp);

PR_EXTERN(JSFunction *)
JS_GetFrameFunction(JSContext *cx, JSStackFrame *fp);

/************************************************************************/

PR_EXTERN(const char *)
JS_GetScriptFilename(JSContext *cx, JSScript *script);

PR_EXTERN(uintN)
JS_GetScriptBaseLineNumber(JSContext *cx, JSScript *script);

PR_EXTERN(uintN)
JS_GetScriptLineExtent(JSContext *cx, JSScript *script);

/************************************************************************/

/* called just after script creation */
typedef void
(*JSNewScriptHookProc)(
		JSContext   *cx,
		const char  *filename,      /* URL this script loads from */
		uintN       lineno,         /* line where this script starts */
		JSScript    *script,
		JSFunction  *fun,
		void        *callerdata );

/* called just before script destruction */
typedef void
(*JSDestroyScriptHookProc)(
		JSContext   *cx,
		JSScript    *script,
		void        *callerdata );

PR_EXTERN(void)
JS_SetNewScriptHookProc(JSRuntime *rt, JSNewScriptHookProc hookproc,
			void *callerdata);

PR_EXTERN(void)
JS_SetDestroyScriptHookProc(JSRuntime *rt, JSDestroyScriptHookProc hookproc,
			    void *callerdata);

/************************************************************************/

PR_EXTERN(JSBool)
JS_EvaluateInStackFrame(JSContext *cx, JSStackFrame *fp,
			const char *bytes, uintN length,
			const char *filename, uintN lineno,
			jsval *rval);

/************************************************************************/

typedef struct JSPropertyDesc {
    jsval           id;         /* primary id, a string or int */
    jsval           value;      /* property value */
    uint8           flags;      /* flags, see below */
    uint8           spare;      /* unused */
    uint16          slot;       /* argument/variable slot */
    jsval           alias;      /* alias id if JSPD_ALIAS flag */
} JSPropertyDesc;

#define JSPD_ENUMERATE  0x01    /* visible to for/in loop */
#define JSPD_READONLY   0x02    /* assignment is error */
#define JSPD_PERMANENT  0x04    /* property cannot be deleted */
#define JSPD_ALIAS      0x08    /* property has an alias id */
#define JSPD_ARGUMENT   0x10    /* argument to function */
#define JSPD_VARIABLE   0x20    /* local variable in function */

typedef struct JSPropertyDescArray {
    uint32          length;     /* number of elements in array */
    JSPropertyDesc  *array;     /* alloc'd by Get, freed by Put */
} JSPropertyDescArray;

PR_EXTERN(JSProperty *)
JS_PropertyIterator(JSObject *obj, JSProperty **iteratorp);

PR_EXTERN(JSBool)
JS_GetPropertyDesc(JSContext *cx, JSProperty *prop, JSPropertyDesc *pd);

PR_EXTERN(JSBool)
JS_GetPropertyDescArray(JSContext *cx, JSObject *obj, JSPropertyDescArray *pda);

PR_EXTERN(void)
JS_PutPropertyDescArray(JSContext *cx, JSPropertyDescArray *pda);

PR_END_EXTERN_C

#endif /* jsdbgapi_h___ */
