/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsiter_h___
#define jsiter_h___

/*
 * JavaScript iterators.
 */
#include "jscntxt.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsversion.h"

#include "gc/Barrier.h"
#include "vm/Stack.h"

/*
 * For cacheable native iterators, whether the iterator is currently active.
 * Not serialized by XDR.
 */
#define JSITER_ACTIVE       0x1000
#define JSITER_UNREUSABLE   0x2000

namespace js {

struct NativeIterator {
    HeapPtrObject obj;
    HeapPtr<JSFlatString> *props_array;
    HeapPtr<JSFlatString> *props_cursor;
    HeapPtr<JSFlatString> *props_end;
    const Shape **shapes_array;
    uint32_t  shapes_length;
    uint32_t  shapes_key;
    uint32_t  flags;
    JSObject  *next;  /* Forms cx->enumerators list, garbage otherwise. */

    bool isKeyIter() const { return (flags & JSITER_FOREACH) == 0; }

    inline HeapPtr<JSFlatString> *begin() const {
        return props_array;
    }

    inline HeapPtr<JSFlatString> *end() const {
        return props_end;
    }

    size_t numKeys() const {
        return end() - begin();
    }

    HeapPtr<JSFlatString> *current() const {
        JS_ASSERT(props_cursor < props_end);
        return props_cursor;
    }

    void incCursor() {
        props_cursor = props_cursor + 1;
    }

    static NativeIterator *allocateIterator(JSContext *cx, uint32_t slength,
                                            const js::AutoIdVector &props);
    void init(JSObject *obj, unsigned flags, uint32_t slength, uint32_t key);

    void mark(JSTracer *trc);
};

class ElementIteratorObject : public JSObject {
  public:
    enum {
        TargetSlot,
        IndexSlot,
        NumSlots
    };

    static JSObject *create(JSContext *cx, HandleObject target);

    inline uint32_t getIndex() const;
    inline void setIndex(uint32_t index);
    inline JSObject *getTargetObject() const;

    /*
        Array iterators are like this:

        Array.prototype[iterate] = function () {
            for (var i = 0; i < (this.length >>> 0); i++) {
                var desc = Object.getOwnPropertyDescriptor(this, i);
                yield desc === undefined ? undefined : this[i];
            }
        }

        This has the following implications:

          - Array iterators are generic; Array.prototype[iterate] can be transferred to
            any other object to create iterators over it.

          - The next() method of an Array iterator is non-reentrant. Trying to reenter,
            e.g. by using it on an object with a length getter that calls it.next() on
            the same iterator, causes a TypeError.

          - The iterator fetches obj.length every time its next() method is called.

          - The iterator converts obj.length to a whole number using ToUint32. As a
            consequence the iterator can't go on forever; it can yield at most 2^32-1
            values. Then i will be 0xffffffff, and no possible length value will be
            greater than that.

          - The iterator does not skip "array holes". When it encounters a hole, it
            yields undefined.

          - The iterator never consults the prototype chain.

          - If an element has a getter which throws, the exception is propagated, and
            the iterator is closed (that is, all future calls to next() will simply
            throw StopIteration).

        Note that if next() were reentrant, even more details of its inner
        workings would be observable.
    */

    /*
     * If there are any more elements to visit, store the value of the next
     * element in *vp, increment the index, and return true. If not, call
     * vp->setMagic(JS_NO_ITER_VALUE) and return true. Return false on error.
     */
    bool iteratorNext(JSContext *cx, Value *vp);
};

bool
VectorToIdArray(JSContext *cx, AutoIdVector &props, JSIdArray **idap);

bool
GetIterator(JSContext *cx, HandleObject obj, unsigned flags, Value *vp);

bool
VectorToKeyIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &props, Value *vp);

bool
VectorToValueIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &props, Value *vp);

/*
 * Creates either a key or value iterator, depending on flags. For a value
 * iterator, performs value-lookup to convert the given list of jsids.
 */
bool
EnumeratedIdVectorToIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &props, Value *vp);

