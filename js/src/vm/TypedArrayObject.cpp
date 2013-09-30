/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TypedArrayObject.h"

#include "mozilla/Alignment.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"

#include <string.h>
#ifndef XP_WIN
# include <sys/mman.h>
#endif

#include "jsapi.h"
#include "jsarray.h"
#include "jscntxt.h"
#include "jscpucfg.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jstypes.h"
#include "jsutil.h"
#ifdef XP_WIN
# include "jswin.h"
#endif
#include "jswrapper.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "jit/AsmJS.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/NumericConversions.h"
#include "vm/WrapperObject.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/Shape-inl.h"

#if JS_USE_NEW_OBJECT_REPRESENTATION
// See the comment above OldObjectRepresentationHack.
#  error "TypedArray support for new object representation unimplemented."
#endif

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::IsNaN;
using mozilla::PodCopy;
using JS::GenericNaN;

/*
 * Allocate array buffers with the maximum number of fixed slots marked as
 * reserved, so that the fixed slots may be used for the buffer's contents.
 * The last fixed slot is kept for the object's private data.
 */
static const uint8_t ARRAYBUFFER_RESERVED_SLOTS = JSObject::MAX_FIXED_SLOTS - 1;

// Sentinel value used to initialize ArrayBufferViewObjects' NEXT_BUFFER_SLOTs
// to show that they have not yet been added to any ArrayBufferObject list.
js::ArrayBufferObject * const UNSET_BUFFER_LINK = reinterpret_cast<js::ArrayBufferObject*>(0x2);

static bool
ValueIsLength(const Value &v, uint32_t *len)
{
    if (v.isInt32()) {
        int32_t i = v.toInt32();
        if (i < 0)
            return false;
        *len = i;
        return true;
    }

    if (v.isDouble()) {
        double d = v.toDouble();
        if (IsNaN(d))
            return false;

        uint32_t length = uint32_t(d);
        if (d != double(length))
            return false;

        *len = length;
        return true;
    }

    return false;
}

/*
 * Convert |v| to an array index for an array of length |length| per
 * the Typed Array Specification section 7.0, |subarray|. If successful,
 * the output value is in the range [0, length].
 */
static bool
ToClampedIndex(JSContext *cx, HandleValue v, uint32_t length, uint32_t *out)
{
    int32_t result;
    if (!ToInt32(cx, v, &result))
        return false;
    if (result < 0) {
        result += length;
        if (result < 0)
            result = 0;
    } else if (uint32_t(result) > length) {
        result = length;
    }
    *out = uint32_t(result);
    return true;
}

/*
 * ArrayBufferObject
 *
 * This class holds the underlying raw buffer that the TypedArrayObject classes
 * access.  It can be created explicitly and passed to a TypedArrayObject, or
 * can be created implicitly by constructing a TypedArrayObject with a size.
 */

JS_ALWAYS_INLINE bool
IsArrayBuffer(HandleValue v)
{
    return v.isObject() && v.toObject().hasClass(&ArrayBufferObject::class_);
}

JS_ALWAYS_INLINE bool
ArrayBufferObject::byteLengthGetterImpl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsArrayBuffer(args.thisv()));
    args.rval().setInt32(args.thisv().toObject().as<ArrayBufferObject>().byteLength());
    return true;
}

bool
ArrayBufferObject::byteLengthGetter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsArrayBuffer, byteLengthGetterImpl>(cx, args);
}

bool
ArrayBufferObject::fun_slice_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsArrayBuffer(args.thisv()));

    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());

    // these are the default values
    uint32_t length = thisObj->as<ArrayBufferObject>().byteLength();
    uint32_t begin = 0, end = length;

    if (args.length() > 0) {
        if (!ToClampedIndex(cx, args[0], length, &begin))
            return false;

        if (args.length() > 1) {
            if (!ToClampedIndex(cx, args[1], length, &end))
                return false;
        }
    }

    if (begin > end)
        begin = end;

    JSObject *nobj = createSlice(cx, thisObj->as<ArrayBufferObject>(), begin, end);
    if (!nobj)
        return false;
    args.rval().setObject(*nobj);
    return true;
}

bool
ArrayBufferObject::fun_slice(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsArrayBuffer, fun_slice_impl>(cx, args);
}

/*
 * new ArrayBuffer(byteLength)
 */
bool
ArrayBufferObject::class_constructor(JSContext *cx, unsigned argc, Value *vp)
{
    int32_t nbytes = 0;
    CallArgs args = CallArgsFromVp(argc, vp);
    if (argc > 0 && !ToInt32(cx, args[0], &nbytes))
        return false;

    if (nbytes < 0) {
        /*
         * We're just not going to support arrays that are bigger than what will fit
         * as an integer value; if someone actually ever complains (validly), then we
         * can fix.
         */
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_ARRAY_LENGTH);
        return false;
    }

    JSObject *bufobj = create(cx, uint32_t(nbytes));
    if (!bufobj)
        return false;
    args.rval().setObject(*bufobj);
    return true;
}

/*
 * Note that some callers are allowed to pass in a nullptr cx, so we allocate
 * with the cx if available and fall back to the runtime.  If oldptr is given,
 * it's expected to be a previously-allocated ObjectElements* pointer that we
 * then realloc.
 */
static ObjectElements *
AllocateArrayBufferContents(JSContext *maybecx, uint32_t nbytes, uint8_t *initdata, void *oldptr = nullptr)
{
    uint32_t size = nbytes + sizeof(ObjectElements);
    ObjectElements *newheader;

    // if oldptr is given, then we need to do a realloc
    if (oldptr) {
        ObjectElements *oldheader = static_cast<ObjectElements *>(oldptr);
        uint32_t oldnbytes = ArrayBufferObject::headerInitializedLength(oldheader);
        newheader = static_cast<ObjectElements *>(maybecx ? maybecx->realloc_(oldptr, size) : js_realloc(oldptr, size));

        // if we grew the array, we need to set the new bytes to 0
        if (newheader && nbytes > oldnbytes)
            memset(reinterpret_cast<uint8_t*>(newheader->elements()) + oldnbytes, 0, nbytes - oldnbytes);
    } else {
        newheader = static_cast<ObjectElements *>(maybecx ? maybecx->calloc_(size) : js_calloc(size));
    }
    if (!newheader) {
        if (maybecx)
            js_ReportOutOfMemory(maybecx);
        return nullptr;
    }

    if (initdata)
        memcpy(newheader->elements(), initdata, nbytes);

    // we rely on this being correct
    ArrayBufferObject::setElementsHeader(newheader, nbytes);

    return newheader;
}

bool
ArrayBufferObject::allocateSlots(JSContext *maybecx, uint32_t bytes, uint8_t *contents)
{
    /*
     * ArrayBufferObjects delegate added properties to another JSObject, so
     * their internal layout can use the object's fixed slots for storage.
     * Set up the object to look like an array with an elements header.
     */
    JS_ASSERT(!hasDynamicSlots() && !hasDynamicElements());

    size_t usableSlots = ARRAYBUFFER_RESERVED_SLOTS - ObjectElements::VALUES_PER_HEADER;

    if (bytes > sizeof(Value) * usableSlots) {
        ObjectElements *header = AllocateArrayBufferContents(maybecx, bytes, contents);
        if (!header)
            return false;
        elements = header->elements();
    } else {
        elements = fixedElements();
        if (contents)
            memcpy(elements, contents, bytes);
        else
            memset(elements, 0, bytes);
    }

    setElementsHeader(getElementsHeader(), bytes);

    return true;
}

static inline void
PostBarrierTypedArrayObject(JSObject *obj)
{
#ifdef JSGC_GENERATIONAL
    JS_ASSERT(obj);
    JSRuntime *rt = obj->runtimeFromMainThread();
    if (!rt->isHeapBusy() && !IsInsideNursery(rt, obj))
        rt->gcStoreBuffer.putWholeCell(obj);
#endif
}

// The list of views must be stored somewhere in the ArrayBufferObject, but
// the slots are already being used for the element storage and the private
// field is used for a delegate object. The ObjectElements header has space
// for it, but I don't want to mess around with adding unions to it with
// JS_USE_NEW_OBJECT_REPRESENTATION pending, since it will solve this much
// more cleanly.
struct OldObjectRepresentationHack {
    uint32_t capacity;
    uint32_t initializedLength;
    EncapsulatedPtr<ArrayBufferViewObject> views;
};

static ArrayBufferViewObject *
GetViewList(ArrayBufferObject *obj)
{
    return reinterpret_cast<OldObjectRepresentationHack*>(obj->getElementsHeader())->views;
}

static void
SetViewList(ArrayBufferObject *obj, ArrayBufferViewObject *viewsHead)
{
    reinterpret_cast<OldObjectRepresentationHack*>(obj->getElementsHeader())->views = viewsHead;
    PostBarrierTypedArrayObject(obj);
}

static void
InitViewList(ArrayBufferObject *obj, ArrayBufferViewObject *viewsHead)
{
    reinterpret_cast<OldObjectRepresentationHack*>(obj->getElementsHeader())->views.init(viewsHead);
    PostBarrierTypedArrayObject(obj);
}

static EncapsulatedPtr<ArrayBufferViewObject> &
GetViewListRef(ArrayBufferObject *obj)
{
    JS_ASSERT(obj->runtimeFromMainThread()->isHeapBusy());
    return reinterpret_cast<OldObjectRepresentationHack*>(obj->getElementsHeader())->views;
}

void
ArrayBufferObject::changeContents(JSContext *maybecx, ObjectElements *newHeader)
{
   // Grab out data before invalidating it.
   uint32_t byteLengthCopy = byteLength();
   uintptr_t oldDataPointer = uintptr_t(dataPointer());
   ArrayBufferViewObject *viewListHead = GetViewList(this);

   // Update all views.
   uintptr_t newDataPointer = uintptr_t(newHeader->elements());
   for (ArrayBufferViewObject *view = viewListHead; view; view = view->nextView()) {
       uintptr_t newDataPtr = uintptr_t(view->getPrivate()) - oldDataPointer + newDataPointer;
       view->setPrivate(reinterpret_cast<uint8_t*>(newDataPtr));

       // Notify compiled jit code that the base pointer has moved.
       if (maybecx)
           MarkObjectStateChange(maybecx, view);
   }

   // Change to the new header (now, so we can use SetViewList).
   elements = newHeader->elements();

   // Initialize 'newHeader'.
   ArrayBufferObject::setElementsHeader(newHeader, byteLengthCopy);
   SetViewList(this, viewListHead);
}

bool
ArrayBufferObject::uninlineData(JSContext *maybecx)
{
   if (hasDynamicElements())
       return true;

   ObjectElements *newHeader = AllocateArrayBufferContents(maybecx, byteLength(), dataPointer());
   if (!newHeader)
       return false;

   changeContents(maybecx, newHeader);
   return true;
}

#if defined(JS_ION) && defined(JS_CPU_X64)
// To avoid dynamically checking bounds on each load/store, asm.js code relies
// on the SIGSEGV handler in AsmJSSignalHandlers.cpp. However, this only works
// if we can guarantee that *any* out-of-bounds access generates a fault. This
// isn't generally true since an out-of-bounds access could land on other
// Mozilla data. To overcome this on x64, we reserve an entire 4GB space,
// making only the range [0, byteLength) accessible, and use a 32-bit unsigned
// index into this space. (x86 and ARM require different tricks.)
//
// One complication is that we need to put an ObjectElements struct immediately
// before the data array (as required by the general JSObject data structure).
// Thus, we must stick a page before the elements to hold ObjectElements.
//
//   |<------------------------------ 4GB + 1 pages --------------------->|
//           |<--- sizeof --->|<------------------- 4GB ----------------->|
//
//   | waste | ObjectElements | data array | inaccessible reserved memory |
//                            ^            ^                              ^
//                            |            \                             /
//                      obj->elements       required to be page boundaries
//
JS_STATIC_ASSERT(sizeof(ObjectElements) < AsmJSPageSize);
JS_STATIC_ASSERT(AsmJSAllocationGranularity == AsmJSPageSize);
static const size_t AsmJSMappedSize = AsmJSPageSize + AsmJSBufferProtectedSize;

bool
ArrayBufferObject::prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (buffer->isAsmJSArrayBuffer())
        return true;

    // Get the entire reserved region (with all pages inaccessible).
    void *p;
# ifdef XP_WIN
    p = VirtualAlloc(nullptr, AsmJSMappedSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!p)
        return false;
# else
    p = mmap(nullptr, AsmJSMappedSize, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED)
        return false;
# endif

    // Enable access to the valid region.
    JS_ASSERT(buffer->byteLength() % AsmJSAllocationGranularity == 0);
# ifdef XP_WIN
    if (!VirtualAlloc(p, AsmJSPageSize + buffer->byteLength(), MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(p, 0, MEM_RELEASE);
        return false;
    }
# else
    if (mprotect(p, AsmJSPageSize + buffer->byteLength(), PROT_READ | PROT_WRITE)) {
        munmap(p, AsmJSMappedSize);
        return false;
    }
# endif

    // Copy over the current contents of the typed array.
    uint8_t *data = reinterpret_cast<uint8_t*>(p) + AsmJSPageSize;
    memcpy(data, buffer->dataPointer(), buffer->byteLength());

    // Swap the new elements into the ArrayBufferObject.
    ObjectElements *newHeader = reinterpret_cast<ObjectElements*>(data - sizeof(ObjectElements));
    ObjectElements *oldHeader = buffer->hasDynamicElements() ? buffer->getElementsHeader()
                                                             : nullptr;
    buffer->changeContents(cx, newHeader);
    js_free(oldHeader);

    // Mark the ArrayBufferObject so (1) we don't do this again, (2) we know not
    // to js_free the header in the normal way.
    newHeader->setIsAsmJSArrayBuffer();
    JS_ASSERT(data == buffer->dataPointer());
    return true;
}

void
ArrayBufferObject::releaseAsmJSArrayBuffer(FreeOp *fop, JSObject *obj)
{
    ArrayBufferObject &buffer = obj->as<ArrayBufferObject>();
    JS_ASSERT(buffer.isAsmJSArrayBuffer());

    uint8_t *p = buffer.dataPointer() - AsmJSPageSize ;
    JS_ASSERT(uintptr_t(p) % AsmJSPageSize == 0);
# ifdef XP_WIN
    VirtualFree(p, 0, MEM_RELEASE);
# else
    munmap(p, AsmJSMappedSize);
# endif
}

void
ArrayBufferObject::neuterAsmJSArrayBuffer(ArrayBufferObject &buffer)
{
    // Protect all the pages so that any read/write will generate a fault which
    // the AsmJSMemoryFaultHandler will turn into the expected result value.
    JS_ASSERT(buffer.isAsmJSArrayBuffer());
    JS_ASSERT(buffer.byteLength() % AsmJSAllocationGranularity == 0);
#ifdef XP_WIN
    if (!VirtualAlloc(buffer.dataPointer(), buffer.byteLength(), MEM_RESERVE, PAGE_NOACCESS))
        MOZ_CRASH();
#else
    if (mprotect(buffer.dataPointer(), buffer.byteLength(), PROT_NONE))
        MOZ_CRASH();
#endif
}
#else  /* defined(JS_ION) && defined(JS_CPU_X64) */
bool
ArrayBufferObject::prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (!buffer->uninlineData(cx))
        return false;

    buffer->getElementsHeader()->setIsAsmJSArrayBuffer();
    return true;
}

void
ArrayBufferObject::releaseAsmJSArrayBuffer(FreeOp *fop, JSObject *obj)
{
    fop->free_(obj->as<ArrayBufferObject>().getElementsHeader());
}

void
ArrayBufferObject::neuterAsmJSArrayBuffer(ArrayBufferObject &buffer)
{
    // TODO: be ever-so-slightly unsound (but safe) for now.
}
#endif

