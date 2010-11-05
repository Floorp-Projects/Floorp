/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Copyright (C) 2007  Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *      Brendan Eich <brendan@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef INCLUDE_MOZILLA_DTRACE
#include "javascript-trace.h"
#endif
#include "jspubtd.h"
#include "jsprvtd.h"

#ifndef _JSPROBES_H
#define _JSPROBES_H

namespace js {

class Probes {
    static const char nullName[];

    static const char *FunctionClassname(const JSFunction *fun);
    static const char *ScriptFilename(JSScript *script);
    static int FunctionLineNumber(JSContext *cx, const JSFunction *fun);
    static const char *FunctionName(JSContext *cx, const JSFunction *fun);

    static void enterJSFunImpl(JSContext *cx, const JSFunction *fun);
    static void handleFunctionReturn(JSContext *cx, JSFunction *fun);
    static void finalizeObjectImpl(JSObject *obj);
  public:
    /*
     * If |lval| is provided to the enter/exit methods, it is tested to see if
     * it is a function as a predicate to the dtrace event emission.
     */
    static void enterJSFun(JSContext *cx, JSFunction *fun, js::Value *lval = NULL);
    static void exitJSFun(JSContext *cx, JSFunction *fun, js::Value *lval = NULL);

    static void startExecution(JSContext *cx, JSScript *script);
    static void stopExecution(JSContext *cx, JSScript *script);

    static void resizeHeap(JSCompartment *compartment, size_t oldSize, size_t newSize);

    /* |obj| must exist (its class and size are computed) */
    static void createObject(JSContext *cx, JSObject *obj);

    static void resizeObject(JSContext *cx, JSObject *obj, size_t oldSize, size_t newSize);

    /* |obj| must still exist (its class is accessed) */
    static void finalizeObject(JSObject *obj);

    /*
     * |string| does not need to contain any content yet; only its
     * pointer value is used. |length| is the length of the string and
     * does not imply anything about the amount of storage consumed to
     * store the string. (It may be a short string, an external
     * string, or a rope, and the encoding is not taken into
     * consideration.)
     */
    static void createString(JSContext *cx, JSString *string, size_t length);

    /*
     * |string| must still have a valid length.
     */
    static void finalizeString(JSString *string);

    static void compileScriptBegin(JSContext *cx, const char *filename, int lineno);
    static void compileScriptEnd(JSContext *cx, JSScript *script, const char *filename, int lineno);

    static void calloutBegin(JSContext *cx, JSFunction *fun);
    static void calloutEnd(JSContext *cx, JSFunction *fun);

    static void acquireMemory(JSContext *cx, void *address, size_t nbytes);
    static void releaseMemory(JSContext *cx, void *address, size_t nbytes);

    static void GCStart(JSCompartment *compartment);
    static void GCEnd(JSCompartment *compartment);
    static void GCStartMarkPhase(JSCompartment *compartment);

    static void GCEndMarkPhase(JSCompartment *compartment);
    static void GCStartSweepPhase(JSCompartment *compartment);
    static void GCEndSweepPhase(JSCompartment *compartment);

    static JSBool CustomMark(JSString *string);
    static JSBool CustomMark(const char *string);
    static JSBool CustomMark(int marker);
};

inline void
Probes::enterJSFun(JSContext *cx, JSFunction *fun, js::Value *lval)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (!lval || IsFunctionObject(*lval)) {
        if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED())
            enterJSFunImpl(cx, fun);
    }
#endif
#ifdef MOZ_TRACE_JSCALLS
    cx->doFunctionCallback(fun, fun ? FUN_SCRIPT(fun) : NULL, true);
#endif
}

inline void
Probes::exitJSFun(JSContext *cx, JSFunction *fun, js::Value *lval)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (!lval || IsFunctionObject(*lval)) {
        if (JAVASCRIPT_FUNCTION_RETURN_ENABLED())
            handleFunctionReturn(cx, fun);
    }
