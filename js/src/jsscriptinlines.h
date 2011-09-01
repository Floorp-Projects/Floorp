/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79 ft=cpp:
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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsscriptinlines_h___
#define jsscriptinlines_h___

#include "jsautooplen.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsopcode.h"
#include "jsregexp.h"
#include "jsscript.h"
#include "jsscope.h"
#include "vm/GlobalObject.h"

#include "jsscopeinlines.h"

namespace js {

inline
Bindings::Bindings(JSContext *cx)
  : lastBinding(NULL), nargs(0), nvars(0), nupvars(0),
    hasExtensibleParents(false)
{
}

inline void
Bindings::transfer(JSContext *cx, Bindings *bindings)
{
    JS_ASSERT(!lastBinding);

    *this = *bindings;
#ifdef DEBUG
    bindings->lastBinding = NULL;
#endif

    /* Preserve back-pointer invariants across the lastBinding transfer. */
    if (lastBinding && lastBinding->inDictionary())
        lastBinding->listp = &this->lastBinding;
}

inline void
Bindings::clone(JSContext *cx, Bindings *bindings)
{
    JS_ASSERT(!lastBinding);

    /*
     * Non-dictionary bindings are fine to share, as are dictionary bindings if
     * they're copy-on-modification.
     */
    JS_ASSERT(!bindings->lastBinding ||
              !bindings->lastBinding->inDictionary() ||
              bindings->lastBinding->frozen());

    *this = *bindings;
}

Shape *
Bindings::lastShape() const
{
    JS_ASSERT(lastBinding);
    JS_ASSERT_IF(lastBinding->inDictionary(), lastBinding->frozen());
    return lastBinding;
}

bool
Bindings::ensureShape(JSContext *cx)
{
    if (!lastBinding) {
        lastBinding = EmptyShape::getEmptyCallShape(cx);
        if (!lastBinding)
            return false;
    }
    return true;
}

extern const char *
CurrentScriptFileAndLineSlow(JSContext *cx, uintN *linenop);

inline const char *
CurrentScriptFileAndLine(JSContext *cx, uintN *linenop, LineOption opt)
{
    if (opt == CALLED_FROM_JSOP_EVAL) {
        JS_ASSERT(js_GetOpcode(cx, cx->fp()->script(), cx->regs().pc) == JSOP_EVAL);
        JS_ASSERT(*(cx->regs().pc + JSOP_EVAL_LENGTH) == JSOP_LINENO);
        *linenop = GET_UINT16(cx->regs().pc + JSOP_EVAL_LENGTH);
        return cx->fp()->script()->filename;
    }

    return CurrentScriptFileAndLineSlow(cx, linenop);
}

} // namespace js

inline JSFunction *
JSScript::getFunction(size_t index)
{
    JSObject *funobj = getObject(index);
    JS_ASSERT(funobj->isFunction());
    JS_ASSERT(funobj == (JSObject *) funobj->getPrivate());
    JSFunction *fun = (JSFunction *) funobj;
    JS_ASSERT(fun->isInterpreted());
    return fun;
}

inline JSFunction *
JSScript::getCallerFunction()
{
    JS_ASSERT(savedCallerFun);
    return getFunction(0);
}

inline JSObject *
JSScript::getRegExp(size_t index)
{
    JSObjectArray *arr = regexps();
    JS_ASSERT((uint32) index < arr->length);
    JSObject *obj = arr->vector[index];
    JS_ASSERT(obj->getClass() == &js_RegExpClass);
    return obj;
}

inline bool
JSScript::isEmpty() const
{
    if (length > 3)
        return false;

    jsbytecode *pc = code;
    if (noScriptRval && JSOp(*pc) == JSOP_FALSE)
        ++pc;
    return JSOp(*pc) == JSOP_STOP;
}

inline bool
JSScript::hasGlobal() const
{
    /*
     * Make sure that we don't try to query information about global objects
     * which have had their scopes cleared. compileAndGo code should not run
     * anymore against such globals.
     */
    JS_ASSERT(types && types->hasScope());
    js::GlobalObject *obj = types->global;
    return obj && !obj->isCleared();
}

inline js::GlobalObject *
JSScript::global() const
{
    JS_ASSERT(hasGlobal());
    return types->global;
}

inline bool
JSScript::hasClearedGlobal() const
{
    JS_ASSERT(types && types->hasScope());
    js::GlobalObject *obj = types->global;
    return obj && obj->isCleared();
}

inline JSFunction *
JSScript::function() const
{
    JS_ASSERT(hasFunction && types);
    return types->function;
}

inline js::types::TypeScriptNesting *
JSScript::nesting() const
{
    JS_ASSERT(hasFunction && types && types->hasScope());
    return types->nesting;
}

inline void
JSScript::clearNesting()
{
    js::types::TypeScriptNesting *nesting = this->nesting();
    if (nesting) {
        js::Foreground::delete_(nesting);
        types->nesting = NULL;
    }
}

#endif /* jsscriptinlines_h___ */
