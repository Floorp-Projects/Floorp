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

struct JSScript {
    jsbytecode   *code;         /* bytecodes and their immediate operands */
    uint32       length;        /* length of code vector */
    JSAtomMap    atomMap;       /* maps immediate index to literal struct */
    const char   *filename;     /* source filename or null */
    uintN        lineno;        /* base line number of script */
    uintN        depth;         /* maximum stack depth in slots */
    jssrcnote    *notes;        /* line number and other decompiling data */
    JSSymbol     *args;         /* formal argument list */
    JSSymbol     *vars;         /* local variable list */
    JSPrincipals *principals;	/* principals for this script */
    void         *javaData;     /* extra data used by jsjava.c */
};

extern JSScript *
js_NewScript(JSContext *cx, JSCodeGenerator *cg, const char *filename,
	     uintN lineno, JSPrincipals *principals, JSFunction *fun);

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

PR_END_EXTERN_C

#endif /* jsscript_h___ */
