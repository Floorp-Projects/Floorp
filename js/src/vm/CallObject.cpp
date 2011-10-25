/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 * The Original Code is SpiderMonkey call object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Biggar <pbiggar@mozilla.com> (original author)
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

#include "jsobjinlines.h"
#include "CallObject.h"

#include "CallObject-inl.h"

namespace js {

/*
 * Construct a call object for the given bindings.  If this is a call object
 * for a function invocation, callee should be the function being called.
 * Otherwise it must be a call object for eval of strict mode code, and callee
 * must be null.
 */
CallObject *
CallObject::create(JSContext *cx, JSScript *script, JSObject &scopeChain, JSObject *callee)
{
    Bindings &bindings = script->bindings;
    size_t argsVars = bindings.countArgsAndVars();
    size_t slots = RESERVED_SLOTS + argsVars;
    gc::AllocKind kind = gc::GetGCObjectKind(slots);

    /*
     * Make sure that the arguments and variables in the call object all end up
     * in a contiguous range of slots. We need this to be able to embed the
     * args/vars arrays in the TypeScriptNesting for the function, after the
     * call object's frame has finished.
     */
    if (cx->typeInferenceEnabled() && gc::GetGCKindSlots(kind) < slots) {
        kind = gc::GetGCObjectKind(RESERVED_SLOTS);
        JS_ASSERT(gc::GetGCKindSlots(kind) == RESERVED_SLOTS);
    }

    JSObject *obj = js_NewGCObject(cx, kind);
    if (!obj)
        return NULL;

    /* Init immediately to avoid GC seeing a half-init'ed object. */
    obj->initCall(cx, bindings, &scopeChain);
    obj->makeVarObj();

    /* This must come after callobj->lastProp has been set. */
    if (!obj->ensureInstanceReservedSlots(cx, argsVars))
        return NULL;

#ifdef DEBUG
    for (Shape::Range r = obj->lastProperty(); !r.empty(); r.popFront()) {
        const Shape &s = r.front();
        if (s.slot != SHAPE_INVALID_SLOT) {
            JS_ASSERT(s.slot + 1 == obj->slotSpan());
            break;
        }
    }
#endif

    CallObject &callobj = obj->asCall();
    callobj.initCallee(callee);
    return &callobj;
}

}