void
ArrayBufferObject::addView(ArrayBufferViewObject *view)
{
    // This view should never have been associated with a buffer before
    JS_ASSERT(view->bufferLink() == UNSET_BUFFER_LINK);

    // Note that pre-barriers are not needed here because either the list was
    // previously empty, in which case no pointer is being overwritten, or the
    // list was nonempty and will be made weak during this call (and weak
    // pointers cannot violate the snapshot-at-the-beginning invariant.)

    ArrayBufferViewObject *viewsHead = GetViewList(this);
    if (viewsHead == nullptr) {
        // This ArrayBufferObject will have a single view at this point, so it
        // is a strong pointer (it will be marked during tracing.)
        JS_ASSERT(view->nextView() == nullptr);
    } else {
        view->prependToViews(viewsHead);
    }

    SetViewList(this, view);
}

JSObject *
ArrayBufferObject::create(JSContext *cx, uint32_t nbytes, uint8_t *contents)
{
    SkipRoot skip(cx, &contents);

    RootedObject obj(cx, NewBuiltinClassInstance(cx, &class_));
    if (!obj)
        return nullptr;
    JS_ASSERT_IF(obj->isTenured(), obj->tenuredGetAllocKind() == gc::FINALIZE_OBJECT16_BACKGROUND);
    JS_ASSERT(obj->getClass() == &class_);

    js::Shape *empty = EmptyShape::getInitialShape(cx, &class_,
                                                   obj->getProto(), obj->getParent(), obj->getMetadata(),
                                                   gc::FINALIZE_OBJECT16_BACKGROUND);
    if (!empty)
        return nullptr;
    obj->setLastPropertyInfallible(empty);

    /*
     * The beginning stores an ObjectElements header structure holding the
     * length. The rest of it is a flat data store for the array buffer.
     */
    if (!obj->as<ArrayBufferObject>().allocateSlots(cx, nbytes, contents))
        return nullptr;

    return obj;
}

JSObject *
ArrayBufferObject::createSlice(JSContext *cx, ArrayBufferObject &arrayBuffer,
                               uint32_t begin, uint32_t end)
{
    JS_ASSERT(begin <= arrayBuffer.byteLength());
    JS_ASSERT(end <= arrayBuffer.byteLength());
    JS_ASSERT(begin <= end);
    uint32_t length = end - begin;

    if (arrayBuffer.hasData())
        return create(cx, length, arrayBuffer.dataPointer() + begin);

    return create(cx, 0);
}

bool
ArrayBufferObject::createDataViewForThisImpl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsArrayBuffer(args.thisv()));

    /*
     * This method is only called for |DataView(alienBuf, ...)| which calls
     * this as |createDataViewForThis.call(alienBuf, ..., DataView.prototype)|,
     * ergo there must be at least two arguments.
     */
    JS_ASSERT(args.length() >= 2);

    Rooted<JSObject*> proto(cx, &args[args.length() - 1].toObject());

    Rooted<JSObject*> buffer(cx, &args.thisv().toObject());

    /*
     * Pop off the passed-along prototype and delegate to normal DataViewObject
     * construction.
     */
    CallArgs frobbedArgs = CallArgsFromVp(args.length() - 1, args.base());
    return DataViewObject::construct(cx, buffer, frobbedArgs, proto);
}

bool
ArrayBufferObject::createDataViewForThis(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsArrayBuffer, createDataViewForThisImpl>(cx, args);
}

bool
ArrayBufferObject::stealContents(JSContext *cx, JSObject *obj, void **contents,
                                 uint8_t **data)
{
    ArrayBufferObject &buffer = obj->as<ArrayBufferObject>();
    ArrayBufferViewObject *views = GetViewList(&buffer);
    js::ObjectElements *header = js::ObjectElements::fromElements((js::HeapSlot*)buffer.dataPointer());
    if (buffer.hasDynamicElements() && !buffer.isAsmJSArrayBuffer()) {
        SetViewList(&buffer, nullptr);
        *contents = header;
        *data = buffer.dataPointer();

        buffer.setFixedElements();
        header = js::ObjectElements::fromElements((js::HeapSlot*)buffer.dataPointer());
    } else {
        uint32_t length = buffer.byteLength();
        js::ObjectElements *newheader =
            AllocateArrayBufferContents(cx, length, buffer.dataPointer());
        if (!newheader) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        ArrayBufferObject::setElementsHeader(newheader, length);
        *contents = newheader;
        *data = reinterpret_cast<uint8_t *>(newheader + 1);

        if (buffer.isAsmJSArrayBuffer())
            ArrayBufferObject::neuterAsmJSArrayBuffer(buffer);
    }

    // Neuter the donor ArrayBufferObject and all views of it
    ArrayBufferObject::setElementsHeader(header, 0);
    InitViewList(&buffer, views);
    for (ArrayBufferViewObject *view = views; view; view = view->nextView())
        view->neuter();

    return true;
}

void
ArrayBufferObject::obj_trace(JSTracer *trc, JSObject *obj)
{
    /*
     * If this object changes, it will get marked via the private data barrier,
     * so it's safe to leave it Unbarriered.
     */
    JSObject *delegate = static_cast<JSObject*>(obj->getPrivate());
    if (delegate) {
        JS_SET_TRACING_LOCATION(trc, &obj->privateRef(obj->numFixedSlots()));
        MarkObjectUnbarriered(trc, &delegate, "arraybuffer.delegate");
        obj->setPrivateUnbarriered(delegate);
    }

    // ArrayBufferObjects need to maintain a list of possibly-weak pointers to
    // their views. The straightforward way to update the weak pointers would
    // be in the views' finalizers, but giving views finalizers means they
    // cannot be swept in the background. This results in a very high
    // performance cost.  Instead, ArrayBufferObjects with a single view hold a
    // strong pointer to the view. This can entrain garbage when the single
    // view becomes otherwise unreachable while the buffer is still live, but
    // this is expected to be rare. ArrayBufferObjects with 0-1 views are
    // expected to be by far the most common cases. ArrayBufferObjects with
    // multiple views are collected into a linked list during collection, and
    // then swept to prune out their dead views.

    ArrayBufferObject &buffer = obj->as<ArrayBufferObject>();
    ArrayBufferViewObject *viewsHead = GetViewList(&buffer);
    if (!viewsHead)
        return;

    // During minor collections, mark weak pointers on the buffer strongly.
    if (trc->runtime->isHeapMinorCollecting()) {
        MarkObject(trc, &GetViewListRef(&buffer), "arraybuffer.viewlist");
        ArrayBufferViewObject *prior = GetViewList(&buffer);
        for (ArrayBufferViewObject *view = prior->nextView();
             view;
             prior = view, view = view->nextView())
        {
            MarkObjectUnbarriered(trc, &view, "arraybuffer.views");
            prior->setNextView(view);
        }
        return;
    }

    ArrayBufferViewObject *firstView = viewsHead;
    if (firstView->nextView() == nullptr) {
        // Single view: mark it, but only if we're actually doing a GC pass
        // right now. Otherwise, the tracing pass for barrier verification will
        // fail if we add another view and the pointer becomes weak.
        if (IS_GC_MARKING_TRACER(trc))
            MarkObject(trc, &GetViewListRef(&buffer), "arraybuffer.singleview");
    } else {
        // Multiple views: do not mark, but append buffer to list.
        if (IS_GC_MARKING_TRACER(trc)) {
            // obj_trace may be called multiple times before sweep(), so avoid
            // adding this buffer to the list multiple times.
            if (firstView->bufferLink() == UNSET_BUFFER_LINK) {
                JS_ASSERT(obj->compartment() == firstView->compartment());
                ArrayBufferObject **bufList = &obj->compartment()->gcLiveArrayBuffers;
                firstView->setBufferLink(*bufList);
                *bufList = &obj->as<ArrayBufferObject>();
            } else {
#ifdef DEBUG
                bool found = false;
                for (ArrayBufferObject *p = obj->compartment()->gcLiveArrayBuffers;
                     p;
                     p = GetViewList(p)->bufferLink())
                {
                    if (p == obj)
                    {
                        JS_ASSERT(!found);
                        found = true;
                    }
                }
#endif
            }
        }
    }
}

void
ArrayBufferObject::sweep(JSCompartment *compartment)
{
    ArrayBufferObject *buffer = compartment->gcLiveArrayBuffers;
    JS_ASSERT(buffer != UNSET_BUFFER_LINK);
    compartment->gcLiveArrayBuffers = nullptr;

    while (buffer) {
        ArrayBufferViewObject *viewsHead = GetViewList(buffer);
        JS_ASSERT(viewsHead);

        ArrayBufferObject *nextBuffer = viewsHead->bufferLink();
        JS_ASSERT(nextBuffer != UNSET_BUFFER_LINK);
        viewsHead->setBufferLink(UNSET_BUFFER_LINK);

        // Rebuild the list of views of the ArrayBufferObject, discarding dead
        // views.  If there is only one view, it will have already been marked.
        ArrayBufferViewObject *prevLiveView = nullptr;
        ArrayBufferViewObject *view = viewsHead;
        while (view) {
            JS_ASSERT(buffer->compartment() == view->compartment());
            ArrayBufferViewObject *nextView = view->nextView();
            if (!IsObjectAboutToBeFinalized(&view)) {
                view->setNextView(prevLiveView);
                prevLiveView = view;
            }
            view = nextView;
        }
        SetViewList(buffer, prevLiveView);

        buffer = nextBuffer;
    }
}

void
ArrayBufferObject::resetArrayBufferList(JSCompartment *comp)
{
    ArrayBufferObject *buffer = comp->gcLiveArrayBuffers;
    JS_ASSERT(buffer != UNSET_BUFFER_LINK);
    comp->gcLiveArrayBuffers = nullptr;

    while (buffer) {
        ArrayBufferViewObject *view = GetViewList(buffer);
        JS_ASSERT(view);

        ArrayBufferObject *nextBuffer = view->bufferLink();
        JS_ASSERT(nextBuffer != UNSET_BUFFER_LINK);

        view->setBufferLink(UNSET_BUFFER_LINK);
        buffer = nextBuffer;
    }
}

/* static */ bool
ArrayBufferObject::saveArrayBufferList(JSCompartment *comp, ArrayBufferVector &vector)
{
    ArrayBufferObject *buffer = comp->gcLiveArrayBuffers;
    while (buffer) {
        JS_ASSERT(buffer != UNSET_BUFFER_LINK);
        if (!vector.append(buffer))
            return false;

        ArrayBufferViewObject *view = GetViewList(buffer);
        JS_ASSERT(view);
        buffer = view->bufferLink();
    }
    return true;
}

/* static */ void
ArrayBufferObject::restoreArrayBufferLists(ArrayBufferVector &vector)
{
    for (ArrayBufferObject **p = vector.begin(); p != vector.end(); p++) {
        ArrayBufferObject *buffer = *p;
        JSCompartment *comp = buffer->compartment();
        ArrayBufferViewObject *firstView = GetViewList(buffer);
        JS_ASSERT(firstView);
        JS_ASSERT(firstView->compartment() == comp);
        JS_ASSERT(firstView->bufferLink() == UNSET_BUFFER_LINK);
        firstView->setBufferLink(comp->gcLiveArrayBuffers);
        comp->gcLiveArrayBuffers = buffer;
    }
}

bool
ArrayBufferObject::obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                     MutableHandleObject objp, MutableHandleShape propp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;

    bool delegateResult = JSObject::lookupGeneric(cx, delegate, id, objp, propp);

    /* If false, there was an error, so propagate it.
     * Otherwise, if propp is non-null, the property
     * was found. Otherwise it was not
     * found so look in the prototype chain.
     */
    if (!delegateResult)
        return false;

    if (propp) {
        if (objp == delegate)
            objp.set(obj);
        return true;
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        objp.set(nullptr);
        propp.set(nullptr);
        return true;
    }

    return JSObject::lookupGeneric(cx, proto, id, objp, propp);
}

bool
ArrayBufferObject::obj_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                      MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

bool
ArrayBufferObject::obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                     MutableHandleObject objp, MutableHandleShape propp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;

    /*
     * If false, there was an error, so propagate it.
     * Otherwise, if propp is non-null, the property
     * was found. Otherwise it was not
     * found so look in the prototype chain.
     */
    if (!JSObject::lookupElement(cx, delegate, index, objp, propp))
        return false;

    if (propp) {
        if (objp == delegate)
            objp.set(obj);
        return true;
    }

    RootedObject proto(cx, obj->getProto());
    if (proto)
        return JSObject::lookupElement(cx, proto, index, objp, propp);

    objp.set(nullptr);
    propp.set(nullptr);
    return true;
}

bool
ArrayBufferObject::obj_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                     MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

bool
ArrayBufferObject::obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DefineGeneric(cx, delegate, id, v, getter, setter, attrs);
}

bool
ArrayBufferObject::obj_defineProperty(JSContext *cx, HandleObject obj,
                                      HandlePropertyName name, HandleValue v,
                                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

bool
ArrayBufferObject::obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DefineElement(cx, delegate, index, v, getter, setter, attrs);
}

bool
ArrayBufferObject::obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue v,
                                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

bool
ArrayBufferObject::obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                                  HandleId id, MutableHandleValue vp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::GetProperty(cx, delegate, receiver, id, vp);
}

bool
ArrayBufferObject::obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                                   HandlePropertyName name, MutableHandleValue vp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    Rooted<jsid> id(cx, NameToId(name));
    return baseops::GetProperty(cx, delegate, receiver, id, vp);
}

bool
ArrayBufferObject::obj_getElement(JSContext *cx, HandleObject obj,
                                  HandleObject receiver, uint32_t index, MutableHandleValue vp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::GetElement(cx, delegate, receiver, index, vp);
}

bool
ArrayBufferObject::obj_getElementIfPresent(JSContext *cx, HandleObject obj, HandleObject receiver,
                                           uint32_t index, MutableHandleValue vp, bool *present)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return JSObject::getElementIfPresent(cx, delegate, receiver, index, vp, present);
}

bool
ArrayBufferObject::obj_getSpecial(JSContext *cx, HandleObject obj,
                                  HandleObject receiver, HandleSpecialId sid,
                                  MutableHandleValue vp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

bool
ArrayBufferObject::obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                  MutableHandleValue vp, bool strict)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;

    return baseops::SetPropertyHelper(cx, delegate, obj, id, 0, vp, strict);
}

bool
ArrayBufferObject::obj_setProperty(JSContext *cx, HandleObject obj,
                                   HandlePropertyName name, MutableHandleValue vp, bool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

bool
ArrayBufferObject::obj_setElement(JSContext *cx, HandleObject obj,
                                  uint32_t index, MutableHandleValue vp, bool strict)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;

    return baseops::SetElementHelper(cx, delegate, obj, index, 0, vp, strict);
}

bool
ArrayBufferObject::obj_setSpecial(JSContext *cx, HandleObject obj,
                                  HandleSpecialId sid, MutableHandleValue vp, bool strict)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

bool
ArrayBufferObject::obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                            HandleId id, unsigned *attrsp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::GetAttributes(cx, delegate, id, attrsp);
}

bool
ArrayBufferObject::obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                            HandleId id, unsigned *attrsp)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::SetAttributes(cx, delegate, id, attrsp);
}

bool
ArrayBufferObject::obj_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                      bool *succeeded)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DeleteProperty(cx, delegate, name, succeeded);
}

bool
ArrayBufferObject::obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                                     bool *succeeded)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DeleteElement(cx, delegate, index, succeeded);
}

bool
ArrayBufferObject::obj_deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                     bool *succeeded)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DeleteSpecial(cx, delegate, sid, succeeded);
}

bool
ArrayBufferObject::obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                                 MutableHandleValue statep, MutableHandleId idp)
{
    statep.setNull();
    return true;
}

/*
 * ArrayBufferViewObject
 */

