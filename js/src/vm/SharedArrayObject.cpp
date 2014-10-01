/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/SharedArrayObject.h"

#include "jsprf.h"
#include "jsobjinlines.h"

#ifdef XP_WIN
# include "jswin.h"
#endif
#include "jswrapper.h"
#ifndef XP_WIN
# include <sys/mman.h>
#endif
#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
#endif

#include "mozilla/Atomics.h"

#include "asmjs/AsmJSValidate.h"
#include "vm/TypedArrayCommon.h"

using namespace js;

static inline void *
MapMemory(size_t length, bool commit)
{
#ifdef XP_WIN
    int prot = (commit ? MEM_COMMIT : MEM_RESERVE);
    int flags = (commit ? PAGE_READWRITE : PAGE_NOACCESS);
    return VirtualAlloc(nullptr, length, prot, flags);
#else
    int prot = (commit ? (PROT_READ | PROT_WRITE) : PROT_NONE);
    void *p = mmap(nullptr, length, prot, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED)
        return nullptr;
    return p;
#endif
}

static inline void
UnmapMemory(void *addr, size_t len)
{
#ifdef XP_WIN
    VirtualFree(addr, 0, MEM_RELEASE);
#else
    munmap(addr, len);
#endif
}

static inline bool
MarkValidRegion(void *addr, size_t len)
{
#ifdef XP_WIN
    if (!VirtualAlloc(addr, len, MEM_COMMIT, PAGE_READWRITE))
        return false;
    return true;
#else
    if (mprotect(addr, len, PROT_READ | PROT_WRITE))
        return false;
    return true;
#endif
}

#ifdef JS_CODEGEN_X64
// Since this SharedArrayBuffer will likely be used for asm.js code, prepare it
// for asm.js by mapping the 4gb protected zone described in AsmJSValidate.h.
// Since we want to put the SharedArrayBuffer header immediately before the
// heap but keep the heap page-aligned, allocate an extra page before the heap.
static const uint64_t SharedArrayMappedSize = AsmJSMappedSize + AsmJSPageSize;
static_assert(sizeof(SharedArrayRawBuffer) < AsmJSPageSize, "Page size not big enough");
#endif

SharedArrayRawBuffer *
SharedArrayRawBuffer::New(uint32_t length)
{
    // The value (uint32_t)-1 is used as a signal in various places,
    // so guard against it on principle.
    MOZ_ASSERT(length != (uint32_t)-1);

    // Enforced by SharedArrayBufferObject::New(cx, length).
    MOZ_ASSERT(IsValidAsmJSHeapLength(length));

#ifdef JS_CODEGEN_X64
    // Get the entire reserved region (with all pages inaccessible)
    void *p = MapMemory(SharedArrayMappedSize, false);
    if (!p)
        return nullptr;

    size_t validLength = AsmJSPageSize + length;
    if (!MarkValidRegion(p, validLength)) {
        UnmapMemory(p, SharedArrayMappedSize);
        return nullptr;
    }
#   if defined(MOZ_VALGRIND) && defined(VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE)
    // Tell Valgrind/Memcheck to not report accesses in the inaccessible region.
    VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE((unsigned char*)p + validLength,
                                                   SharedArrayMappedSize-validLength);
#   endif
#else
    uint32_t allocSize = length + AsmJSPageSize;
    if (allocSize <= length)
        return nullptr;

    void *p = MapMemory(allocSize, true);
    if (!p)
        return nullptr;
#endif
    uint8_t *buffer = reinterpret_cast<uint8_t*>(p) + AsmJSPageSize;
    uint8_t *base = buffer - sizeof(SharedArrayRawBuffer);
    return new (base) SharedArrayRawBuffer(buffer, length);
}

void
SharedArrayRawBuffer::addReference()
{
    MOZ_ASSERT(this->refcount > 0);
    ++this->refcount; // Atomic.
}

