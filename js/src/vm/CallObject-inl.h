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

#ifndef CallObject_inl_h___
#define CallObject_inl_h___

#include "CallObject.h"

namespace js {

inline bool
CallObject::isForEval() const
{
    JS_ASSERT(getFixedSlot(CALLEE_SLOT).isObjectOrNull());
    JS_ASSERT_IF(getFixedSlot(CALLEE_SLOT).isObject(),
                 getFixedSlot(CALLEE_SLOT).toObject().isFunction());
    return getFixedSlot(CALLEE_SLOT).isNull();
}

inline js::StackFrame *
CallObject::maybeStackFrame() const
{
    return reinterpret_cast<js::StackFrame *>(getPrivate());
}

inline void
CallObject::setStackFrame(StackFrame *frame)
{
    return setPrivate(frame);
}

inline void
CallObject::setCallee(JSObject *callee)
{
    JS_ASSERT_IF(callee, callee->isFunction());
    setFixedSlot(CALLEE_SLOT, js::ObjectOrNullValue(callee));
}

inline JSObject *
CallObject::getCallee() const
{
    return getFixedSlot(CALLEE_SLOT).toObjectOrNull();
}

inline JSFunction *
CallObject::getCalleeFunction() const
{
    return getFixedSlot(CALLEE_SLOT).toObject().getFunctionPrivate();
}

inline const js::Value &
CallObject::getArguments() const
{
    JS_ASSERT(!isForEval());
    return getFixedSlot(ARGUMENTS_SLOT);
}

inline void
CallObject::setArguments(const js::Value &v)
{
    JS_ASSERT(!isForEval());
    setFixedSlot(ARGUMENTS_SLOT, v);
}

inline const js::Value &
CallObject::arg(uintN i) const
{
    JS_ASSERT(i < getCalleeFunction()->nargs);
    return getSlot(RESERVED_SLOTS + i);
}

inline void
CallObject::setArg(uintN i, const js::Value &v)
{
    JS_ASSERT(i < getCalleeFunction()->nargs);
    setSlot(RESERVED_SLOTS + i, v);
}

inline const js::Value &
CallObject::var(uintN i) const
{
    JSFunction *fun = getCalleeFunction();
    JS_ASSERT(fun->nargs == fun->script()->bindings.countArgs());
    JS_ASSERT(i < fun->script()->bindings.countVars());
    return getSlot(RESERVED_SLOTS + fun->nargs + i);
}

inline void
CallObject::setVar(uintN i, const js::Value &v)
{
    JSFunction *fun = getCalleeFunction();
    JS_ASSERT(fun->nargs == fun->script()->bindings.countArgs());
    JS_ASSERT(i < fun->script()->bindings.countVars());
    setSlot(RESERVED_SLOTS + fun->nargs + i, v);
}

inline void
CallObject::copyValues(uintN nargs, Value *argv, uintN nvars, Value *slots)
{
    JS_ASSERT(numSlots() >= RESERVED_SLOTS + nargs + nvars);
    copySlotRange(RESERVED_SLOTS, argv, nargs);
    copySlotRange(RESERVED_SLOTS + nargs, slots, nvars);
}

inline js::Value *
CallObject::argArray()
{
    js::DebugOnly<JSFunction*> fun = getCalleeFunction();
    JS_ASSERT(hasContiguousSlots(RESERVED_SLOTS, fun->nargs));
    return getSlotAddress(RESERVED_SLOTS);
}

inline js::Value *
CallObject::varArray()
{
    JSFunction *fun = getCalleeFunction();
    JS_ASSERT(hasContiguousSlots(RESERVED_SLOTS + fun->nargs,
                                 fun->script()->bindings.countVars()));
    return getSlotAddress(RESERVED_SLOTS + fun->nargs);
}

}

#endif /* CallObject_inl_h___ */