#endif
#ifdef MOZ_TRACE_JSCALLS
    cx->doFunctionCallback(fun, fun ? FUN_SCRIPT(fun) : NULL, false);
#endif
}

inline void
Probes::createObject(JSContext *cx, JSObject *obj)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_CREATE_ENABLED()) {
        Class *clasp = obj->getClass();
        JAVASCRIPT_OBJECT_CREATE((char *)clasp->name, (uintptr_t)obj);
    }
#endif
}

inline void
Probes::finalizeObject(JSObject *obj)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_FINALIZE_ENABLED()) {
        Class *clasp = obj->getClass();

        /* the first arg is NULL - reserved for future use (filename?) */
        JAVASCRIPT_OBJECT_FINALIZE(NULL, (char *)clasp->name, (uintptr_t)obj);
    }
#endif
}

inline void
Probes::startExecution(JSContext *cx, JSScript *script)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_START_ENABLED())
        JAVASCRIPT_EXECUTE_START(script->filename ? (char *)script->filename : nullName,
                                 script->lineno);
#endif
#ifdef MOZ_TRACE_JSCALLS
    cx->doFunctionCallback(NULL, script, true);
#endif
}

inline void
Probes::stopExecution(JSContext *cx, JSScript *script)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_DONE_ENABLED())
        JAVASCRIPT_EXECUTE_DONE(script->filename ? (char *)script->filename : nullName,
                                script->lineno);
#endif
#ifdef MOZ_TRACE_JSCALLS
    cx->doFunctionCallback(NULL, script, false);
#endif
}

/*
 * New probes with no implementations, yet. Next patch will implement
 * them. These are here just to make all intermediate patches compile
 * and run.
 */
inline void Probes::resizeHeap(JSCompartment *compartment, size_t oldSize, size_t newSize) {}
inline void Probes::resizeObject(JSContext *cx, JSObject *obj, size_t oldSize, size_t newSize) {}
inline void Probes::createString(JSContext *cx, JSString *string, size_t length) {}
inline void Probes::finalizeString(JSString *string) {}
inline void Probes::compileScriptBegin(JSContext *cx, const char *filename, int lineno) {}
inline void Probes::compileScriptEnd(JSContext *cx, JSScript *script, const char *filename, int lineno) {}
inline void Probes::calloutBegin(JSContext *cx, JSFunction *fun) {}
inline void Probes::calloutEnd(JSContext *cx, JSFunction *fun) {}
inline void Probes::acquireMemory(JSContext *cx, void *address, size_t nbytes) {}
inline void Probes::releaseMemory(JSContext *cx, void *address, size_t nbytes) {}
inline void Probes::GCStart(JSCompartment *compartment) {}
inline void Probes::GCEnd(JSCompartment *compartment) {}
inline void Probes::GCStartMarkPhase(JSCompartment *compartment) {}
inline void Probes::GCEndMarkPhase(JSCompartment *compartment) {}
inline void Probes::GCStartSweepPhase(JSCompartment *compartment) {}
inline void Probes::GCEndSweepPhase(JSCompartment *compartment) {}
inline JSBool Probes::CustomMark(JSString *string) { return JS_TRUE; }
inline JSBool Probes::CustomMark(const char *string) { return JS_TRUE; }
inline JSBool Probes::CustomMark(int marker) { return JS_TRUE; }

struct AutoFunctionCallProbe {
    JSContext * const cx;
    JSFunction *fun;
    js::Value *lval;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

    AutoFunctionCallProbe(JSContext *cx, JSFunction *fun, js::Value *lval = NULL
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), fun(fun), lval(lval)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        Probes::enterJSFun(cx, fun, lval);
    }

    ~AutoFunctionCallProbe() {
        Probes::exitJSFun(cx, fun, lval);
    }
};

} /* namespace js */
    
#endif /* _JSPROBES_H */
