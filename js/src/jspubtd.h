/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
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

#ifndef jspubtd_h___
#define jspubtd_h___
/*
 * JS public API typedefs.
 */
#include "jstypes.h"
#include "jscompat.h"

JS_BEGIN_EXTERN_C

/* Scalar typedefs. */
typedef uint16    jschar;
typedef int32     jsint;
typedef uint32    jsuint;
typedef float64   jsdouble;
typedef jsword    jsval;
typedef jsword    jsid;
typedef int32     jsrefcount;   /* PRInt32 if JS_THREADSAFE, see jslock.h */

typedef enum JSVersion {
    JSVERSION_1_0     = 100,
    JSVERSION_1_1     = 110,
    JSVERSION_1_2     = 120,
    JSVERSION_1_3     = 130,
    JSVERSION_1_4     = 140,
    JSVERSION_1_5     = 150,
    JSVERSION_DEFAULT = 0,
    JSVERSION_UNKNOWN = -1
} JSVersion;

#define JSVERSION_IS_ECMA(version) \
    ((version) == JSVERSION_DEFAULT || (version) >= JSVERSION_1_3)

/* Result of typeof operator enumeration. */
typedef enum JSType {
    JSTYPE_VOID,                /* undefined */
    JSTYPE_OBJECT,              /* object */
    JSTYPE_FUNCTION,            /* function */
    JSTYPE_STRING,              /* string */
    JSTYPE_NUMBER,              /* number */
    JSTYPE_BOOLEAN,             /* boolean */
    JSTYPE_LIMIT
} JSType;

/* JSObjectOps.checkAccess mode enumeration. */
typedef enum JSAccessMode {
    JSACC_PROTO,
    JSACC_PARENT,
    JSACC_IMPORT,
    JSACC_WATCH,
    JSACC_LIMIT
} JSAccessMode;

/*
 * This enum type is used to control the behavior of a JSObject property
 * iterator function that has type JSNewEnumerate.
 */
typedef enum JSIterateOp {
    JSENUMERATE_INIT,       /* Create new iterator state */
    JSENUMERATE_NEXT,       /* Iterate once */
    JSENUMERATE_DESTROY     /* Destroy iterator state */
} JSIterateOp;

/* Struct typedefs. */
typedef struct JSClass           JSClass;
typedef struct JSConstDoubleSpec JSConstDoubleSpec;
typedef struct JSContext         JSContext;
typedef struct JSErrorReport     JSErrorReport;
typedef struct JSFunction        JSFunction;
typedef struct JSFunctionSpec    JSFunctionSpec;
typedef struct JSIdArray         JSIdArray;
typedef struct JSProperty        JSProperty;
typedef struct JSPropertySpec    JSPropertySpec;
typedef struct JSObject          JSObject;
typedef struct JSObjectMap       JSObjectMap;
typedef struct JSObjectOps       JSObjectOps;
typedef struct JSRuntime         JSRuntime;
typedef struct JSRuntime         JSTaskState;	/* XXX deprecated name */
typedef struct JSScript          JSScript;
typedef struct JSString          JSString;
typedef struct JSXDRState	 JSXDRState;
typedef struct JSExceptionState  JSExceptionState;
typedef struct JSLocaleCallbacks JSLocaleCallbacks;

/* JSClass (and JSObjectOps where appropriate) function pointer typedefs. */

