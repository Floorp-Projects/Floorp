/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
* Private Headers for JSDB
*/

#ifndef jsdbpriv_h___
#define jsdbpriv_h___

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsdbgapi.h"
#include "jsdb.h"

/***************************************************************************/

typedef struct JSDB_Data
{
    JSDContext*     jsdcTarget;
    JSRuntime*      rtTarget;
    JSRuntime*      rtDebugger;
    JSContext*      cxDebugger;
    JSObject*       globDebugger;
    JSObject*       jsdOb;
    jsval           jsScriptHook;
    jsval           jsExecutionHook;
    jsval           jsErrorReporterHook;
    JSDThreadState* jsdthreadstate;
    int             debuggerDepth;

} JSDB_Data;

extern JSBool
jsdb_ReflectJSD(JSDB_Data* data);

/***********************************/
/*
*  System to store JSD_xxx handles in jsvals. This supports tracking the
*  handle's type - both for debugging and for automatic 'dropping' of
*  reference counted handle types (e.g. JSDValue).
*/

typedef enum
{
    JSDB_GENERIC = 0,
    JSDB_CONTEXT,
    JSDB_SCRIPT,
    JSDB_SOURCETEXT,
    JSDB_THREADSTATE,
    JSDB_STACKFRAMEINFO,
    JSDB_VALUE,
    JSDB_PROPERTY,
    JSDB_OBJECT
} JSDBHandleType;

#define H2P_GENERIC(cx,val)        (void*)              jsdb_HandleValToPointer((cx),(val),JSDB_GENERIC)
#define H2P_CONTEXT(cx,val)        (JSDContext*)        jsdb_HandleValToPointer((cx),(val),JSDB_CONTEXT)
#define H2P_SCRIPT(cx,val)         (JSDScript*)         jsdb_HandleValToPointer((cx),(val),JSDB_SCRIPT)
#define H2P_SOURCETEXT(cx,val)     (JSDSourceText*)     jsdb_HandleValToPointer((cx),(val),JSDB_SOURCETEXT)
#define H2P_THREADSTATE(cx,val)    (JSDThreadState*)    jsdb_HandleValToPointer((cx),(val),JSDB_THREADSTATE)
#define H2P_STACKFRAMEINFO(cx,val) (JSDStackFrameInfo*) jsdb_HandleValToPointer((cx),(val),JSDB_STACKFRAMEINFO)
#define H2P_VALUE(cx,val)          (JSDValue*)          jsdb_HandleValToPointer((cx),(val),JSDB_VALUE)
#define H2P_PROPERTY(cx,val)       (JSDProperty*)       jsdb_HandleValToPointer((cx),(val),JSDB_PROPERTY)
#define H2P_OBJECT(cx,val)         (JSDObject*)         jsdb_HandleValToPointer((cx),(val),JSDB_OBJECT)

#define P2H_GENERIC(cx,ptr)        jsdb_PointerToNewHandleVal((cx),(ptr),JSDB_GENERIC)
#define P2H_CONTEXT(cx,ptr)        jsdb_PointerToNewHandleVal((cx),(ptr),JSDB_CONTEXT)
#define P2H_SCRIPT(cx,ptr)         jsdb_PointerToNewHandleVal((cx),(ptr),JSDB_SCRIPT)
#define P2H_SOURCETEXT(cx,ptr)     jsdb_PointerToNewHandleVal((cx),(ptr),JSDB_SOURCETEXT)
#define P2H_THREADSTATE(cx,ptr)    jsdb_PointerToNewHandleVal((cx),(ptr),JSDB_THREADSTATE)
#define P2H_STACKFRAMEINFO(cx,ptr) jsdb_PointerToNewHandleVal((cx),(ptr),JSDB_STACKFRAMEINFO)
#define P2H_VALUE(cx,ptr)          jsdb_PointerToNewHandleVal((cx),(ptr),JSDB_VALUE)
#define P2H_PROPERTY(cx,ptr)       jsdb_PointerToNewHandleVal((cx),(ptr),JSDB_PROPERTY)
#define P2H_OBJECT(cx,ptr)         jsdb_PointerToNewHandleVal((cx),(ptr),JSDB_OBJECT)

extern jsval
jsdb_PointerToNewHandleVal(JSContext *cx, void* ptr, JSDBHandleType type);

extern void*
jsdb_HandleValToPointer(JSContext *cx, jsval val, JSDBHandleType type);

/***********************************/

extern JSBool
jsdb_SetThreadState(JSDB_Data* data, JSDThreadState* jsdthreadstate);

extern uintN JS_DLL_CALLBACK
jsdb_ExecHookHandler(JSDContext*     jsdc,
                     JSDThreadState* jsdthreadstate,
                     uintN           type,
                     void*           callerdata,
                     jsval*          rval);

extern JSBool
jsdb_EvalReturnExpression(JSDB_Data* data, jsval* rval);

#endif /* jsdbpriv_h___ */