void
SharedArrayRawBuffer::dropReference()
{
    // Drop the reference to the buffer.
    uint32_t refcount = --this->refcount; // Atomic.

    // If this was the final reference, release the buffer.
    if (refcount == 0) {
        uint8_t *p = this->dataPointer() - AsmJSPageSize;
        MOZ_ASSERT(uintptr_t(p) % AsmJSPageSize == 0);
#ifdef JS_CODEGEN_X64
        UnmapMemory(p, SharedArrayMappedSize);
#       if defined(MOZ_VALGRIND) \
           && defined(VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE)
        // Tell Valgrind/Memcheck to recommence reporting accesses in the
        // previously-inaccessible region.
        VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(p, SharedArrayMappedSize);
#       endif
#else
        UnmapMemory(p, this->length + AsmJSPageSize);
#endif
    }
}

const JSFunctionSpec SharedArrayBufferObject::jsfuncs[] = {
    /* Nothing yet */
    JS_FS_END
};

const JSFunctionSpec SharedArrayBufferObject::jsstaticfuncs[] = {
    JS_FN("isView", SharedArrayBufferObject::fun_isView, 1, 0),
    JS_FS_END
};

MOZ_ALWAYS_INLINE bool
SharedArrayBufferObject::byteLengthGetterImpl(JSContext *cx, CallArgs args)
{
    MOZ_ASSERT(IsSharedArrayBuffer(args.thisv()));
    args.rval().setInt32(args.thisv().toObject().as<SharedArrayBufferObject>().byteLength());
    return true;
}

bool
SharedArrayBufferObject::byteLengthGetter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsSharedArrayBuffer, byteLengthGetterImpl>(cx, args);
}

bool
SharedArrayBufferObject::fun_isView(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(args.get(0).isObject() &&
                           JS_IsSharedTypedArrayObject(&args.get(0).toObject()));
    return true;
}

bool
SharedArrayBufferObject::class_constructor(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.isConstructing()) {
        if (args.hasDefined(0) && IsObjectWithClass(args[0], ESClass_SharedArrayBuffer, cx)) {
            args.rval().set(args[0]);
            return true;
        }
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SHARED_ARRAY_BAD_OBJECT);
        return false;
    }

    uint32_t length;
    bool overflow;
    if (!ToLengthClamped(cx, args.get(0), &length, &overflow)) {
        // Bug 1068458: Limit length to 2^31-1.
        if (overflow || length > INT32_MAX)
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SHARED_ARRAY_BAD_LENGTH);
        return false;
    }

    JSObject *bufobj = New(cx, length);
    if (!bufobj)
        return false;
    args.rval().setObject(*bufobj);
    return true;
}

SharedArrayBufferObject *
SharedArrayBufferObject::New(JSContext *cx, uint32_t length)
{
    if (!IsValidAsmJSHeapLength(length)) {
        mozilla::UniquePtr<char[], JS::FreePolicy> msg;
        if (length > INT32_MAX)
            msg.reset(JS_smprintf("SharedArrayBuffer byteLength 0x%x is too large", length));
        else
            msg.reset(JS_smprintf("SharedArrayBuffer byteLength 0x%x is not a valid length. The next valid "
                                  "length is 0x%x", length, RoundUpToNextValidAsmJSHeapLength(length)));
        if (msg)
            JS_ReportError(cx, msg.get());
        return nullptr;
    }

    SharedArrayRawBuffer *buffer = SharedArrayRawBuffer::New(length);
    if (!buffer)
        return nullptr;

    return New(cx, buffer);
}

SharedArrayBufferObject *
SharedArrayBufferObject::New(JSContext *cx, SharedArrayRawBuffer *buffer)
{
    Rooted<SharedArrayBufferObject*> obj(cx, NewBuiltinClassInstance<SharedArrayBufferObject>(cx));
    if (!obj)
        return nullptr;

    MOZ_ASSERT(obj->getClass() == &class_);

    obj->acceptRawBuffer(buffer);

    return obj;
}