inline void
ArrayBufferViewObject::setBufferLink(ArrayBufferObject *buffer)
{
    setFixedSlot(NEXT_BUFFER_SLOT, PrivateValue(buffer));
    PostBarrierTypedArrayObject(this);
}

inline void
ArrayBufferViewObject::setNextView(ArrayBufferViewObject *view)
{
    setFixedSlot(NEXT_VIEW_SLOT, PrivateValue(view));
    PostBarrierTypedArrayObject(this);
}

/*
 * TypedArrayObject
 *
 * The non-templated base class for the specific typed implementations.
 * This class holds all the member variables that are used by
 * the subclasses.
 */

inline bool
TypedArrayObject::isArrayIndex(jsid id, uint32_t *ip)
{
    uint32_t index;
    if (js_IdIsIndex(id, &index) && index < length()) {
        if (ip)
            *ip = index;
        return true;
    }

    return false;
}

void
TypedArrayObject::neuter()
{
    setSlot(LENGTH_SLOT, Int32Value(0));
    setSlot(BYTELENGTH_SLOT, Int32Value(0));
    setSlot(BYTEOFFSET_SLOT, Int32Value(0));
    setPrivate(nullptr);
}

bool
TypedArrayObject::obj_lookupGeneric(JSContext *cx, HandleObject tarray, HandleId id,
                                    MutableHandleObject objp, MutableHandleShape propp)
{
    if (tarray->as<TypedArrayObject>().isArrayIndex(id)) {
        MarkNonNativePropertyFound(propp);
        objp.set(tarray);
        return true;
    }

    RootedObject proto(cx, tarray->getProto());
    if (!proto) {
        objp.set(nullptr);
        propp.set(nullptr);
        return true;
    }

    return JSObject::lookupGeneric(cx, proto, id, objp, propp);
}

bool
TypedArrayObject::obj_lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                     MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

bool
TypedArrayObject::obj_lookupElement(JSContext *cx, HandleObject tarray, uint32_t index,
                                    MutableHandleObject objp, MutableHandleShape propp)
{
    if (index < tarray->as<TypedArrayObject>().length()) {
        MarkNonNativePropertyFound(propp);
        objp.set(tarray);
        return true;
    }

    RootedObject proto(cx, tarray->getProto());
    if (proto)
        return JSObject::lookupElement(cx, proto, index, objp, propp);

    objp.set(nullptr);
    propp.set(nullptr);
    return true;
}

bool
TypedArrayObject::obj_lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                    MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

bool
TypedArrayObject::obj_getGenericAttributes(JSContext *cx, HandleObject obj, HandleId id,
                                           unsigned *attrsp)
{
    *attrsp = JSPROP_PERMANENT | JSPROP_ENUMERATE;
    return true;
}

