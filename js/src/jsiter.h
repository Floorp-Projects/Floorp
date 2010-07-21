/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 *
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef jsiter_h___
#define jsiter_h___

/*
 * JavaScript iterators.
 */
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsversion.h"

JS_BEGIN_EXTERN_C

/*
 * NB: these flag bits are encoded into the bytecode stream in the immediate
 * operand of JSOP_ITER, so don't change them without advancing jsxdrapi.h's
 * JSXDR_BYTECODE_VERSION.
 */
#define JSITER_ENUMERATE  0x1   /* for-in compatible hidden default iterator */
#define JSITER_FOREACH    0x2   /* return [key, value] pair rather than key */
#define JSITER_KEYVALUE   0x4   /* destructuring for-in wants [key, value] */
#define JSITER_OWNONLY    0x8   /* iterate over obj's own properties only */
#define JSITER_HIDDEN     0x10  /* also enumerate non-enumerable properties */

struct NativeIterator {
    JSObject  *obj;
    jsval     *props_array;
    jsval     *props_cursor;
    jsval     *props_end;
    uint32    *shapes_array;
    uint32    shapes_length;
    uint32    shapes_key;
    uintN     flags;
    JSObject  *next;

    static NativeIterator *allocate(JSContext *cx, JSObject *obj, uintN flags,
                                    uint32 *sarray, uint32 slength, uint32 key,
                                    js::AutoValueVector &props);

    inline size_t length() const {
        return props_end - props_array;
    }

    inline jsval *begin() const {
        return props_array;
    }

    void mark(JSTracer *trc);
};

/*
 * Magic jsval that indicates that a custom enumerate hook forwarded
 * to js_Enumerate, which really means the object can be enumerated like
 * a native object.
 */
static const jsval JSVAL_NATIVE_ENUMERATE_COOKIE = SPECIAL_TO_JSVAL(0x220576);

bool
VectorToIdArray(JSContext *cx, js::AutoValueVector &props, JSIdArray **idap);

bool
GetPropertyNames(JSContext *cx, JSObject *obj, uintN flags, js::AutoValueVector &props);

bool
GetIterator(JSContext *cx, JSObject *obj, uintN flags, jsval *vp);

bool
IdVectorToIterator(JSContext *cx, JSObject *obj, uintN flags, js::AutoValueVector &props, jsval *vp);

/*
 * Convert the value stored in *vp to its iteration object. The flags should
 * contain JSITER_ENUMERATE if js_ValueToIterator is called when enumerating
 * for-in semantics are required, and when the caller can guarantee that the
 * iterator will never be exposed to scripts.
 */
extern JS_FRIEND_API(JSBool)
js_ValueToIterator(JSContext *cx, uintN flags, jsval *vp);

extern JS_FRIEND_API(JSBool)
js_CloseIterator(JSContext *cx, jsval v);

bool
js_SuppressDeletedProperty(JSContext *cx, JSObject *obj, jsid id);

/*
 * IteratorMore() indicates whether another value is available. It might
 * internally call iterobj.next() and then cache the value until its
 * picked up by IteratorNext(). The value is cached in the current context.
 */
extern JSBool
js_IteratorMore(JSContext *cx, JSObject *iterobj, jsval *rval);

extern JSBool
js_IteratorNext(JSContext *cx, JSObject *iterobj, jsval *rval);

extern JSBool
js_ThrowStopIteration(JSContext *cx);

#if JS_HAS_GENERATORS

/*
 * Generator state codes.
 */
typedef enum JSGeneratorState {
    JSGEN_NEWBORN,  /* not yet started */
    JSGEN_OPEN,     /* started by a .next() or .send(undefined) call */
    JSGEN_RUNNING,  /* currently executing via .next(), etc., call */
    JSGEN_CLOSING,  /* close method is doing asynchronous return */
    JSGEN_CLOSED    /* closed, cannot be started or closed again */
} JSGeneratorState;

struct JSGenerator {
    JSObject            *obj;
    JSGeneratorState    state;
    JSFrameRegs         savedRegs;
    uintN               vplen;
    JSStackFrame        *liveFrame;
    JSObject            *enumerators;
    jsval               floatingStack[1];

    JSStackFrame *getFloatingFrame() {
        return reinterpret_cast<JSStackFrame *>(floatingStack + vplen);
    }

    JSStackFrame *getLiveFrame() {
        JS_ASSERT((state == JSGEN_RUNNING || state == JSGEN_CLOSING) ==
                  (liveFrame != getFloatingFrame()));
        return liveFrame;
    }
};

extern JSObject *
js_NewGenerator(JSContext *cx);

/*
 * Generator stack frames do not have stable pointers since they get copied to
 * and from the generator object and the stack (see SendToGenerator). This is a
 * problem for Block and With objects, which need to store a pointer to the
 * enclosing stack frame. The solution is for Block and With objects to store
 * a pointer to the "floating" stack frame stored in the generator object,
 * since it is stable, and maintain, in the generator object, a pointer to the
 * "live" stack frame (either a copy on the stack or the floating frame). Thus,
 * Block and With objects must "normalize" to and from the floating/live frames
 * in the case of generators using the following functions.
 */
inline JSStackFrame *
js_FloatingFrameIfGenerator(JSContext *cx, JSStackFrame *fp)
{
    JS_ASSERT(cx->stack().contains(fp));
    if (JS_UNLIKELY(fp->isGenerator()))
        return cx->generatorFor(fp)->getFloatingFrame();
    return fp;
}

/* Given a floating frame, given the JSGenerator containing it. */
extern JSGenerator *
js_FloatingFrameToGenerator(JSStackFrame *fp);

inline JSStackFrame *
js_LiveFrameIfGenerator(JSStackFrame *fp)
{
    if (fp->flags & JSFRAME_GENERATOR)
        return js_FloatingFrameToGenerator(fp)->getLiveFrame();
    return fp;
}

#endif

extern JSExtendedClass js_GeneratorClass;
extern JSExtendedClass js_IteratorClass;
extern JSClass         js_StopIterationClass;

static inline bool
js_ValueIsStopIteration(jsval v)
{
    return !JSVAL_IS_PRIMITIVE(v) &&
           JSVAL_TO_OBJECT(v)->getClass() == &js_StopIterationClass;
}

extern JSObject *
js_InitIteratorClasses(JSContext *cx, JSObject *obj);

JS_END_EXTERN_C

#endif /* jsiter_h___ */
