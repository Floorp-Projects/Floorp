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
#include "jsstddef.h"
#include <string.h>
#include "prtypes.h"
#include "prassert.h"
#include "prprintf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsscript.h"
#if JS_HAS_XDR
#include "jsxdrapi.h"
#endif

#if JS_HAS_SCRIPT_OBJECT

#if JS_HAS_TOSOURCE
static JSBool
script_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
    JSScript *script;
    size_t i, j, k, n;
    char buf[16];
    jschar *s, *t;
    uint32 indent;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &js_ScriptClass, argv))
	return JS_FALSE;
    script = JS_GetPrivate(cx, obj);

    /* Let n count the source string length, j the "front porch" length. */
    j = PR_snprintf(buf, sizeof buf, "(new %s(", js_ScriptClass.name);
    n = j + 2;
    if (!script) {
	/* Let k count the constructor argument string length. */
	k = 0;
    } else {
	indent = 0;
	if (argc && !js_ValueToECMAUint32(cx, argv[0], &indent))
	    return JS_FALSE;
	str = JS_DecompileScript(cx, script, "Script.prototype.toSource",
				 (uintN)indent);
	if (!str)
	    return JS_FALSE;
    	str = js_QuoteString(cx, str, '\'');
	if (!str)
	    return JS_FALSE;
        s = str->chars;
	k = str->length;
    	n += k;
    }

    /* Allocate the source string and copy into it. */
    t = JS_malloc(cx, (n + 1) * sizeof(jschar));
    if (!t)
	return JS_FALSE;
    for (i = 0; i < j; i++)
	t[i] = buf[i];
    for (j = 0; j < k; i++, j++)
	t[i] = s[j];
    t[i++] = ')';
    t[i++] = ')';
    t[i] = 0;

    /* Create and return a JS string for t. */
    str = JS_NewUCString(cx, t, n);
    if (!str) {
	JS_free(cx, t);
	return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#endif /* JS_HAS_TOSOURCE */

static JSBool
script_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
    JSScript *script;
    uint32 indent;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &js_ScriptClass, argv))
    	return JS_FALSE;
    script = JS_GetPrivate(cx, obj);
    if (!script) {
    	*rval = STRING_TO_JSVAL(cx->runtime->emptyString);
    	return JS_TRUE;
    }

    indent = 0;
    if (argc && !js_ValueToECMAUint32(cx, argv[0], &indent))
	return JS_FALSE;
    str = JS_DecompileScript(cx, script, "Script.prototype.toString",
			     (uintN)indent);
    if (!str)
    	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
script_compile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	       jsval *rval)
{
    JSScript *oldscript, *script;
    JSString *str;
    JSStackFrame *fp, *caller;
    JSObject *scopeobj;
    const char *file;
    uintN line;
    JSPrincipals *principals;

    /* If no args, leave private undefined and return early. */
    if (argc == 0)
    	goto out;

    /* Otherwise, the first arg is the script source to compile. */
    str = js_ValueToString(cx, argv[0]);
    if (!str)
    	return JS_FALSE;

    /* Compile using the caller's scope chain, which js_Invoke passes to fp. */
    fp = cx->fp;
    caller = fp->down;
    PR_ASSERT(fp->scopeChain == caller->scopeChain);

    scopeobj = NULL;
    if (argc >= 2) {
    	if (!js_ValueToObject(cx, argv[1], &scopeobj))
	    return JS_FALSE;
    	argv[1] = OBJECT_TO_JSVAL(scopeobj);
    }
    if (!scopeobj)
    	scopeobj = caller->scopeChain;

    if (caller->script) {
	file = caller->script->filename;
	line = js_PCToLineNumber(caller->script, caller->pc);
	principals = caller->script->principals;
    } else {
	file = NULL;
	line = 0;
	principals = NULL;
    }

    /* Compile the new script using the caller's scope chain, a la eval(). */
    script = JS_CompileUCScriptForPrincipals(cx, scopeobj, principals,
					     str->chars, str->length,
					     file, line);
    if (!script)
    	return JS_FALSE;

    /* Swap script for obj's old script, if any. */
    oldscript = JS_GetPrivate(cx, obj);
    if (!JS_SetPrivate(cx, obj, script)) {
	js_DestroyScript(cx, script);
    	return JS_FALSE;
    }
    if (oldscript)
	js_DestroyScript(cx, oldscript);

    script->object = obj;
out:
    /* Return the object. */
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
script_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSScript *script;
    JSStackFrame *fp, *caller;
    JSObject *scopeobj;

    if (!JS_InstanceOf(cx, obj, &js_ScriptClass, argv))
    	return JS_FALSE;
    script = JS_GetPrivate(cx, obj);
    if (!script)
    	return JS_TRUE;

    scopeobj = NULL;
    if (argc) {
    	if (!js_ValueToObject(cx, argv[0], &scopeobj))
	    return JS_FALSE;
    	argv[0] = OBJECT_TO_JSVAL(scopeobj);
    }

    /* Emulate eval() by using caller's this, scope chain, and sharp array. */
    fp = cx->fp;
    caller = fp->down;
    if (!scopeobj)
    	scopeobj = caller->scopeChain;
    fp->thisp = caller->thisp;
    PR_ASSERT(fp->scopeChain == caller->scopeChain);
    fp->sharpArray = caller->sharpArray;
    return js_Execute(cx, scopeobj, script, NULL, fp, JS_FALSE, rval);
}

