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

/*
 * JS script operations.
 */
#include "jsstddef.h"
#include <string.h>
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
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
    script = (JSScript *) JS_GetPrivate(cx, obj);

    /* Let n count the source string length, j the "front porch" length. */
    j = JS_snprintf(buf, sizeof buf, "(new %s(", js_ScriptClass.name);
    n = j + 2;
    if (!script) {
        /* Let k count the constructor argument string length. */
        k = 0;
        s = NULL;               /* quell GCC overwarning */
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
    t = (jschar *) JS_malloc(cx, (n + 1) * sizeof(jschar));
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
    script = (JSScript *) JS_GetPrivate(cx, obj);
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

    /* Make sure obj is a Script object. */
    if (!JS_InstanceOf(cx, obj, &js_ScriptClass, argv))
        return JS_FALSE;

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
    JS_ASSERT(fp->scopeChain == caller->scopeChain);

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
    oldscript = (JSScript *) JS_GetPrivate(cx, obj);
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
    script = (JSScript *) JS_GetPrivate(cx, obj);
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
    JS_ASSERT(fp->scopeChain == caller->scopeChain);
    fp->sharpArray = caller->sharpArray;
    return js_Execute(cx, scopeobj, script, fp, 0, rval);
}

#if JS_HAS_XDR
static JSBool
XDRAtomListElement(JSXDRState *xdr, JSAtomListElement *ale)
{
    jsval value;
    jsatomid index;

    if (xdr->mode == JSXDR_ENCODE)
        value = ATOM_KEY(ALE_ATOM(ale));

    index = ALE_INDEX(ale);
    if (!JS_XDRUint32(xdr, &index))
        return JS_FALSE;
    ALE_SET_INDEX(ale, index);

    if (!JS_XDRValue(xdr, &value))
        return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE) {
        if (!ALE_SET_ATOM(ale, js_AtomizeValue(xdr->cx, value, 0)))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
XDRAtomMap(JSXDRState *xdr, JSAtomMap *map)
{
    uint32 length;
    uintN i;
    JSBool ok;

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
        mark = JS_ARENA_MARK(&cx->tempPool);
        ATOM_LIST_INIT(&al);
        for (i = 0; i < length; i++) {
            JS_ARENA_ALLOCATE_TYPE(ale, JSAtomListElement, &cx->tempPool);
            if (!ale ||
                !XDRAtomListElement(xdr, ale)) {
                if (!ale)
                    JS_ReportOutOfMemory(cx);
                JS_ARENA_RELEASE(&cx->tempPool, mark);
                return JS_FALSE;
            }
            ALE_SET_NEXT(ale, al.list);
            al.count++;
            al.list = ale;
        }
        ok = js_InitAtomMap(cx, map, &al);
        JS_ARENA_RELEASE(&cx->tempPool, mark);
        return ok;
    }

    if (xdr->mode == JSXDR_ENCODE) {
        JSAtomListElement ale;

        for (i = 0; i < map->length; i++) {
            ALE_SET_ATOM(&ale, map->vector[i]);
            ALE_SET_INDEX(&ale, i);
            if (!XDRAtomListElement(xdr, &ale))
                return JS_FALSE;
        }
    }
    return JS_TRUE;
}