bool
TypedArrayObject::obj_setGenericAttributes(JSContext *cx, HandleObject obj, HandleId id,
                                           unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

/* static */ int
TypedArrayObject::lengthOffset()
{
    return JSObject::getFixedSlotOffset(LENGTH_SLOT);
}

/* static */ int
TypedArrayObject::dataOffset()
{
    return JSObject::getPrivateDataOffset(DATA_SLOT);
}

/* Helper clamped uint8_t type */

uint32_t JS_FASTCALL
js::ClampDoubleToUint8(const double x)
{
    // Not < so that NaN coerces to 0
    if (!(x >= 0))
        return 0;

    if (x > 255)
        return 255;

    double toTruncate = x + 0.5;
    uint8_t y = uint8_t(toTruncate);

    /*
     * now val is rounded to nearest, ties rounded up.  We want
     * rounded to nearest ties to even, so check whether we had a
     * tie.
     */
    if (y == toTruncate) {
        /*
         * It was a tie (since adding 0.5 gave us the exact integer
         * we want).  Since we rounded up, we either already have an
         * even number or we have an odd number but the number we
         * want is one less.  So just unconditionally masking out the
         * ones bit should do the trick to get us the value we
         * want.
         */
        return (y & ~1);
    }

    return y;
}

bool
js::ToDoubleForTypedArray(JSContext *cx, JS::HandleValue vp, double *d)
{
    if (vp.isDouble()) {
        *d = vp.toDouble();
    } else if (vp.isNull()) {
        *d = 0.0;
    } else if (vp.isPrimitive()) {
        JS_ASSERT(vp.isString() || vp.isUndefined() || vp.isBoolean());
        if (vp.isString()) {
            if (!ToNumber(cx, vp, d))
                return false;
        } else if (vp.isUndefined()) {
            *d = GenericNaN();
        } else {
            *d = double(vp.toBoolean());
        }
    } else {
        // non-primitive assignments become NaN or 0 (for float/int arrays)
        *d = GenericNaN();
    }

#ifdef JS_MORE_DETERMINISTIC
    // It's possible to have a NaN value with the sign bit set. The spec allows
    // this but it can confuse differential testing when this value is stored
    // to a float array and then read back as integer. To work around this, we
    // always canonicalize NaN values in more-deterministic builds.
    *d = JS_CANONICALIZE_NAN(*d);
#endif

    return true;
}

/*
 * This method is used to trace TypedArrayObjects and DataViewObjects. We need
 * a custom tracer because some of an ArrayBufferViewObject's reserved slots
 * are weak references, and some need to be updated specially during moving
 * GCs.
 */
/* static */ void
ArrayBufferViewObject::trace(JSTracer *trc, JSObject *obj)
{
    HeapSlot &bufSlot = obj->getReservedSlotRef(BUFFER_SLOT);
    MarkSlot(trc, &bufSlot, "typedarray.buffer");

    /* Update obj's data slot if the array buffer moved. Note that during
     * initialization, bufSlot may still be JSVAL_VOID. */
    if (bufSlot.isObject()) {
        ArrayBufferObject &buf = bufSlot.toObject().as<ArrayBufferObject>();
        int32_t offset = obj->getReservedSlot(BYTEOFFSET_SLOT).toInt32();
        obj->initPrivate(buf.dataPointer() + offset);
    }

    /* Update NEXT_VIEW_SLOT, if the view moved. */
    IsSlotMarked(&obj->getReservedSlotRef(NEXT_VIEW_SLOT));
}

template<typename NativeType> static inline const int TypeIDOfType();
template<> inline const int TypeIDOfType<int8_t>() { return ScalarTypeRepresentation::TYPE_INT8; }
template<> inline const int TypeIDOfType<uint8_t>() { return ScalarTypeRepresentation::TYPE_UINT8; }
template<> inline const int TypeIDOfType<int16_t>() { return ScalarTypeRepresentation::TYPE_INT16; }
template<> inline const int TypeIDOfType<uint16_t>() { return ScalarTypeRepresentation::TYPE_UINT16; }
template<> inline const int TypeIDOfType<int32_t>() { return ScalarTypeRepresentation::TYPE_INT32; }
template<> inline const int TypeIDOfType<uint32_t>() { return ScalarTypeRepresentation::TYPE_UINT32; }
template<> inline const int TypeIDOfType<float>() { return ScalarTypeRepresentation::TYPE_FLOAT32; }
template<> inline const int TypeIDOfType<double>() { return ScalarTypeRepresentation::TYPE_FLOAT64; }
template<> inline const int TypeIDOfType<uint8_clamped>() { return ScalarTypeRepresentation::TYPE_UINT8_CLAMPED; }

template<typename ElementType>
static inline JSObject *
NewArray(JSContext *cx, uint32_t nelements);

static inline void
InitArrayBufferViewDataPointer(JSObject *obj, ArrayBufferObject *buffer, size_t byteOffset)
{
    /*
     * N.B. The base of the array's data is stored in the object's
     * private data rather than a slot to avoid alignment restrictions
     * on private Values.
     */
    obj->initPrivate(buffer->dataPointer() + byteOffset);
    PostBarrierTypedArrayObject(obj);
}

namespace {

template<typename NativeType>
class TypedArrayObjectTemplate : public TypedArrayObject
{
  public:
    typedef NativeType ThisType;
    typedef TypedArrayObjectTemplate<NativeType> ThisTypedArrayObject;
    static const int ArrayTypeID() { return TypeIDOfType<NativeType>(); }
    static const bool ArrayTypeIsUnsigned() { return TypeIsUnsigned<NativeType>(); }
    static const bool ArrayTypeIsFloatingPoint() { return TypeIsFloatingPoint<NativeType>(); }

    static const size_t BYTES_PER_ELEMENT = sizeof(ThisType);

    static inline const Class *protoClass()
    {
        return &TypedArrayObject::protoClasses[ArrayTypeID()];
    }

    static inline const Class *fastClass()
    {
        return &TypedArrayObject::classes[ArrayTypeID()];
    }

    static bool is(HandleValue v) {
        return v.isObject() && v.toObject().hasClass(fastClass());
    }

    static bool
    obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name,
                    MutableHandleValue vp)
    {
        RootedObject proto(cx, obj->getProto());
        if (!proto) {
            vp.setUndefined();
            return true;
        }

        return JSObject::getProperty(cx, proto, receiver, name, vp);
    }

    static bool
    obj_getElement(JSContext *cx, HandleObject tarray, HandleObject receiver, uint32_t index,
                   MutableHandleValue vp)
    {
        if (index < tarray->as<TypedArrayObject>().length()) {
            copyIndexToValue(tarray, index, vp);
            return true;
        }

        vp.setUndefined();
        return true;
    }

    static bool
    obj_getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid,
                   MutableHandleValue vp)
    {
        RootedObject proto(cx, obj->getProto());
        if (!proto) {
            vp.setUndefined();
            return true;
        }

        return JSObject::getSpecial(cx, proto, receiver, sid, vp);
    }

    static bool
    obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id,
                   MutableHandleValue vp)
    {
        RootedValue idval(cx, IdToValue(id));

        uint32_t index;
        if (IsDefinitelyIndex(idval, &index))
            return obj_getElement(cx, obj, receiver, index, vp);

        Rooted<SpecialId> sid(cx);
        if (ValueIsSpecial(obj, &idval, &sid, cx))
            return obj_getSpecial(cx, obj, receiver, sid, vp);

        JSAtom *atom = ToAtom<CanGC>(cx, idval);
        if (!atom)
            return false;

        if (atom->isIndex(&index))
            return obj_getElement(cx, obj, receiver, index, vp);

        Rooted<PropertyName*> name(cx, atom->asPropertyName());
        return obj_getProperty(cx, obj, receiver, name, vp);
    }

    static bool
    obj_getElementIfPresent(JSContext *cx, HandleObject tarray, HandleObject receiver, uint32_t index,
                            MutableHandleValue vp, bool *present)
    {
        // Fast-path the common case of index < length
        if (index < tarray->as<TypedArrayObject>().length()) {
            // this inline function is specialized for each type
            copyIndexToValue(tarray, index, vp);
            *present = true;
            return true;
        }

        RootedObject proto(cx, tarray->getProto());
        if (!proto) {
            vp.setUndefined();
            return true;
        }

        return JSObject::getElementIfPresent(cx, proto, receiver, index, vp, present);
    }

    static bool
    setElementTail(JSContext *cx, HandleObject tarray, uint32_t index,
                   MutableHandleValue vp, bool strict)
    {
        JS_ASSERT(tarray);
        JS_ASSERT(index < tarray->as<TypedArrayObject>().length());

        if (vp.isInt32()) {
            setIndex(tarray, index, NativeType(vp.toInt32()));
            return true;
        }

        double d;
        if (!ToDoubleForTypedArray(cx, vp, &d))
            return false;

        // If the array is an integer array, we only handle up to
        // 32-bit ints from this point on.  if we want to handle
        // 64-bit ints, we'll need some changes.

        // Assign based on characteristics of the destination type
        if (ArrayTypeIsFloatingPoint()) {
            setIndex(tarray, index, NativeType(d));
        } else if (ArrayTypeIsUnsigned()) {
            JS_ASSERT(sizeof(NativeType) <= 4);
            uint32_t n = ToUint32(d);
            setIndex(tarray, index, NativeType(n));
        } else if (ArrayTypeID() == ScalarTypeRepresentation::TYPE_UINT8_CLAMPED) {
            // The uint8_clamped type has a special rounding converter
            // for doubles.
            setIndex(tarray, index, NativeType(d));
        } else {
            JS_ASSERT(sizeof(NativeType) <= 4);
            int32_t n = ToInt32(d);
            setIndex(tarray, index, NativeType(n));
        }

        return true;
    }

    static bool
    obj_setGeneric(JSContext *cx, HandleObject tarray, HandleId id,
                   MutableHandleValue vp, bool strict)
    {
        uint32_t index;
        // We can't just chain to js_SetPropertyHelper, because we're not a normal object.
        if (!tarray->as<TypedArrayObject>().isArrayIndex(id, &index)) {
            // Silent ignore is better than an exception here, because
            // at some point we may want to support other properties on
            // these objects.  This is especially true when these arrays
            // are used to implement HTML Canvas 2D's PixelArray objects,
            // which used to be plain old arrays.
            vp.setUndefined();
            return true;
        }

        return setElementTail(cx, tarray, index, vp, strict);
    }

    static bool
    obj_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                    MutableHandleValue vp, bool strict)
    {
        Rooted<jsid> id(cx, NameToId(name));
        return obj_setGeneric(cx, obj, id, vp, strict);
    }

    static bool
    obj_setElement(JSContext *cx, HandleObject tarray, uint32_t index,
                   MutableHandleValue vp, bool strict)
    {
        if (index >= tarray->as<TypedArrayObject>().length()) {
            // Silent ignore is better than an exception here, because
            // at some point we may want to support other properties on
            // these objects.  This is especially true when these arrays
            // are used to implement HTML Canvas 2D's PixelArray objects,
            // which used to be plain old arrays.
            vp.setUndefined();
            return true;
        }

        return setElementTail(cx, tarray, index, vp, strict);
    }

    static bool
    obj_setSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                   MutableHandleValue vp, bool strict)
    {
        Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
        return obj_setGeneric(cx, obj, id, vp, strict);
    }

    static bool
    obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        RootedValue tmp(cx, v);
        return obj_setGeneric(cx, obj, id, &tmp, false);
    }

    static bool
    obj_defineProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, HandleValue v,
                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        Rooted<jsid> id(cx, NameToId(name));
        return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
    }

    static bool
    obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        RootedValue tmp(cx, v);
        return obj_setElement(cx, obj, index, &tmp, false);
    }

    static bool
    obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue v,
                      PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
    {
        Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
        return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
    }

    static bool
    obj_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, bool *succeeded)
    {
        *succeeded = true;
        return true;
    }

    static bool
    obj_deleteElement(JSContext *cx, HandleObject tarray, uint32_t index, bool *succeeded)
    {
        if (index < tarray->as<TypedArrayObject>().length()) {
            *succeeded = false;
            return true;
        }

        *succeeded = true;
        return true;
    }

    static bool
    obj_deleteSpecial(JSContext *cx, HandleObject tarray, HandleSpecialId sid, bool *succeeded)
    {
        *succeeded = true;
        return true;
    }

    static bool
    obj_enumerate(JSContext *cx, HandleObject tarray, JSIterateOp enum_op,
                  MutableHandleValue statep, MutableHandleId idp)
    {
        JS_ASSERT(tarray->is<TypedArrayObject>());

        uint32_t index;
        switch (enum_op) {
          case JSENUMERATE_INIT_ALL:
          case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(::INT_TO_JSID(tarray->as<TypedArrayObject>().length()));
            break;

          case JSENUMERATE_NEXT: {
            index = static_cast<uint32_t>(statep.toInt32());
            uint32_t length = tarray->as<TypedArrayObject>().length();
            if (index < length) {
                idp.set(::INT_TO_JSID(index));
                statep.setInt32(index + 1);
            } else {
                JS_ASSERT(index == length);
                statep.setNull();
            }
            break;
          }
          case JSENUMERATE_DESTROY:
            statep.setNull();
            break;
        }

        return true;
    }

    static TypedArrayObject *
    makeProtoInstance(JSContext *cx, HandleObject proto)
    {
        JS_ASSERT(proto);

        RootedObject obj(cx, NewBuiltinClassInstance(cx, fastClass()));
        if (!obj)
            return nullptr;

        types::TypeObject *type = cx->getNewType(obj->getClass(), proto.get());
        if (!type)
            return nullptr;
        obj->setType(type);

        return &obj->as<TypedArrayObject>();
    }

    static TypedArrayObject *
    makeTypedInstance(JSContext *cx, uint32_t len)
    {
        if (len * sizeof(NativeType) >= TypedArrayObject::SINGLETON_TYPE_BYTE_LENGTH) {
            return &NewBuiltinClassInstance(cx, fastClass(),
                                            SingletonObject)->as<TypedArrayObject>();
        }

        jsbytecode *pc;
        RootedScript script(cx, cx->currentScript(&pc));
        NewObjectKind newKind = script
                                ? UseNewTypeForInitializer(script, pc, fastClass())
                                : GenericObject;
        RootedObject obj(cx, NewBuiltinClassInstance(cx, fastClass(), newKind));
        if (!obj)
            return nullptr;

        if (script) {
            if (!types::SetInitializerObjectType(cx, script, pc, obj, newKind))
                return nullptr;
        }

        return &obj->as<TypedArrayObject>();
    }

    static JSObject *
    makeInstance(JSContext *cx, HandleObject bufobj, uint32_t byteOffset, uint32_t len,
                 HandleObject proto)
    {
        Rooted<TypedArrayObject*> obj(cx);
        if (proto)
            obj = makeProtoInstance(cx, proto);
        else if (cx->typeInferenceEnabled())
            obj = makeTypedInstance(cx, len);
        else
            obj = &NewBuiltinClassInstance(cx, fastClass())->as<TypedArrayObject>();
        if (!obj)
            return nullptr;
        JS_ASSERT_IF(obj->isTenured(),
                     obj->tenuredGetAllocKind() == gc::FINALIZE_OBJECT8_BACKGROUND);

        obj->setSlot(TYPE_SLOT, Int32Value(ArrayTypeID()));
        obj->setSlot(BUFFER_SLOT, ObjectValue(*bufobj));

        Rooted<ArrayBufferObject *> buffer(cx, &bufobj->as<ArrayBufferObject>());

        InitArrayBufferViewDataPointer(obj, buffer, byteOffset);
        obj->setSlot(LENGTH_SLOT, Int32Value(len));
        obj->setSlot(BYTEOFFSET_SLOT, Int32Value(byteOffset));
        obj->setSlot(BYTELENGTH_SLOT, Int32Value(len * sizeof(NativeType)));
        obj->setSlot(NEXT_VIEW_SLOT, PrivateValue(nullptr));
        obj->setSlot(NEXT_BUFFER_SLOT, PrivateValue(UNSET_BUFFER_LINK));

        // Mark the object as non-extensible. We cannot simply call
        // obj->preventExtensions() because that has to iterate through all
        // properties, and on long arrays that is much too slow. We could
        // initialize the length fields to zero to avoid that, but then it
        // would just boil down to a slightly slower wrapper around the
        // following code anyway:
        js::Shape *empty = EmptyShape::getInitialShape(cx, fastClass(),
                                                       obj->getProto(), obj->getParent(), obj->getMetadata(),
                                                       gc::FINALIZE_OBJECT8_BACKGROUND,
                                                       BaseShape::NOT_EXTENSIBLE);
        if (!empty)
            return nullptr;
        obj->setLastPropertyInfallible(empty);

#ifdef DEBUG
        uint32_t bufferByteLength = buffer->byteLength();
        uint32_t arrayByteLength = obj->byteLength();
        uint32_t arrayByteOffset = obj->byteOffset();
        JS_ASSERT(buffer->dataPointer() <= obj->viewData());
        JS_ASSERT(bufferByteLength - arrayByteOffset >= arrayByteLength);
        JS_ASSERT(arrayByteOffset <= bufferByteLength);

        // Verify that the private slot is at the expected place
        JS_ASSERT(obj->numFixedSlots() == DATA_SLOT);
#endif

        buffer->addView(obj);

        return obj;
    }

    static JSObject *
    makeInstance(JSContext *cx, HandleObject bufobj, uint32_t byteOffset, uint32_t len)
    {
        RootedObject nullproto(cx, nullptr);
        return makeInstance(cx, bufobj, byteOffset, len, nullproto);
    }

    /*
     * new [Type]Array(length)
     * new [Type]Array(otherTypedArray)
     * new [Type]Array(JSArray)
     * new [Type]Array(ArrayBuffer, [optional] byteOffset, [optional] length)
     */
    static bool
    class_constructor(JSContext *cx, unsigned argc, Value *vp)
    {
        /* N.B. this is a constructor for protoClass, not fastClass! */
        CallArgs args = CallArgsFromVp(argc, vp);
        JSObject *obj = create(cx, args);
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    static JSObject *
    create(JSContext *cx, const CallArgs& args)
    {
        /* () or (number) */
        uint32_t len = 0;
        if (args.length() == 0 || ValueIsLength(args[0], &len))
            return fromLength(cx, len);

        /* (not an object) */
        if (!args[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return nullptr;
        }

        RootedObject dataObj(cx, &args.get(0).toObject());

        /*
         * (typedArray)
         * (type[] array)
         *
         * Otherwise create a new typed array and copy elements 0..len-1
         * properties from the object, treating it as some sort of array.
         * Note that offset and length will be ignored
         */
        if (!UncheckedUnwrap(dataObj)->is<ArrayBufferObject>())
            return fromArray(cx, dataObj);

        /* (ArrayBuffer, [byteOffset, [length]]) */
        int32_t byteOffset = 0;
        int32_t length = -1;

        if (args.length() > 1) {
            if (!ToInt32(cx, args[1], &byteOffset))
                return nullptr;
            if (byteOffset < 0) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                     JSMSG_TYPED_ARRAY_NEGATIVE_ARG, "1");
                return nullptr;
            }

            if (args.length() > 2) {
                if (!ToInt32(cx, args[2], &length))
                    return nullptr;
                if (length < 0) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                         JSMSG_TYPED_ARRAY_NEGATIVE_ARG, "2");
                    return nullptr;
                }
            }
        }

        Rooted<JSObject*> proto(cx, nullptr);
        return fromBuffer(cx, dataObj, byteOffset, length, proto);
    }

    static bool IsThisClass(HandleValue v) {
        return v.isObject() && v.toObject().hasClass(fastClass());
    }

    template<Value ValueGetter(TypedArrayObject *tarr)>
    static bool
    GetterImpl(JSContext *cx, CallArgs args)
    {
        JS_ASSERT(IsThisClass(args.thisv()));
        args.rval().set(ValueGetter(&args.thisv().toObject().as<TypedArrayObject>()));
        return true;
    }

    // ValueGetter is a function that takes an unwrapped typed array object and
    // returns a Value. Given such a function, Getter<> is a native that
    // retrieves a given Value, probably from a slot on the object.
    template<Value ValueGetter(TypedArrayObject *tarr)>
    static bool
    Getter(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);
        return CallNonGenericMethod<ThisTypedArrayObject::IsThisClass,
                                    ThisTypedArrayObject::GetterImpl<ValueGetter> >(cx, args);
    }

    // Define an accessor for a read-only property that invokes a native getter
    template<Value ValueGetter(TypedArrayObject *tarr)>
    static bool
    DefineGetter(JSContext *cx, PropertyName *name, HandleObject proto)
    {
        RootedId id(cx, NameToId(name));
        unsigned flags = JSPROP_SHARED | JSPROP_GETTER | JSPROP_PERMANENT;

        Rooted<GlobalObject*> global(cx, cx->compartment()->maybeGlobal());
        JSObject *getter = NewFunction(cx, NullPtr(), Getter<ValueGetter>, 0,
                                       JSFunction::NATIVE_FUN, global, NullPtr());
        if (!getter)
            return false;

        RootedValue value(cx, UndefinedValue());
        return DefineNativeProperty(cx, proto, id, value,
                                    JS_DATA_TO_FUNC_PTR(PropertyOp, getter), nullptr,
                                    flags, 0, 0);
    }

    static
    bool defineGetters(JSContext *cx, HandleObject proto)
    {
        if (!DefineGetter<lengthValue>(cx, cx->names().length, proto))
            return false;

        if (!DefineGetter<bufferValue>(cx, cx->names().buffer, proto))
            return false;

        if (!DefineGetter<byteLengthValue>(cx, cx->names().byteLength, proto))
            return false;

        if (!DefineGetter<byteOffsetValue>(cx, cx->names().byteOffset, proto))
            return false;

        return true;
    }

    /* subarray(start[, end]) */
    static bool
    fun_subarray_impl(JSContext *cx, CallArgs args)
    {
        JS_ASSERT(IsThisClass(args.thisv()));
        Rooted<TypedArrayObject*> tarray(cx, &args.thisv().toObject().as<TypedArrayObject>());

        // these are the default values
        uint32_t length = tarray->length();
        uint32_t begin = 0, end = length;

        if (args.length() > 0) {
            if (!ToClampedIndex(cx, args[0], length, &begin))
                return false;

            if (args.length() > 1) {
                if (!ToClampedIndex(cx, args[1], length, &end))
                    return false;
            }
        }

        if (begin > end)
            begin = end;

        JSObject *nobj = createSubarray(cx, tarray, begin, end);
        if (!nobj)
            return false;
        args.rval().setObject(*nobj);
        return true;
    }

    static bool
    fun_subarray(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);
        return CallNonGenericMethod<ThisTypedArrayObject::IsThisClass,
                                    ThisTypedArrayObject::fun_subarray_impl>(cx, args);
    }

    /* move(begin, end, dest) */
    static bool
    fun_move_impl(JSContext *cx, CallArgs args)
    {
        JS_ASSERT(IsThisClass(args.thisv()));
        Rooted<TypedArrayObject*> tarray(cx, &args.thisv().toObject().as<TypedArrayObject>());

        if (args.length() < 3) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        uint32_t srcBegin;
        uint32_t srcEnd;
        uint32_t dest;

        uint32_t length = tarray->length();
        if (!ToClampedIndex(cx, args[0], length, &srcBegin) ||
            !ToClampedIndex(cx, args[1], length, &srcEnd) ||
            !ToClampedIndex(cx, args[2], length, &dest) ||
            srcBegin > srcEnd)
        {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        uint32_t nelts = srcEnd - srcBegin;

        JS_ASSERT(dest + nelts >= dest);
        if (dest + nelts > length) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        uint32_t byteDest = dest * sizeof(NativeType);
        uint32_t byteSrc = srcBegin * sizeof(NativeType);
        uint32_t byteSize = nelts * sizeof(NativeType);

#ifdef DEBUG
        uint32_t viewByteLength = tarray->byteLength();
        JS_ASSERT(byteDest <= viewByteLength);
        JS_ASSERT(byteSrc <= viewByteLength);
        JS_ASSERT(byteDest + byteSize <= viewByteLength);
        JS_ASSERT(byteSrc + byteSize <= viewByteLength);

        // Should not overflow because size is limited to 2^31
        JS_ASSERT(byteDest + byteSize >= byteDest);
        JS_ASSERT(byteSrc + byteSize >= byteSrc);
#endif

        uint8_t *data = static_cast<uint8_t*>(tarray->viewData());
        memmove(&data[byteDest], &data[byteSrc], byteSize);
        args.rval().setUndefined();
        return true;
    }

    static bool
    fun_move(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);
        return CallNonGenericMethod<ThisTypedArrayObject::IsThisClass,
                                    ThisTypedArrayObject::fun_move_impl>(cx, args);
    }

    /* set(array[, offset]) */
    static bool
    fun_set_impl(JSContext *cx, CallArgs args)
    {
        JS_ASSERT(IsThisClass(args.thisv()));
        Rooted<TypedArrayObject*> tarray(cx, &args.thisv().toObject().as<TypedArrayObject>());

        // first arg must be either a typed array or a JS array
        if (args.length() == 0 || !args[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        int32_t offset = 0;
        if (args.length() > 1) {
            if (!ToInt32(cx, args[1], &offset))
                return false;

            if (offset < 0 || uint32_t(offset) > tarray->length()) {
                // the given offset is bogus
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                     JSMSG_TYPED_ARRAY_BAD_INDEX, "2");
                return false;
            }
        }

        if (!args[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        RootedObject arg0(cx, args[0].toObjectOrNull());
        if (arg0->is<TypedArrayObject>()) {
            if (arg0->as<TypedArrayObject>().length() > tarray->length() - offset) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_ARRAY_LENGTH);
                return false;
            }

            if (!copyFromTypedArray(cx, tarray, arg0, offset))
                return false;
        } else {
            uint32_t len;
            if (!GetLengthProperty(cx, arg0, &len))
                return false;

            // avoid overflow; we know that offset <= length
            if (len > tarray->length() - offset) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_ARRAY_LENGTH);
                return false;
            }

            if (!copyFromArray(cx, tarray, arg0, len, offset))
                return false;
        }

        args.rval().setUndefined();
        return true;
    }

    static bool
    fun_set(JSContext *cx, unsigned argc, Value *vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);
        return CallNonGenericMethod<ThisTypedArrayObject::IsThisClass,
                                    ThisTypedArrayObject::fun_set_impl>(cx, args);
    }

  public:
    static JSObject *
    fromBuffer(JSContext *cx, HandleObject bufobj, uint32_t byteOffset, int32_t lengthInt,
               HandleObject proto)
    {
        if (!ObjectClassIs(bufobj, ESClass_ArrayBuffer, cx)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return nullptr; // must be arrayBuffer
        }

        JS_ASSERT(bufobj->is<ArrayBufferObject>() || bufobj->is<ProxyObject>());
        if (bufobj->is<ProxyObject>()) {
            /*
             * Normally, NonGenericMethodGuard handles the case of transparent
             * wrappers. However, we have a peculiar situation: we want to
             * construct the new typed array in the compartment of the buffer,
             * so that the typed array can point directly at their buffer's
             * data without crossing compartment boundaries. So we use the
             * machinery underlying NonGenericMethodGuard directly to proxy the
             * native call. We will end up with a wrapper in the origin
             * compartment for a view in the target compartment referencing the
             * ArrayBufferObject in that same compartment.
             */
            JSObject *wrapped = CheckedUnwrap(bufobj);
            if (!wrapped) {
                JS_ReportError(cx, "Permission denied to access object");
                return nullptr;
            }
            if (wrapped->is<ArrayBufferObject>()) {
                /*
                 * And for even more fun, the new view's prototype should be
                 * set to the origin compartment's prototype object, not the
                 * target's (specifically, the actual view in the target
                 * compartment will use as its prototype a wrapper around the
                 * origin compartment's view.prototype object).
                 *
                 * Rather than hack some crazy solution together, implement
                 * this all using a private helper function, created when
                 * ArrayBufferObject was initialized and cached in the global.
                 * This reuses all the existing cross-compartment crazy so we
                 * don't have to do anything *uniquely* crazy here.
                 */

                Rooted<JSObject*> proto(cx);
                if (!FindProto(cx, fastClass(), &proto))
                    return nullptr;

                InvokeArgs args(cx);
                if (!args.init(3))
                    return nullptr;

                args.setCallee(cx->compartment()->maybeGlobal()->createArrayFromBuffer<NativeType>());
                args.setThis(ObjectValue(*bufobj));
                args[0].setNumber(byteOffset);
                args[1].setInt32(lengthInt);
                args[2].setObject(*proto);

                if (!Invoke(cx, args))
                    return nullptr;
                return &args.rval().toObject();
            }
        }

        if (!bufobj->is<ArrayBufferObject>()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return nullptr; // must be arrayBuffer
        }

        ArrayBufferObject &buffer = bufobj->as<ArrayBufferObject>();

        if (byteOffset > buffer.byteLength() || byteOffset % sizeof(NativeType) != 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return nullptr; // invalid byteOffset
        }

        uint32_t len;
        if (lengthInt == -1) {
            len = (buffer.byteLength() - byteOffset) / sizeof(NativeType);
            if (len * sizeof(NativeType) != buffer.byteLength() - byteOffset) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                     JSMSG_TYPED_ARRAY_BAD_ARGS);
                return nullptr; // given byte array doesn't map exactly to sizeof(NativeType) * N
            }
        } else {
            len = uint32_t(lengthInt);
        }

        // Go slowly and check for overflow.
        uint32_t arrayByteLength = len * sizeof(NativeType);
        if (len >= INT32_MAX / sizeof(NativeType) || byteOffset >= INT32_MAX - arrayByteLength) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return nullptr; // overflow when calculating byteOffset + len * sizeof(NativeType)
        }

        if (arrayByteLength + byteOffset > buffer.byteLength()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return nullptr; // byteOffset + len is too big for the arraybuffer
        }

        return makeInstance(cx, bufobj, byteOffset, len, proto);
    }

    static JSObject *
    fromLength(JSContext *cx, uint32_t nelements)
    {
        RootedObject buffer(cx, createBufferWithSizeAndCount(cx, nelements));
        if (!buffer)
            return nullptr;
        return makeInstance(cx, buffer, 0, nelements);
    }

    static JSObject *
    fromArray(JSContext *cx, HandleObject other)
    {
        uint32_t len;
        if (other->is<TypedArrayObject>()) {
            len = other->as<TypedArrayObject>().length();
        } else if (!GetLengthProperty(cx, other, &len)) {
            return nullptr;
        }

        RootedObject bufobj(cx, createBufferWithSizeAndCount(cx, len));
        if (!bufobj)
            return nullptr;

        RootedObject obj(cx, makeInstance(cx, bufobj, 0, len));
        if (!obj || !copyFromArray(cx, obj, other, len))
            return nullptr;
        return obj;
    }

    static const NativeType
    getIndex(JSObject *obj, uint32_t index)
    {
        return *(static_cast<const NativeType*>(obj->as<TypedArrayObject>().viewData()) + index);
    }

    static void
    setIndex(JSObject *obj, uint32_t index, NativeType val)
    {
        *(static_cast<NativeType*>(obj->as<TypedArrayObject>().viewData()) + index) = val;
    }

    static void copyIndexToValue(JSObject *tarray, uint32_t index, MutableHandleValue vp);

    static JSObject *
    createSubarray(JSContext *cx, HandleObject tarrayArg, uint32_t begin, uint32_t end)
    {
        Rooted<TypedArrayObject*> tarray(cx, &tarrayArg->as<TypedArrayObject>());

        JS_ASSERT(begin <= tarray->length());
        JS_ASSERT(end <= tarray->length());

        RootedObject bufobj(cx, tarray->buffer());
        JS_ASSERT(bufobj);

        JS_ASSERT(begin <= end);
        uint32_t length = end - begin;

        JS_ASSERT(begin < UINT32_MAX / sizeof(NativeType));
        uint32_t arrayByteOffset = tarray->byteOffset();
        JS_ASSERT(UINT32_MAX - begin * sizeof(NativeType) >= arrayByteOffset);
        uint32_t byteOffset = arrayByteOffset + begin * sizeof(NativeType);

        return makeInstance(cx, bufobj, byteOffset, length);
    }

  protected:
    static NativeType
    nativeFromDouble(double d)
    {
        if (!ArrayTypeIsFloatingPoint() && JS_UNLIKELY(IsNaN(d)))
            return NativeType(int32_t(0));
        if (TypeIsFloatingPoint<NativeType>())
            return NativeType(d);
        if (TypeIsUnsigned<NativeType>())
            return NativeType(ToUint32(d));
        return NativeType(ToInt32(d));
    }

    static bool
    nativeFromValue(JSContext *cx, const Value &v, NativeType *result)
    {
        if (v.isInt32()) {
            *result = v.toInt32();
            return true;
        }

        if (v.isDouble()) {
            *result = nativeFromDouble(v.toDouble());
            return true;
        }

        /*
         * The condition guarantees that holes and undefined values
         * are treated identically.
         */
        if (v.isPrimitive() && !v.isMagic() && !v.isUndefined()) {
            RootedValue primitive(cx, v);
            double dval;
            // ToNumber will only fail from OOM
            if (!ToNumber(cx, primitive, &dval))
                return false;
            *result = nativeFromDouble(dval);
            return true;
        }

        *result = ArrayTypeIsFloatingPoint()
                  ? NativeType(GenericNaN())
                  : NativeType(int32_t(0));
        return true;
    }

    static bool
    copyFromArray(JSContext *cx, HandleObject thisTypedArrayObj,
                  HandleObject ar, uint32_t len, uint32_t offset = 0)
    {
        Rooted<TypedArrayObject*> thisTypedArray(cx, &thisTypedArrayObj->as<TypedArrayObject>());
        JS_ASSERT(offset <= thisTypedArray->length());
        JS_ASSERT(len <= thisTypedArray->length() - offset);
        if (ar->is<TypedArrayObject>())
            return copyFromTypedArray(cx, thisTypedArray, ar, offset);

        JSRuntime *runtime = cx->runtime();
        uint64_t gcNumber = runtime->gcNumber;

        NativeType *dest = static_cast<NativeType*>(thisTypedArray->viewData()) + offset;
        SkipRoot skipDest(cx, &dest);

        if (ar->is<ArrayObject>() && !ar->isIndexed() && ar->getDenseInitializedLength() >= len) {
            JS_ASSERT(ar->as<ArrayObject>().length() == len);

            /*
             * The only way the code below can GC is if nativeFromValue fails,
             * but in that case we return false immediately, so we do not need
             * to root |src| and |dest|.
             */
            const Value *src = ar->getDenseElements();
            SkipRoot skipSrc(cx, &src);
            for (uint32_t i = 0; i < len; ++i) {
                NativeType n;
                if (!nativeFromValue(cx, src[i], &n))
                    return false;
                dest[i] = n;
            }
            JS_ASSERT(runtime->gcNumber == gcNumber);
        } else {
            RootedValue v(cx);

            for (uint32_t i = 0; i < len; ++i) {
                if (!JSObject::getElement(cx, ar, ar, i, &v))
                    return false;
                NativeType n;
                if (!nativeFromValue(cx, v, &n))
                    return false;

                /*
                 * Detect when a GC has occurred so we can update the dest
                 * pointers in case it has been moved.
                 */
                if (runtime->gcNumber != gcNumber) {
                    dest = static_cast<NativeType*>(thisTypedArray->viewData()) + offset;
                    gcNumber = runtime->gcNumber;
                }
                dest[i] = n;
            }
        }

        return true;
    }

    static bool
    copyFromTypedArray(JSContext *cx, JSObject *thisTypedArrayObj, JSObject *tarrayObj,
                       uint32_t offset)
    {
        TypedArrayObject *thisTypedArray = &thisTypedArrayObj->as<TypedArrayObject>();
        TypedArrayObject *tarray = &tarrayObj->as<TypedArrayObject>();
        JS_ASSERT(offset <= thisTypedArray->length());
        JS_ASSERT(tarray->length() <= thisTypedArray->length() - offset);
        if (tarray->buffer() == thisTypedArray->buffer())
            return copyFromWithOverlap(cx, thisTypedArray, tarray, offset);

        NativeType *dest = static_cast<NativeType*>(thisTypedArray->viewData()) + offset;

        if (tarray->type() == thisTypedArray->type()) {
            js_memcpy(dest, tarray->viewData(), tarray->byteLength());
            return true;
        }

        unsigned srclen = tarray->length();
        switch (tarray->type()) {
          case ScalarTypeRepresentation::TYPE_INT8: {
            int8_t *src = static_cast<int8_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_UINT8:
          case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED: {
            uint8_t *src = static_cast<uint8_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_INT16: {
            int16_t *src = static_cast<int16_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_UINT16: {
            uint16_t *src = static_cast<uint16_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_INT32: {
            int32_t *src = static_cast<int32_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_UINT32: {
            uint32_t *src = static_cast<uint32_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_FLOAT32: {
            float *src = static_cast<float*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_FLOAT64: {
            double *src = static_cast<double*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          default:
            MOZ_ASSUME_UNREACHABLE("copyFrom with a TypedArrayObject of unknown type");
        }

        return true;
    }

    static bool
    copyFromWithOverlap(JSContext *cx, JSObject *selfObj, JSObject *tarrayObj, uint32_t offset)
    {
        TypedArrayObject *self = &selfObj->as<TypedArrayObject>();
        TypedArrayObject *tarray = &tarrayObj->as<TypedArrayObject>();

        JS_ASSERT(offset <= self->length());

        NativeType *dest = static_cast<NativeType*>(self->viewData()) + offset;
        uint32_t byteLength = tarray->byteLength();

        if (tarray->type() == self->type()) {
            memmove(dest, tarray->viewData(), byteLength);
            return true;
        }

        // We have to make a copy of the source array here, since
        // there's overlap, and we have to convert types.
        void *srcbuf = cx->malloc_(byteLength);
        if (!srcbuf)
            return false;
        js_memcpy(srcbuf, tarray->viewData(), byteLength);

        switch (tarray->type()) {
          case ScalarTypeRepresentation::TYPE_INT8: {
            int8_t *src = (int8_t*) srcbuf;
            for (unsigned i = 0; i < tarray->length(); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_UINT8:
          case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED: {
            uint8_t *src = (uint8_t*) srcbuf;
            for (unsigned i = 0; i < tarray->length(); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_INT16: {
            int16_t *src = (int16_t*) srcbuf;
            for (unsigned i = 0; i < tarray->length(); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_UINT16: {
            uint16_t *src = (uint16_t*) srcbuf;
            for (unsigned i = 0; i < tarray->length(); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_INT32: {
            int32_t *src = (int32_t*) srcbuf;
            for (unsigned i = 0; i < tarray->length(); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_UINT32: {
            uint32_t *src = (uint32_t*) srcbuf;
            for (unsigned i = 0; i < tarray->length(); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_FLOAT32: {
            float *src = (float*) srcbuf;
            for (unsigned i = 0; i < tarray->length(); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case ScalarTypeRepresentation::TYPE_FLOAT64: {
            double *src = (double*) srcbuf;
            for (unsigned i = 0; i < tarray->length(); ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          default:
            MOZ_ASSUME_UNREACHABLE("copyFromWithOverlap with a TypedArrayObject of unknown type");
        }

        js_free(srcbuf);
        return true;
    }

    static JSObject *
    createBufferWithSizeAndCount(JSContext *cx, uint32_t count)
    {
        size_t size = sizeof(NativeType);
        if (size != 0 && count >= INT32_MAX / size) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_NEED_DIET, "size and count");
            return nullptr;
        }

        uint32_t bytelen = size * count;
        return ArrayBufferObject::create(cx, bytelen);
    }
};

class Int8ArrayObject : public TypedArrayObjectTemplate<int8_t> {
  public:
    enum { ACTUAL_TYPE = ScalarTypeRepresentation::TYPE_INT8 };
    static const JSProtoKey key = JSProto_Int8Array;
    static const JSFunctionSpec jsfuncs[];
};
class Uint8ArrayObject : public TypedArrayObjectTemplate<uint8_t> {
  public:
    enum { ACTUAL_TYPE = ScalarTypeRepresentation::TYPE_UINT8 };
    static const JSProtoKey key = JSProto_Uint8Array;
    static const JSFunctionSpec jsfuncs[];
};
class Int16ArrayObject : public TypedArrayObjectTemplate<int16_t> {
  public:
    enum { ACTUAL_TYPE = ScalarTypeRepresentation::TYPE_INT16 };
    static const JSProtoKey key = JSProto_Int16Array;
    static const JSFunctionSpec jsfuncs[];
};
class Uint16ArrayObject : public TypedArrayObjectTemplate<uint16_t> {
  public:
    enum { ACTUAL_TYPE = ScalarTypeRepresentation::TYPE_UINT16 };
    static const JSProtoKey key = JSProto_Uint16Array;
    static const JSFunctionSpec jsfuncs[];
};
class Int32ArrayObject : public TypedArrayObjectTemplate<int32_t> {
  public:
    enum { ACTUAL_TYPE = ScalarTypeRepresentation::TYPE_INT32 };
    static const JSProtoKey key = JSProto_Int32Array;
    static const JSFunctionSpec jsfuncs[];
};
class Uint32ArrayObject : public TypedArrayObjectTemplate<uint32_t> {
  public:
    enum { ACTUAL_TYPE = ScalarTypeRepresentation::TYPE_UINT32 };
    static const JSProtoKey key = JSProto_Uint32Array;
    static const JSFunctionSpec jsfuncs[];
};
class Float32ArrayObject : public TypedArrayObjectTemplate<float> {
  public:
    enum { ACTUAL_TYPE = ScalarTypeRepresentation::TYPE_FLOAT32 };
    static const JSProtoKey key = JSProto_Float32Array;
    static const JSFunctionSpec jsfuncs[];
};
class Float64ArrayObject : public TypedArrayObjectTemplate<double> {
  public:
    enum { ACTUAL_TYPE = ScalarTypeRepresentation::TYPE_FLOAT64 };
    static const JSProtoKey key = JSProto_Float64Array;
    static const JSFunctionSpec jsfuncs[];
};
class Uint8ClampedArrayObject : public TypedArrayObjectTemplate<uint8_clamped> {
  public:
    enum { ACTUAL_TYPE = ScalarTypeRepresentation::TYPE_UINT8_CLAMPED };
    static const JSProtoKey key = JSProto_Uint8ClampedArray;
    static const JSFunctionSpec jsfuncs[];
};

} /* anonymous namespace */

template<typename T>
bool
ArrayBufferObject::createTypedArrayFromBufferImpl(JSContext *cx, CallArgs args)
{
    typedef TypedArrayObjectTemplate<T> ArrayType;
    JS_ASSERT(IsArrayBuffer(args.thisv()));
    JS_ASSERT(args.length() == 3);

    Rooted<JSObject*> buffer(cx, &args.thisv().toObject());
    Rooted<JSObject*> proto(cx, &args[2].toObject());

    Rooted<JSObject*> obj(cx);
    double byteOffset = args[0].toNumber();
    MOZ_ASSERT(0 <= byteOffset);
    MOZ_ASSERT(byteOffset <= UINT32_MAX);
    MOZ_ASSERT(byteOffset == uint32_t(byteOffset));
    obj = ArrayType::fromBuffer(cx, buffer, uint32_t(byteOffset), args[1].toInt32(), proto);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

template<typename T>
bool
ArrayBufferObject::createTypedArrayFromBuffer(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsArrayBuffer, createTypedArrayFromBufferImpl<T> >(cx, args);
}

void
ArrayBufferViewObject::prependToViews(ArrayBufferViewObject *viewsHead)
{
    setNextView(viewsHead);

    // Move the multiview buffer list link into this view since we're
    // prepending it to the list.
    setBufferLink(viewsHead->bufferLink());
    viewsHead->setBufferLink(UNSET_BUFFER_LINK);
}

void
ArrayBufferViewObject::neuter()
{
    if (is<DataViewObject>())
        as<DataViewObject>().neuter();
    else
        as<TypedArrayObject>().neuter();
}

// this default implementation is only valid for integer types
// less than 32-bits in size.
template<typename NativeType>
void
TypedArrayObjectTemplate<NativeType>::copyIndexToValue(JSObject *tarray, uint32_t index,
                                                       MutableHandleValue vp)
{
    JS_STATIC_ASSERT(sizeof(NativeType) < 4);

    vp.setInt32(getIndex(tarray, index));
}

namespace {

// and we need to specialize for 32-bit integers and floats
template<>
void
TypedArrayObjectTemplate<int32_t>::copyIndexToValue(JSObject *tarray, uint32_t index,
                                                    MutableHandleValue vp)
{
    int32_t val = getIndex(tarray, index);
    vp.setInt32(val);
}

template<>
void
TypedArrayObjectTemplate<uint32_t>::copyIndexToValue(JSObject *tarray, uint32_t index,
                                                     MutableHandleValue vp)
{
    uint32_t val = getIndex(tarray, index);
    vp.setNumber(val);
}

template<>
void
TypedArrayObjectTemplate<float>::copyIndexToValue(JSObject *tarray, uint32_t index,
                                                  MutableHandleValue vp)
{
    float val = getIndex(tarray, index);
    double dval = val;

    /*
     * Doubles in typed arrays could be typed-punned arrays of integers. This
     * could allow user code to break the engine-wide invariant that only
     * canonical nans are stored into jsvals, which means user code could
     * confuse the engine into interpreting a double-typed jsval as an
     * object-typed jsval.
     *
     * This could be removed for platforms/compilers known to convert a 32-bit
     * non-canonical nan to a 64-bit canonical nan.
     */
    vp.setDouble(JS_CANONICALIZE_NAN(dval));
}

template<>
void
TypedArrayObjectTemplate<double>::copyIndexToValue(JSObject *tarray, uint32_t index,
                                                   MutableHandleValue vp)
{
    double val = getIndex(tarray, index);

    /*
     * Doubles in typed arrays could be typed-punned arrays of integers. This
     * could allow user code to break the engine-wide invariant that only
     * canonical nans are stored into jsvals, which means user code could
     * confuse the engine into interpreting a double-typed jsval as an
     * object-typed jsval.
     */
    vp.setDouble(JS_CANONICALIZE_NAN(val));
}

} /* anonymous namespace */

static NewObjectKind
DataViewNewObjectKind(JSContext *cx, uint32_t byteLength, JSObject *proto)
{
    if (!proto && byteLength >= TypedArrayObject::SINGLETON_TYPE_BYTE_LENGTH)
        return SingletonObject;
    jsbytecode *pc;
    JSScript *script = cx->currentScript(&pc);
    if (!script)
        return GenericObject;
    return types::UseNewTypeForInitializer(script, pc, &DataViewObject::class_);
}

inline DataViewObject *
DataViewObject::create(JSContext *cx, uint32_t byteOffset, uint32_t byteLength,
                       Handle<ArrayBufferObject*> arrayBuffer, JSObject *protoArg)
{
    JS_ASSERT(byteOffset <= INT32_MAX);
    JS_ASSERT(byteLength <= INT32_MAX);

    RootedObject proto(cx, protoArg);
    RootedObject obj(cx);

    NewObjectKind newKind = DataViewNewObjectKind(cx, byteLength, proto);
    obj = NewBuiltinClassInstance(cx, &class_, newKind);
    if (!obj)
        return nullptr;

    if (proto) {
        types::TypeObject *type = cx->getNewType(&class_, TaggedProto(proto));
        if (!type)
            return nullptr;
        obj->setType(type);
    } else if (cx->typeInferenceEnabled()) {
        if (byteLength >= TypedArrayObject::SINGLETON_TYPE_BYTE_LENGTH) {
            JS_ASSERT(obj->hasSingletonType());
        } else {
            jsbytecode *pc;
            RootedScript script(cx, cx->currentScript(&pc));
            if (script) {
                if (!types::SetInitializerObjectType(cx, script, pc, obj, newKind))
                    return nullptr;
            }
        }
    }

    DataViewObject &dvobj = obj->as<DataViewObject>();
    dvobj.setFixedSlot(BYTEOFFSET_SLOT, Int32Value(byteOffset));
    dvobj.setFixedSlot(BYTELENGTH_SLOT, Int32Value(byteLength));
    dvobj.setFixedSlot(BUFFER_SLOT, ObjectValue(*arrayBuffer));
    dvobj.setFixedSlot(NEXT_VIEW_SLOT, PrivateValue(nullptr));
    dvobj.setFixedSlot(NEXT_BUFFER_SLOT, PrivateValue(UNSET_BUFFER_LINK));
    InitArrayBufferViewDataPointer(obj, arrayBuffer, byteOffset);
    JS_ASSERT(byteOffset + byteLength <= arrayBuffer->byteLength());

    // Verify that the private slot is at the expected place
    JS_ASSERT(dvobj.numFixedSlots() == DATA_SLOT);

    arrayBuffer->as<ArrayBufferObject>().addView(&dvobj);

    return &dvobj;
}

bool
DataViewObject::construct(JSContext *cx, JSObject *bufobj, const CallArgs &args, HandleObject proto)
{
    if (!bufobj->is<ArrayBufferObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NOT_EXPECTED_TYPE,
                             "DataView", "ArrayBuffer", bufobj->getClass()->name);
        return false;
    }

    Rooted<ArrayBufferObject*> buffer(cx, &bufobj->as<ArrayBufferObject>());
    uint32_t bufferLength = buffer->byteLength();
    uint32_t byteOffset = 0;
    uint32_t byteLength = bufferLength;

    if (args.length() > 1) {
        if (!ToUint32(cx, args[1], &byteOffset))
            return false;
        if (byteOffset > INT32_MAX) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_ARG_INDEX_OUT_OF_RANGE, "1");
            return false;
        }

        if (args.length() > 2) {
            if (!ToUint32(cx, args[2], &byteLength))
                return false;
            if (byteLength > INT32_MAX) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                     JSMSG_ARG_INDEX_OUT_OF_RANGE, "2");
                return false;
            }
        } else {
            if (byteOffset > bufferLength) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                     JSMSG_ARG_INDEX_OUT_OF_RANGE, "1");
                return false;
            }

            byteLength = bufferLength - byteOffset;
        }
    }

    /* The sum of these cannot overflow a uint32_t */
    JS_ASSERT(byteOffset <= INT32_MAX);
    JS_ASSERT(byteLength <= INT32_MAX);

    if (byteOffset + byteLength > bufferLength) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_ARG_INDEX_OUT_OF_RANGE, "1");
        return false;
    }

    JSObject *obj = DataViewObject::create(cx, byteOffset, byteLength, buffer, proto);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

bool
DataViewObject::class_constructor(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject bufobj(cx);
    if (!GetFirstArgumentAsObject(cx, args, "DataView constructor", &bufobj))
        return false;

    if (bufobj->is<WrapperObject>() && UncheckedUnwrap(bufobj)->is<ArrayBufferObject>()) {
        Rooted<GlobalObject*> global(cx, cx->compartment()->maybeGlobal());
        Rooted<JSObject*> proto(cx, global->getOrCreateDataViewPrototype(cx));
        if (!proto)
            return false;

        InvokeArgs args2(cx);
        if (!args2.init(args.length() + 1))
            return false;
        args2.setCallee(global->createDataViewForThis());
        args2.setThis(ObjectValue(*bufobj));
        PodCopy(args2.array(), args.array(), args.length());
        args2[argc].setObject(*proto);
        if (!Invoke(cx, args2))
            return false;
        args.rval().set(args2.rval());
        return true;
    }

    return construct(cx, bufobj, args, NullPtr());
}

/* static */ bool
DataViewObject::getDataPointer(JSContext *cx, Handle<DataViewObject*> obj,
                               CallArgs args, size_t typeSize, uint8_t **data)
{
    uint32_t offset;
    JS_ASSERT(args.length() > 0);
    if (!ToUint32(cx, args[0], &offset))
        return false;
    if (offset > UINT32_MAX - typeSize || offset + typeSize > obj->byteLength()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_ARG_INDEX_OUT_OF_RANGE, "1");
        return false;
    }

    *data = static_cast<uint8_t*>(obj->dataPointer()) + offset;
    return true;
}

static inline bool
needToSwapBytes(bool littleEndian)
{
#if IS_LITTLE_ENDIAN
    return !littleEndian;
#else
    return littleEndian;
#endif
}

static inline uint8_t
swapBytes(uint8_t x)
{
    return x;
}

static inline uint16_t
swapBytes(uint16_t x)
{
    return ((x & 0xff) << 8) | (x >> 8);
}

static inline uint32_t
swapBytes(uint32_t x)
{
    return ((x & 0xff) << 24) |
           ((x & 0xff00) << 8) |
           ((x & 0xff0000) >> 8) |
           ((x & 0xff000000) >> 24);
}

static inline uint64_t
swapBytes(uint64_t x)
{
    uint32_t a = x & UINT32_MAX;
    uint32_t b = x >> 32;
    return (uint64_t(swapBytes(a)) << 32) | swapBytes(b);
}

template <typename DataType> struct DataToRepType { typedef DataType result; };
template <> struct DataToRepType<int8_t>   { typedef uint8_t result; };
template <> struct DataToRepType<uint8_t>  { typedef uint8_t result; };
template <> struct DataToRepType<int16_t>  { typedef uint16_t result; };
template <> struct DataToRepType<uint16_t> { typedef uint16_t result; };
template <> struct DataToRepType<int32_t>  { typedef uint32_t result; };
template <> struct DataToRepType<uint32_t> { typedef uint32_t result; };
template <> struct DataToRepType<float>    { typedef uint32_t result; };
template <> struct DataToRepType<double>   { typedef uint64_t result; };

template <typename DataType>
struct DataViewIO
{
    typedef typename DataToRepType<DataType>::result ReadWriteType;

    static void fromBuffer(DataType *dest, const uint8_t *unalignedBuffer, bool wantSwap)
    {
        JS_ASSERT((reinterpret_cast<uintptr_t>(dest) & (Min<size_t>(MOZ_ALIGNOF(void*), sizeof(DataType)) - 1)) == 0);
        memcpy((void *) dest, unalignedBuffer, sizeof(ReadWriteType));
        if (wantSwap) {
            ReadWriteType *rwDest = reinterpret_cast<ReadWriteType *>(dest);
            *rwDest = swapBytes(*rwDest);
        }
    }

    static void toBuffer(uint8_t *unalignedBuffer, const DataType *src, bool wantSwap)
    {
        JS_ASSERT((reinterpret_cast<uintptr_t>(src) & (Min<size_t>(MOZ_ALIGNOF(void*), sizeof(DataType)) - 1)) == 0);
        ReadWriteType temp = *reinterpret_cast<const ReadWriteType *>(src);
        if (wantSwap)
            temp = swapBytes(temp);
        memcpy(unalignedBuffer, (void *) &temp, sizeof(ReadWriteType));
    }
};

template<typename NativeType>
/* static */ bool
DataViewObject::read(JSContext *cx, Handle<DataViewObject*> obj,
                     CallArgs &args, NativeType *val, const char *method)
{
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_MORE_ARGS_NEEDED, method, "0", "s");
        return false;
    }

    uint8_t *data;
    if (!getDataPointer(cx, obj, args, sizeof(NativeType), &data))
        return false;

    bool fromLittleEndian = args.length() >= 2 && ToBoolean(args[1]);
    DataViewIO<NativeType>::fromBuffer(val, data, needToSwapBytes(fromLittleEndian));
    return true;
}

template <typename NativeType>
static inline bool
WebIDLCast(JSContext *cx, HandleValue value, NativeType *out)
{
    int32_t temp;
    if (!ToInt32(cx, value, &temp))
        return false;
    // Technically, the behavior of assigning an out of range value to a signed
    // variable is undefined. In practice, compilers seem to do what we want
    // without issuing any warnings.
    *out = static_cast<NativeType>(temp);
    return true;
}

template <>
inline bool
WebIDLCast<float>(JSContext *cx, HandleValue value, float *out)
{
    double temp;
    if (!ToNumber(cx, value, &temp))
        return false;
    *out = static_cast<float>(temp);
    return true;
}

template <>
inline bool
WebIDLCast<double>(JSContext *cx, HandleValue value, double *out)
{
    return ToNumber(cx, value, out);
}

template<typename NativeType>
/* static */ bool
DataViewObject::write(JSContext *cx, Handle<DataViewObject*> obj,
                      CallArgs &args, const char *method)
{
    if (args.length() < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_MORE_ARGS_NEEDED, method, "1", "");
        return false;
    }

    uint8_t *data;
    SkipRoot skipData(cx, &data);
    if (!getDataPointer(cx, obj, args, sizeof(NativeType), &data))
        return false;

    NativeType value;
    if (!WebIDLCast(cx, args[1], &value))
        return false;

    bool toLittleEndian = args.length() >= 3 && ToBoolean(args[2]);
    DataViewIO<NativeType>::toBuffer(data, &value, needToSwapBytes(toLittleEndian));
    return true;
}

bool
DataViewObject::getInt8Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    int8_t val;
    if (!read(cx, thisView, args, &val, "getInt8"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getInt8(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getInt8Impl>(cx, args);
}

bool
DataViewObject::getUint8Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    uint8_t val;
    if (!read(cx, thisView, args, &val, "getUint8"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getUint8(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getUint8Impl>(cx, args);
}

bool
DataViewObject::getInt16Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    int16_t val;
    if (!read(cx, thisView, args, &val, "getInt16"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getInt16(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getInt16Impl>(cx, args);
}

bool
DataViewObject::getUint16Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    uint16_t val;
    if (!read(cx, thisView, args, &val, "getUint16"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getUint16(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getUint16Impl>(cx, args);
}

bool
DataViewObject::getInt32Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    int32_t val;
    if (!read(cx, thisView, args, &val, "getInt32"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getInt32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getInt32Impl>(cx, args);
}

bool
DataViewObject::getUint32Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    uint32_t val;
    if (!read(cx, thisView, args, &val, "getUint32"))
        return false;
    args.rval().setNumber(val);
    return true;
}

bool
DataViewObject::fun_getUint32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getUint32Impl>(cx, args);
}

bool
DataViewObject::getFloat32Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    float val;
    if (!read(cx, thisView, args, &val, "getFloat32"))
        return false;

    args.rval().setDouble(JS_CANONICALIZE_NAN(val));
    return true;
}

bool
DataViewObject::fun_getFloat32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getFloat32Impl>(cx, args);
}

bool
DataViewObject::getFloat64Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    double val;
    if (!read(cx, thisView, args, &val, "getFloat64"))
        return false;

    args.rval().setDouble(JS_CANONICALIZE_NAN(val));
    return true;
}

bool
DataViewObject::fun_getFloat64(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getFloat64Impl>(cx, args);
}

bool
DataViewObject::setInt8Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<int8_t>(cx, thisView, args, "setInt8"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setInt8(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setInt8Impl>(cx, args);
}

bool
DataViewObject::setUint8Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<uint8_t>(cx, thisView, args, "setUint8"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setUint8(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setUint8Impl>(cx, args);
}

bool
DataViewObject::setInt16Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<int16_t>(cx, thisView, args, "setInt16"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setInt16(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setInt16Impl>(cx, args);
}

bool
DataViewObject::setUint16Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<uint16_t>(cx, thisView, args, "setUint16"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setUint16(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setUint16Impl>(cx, args);
}

bool
DataViewObject::setInt32Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<int32_t>(cx, thisView, args, "setInt32"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setInt32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setInt32Impl>(cx, args);
}

bool
DataViewObject::setUint32Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<uint32_t>(cx, thisView, args, "setUint32"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setUint32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setUint32Impl>(cx, args);
}

bool
DataViewObject::setFloat32Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<float>(cx, thisView, args, "setFloat32"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setFloat32(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setFloat32Impl>(cx, args);
}

bool
DataViewObject::setFloat64Impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<double>(cx, thisView, args, "setFloat64"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setFloat64(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setFloat64Impl>(cx, args);
}

void
TypedArrayObject::copyTypedArrayElement(uint32_t index, MutableHandleValue vp)
{
    JS_ASSERT(index < length());

    switch (type()) {
      case ScalarTypeRepresentation::TYPE_INT8:
        TypedArrayObjectTemplate<int8_t>::copyIndexToValue(this, index, vp);
        break;
      case ScalarTypeRepresentation::TYPE_UINT8:
        TypedArrayObjectTemplate<uint8_t>::copyIndexToValue(this, index, vp);
        break;
      case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
        TypedArrayObjectTemplate<uint8_clamped>::copyIndexToValue(this, index, vp);
        break;
      case ScalarTypeRepresentation::TYPE_INT16:
        TypedArrayObjectTemplate<int16_t>::copyIndexToValue(this, index, vp);
        break;
      case ScalarTypeRepresentation::TYPE_UINT16:
        TypedArrayObjectTemplate<uint16_t>::copyIndexToValue(this, index, vp);
        break;
      case ScalarTypeRepresentation::TYPE_INT32:
        TypedArrayObjectTemplate<int32_t>::copyIndexToValue(this, index, vp);
        break;
      case ScalarTypeRepresentation::TYPE_UINT32:
        TypedArrayObjectTemplate<uint32_t>::copyIndexToValue(this, index, vp);
        break;
      case ScalarTypeRepresentation::TYPE_FLOAT32:
        TypedArrayObjectTemplate<float>::copyIndexToValue(this, index, vp);
        break;
      case ScalarTypeRepresentation::TYPE_FLOAT64:
        TypedArrayObjectTemplate<double>::copyIndexToValue(this, index, vp);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unknown TypedArray type");
        break;
    }
}

/***
 *** JS impl
 ***/

/*
 * ArrayBufferObject (base)
 */

const Class ArrayBufferObject::protoClass = {
    "ArrayBufferPrototype",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(ARRAYBUFFER_RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

const Class ArrayBufferObject::class_ = {
    "ArrayBuffer",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_IMPLEMENTS_BARRIERS |
    Class::NON_NATIVE |
    JSCLASS_HAS_RESERVED_SLOTS(ARRAYBUFFER_RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,        /* finalize    */
    nullptr,        /* checkAccess */
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    ArrayBufferObject::obj_trace,
    JS_NULL_CLASS_EXT,
    {
        ArrayBufferObject::obj_lookupGeneric,
        ArrayBufferObject::obj_lookupProperty,
        ArrayBufferObject::obj_lookupElement,
        ArrayBufferObject::obj_lookupSpecial,
        ArrayBufferObject::obj_defineGeneric,
        ArrayBufferObject::obj_defineProperty,
        ArrayBufferObject::obj_defineElement,
        ArrayBufferObject::obj_defineSpecial,
        ArrayBufferObject::obj_getGeneric,
        ArrayBufferObject::obj_getProperty,
        ArrayBufferObject::obj_getElement,
        ArrayBufferObject::obj_getElementIfPresent,
        ArrayBufferObject::obj_getSpecial,
        ArrayBufferObject::obj_setGeneric,
        ArrayBufferObject::obj_setProperty,
        ArrayBufferObject::obj_setElement,
        ArrayBufferObject::obj_setSpecial,
        ArrayBufferObject::obj_getGenericAttributes,
        ArrayBufferObject::obj_setGenericAttributes,
        ArrayBufferObject::obj_deleteProperty,
        ArrayBufferObject::obj_deleteElement,
        ArrayBufferObject::obj_deleteSpecial,
        ArrayBufferObject::obj_enumerate,
        nullptr,       /* thisObject      */
    }
};

const JSFunctionSpec ArrayBufferObject::jsfuncs[] = {
    JS_FN("slice", ArrayBufferObject::fun_slice, 2, JSFUN_GENERIC_NATIVE),
    JS_FS_END
};

/*
 * TypedArrayObject boilerplate
 */

#ifndef RELEASE_BUILD
# define IMPL_TYPED_ARRAY_STATICS(_typedArray)                                     \
const JSFunctionSpec _typedArray##Object::jsfuncs[] = {                            \
    JS_FN("iterator", JS_ArrayIterator, 0, 0),                                     \
    JS_FN("subarray", _typedArray##Object::fun_subarray, 2, JSFUN_GENERIC_NATIVE), \
    JS_FN("set", _typedArray##Object::fun_set, 2, JSFUN_GENERIC_NATIVE),           \
    JS_FN("move", _typedArray##Object::fun_move, 3, JSFUN_GENERIC_NATIVE),         \
    JS_FS_END                                                                      \
}
#else
# define IMPL_TYPED_ARRAY_STATICS(_typedArray)                                     \
const JSFunctionSpec _typedArray##Object::jsfuncs[] = {                            \
    JS_FN("iterator", JS_ArrayIterator, 0, 0),                                     \
    JS_FN("subarray", _typedArray##Object::fun_subarray, 2, JSFUN_GENERIC_NATIVE), \
    JS_FN("set", _typedArray##Object::fun_set, 2, JSFUN_GENERIC_NATIVE),           \
    JS_FS_END                                                                      \
}
#endif

#define IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Name,NativeType)                                 \
  JS_FRIEND_API(JSObject *) JS_New ## Name ## Array(JSContext *cx, uint32_t nelements)       \
  {                                                                                          \
      return TypedArrayObjectTemplate<NativeType>::fromLength(cx, nelements);                \
  }                                                                                          \
  JS_FRIEND_API(JSObject *) JS_New ## Name ## ArrayFromArray(JSContext *cx, JSObject *other_)\
  {                                                                                          \
      Rooted<JSObject*> other(cx, other_);                                                   \
      return TypedArrayObjectTemplate<NativeType>::fromArray(cx, other);                     \
  }                                                                                          \
  JS_FRIEND_API(JSObject *) JS_New ## Name ## ArrayWithBuffer(JSContext *cx,                 \
                               JSObject *arrayBuffer_, uint32_t byteOffset, int32_t length)  \
  {                                                                                          \
      Rooted<JSObject*> arrayBuffer(cx, arrayBuffer_);                                       \
      Rooted<JSObject*> proto(cx, nullptr);                                                  \
      return TypedArrayObjectTemplate<NativeType>::fromBuffer(cx, arrayBuffer, byteOffset,   \
                                                              length, proto);                \
  }                                                                                          \
  JS_FRIEND_API(bool) JS_Is ## Name ## Array(JSObject *obj)                                  \
  {                                                                                          \
      if (!(obj = CheckedUnwrap(obj)))                                                       \
          return false;                                                                      \
      const Class *clasp = obj->getClass();                                                  \
      return (clasp == &TypedArrayObject::classes[TypedArrayObjectTemplate<NativeType>::ArrayTypeID()]); \
  }

IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Int8, int8_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Uint8, uint8_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Uint8Clamped, uint8_clamped)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Int16, int16_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Uint16, uint16_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Int32, int32_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Uint32, uint32_t)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Float32, float)
IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Float64, double)

#define IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Name, ExternalType, InternalType)              \
  JS_FRIEND_API(JSObject *) JS_GetObjectAs ## Name ## Array(JSObject *obj,                  \
                                                            uint32_t *length,               \
                                                            ExternalType **data)            \
  {                                                                                         \
      if (!(obj = CheckedUnwrap(obj)))                                                      \
          return nullptr;                                                                   \
                                                                                            \
      const Class *clasp = obj->getClass();                                                 \
      if (clasp != &TypedArrayObject::classes[TypedArrayObjectTemplate<InternalType>::ArrayTypeID()]) \
          return nullptr;                                                                   \
                                                                                            \
      TypedArrayObject *tarr = &obj->as<TypedArrayObject>();                                \
      *length = tarr->length();                                                             \
      *data = static_cast<ExternalType *>(tarr->viewData());                                \
                                                                                            \
      return obj;                                                                           \
  }

IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Int8, int8_t, int8_t)
IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Uint8, uint8_t, uint8_t)
IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Uint8Clamped, uint8_t, uint8_clamped)
IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Int16, int16_t, int16_t)
IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Uint16, uint16_t, uint16_t)
IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Int32, int32_t, int32_t)
IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Uint32, uint32_t, uint32_t)
IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Float32, float, float)
IMPL_TYPED_ARRAY_COMBINED_UNWRAPPERS(Float64, double, double)

#define IMPL_TYPED_ARRAY_PROTO_CLASS(_typedArray)                              \
{                                                                              \
    #_typedArray "Prototype",                                                  \
    JSCLASS_HAS_RESERVED_SLOTS(TypedArrayObject::RESERVED_SLOTS) |             \
    JSCLASS_HAS_PRIVATE |                                                      \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##_typedArray),                           \
    JS_PropertyStub,         /* addProperty */                                 \
    JS_DeletePropertyStub,   /* delProperty */                                 \
    JS_PropertyStub,         /* getProperty */                                 \
    JS_StrictPropertyStub,   /* setProperty */                                 \
    JS_EnumerateStub,                                                          \
    JS_ResolveStub,                                                            \
    JS_ConvertStub                                                             \
}

#define IMPL_TYPED_ARRAY_FAST_CLASS(_typedArray)                               \
{                                                                              \
    #_typedArray,                                                              \
    JSCLASS_HAS_RESERVED_SLOTS(TypedArrayObject::RESERVED_SLOTS) |             \
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |                        \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##_typedArray) |                          \
    Class::NON_NATIVE,                                                         \
    JS_PropertyStub,         /* addProperty */                                 \
    JS_DeletePropertyStub,   /* delProperty */                                 \
    JS_PropertyStub,         /* getProperty */                                 \
    JS_StrictPropertyStub,   /* setProperty */                                 \
    JS_EnumerateStub,                                                          \
    JS_ResolveStub,                                                            \
    JS_ConvertStub,                                                            \
    nullptr,                 /* finalize */                                    \
    nullptr,                 /* checkAccess */                                 \
    nullptr,                 /* call        */                                 \
    nullptr,                 /* hasInstance */                                 \
    nullptr,                 /* construct   */                                 \
    ArrayBufferViewObject::trace, /* trace  */                                 \
    {                                                                          \
        nullptr,    /* outerObject */                                          \
        nullptr,    /* innerObject */                                          \
        nullptr,    /* iteratorObject  */                                      \
        false,      /* isWrappedNative */                                      \
    },                                                                         \
    {                                                                          \
        _typedArray##Object::obj_lookupGeneric,                                \
        _typedArray##Object::obj_lookupProperty,                               \
        _typedArray##Object::obj_lookupElement,                                \
        _typedArray##Object::obj_lookupSpecial,                                \
        _typedArray##Object::obj_defineGeneric,                                \
        _typedArray##Object::obj_defineProperty,                               \
        _typedArray##Object::obj_defineElement,                                \
        _typedArray##Object::obj_defineSpecial,                                \
        _typedArray##Object::obj_getGeneric,                                   \
        _typedArray##Object::obj_getProperty,                                  \
        _typedArray##Object::obj_getElement,                                   \
        _typedArray##Object::obj_getElementIfPresent,                          \
        _typedArray##Object::obj_getSpecial,                                   \
        _typedArray##Object::obj_setGeneric,                                   \
        _typedArray##Object::obj_setProperty,                                  \
        _typedArray##Object::obj_setElement,                                   \
        _typedArray##Object::obj_setSpecial,                                   \
        _typedArray##Object::obj_getGenericAttributes,                         \
        _typedArray##Object::obj_setGenericAttributes,                         \
        _typedArray##Object::obj_deleteProperty,                               \
        _typedArray##Object::obj_deleteElement,                                \
        _typedArray##Object::obj_deleteSpecial,                                \
        _typedArray##Object::obj_enumerate,                                    \
        nullptr,             /* thisObject  */                                 \
    }                                                                          \
}

template<class ArrayType>
static inline JSObject *
InitTypedArrayClass(JSContext *cx)
{
    Rooted<GlobalObject*> global(cx, cx->compartment()->maybeGlobal());
    RootedObject proto(cx, global->createBlankPrototype(cx, ArrayType::protoClass()));
    if (!proto)
        return nullptr;

    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, ArrayType::class_constructor,
                                     ClassName(ArrayType::key, cx), 3);
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    RootedValue bytesValue(cx, Int32Value(ArrayType::BYTES_PER_ELEMENT));

    if (!JSObject::defineProperty(cx, ctor,
                                  cx->names().BYTES_PER_ELEMENT, bytesValue,
                                  JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_PERMANENT | JSPROP_READONLY) ||
        !JSObject::defineProperty(cx, proto,
                                  cx->names().BYTES_PER_ELEMENT, bytesValue,
                                  JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_PERMANENT | JSPROP_READONLY))
    {
        return nullptr;
    }

    if (!ArrayType::defineGetters(cx, proto))
        return nullptr;

    if (!JS_DefineFunctions(cx, proto, ArrayType::jsfuncs))
        return nullptr;

    RootedFunction fun(cx);
    fun =
        NewFunction(cx, NullPtr(),
                    ArrayBufferObject::createTypedArrayFromBuffer<typename ArrayType::ThisType>,
                    0, JSFunction::NATIVE_FUN, global, NullPtr());
    if (!fun)
        return nullptr;

    if (!DefineConstructorAndPrototype(cx, global, ArrayType::key, ctor, proto))
        return nullptr;

    global->setCreateArrayFromBuffer<typename ArrayType::ThisType>(fun);

    return proto;
}

IMPL_TYPED_ARRAY_STATICS(Int8Array);
IMPL_TYPED_ARRAY_STATICS(Uint8Array);
IMPL_TYPED_ARRAY_STATICS(Int16Array);
IMPL_TYPED_ARRAY_STATICS(Uint16Array);
IMPL_TYPED_ARRAY_STATICS(Int32Array);
IMPL_TYPED_ARRAY_STATICS(Uint32Array);
IMPL_TYPED_ARRAY_STATICS(Float32Array);
IMPL_TYPED_ARRAY_STATICS(Float64Array);
IMPL_TYPED_ARRAY_STATICS(Uint8ClampedArray);

const Class TypedArrayObject::classes[ScalarTypeRepresentation::TYPE_MAX] = {
    IMPL_TYPED_ARRAY_FAST_CLASS(Int8Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Uint8Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Int16Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Uint16Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Int32Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Uint32Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Float32Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Float64Array),
    IMPL_TYPED_ARRAY_FAST_CLASS(Uint8ClampedArray)
};

const Class TypedArrayObject::protoClasses[ScalarTypeRepresentation::TYPE_MAX] = {
    IMPL_TYPED_ARRAY_PROTO_CLASS(Int8Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint8Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Int16Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint16Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Int32Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint32Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Float32Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Float64Array),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint8ClampedArray)
};

#define CHECK(t, a) { if (t == a::IsThisClass) return true; }
JS_FRIEND_API(bool)
js::IsTypedArrayThisCheck(JS::IsAcceptableThis test)
{
    CHECK(test, Int8ArrayObject);
    CHECK(test, Uint8ArrayObject);
    CHECK(test, Int16ArrayObject);
    CHECK(test, Uint16ArrayObject);
    CHECK(test, Int32ArrayObject);
    CHECK(test, Uint32ArrayObject);
    CHECK(test, Float32ArrayObject);
    CHECK(test, Float64ArrayObject);
    CHECK(test, Uint8ClampedArrayObject);
    return false;
}
#undef CHECK

static JSObject *
InitArrayBufferClass(JSContext *cx)
{
    Rooted<GlobalObject*> global(cx, cx->compartment()->maybeGlobal());
    RootedObject arrayBufferProto(cx, global->createBlankPrototype(cx, &ArrayBufferObject::protoClass));
    if (!arrayBufferProto)
        return nullptr;

    RootedFunction ctor(cx, global->createConstructor(cx, ArrayBufferObject::class_constructor,
                                                      cx->names().ArrayBuffer, 1));
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, arrayBufferProto))
        return nullptr;

    RootedId byteLengthId(cx, NameToId(cx->names().byteLength));
    unsigned flags = JSPROP_SHARED | JSPROP_GETTER | JSPROP_PERMANENT;
    JSObject *getter = NewFunction(cx, NullPtr(), ArrayBufferObject::byteLengthGetter, 0,
                                   JSFunction::NATIVE_FUN, global, NullPtr());
    if (!getter)
        return nullptr;

    RootedValue value(cx, UndefinedValue());
    if (!DefineNativeProperty(cx, arrayBufferProto, byteLengthId, value,
                              JS_DATA_TO_FUNC_PTR(PropertyOp, getter), nullptr, flags, 0, 0))
        return nullptr;

    if (!JS_DefineFunctions(cx, arrayBufferProto, ArrayBufferObject::jsfuncs))
        return nullptr;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_ArrayBuffer, ctor, arrayBufferProto))
        return nullptr;

    return arrayBufferProto;
}

const Class DataViewObject::protoClass = {
    "DataViewPrototype",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(DataViewObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_DataView),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

const Class DataViewObject::class_ = {
    "DataView",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_IMPLEMENTS_BARRIERS |
    /* Bug 886622: Consider making this Class NON_NATIVE. */
    JSCLASS_HAS_RESERVED_SLOTS(DataViewObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_DataView),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,                 /* finalize */
    nullptr,                 /* checkAccess */
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    ArrayBufferViewObject::trace, /* trace  */
    JS_NULL_CLASS_EXT,
    JS_NULL_OBJECT_OPS
};

const JSFunctionSpec DataViewObject::jsfuncs[] = {
    JS_FN("getInt8",    DataViewObject::fun_getInt8,      1,0),
    JS_FN("getUint8",   DataViewObject::fun_getUint8,     1,0),
    JS_FN("getInt16",   DataViewObject::fun_getInt16,     2,0),
    JS_FN("getUint16",  DataViewObject::fun_getUint16,    2,0),
    JS_FN("getInt32",   DataViewObject::fun_getInt32,     2,0),
    JS_FN("getUint32",  DataViewObject::fun_getUint32,    2,0),
    JS_FN("getFloat32", DataViewObject::fun_getFloat32,   2,0),
    JS_FN("getFloat64", DataViewObject::fun_getFloat64,   2,0),
    JS_FN("setInt8",    DataViewObject::fun_setInt8,      2,0),
    JS_FN("setUint8",   DataViewObject::fun_setUint8,     2,0),
    JS_FN("setInt16",   DataViewObject::fun_setInt16,     3,0),
    JS_FN("setUint16",  DataViewObject::fun_setUint16,    3,0),
    JS_FN("setInt32",   DataViewObject::fun_setInt32,     3,0),
    JS_FN("setUint32",  DataViewObject::fun_setUint32,    3,0),
    JS_FN("setFloat32", DataViewObject::fun_setFloat32,   3,0),
    JS_FN("setFloat64", DataViewObject::fun_setFloat64,   3,0),
    JS_FS_END
};

template<Value ValueGetter(DataViewObject *view)>
bool
DataViewObject::getterImpl(JSContext *cx, CallArgs args)
{
    args.rval().set(ValueGetter(&args.thisv().toObject().as<DataViewObject>()));
    return true;
}

template<Value ValueGetter(DataViewObject *view)>
bool
DataViewObject::getter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getterImpl<ValueGetter> >(cx, args);
}

template<Value ValueGetter(DataViewObject *view)>
bool
DataViewObject::defineGetter(JSContext *cx, PropertyName *name, HandleObject proto)
{
    RootedId id(cx, NameToId(name));
    unsigned flags = JSPROP_SHARED | JSPROP_GETTER | JSPROP_PERMANENT;

    Rooted<GlobalObject*> global(cx, cx->compartment()->maybeGlobal());
    JSObject *getter = NewFunction(cx, NullPtr(), DataViewObject::getter<ValueGetter>, 0,
                                   JSFunction::NATIVE_FUN, global, NullPtr());
    if (!getter)
        return false;

    RootedValue value(cx, UndefinedValue());
    return DefineNativeProperty(cx, proto, id, value,
                                JS_DATA_TO_FUNC_PTR(PropertyOp, getter), nullptr,
                                flags, 0, 0);
}

/* static */ JSObject *
DataViewObject::initClass(JSContext *cx)
{
    Rooted<GlobalObject*> global(cx, cx->compartment()->maybeGlobal());
    RootedObject proto(cx, global->createBlankPrototype(cx, &DataViewObject::protoClass));
    if (!proto)
        return nullptr;

    RootedFunction ctor(cx, global->createConstructor(cx, DataViewObject::class_constructor,
                                                      cx->names().DataView, 3));
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    if (!defineGetter<bufferValue>(cx, cx->names().buffer, proto))
        return nullptr;

    if (!defineGetter<byteLengthValue>(cx, cx->names().byteLength, proto))
        return nullptr;

    if (!defineGetter<byteOffsetValue>(cx, cx->names().byteOffset, proto))
        return nullptr;

    if (!JS_DefineFunctions(cx, proto, DataViewObject::jsfuncs))
        return nullptr;

    /*
     * Create a helper function to implement the craziness of
     * |new DataView(new otherWindow.ArrayBuffer())|, and install it in the
     * global for use by the DataViewObject constructor.
     */
    RootedFunction fun(cx, NewFunction(cx, NullPtr(), ArrayBufferObject::createDataViewForThis,
                                       0, JSFunction::NATIVE_FUN, global, NullPtr()));
    if (!fun)
        return nullptr;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_DataView, ctor, proto))
        return nullptr;

    global->setCreateDataViewForThis(fun);

    return proto;
}

void
DataViewObject::neuter()
{
    setSlot(BYTELENGTH_SLOT, Int32Value(0));
    setSlot(BYTEOFFSET_SLOT, Int32Value(0));
    setPrivate(nullptr);
}

JSObject *
js_InitTypedArrayClasses(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

    /* Idempotency required: we initialize several things, possibly lazily. */
    RootedObject stop(cx);
    if (!js_GetClassObject(cx, global, JSProto_ArrayBuffer, &stop))
        return nullptr;
    if (stop)
        return stop;

    if (!InitTypedArrayClass<Int8ArrayObject>(cx) ||
        !InitTypedArrayClass<Uint8ArrayObject>(cx) ||
        !InitTypedArrayClass<Int16ArrayObject>(cx) ||
        !InitTypedArrayClass<Uint16ArrayObject>(cx) ||
        !InitTypedArrayClass<Int32ArrayObject>(cx) ||
        !InitTypedArrayClass<Uint32ArrayObject>(cx) ||
        !InitTypedArrayClass<Float32ArrayObject>(cx) ||
        !InitTypedArrayClass<Float64ArrayObject>(cx) ||
        !InitTypedArrayClass<Uint8ClampedArrayObject>(cx) ||
        !DataViewObject::initClass(cx))
    {
        return nullptr;
    }

    return InitArrayBufferClass(cx);
}

bool
js::IsTypedArrayConstructor(HandleValue v, uint32_t type)
{
    switch (type) {
      case ScalarTypeRepresentation::TYPE_INT8:
        return IsNativeFunction(v, Int8ArrayObject::class_constructor);
      case ScalarTypeRepresentation::TYPE_UINT8:
        return IsNativeFunction(v, Uint8ArrayObject::class_constructor);
      case ScalarTypeRepresentation::TYPE_INT16:
        return IsNativeFunction(v, Int16ArrayObject::class_constructor);
      case ScalarTypeRepresentation::TYPE_UINT16:
        return IsNativeFunction(v, Uint16ArrayObject::class_constructor);
      case ScalarTypeRepresentation::TYPE_INT32:
        return IsNativeFunction(v, Int32ArrayObject::class_constructor);
      case ScalarTypeRepresentation::TYPE_UINT32:
        return IsNativeFunction(v, Uint32ArrayObject::class_constructor);
      case ScalarTypeRepresentation::TYPE_FLOAT32:
        return IsNativeFunction(v, Float32ArrayObject::class_constructor);
      case ScalarTypeRepresentation::TYPE_FLOAT64:
        return IsNativeFunction(v, Float64ArrayObject::class_constructor);
      case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
        return IsNativeFunction(v, Uint8ClampedArrayObject::class_constructor);
    }
    MOZ_ASSUME_UNREACHABLE("unexpected typed array type");
}

bool
js::IsTypedArrayBuffer(HandleValue v)
{
    return v.isObject() && v.toObject().is<ArrayBufferObject>();
}

/* JS Friend API */

JS_FRIEND_API(bool)
JS_IsArrayBufferObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<ArrayBufferObject>() : false;
}

JS_FRIEND_API(bool)
JS_IsTypedArrayObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<TypedArrayObject>() : false;
}

JS_FRIEND_API(bool)
JS_IsArrayBufferViewObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<ArrayBufferViewObject>() : false;
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferByteLength(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->as<ArrayBufferObject>().byteLength() : 0;
}

JS_FRIEND_API(uint8_t *)
JS_GetArrayBufferData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    ArrayBufferObject &buffer = obj->as<ArrayBufferObject>();
    if (!buffer.uninlineData(nullptr))
        return nullptr;
    return buffer.dataPointer();
}

