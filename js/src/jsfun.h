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

#ifndef jsfun_h___
#define jsfun_h___
/*
 * JS function definitions.
 */
#include "jsprvtd.h"
#include "jspubtd.h"

PR_BEGIN_EXTERN_C

struct JSFunction {
    JSObject     *object;       /* back-pointer to GC'ed object header */
    JSNative     call;          /* native method pointer or null */
    uint8        nargs;         /* minimum number of actual arguments */
    uint8        flags;         /* bound method and other flags, see jsapi.h */
    uint16       nvars;         /* number of local variables */
    JSAtom       *atom;         /* name for diagnostics and decompiling */
    JSScript     *script;       /* interpreted bytecode descriptor or null */
};

extern JSClass js_CallClass;
extern JSClass js_ClosureClass;
extern JSClass js_FunctionClass;

/*
 * NB: jsapi.h and jsobj.h must be included before any call to this macro.
 */
#define JSVAL_IS_FUNCTION(v)                                                  \
    (JSVAL_IS_OBJECT(v) && JSVAL_TO_OBJECT(v) &&                              \
     JSVAL_TO_OBJECT(v)->map->clasp == &js_FunctionClass)

extern JSBool
js_IsIdentifier(JSString *str);

extern JSObject *
js_InitFunctionClass(JSContext *cx, JSObject *obj);

extern JSBool
js_InitCallAndClosureClasses(JSContext *cx, JSObject *obj,
			     JSObject *arrayProto);

extern JSFunction *
js_NewFunction(JSContext *cx, JSNative call, uintN nargs, uintN flags,
	       JSObject *parent, JSAtom *atom);

extern JSFunction *
js_DefineFunction(JSContext *cx, JSObject *obj, JSAtom *atom, JSNative call,
		  uintN nargs, uintN flags);

extern JSFunction *
js_ValueToFunction(JSContext *cx, jsval v);

extern JSObject *
js_GetCallObject(JSContext *cx, JSStackFrame *fp, JSObject *parent);

extern JSBool
js_PutCallObject(JSContext *cx, JSStackFrame *fp);

extern JSBool
js_GetCallVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_SetCallVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

PR_END_EXTERN_C

#endif /* jsfun_h___ */