JSBool
js_XDRScript(JSXDRState *xdr, JSScript **scriptp, JSBool *hasMagic)
{
    JSScript *script;
    uint32 length, lineno, depth, magic, notelen, numtrys;
    uint32 prologLength, version;

    script = *scriptp;
    numtrys = 0;

    /*
     * Encode prologLength and version after script->length (_2 or greater),
     * but decode both new (>= _2) and old, prolog&version-free (_1) scripts.
     * Version _3 supports principals serialization.
     */
    if (xdr->mode == JSXDR_ENCODE)
        magic = JSXDR_MAGIC_SCRIPT_CURRENT;
    if (!JS_XDRUint32(xdr, &magic))
        return JS_FALSE;
    if (magic != JSXDR_MAGIC_SCRIPT_3 &&
        magic != JSXDR_MAGIC_SCRIPT_2 &&
        magic != JSXDR_MAGIC_SCRIPT_1) {
        *hasMagic = JS_FALSE;
        return JS_TRUE;
    }
    *hasMagic = JS_TRUE;

    if (xdr->mode == JSXDR_ENCODE) {
        jssrcnote *sn = script->notes;
        length = script->length;
        prologLength = script->main - script->code;
        version = (int32) script->version;
        lineno = (uint32)script->lineno;
        depth = (uint32)script->depth;

        /* Count the trynotes. */
        if (script->trynotes) {
            for (; script->trynotes[numtrys].catchStart; numtrys++)
                continue;
            numtrys++;          /* room for the end marker */
        }

        /* Count the src notes. */
        while (!SN_IS_TERMINATOR(sn))
            sn = SN_NEXT(sn);
        notelen = sn - script->notes;
        notelen++;              /* room for the terminator */
    }

    if (!JS_XDRUint32(xdr, &length))
        return JS_FALSE;
    if (magic >= JSXDR_MAGIC_SCRIPT_2) {
        if (!JS_XDRUint32(xdr, &prologLength))
            return JS_FALSE;
        if (!JS_XDRUint32(xdr, &version))
            return JS_FALSE;
    }

    if (xdr->mode == JSXDR_DECODE) {
        script = js_NewScript(xdr->cx, length);
        if (!script)
            return JS_FALSE;
        if (magic >= JSXDR_MAGIC_SCRIPT_2) {
            script->main += prologLength;
            script->version = (JSVersion) version;
        }
        *scriptp = script;
    }

    if (!JS_XDRBytes(xdr, (char **)&script->code, length) ||
        !XDRAtomMap(xdr, &script->atomMap) ||
        !JS_XDRUint32(xdr, &notelen) ||
        /* malloc on decode only */
        (!(xdr->mode == JSXDR_ENCODE ||
           (script->notes = (jssrcnote *)
            JS_malloc(xdr->cx, notelen)) != NULL)) ||
        !JS_XDRBytes(xdr, (char **)&script->notes, notelen) ||
        !JS_XDRCStringOrNull(xdr, (char **)&script->filename) ||
        !JS_XDRUint32(xdr, &lineno) ||
        !JS_XDRUint32(xdr, &depth) ||
        !JS_XDRUint32(xdr, &numtrys)) {
        goto error;
    }

    if (magic >= JSXDR_MAGIC_SCRIPT_3) {
        JSPrincipals *principals;
        uint32 encodeable;

        if (xdr->mode == JSXDR_ENCODE) {
            principals = script->principals;
            encodeable = (xdr->cx->runtime->principalsTranscoder != NULL);
            if (!JS_XDRUint32(xdr, &encodeable))
                goto error;
            if (encodeable &&
                !xdr->cx->runtime->principalsTranscoder(xdr, &principals)) {
                goto error;
            }
        } else {
            if (!JS_XDRUint32(xdr, &encodeable))
                goto error;
            if (encodeable) {
                if (!xdr->cx->runtime->principalsTranscoder) {
                    JS_ReportErrorNumber(xdr->cx, js_GetErrorMessage, NULL,
                                         JSMSG_CANT_DECODE_PRINCIPALS);
                    goto error;
                }
                if (!xdr->cx->runtime->principalsTranscoder(xdr, &principals))
                    goto error;
                script->principals = principals;
            }
        }
    }

    if (xdr->mode == JSXDR_DECODE) {
        script->lineno = (uintN)lineno;
        script->depth = (uintN)depth;
        if (numtrys) {
            script->trynotes = (JSTryNote *)
                JS_malloc(xdr->cx, sizeof(JSTryNote) * (numtrys + 1));
            if (!script->trynotes)
                goto error;
        } else {
            script->trynotes = NULL;
        }
    }
    for (; numtrys; numtrys--) {
        JSTryNote *tn = &script->trynotes[numtrys - 1];
        uint32 start = (ptrdiff_t) tn->start,
               catchLength = (ptrdiff_t) tn->length,
               catchStart = (ptrdiff_t) tn->catchStart;
        if (!JS_XDRUint32(xdr, &start) ||
            !JS_XDRUint32(xdr, &catchLength) ||
            !JS_XDRUint32(xdr, &catchStart)) {
            goto error;
        }
        tn->start = (ptrdiff_t) start;
        tn->length = (ptrdiff_t) catchLength;
        tn->catchStart = (ptrdiff_t) catchStart;
    }
    return JS_TRUE;

  error:
    if (xdr->mode == JSXDR_DECODE) {
        js_DestroyScript(xdr->cx, script);
        *scriptp = NULL;
    }
    return JS_FALSE;
}

