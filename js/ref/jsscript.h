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

#ifndef jsscript_h___
#define jsscript_h___
/*
 * JS script descriptor.
 */
#include "jsatom.h"
#include "jsprvtd.h"

PR_BEGIN_EXTERN_C

/*
 * Exception handling runtime information.
 *
 * All fields except length are code offsets, relative to the beginning of
 * the script.  If script->trynotes is not null, it points to a vector of
 * these structs terminated by one with start, catchStart, and finallyStart
 * all equal to 0, and length == script->length.
 */
struct JSTryNote {
    ptrdiff_t    start;         /* start of try statement */
    ptrdiff_t    length;        /* count of try statement bytecodes */
    ptrdiff_t    catchStart;    /* start of catch block (0 if none) */
    ptrdiff_t    finallyStart;  /* XXX unneeded except for end-of-vector mark:
				   if (!catchStart && !finallyStart) break */
};

struct JSScript {
    jsbytecode   *code;         /* bytecodes and their immediate operands */
    uint32       length;        /* length of code vector */
    JSAtomMap    atomMap;       /* maps immediate index to literal struct */
    const char   *filename;     /* source filename or null */
    uintN        lineno;        /* base line number of script */
    uintN        depth;         /* maximum stack depth in slots */
    jssrcnote    *notes;        /* line number and other decompiling data */
    JSTryNote    *trynotes;     /* exception table for this script */
    JSPrincipals *principals;   /* principals for this script */
    void         *javaData;     /* XXX extra data used by jsjava.c */
    JSObject     *object;       /* optional Script-class object wrapper */
};

extern JSClass js_ScriptClass;

extern JSObject *
js_InitScriptClass(JSContext *cx, JSObject *obj);

extern JSScript *
js_NewScript(JSContext *cx, uint32 length);

extern JSScript *
js_NewScriptFromParams(JSContext *cx, jsbytecode *code, uint32 length,
		       const char *filename, uintN lineno, uintN depth,
		       jssrcnote *notes, JSTryNote *trynotes,
		       JSPrincipals *principals);

extern JS_FRIEND_API(JSScript *)
js_NewScriptFromCG(JSContext *cx, JSCodeGenerator *cg, JSFunction *fun);

extern void
js_DestroyScript(JSContext *cx, JSScript *script);

extern jssrcnote *
js_GetSrcNote(JSScript *script, jsbytecode *pc);

extern uintN
js_PCToLineNumber(JSScript *script, jsbytecode *pc);

extern jsbytecode *
js_LineNumberToPC(JSScript *script, uintN lineno);

extern uintN
js_GetScriptLineExtent(JSScript *script);

extern JSBool
js_XDRScript(JSXDRState *xdr, JSScript **scriptp, JSBool *magic);

PR_END_EXTERN_C

#endif /* jsscript_h___ */
