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

#ifndef jsfun_h___
#define jsfun_h___
/*
 * JS function definitions.
 */
#include "jsprvtd.h"
#include "jspubtd.h"

JS_BEGIN_EXTERN_C

struct JSFunction {
    jsrefcount	 nrefs;		/* number of referencing objects */
    JSObject     *object;       /* back-pointer to GC'ed object header */
    JSNative     native;        /* native method pointer or null */
    JSScript     *script;       /* interpreted bytecode descriptor or null */
    uint16       nargs;         /* minimum number of actual arguments */
    uint16       extra;         /* number of arg slots for local GC roots */
    uint16       nvars;         /* number of local variables */
    uint8        flags;         /* bound method and other flags, see jsapi.h */
    uint8        spare;         /* reserved for future use */
    JSAtom       *atom;         /* name for diagnostics and decompiling */
    JSClass      *clasp;        /* if non-null, constructor for this class */
};

extern JSClass js_ArgumentsClass;
extern JSClass js_CallClass;

/* JS_FRIEND_DATA so that JSVAL_IS_FUNCTION is callable from outside */
extern JS_FRIEND_DATA(JSClass) js_FunctionClass;

/*
 * NB: jsapi.h and jsobj.h must be included before any call to this macro.
 */
#define JSVAL_IS_FUNCTION(cx, v)                                              \
    (JSVAL_IS_OBJECT(v) && JSVAL_TO_OBJECT(v) &&                              \
     OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(v)) == &js_FunctionClass)

extern JSBool
js_IsIdentifier(JSString *str);

extern JSObject *
js_InitFunctionClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitArgumentsClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitCallClass(JSContext *cx, JSObject *obj);

extern JSFunction *
js_NewFunction(JSContext *cx, JSObject *funobj, JSNative native, uintN nargs,
	       uintN flags, JSObject *parent, JSAtom *atom);

extern JSObject *
js_CloneFunctionObject(JSContext *cx, JSObject *funobj, JSObject *parent);

extern JSBool
js_LinkFunctionObject(JSContext *cx, JSFunction *fun, JSObject *object);

extern JSFunction *
js_DefineFunction(JSContext *cx, JSObject *obj, JSAtom *atom, JSNative native,
		  uintN nargs, uintN flags);

extern JSFunction *
js_ValueToFunction(JSContext *cx, jsval *vp, JSBool constructing);

extern void
js_ReportIsNotFunction(JSContext *cx, jsval *vp, JSBool constructing);

extern JSObject *
js_GetCallObject(JSContext *cx, JSStackFrame *fp, JSObject *parent);

extern JSBool
js_PutCallObject(JSContext *cx, JSStackFrame *fp);

extern JSBool
js_GetCallVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_SetCallVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_GetArgsValue(JSContext *cx, JSStackFrame *fp, jsval *vp);

extern JSBool
js_GetArgsProperty(JSContext *cx, JSStackFrame *fp, jsid id,
                   JSObject **objp, jsval *vp);

extern JSObject *
js_GetArgsObject(JSContext *cx, JSStackFrame *fp);

extern JSBool
js_PutArgsObject(JSContext *cx, JSStackFrame *fp);

extern JSBool
js_XDRFunction(JSXDRState *xdr, JSObject **objp);

JS_END_EXTERN_C

#endif /* jsfun_h___ */