static JSBool
script_freeze(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    JSXDRState *xdr;
    JSScript *script;
    JSBool ok, hasMagic;
    uint32 len;
    void *buf;
    JSString *str;

    if (!JS_InstanceOf(cx, obj, &js_ScriptClass, argv))
        return JS_FALSE;
    script = (JSScript *) JS_GetPrivate(cx, obj);
    if (!script)
        return JS_TRUE;

    /* create new XDR */
    xdr = JS_XDRNewMem(cx, JSXDR_ENCODE);
    if (!xdr)
        return JS_FALSE;

    /* write  */
    ok = js_XDRScript(xdr, &script, &hasMagic);
    if (!ok)
        goto out;
    if (!hasMagic) {
        *rval = JSVAL_VOID;
        goto out;
    }

    buf = JS_XDRMemGetData(xdr, &len);
    if (!buf) {
        ok = JS_FALSE;
        goto out;
    }

    JS_ASSERT((jsword)buf % sizeof(jschar) == 0);
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
    JSBool ok, hasMagic;

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
    to = (jschar *) JS_malloc(cx, len * sizeof(jschar));
    if (!to) {
        JS_XDRDestroy(xdr);
        return JS_FALSE;
    }
    for (i = 0; i < len; i++)
        to[i] = JSXDR_SWAB16(from[i]);
    buf = (char *)to;
  }
#endif
    len *= sizeof(jschar);
    JS_XDRMemSetData(xdr, buf, len);

    /* XXXbe should magic mismatch be error, or false return value? */
    ok = js_XDRScript(xdr, &script, &hasMagic);
    if (!ok)
        goto out;
    if (!hasMagic) {
        *rval = JSVAL_FALSE;
        goto out;
    }

    /* Swap script for obj's old script, if any. */
    oldscript = (JSScript *) JS_GetPrivate(cx, obj);
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
    {js_toSource_str,   script_toSource,        0,0,0},
#endif
    {js_toString_str,   script_toString,        0,0,0},
    {"compile",         script_compile,         2,0,0},
    {"exec",            script_exec,            1,0,0},
#if JS_HAS_XDR
    {"freeze",          script_freeze,          0,0,0},
    {js_thaw_str,       script_thaw,            1,0,0},
#endif /* JS_HAS_XDR */
    {0,0,0,0,0}
};

#endif /* JS_HAS_SCRIPT_OBJECT */

static void
script_finalize(JSContext *cx, JSObject *obj)
{
    JSScript *script;

    script = (JSScript *) JS_GetPrivate(cx, obj);
    if (script)
        js_DestroyScript(cx, script);
}

static JSBool
script_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if JS_HAS_SCRIPT_OBJECT
    return script_exec(cx, JSVAL_TO_OBJECT(argv[-2]), argc, argv, rval);
#else
    return JS_FALSE;
#endif
}