typedef JSBool
(* JS_DLL_CALLBACK JSPropertyOp)(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

/*
 * This function type is used for callbacks that enumerate the properties of
 * a JSObject.  The behavior depends on the value of enum_op:
 *
 *  JSENUMERATE_INIT
 *    A new, opaque iterator state should be allocated and stored in *statep.
 *    (You can use PRIVATE_TO_JSVAL() to tag the pointer to be stored).
 *
 *    The number of properties that will be enumerated should be returned as
 *    an integer jsval in *idp, if idp is non-null, and provided the number of
 *    enumerable properties is known.  If idp is non-null and the number of
 *    enumerable properties can't be computed in advance, *idp should be set
 *    to JSVAL_ZERO.
 *
 *  JSENUMERATE_NEXT
 *    A previously allocated opaque iterator state is passed in via statep.
 *    Return the next jsid in the iteration using *idp.  The opaque iterator
 *    state pointed at by statep is destroyed and *statep is set to JSVAL_NULL
 *    if there are no properties left to enumerate.
 *
 *  JSENUMERATE_DESTROY
 *    Destroy the opaque iterator state previously allocated in *statep by a
 *    call to this function when enum_op was JSENUMERATE_INIT.
 *
 * The return value is used to indicate success, with a value of JS_FALSE
 * indicating failure.
 */
typedef JSBool
(* JS_DLL_CALLBACK JSNewEnumerateOp)(JSContext *cx, JSObject *obj,
                                     JSIterateOp enum_op,
                                     jsval *statep, jsid *idp);

typedef JSBool
(* JS_DLL_CALLBACK JSEnumerateOp)(JSContext *cx, JSObject *obj);

typedef JSBool
(* JS_DLL_CALLBACK JSResolveOp)(JSContext *cx, JSObject *obj, jsval id);

typedef JSBool
(* JS_DLL_CALLBACK JSNewResolveOp)(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                                   JSObject **objp);

typedef JSBool
(* JS_DLL_CALLBACK JSConvertOp)(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

typedef void
(* JS_DLL_CALLBACK JSFinalizeOp)(JSContext *cx, JSObject *obj);

typedef void
(* JS_DLL_CALLBACK JSStringFinalizeOp)(JSContext *cx, JSString *str);

typedef JSObjectOps *
(* JS_DLL_CALLBACK JSGetObjectOps)(JSContext *cx, JSClass *clasp);

typedef JSBool
(* JS_DLL_CALLBACK JSCheckAccessOp)(JSContext *cx, JSObject *obj, jsval id,
                                    JSAccessMode mode, jsval *vp);

typedef JSBool
(* JS_DLL_CALLBACK JSXDRObjectOp)(JSXDRState *xdr, JSObject **objp);

typedef JSBool
(* JS_DLL_CALLBACK JSHasInstanceOp)(JSContext *cx, JSObject *obj, jsval v,
                                    JSBool *bp);

typedef JSBool
(* JS_DLL_CALLBACK JSSetObjectSlotOp)(JSContext *cx, JSObject *obj, uint32 slot,
                                      JSObject *pobj);

typedef uint32
(* JS_DLL_CALLBACK JSMarkOp)(JSContext *cx, JSObject *obj, void *arg);

/* JSObjectOps function pointer typedefs. */

typedef JSObjectMap *
(* JS_DLL_CALLBACK JSNewObjectMapOp)(JSContext *cx, jsrefcount nrefs,
                                     JSObjectOps *ops, JSClass *clasp,
                                     JSObject *obj);

typedef void
(* JS_DLL_CALLBACK JSObjectMapOp)(JSContext *cx, JSObjectMap *map);

typedef JSBool
(* JS_DLL_CALLBACK JSLookupPropOp)(JSContext *cx, JSObject *obj, jsid id,
                                   JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                                 , const char *file, uintN line
#endif
                                  );

typedef JSBool
(* JS_DLL_CALLBACK JSDefinePropOp)(JSContext *cx, JSObject *obj, jsid id, jsval value,
                                   JSPropertyOp getter, JSPropertyOp setter,
                                   uintN attrs, JSProperty **propp);

typedef JSBool
(* JS_DLL_CALLBACK JSPropertyIdOp)(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

typedef JSBool
(* JS_DLL_CALLBACK JSAttributesOp)(JSContext *cx, JSObject *obj, jsid id,
                                   JSProperty *prop, uintN *attrsp);

typedef JSBool
(* JS_DLL_CALLBACK JSCheckAccessIdOp)(JSContext *cx, JSObject *obj, jsid id,
                                      JSAccessMode mode, jsval *vp, uintN *attrsp);

typedef JSObject *
(* JS_DLL_CALLBACK JSObjectOp)(JSContext *cx, JSObject *obj);

typedef void
(* JS_DLL_CALLBACK JSPropertyRefOp)(JSContext *cx, JSObject *obj, JSProperty *prop);

/* Typedef for native functions called by the JS VM. */

typedef JSBool
(* JS_DLL_CALLBACK JSNative)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                             jsval *rval);

/* Callbacks and their arguments. */

typedef enum JSGCStatus {
    JSGC_BEGIN,
    JSGC_END,
    JSGC_MARK_END
} JSGCStatus;

typedef JSBool
(* JS_DLL_CALLBACK JSGCCallback)(JSContext *cx, JSGCStatus status);

typedef JSBool
(* JS_DLL_CALLBACK JSBranchCallback)(JSContext *cx, JSScript *script);

typedef void
(* JS_DLL_CALLBACK JSErrorReporter)(JSContext *cx, const char *message,
                                    JSErrorReport *report);

typedef struct JSErrorFormatString {
    const char *format;
    uintN argCount;
} JSErrorFormatString;

typedef const JSErrorFormatString *
(* JS_DLL_CALLBACK JSErrorCallback)(void *userRef, const char *locale,
			            const uintN errorNumber);

#ifdef va_start
#define JS_ARGUMENT_FORMATTER_DEFINED 1

typedef JSBool
(* JS_DLL_CALLBACK JSArgumentFormatter)(JSContext *cx, const char *format,
                                        JSBool fromJS, jsval **vpp, va_list *app);
#endif

typedef JSBool 
(* JS_DLL_CALLBACK JSLocaleToUpperCase)(JSContext *cx, JSString *src, jsval *rval);

typedef JSBool
(* JS_DLL_CALLBACK JSLocaleToLowerCase)(JSContext *cx, JSString *src, jsval *rval);

typedef JSBool
(* JS_DLL_CALLBACK JSLocaleCompare)(JSContext *cx, JSString *src1, JSString *src2, jsval *rval);



JS_END_EXTERN_C

#endif /* jspubtd_h___ */
