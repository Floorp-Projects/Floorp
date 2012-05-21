/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jslogic_h_inl__ && defined JS_METHODJIT
#define jslogic_h_inl__

namespace js {
namespace mjit {

static inline void
ThrowException(VMFrame &f)
{
    void *ptr = JS_FUNC_TO_DATA_PTR(void *, JaegerThrowpoline);
    *f.returnAddressLocation() = ptr;
}

#define THROW()   do { mjit::ThrowException(f); return; } while (0)
#define THROWV(v) do { mjit::ThrowException(f); return v; } while (0)

static inline void
ReportAtomNotDefined(JSContext *cx, JSAtom *atom)
{
    JSAutoByteString printable;
    if (js_AtomToPrintableString(cx, atom, &printable))
        js_ReportIsNotDefined(cx, printable.ptr());
}

#define NATIVE_SET(cx,obj,shape,entry,strict,vp)                              \
    JS_BEGIN_MACRO                                                            \
        if (shape->hasDefaultSetter() &&                                      \
            (shape)->hasSlot() &&                                             \
            !(shape)->isMethod()) {                                           \
            /* Fast path for, e.g., plain Object instance properties. */      \
            (obj)->nativeSetSlotWithType(cx, shape, *vp);                     \
        } else {                                                              \
            if (!js_NativeSet(cx, obj, shape, false, strict, vp))             \
                THROW();                                                      \
        }                                                                     \
    JS_END_MACRO

#define NATIVE_GET(cx,obj,pobj,shape,getHow,vp,onerr)                         \
    JS_BEGIN_MACRO                                                            \
        if (shape->isDataDescriptor() && shape->hasDefaultGetter()) {         \
            /* Fast path for Object instance properties. */                   \
            JS_ASSERT((shape)->slot() != SHAPE_INVALID_SLOT ||                \
                      !shape->hasDefaultSetter());                            \
            if (((shape)->slot() != SHAPE_INVALID_SLOT))                      \
                *(vp) = (pobj)->nativeGetSlot((shape)->slot());               \
            else                                                              \
                (vp)->setUndefined();                                         \
        } else {                                                              \
            if (!js_NativeGet(cx, obj, pobj, shape, getHow, vp))              \
                onerr;                                                        \
        }                                                                     \
    JS_END_MACRO

}}

#endif /* jslogic_h__ */