static uint32
script_mark(JSContext *cx, JSObject *obj, void *arg)
{
    JSScript *script;

    script = (JSScript *) JS_GetPrivate(cx, obj);
    if (script)
        js_MarkScript(cx, script, arg);
    return 0;
}

JS_FRIEND_DATA(JSClass) js_ScriptClass = {
    js_Script_str,
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   script_finalize,
    NULL,             NULL,             script_call,      NULL,/*XXXbe xdr*/
    NULL,             NULL,             script_mark,      0
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
    {js_thaw_str,       script_static_thaw,     1,0,0},
    {0,0,0,0,0}
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

    script = (JSScript *)
        JS_malloc(cx, sizeof(JSScript) + length * sizeof(jsbytecode));
    if (!script)
        return NULL;
    memset(script, 0, sizeof(JSScript));
    script->code = script->main = (jsbytecode *)(script + 1);
    script->length = length;
    script->version = cx->version;
    return script;
}

JSScript *
js_NewScriptFromParams(JSContext *cx, jsbytecode *code, uint32 length,
                       jsbytecode *prolog, uint32 prologLength,
                       const char *filename, uintN lineno, uintN depth,
                       jssrcnote *notes, JSTryNote *trynotes,
                       JSPrincipals *principals)
{
    JSScript *script;

    script = js_NewScript(cx, prologLength + length);
    if (!script)
        return NULL;
    script->main += prologLength;
    memcpy(script->code, prolog, prologLength * sizeof(jsbytecode));
    memcpy(script->main, code, length * sizeof(jsbytecode));
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
    script = js_NewScriptFromParams(cx, CG_BASE(cg), CG_OFFSET(cg),
                                    CG_PROLOG_BASE(cg), CG_PROLOG_OFFSET(cg),
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
        /*
         * We use a dummy stack frame to protect the script from a GC caused
         * by debugger-hook execution.
         *
         * XXX We really need a way to manage local roots and such more
         * XXX automatically, at which point we can remove this one-off hack
         * XXX and others within the engine.  See bug 40757 for discussion.
         */
        JSStackFrame dummy;
        
        memset(&dummy, 0, sizeof dummy);
        dummy.down = cx->fp;
        dummy.script = script;
        cx->fp = &dummy;

        (*hook)(cx, cg->filename, cg->firstLine, script, fun,
                rt->newScriptHookData);

        cx->fp = dummy.down;
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
    JS_free(cx, (void *)script->filename);
    JS_free(cx, script->notes);
    JS_free(cx, script->trynotes);
    if (script->principals)
        JSPRINCIPALS_DROP(cx, script->principals);
    JS_free(cx, script);
}

void
js_MarkScript(JSContext *cx, JSScript *script, void *arg)
{
    JSAtomMap *map;
    uintN i, length;
    JSAtom **vector;

    map = &script->atomMap;
    length = map->length;
    vector = map->vector;
    for (i = 0; i < length; i++)
        GC_MARK_ATOM(cx, vector[i], arg);
}

jssrcnote *
js_GetSrcNote(JSScript *script, jsbytecode *pc)
{
    jssrcnote *sn;
    ptrdiff_t offset, target;

    sn = script->notes;
    if (!sn)
        return NULL;
    target = PTRDIFF(pc, script->main, jsbytecode);
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
    target = PTRDIFF(pc, script->main, jsbytecode);
    lineno = script->lineno;
    for (offset = 0; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        offset += SN_DELTA(sn);
        type = (JSSrcNoteType) SN_TYPE(sn);
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
        type = (JSSrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            lineno = (uintN) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            lineno++;
        }
    }
    return script->main + offset;
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
        type = (JSSrcNoteType) SN_TYPE(sn);
        if (type == SRC_SETLINE) {
            lineno = (uintN) js_GetSrcNoteOffset(sn, 0);
        } else if (type == SRC_NEWLINE) {
            lineno++;
        }
    }
    return 1 + lineno - script->lineno;
}