JS_FRIEND_API(JSObject *)
JS_NewArrayBuffer(JSContext *cx, uint32_t nbytes)
{
    JS_ASSERT(nbytes <= INT32_MAX);
    return ArrayBufferObject::create(cx, nbytes);
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayBufferWithContents(JSContext *cx, void *contents)
{
    JS_ASSERT(contents);
    JSObject *obj = ArrayBufferObject::create(cx, 0);
    if (!obj)
        return nullptr;
    obj->setDynamicElements(reinterpret_cast<js::ObjectElements *>(contents));
    JS_ASSERT(GetViewList(&obj->as<ArrayBufferObject>()) == nullptr);
    return obj;
}

JS_PUBLIC_API(bool)
JS_AllocateArrayBufferContents(JSContext *cx, uint32_t nbytes, void **contents, uint8_t **data)
{
    js::ObjectElements *header = AllocateArrayBufferContents(cx, nbytes, nullptr);
    if (!header)
        return false;

    ArrayBufferObject::setElementsHeader(header, nbytes);

    *contents = header;
    *data = reinterpret_cast<uint8_t*>(header->elements());
    return true;
}

JS_PUBLIC_API(bool)
JS_ReallocateArrayBufferContents(JSContext *cx, uint32_t nbytes, void **contents, uint8_t **data)
{
    js::ObjectElements *header = AllocateArrayBufferContents(cx, nbytes, nullptr, *contents);
    if (!header)
        return false;

    ArrayBufferObject::setElementsHeader(header, nbytes);

    *contents = header;
    *data = reinterpret_cast<uint8_t*>(header->elements());
    return true;
}

JS_PUBLIC_API(bool)
JS_StealArrayBufferContents(JSContext *cx, JSObject *obj, void **contents,
                            uint8_t **data)
{
    if (!(obj = CheckedUnwrap(obj)))
        return false;

    if (!obj->is<ArrayBufferObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }

    if (!ArrayBufferObject::stealContents(cx, obj, contents, data))
        return false;

    return true;
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayLength(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<TypedArrayObject>().length();
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteOffset(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<TypedArrayObject>().byteOffset();
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteLength(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<TypedArrayObject>().byteLength();
}

JS_FRIEND_API(JSArrayBufferViewType)
JS_GetArrayBufferViewType(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return ArrayBufferView::TYPE_MAX;

    if (obj->is<TypedArrayObject>())
        return static_cast<JSArrayBufferViewType>(obj->as<TypedArrayObject>().type());
    else if (obj->is<DataViewObject>())
        return ArrayBufferView::TYPE_DATAVIEW;
    MOZ_ASSUME_UNREACHABLE("invalid ArrayBufferView type");
}

JS_FRIEND_API(int8_t *)
JS_GetInt8ArrayData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT(tarr->type() == ArrayBufferView::TYPE_INT8);
    return static_cast<int8_t *>(tarr->viewData());
}

JS_FRIEND_API(uint8_t *)
JS_GetUint8ArrayData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT(tarr->type() == ArrayBufferView::TYPE_UINT8);
    return static_cast<uint8_t *>(tarr->viewData());
}

JS_FRIEND_API(uint8_t *)
JS_GetUint8ClampedArrayData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT(tarr->type() == ArrayBufferView::TYPE_UINT8_CLAMPED);
    return static_cast<uint8_t *>(tarr->viewData());
}

JS_FRIEND_API(int16_t *)
JS_GetInt16ArrayData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT(tarr->type() == ArrayBufferView::TYPE_INT16);
    return static_cast<int16_t *>(tarr->viewData());
}

