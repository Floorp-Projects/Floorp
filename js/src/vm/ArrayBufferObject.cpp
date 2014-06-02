/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ArrayBufferObject.h"

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
#include "jit/AsmJSModule.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/NumericConversions.h"
#include "vm/SharedArrayObject.h"
#include "vm/WrapperObject.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/Shape-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

/*
 * Allocate array buffers with the maximum number of fixed slots marked as
 * reserved, so that the fixed slots may be used for the buffer's contents.
 * The last fixed slot is kept for the object's private data.
 */
static const uint8_t ARRAYBUFFER_RESERVED_SLOTS = JSObject::MAX_FIXED_SLOTS - 1;

// Sentinel value used to initialize ArrayBufferViewObjects' NEXT_BUFFER_SLOTs
// to show that they have not yet been added to any ArrayBufferObject list.
js::ArrayBufferObject * const js::UNSET_BUFFER_LINK = reinterpret_cast<js::ArrayBufferObject*>(0x2);

/*
 * Convert |v| to an array index for an array of length |length| per
 * the Typed Array Specification section 7.0, |subarray|. If successful,
 * the output value is in the range [0, length].
 */
bool
js::ToClampedIndex(JSContext *cx, HandleValue v, uint32_t length, uint32_t *out)
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
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    ArrayBufferObject::obj_trace,
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    {
        ArrayBufferObject::obj_lookupGeneric,
        ArrayBufferObject::obj_lookupProperty,
        ArrayBufferObject::obj_lookupElement,
        ArrayBufferObject::obj_defineGeneric,
        ArrayBufferObject::obj_defineProperty,
        ArrayBufferObject::obj_defineElement,
        ArrayBufferObject::obj_getGeneric,
        ArrayBufferObject::obj_getProperty,
        ArrayBufferObject::obj_getElement,
        ArrayBufferObject::obj_setGeneric,
        ArrayBufferObject::obj_setProperty,
        ArrayBufferObject::obj_setElement,
        ArrayBufferObject::obj_getGenericAttributes,
        ArrayBufferObject::obj_setGenericAttributes,
        ArrayBufferObject::obj_deleteProperty,
        ArrayBufferObject::obj_deleteElement,
        nullptr, nullptr, /* watch/unwatch */
        nullptr,          /* slice */
        ArrayBufferObject::obj_enumerate,
        nullptr,          /* thisObject      */
    }
};

const JSFunctionSpec ArrayBufferObject::jsfuncs[] = {
    JS_FN("slice", ArrayBufferObject::fun_slice, 2, JSFUN_GENERIC_NATIVE),
    JS_FS_END
};

const JSFunctionSpec ArrayBufferObject::jsstaticfuncs[] = {
    JS_FN("isView", ArrayBufferObject::fun_isView, 1, 0),
    JS_FS_END
};

bool
js::IsArrayBuffer(HandleValue v)
{
    return v.isObject() &&
           (v.toObject().is<ArrayBufferObject>() ||
            v.toObject().is<SharedArrayBufferObject>());
}

bool
js::IsArrayBuffer(HandleObject obj)
{
    return obj->is<ArrayBufferObject>() || obj->is<SharedArrayBufferObject>();
}

bool
js::IsArrayBuffer(JSObject *obj)
{
    return obj->is<ArrayBufferObject>() || obj->is<SharedArrayBufferObject>();
}

ArrayBufferObject &
js::AsArrayBuffer(HandleObject obj)
{
    JS_ASSERT(IsArrayBuffer(obj));
    if (obj->is<SharedArrayBufferObject>())
        return obj->as<SharedArrayBufferObject>();
    return obj->as<ArrayBufferObject>();
}

ArrayBufferObject &
js::AsArrayBuffer(JSObject *obj)
{
    JS_ASSERT(IsArrayBuffer(obj));
    if (obj->is<SharedArrayBufferObject>())
        return obj->as<SharedArrayBufferObject>();
    return obj->as<ArrayBufferObject>();
}

