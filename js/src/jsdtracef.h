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

#ifndef _JSDTRACEF_H
#define _JSDTRACEF_H

namespace js {

class DTrace {
    static void enterJSFunImpl(JSContext *cx, JSStackFrame *fp, const JSFunction *fun);
    static void handleFunctionInfo(JSContext *cx, JSStackFrame *fp, JSStackFrame *dfp,
                                   JSFunction *fun);
    static void handleFunctionArgs(JSContext *cx, JSStackFrame *fp, const JSFunction *fun,
                                   jsuint argc, jsval *argv);
    static void handleFunctionRval(JSContext *cx, JSStackFrame *fp, JSFunction *fun, jsval rval);
    static void handleFunctionReturn(JSContext *cx, JSStackFrame *fp, JSFunction *fun);
    static void finalizeObjectImpl(JSObject *obj);
  public:
    /*
     * If |lval| is provided to the enter/exit methods, it is tested to see if
     * it is a function as a predicate to the dtrace event emission.
     */
    static void enterJSFun(JSContext *cx, JSStackFrame *fp, JSFunction *fun,
                           JSStackFrame *dfp, jsuint argc, jsval *argv, jsval *lval = NULL);
    static void exitJSFun(JSContext *cx, JSStackFrame *fp, JSFunction *fun, jsval rval,
                          jsval *lval = NULL);

    static void finalizeObject(JSObject *obj);

    class ExecutionScope {
        const JSScript *script;
        void startExecution();
        void endExecution();
      public:
        explicit ExecutionScope(JSScript *script);
        ~ExecutionScope();
    };

    class ObjectCreationScope {
        JSContext       * const cx;
        JSStackFrame    * const fp;
        JSClass         * const clasp;
        void handleCreationStart();
        void handleCreationImpl(JSObject *obj);
        void handleCreationEnd();
      public:
        ObjectCreationScope(JSContext *cx, JSStackFrame *fp, JSClass *clasp);
        void handleCreation(JSObject *obj);
        ~ObjectCreationScope();
    };

};

inline void
DTrace::enterJSFun(JSContext *cx, JSStackFrame *fp, JSFunction *fun, JSStackFrame *dfp,
                   jsuint argc, jsval *argv, jsval *lval)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (!lval || VALUE_IS_FUNCTION(cx, *lval)) {
        if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED())
            enterJSFunImpl(cx, fp, fun);
        if (JAVASCRIPT_FUNCTION_INFO_ENABLED())
            handleFunctionInfo(cx, fp, dfp, fun);
        if (JAVASCRIPT_FUNCTION_ARGS_ENABLED())
            handleFunctionArgs(cx, fp, fun, argc, argv);
    }
#endif
}

inline void
DTrace::exitJSFun(JSContext *cx, JSStackFrame *fp, JSFunction *fun, jsval rval, jsval *lval)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (!lval || VALUE_IS_FUNCTION(cx, *lval)) {
        if (JAVASCRIPT_FUNCTION_RVAL_ENABLED())
            handleFunctionRval(cx, fp, fun, rval);
        if (JAVASCRIPT_FUNCTION_RETURN_ENABLED())
            handleFunctionReturn(cx, fp, fun);
    }
#endif
}

inline void
DTrace::finalizeObject(JSObject *obj)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_FINALIZE_ENABLED())
        finalizeObjectImpl(obj);
#endif
}

/* Execution scope. */

inline
DTrace::ExecutionScope::ExecutionScope(JSScript *script)
  : script(script)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_START_ENABLED())
        startExecution();
#endif
}

inline
DTrace::ExecutionScope::~ExecutionScope()
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_DONE_ENABLED())
        endExecution();
#endif
}

/* Object creation scope. */

inline
DTrace::ObjectCreationScope::ObjectCreationScope(JSContext *cx, JSStackFrame *fp, JSClass *clasp)
  : cx(cx), fp(fp), clasp(clasp)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_CREATE_START_ENABLED())
        handleCreationStart();
#endif
}

inline void
DTrace::ObjectCreationScope::handleCreation(JSObject *obj)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_CREATE_ENABLED())
        handleCreationImpl(obj);
#endif
}

inline
DTrace::ObjectCreationScope::~ObjectCreationScope()
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_CREATE_DONE_ENABLED())
        handleCreationEnd();
#endif
}

} /* namespace js */
    
#endif /* _JSDTRACE_H */