#if JS_HAS_XDR
static JSBool
XDRAtom1(JSXDRState *xdr, JSAtomListElement *ale)
{
    uint32 type;
    jsval value;

    if (xdr->mode == JSXDR_ENCODE) {
	type = JSVAL_TAG(ATOM_KEY(ale->atom));
	value = ATOM_KEY(ale->atom);
    }

    if (!JS_XDRUint32(xdr, &ale->index) ||
	!JS_XDRValue(xdr, &value))
	return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE) {
	ale->atom = js_AtomizeValue(xdr->cx, value, 0);
	if (!ale->atom)
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
XDRAtomMap(JSXDRState *xdr, JSAtomMap *map)
{
    uint32 length;
    uintN i;

    if (xdr->mode == JSXDR_ENCODE)
	length = map->length;

    if (!JS_XDRUint32(xdr, &length))
	return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE) {
	JSContext *cx;
	void *mark;
	JSAtomList al;
	JSAtomListElement *ale;

	cx = xdr->cx;
	mark = PR_ARENA_MARK(&cx->tempPool);
	ATOM_LIST_INIT(&al);
	for (i = 0; i < length; i++) {
	    PR_ARENA_ALLOCATE(ale, &cx->tempPool, sizeof(*ale));
	    if (!ale ||
		!XDRAtom1(xdr, ale)) {
		if (!ale)
		    JS_ReportOutOfMemory(cx);
		PR_ARENA_RELEASE(&cx->tempPool, mark);
		return JS_FALSE;
	    }
	    ale->next = al.list;
	    al.count++;
	    al.list = ale;
	}
	if (!js_InitAtomMap(cx, map, &al)) {
	    PR_ARENA_RELEASE(&cx->tempPool, mark);
	    return JS_FALSE;
	}
	PR_ARENA_RELEASE(&cx->tempPool, mark);
    } else if (xdr->mode == JSXDR_ENCODE) {
	JSAtomListElement ale;

	for (i = 0; i < map->length; i++) {
	    ale.atom = map->vector[i];
	    ale.index = i;
	    if (!XDRAtom1(xdr, &ale))
		return JS_FALSE;
	}
    }
    return JS_TRUE;
}

#ifdef JS_XDR_SCRIPT_VERIFIER
/*
 * Make sure that the script is "safe".
 * We currently only check for a balanced stack budget.
 */