MOZ_ALWAYS_INLINE bool
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

    Rooted<ArrayBufferObject*> thisObj(cx, &args.thisv().toObject().as<ArrayBufferObject>());

    // these are the default values
    uint32_t length = thisObj->byteLength();
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

    JSObject *nobj = createSlice(cx, thisObj, begin, end);
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
 * ArrayBuffer.isView(obj); ES6 (Dec 2013 draft) 24.1.3.1
 */
bool
ArrayBufferObject::fun_isView(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(args.get(0).isObject() &&
                           JS_IsArrayBufferViewObject(&args.get(0).toObject()));
    return true;
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
AllocateArrayBufferContents(JSContext *maybecx, uint32_t nbytes, void *oldptr = nullptr)
{
    uint32_t size = nbytes + sizeof(ObjectElements);
    JS_ASSERT(size > nbytes); // be wary of rollover
    ObjectElements *newheader;

    // if oldptr is given, then we need to do a realloc
    if (oldptr) {
        ObjectElements *oldheader = static_cast<ObjectElements *>(oldptr);
        uint32_t oldnbytes = ArrayBufferObject::headerInitializedLength(oldheader);

        void *p = maybecx ? maybecx->runtime()->reallocCanGC(oldptr, size) : js_realloc(oldptr, size);
        newheader = static_cast<ObjectElements *>(p);

        // if we grew the array, we need to set the new bytes to 0
        if (newheader && nbytes > oldnbytes)
            memset(reinterpret_cast<uint8_t*>(newheader->elements()) + oldnbytes, 0, nbytes - oldnbytes);
    } else {
        void *p = maybecx ? maybecx->runtime()->callocCanGC(size) : js_calloc(size);
        newheader = static_cast<ObjectElements *>(p);
    }
    if (!newheader) {
        if (maybecx)
            js_ReportOutOfMemory(maybecx);
        return nullptr;
    }

    ArrayBufferObject::updateElementsHeader(newheader, nbytes);

    return newheader;
}

// The list of views must be stored somewhere in the ArrayBufferObject, but
// the slots are already being used for the element storage and the private
// field is used for a delegate object. The ObjectElements header has space
// for it, but I don't want to mess around with adding unions to it with
// JS_USE_NEW_OBJECT_REPRESENTATION pending, since it will solve this much
// more cleanly.
struct OldObjectRepresentationHack {
    uint32_t flags;
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

/* static */ bool
ArrayBufferObject::neuterViews(JSContext *cx, Handle<ArrayBufferObject*> buffer, void *newData)
{
    // neuterAsmJSArrayBuffer adjusts state specific to the ArrayBuffer data
    // itself, but it only affects the behavior of views
    if (buffer->isAsmJSArrayBuffer()) {
        if (!ArrayBufferObject::neuterAsmJSArrayBuffer(cx, *buffer))
            return false;
    }

    ArrayBufferViewObject *view;
    size_t numViews = 0;
    for (view = GetViewList(buffer); view; view = view->nextView()) {
        numViews++;
        view->neuter(newData);

        // Notify compiled jit code that the base pointer has moved.
        MarkObjectStateChange(cx, view);
    }

    // Remove buffer from the list of buffers with > 1 view.
    if (numViews > 1 && GetViewList(buffer)->bufferLink() != UNSET_BUFFER_LINK) {
        ArrayBufferObject *prev = buffer->compartment()->gcLiveArrayBuffers;
        if (prev == buffer) {
            buffer->compartment()->gcLiveArrayBuffers = GetViewList(prev)->bufferLink();
        } else {
            for (ArrayBufferObject *b = GetViewList(prev)->bufferLink();
                 b;
                 b = GetViewList(b)->bufferLink())
            {
                if (b == buffer) {
                    GetViewList(prev)->setBufferLink(GetViewList(b)->bufferLink());
                    break;
                }
                prev = b;
            }
        }
    }

    return true;
}

uint8_t *
ArrayBufferObject::dataPointer() const {
    if (isSharedArrayBuffer())
        return (uint8_t *)this->as<SharedArrayBufferObject>().dataPointer();
    return (uint8_t *)elements;
}

void
ArrayBufferObject::setNewData(ObjectElements *newHeader, uint32_t byteLength)
{
    JS_ASSERT(!isAsmJSArrayBuffer());
    JS_ASSERT(!isSharedArrayBuffer());

#ifdef JSGC_GENERATIONAL
    ObjectElements *oldHeader = ObjectElements::fromElements(elements);
    JS_ASSERT(oldHeader != newHeader);
    JSRuntime *rt = runtimeFromMainThread();
    if (hasDynamicElements())
        rt->gcNursery.notifyRemovedElements(this, oldHeader);
#endif

    elements = newHeader->elements();

#ifdef JSGC_GENERATIONAL
    if (hasDynamicElements())
        rt->gcNursery.notifyNewElements(this, newHeader);
#endif

    initElementsHeader(newHeader, byteLength);
}

void
ArrayBufferObject::changeContents(JSContext *cx, ObjectElements *newHeader)
{
    JS_ASSERT(!isAsmJSArrayBuffer());
    JS_ASSERT(!isSharedArrayBuffer());

    // Grab out data before invalidating it.
    uint32_t byteLengthCopy = byteLength();
    uintptr_t oldDataPointer = uintptr_t(dataPointer());
    ArrayBufferViewObject *viewListHead = GetViewList(this);

    // Update all views.
    uintptr_t newDataPointer = uintptr_t(newHeader->elements());
    for (ArrayBufferViewObject *view = viewListHead; view; view = view->nextView()) {
        // Watch out for NULL data pointers in views. This means that the view
        // is not fully initialized (in which case it'll be initialized later
        // with the correct pointer).
        uint8_t *viewDataPointer = view->dataPointer();
        if (viewDataPointer) {
            viewDataPointer += newDataPointer - oldDataPointer;
            view->setPrivate(viewDataPointer);
        }

        // Notify compiled jit code that the base pointer has moved.
        MarkObjectStateChange(cx, view);
    }

    // The list of views in the old header is reachable if the contents are
    // being transferred, so null it out
    SetViewList(this, nullptr);

    setNewData(newHeader, byteLengthCopy);

    InitViewList(this, viewListHead);
}

void
ArrayBufferObject::neuter(ObjectElements *newHeader, JSContext *cx)
{
    MOZ_ASSERT(!isSharedArrayBuffer());

    ObjectElements *oldHeader = getElementsHeader();
    if (hasStealableContents() && oldHeader != newHeader) {
        MOZ_ASSERT(newHeader);

        setNewData(newHeader, byteLength());

        FreeOp fop(cx->runtime(), false);
        fop.free_(oldHeader);
    } else {
        elements = newHeader->elements();
    }

    uint32_t byteLen = 0;
    updateElementsHeader(newHeader, byteLen);

    newHeader->setIsNeuteredBuffer();
}

/* static */ bool
ArrayBufferObject::ensureNonInline(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    JS_ASSERT(!buffer->isSharedArrayBuffer());
    if (buffer->hasDynamicElements())
        return true;

    ObjectElements *newHeader = AllocateArrayBufferContents(cx, buffer->byteLength());
    if (!newHeader)
        return false;

    void *newHeaderDataPointer = reinterpret_cast<void*>(newHeader->elements());
    memcpy(newHeaderDataPointer, buffer->dataPointer(), buffer->byteLength());

    buffer->changeContents(cx, newHeader);
    return true;
}

#if defined(JS_CPU_X64)
// Refer to comment above AsmJSMappedSize in AsmJS.h.
JS_STATIC_ASSERT(sizeof(ObjectElements) < AsmJSPageSize);
JS_STATIC_ASSERT(AsmJSAllocationGranularity == AsmJSPageSize);
#endif

#if defined(JS_ION) && defined(JS_CPU_X64)
bool
ArrayBufferObject::prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (buffer->isAsmJSArrayBuffer())
        return true;

    // SharedArrayBuffers are already created with AsmJS support in mind.
    if (buffer->isSharedArrayBuffer())
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
#else  /* defined(JS_ION) && defined(JS_CPU_X64) */
bool
ArrayBufferObject::prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (buffer->isAsmJSArrayBuffer())
        return true;

    if (buffer->isSharedArrayBuffer())
        return true;

    if (!ensureNonInline(cx, buffer))
        return false;

    JS_ASSERT(buffer->hasDynamicElements());
    buffer->getElementsHeader()->setIsAsmJSArrayBuffer();
    return true;
}

void
ArrayBufferObject::releaseAsmJSArrayBuffer(FreeOp *fop, JSObject *obj)
{
    fop->free_(obj->as<ArrayBufferObject>().getElementsHeader());
}
#endif

bool
ArrayBufferObject::neuterAsmJSArrayBuffer(JSContext *cx, ArrayBufferObject &buffer)
{
    JS_ASSERT(!buffer.isSharedArrayBuffer());
#ifdef JS_ION
    AsmJSActivation *act = cx->mainThread().asmJSActivationStackFromOwnerThread();
    for (; act; act = act->prevAsmJS()) {
        if (act->module().maybeHeapBufferObject() == &buffer)
            break;
    }
    if (!act)
        return true;

    js_ReportOverRecursed(cx);
    return false;
#else
    return true;
#endif
}

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

ArrayBufferObject *
ArrayBufferObject::create(JSContext *cx, uint32_t nbytes, bool clear /* = true */,
                          NewObjectKind newKind /* = GenericObject */)
{
    Rooted<ArrayBufferObject*> obj(cx, NewBuiltinClassInstance<ArrayBufferObject>(cx, newKind));
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

    // ArrayBufferObjects delegate added properties to another JSObject, so
    // their internal layout can use the object's fixed slots for storage.
    // Set up the object to look like an array with an elements header.
    JS_ASSERT(!obj->hasDynamicSlots());
    JS_ASSERT(!obj->hasDynamicElements());

    // The beginning stores an ObjectElements header structure holding the
    // length. The rest of it is a flat data store for the array buffer.
    size_t usableSlots = ARRAYBUFFER_RESERVED_SLOTS - ObjectElements::VALUES_PER_HEADER;

    if (nbytes > sizeof(Value) * usableSlots) {
        ObjectElements *header = AllocateArrayBufferContents(cx, nbytes);
        if (!header)
            return nullptr;
        obj->elements = header->elements();

#ifdef JSGC_GENERATIONAL
        JSRuntime *rt = obj->runtimeFromMainThread();
        rt->gcNursery.notifyNewElements(obj, header);
#endif
        obj->initElementsHeader(obj->getElementsHeader(), nbytes);
    } else {
        // Elements header must be initialized before dataPointer() is callable.
        obj->setFixedElements();
        obj->initElementsHeader(obj->getElementsHeader(), nbytes);
        if (clear)
            memset(obj->dataPointer(), 0, nbytes);
    }

    return obj;
}

JSObject *
ArrayBufferObject::createSlice(JSContext *cx, Handle<ArrayBufferObject*> arrayBuffer,
                               uint32_t begin, uint32_t end)
{
    uint32_t bufLength = arrayBuffer->byteLength();
    if (begin > bufLength || end > bufLength || begin > end) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPE_ERR_BAD_ARGS);
        return nullptr;
    }

    uint32_t length = end - begin;

    if (!arrayBuffer->hasData())
        return create(cx, 0);

    ArrayBufferObject *slice = create(cx, length, false);
    if (!slice)
        return nullptr;
    memcpy(slice->dataPointer(), arrayBuffer->dataPointer() + begin, length);
    return slice;
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

