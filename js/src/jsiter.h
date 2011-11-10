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
    HeapPtrObject  obj;
    HeapId    *props_array;
    HeapId    *props_cursor;
    HeapId    *props_end;
    uint32    *shapes_array;
    uint32    shapes_length;
    uint32    shapes_key;
    uint32    flags;
    JSObject  *next;  /* Forms cx->enumerators list, garbage otherwise. */

    bool isKeyIter() const { return (flags & JSITER_FOREACH) == 0; }

    inline HeapId *begin() const {
        return props_array;
    }

    inline HeapId *end() const {
        return props_end;
    }

    size_t numKeys() const {
        return end() - begin();
    }

    HeapId *current() const {
        JS_ASSERT(props_cursor < props_end);
        return props_cursor;
    }

    void incCursor() {
        props_cursor = props_cursor + 1;
    }

    static NativeIterator *allocateIterator(JSContext *cx, uint32 slength,
                                            const js::AutoIdVector &props);
    void init(JSObject *obj, uintN flags, uint32 slength, uint32 key);

    void mark(JSTracer *trc);
};

bool
VectorToIdArray(JSContext *cx, js::AutoIdVector &props, JSIdArray **idap);

bool
GetIterator(JSContext *cx, JSObject *obj, uintN flags, js::Value *vp);

bool
VectorToKeyIterator(JSContext *cx, JSObject *obj, uintN flags, js::AutoIdVector &props, js::Value *vp);

bool
VectorToValueIterator(JSContext *cx, JSObject *obj, uintN flags, js::AutoIdVector &props, js::Value *vp);

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
js_SuppressDeletedElement(JSContext *cx, JSObject *obj, uint32 index);

bool
js_SuppressDeletedElements(JSContext *cx, JSObject *obj, uint32 begin, uint32 end);

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
    js::HeapPtrObject   obj;
    JSGeneratorState    state;
    js::FrameRegs       regs;
    JSObject            *enumerators;
    js::StackFrame      *floating;
    js::Value           floatingStack[1];

    js::StackFrame *floatingFrame() {
        return floating;
    }

    js::StackFrame *liveFrame() {
        JS_ASSERT((state == JSGEN_RUNNING || state == JSGEN_CLOSING) ==
                  (regs.fp() != floatingFrame()));
        return regs.fp();
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
inline js::StackFrame *
js_FloatingFrameIfGenerator(JSContext *cx, js::StackFrame *fp)
{
    if (JS_UNLIKELY(fp->isGeneratorFrame()))
        return cx->generatorFor(fp)->floatingFrame();
    return fp;
}

/* Given a floating frame, given the JSGenerator containing it. */
extern JSGenerator *
js_FloatingFrameToGenerator(js::StackFrame *fp);

inline js::StackFrame *
js_LiveFrameIfGenerator(js::StackFrame *fp)
{
    return fp->isGeneratorFrame() ? js_FloatingFrameToGenerator(fp)->liveFrame() : fp;
}

#endif

namespace js {

static inline bool
IsStopIteration(const js::Value &v)
{
    return v.isObject() && v.toObject().isStopIteration();
}

}  /* namespace js */

extern JSObject *
js_InitIteratorClasses(JSContext *cx, JSObject *obj);

#endif /* jsiter_h___ */