static JSBool
VerifyScript(JSContext *cx, JSScript *scr) {
    int depth = 0;
    jsbytecode *pc, *endpc;

    pc = script->code;
    endpc = pc + script->length; /* XXX attack based on falsified length? */

    while (pc < endpc) {
        JSCodeSpec *cs = &js_CodeSpec[(JSOp)*pc];
        intN nuses = cs->nuses;
        if (nuses < 0)
            nuses = 2 + GET_ARGC(pc);
        depth -= nuses;
        if (depth < 0) {        /* underflow */
            JS_ReportError(cx,
                           "script verification failed: stack underflow\n");
            return JS_FALSE;
        }
        depth += cs->ndefs;
        if (depth > maxdepth) {
            maxdepth = depth;
            if (depth > script->depth) { /* overflow */
                JS_ReportError(cx,
                               "script verification failed: stack overflow\n");
                return JS_FALSE;
            }
        }
        if (cs->format & JOF_TYPEMASK == JOF_TABLESWITCH ||
            cs->format & JOF_TYPEMASK == JOF_LOOKUPSWITCH) {
            /*
             * the immediate operand is the length of the switch/lookup table
             * (also the default offset)
             */
            pc += GET_JUMP_OFFSET(pc);
        } else {
            pc += cs->length;
        }
    }
    if (maxdepth != script->depth) { /* perhaps not attack, but malformed */
        JS_ReportError(cx,
                       "script stack budget mismatch (max %d, declared %d)\n",
                       maxdepth, script->depth);
        return JS_FALSE;
    }
    return JS_TRUE;
}
#endif

JSBool
js_XDRScript(JSXDRState *xdr, JSScript **scriptp, JSBool *magic)
{
    JSScript *script;
    uint32 length, lineno, depth, magicval;

    if (xdr->mode == JSXDR_ENCODE)
	magicval = SCRIPT_XDRMAGIC;
    if (!JS_XDRUint32(xdr, &magicval))
	return JS_FALSE;
    if (magicval != SCRIPT_XDRMAGIC) {
	*magic = JS_FALSE;
	return JS_TRUE;
    }
    *magic = JS_TRUE;
    if (xdr->mode == JSXDR_ENCODE) {
	script = *scriptp;
	length = script->length;
	lineno = (uint32)script->lineno;
	depth = (uint32)script->depth;
    }
    if (!JS_XDRUint32(xdr, &length))
	return JS_FALSE;
    if (xdr->mode == JSXDR_DECODE) {
	script = js_NewScript(xdr->cx, length);
	if (!script)
	    return JS_FALSE;
	*scriptp = script;
    }
    if (!JS_XDRBytes(xdr, (char **)&script->code, length) ||
	!XDRAtomMap(xdr, &script->atomMap) ||
	!JS_XDRCStringOrNull(xdr, (char **)&script->notes) ||
	!JS_XDRCStringOrNull(xdr, (char **)&script->filename) ||
	!JS_XDRUint32(xdr, &lineno) ||
	!JS_XDRUint32(xdr, &depth)) {
	if (xdr->mode == JSXDR_DECODE) {
	    js_DestroyScript(xdr->cx, script);
	    *scriptp = NULL;
	}
	return JS_FALSE;
    }
    if (xdr->mode == JSXDR_DECODE) {
	script->lineno = (uintN)lineno;
	script->depth = (uintN)depth;
#ifdef JS_XDR_SCRIPT_VERIFIER
        if (!VerifyScript(xdr->cx, script)) {
            js_DestroyScript(xdr->cx, script);
            *scriptp = NULL;
            return JS_FALSE;
        }
#endif
    }
    return JS_TRUE;
}

static JSBool
script_freeze(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
    JSXDRState *xdr;
    JSScript *script;
    JSBool ok, magic;
    uint32 len;
    void *buf;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &js_ScriptClass, argv))
	return JS_FALSE;
    script = JS_GetPrivate(cx, obj);
    if (!script)
	return JS_TRUE;

    /* create new XDR */
    xdr = JS_XDRNewMem(cx, JSXDR_ENCODE);
    if (!xdr)
	return JS_FALSE;

    /* write  */
    ok = js_XDRScript(xdr, &script, &magic);
    if (!ok)
	goto out;
    if (!magic) {
	*rval = JSVAL_VOID;
	goto out;
    }

    buf = JS_XDRMemGetData(xdr, &len);
    if (!buf) {
	ok = JS_FALSE;
	goto out;
    }

    PR_ASSERT((prword)buf % sizeof(jschar) == 0);
    len /= sizeof(jschar);
    str = JS_NewUCStringCopyN(cx, (jschar *)buf, len);
    if (!str) {
	ok = JS_FALSE;
	goto out;
    }

