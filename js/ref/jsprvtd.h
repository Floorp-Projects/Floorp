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

#ifndef jsprvtd_h___
#define jsprvtd_h___
/*
 * JS private typename definitions.
 *
 * This header is included only in other .h files, for convenience and for
 * simplicity of type naming.  The alternative for structures is to use tags,
 * which are named the same as their typedef names (legal in C/C++, and less
 * noisy than suffixing the typedef name with "Struct" or "Str").  Instead,
 * all .h files that include this file may use the same typedef name, whether
 * declaring a pointer to struct type, or defining a member of struct type.
 *
 * A few fundamental scalar types are defined here too.  Neither the scalar
 * nor the struct typedefs should change much, therefore the nearly-global
 * make dependency induced by this file should not prove painful.
 */

#include "jspubtd.h"

/* Scalar typedefs. */
typedef uint8  jsbytecode;
typedef uint8  jssrcnote;
typedef uint32 jsatomid;

/* Struct typedefs. */
typedef struct JSCodeGenerator  JSCodeGenerator;
typedef struct JSGCThing        JSGCThing;
typedef struct JSParseNode      JSParseNode;
typedef struct JSSharpObjectMap JSSharpObjectMap;
typedef struct JSToken          JSToken;
typedef struct JSTokenPos       JSTokenPos;
typedef struct JSTokenPtr       JSTokenPtr;
typedef struct JSTokenStream    JSTokenStream;
typedef struct JSTreeContext    JSTreeContext;
typedef struct JSTryNote       JSTryNote;

/* Friend "Advanced API" typedefs. */
typedef struct JSAtom           JSAtom;
typedef struct JSAtomList       JSAtomList;
typedef struct JSAtomListElement JSAtomListElement;
typedef struct JSAtomMap        JSAtomMap;
typedef struct JSAtomState      JSAtomState;
typedef struct JSCodeSpec       JSCodeSpec;
typedef struct JSPrinter        JSPrinter;
typedef struct JSRegExp         JSRegExp;
typedef struct JSRegExpStatics  JSRegExpStatics;
typedef struct JSScope          JSScope;
typedef struct JSScopeOps       JSScopeOps;
typedef struct JSScopeProperty  JSScopeProperty;
typedef struct JSStackFrame     JSStackFrame;
typedef struct JSSubString      JSSubString;
typedef struct JSSymbol         JSSymbol;

/* "Friend" types used by jscntxt.h and jsdbgapi.h. */
typedef enum JSTrapStatus {
    JSTRAP_ERROR,
    JSTRAP_CONTINUE,
    JSTRAP_RETURN,
    JSTRAP_LIMIT
} JSTrapStatus;

typedef JSTrapStatus
(*JSTrapHandler)(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
		 void *closure);

/* called just after script creation */
typedef void
(*JSNewScriptHook)( JSContext   *cx,
		    const char  *filename,  /* URL this script loads from */
		    uintN       lineno,     /* line where this script starts */
		    JSScript    *script,
		    JSFunction  *fun,
		    void        *callerdata );

/* called just before script destruction */
typedef void
(*JSDestroyScriptHook)( JSContext   *cx,
			JSScript    *script,
			void        *callerdata );

#endif /* jsprvtd_h___ */
