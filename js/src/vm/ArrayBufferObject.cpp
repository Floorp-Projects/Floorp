/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ArrayBufferObject.h"

#include "mozilla/Alignment.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"
#include "mozilla/TaggedAnonymousMemory.h"

#include <string.h>
#ifndef XP_WIN
# include <sys/mman.h>
#endif

#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
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

#include "asmjs/AsmJSModule.h"
#include "asmjs/AsmJSValidate.h"
#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "gc/Memory.h"
#include "js/MemoryMetrics.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/NumericConversions.h"
#include "vm/WrapperObject.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/Shape-inl.h"

using mozilla::DebugOnly;

using namespace js;
using namespace js::gc;
using namespace js::types;

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
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayBuffer) |
    JSCLASS_BACKGROUND_FINALIZE,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    ArrayBufferObject::finalize,
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    nullptr,        /* trace       */
    JS_NULL_CLASS_SPEC,
    {
        nullptr,    /* outerObject */
        nullptr,    /* innerObject */
        nullptr,    /* iteratorObject */
        false,      /* isWrappedNative */
        nullptr,    /* weakmapKeyDelegateOp */
        ArrayBufferObject::objectMoved
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
    return v.isObject() && v.toObject().is<ArrayBufferObject>();
}

bool
js::IsArrayBuffer(HandleObject obj)
{
    return obj->is<ArrayBufferObject>();
}

bool
js::IsArrayBuffer(JSObject *obj)
{
    return obj->is<ArrayBufferObject>();
}

ArrayBufferObject &
js::AsArrayBuffer(HandleObject obj)
{
    MOZ_ASSERT(IsArrayBuffer(obj));
    return obj->as<ArrayBufferObject>();
}

ArrayBufferObject &
js::AsArrayBuffer(JSObject *obj)
{
    MOZ_ASSERT(IsArrayBuffer(obj));
    return obj->as<ArrayBufferObject>();
}

