/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef jsscript_h___
#define jsscript_h___
/*
 * JS script descriptor.
 */
#include "jsatom.h"
#include "jsprvtd.h"

JS_BEGIN_EXTERN_C

/*
 * Exception handling runtime information.
 *
 * All fields except length are code offsets, relative to the beginning of
 * the script.  If script->trynotes is not null, it points to a vector of
 * these structs terminated by one with catchStart == 0.
 */
struct JSTryNote {
    ptrdiff_t    start;         /* start of try statement */
    ptrdiff_t    length;        /* count of try statement bytecodes */
    ptrdiff_t    catchStart;    /* start of catch block (0 if end) */
};

struct JSScript {
    jsbytecode   *code;         /* bytecodes and their immediate operands */
    uint32       length;        /* length of code vector */
    jsbytecode   *main;         /* main entry point, after predef'ing prolog */
    JSVersion    version;       /* JS version under which script was compiled */
    JSAtomMap    atomMap;       /* maps immediate index to literal struct */
    const char   *filename;     /* source filename or null */
    uintN        lineno;        /* base line number of script */
    uintN        depth;         /* maximum stack depth in slots */
    jssrcnote    *notes;        /* line number and other decompiling data */
    JSTryNote    *trynotes;     /* exception table for this script */
    JSPrincipals *principals;   /* principals for this script */
    JSObject     *object;       /* optional Script-class object wrapper */
};

#define JSSCRIPT_FIND_CATCH_START(script, pc, catchpc)                        \
    JS_BEGIN_MACRO                                                            \
        JSTryNote *_tn = (script)->trynotes;                                  \
        jsbytecode *_catchpc = NULL;                                          \
        if (_tn) {                                                            \
            ptrdiff_t _offset = PTRDIFF(pc, (script)->main, jsbytecode);      \
            while (JS_UPTRDIFF(_offset, _tn->start) >= (jsuword)_tn->length)  \
                _tn++;                                                        \
            if (_tn->catchStart)                                              \
                _catchpc = (script)->main + _tn->catchStart;                  \
        }                                                                     \
        catchpc = _catchpc;                                                   \
    JS_END_MACRO

extern JS_FRIEND_DATA(JSClass) js_ScriptClass;

extern JSObject *
js_InitScriptClass(JSContext *cx, JSObject *obj);

extern JSScript *
js_NewScript(JSContext *cx, uint32 length);

extern JSScript *
js_NewScriptFromParams(JSContext *cx, jsbytecode *code, uint32 length,
		       jsbytecode *prolog, uint32 prologLength,
		       const char *filename, uintN lineno, uintN depth,
		       jssrcnote *notes, JSTryNote *trynotes,
		       JSPrincipals *principals);

extern JS_FRIEND_API(JSScript *)
js_NewScriptFromCG(JSContext *cx, JSCodeGenerator *cg, JSFunction *fun);

extern void
js_DestroyScript(JSContext *cx, JSScript *script);

extern void
js_MarkScript(JSContext *cx, JSScript *script, void *arg);

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

JS_END_EXTERN_C

#endif /* jsscript_h___ */
