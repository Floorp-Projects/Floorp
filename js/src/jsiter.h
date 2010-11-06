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
#include "jscntxt.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsversion.h"

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

/*
 * For cacheable native iterators, whether the iterator is currently active.
 * Not serialized by XDR.
 */
#define JSITER_ACTIVE     0x1000

namespace js {

struct NativeIterator {
    JSObject  *obj;
    void      *props_array;
    void      *props_cursor;
    void      *props_end;
    uint32    *shapes_array;
    uint32    shapes_length;
    uint32    shapes_key;
    uint32    flags;
    JSObject  *next;  /* Forms cx->enumerators list, garbage otherwise. */

    bool isKeyIter() const { return (flags & JSITER_FOREACH) == 0; }

    inline jsid *beginKey() const {
        JS_ASSERT(isKeyIter());
        return (jsid *)props_array;
    }

    inline jsid *endKey() const {
        JS_ASSERT(isKeyIter());
        return (jsid *)props_end;
    }

    size_t numKeys() const {
        return endKey() - beginKey();
    }

    jsid *currentKey() const {
        JS_ASSERT(isKeyIter());
        return reinterpret_cast<jsid *>(props_cursor);
    }

    void incKeyCursor() {
        JS_ASSERT(isKeyIter());
        props_cursor = reinterpret_cast<jsid *>(props_cursor) + 1;
    }

    inline js::Value *beginValue() const {
        JS_ASSERT(!isKeyIter());
        return (js::Value *)props_array;
    }

    inline js::Value *endValue() const {
        JS_ASSERT(!isKeyIter());
        return (js::Value *)props_end;
    }

    size_t numValues() const {
        return endValue() - beginValue();
    }

    js::Value *currentValue() const {
        JS_ASSERT(!isKeyIter());
        return reinterpret_cast<js::Value *>(props_cursor);
    }

    void incValueCursor() {
        JS_ASSERT(!isKeyIter());
        props_cursor = reinterpret_cast<js::Value *>(props_cursor) + 1;
    }

    static NativeIterator *allocateKeyIterator(JSContext *cx, uint32 slength,
                                               const js::AutoIdVector &props);
    static NativeIterator *allocateValueIterator(JSContext *cx,
                                                 const js::AutoValueVector &props);
    void init(JSObject *obj, uintN flags, uint32 slength, uint32 key);

    void mark(JSTracer *trc);
};

bool
VectorToIdArray(JSContext *cx, js::AutoIdVector &props, JSIdArray **idap);

JS_FRIEND_API(bool)
GetPropertyNames(JSContext *cx, JSObject *obj, uintN flags, js::AutoIdVector *props);

bool
GetIterator(JSContext *cx, JSObject *obj, uintN flags, js::Value *vp);

bool
VectorToKeyIterator(JSContext *cx, JSObject *obj, uintN flags, js::AutoIdVector &props, js::Value *vp);

bool
VectorToValueIterator(JSContext *cx, JSObject *obj, uintN flags, js::AutoValueVector &props, js::Value *vp);

/*
 * Creates either a key or value iterator, depending on flags. For a value
 * iterator, performs value-lookup to convert the given list of jsids.
 */
bool
EnumeratedIdVectorToIterator(JSContext *cx, JSObject *obj, uintN flags, js::AutoIdVector &props, js::Value *vp);

}

/*
 * Convert the value stored in *vp to its iteration object. The flags should
 * contain JSITER_ENUMERATE if js_ValueToIterator is called when enumerating
 * for-in semantics are required, and when the caller can guarantee that the
 * iterator will never be exposed to scripts.
 */
extern JS_FRIEND_API(JSBool)
js_ValueToIterator(JSContext *cx, uintN flags, js::Value *vp);

extern JS_FRIEND_API(JSBool)
js_CloseIterator(JSContext *cx, JSObject *iterObj);

bool
js_SuppressDeletedProperty(JSContext *cx, JSObject *obj, jsid id);

bool
js_SuppressDeletedIndexProperties(JSContext *cx, JSObject *obj, jsint begin, jsint end);

/*
 * IteratorMore() indicates whether another value is available. It might
 * internally call iterobj.next() and then cache the value until its
 * picked up by IteratorNext(). The value is cached in the current context.
 */
extern JSBool
js_IteratorMore(JSContext *cx, JSObject *iterobj, js::Value *rval);

extern JSBool
js_IteratorNext(JSContext *cx, JSObject *iterobj, js::Value *rval);

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
    JSFrameRegs         regs;
    JSObject            *enumerators;
    JSStackFrame        *floating;
    js::Value           floatingStack[1];

    JSStackFrame *floatingFrame() {
        return floating;
    }

    JSStackFrame *liveFrame() {
        JS_ASSERT((state == JSGEN_RUNNING || state == JSGEN_CLOSING) ==
                  (regs.fp != floatingFrame()));
        return regs.fp;
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
    if (JS_UNLIKELY(fp->isGeneratorFrame()))
        return cx->generatorFor(fp)->floatingFrame();
    return fp;
}

/* Given a floating frame, given the JSGenerator containing it. */
extern JSGenerator *
js_FloatingFrameToGenerator(JSStackFrame *fp);

inline JSStackFrame *
js_LiveFrameIfGenerator(JSStackFrame *fp)
{
    return fp->isGeneratorFrame() ? js_FloatingFrameToGenerator(fp)->liveFrame() : fp;
}

#endif

extern js::Class js_GeneratorClass;
extern js::Class js_IteratorClass;
extern js::Class js_StopIterationClass;

static inline bool
js_ValueIsStopIteration(const js::Value &v)
{
    return v.isObject() && v.toObject().getClass() == &js_StopIterationClass;
}

extern JSObject *
js_InitIteratorClasses(JSContext *cx, JSObject *obj);

#endif /* jsiter_h___ */