MOZ_ALWAYS_INLINE bool
ArrayBufferObject::byteLengthGetterImpl(JSContext *cx, CallArgs args)
{
    MOZ_ASSERT(IsArrayBuffer(args.thisv()));
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
    MOZ_ASSERT(IsArrayBuffer(args.thisv()));

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

static ArrayBufferObject::BufferContents
AllocateArrayBufferContents(JSContext *cx, uint32_t nbytes)
{
    uint8_t *p = cx->runtime()->pod_callocCanGC<uint8_t>(nbytes);
    if (!p)
        js_ReportOutOfMemory(cx);

    return ArrayBufferObject::BufferContents::create<ArrayBufferObject::PLAIN_BUFFER>(p);
}

bool
ArrayBufferObject::canNeuter(JSContext *cx)
{
    if (isAsmJSArrayBuffer()) {
        if (!ArrayBufferObject::canNeuterAsmJSArrayBuffer(cx, *this))
            return false;
    }

    return true;
}

void
ArrayBufferObject::neuterView(JSContext *cx, ArrayBufferViewObject *view,
                              BufferContents newContents)
{
    view->neuter(newContents.data());

    // Notify compiled jit code that the base pointer has moved.
    MarkObjectStateChange(cx, view);
}

/* static */ void
ArrayBufferObject::neuter(JSContext *cx, Handle<ArrayBufferObject*> buffer,
                          BufferContents newContents)
{
    MOZ_ASSERT(buffer->canNeuter(cx));

    // Neuter all views on the buffer, clear out the list of views and the
    // buffer's data.

    if (InnerViewTable::ViewVector *views = cx->compartment()->innerViews.maybeViewsUnbarriered(buffer)) {
        for (size_t i = 0; i < views->length(); i++)
            buffer->neuterView(cx, (*views)[i], newContents);
        cx->compartment()->innerViews.removeViews(buffer);
    }
    if (buffer->firstView())
        buffer->neuterView(cx, buffer->firstView(), newContents);
    buffer->setFirstView(nullptr);

    if (newContents.data() != buffer->dataPointer())
        buffer->setNewOwnedData(cx->runtime()->defaultFreeOp(), newContents);

    buffer->setByteLength(0);
    buffer->setIsNeutered();
}

void
ArrayBufferObject::setNewOwnedData(FreeOp* fop, BufferContents newContents)
{
    MOZ_ASSERT(!isAsmJSArrayBuffer());

    if (ownsData()) {
        MOZ_ASSERT(newContents.data() != dataPointer());
        releaseData(fop);
    }

    setDataPointer(newContents, OwnsData);
}

void
ArrayBufferObject::changeViewContents(JSContext *cx, ArrayBufferViewObject *view,
                                      uint8_t *oldDataPointer, BufferContents newContents)
{
    // Watch out for NULL data pointers in views. This means that the view
    // is not fully initialized (in which case it'll be initialized later
    // with the correct pointer).
    uint8_t *viewDataPointer = view->dataPointer();
    if (viewDataPointer) {
        MOZ_ASSERT(newContents);
        ptrdiff_t offset = viewDataPointer - oldDataPointer;
        viewDataPointer = static_cast<uint8_t *>(newContents.data()) + offset;
        view->setPrivate(viewDataPointer);
    }

    // Notify compiled jit code that the base pointer has moved.
    MarkObjectStateChange(cx, view);
}

void
ArrayBufferObject::changeContents(JSContext *cx, BufferContents newContents)
{
    // Change buffer contents.
    uint8_t* oldDataPointer = dataPointer();
    setNewOwnedData(cx->runtime()->defaultFreeOp(), newContents);

    // Update all views.
    if (InnerViewTable::ViewVector *views = cx->compartment()->innerViews.maybeViewsUnbarriered(this)) {
        for (size_t i = 0; i < views->length(); i++)
            changeViewContents(cx, (*views)[i], oldDataPointer, newContents);
    }
    if (firstView())
        changeViewContents(cx, firstView(), oldDataPointer, newContents);
}

/* static */ bool
ArrayBufferObject::prepareForAsmJSNoSignals(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (buffer->isAsmJSArrayBuffer())
        return true;

    if (!ensureNonInline(cx, buffer))
        return false;

    buffer->setIsAsmJSArrayBuffer();
    return true;
}

void
ArrayBufferObject::releaseAsmJSArrayNoSignals(FreeOp *fop)
{
    MOZ_ASSERT(!(bufferKind() & MAPPED_BUFFER));
    fop->free_(dataPointer());
}

#ifdef JS_CODEGEN_X64
/* static */ bool
ArrayBufferObject::prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer,
                                   bool usesSignalHandlers)
{
    // If we can't use signal handlers, just do it like on other platforms.
    if (!usesSignalHandlers)
        return prepareForAsmJSNoSignals(cx, buffer);

    if (buffer->isAsmJSArrayBuffer())
        return true;

    // Get the entire reserved region (with all pages inaccessible).
    void *data;
# ifdef XP_WIN
    data = VirtualAlloc(nullptr, AsmJSMappedSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!data)
        return false;
# else
    data = MozTaggedAnonymousMmap(nullptr, AsmJSMappedSize, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0, "asm-js-reserved");
    if (data == MAP_FAILED)
        return false;
# endif

    // Enable access to the valid region.
    MOZ_ASSERT(buffer->byteLength() % AsmJSPageSize == 0);
# ifdef XP_WIN
    if (!VirtualAlloc(data, buffer->byteLength(), MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(data, 0, MEM_RELEASE);
        return false;
    }
# else
    size_t validLength = buffer->byteLength();
    if (mprotect(data, validLength, PROT_READ | PROT_WRITE)) {
        munmap(data, AsmJSMappedSize);
        return false;
    }
#   if defined(MOZ_VALGRIND) && defined(VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE)
    // Tell Valgrind/Memcheck to not report accesses in the inaccessible region.
    VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE((unsigned char*)data + validLength,
                                                   AsmJSMappedSize-validLength);
#   endif
# endif

    // Copy over the current contents of the typed array.
    memcpy(data, buffer->dataPointer(), buffer->byteLength());

    // Swap the new elements into the ArrayBufferObject. Mark the
    // ArrayBufferObject so we don't do this again.
    BufferContents newContents = BufferContents::create<BufferKind(ASMJS_BUFFER|MAPPED_BUFFER)>(data);
    buffer->changeContents(cx, newContents);
    MOZ_ASSERT(data == buffer->dataPointer());

    return true;
}

void
ArrayBufferObject::releaseAsmJSArray(FreeOp *fop)
{
    if (!(bufferKind() & MAPPED_BUFFER)) {
        releaseAsmJSArrayNoSignals(fop);
        return;
    }

    void *data = dataPointer();

    MOZ_ASSERT(uintptr_t(data) % AsmJSPageSize == 0);
# ifdef XP_WIN
    VirtualFree(data, 0, MEM_RELEASE);
# else
    munmap(data, AsmJSMappedSize);
#   if defined(MOZ_VALGRIND) && defined(VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE)
    // Tell Valgrind/Memcheck to recommence reporting accesses in the
    // previously-inaccessible region.
    if (AsmJSMappedSize > 0) {
        VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(data, AsmJSMappedSize);
    }
#   endif
# endif
}
#else // JS_CODEGEN_X64
bool
ArrayBufferObject::prepareForAsmJS(JSContext *cx, Handle<ArrayBufferObject*> buffer,
                                   bool usesSignalHandlers)
{
    // Platforms other than x64 don't use signalling for bounds checking, so
    // just use the variant with no signals.
    MOZ_ASSERT(!usesSignalHandlers);
    return prepareForAsmJSNoSignals(cx, buffer);
}

void
ArrayBufferObject::releaseAsmJSArray(FreeOp *fop)
{
    // See comment above.
    releaseAsmJSArrayNoSignals(fop);
}
#endif

bool
ArrayBufferObject::canNeuterAsmJSArrayBuffer(JSContext *cx, ArrayBufferObject &buffer)
{
    AsmJSActivation *act = cx->mainThread().asmJSActivationStack();
    for (; act; act = act->prevAsmJS()) {
        if (act->module().maybeHeapBufferObject() == &buffer)
            break;
    }
    if (!act)
        return true;

    return false;
}

ArrayBufferObject::BufferContents
ArrayBufferObject::createMappedContents(int fd, size_t offset, size_t length)
{
    void *data = AllocateMappedContent(fd, offset, length, ARRAY_BUFFER_ALIGNMENT);
    return BufferContents::create<MAPPED_BUFFER>(data);
}

void
ArrayBufferObject::releaseMappedArray()
{
    if(!isMappedArrayBuffer() || isNeutered())
        return;

    DeallocateMappedContent(dataPointer(), byteLength());
}

uint8_t *
ArrayBufferObject::inlineDataPointer() const
{
    return static_cast<uint8_t *>(fixedData(JSCLASS_RESERVED_SLOTS(&class_)));
}

uint8_t *
ArrayBufferObject::dataPointer() const
{
    return static_cast<uint8_t *>(getSlot(DATA_SLOT).toPrivate());
}

void
ArrayBufferObject::releaseData(FreeOp *fop)
{
    MOZ_ASSERT(ownsData());

    BufferKind bufkind = bufferKind();
    if (bufkind & ASMJS_BUFFER)
        releaseAsmJSArray(fop);
    else if (bufkind & MAPPED_BUFFER)
        releaseMappedArray();
    else
        fop->free_(dataPointer());
}

void
ArrayBufferObject::setDataPointer(BufferContents contents, OwnsState ownsData)
{
    setSlot(DATA_SLOT, PrivateValue(contents.data()));
    setOwnsData(ownsData);
    setFlags((flags() & ~KIND_MASK) | contents.kind());
}

size_t
ArrayBufferObject::byteLength() const
{
    return size_t(getSlot(BYTE_LENGTH_SLOT).toDouble());
}

void
ArrayBufferObject::setByteLength(size_t length)
{
    setSlot(BYTE_LENGTH_SLOT, DoubleValue(length));
}

uint32_t
ArrayBufferObject::flags() const
{
    return uint32_t(getSlot(FLAGS_SLOT).toInt32());
}

void
ArrayBufferObject::setFlags(uint32_t flags)
{
    setSlot(FLAGS_SLOT, Int32Value(flags));
}

ArrayBufferObject *
ArrayBufferObject::create(JSContext *cx, uint32_t nbytes, BufferContents contents,
                          NewObjectKind newKind /* = GenericObject */)
{
    MOZ_ASSERT_IF(contents.kind() & MAPPED_BUFFER, contents);

    // If we need to allocate data, try to use a larger object size class so
    // that the array buffer's data can be allocated inline with the object.
    // The extra space will be left unused by the object's fixed slots and
    // available for the buffer's data, see NewObject().
    size_t reservedSlots = JSCLASS_RESERVED_SLOTS(&class_);

    size_t nslots = reservedSlots;
    bool allocated = false;
    if (contents) {
        // The ABO is taking ownership, so account the bytes against the zone.
        size_t nAllocated = nbytes;
        if (contents.kind() & MAPPED_BUFFER)
            nAllocated = JS_ROUNDUP(nbytes, js::gc::SystemPageSize());
        cx->zone()->updateMallocCounter(nAllocated);
    } else {
        size_t usableSlots = JSObject::MAX_FIXED_SLOTS - reservedSlots;
        if (nbytes <= usableSlots * sizeof(Value)) {
            int newSlots = (nbytes - 1) / sizeof(Value) + 1;
            MOZ_ASSERT(int(nbytes) <= newSlots * int(sizeof(Value)));
            nslots = reservedSlots + newSlots;
            contents = BufferContents::createUnowned(nullptr);
        } else {
            contents = AllocateArrayBufferContents(cx, nbytes);
            if (!contents)
                return nullptr;
            allocated = true;
        }
    }

    MOZ_ASSERT(!(class_.flags & JSCLASS_HAS_PRIVATE));
    gc::AllocKind allocKind = GetGCObjectKind(nslots);

    Rooted<ArrayBufferObject*> obj(cx, NewBuiltinClassInstance<ArrayBufferObject>(cx, allocKind, newKind));
    if (!obj) {
        if (allocated)
            js_free(contents.data());
        return nullptr;
    }

    MOZ_ASSERT(obj->getClass() == &class_);
    MOZ_ASSERT(!gc::IsInsideNursery(obj));

    if (!contents) {
        void *data = obj->inlineDataPointer();
        memset(data, 0, nbytes);
        obj->initialize(nbytes, BufferContents::createUnowned(data), DoesntOwnData);
    } else {
        obj->initialize(nbytes, contents, OwnsData);
    }

    return obj;
}

ArrayBufferObject *
ArrayBufferObject::create(JSContext *cx, uint32_t nbytes,
                          NewObjectKind newKind /* = GenericObject */)
{
    return create(cx, nbytes, BufferContents::createUnowned(nullptr));
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

    ArrayBufferObject *slice = create(cx, length);
    if (!slice)
        return nullptr;
    memcpy(slice->dataPointer(), arrayBuffer->dataPointer() + begin, length);
    return slice;
}

bool
ArrayBufferObject::createDataViewForThisImpl(JSContext *cx, CallArgs args)
{
    MOZ_ASSERT(IsArrayBuffer(args.thisv()));

    /*
     * This method is only called for |DataView(alienBuf, ...)| which calls
     * this as |createDataViewForThis.call(alienBuf, ..., DataView.prototype)|,
     * ergo there must be at least two arguments.
     */
    MOZ_ASSERT(args.length() >= 2);

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
ArrayBufferObject::ensureNonInline(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (!buffer->ownsData()) {
        BufferContents contents = AllocateArrayBufferContents(cx, buffer->byteLength());
        if (!contents)
            return false;
        memcpy(contents.data(), buffer->dataPointer(), buffer->byteLength());
        buffer->changeContents(cx, contents);
    }

    return true;
}

/* static */ ArrayBufferObject::BufferContents
ArrayBufferObject::stealContents(JSContext *cx, Handle<ArrayBufferObject*> buffer)
{
    if (!buffer->canNeuter(cx)) {
        js_ReportOverRecursed(cx);
        return BufferContents::createUnowned(nullptr);
    }

    BufferContents oldContents(buffer->dataPointer(), buffer->bufferKind());
    BufferContents newContents = AllocateArrayBufferContents(cx, buffer->byteLength());
    if (!newContents)
        return BufferContents::createUnowned(nullptr);

    if (buffer->hasStealableContents()) {
        // Return the old contents and give the neutered buffer a pointer to
        // freshly allocated memory that we will never write to and should
        // never get committed.
        buffer->setOwnsData(DoesntOwnData);
        ArrayBufferObject::neuter(cx, buffer, newContents);
        return oldContents;
    }

    // Create a new chunk of memory to return since we cannot steal the
    // existing contents away from the buffer.
    memcpy(newContents.data(), oldContents.data(), buffer->byteLength());
    ArrayBufferObject::neuter(cx, buffer, oldContents);
    return newContents;
}

/* static */ void
ArrayBufferObject::addSizeOfExcludingThis(JSObject *obj, mozilla::MallocSizeOf mallocSizeOf,
                                          JS::ClassInfo *info)
{
    ArrayBufferObject &buffer = AsArrayBuffer(obj);

    if (!buffer.ownsData())
        return;

    if (MOZ_UNLIKELY(buffer.bufferKind() & ASMJS_BUFFER)) {
        // On x64, ArrayBufferObject::prepareForAsmJS switches the
        // ArrayBufferObject to use mmap'd storage.
        if (buffer.bufferKind() & MAPPED_BUFFER)
            info->objectsNonHeapElementsAsmJS += buffer.byteLength();
        else
            info->objectsMallocHeapElementsAsmJS += mallocSizeOf(buffer.dataPointer());
    } else if (MOZ_UNLIKELY(buffer.bufferKind() & MAPPED_BUFFER)) {
        info->objectsNonHeapElementsMapped += buffer.byteLength();
    } else if (buffer.dataPointer()) {
        info->objectsMallocHeapElementsNonAsmJS += mallocSizeOf(buffer.dataPointer());
    }
}

/* static */ void
ArrayBufferObject::finalize(FreeOp *fop, JSObject *obj)
{
    ArrayBufferObject &buffer = obj->as<ArrayBufferObject>();

    if (buffer.ownsData())
        buffer.releaseData(fop);
}

/* static */ void
ArrayBufferObject::objectMoved(JSObject *obj, const JSObject *old)
{
    ArrayBufferObject &dst = obj->as<ArrayBufferObject>();
    const ArrayBufferObject &src = old->as<ArrayBufferObject>();

    // Fix up possible inline data pointer.
    if (src.hasInlineData())
        dst.setSlot(DATA_SLOT, PrivateValue(dst.inlineDataPointer()));
}

ArrayBufferViewObject *
ArrayBufferObject::firstView()
{
    return getSlot(FIRST_VIEW_SLOT).isObject()
           ? &getSlot(FIRST_VIEW_SLOT).toObject().as<ArrayBufferViewObject>()
           : nullptr;
}

void
ArrayBufferObject::setFirstView(ArrayBufferViewObject *view)
{
    setSlot(FIRST_VIEW_SLOT, ObjectOrNullValue(view));
}

bool
ArrayBufferObject::addView(JSContext *cx, ArrayBufferViewObject *view)
{
    if (!firstView()) {
        setFirstView(view);
        return true;
    }
    return cx->compartment()->innerViews.addView(cx, this, view);
}

/*
 * InnerViewTable
 */

static size_t VIEW_LIST_MAX_LENGTH = 500;

bool
InnerViewTable::addView(JSContext *cx, ArrayBufferObject *obj, ArrayBufferViewObject *view)
{
    // ArrayBufferObject entries are only added when there are multiple views.
    MOZ_ASSERT(obj->firstView());

    if (!map.initialized() && !map.init())
        return false;

    Map::AddPtr p = map.lookupForAdd(obj);

    MOZ_ASSERT(!gc::IsInsideNursery(obj));
    bool addToNursery = nurseryKeysValid && gc::IsInsideNursery(view);

    if (p) {
        ViewVector &views = p->value();
        MOZ_ASSERT(!views.empty());

        if (addToNursery) {
            // Only add the entry to |nurseryKeys| if it isn't already there.
            if (views.length() >= VIEW_LIST_MAX_LENGTH) {
                // To avoid quadratic blowup, skip the loop below if we end up
                // adding enormous numbers of views for the same object.
                nurseryKeysValid = false;
            } else {
                for (size_t i = 0; i < views.length(); i++) {
                    if (gc::IsInsideNursery(views[i]))
                        addToNursery = false;
                }
            }
        }

        if (!views.append(view))
            return false;
    } else {
        if (!map.add(p, obj, ViewVector()))
            return false;
        JS_ALWAYS_TRUE(p->value().append(view));
    }

    if (addToNursery && !nurseryKeys.append(obj))
        nurseryKeysValid = false;

    return true;
}

InnerViewTable::ViewVector *
InnerViewTable::maybeViewsUnbarriered(ArrayBufferObject *obj)
{
    if (!map.initialized())
        return nullptr;

    Map::Ptr p = map.lookup(obj);
    if (p)
        return &p->value();
    return nullptr;
}

void
InnerViewTable::removeViews(ArrayBufferObject *obj)
{
    Map::Ptr p = map.lookup(obj);
    MOZ_ASSERT(p);

    map.remove(p);
}

bool
InnerViewTable::sweepEntry(JSObject **pkey, ViewVector &views)
{
    if (IsObjectAboutToBeFinalized(pkey))
        return true;

    MOZ_ASSERT(!views.empty());
    for (size_t i = 0; i < views.length(); i++) {
        if (IsObjectAboutToBeFinalized(&views[i])) {
            views[i--] = views.back();
            views.popBack();
        }
    }

    return views.empty();
}

void
InnerViewTable::sweep(JSRuntime *rt)
{
    MOZ_ASSERT(nurseryKeys.empty());

    if (!map.initialized())
        return;

    for (Map::Enum e(map); !e.empty(); e.popFront()) {
        JSObject *key = e.front().key();
        if (sweepEntry(&key, e.front().value()))
            e.removeFront();
        else if (key != e.front().key())
            e.rekeyFront(key);
    }
}

void
InnerViewTable::sweepAfterMinorGC(JSRuntime *rt)
{
    MOZ_ASSERT(!nurseryKeys.empty());

    if (nurseryKeysValid) {
        for (size_t i = 0; i < nurseryKeys.length(); i++) {
            JSObject *key = nurseryKeys[i];
            Map::Ptr p = map.lookup(key);
            if (!p)
                continue;

            if (sweepEntry(&key, p->value()))
                map.remove(nurseryKeys[i]);
            else
                map.rekeyIfMoved(nurseryKeys[i], key);
        }
        nurseryKeys.clear();
    } else {
        // Do the required sweeping by looking at every map entry.
        nurseryKeys.clear();
        sweep(rt);

        nurseryKeysValid = true;
    }
}

size_t
InnerViewTable::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    if (!map.initialized())
        return 0;

    size_t vectorSize = 0;
    for (Map::Enum e(map); !e.empty(); e.popFront())
        vectorSize += e.front().value().sizeOfExcludingThis(mallocSizeOf);

    return vectorSize
         + map.sizeOfExcludingThis(mallocSizeOf)
         + nurseryKeys.sizeOfExcludingThis(mallocSizeOf);
}

/*
 * ArrayBufferViewObject
 */

/*
 * This method is used to trace TypedArrayObjects and DataViewObjects. We need
 * a custom tracer to move the object's data pointer if its owner was moved and
 * stores its data inline.
 */
/* static */ void
ArrayBufferViewObject::trace(JSTracer *trc, JSObject *obj)
{
    HeapSlot &bufSlot = obj->getReservedSlotRef(TypedArrayLayout::BUFFER_SLOT);
    MarkSlot(trc, &bufSlot, "typedarray.buffer");

    // Update obj's data pointer if the array buffer moved. Note that during
    // initialization, bufSlot may still contain |undefined|.
    if (bufSlot.isObject()) {
        ArrayBufferObject &buf = AsArrayBuffer(MaybeForwarded(&bufSlot.toObject()));
        int32_t offset = obj->getReservedSlot(TypedArrayLayout::BYTEOFFSET_SLOT).toInt32();
        MOZ_ASSERT(buf.dataPointer() != nullptr);
        obj->initPrivate(buf.dataPointer() + offset);
    }
}

template <>
bool
JSObject::is<js::ArrayBufferViewObject>() const
{
    return is<DataViewObject>() || is<TypedArrayObject>() || is<TypedObject>();
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
        as<OutlineTypedObject>().neuter(newData);
}

uint8_t *
ArrayBufferViewObject::dataPointer()
{
    if (is<DataViewObject>())
        return static_cast<uint8_t *>(as<DataViewObject>().dataPointer());
    if (is<TypedArrayObject>())
        return static_cast<uint8_t *>(as<TypedArrayObject>().viewData());
    return as<TypedObject>().typedMem();
}

/* static */ ArrayBufferObject *
ArrayBufferViewObject::bufferObject(JSContext *cx, Handle<ArrayBufferViewObject *> thisObject)
{
    if (thisObject->is<TypedArrayObject>()) {
        Rooted<TypedArrayObject *> typedArray(cx, &thisObject->as<TypedArrayObject>());
        if (!TypedArrayObject::ensureHasBuffer(cx, typedArray))
            return nullptr;
        return thisObject->as<TypedArrayObject>().buffer();
    }
    MOZ_ASSERT(thisObject->is<DataViewObject>());
    return &thisObject->as<DataViewObject>().arrayBuffer();
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
JS_GetStableArrayBufferData(JSContext *cx, HandleObject objArg)
{
    JSObject *obj = CheckedUnwrap(objArg);
    if (!obj)
        return nullptr;

    Rooted<ArrayBufferObject*> buffer(cx, &AsArrayBuffer(obj));
    if (!ArrayBufferObject::ensureNonInline(cx, buffer))
        return nullptr;

    return buffer->dataPointer();
}

JS_FRIEND_API(bool)
JS_NeuterArrayBuffer(JSContext *cx, HandleObject obj,
                     NeuterDataDisposition changeData)
{
    if (!obj->is<ArrayBufferObject>()) {
        JS_ReportError(cx, "ArrayBuffer object required");
        return false;
    }

    Rooted<ArrayBufferObject*> buffer(cx, &obj->as<ArrayBufferObject>());

    if (!buffer->canNeuter(cx)) {
        js_ReportOverRecursed(cx);
        return false;
    }

    if (changeData == ChangeData && buffer->hasStealableContents()) {
        ArrayBufferObject::BufferContents newContents =
            AllocateArrayBufferContents(cx, buffer->byteLength());
        if (!newContents)
            return false;
        ArrayBufferObject::neuter(cx, buffer, newContents);
    } else {
        ArrayBufferObject::neuter(cx, buffer, buffer->contents());
    }

    return true;
}

JS_FRIEND_API(bool)
JS_IsNeuteredArrayBufferObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return false;

    return obj->is<ArrayBufferObject>()
           ? obj->as<ArrayBufferObject>().isNeutered()
           : false;
}

JS_FRIEND_API(JSObject *)
JS_NewArrayBuffer(JSContext *cx, uint32_t nbytes)
{
    MOZ_ASSERT(nbytes <= INT32_MAX);
    return ArrayBufferObject::create(cx, nbytes);
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayBufferWithContents(JSContext *cx, size_t nbytes, void *data)
{
    MOZ_ASSERT(data);
    ArrayBufferObject::BufferContents contents =
        ArrayBufferObject::BufferContents::create<ArrayBufferObject::PLAIN_BUFFER>(data);
    return ArrayBufferObject::create(cx, nbytes, contents, TenuredObject);
}

JS_FRIEND_API(bool)
JS_IsArrayBufferObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<ArrayBufferObject>() : false;
}

JS_FRIEND_API(bool)
JS_ArrayBufferHasData(JSObject *obj)
{
    return CheckedUnwrap(obj)->as<ArrayBufferObject>().hasData();
}

JS_FRIEND_API(JSObject *)
js::UnwrapArrayBuffer(JSObject *obj)
{
    if (JSObject *unwrapped = CheckedUnwrap(obj))
        return unwrapped->is<ArrayBufferObject>() ? unwrapped : nullptr;
    return nullptr;
}

JS_PUBLIC_API(void *)
JS_StealArrayBufferContents(JSContext *cx, HandleObject objArg)
{
    JSObject *obj = CheckedUnwrap(objArg);
    if (!obj)
        return nullptr;

    if (!obj->is<ArrayBufferObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return nullptr;
    }

    Rooted<ArrayBufferObject*> buffer(cx, &obj->as<ArrayBufferObject>());
    return ArrayBufferObject::stealContents(cx, buffer).data();
}

JS_PUBLIC_API(JSObject *)
JS_NewMappedArrayBufferWithContents(JSContext *cx, size_t nbytes, void *data)
{
    MOZ_ASSERT(data);
    ArrayBufferObject::BufferContents contents =
        ArrayBufferObject::BufferContents::create<ArrayBufferObject::MAPPED_BUFFER>(data);
    return ArrayBufferObject::create(cx, nbytes, contents, TenuredObject);
}

JS_PUBLIC_API(void *)
JS_CreateMappedArrayBufferContents(int fd, size_t offset, size_t length)
{
    return ArrayBufferObject::createMappedContents(fd, offset, length).data();
}

JS_PUBLIC_API(void)
JS_ReleaseMappedArrayBufferContents(void *contents, size_t length)
{
    DeallocateMappedContent(contents, length);
}

JS_FRIEND_API(bool)
JS_IsMappedArrayBufferObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return false;

    return obj->is<ArrayBufferObject>()
           ? obj->as<ArrayBufferObject>().isMappedArrayBuffer()
           : false;
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
JS_GetArrayBufferViewBuffer(JSContext *cx, HandleObject objArg)
{
    JSObject *obj = CheckedUnwrap(objArg);
    if (!obj)
        return nullptr;
    Rooted<ArrayBufferViewObject *> viewObject(cx, &obj->as<ArrayBufferViewObject>());
    return ArrayBufferViewObject::bufferObject(cx, viewObject);
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
