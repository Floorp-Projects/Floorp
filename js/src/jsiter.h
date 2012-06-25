/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Convenience class for imitating a JS level for-of loop. Typical usage:
 *
 *     ForOfIterator it(cx, iterable);
 *     while (it.next()) {
 *        if (!DoStuff(cx, it.value()))
 *            return false;
 *     }
 *     if (!it.close())
 *         return false;
 *
 * The final it.close() check is needed in order to check for cases where
 * any of the iterator operations fail.
 *
 * it.close() may be skipped only if something in the body of the loop fails
 * and the failure is allowed to propagate on cx, as in this example if DoStuff
 * fails. In that case, ForOfIterator's destructor does all necessary cleanup.
 */
class ForOfIterator {
  private:
    JSContext *cx;
    RootedObject iterator;
    RootedValue currentValue;
    bool ok;
    bool closed;

    ForOfIterator(const ForOfIterator &) MOZ_DELETE;
    ForOfIterator &operator=(const ForOfIterator &) MOZ_DELETE;

  public:
    ForOfIterator(JSContext *cx, const Value &iterable)
        : cx(cx), iterator(cx, NULL), currentValue(cx), closed(false)
    {
        RootedValue iterv(cx, iterable);
        ok = ValueToIterator(cx, JSITER_FOR_OF, iterv.address());
        iterator = ok ? &iterv.reference().toObject() : NULL;
    }

    ~ForOfIterator() {
        if (!closed)
            close();
    }

    bool next() {
        JS_ASSERT(!closed);
        ok = ok && Next(cx, iterator, currentValue.address());
        return ok && !currentValue.reference().isMagic(JS_NO_ITER_VALUE);
    }

    Value &value() {
        JS_ASSERT(ok);
        JS_ASSERT(!closed);
        return currentValue.reference();
    }

    bool close() {
        JS_ASSERT(!closed);
        closed = true;
        if (!iterator)
            return false;
        bool throwing = cx->isExceptionPending();
        RootedValue exc(cx);
        if (throwing) {
            exc = cx->getPendingException();
            cx->clearPendingException();
        }
        bool closedOK = CloseIterator(cx, iterator);
        if (throwing && closedOK)
            cx->setPendingException(exc);
        return ok && !throwing && closedOK;
    }
};

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

namespace js {

bool
GeneratorHasMarkableFrame(JSGenerator *gen);

} /* namespace js */
#endif

extern JSObject *
js_InitIteratorClasses(JSContext *cx, JSObject *obj);

#endif /* jsiter_h___ */