/* static */ bool
ArrayBufferObject::stealContents(JSContext *cx, Handle<ArrayBufferObject*> buffer, void **contents,
                                 uint8_t **data)
{
    uint32_t byteLen = buffer->byteLength();

    // If the ArrayBuffer's elements are transferrable, transfer ownership
    // directly.  Otherwise we have to copy the data into new elements.
    ObjectElements *transferableHeader;
    ObjectElements *newHeader;
    bool stolen = buffer->hasStealableContents();
    if (stolen) {
        transferableHeader = buffer->getElementsHeader();

        newHeader = AllocateArrayBufferContents(cx, byteLen);
        if (!newHeader)
            return false;
    } else {
        transferableHeader = AllocateArrayBufferContents(cx, byteLen);
        if (!transferableHeader)
            return false;

        initElementsHeader(transferableHeader, byteLen);
        void *headerDataPointer = reinterpret_cast<void*>(transferableHeader->elements());
        memcpy(headerDataPointer, buffer->dataPointer(), byteLen);

        // Keep using the current elements.
        newHeader = buffer->getElementsHeader();
    }

    JS_ASSERT(!IsInsideNursery(cx->runtime(), transferableHeader));
    *contents = transferableHeader;
    *data = reinterpret_cast<uint8_t *>(transferableHeader + 1);

    // Neuter the views, which may also mprotect(PROT_NONE) the buffer. So do
    // it after copying out the data.
    if (!ArrayBufferObject::neuterViews(cx, buffer, newHeader->elements()))
        return false;

    // If the elements were transferrable, revert the buffer back to using
    // inline storage so it doesn't attempt to free the stolen elements when
    // finalized.  Be careful to preserve the view list in the process.
    if (stolen) {
        ArrayBufferViewObject *viewListHead = GetViewList(buffer);

        SetViewList(buffer, nullptr);

        buffer->setNewData(ObjectElements::fromElements(buffer->fixedElements()), 0);

        InitViewList(buffer, viewListHead);
    }

    buffer->neuter(newHeader, cx);
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

    if (!IS_GC_MARKING_TRACER(trc) && !trc->runtime->isHeapMinorCollecting())
        return;

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

    ArrayBufferObject &buffer = AsArrayBuffer(obj);
    ArrayBufferViewObject *viewsHead = UpdateObjectIfRelocated(trc->runtime,
                                                               &GetViewListRef(&buffer));
    if (!viewsHead)
        return;

    viewsHead = UpdateObjectIfRelocated(trc->runtime, &GetViewListRef(&buffer));
    ArrayBufferViewObject *firstView = viewsHead;
    if (firstView->nextView() == nullptr) {
        // Single view: mark it, but only if we're actually doing a GC pass
        // right now. Otherwise, the tracing pass for barrier verification will
        // fail if we add another view and the pointer becomes weak.
        MarkObject(trc, &GetViewListRef(&buffer), "arraybuffer.singleview");
    } else {
        // Multiple views: do not mark, but append buffer to list.
        // obj_trace may be called multiple times before sweep(), so avoid
        // adding this buffer to the list multiple times.
        if (firstView->bufferLink() == UNSET_BUFFER_LINK) {
            JS_ASSERT(obj->compartment() == firstView->compartment());
            ArrayBufferObject **bufList = &obj->compartment()->gcLiveArrayBuffers;
            firstView->setBufferLink(*bufList);
            *bufList = &AsArrayBuffer(obj);
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

/* static */ void
ArrayBufferObject::sweep(JSCompartment *compartment)
{
    JSRuntime *rt = compartment->runtimeFromMainThread();
    ArrayBufferObject *buffer = compartment->gcLiveArrayBuffers;
    JS_ASSERT(buffer != UNSET_BUFFER_LINK);
    compartment->gcLiveArrayBuffers = nullptr;

    while (buffer) {
        ArrayBufferViewObject *viewsHead = UpdateObjectIfRelocated(rt, &GetViewListRef(buffer));
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
            view = UpdateObjectIfRelocated(rt, &nextView);
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
ArrayBufferObject::obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                  MutableHandleValue vp, bool strict)
{
    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;

    return baseops::SetPropertyHelper<SequentialExecution>(cx, delegate, obj, id, 0, vp, strict);
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
ArrayBufferObject::obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                                 MutableHandleValue statep, MutableHandleId idp)
{
    statep.setNull();
    return true;
}

/*
 * ArrayBufferViewObject
 */

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

    // Update obj's data slot if the array buffer moved. Note that during
    // initialization, bufSlot may still contain |undefined|.
    if (bufSlot.isObject()) {
        ArrayBufferObject &buf = AsArrayBuffer(&bufSlot.toObject());
        int32_t offset = obj->getReservedSlot(BYTEOFFSET_SLOT).toInt32();
        MOZ_ASSERT(buf.dataPointer() != nullptr);
        obj->initPrivate(buf.dataPointer() + offset);
    }

    /* Update NEXT_VIEW_SLOT, if the view moved. */
    IsSlotMarked(&obj->getReservedSlotRef(NEXT_VIEW_SLOT));
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
ArrayBufferViewObject::neuter(void *newData)
{
    MOZ_ASSERT(newData != nullptr);
    if (is<DataViewObject>())
        as<DataViewObject>().neuter(newData);
    else if (is<TypedArrayObject>())
        as<TypedArrayObject>().neuter(newData);
    else
        as<TypedObject>().neuter(newData);
}

/* JS Friend API */

JS_FRIEND_API(bool)
JS_IsArrayBufferViewObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<ArrayBufferViewObject>() : false;
}

JS_FRIEND_API(JSObject *)
js::UnwrapArrayBufferView(JSObject *obj)
{
    if (JSObject *unwrapped = CheckedUnwrap(obj))
        return unwrapped->is<ArrayBufferViewObject>() ? unwrapped : nullptr;
    return nullptr;
}

JS_FRIEND_API(uint32_t)
JS_GetArrayBufferByteLength(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? AsArrayBuffer(obj).byteLength() : 0;
}

JS_FRIEND_API(uint8_t *)
JS_GetArrayBufferData(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    return AsArrayBuffer(obj).dataPointer();
}

JS_FRIEND_API(uint8_t *)
JS_GetStableArrayBufferData(JSContext *cx, JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;

    Rooted<ArrayBufferObject*> buffer(cx, &AsArrayBuffer(obj));
    if (!ArrayBufferObject::ensureNonInline(cx, buffer))
        return nullptr;

    return buffer->dataPointer();
}

JS_FRIEND_API(bool)
js::NeuterArrayBuffer(JSContext *cx, HandleObject obj,
                      NeuterDataDisposition changeData)
{
    if (!obj->is<ArrayBufferObject>()) {
        JS_ReportError(cx, "ArrayBuffer object required");
        return false;
    }

    Rooted<ArrayBufferObject*> buffer(cx, &obj->as<ArrayBufferObject>());

    ObjectElements *newHeader;
    bool allocateNewData = (changeData == ChangeData &&
                            buffer->hasStealableContents());
    if (allocateNewData) {
        // If we're "disposing" with the buffer contents, allocate zeroed
        // memory of equal size and swap that in as contents.  This ensures
        // that stale indexes that assume the original length, won't index out
        // of bounds.  This is a temporary hack: when we're confident we've
        // eradicated all stale accesses, we'll stop doing this.
        newHeader = AllocateArrayBufferContents(cx, buffer->byteLength());
        if (!newHeader)
            return false;
    } else {
        // This case neuters out the existing elements in-place, so use the
        // old header as new.
        newHeader = buffer->getElementsHeader();
    }

    // Mark all views of the ArrayBuffer as neutered.
    if (!ArrayBufferObject::neuterViews(cx, buffer, newHeader->elements())) {
        if (allocateNewData) {
            FreeOp fop(cx->runtime(), false);
            fop.free_(newHeader);
        }
        return false;
    }

    buffer->neuter(newHeader, cx);
    return true;
}

JS_FRIEND_API(bool)
JS_NeuterArrayBuffer(JSContext *cx, HandleObject obj)
{
    return js::NeuterArrayBuffer(cx, obj, ChangeData);
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

    // Do not allocate ArrayBuffers with an API-provided pointer in the nursery.
    // These are likely to be long lived and the nursery does not know how to
    // free the contents.
    JSObject *obj = ArrayBufferObject::create(cx, 0, true, TenuredObject);
    if (!obj)
        return nullptr;
    js::ObjectElements *elements = reinterpret_cast<js::ObjectElements *>(contents);
    obj->setDynamicElements(elements);
    JS_ASSERT(GetViewList(&obj->as<ArrayBufferObject>()) == nullptr);

#ifdef JSGC_GENERATIONAL
    cx->runtime()->gcNursery.notifyNewElements(obj, elements);
#endif
    return obj;
}

JS_PUBLIC_API(bool)
JS_AllocateArrayBufferContents(JSContext *maybecx, uint32_t nbytes,
                               void **contents, uint8_t **data)
{
    js::ObjectElements *header = AllocateArrayBufferContents(maybecx, nbytes);
    if (!header)
        return false;

    ArrayBufferObject::updateElementsHeader(header, nbytes);

    *contents = header;
    *data = reinterpret_cast<uint8_t*>(header->elements());
    return true;
}

JS_PUBLIC_API(bool)
JS_ReallocateArrayBufferContents(JSContext *maybecx, uint32_t nbytes, void **contents, uint8_t **data)
{
    js::ObjectElements *header = AllocateArrayBufferContents(maybecx, nbytes, *contents);
    if (!header)
        return false;

    ArrayBufferObject::initElementsHeader(header, nbytes);

    *contents = header;
    *data = reinterpret_cast<uint8_t*>(header->elements());
    return true;
}

JS_FRIEND_API(bool)
JS_IsArrayBufferObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<ArrayBufferObject>() : false;
}

JS_FRIEND_API(JSObject *)
js::UnwrapArrayBuffer(JSObject *obj)
{
    if (JSObject *unwrapped = CheckedUnwrap(obj))
        return unwrapped->is<ArrayBufferObject>() ? unwrapped : nullptr;
    return nullptr;
}

JS_PUBLIC_API(bool)
JS_StealArrayBufferContents(JSContext *cx, HandleObject objArg, void **contents, uint8_t **data)
{
    JSObject *obj = CheckedUnwrap(objArg);
    if (!obj)
        return false;

    if (!obj->is<ArrayBufferObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }

    Rooted<ArrayBufferObject*> buffer(cx, &obj->as<ArrayBufferObject>());
    if (!ArrayBufferObject::stealContents(cx, buffer, contents, data))
        return false;

    return true;
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

JS_FRIEND_API(void)
js::GetArrayBufferViewLengthAndData(JSObject *obj, uint32_t *length, uint8_t **data)
{
    MOZ_ASSERT(obj->is<ArrayBufferViewObject>());

    *length = obj->is<DataViewObject>()
              ? obj->as<DataViewObject>().byteLength()
              : obj->as<TypedArrayObject>().byteLength();

    *data = static_cast<uint8_t*>(obj->is<DataViewObject>()
                                  ? obj->as<DataViewObject>().dataPointer()
                                  : obj->as<TypedArrayObject>().viewData());
}

JS_FRIEND_API(JSObject *)
JS_GetObjectAsArrayBuffer(JSObject *obj, uint32_t *length, uint8_t **data)
{
    if (!(obj = CheckedUnwrap(obj)))
        return nullptr;
    if (!IsArrayBuffer(obj))
        return nullptr;

    *length = AsArrayBuffer(obj).byteLength();
    *data = AsArrayBuffer(obj).dataPointer();

    return obj;
}

JS_FRIEND_API(void)
js::GetArrayBufferLengthAndData(JSObject *obj, uint32_t *length, uint8_t **data)
{
    MOZ_ASSERT(IsArrayBuffer(obj));
    *length = AsArrayBuffer(obj).byteLength();
    *data = AsArrayBuffer(obj).dataPointer();
}