JS_FRIEND_API(uint16_t *)
JS_GetUint16ArrayData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT(tarr->type() == ArrayBufferView::TYPE_UINT16);
    return static_cast<uint16_t *>(tarr->viewData());
}

JS_FRIEND_API(int32_t *)
JS_GetInt32ArrayData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT(tarr->type() == ArrayBufferView::TYPE_INT32);
    return static_cast<int32_t *>(tarr->viewData());
}

JS_FRIEND_API(uint32_t *)
JS_GetUint32ArrayData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT(tarr->type() == ArrayBufferView::TYPE_UINT32);
    return static_cast<uint32_t *>(tarr->viewData());
}

JS_FRIEND_API(float *)
JS_GetFloat32ArrayData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT(tarr->type() == ArrayBufferView::TYPE_FLOAT32);
    return static_cast<float *>(tarr->viewData());
}

JS_FRIEND_API(double *)
JS_GetFloat64ArrayData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject *tarr = &obj->as<TypedArrayObject>();
    JS_ASSERT(tarr->type() == ArrayBufferView::TYPE_FLOAT64);
    return static_cast<double *>(tarr->viewData());
}

JS_FRIEND_API(bool)
JS_IsDataViewObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<DataViewObject>() : false;
}

JS_FRIEND_API(uint32_t)
JS_GetDataViewByteOffset(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<DataViewObject>().byteOffset();
}