#if IS_BIG_ENDIAN
  {
    jschar *chars;
    uint32 i;

    /* Swap bytes in Unichars to keep frozen strings machine-independent. */
    chars = JS_GetStringChars(str);
    for (i = 0; i < len; i++)
    	chars[i] = JSXDR_SWAB16(chars[i]);
  }
#endif
    *rval = STRING_TO_JSVAL(str);

out:
    JS_XDRDestroy(xdr);
    return ok;
}

static JSBool
script_thaw(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	    jsval *rval)
{
    JSXDRState *xdr;
    JSString *str;
    void *buf;
    uint32 len;
    JSScript *script, *oldscript;
    JSBool ok, magic;

    if (!JS_InstanceOf(cx, obj, &js_ScriptClass, argv))
	return JS_FALSE;

    if (argc == 0)
    	return JS_TRUE;
    str = js_ValueToString(cx, argv[0]);
    if (!str)
    	return JS_FALSE;

    /* create new XDR */
    xdr = JS_XDRNewMem(cx, JSXDR_DECODE);
    if (!xdr)
	return JS_FALSE;

    buf = JS_GetStringChars(str);
    len = JS_GetStringLength(str);
#if IS_BIG_ENDIAN
  {
    jschar *from, *to;
    uint32 i;

    /* Swap bytes in Unichars to keep frozen strings machine-independent. */
    from = (jschar *)buf;
    to = JS_malloc(cx, len * sizeof(jschar));
    if (!to)
    	return JS_FALSE;
    for (i = 0; i < len; i++)
    	to[i] = JSXDR_SWAB16(from[i]);
    buf = (char *)to;
  }
#endif
    len *= sizeof(jschar);
    JS_XDRMemSetData(xdr, buf, len);

    /* XXXbe should magic mismatch be error, or false return value? */
    ok = js_XDRScript(xdr, &script, &magic);
    if (!ok)
	goto out;
    if (!magic) {
	*rval = JSVAL_FALSE;
	goto out;
    }

    /* Swap script for obj's old script, if any. */
    oldscript = JS_GetPrivate(cx, obj);
    ok = JS_SetPrivate(cx, obj, script);
    if (!ok) {
	JS_free(cx, script);
	goto out;
    }
    if (oldscript)
	js_DestroyScript(cx, oldscript);

    script->object = obj;

out:
    /*
     * We reset the buffer to be NULL so that it doesn't free the chars
     * memory owned by str (argv[0]).
     */
    JS_XDRMemSetData(xdr, NULL, 0);
    JS_XDRDestroy(xdr);
#if IS_BIG_ENDIAN
    JS_free(cx, buf);
#endif
    *rval = JSVAL_TRUE;
    return ok;
}

#endif /* JS_HAS_XDR */

static char js_thaw_str[] = "thaw";

static JSFunctionSpec script_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,   script_toSource,        0},
#endif
    {js_toString_str,   script_toString,        0},
    {"compile",         script_compile,         2},
    {"exec",            script_exec,            1},
#if JS_HAS_XDR
    {"freeze",		script_freeze,		0},
    {js_thaw_str,	script_thaw,		1},
#endif /* JS_HAS_XDR */
    {0}
};

#endif /* JS_HAS_SCRIPT_OBJECT */

static void
script_finalize(JSContext *cx, JSObject *obj)
{
    JSScript *script;

    script = JS_GetPrivate(cx, obj);
    if (script)
    	js_DestroyScript(cx, script);
}

static JSBool
script_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return script_exec(cx, JSVAL_TO_OBJECT(argv[-2]), argc, argv, rval);
}

JSClass js_ScriptClass = {
    "Script",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   script_finalize,
    NULL,             NULL,             script_call,      NULL,/*XXXbe xdr*/
};

#if JS_HAS_SCRIPT_OBJECT

static JSBool
Script(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    /* If not constructing, replace obj with a new Script object. */
    if (!cx->fp->constructing) {
    	obj = js_NewObject(cx, &js_ScriptClass, NULL, NULL);
    	if (!obj)
	    return JS_FALSE;
    }
    return script_compile(cx, obj, argc, argv, rval);
}