/*
 * Convert the value stored in *vp to its iteration object. The flags should
 * contain JSITER_ENUMERATE if js::ValueToIterator is called when enumerating
 * for-in semantics are required, and when the caller can guarantee that the
 * iterator will never be exposed to scripts.
 */
extern JSBool
ValueToIterator(JSContext *cx, unsigned flags, Value *vp);

extern bool
CloseIterator(JSContext *cx, JSObject *iterObj);

extern bool
UnwindIteratorForException(JSContext *cx, JSObject *obj);

extern void
UnwindIteratorForUncatchableException(JSContext *cx, JSObject *obj);

}

extern bool
js_SuppressDeletedProperty(JSContext *cx, js::HandleObject obj, jsid id);

extern bool
js_SuppressDeletedElement(JSContext *cx, js::HandleObject obj, uint32_t index);

extern bool
js_SuppressDeletedElements(JSContext *cx, js::HandleObject obj, uint32_t begin, uint32_t end);

/*
 * IteratorMore() indicates whether another value is available. It might
 * internally call iterobj.next() and then cache the value until its
 * picked up by IteratorNext(). The value is cached in the current context.
 */
extern JSBool
js_IteratorMore(JSContext *cx, js::HandleObject iterobj, js::Value *rval);

extern JSBool
js_IteratorNext(JSContext *cx, JSObject *iterobj, js::Value *rval);

extern JSBool
js_ThrowStopIteration(JSContext *cx);

namespace js {

/*
 * Get the next value from an iterator object.
 *
 * On success, store the next value in *vp and return true; if there are no
 * more values, store the magic value JS_NO_ITER_VALUE in *vp and return true.
 */
inline bool
Next(JSContext *cx, HandleObject iter, Value *vp)
{
    if (!js_IteratorMore(cx, iter, vp))
        return false;
    if (vp->toBoolean())
        return js_IteratorNext(cx, iter, vp);
    vp->setMagic(JS_NO_ITER_VALUE);
    return true;
}

/*
 * Imitate a for-of loop. This does the equivalent of the JS code:
 *
 *     for (let v of iterable)
 *         op(v);
 *
 * But the actual signature of op must be:
 *     bool op(JSContext *cx, const Value &v);
 *
 * There is no feature like JS 'break'. op must return false only
 * in case of exception or error.
 */
template <class Op>
bool
ForOf(JSContext *cx, const Value &iterable, Op op)
{
    Value iterv(iterable);
    if (!ValueToIterator(cx, JSITER_FOR_OF, &iterv))
        return false;
    RootedObject iter(cx, &iterv.toObject());

    bool ok = true;
    while (ok) {
        Value v;
        ok = Next(cx, iter, &v);
        if (ok) {
            if (v.isMagic(JS_NO_ITER_VALUE))
                break;
            ok = op(cx, v);
        }
    }

    bool throwing = !ok && cx->isExceptionPending();
    Value exc;
    if (throwing) {
        exc = cx->getPendingException();
        cx->clearPendingException();
    }
    bool closedOK = CloseIterator(cx, iter);
    if (throwing && closedOK)
        cx->setPendingException(exc);
    return ok && closedOK;
}

} /* namespace js */

#if JS_HAS_GENERATORS

/*
 * Generator state codes.
 */
enum JSGeneratorState
{
    JSGEN_NEWBORN,  /* not yet started */
    JSGEN_OPEN,     /* started by a .next() or .send(undefined) call */
    JSGEN_RUNNING,  /* currently executing via .next(), etc., call */
    JSGEN_CLOSING,  /* close method is doing asynchronous return */
    JSGEN_CLOSED    /* closed, cannot be started or closed again */
};

struct JSGenerator
{
    js::HeapPtrObject   obj;
    JSGeneratorState    state;
    js::FrameRegs       regs;
    JSObject            *enumerators;
    JSGenerator         *prevGenerator;
    js::StackFrame      *fp;
    js::HeapValue       stackSnapshot[1];
};

extern JSObject *
js_NewGenerator(JSContext *cx);
#endif

extern JSObject *
js_InitIteratorClasses(JSContext *cx, JSObject *obj);

#endif /* jsiter_h___ */
