/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
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

#ifndef jslogic_h_inl__
#define jslogic_h_inl__

namespace js {
namespace mjit {

static inline void
ThrowException(VMFrame &f)
{
    void *ptr = JS_FUNC_TO_DATA_PTR(void *, JaegerThrowpoline);
    *f.returnAddressLocation() = ptr;
}

#define THROW()   do { ThrowException(f); return; } while (0)
#define THROWV(v) do { ThrowException(f); return v; } while (0)

static inline JSObject *
ValueToObject(JSContext *cx, Value *vp)
{
    if (vp->isObject())
        return &vp->toObject();
    return js_ValueToNonNullObject(cx, *vp);
}

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
            (shape)->slot != SHAPE_INVALID_SLOT &&                            \
            !(obj)->brandedOrHasMethodBarrier()) {                            \
            /* Fast path for, e.g., plain Object instance properties. */      \
            (obj)->nativeSetSlot((shape)->slot, *vp);                         \
        } else {                                                              \
            if (!js_NativeSet(cx, obj, shape, false, strict, vp))             \
                THROW();                                                      \
        }                                                                     \
    JS_END_MACRO

#define NATIVE_GET(cx,obj,pobj,shape,getHow,vp,onerr)                         \
    JS_BEGIN_MACRO                                                            \
        if (shape->isDataDescriptor() && shape->hasDefaultGetter()) {         \
            /* Fast path for Object instance properties. */                   \
            JS_ASSERT((shape)->slot != SHAPE_INVALID_SLOT ||                  \
                      !shape->hasDefaultSetter());                            \
            if (((shape)->slot != SHAPE_INVALID_SLOT))                        \
                *(vp) = (pobj)->nativeGetSlot((shape)->slot);                 \
            else                                                              \
                (vp)->setUndefined();                                         \
        } else {                                                              \
            if (!js_NativeGet(cx, obj, pobj, shape, getHow, vp))              \
                onerr;                                                        \
        }                                                                     \
    JS_END_MACRO

}}

#endif /* jslogic_h__ */