#if JS_HAS_XDR

static JSBool
script_static_thaw(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
		   jsval *rval)
{
    obj = js_NewObject(cx, &js_ScriptClass, NULL, NULL);
    if (!obj)
	return JS_FALSE;
    if (!script_thaw(cx, obj, argc, argv, rval))
    	return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSFunctionSpec script_static_methods[] = {
    {js_thaw_str,       script_static_thaw,     1},
    {0}
};

#else  /* !JS_HAS_XDR */

#define script_static_methods   NULL

#endif /* !JS_HAS_XDR */

JSObject *
js_InitScriptClass(JSContext *cx, JSObject *obj)
{
    return JS_InitClass(cx, obj, NULL, &js_ScriptClass, Script, 1,
			NULL, script_methods, NULL, script_static_methods);
}

#endif /* JS_HAS_SCRIPT_OBJECT */

JSScript *
js_NewScript(JSContext *cx, uint32 length)
{
    JSScript *script;

    script = JS_malloc(cx, sizeof(JSScript) + length * sizeof(jsbytecode));
    if (!script)
	return NULL;
    memset(script, 0, sizeof(JSScript));
    script->code = (jsbytecode *)(script + 1);
    script->length = length;
    return script;
}

JSScript *
js_NewScriptFromParams(JSContext *cx, jsbytecode *code, uint32 length,
		       const char *filename, uintN lineno, uintN depth,
		       jssrcnote *notes, JSTryNote *trynotes,
		       JSPrincipals *principals)
{
    JSScript *script;

    script = js_NewScript(cx, length);
    if (!script)
        return NULL;
    memcpy(script->code, code, length * sizeof(jsbytecode));
    if (filename) {
        script->filename = JS_strdup(cx, filename);
        if (!script->filename) {
	    js_DestroyScript(cx, script);
	    return NULL;
	}
    }
    script->lineno = lineno;
    script->depth = depth;
    script->notes = notes;
    script->trynotes = trynotes;
    if (principals)
        JSPRINCIPALS_HOLD(cx, principals);
    script->principals = principals;
    return script;
}

JS_FRIEND_API(JSScript *)
js_NewScriptFromCG(JSContext *cx, JSCodeGenerator *cg, JSFunction *fun)
{
    JSTryNote *trynotes;
    jssrcnote *notes;
    JSScript *script;
    JSRuntime *rt;
    JSNewScriptHook hook;

    if (!js_FinishTakingTryNotes(cx, cg, &trynotes))
	return NULL;
    notes = js_FinishTakingSrcNotes(cx, cg);
    script = js_NewScriptFromParams(cx, cg->base, CG_OFFSET(cg),
				    cg->filename, cg->firstLine,
				    cg->maxStackDepth, notes, trynotes,
				    cg->principals);
    if (!script)
        return NULL;
    if (!notes || !js_InitAtomMap(cx, &script->atomMap, &cg->atomList)) {
	js_DestroyScript(cx, script);
	return NULL;
    }

    /* Tell the debugger about this compiled script. */
    rt = cx->runtime;
    hook = rt->newScriptHook;
    if (hook) {
        (*hook)(cx, cg->filename, cg->firstLine, script, fun,
        	rt->newScriptHookData);
    }
    return script;
}

void
js_DestroyScript(JSContext *cx, JSScript *script)
{
    JSRuntime *rt;
    JSDestroyScriptHook hook;

    rt = cx->runtime;
    hook = rt->destroyScriptHook;
    if (hook)
        (*hook)(cx, script, rt->destroyScriptHookData);

    JS_ClearScriptTraps(cx, script);
    js_FreeAtomMap(cx, &script->atomMap);
    if (js_InterpreterHooks && js_InterpreterHooks->destroyScript)
        js_InterpreterHooks->destroyScript(cx, script);
    JS_free(cx, (void *)script->filename);
    JS_free(cx, script->notes);
    JS_free(cx, script->trynotes);
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
    target = PTRDIFF(pc, script->code, jsbytecode);
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
    target = PTRDIFF(pc, script->code, jsbytecode);
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
