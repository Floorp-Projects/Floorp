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

#ifndef jspubtd_h___
#define jspubtd_h___
/*
 * JS public API typedefs.
 */
#include "prtypes.h"

#define NETSCAPE_INTERNAL 1
#include "jscompat.h"

#define JS_FRIEND_API(t) PR_IMPLEMENT(t)

/* Scalar typedefs. */
typedef uint16    jschar;
typedef int32     jsint;
typedef uint32    jsuint;
typedef float64   jsdouble;
typedef prword    jsval;

/* Boolean enum and packed int types. */
typedef PRBool       JSBool;
typedef PRPackedBool JSPackedBool;

#define JS_FALSE     PR_FALSE
#define JS_TRUE      PR_TRUE

typedef enum JSVersion {
    JSVERSION_1_0     = 100,
    JSVERSION_1_1     = 110,
    JSVERSION_1_2     = 120,
    JSVERSION_1_3     = 130,
    JSVERSION_DEFAULT = 0,
    JSVERSION_UNKNOWN = -1
} JSVersion;

#define JSVERSION_IS_ECMA(version) \
    ((version) == JSVERSION_DEFAULT || (version) >= JSVERSION_1_3)

/* Typeof operator result enumeration. */
typedef enum JSType {
    JSTYPE_VOID,                /* undefined */
    JSTYPE_OBJECT,              /* object */
    JSTYPE_FUNCTION,            /* function */
    JSTYPE_STRING,              /* string */
    JSTYPE_NUMBER,              /* number */
    JSTYPE_BOOLEAN,             /* boolean */
    JSTYPE_LIMIT
} JSType;

/* Struct typedefs. */
typedef struct JSClass           JSClass;
typedef struct JSConstDoubleSpec JSConstDoubleSpec;
typedef struct JSContext         JSContext;
typedef struct JSErrorReport     JSErrorReport;
typedef struct JSFunction        JSFunction;
typedef struct JSFunctionSpec    JSFunctionSpec;
typedef struct JSPropertySpec    JSPropertySpec;
typedef struct JSObject          JSObject;
typedef struct JSObjectMap       JSObjectMap;
typedef struct JSRuntime         JSRuntime;
typedef struct JSRuntime         JSTaskState;	/* XXX deprecated name */
typedef struct JSScript          JSScript;
typedef struct JSString          JSString;

#ifndef CRT_CALL
#ifdef XP_OS2
#define CRT_CALL _Optlink
#else
#define CRT_CALL 
#endif
#endif

/* Typedefs for user-supplied functions called by the JS VM. */
typedef JSBool
(* CRT_CALL JSPropertyOp)(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

typedef JSBool
(* CRT_CALL JSEnumerateOp)(JSContext *cx, JSObject *obj);

typedef JSBool
(* CRT_CALL JSResolveOp)(JSContext *cx, JSObject *obj, jsval id);

typedef JSBool
(* CRT_CALL JSNewResolveOp)(JSContext *cx, JSObject *obj, jsval id,
			    JSObject **objp);

typedef JSBool
(* CRT_CALL JSConvertOp)(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

typedef void
(* CRT_CALL JSFinalizeOp)(JSContext *cx, JSObject *obj);

typedef JSBool
(* CRT_CALL JSNative)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		      jsval *rval);

typedef JSBool
(* CRT_CALL JSBranchCallback)(JSContext *cx, JSScript *script);

typedef void
(* CRT_CALL JSErrorReporter)(JSContext *cx, const char *message,
			     JSErrorReport *report);

#ifdef NETSCAPE_INTERNAL
/* XXX remove and provide transcoder API when JS uses Unicode */
typedef int (* CRT_CALL JSCharScanner)(int, char);
#endif

#endif /* jspubtd_h___ */