JS_FRIEND_API(void *)
JS_GetDataViewData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    return obj->as<DataViewObject>().dataPointer();
}

JS_FRIEND_API(uint32_t)
JS_GetDataViewByteLength(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<DataViewObject>().byteLength();
}

JS_FRIEND_API(void *)
JS_GetArrayBufferViewData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    return obj->is<DataViewObject>() ? obj->as<DataViewObject>().dataPointer()
                                     : obj->as<TypedArrayObject>().viewData();
}

JS_FRIEND_API(JSObject *)
JS_GetArrayBufferViewBuffer(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    return obj->as<ArrayBufferViewObject>().bufferObject();
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferViewByteLength(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->is<DataViewObject>()
           ? obj->as<DataViewObject>().byteLength()
           : obj->as<TypedArrayObject>().byteLength();
}

JS_FRIEND_API(JSObject *)
JS_GetObjectAsArrayBufferView(JSObject *obj, uint32_t *length, uint8_t **data)
{
    if (!(obj = CheckedUnwrap(obj)))
        return nullptr;
    if (!(obj->is<ArrayBufferViewObject>()))
        return nullptr;

    *length = obj->is<DataViewObject>()
              ? obj->as<DataViewObject>().byteLength()
              : obj->as<TypedArrayObject>().byteLength();

    *data = static_cast<uint8_t*>(obj->is<DataViewObject>()
                                  ? obj->as<DataViewObject>().dataPointer()
                                  : obj->as<TypedArrayObject>().viewData());
    return obj;
}

JS_FRIEND_API(JSObject *)
JS_GetObjectAsArrayBuffer(JSObject *obj, uint32_t *length, uint8_t **data)
{
   if (!(obj = CheckedUnwrap(obj)))
       return nullptr;
    if (!obj->is<ArrayBufferObject>())
        return nullptr;

    *length = obj->as<ArrayBufferObject>().byteLength();
    *data = obj->as<ArrayBufferObject>().dataPointer();

    return obj;
}
