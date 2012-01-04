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
 * The Original Code is SpiderMonkey global object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jsgcmark.h"

#include "gc/Barrier.h"

#ifndef jsgc_barrier_inl_h___
#define jsgc_barrier_inl_h___

namespace js {

static JS_ALWAYS_INLINE void
ClearValueRange(JSCompartment *comp, HeapValue *vec, uintN len, bool useHoles)
{
    if (useHoles) {
        for (uintN i = 0; i < len; i++)
            vec[i].set(comp, MagicValue(JS_ARRAY_HOLE));
    } else {
        for (uintN i = 0; i < len; i++)
            vec[i].set(comp, UndefinedValue());
    }
}

static JS_ALWAYS_INLINE void
InitValueRange(HeapValue *vec, uintN len, bool useHoles)
{
    if (useHoles) {
        for (uintN i = 0; i < len; i++)
            vec[i].init(MagicValue(JS_ARRAY_HOLE));
    } else {
        for (uintN i = 0; i < len; i++)
            vec[i].init(UndefinedValue());
    }
}

static JS_ALWAYS_INLINE void
DestroyValueRange(HeapValue *vec, uintN len)
{
    for (uintN i = 0; i < len; i++)
        vec[i].~HeapValue();
}

inline
HeapValue::HeapValue(const Value &v)
    : value(v)
{
    JS_ASSERT(!IsPoisonedValue(v));
    post();
}

inline
HeapValue::HeapValue(const HeapValue &v)
    : value(v.value)
{
    JS_ASSERT(!IsPoisonedValue(v.value));
    post();
}

inline
HeapValue::~HeapValue()
{
    pre();
}

inline void
HeapValue::init(const Value &v)
{
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post();
}

inline void
HeapValue::init(JSCompartment *comp, const Value &v)
{
    value = v;
    post(comp);
}

inline void
HeapValue::writeBarrierPre(const Value &value)
{
#ifdef JSGC_INCREMENTAL
    if (value.isMarkable()) {
        js::gc::Cell *cell = (js::gc::Cell *)value.toGCThing();
        writeBarrierPre(cell->compartment(), value);
    }
#endif
}

inline void
HeapValue::writeBarrierPost(const Value &value, void *addr)
{
}

inline void
HeapValue::writeBarrierPre(JSCompartment *comp, const Value &value)
{
#ifdef JSGC_INCREMENTAL
    if (comp->needsBarrier())
        js::gc::MarkValueUnbarriered(comp->barrierTracer(), value, "write barrier");
#endif
}

inline void
HeapValue::writeBarrierPost(JSCompartment *comp, const Value &value, void *addr)
{
}

inline void
HeapValue::pre()
{
    writeBarrierPre(value);
}

inline void
HeapValue::post()
{
}

inline void
HeapValue::pre(JSCompartment *comp)
{
    writeBarrierPre(comp, value);
}

inline void
HeapValue::post(JSCompartment *comp)
{
}

inline HeapValue &
HeapValue::operator=(const Value &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post();
    return *this;
}

inline HeapValue &
HeapValue::operator=(const HeapValue &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v.value));
    value = v.value;
    post();
    return *this;
}

inline void
HeapValue::set(JSCompartment *comp, const Value &v)
{
#ifdef DEBUG
    if (value.isMarkable()) {
        js::gc::Cell *cell = (js::gc::Cell *)value.toGCThing();
        JS_ASSERT(cell->compartment() == comp ||
                  cell->compartment() == comp->rt->atomsCompartment);
    }
#endif

    pre(comp);
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post(comp);
}

inline
HeapId::HeapId(jsid id)
    : value(id)
{
    JS_ASSERT(!IsPoisonedId(id));
    post();
}

inline
HeapId::~HeapId()
{
    pre();
}

inline void
HeapId::init(jsid id)
{
    JS_ASSERT(!IsPoisonedId(id));
    value = id;
    post();
}

inline void
HeapId::pre()
{
#ifdef JSGC_INCREMENTAL
    if (JS_UNLIKELY(JSID_IS_OBJECT(value))) {
        JSObject *obj = JSID_TO_OBJECT(value);
        JSCompartment *comp = obj->compartment();
        if (comp->needsBarrier())
            js::gc::MarkObjectUnbarriered(comp->barrierTracer(), obj, "write barrier");
    }
#endif
}

inline void
HeapId::post()
{
}

inline HeapId &
HeapId::operator=(jsid id)
{
    pre();
    JS_ASSERT(!IsPoisonedId(id));
    value = id;
    post();
    return *this;
}

inline HeapId &
HeapId::operator=(const HeapId &v)
{
    pre();
    JS_ASSERT(!IsPoisonedId(v.value));
    value = v.value;
    post();
    return *this;
}

} /* namespace js */

#endif /* jsgc_barrier_inl_h___ */