void
SharedArrayBufferObject::acceptRawBuffer(SharedArrayRawBuffer *buffer)
{
    setReservedSlot(RAWBUF_SLOT, PrivateValue(buffer));
}

void
SharedArrayBufferObject::dropRawBuffer()
{
    setReservedSlot(RAWBUF_SLOT, UndefinedValue());
}

SharedArrayRawBuffer *
SharedArrayBufferObject::rawBufferObject() const
{
    Value v = getReservedSlot(RAWBUF_SLOT);
    MOZ_ASSERT(!v.isUndefined());
    return reinterpret_cast<SharedArrayRawBuffer *>(v.toPrivate());
}

void
SharedArrayBufferObject::Finalize(FreeOp *fop, JSObject *obj)
{
    SharedArrayBufferObject &buf = obj->as<SharedArrayBufferObject>();

    // Detect the case of failure during SharedArrayBufferObject creation,
    // which causes a SharedArrayRawBuffer to never be attached.
    Value v = buf.getReservedSlot(RAWBUF_SLOT);
    if (!v.isUndefined()) {
        buf.rawBufferObject()->dropReference();
        buf.dropRawBuffer();
    }
}

/* static */ void
SharedArrayBufferObject::addSizeOfExcludingThis(JSObject *obj, mozilla::MallocSizeOf mallocSizeOf,
                                                JS::ClassInfo *info)
{
    info->objectsNonHeapElementsMapped += obj->as<SharedArrayBufferObject>().byteLength();
}

const Class SharedArrayBufferObject::protoClass = {
    "SharedArrayBufferPrototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_SharedArrayBuffer),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

const Class SharedArrayBufferObject::class_ = {
    "SharedArrayBuffer",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(SharedArrayBufferObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_SharedArrayBuffer),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    SharedArrayBufferObject::Finalize,
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    nullptr,        /* trace */
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT
};

JSObject *
js_InitSharedArrayBufferClass(JSContext *cx, HandleObject obj)
{
    MOZ_ASSERT(obj->isNative());
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    RootedObject proto(cx, global->createBlankPrototype(cx, &SharedArrayBufferObject::protoClass));
    if (!proto)
        return nullptr;

    RootedFunction ctor(cx, global->createConstructor(cx, SharedArrayBufferObject::class_constructor,
                                                      cx->names().SharedArrayBuffer, 1));
    if (!ctor)
        return nullptr;

    if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_SharedArrayBuffer, ctor, proto))
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    RootedId byteLengthId(cx, NameToId(cx->names().byteLength));
    unsigned attrs = JSPROP_SHARED | JSPROP_GETTER | JSPROP_PERMANENT;
    JSObject *getter = NewFunction(cx, NullPtr(), SharedArrayBufferObject::byteLengthGetter, 0,
                                   JSFunction::NATIVE_FUN, global, NullPtr());
    if (!getter)
        return nullptr;

    if (!DefineNativeProperty(cx, proto, byteLengthId, UndefinedHandleValue,
                              JS_DATA_TO_FUNC_PTR(PropertyOp, getter), nullptr, attrs))
        return nullptr;

    if (!JS_DefineFunctions(cx, ctor, SharedArrayBufferObject::jsstaticfuncs))
        return nullptr;

    if (!JS_DefineFunctions(cx, proto, SharedArrayBufferObject::jsfuncs))
        return nullptr;

    return proto;
}

bool
js::IsSharedArrayBuffer(HandleValue v)
{
    return v.isObject() && v.toObject().is<SharedArrayBufferObject>();
}

bool
js::IsSharedArrayBuffer(HandleObject o)
{
    return o->is<SharedArrayBufferObject>();
}

SharedArrayBufferObject &
js::AsSharedArrayBuffer(HandleObject obj)
{
    MOZ_ASSERT(IsSharedArrayBuffer(obj));
    return obj->as<SharedArrayBufferObject>();
}

JS_FRIEND_API(bool)
JS_IsSharedArrayBufferObject(JSObject *obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<ArrayBufferObject>() : false;
}
