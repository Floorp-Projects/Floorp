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

/*
 * JS script operations.
 */
#include <string.h>
#include "prtypes.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsscript.h"

JSScript *
js_NewScript(JSContext *cx, JSCodeGenerator *cg, const char *filename,
             uintN lineno, JSPrincipals *principals, JSFunction *fun)
{
    JSScript *script;
    ptrdiff_t length;
    JSNewScriptHookProc hookproc;

    length = CG_OFFSET(cg);
    script = JS_malloc(cx, sizeof(JSScript) + length);
    if (!script)
        return NULL;
    memset(script, 0, sizeof(JSScript));
    if (!js_InitAtomMap(cx, &script->atomMap, &cg->atomList)) {
        JS_free(cx, script);
        return NULL;
    }
    if (filename) {
        script->filename = JS_strdup(cx, filename);
        if (!script->filename) {
            js_DestroyScript(cx, script);
            return NULL;
        }
    }
    script->notes = js_FinishTakingSrcNotes(cx, cg);
    if (!script->notes) {
        js_DestroyScript(cx, script);
        return NULL;
    }
    script->code = (jsbytecode *)(script + 1);
    memcpy(script->code, cg->base, length * sizeof(jsbytecode));
    script->length = length;
    script->depth = cg->maxStackDepth;
    script->lineno = lineno;
    script->args = cg->args;
    script->vars = cg->vars;
    script->javaData = 0;
    if (principals)
        JSPRINCIPALS_HOLD(cx, principals);
    script->principals = principals;

    hookproc = (JSNewScriptHookProc)cx->runtime->newScriptHookProc;
    if (hookproc) {
        (*hookproc)(cx, filename, lineno, script, fun,
		    cx->runtime->newScriptHookProcData);
    }

    return script;
}

void
js_DestroyScript(JSContext *cx, JSScript *script)
{
    JSDestroyScriptHookProc hookproc;

    hookproc = (JSDestroyScriptHookProc)cx->runtime->destroyScriptHookProc;
    if (hookproc)
        (*hookproc)(cx, script, cx->runtime->destroyScriptHookProcData);

    JS_ClearScriptTraps(cx, script);
    js_FreeAtomMap(cx, &script->atomMap);
    if (js_InterpreterHooks && js_InterpreterHooks->destroyScript)
        js_InterpreterHooks->destroyScript(cx, script);
    if (script->filename)
        JS_free(cx, (void *)script->filename);
    if (script->notes)
        JS_free(cx, script->notes);
    if (script->principals)
        JSPRINCIPALS_DROP(cx, script->principals);
    JS_free(cx, script);
}

jssrcnote *
js_GetSrcNote(JSScript *script, jsbytecode *pc)
{
    jssrcnote *sn;
    ptrdiff_t offset, target;

    sn = script->notes;
    if (!sn)
        return NULL;
    target = pc - script->code;
    if ((uintN)target >= script->length)
        return NULL;
    for (offset = 0; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        offset += SN_DELTA(sn);
        if (offset == target && SN_IS_GETTABLE(sn))
            return sn;
    }
    return NULL;
}

uintN
js_PCToLineNumber(JSScript *script, jsbytecode *pc)
{
    jssrcnote *sn;
    ptrdiff_t offset, target;
    uintN lineno;
    JSSrcNoteType type;

    sn = script->notes;
    if (!sn)
        return 0;
    target = pc - script->code;
    lineno = script->lineno;
    for (offset = 0; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        offset += SN_DELTA(sn);
        type = SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            if (offset <= target)
                lineno = (uintN) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            if (offset <= target)
                lineno++;
        }
        if (offset > target)
            break;
    }
    return lineno;
}

jsbytecode *
js_LineNumberToPC(JSScript *script, uintN target)
{
    jssrcnote *sn;
    uintN lineno;
    ptrdiff_t offset;
    JSSrcNoteType type;

    sn = script->notes;
    if (!sn)
        return NULL;
    lineno = script->lineno;
    for (offset = 0; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        if (lineno >= target)
            break;
        offset += SN_DELTA(sn);
        type = SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            lineno = (uintN) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            lineno++;
        }
    }
    return script->code + offset;
}

uintN
js_GetScriptLineExtent(JSScript *script)
{
    jssrcnote *sn;
    uintN lineno;
    JSSrcNoteType type;

    sn = script->notes;
    if (!sn)
        return 0;
    lineno = script->lineno;
    for (; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        type = SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            lineno = (uintN) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            lineno++;
        }
    }
    return 1 + lineno - script->lineno;
}
