/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TypedArrayObject.h"

#include "mozilla/Alignment.h"
#include "mozilla/Casting.h"
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

#include "builtin/TypedObjectConstants.h"
#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "jit/InlinableNatives.h"
#include "js/Conversions.h"
#include "vm/ArrayBufferObject.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/PIC.h"
#include "vm/SelfHosting.h"
#include "vm/TypedArrayCommon.h"
#include "vm/WrapperObject.h"

#include "jsatominlines.h"

#include "gc/Nursery-inl.h"
#include "gc/StoreBuffer-inl.h"
#include "vm/ArrayBufferObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Shape-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::AssertedCast;
using JS::CanonicalizeNaN;
using JS::ToInt32;
using JS::ToUint32;

/*
 * TypedArrayObject
 *
 * The non-templated base class for the specific typed implementations.
 * This class holds all the member variables that are used by
 * the subclasses.
 */

/* static */ int
TypedArrayObject::lengthOffset()
{
    return NativeObject::getFixedSlotOffset(LENGTH_SLOT);
}

/* static */ int
TypedArrayObject::dataOffset()
{
    return NativeObject::getPrivateDataOffset(DATA_SLOT);
}

void
TypedArrayObject::notifyBufferDetached(JSContext* cx, void* newData)
{
    MOZ_ASSERT(!isSharedMemory());
    setFixedSlot(TypedArrayObject::LENGTH_SLOT, Int32Value(0));
    setFixedSlot(TypedArrayObject::BYTEOFFSET_SLOT, Int32Value(0));

    // If the object is in the nursery, the buffer will be freed by the next
    // nursery GC. Free the data slot pointer if the object has no inline data.
    Nursery& nursery = cx->runtime()->gc.nursery;
    if (isTenured() && !hasBuffer() && !hasInlineElements() &&
        !nursery.isInside(elements()))
    {
        js_free(elements());
    }

    setPrivate(newData);
}

/* static */ bool
TypedArrayObject::is(HandleValue v)
{
    return v.isObject() && v.toObject().is<TypedArrayObject>();
}

/* static */ bool
TypedArrayObject::ensureHasBuffer(JSContext* cx, Handle<TypedArrayObject*> tarray)
{
    if (tarray->hasBuffer())
        return true;

    Rooted<ArrayBufferObject*> buffer(cx, ArrayBufferObject::create(cx, tarray->byteLength()));
    if (!buffer)
        return false;

    if (!buffer->addView(cx, tarray))
        return false;

    // tarray is not shared, because if it were it would have a buffer.
    memcpy(buffer->dataPointer(), tarray->viewDataUnshared(), tarray->byteLength());

    // If the object is in the nursery, the buffer will be freed by the next
    // nursery GC. Free the data slot pointer if the object has no inline data.
    Nursery& nursery = cx->runtime()->gc.nursery;
    if (tarray->isTenured() && !tarray->hasInlineElements() &&
        !nursery.isInside(tarray->elements()))
    {
        js_free(tarray->elements());
    }

    tarray->setPrivate(buffer->dataPointer());

    tarray->setFixedSlot(TypedArrayObject::BUFFER_SLOT, ObjectValue(*buffer));

    // Notify compiled jit code that the base pointer has moved.
    MarkObjectStateChange(cx, tarray);

    return true;
}

/* static */ void
TypedArrayObject::trace(JSTracer* trc, JSObject* objArg)
{
    // Handle all tracing required when the object has a buffer.
    ArrayBufferViewObject::trace(trc, objArg);
}

void
TypedArrayObject::finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(!IsInsideNursery(obj));
    TypedArrayObject* curObj = &obj->as<TypedArrayObject>();

    // Typed arrays with a buffer object do not need to be free'd
    if (curObj->hasBuffer())
        return;

    // Free the data slot pointer if it does not point into the old JSObject.
    if (!curObj->hasInlineElements())
        js_free(curObj->elements());
}

/* static */ void
TypedArrayObject::objectMoved(JSObject* obj, const JSObject* old)
{
    TypedArrayObject* newObj = &obj->as<TypedArrayObject>();
    const TypedArrayObject* oldObj = &old->as<TypedArrayObject>();

    // Typed arrays with a buffer object do not need an update.
    if (oldObj->hasBuffer())
        return;

    // Update the data slot pointer if it points to the old JSObject.
    if (oldObj->hasInlineElements())
        newObj->setInlineElements();
}

/* static */ size_t
TypedArrayObject::objectMovedDuringMinorGC(JSTracer* trc, JSObject* obj, const JSObject* old,
                                           gc::AllocKind newAllocKind)
{
    TypedArrayObject* newObj = &obj->as<TypedArrayObject>();
    const TypedArrayObject* oldObj = &old->as<TypedArrayObject>();
    MOZ_ASSERT(newObj->elements() == oldObj->elements());
    MOZ_ASSERT(obj->isTenured());

    // Typed arrays with a buffer object do not need an update.
    if (oldObj->hasBuffer())
        return 0;

    Nursery& nursery = trc->runtime()->gc.nursery;
    void* buf = oldObj->elements();

    if (!nursery.isInside(buf)) {
        nursery.removeMallocedBuffer(buf);
        return 0;
    }

    // Determine if we can use inline data for the target array. If this is
    // possible, the nursery will have picked an allocation size that is large
    // enough.
    size_t nbytes = 0;
    switch (oldObj->type()) {
#define OBJECT_MOVED_TYPED_ARRAY(T, N) \
      case Scalar::N: \
        nbytes = oldObj->length() * sizeof(T); \
        break;
JS_FOR_EACH_TYPED_ARRAY(OBJECT_MOVED_TYPED_ARRAY)
#undef OBJECT_MOVED_TYPED_ARRAY
      default:
        MOZ_CRASH("Unsupported TypedArray type");
    }

    size_t headerSize = dataOffset() + sizeof(HeapSlot);
    if (headerSize + nbytes <= GetGCKindBytes(newAllocKind)) {
        MOZ_ASSERT(oldObj->hasInlineElements());
        newObj->setInlineElements();
    } else {
        MOZ_ASSERT(!oldObj->hasInlineElements());
        AutoEnterOOMUnsafeRegion oomUnsafe;
        nbytes = JS_ROUNDUP(nbytes, sizeof(Value));
        void* data = newObj->zone()->pod_malloc<uint8_t>(nbytes);
        if (!data)
            oomUnsafe.crash("Failed to allocate typed array elements while tenuring.");
        MOZ_ASSERT(!nursery.isInside(data));
        newObj->initPrivate(data);
    }

    mozilla::PodCopy(newObj->elements(), oldObj->elements(), nbytes);

    // Set a forwarding pointer for the element buffers in case they were
    // preserved on the stack by Ion.
    nursery.maybeSetForwardingPointer(trc, oldObj->elements(), newObj->elements(),
                                      /* direct = */nbytes >= sizeof(uintptr_t));

    return newObj->hasInlineElements() ? 0 : nbytes;
}

bool
TypedArrayObject::hasInlineElements() const
{
    return elements() == this->fixedData(TypedArrayObject::FIXED_DATA_START) &&
        byteLength() <= TypedArrayObject::INLINE_BUFFER_LIMIT;
}

void
TypedArrayObject::setInlineElements()
{
    char* dataSlot = reinterpret_cast<char*>(this) + this->dataOffset();
    *reinterpret_cast<void**>(dataSlot) = this->fixedData(TypedArrayObject::FIXED_DATA_START);
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
        return y & ~1;
    }

    return y;
}

template<typename ElementType>
static inline JSObject*
NewArray(JSContext* cx, uint32_t nelements);

namespace {

// We allow nullptr for newTarget for all the creation methods, to allow for
// JSFriendAPI functions that don't care about subclassing
static bool
GetPrototypeForInstance(JSContext* cx, HandleObject newTarget, MutableHandleObject proto)
{
    if (newTarget) {
        if (!GetPrototypeFromConstructor(cx, newTarget, proto))
            return false;
    } else {
        proto.set(nullptr);
    }
    return true;
}

enum class SpeciesConstructorOverride {
    None,
    ArrayBuffer
};

template<typename NativeType>
class TypedArrayObjectTemplate : public TypedArrayObject
{
    friend class TypedArrayObject;

  public:
    typedef NativeType ElementType;

    static constexpr Scalar::Type ArrayTypeID() { return TypeIDOfType<NativeType>::id; }
    static bool ArrayTypeIsUnsigned() { return TypeIsUnsigned<NativeType>(); }
    static bool ArrayTypeIsFloatingPoint() { return TypeIsFloatingPoint<NativeType>(); }

    static const size_t BYTES_PER_ELEMENT = sizeof(NativeType);

    static JSObject*
    createPrototype(JSContext* cx, JSProtoKey key)
    {
        Handle<GlobalObject*> global = cx->global();
        RootedObject typedArrayProto(cx, GlobalObject::getOrCreateTypedArrayPrototype(cx, global));
        if (!typedArrayProto)
            return nullptr;

        const Class* clasp = TypedArrayObject::protoClassForType(ArrayTypeID());
        return global->createBlankPrototypeInheriting(cx, clasp, typedArrayProto);
    }

    static JSObject*
    createConstructor(JSContext* cx, JSProtoKey key)
    {
        Handle<GlobalObject*> global = cx->global();
        RootedFunction ctorProto(cx, GlobalObject::getOrCreateTypedArrayConstructor(cx, global));
        if (!ctorProto)
            return nullptr;

        JSFunction* fun = NewFunctionWithProto(cx, class_constructor, 3,
                                               JSFunction::NATIVE_CTOR, nullptr,
                                               ClassName(key, cx),
                                               ctorProto, gc::AllocKind::FUNCTION,
                                               SingletonObject);

        if (fun)
            fun->setJitInfo(&jit::JitInfo_TypedArrayConstructor);

        return fun;
    }

    static bool
    getOrCreateCreateArrayFromBufferFunction(JSContext* cx, MutableHandleValue fval)
    {
        RootedValue cache(cx, cx->global()->createArrayFromBuffer<NativeType>());
        if (cache.isObject()) {
            MOZ_ASSERT(cache.toObject().is<JSFunction>());
            fval.set(cache);
            return true;
        }

        RootedFunction fun(cx);
        fun = NewNativeFunction(cx, ArrayBufferObject::createTypedArrayFromBuffer<NativeType>,
                                0, nullptr);
        if (!fun)
            return false;

        cx->global()->setCreateArrayFromBuffer<NativeType>(fun);

        fval.setObject(*fun);
        return true;
    }

    static inline const Class* instanceClass()
    {
        return TypedArrayObject::classForType(ArrayTypeID());
    }

    static bool is(HandleValue v) {
        return v.isObject() && v.toObject().hasClass(instanceClass());
    }

    static void
    setIndexValue(TypedArrayObject& tarray, uint32_t index, double d)
    {
        // If the array is an integer array, we only handle up to
        // 32-bit ints from this point on.  if we want to handle
        // 64-bit ints, we'll need some changes.

        // Assign based on characteristics of the destination type
        if (ArrayTypeIsFloatingPoint()) {
            setIndex(tarray, index, NativeType(d));
        } else if (ArrayTypeIsUnsigned()) {
            MOZ_ASSERT(sizeof(NativeType) <= 4);
            uint32_t n = ToUint32(d);
            setIndex(tarray, index, NativeType(n));
        } else if (ArrayTypeID() == Scalar::Uint8Clamped) {
            // The uint8_clamped type has a special rounding converter
            // for doubles.
            setIndex(tarray, index, NativeType(d));
        } else {
            MOZ_ASSERT(sizeof(NativeType) <= 4);
            int32_t n = ToInt32(d);
            setIndex(tarray, index, NativeType(n));
        }
    }

    static TypedArrayObject*
    makeProtoInstance(JSContext* cx, HandleObject proto, AllocKind allocKind)
    {
        MOZ_ASSERT(proto);

        JSObject* obj = NewObjectWithClassProto(cx, instanceClass(), proto, allocKind);
        return obj ? &obj->as<TypedArrayObject>() : nullptr;
    }

    static TypedArrayObject*
    makeTypedInstance(JSContext* cx, uint32_t len, gc::AllocKind allocKind)
    {
        const Class* clasp = instanceClass();
        if (len * sizeof(NativeType) >= TypedArrayObject::SINGLETON_BYTE_LENGTH) {
            JSObject* obj = NewBuiltinClassInstance(cx, clasp, allocKind, SingletonObject);
            if (!obj)
                return nullptr;
            return &obj->as<TypedArrayObject>();
        }

        jsbytecode* pc;
        RootedScript script(cx, cx->currentScript(&pc));
        NewObjectKind newKind = GenericObject;
        if (script && ObjectGroup::useSingletonForAllocationSite(script, pc, clasp))
            newKind = SingletonObject;
        RootedObject obj(cx, NewBuiltinClassInstance(cx, clasp, allocKind, newKind));
        if (!obj)
            return nullptr;

        if (script && !ObjectGroup::setAllocationSiteObjectGroup(cx, script, pc, obj,
                                                                 newKind == SingletonObject))
        {
            return nullptr;
        }

        return &obj->as<TypedArrayObject>();
    }

    static TypedArrayObject*
    makeInstance(JSContext* cx, Handle<ArrayBufferObjectMaybeShared*> buffer, uint32_t byteOffset, uint32_t len,
                 HandleObject proto)
    {
        MOZ_ASSERT_IF(!buffer, byteOffset == 0);

        gc::AllocKind allocKind = buffer
                                  ? GetGCObjectKind(instanceClass())
                                  : AllocKindForLazyBuffer(len * sizeof(NativeType));

        // Subclassing mandates that we hand in the proto every time. Most of
        // the time, though, that [[Prototype]] will not be interesting. If
        // it isn't, we can do some more TI optimizations.
        RootedObject checkProto(cx);
        if (!GetBuiltinPrototype(cx, JSCLASS_CACHED_PROTO_KEY(instanceClass()), &checkProto))
            return nullptr;

        AutoSetNewObjectMetadata metadata(cx);
        Rooted<TypedArrayObject*> obj(cx);
        if (proto && proto != checkProto)
            obj = makeProtoInstance(cx, proto, allocKind);
        else
            obj = makeTypedInstance(cx, len, allocKind);
        if (!obj)
            return nullptr;

        bool isSharedMemory = buffer && IsSharedArrayBuffer(buffer.get());

        obj->setFixedSlot(TypedArrayObject::BUFFER_SLOT, ObjectOrNullValue(buffer));
        // This is invariant.  Self-hosting code that sets BUFFER_SLOT
        // (if it does) must maintain it, should it need to.
        if (isSharedMemory)
            obj->setIsSharedMemory();

        if (buffer) {
            obj->initViewData(buffer->dataPointerEither() + byteOffset);

            // If the buffer is for an inline typed object, the data pointer
            // may be in the nursery, so include a barrier to make sure this
            // object is updated if that typed object moves.
            auto ptr = buffer->dataPointerEither();
            if (!IsInsideNursery(obj) && cx->runtime()->gc.nursery.isInside(ptr)) {
                // Shared buffer data should never be nursery-allocated, so we
                // need to fail here if isSharedMemory.  However, mmap() can
                // place a SharedArrayRawBuffer up against the bottom end of a
                // nursery chunk, and a zero-length buffer will erroneously be
                // perceived as being inside the nursery; sidestep that.
                if (isSharedMemory) {
                    MOZ_ASSERT(buffer->byteLength() == 0 &&
                               (uintptr_t(ptr.unwrapValue()) & gc::ChunkMask) == 0);
                } else {
                    cx->runtime()->gc.storeBuffer.putWholeCell(obj);
                }
            }
        } else {
            void* data = obj->fixedData(FIXED_DATA_START);
            obj->initPrivate(data);
            memset(data, 0, len * sizeof(NativeType));
        }

        obj->setFixedSlot(TypedArrayObject::LENGTH_SLOT, Int32Value(len));
        obj->setFixedSlot(TypedArrayObject::BYTEOFFSET_SLOT, Int32Value(byteOffset));

#ifdef DEBUG
        if (buffer) {
            uint32_t arrayByteLength = obj->byteLength();
            uint32_t arrayByteOffset = obj->byteOffset();
            uint32_t bufferByteLength = buffer->byteLength();
            // Unwraps are safe: both are for the pointer value.
            if (IsArrayBuffer(buffer.get())) {
                MOZ_ASSERT_IF(!AsArrayBuffer(buffer.get()).isDetached(),
                              buffer->dataPointerEither().unwrap(/*safe*/) <= obj->viewDataEither().unwrap(/*safe*/));
            }
            MOZ_ASSERT(bufferByteLength - arrayByteOffset >= arrayByteLength);
            MOZ_ASSERT(arrayByteOffset <= bufferByteLength);
        }

        // Verify that the private slot is at the expected place
        MOZ_ASSERT(obj->numFixedSlots() == TypedArrayObject::DATA_SLOT);
#endif

        // ArrayBufferObjects track their views to support detaching.
        if (buffer && buffer->is<ArrayBufferObject>()) {
            if (!buffer->as<ArrayBufferObject>().addView(cx, obj))
                return nullptr;
        }

        return obj;
    }

    static TypedArrayObject*
    makeInstance(JSContext* cx, Handle<ArrayBufferObjectMaybeShared*> buffer,
                 uint32_t byteOffset, uint32_t len)
    {
        RootedObject proto(cx, nullptr);
        return makeInstance(cx, buffer, byteOffset, len, proto);
    }

    static TypedArrayObject*
    makeTemplateObject(JSContext* cx, int32_t len)
    {
        MOZ_ASSERT(len >= 0);
        size_t nbytes;
        MOZ_ALWAYS_TRUE(CalculateAllocSize<NativeType>(len, &nbytes));
        MOZ_ASSERT(nbytes < TypedArrayObject::SINGLETON_BYTE_LENGTH);
        NewObjectKind newKind = TenuredObject;
        bool fitsInline = nbytes <= INLINE_BUFFER_LIMIT;
        const Class* clasp = instanceClass();
        gc::AllocKind allocKind = !fitsInline
                                  ? GetGCObjectKind(clasp)
                                  : AllocKindForLazyBuffer(nbytes);
        MOZ_ASSERT(CanBeFinalizedInBackground(allocKind, clasp));
        allocKind = GetBackgroundAllocKind(allocKind);

        AutoSetNewObjectMetadata metadata(cx);
        jsbytecode* pc;
        RootedScript script(cx, cx->currentScript(&pc));
        if (script && ObjectGroup::useSingletonForAllocationSite(script, pc, clasp))
            newKind = SingletonObject;
        RootedObject tmp(cx, NewBuiltinClassInstance(cx, clasp, allocKind, newKind));
        if (!tmp)
            return nullptr;
        if (script && !ObjectGroup::setAllocationSiteObjectGroup(cx, script, pc, tmp,
                                                                 newKind == SingletonObject))
        {
            return nullptr;
        }

        TypedArrayObject* tarray = &tmp->as<TypedArrayObject>();
        initTypedArraySlots(cx, tarray, len);

        // Template objects do not need memory for its elements, since there
        // won't be any elements to store. Therefore, we set the pointer to
        // nullptr and avoid allocating memory that will never be used.
        tarray->initPrivate(nullptr);

        return tarray;
    }

    static void
    initTypedArraySlots(JSContext* cx, TypedArrayObject* tarray, int32_t len)
    {
        MOZ_ASSERT(len >= 0);
        tarray->setFixedSlot(TypedArrayObject::BUFFER_SLOT, NullValue());
        tarray->setFixedSlot(TypedArrayObject::LENGTH_SLOT, Int32Value(AssertedCast<int32_t>(len)));
        tarray->setFixedSlot(TypedArrayObject::BYTEOFFSET_SLOT, Int32Value(0));

        // Verify that the private slot is at the expected place.
        MOZ_ASSERT(tarray->numFixedSlots() == TypedArrayObject::DATA_SLOT);
    }

    static void
    initTypedArrayData(JSContext* cx, TypedArrayObject* tarray, int32_t len,
                       void* buf, AllocKind allocKind)
    {
        if (buf) {
#ifdef DEBUG
            Nursery& nursery = cx->runtime()->gc.nursery;
            MOZ_ASSERT_IF(!nursery.isInside(buf) && !tarray->hasInlineElements(),
                          tarray->isTenured());
#endif
            tarray->initPrivate(buf);
        } else {
            size_t nbytes = len * sizeof(NativeType);
#ifdef DEBUG
            size_t dataOffset = TypedArrayObject::dataOffset();
            size_t offset = dataOffset + sizeof(HeapSlot);
            MOZ_ASSERT(offset + nbytes <= GetGCKindBytes(allocKind));
#endif

            void* data = tarray->fixedData(FIXED_DATA_START);
            tarray->initPrivate(data);
            memset(data, 0, nbytes);
        }
    }

    static TypedArrayObject*
    makeTypedArrayWithTemplate(JSContext* cx, TypedArrayObject* templateObj, int32_t len)
    {
        size_t nbytes;
        if (len < 0 || !js::CalculateAllocSize<NativeType>(len, &nbytes)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return nullptr;
        }

        bool fitsInline = nbytes <= INLINE_BUFFER_LIMIT;

        AutoSetNewObjectMetadata metadata(cx);

        const Class* clasp = templateObj->group()->clasp();
        gc::AllocKind allocKind = !fitsInline
                                  ? GetGCObjectKind(clasp)
                                  : AllocKindForLazyBuffer(nbytes);
        MOZ_ASSERT(CanBeFinalizedInBackground(allocKind, clasp));
        allocKind = GetBackgroundAllocKind(allocKind);
        RootedObjectGroup group(cx, templateObj->group());

        NewObjectKind newKind = TenuredObject;

        ScopedJSFreePtr<void> buf;
        if (!fitsInline && len > 0) {
            buf = cx->zone()->pod_malloc<uint8_t>(nbytes);
            if (!buf) {
                ReportOutOfMemory(cx);
                return nullptr;
            }

            memset(buf, 0, nbytes);
         }

        RootedObject tmp(cx, NewObjectWithGroup<TypedArrayObject>(cx, group, allocKind, newKind));
        if (!tmp)
            return nullptr;

        TypedArrayObject* obj = &tmp->as<TypedArrayObject>();
        initTypedArraySlots(cx, obj, len);
        initTypedArrayData(cx, obj, len, buf.forget(), allocKind);

        return obj;
    }

    /*
     * new [Type]Array(length)
     * new [Type]Array(otherTypedArray)
     * new [Type]Array(JSArray)
     * new [Type]Array(ArrayBuffer, [optional] byteOffset, [optional] length)
     */
    static bool
    class_constructor(JSContext* cx, unsigned argc, Value* vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);

        if (!ThrowIfNotConstructing(cx, args, "typed array"))
            return false;

        JSObject* obj = create(cx, args);
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    static JSObject*
    create(JSContext* cx, const CallArgs& args)
    {
        MOZ_ASSERT(args.isConstructing());
        RootedObject newTarget(cx, &args.newTarget().toObject());

        /* () or (number) */
        uint32_t len = 0;
        if (args.length() == 0 || ValueIsLength(args[0], &len))
            return fromLength(cx, len, newTarget);

        /* (not an object) */
        if (!args[0].isObject()) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return nullptr;
        }

        RootedObject dataObj(cx, &args.get(0).toObject());

        /*
         * (typedArray)
         * (sharedTypedArray)
         * (type[] array)
         *
         * Otherwise create a new typed array and copy elements 0..len-1
         * properties from the object, treating it as some sort of array.
         * Note that offset and length will be ignored.  Note that a
         * shared array's values are copied here.
         */
        if (!UncheckedUnwrap(dataObj)->is<ArrayBufferObjectMaybeShared>())
            return fromArray(cx, dataObj, newTarget);

        /* (ArrayBuffer, [byteOffset, [length]]) */
        RootedObject proto(cx);
        if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
            return nullptr;

        int32_t byteOffset = 0;
        int32_t length = -1;

        if (args.length() > 1) {
            if (!ToInt32(cx, args[1], &byteOffset))
                return nullptr;
            if (byteOffset < 0) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                          JSMSG_TYPED_ARRAY_NEGATIVE_ARG,
                                          "1");
                return nullptr;
            }

            if (args.length() > 2) {
                if (!ToInt32(cx, args[2], &length))
                    return nullptr;
                if (length < 0) {
                    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                              JSMSG_TYPED_ARRAY_NEGATIVE_ARG,
                                              "2");
                    return nullptr;
                }
            }
        }

        return fromBufferWithProto(cx, dataObj, byteOffset, length, proto);
    }

  public:
    static JSObject*
    fromBuffer(JSContext* cx, HandleObject bufobj, uint32_t byteOffset, int32_t lengthInt) {
        return fromBufferWithProto(cx, bufobj, byteOffset, lengthInt, nullptr);
    }

    static JSObject*
    fromBufferWithProto(JSContext* cx, HandleObject bufobj, uint32_t byteOffset, int32_t lengthInt,
                        HandleObject proto)
    {
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
            JSObject* wrapped = CheckedUnwrap(bufobj);
            if (!wrapped) {
                JS_ReportErrorASCII(cx, "Permission denied to access object");
                return nullptr;
            }

            if (!IsAnyArrayBuffer(wrapped)) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
                return nullptr; // must be arrayBuffer
            }

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

            RootedObject protoRoot(cx, proto);
            if (!protoRoot) {
                if (!GetBuiltinPrototype(cx, JSCLASS_CACHED_PROTO_KEY(instanceClass()), &protoRoot))
                    return nullptr;
            }

            FixedInvokeArgs<3> args(cx);

            args[0].setNumber(byteOffset);
            args[1].setInt32(lengthInt);
            args[2].setObject(*protoRoot);

            RootedValue fval(cx);
            if (!getOrCreateCreateArrayFromBufferFunction(cx, &fval))
                return nullptr;

            RootedValue thisv(cx, ObjectValue(*bufobj));
            RootedValue rval(cx);
            if (!js::Call(cx, fval, thisv, args, &rval))
                return nullptr;

            return &rval.toObject();
        }

        if (!IsAnyArrayBuffer(bufobj)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return nullptr; // must be arrayBuffer
        }

        Rooted<ArrayBufferObjectMaybeShared*> buffer(cx);
        if (IsArrayBuffer(bufobj)) {
            ArrayBufferObject& buf = AsArrayBuffer(bufobj);
            if (buf.isDetached()) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
                return nullptr;
            }

            buffer = static_cast<ArrayBufferObjectMaybeShared*>(&buf);
        } else {
            buffer = static_cast<ArrayBufferObjectMaybeShared*>(&AsSharedArrayBuffer(bufobj));
        }

        if (byteOffset > buffer->byteLength() || byteOffset % sizeof(NativeType) != 0) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_TYPED_ARRAY_CONSTRUCT_BOUNDS);
            return nullptr; // invalid byteOffset
        }

        uint32_t len;
        if (lengthInt == -1) {
            len = (buffer->byteLength() - byteOffset) / sizeof(NativeType);
            if (len * sizeof(NativeType) != buffer->byteLength() - byteOffset) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                          JSMSG_TYPED_ARRAY_CONSTRUCT_BOUNDS);
                return nullptr; // given byte array doesn't map exactly to sizeof(NativeType) * N
            }
        } else {
            len = uint32_t(lengthInt);
        }

        // Go slowly and check for overflow.
        uint32_t arrayByteLength = len * sizeof(NativeType);
        if (len >= INT32_MAX / sizeof(NativeType) || byteOffset >= INT32_MAX - arrayByteLength) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_TYPED_ARRAY_CONSTRUCT_BOUNDS);
            return nullptr; // overflow when calculating byteOffset + len * sizeof(NativeType)
        }

        if (arrayByteLength + byteOffset > buffer->byteLength()) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_TYPED_ARRAY_CONSTRUCT_BOUNDS);
            return nullptr; // byteOffset + len is too big for the arraybuffer
        }

        return makeInstance(cx, buffer, byteOffset, len, proto);
    }

    static bool
    maybeCreateArrayBuffer(JSContext* cx, uint32_t count, uint32_t unit,
                           HandleObject nonDefaultProto,
                           MutableHandle<ArrayBufferObject*> buffer)
    {
        if (count >= INT32_MAX / unit) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NEED_DIET,
                                      "size and count");
            return false;
        }
        uint32_t byteLength = count * unit;

        MOZ_ASSERT(byteLength < INT32_MAX);
        static_assert(INLINE_BUFFER_LIMIT % sizeof(NativeType) == 0,
                      "ArrayBuffer inline storage shouldn't waste any space");

        if (!nonDefaultProto && byteLength <= INLINE_BUFFER_LIMIT) {
            // The array's data can be inline, and the buffer created lazily.
            return true;
        }

        ArrayBufferObject* buf = ArrayBufferObject::create(cx, byteLength, nonDefaultProto);
        if (!buf)
            return false;

        buffer.set(buf);
        return true;
    }

    static JSObject*
    fromLength(JSContext* cx, uint32_t nelements, HandleObject newTarget = nullptr)
    {
        RootedObject proto(cx);
        if (!GetPrototypeForInstance(cx, newTarget, &proto))
            return nullptr;

        Rooted<ArrayBufferObject*> buffer(cx);
        if (!maybeCreateArrayBuffer(cx, nelements, BYTES_PER_ELEMENT, nullptr, &buffer))
            return nullptr;

        return makeInstance(cx, buffer, 0, nelements, proto);
    }

    static bool
    AllocateArrayBuffer(JSContext* cx, HandleValue ctor,
                        uint32_t count, uint32_t unit,
                        MutableHandle<ArrayBufferObject*> buffer);

    static bool
    CloneArrayBufferNoCopy(JSContext* cx, Handle<ArrayBufferObjectMaybeShared*> srcBuffer,
                           bool isWrapped, uint32_t srcByteOffset, uint32_t srcLength,
                           SpeciesConstructorOverride override,
                           MutableHandle<ArrayBufferObject*> buffer);

    static JSObject*
    fromArray(JSContext* cx, HandleObject other, HandleObject newTarget = nullptr);

    static JSObject*
    fromTypedArray(JSContext* cx, HandleObject other, bool isWrapped, HandleObject newTarget);

    static JSObject*
    fromObject(JSContext* cx, HandleObject other, HandleObject newTarget);

    static const NativeType
    getIndex(JSObject* obj, uint32_t index)
    {
        TypedArrayObject& tarray = obj->as<TypedArrayObject>();
        MOZ_ASSERT(index < tarray.length());
        return jit::AtomicOperations::loadSafeWhenRacy(tarray.viewDataEither().cast<NativeType*>() + index);
    }

    static void
    setIndex(TypedArrayObject& tarray, uint32_t index, NativeType val)
    {
        MOZ_ASSERT(index < tarray.length());
        jit::AtomicOperations::storeSafeWhenRacy(tarray.viewDataEither().cast<NativeType*>() + index, val);
    }

    static Value getIndexValue(JSObject* tarray, uint32_t index);
};

#define CREATE_TYPE_FOR_TYPED_ARRAY(T, N) \
    typedef TypedArrayObjectTemplate<T> N##Array;
JS_FOR_EACH_TYPED_ARRAY(CREATE_TYPE_FOR_TYPED_ARRAY)
#undef CREATE_TYPE_FOR_TYPED_ARRAY

} /* anonymous namespace */

TypedArrayObject*
js::TypedArrayCreateWithTemplate(JSContext* cx, HandleObject templateObj, int32_t len)
{
    MOZ_ASSERT(templateObj->is<TypedArrayObject>());
    TypedArrayObject* tobj = &templateObj->as<TypedArrayObject>();

    switch (tobj->type()) {
#define CREATE_TYPED_ARRAY(T, N) \
      case Scalar::N: \
        return TypedArrayObjectTemplate<T>::makeTypedArrayWithTemplate(cx, tobj, len);
JS_FOR_EACH_TYPED_ARRAY(CREATE_TYPED_ARRAY)
#undef CREATE_TYPED_ARRAY
      default:
        MOZ_CRASH("Unsupported TypedArray type");
    }
}

template<typename T>
struct TypedArrayObject::OfType
{
    typedef TypedArrayObjectTemplate<T> Type;
};

// ES 2016 draft Mar 25, 2016 24.1.1.1.
// byteLength = count * unit
template<typename T>
/* static */ bool
TypedArrayObjectTemplate<T>::AllocateArrayBuffer(JSContext* cx, HandleValue ctor,
                                                 uint32_t count, uint32_t unit,
                                                 MutableHandle<ArrayBufferObject*> buffer)
{
    // ES 2016 draft Mar 25, 2016 24.1.1.1 step 1 (partially).
    // ES 2016 draft Mar 25, 2016 9.1.14 steps 1-2.
    MOZ_ASSERT(ctor.isObject());
    RootedObject proto(cx);
    RootedObject ctorObj(cx, &ctor.toObject());
    if (!GetPrototypeFromConstructor(cx, ctorObj, &proto))
        return false;
    JSObject* arrayBufferProto = GlobalObject::getOrCreateArrayBufferPrototype(cx, cx->global());
    if (!arrayBufferProto)
        return false;
    if (proto == arrayBufferProto)
        proto = nullptr;

    // ES 2016 draft Mar 25, 2016 24.1.1.1 steps 1 (remaining part), 2-6.
    if (!maybeCreateArrayBuffer(cx, count, unit, proto, buffer))
        return false;

    return true;
}

static bool
IsArrayBufferConstructor(const Value& v)
{
    return v.isObject() &&
           v.toObject().is<JSFunction>() &&
           v.toObject().as<JSFunction>().isNative() &&
           v.toObject().as<JSFunction>().native() == ArrayBufferObject::class_constructor;
}

static bool
IsArrayBufferSpecies(JSContext* cx, HandleObject origBuffer)
{
    RootedValue ctor(cx);
    if (!GetPropertyPure(cx, origBuffer, NameToId(cx->names().constructor), ctor.address()))
        return false;

    if (!IsArrayBufferConstructor(ctor))
        return false;

    RootedObject ctorObj(cx, &ctor.toObject());
    RootedId speciesId(cx, SYMBOL_TO_JSID(cx->wellKnownSymbols().species));
    JSFunction* getter;
    if (!GetGetterPure(cx, ctorObj, speciesId, &getter))
        return false;

    if (!getter)
        return false;

    return IsSelfHostedFunctionWithName(getter, cx->names().ArrayBufferSpecies);
}

static bool
GetSpeciesConstructor(JSContext* cx, HandleObject obj, bool isWrapped,
                      SpeciesConstructorOverride override, MutableHandleValue ctor)
{
    if (!isWrapped) {
        if (!GlobalObject::ensureConstructor(cx, cx->global(), JSProto_ArrayBuffer))
            return false;
        RootedValue defaultCtor(cx, cx->global()->getConstructor(JSProto_ArrayBuffer));
        // The second disjunct is an optimization.
        if (override == SpeciesConstructorOverride::ArrayBuffer || IsArrayBufferSpecies(cx, obj))
            ctor.set(defaultCtor);
        else if (!SpeciesConstructor(cx, obj, defaultCtor, ctor))
            return false;

        return true;
    }

    {
        JSAutoCompartment ac(cx, obj);
        if (!GlobalObject::ensureConstructor(cx, cx->global(), JSProto_ArrayBuffer))
            return false;
        RootedValue defaultCtor(cx, cx->global()->getConstructor(JSProto_ArrayBuffer));
        if (override == SpeciesConstructorOverride::ArrayBuffer)
            ctor.set(defaultCtor);
        else if (!SpeciesConstructor(cx, obj, defaultCtor, ctor))
            return false;
    }

    return JS_WrapValue(cx, ctor);
}

// ES 2017 draft rev 8633ffd9394b203b8876bb23cb79aff13eb07310 24.1.1.4.
template<typename T>
/* static */ bool
TypedArrayObjectTemplate<T>::CloneArrayBufferNoCopy(JSContext* cx,
                                                    Handle<ArrayBufferObjectMaybeShared*> srcBuffer,
                                                    bool isWrapped, uint32_t srcByteOffset,
                                                    uint32_t srcLength,
                                                    SpeciesConstructorOverride override,
                                                    MutableHandle<ArrayBufferObject*> buffer)
{
    // Step 1 (skipped).

    // Step 2.a.
    RootedValue cloneCtor(cx);
    if (!GetSpeciesConstructor(cx, srcBuffer, isWrapped, override, &cloneCtor))
        return false;

    // Step 2.b.
    if (srcBuffer->isDetached()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
        return false;
    }

    // Steps 3-4 (skipped).

    // Steps 5.
    if (!AllocateArrayBuffer(cx, cloneCtor, srcLength, 1, buffer))
        return false;

    // Step 6.
    if (srcBuffer->isDetached()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
        return false;
    }

    // Steps 7-8 (done in caller).

    // Step 9.
    return true;
}

template<typename T>
/* static */ JSObject*
TypedArrayObjectTemplate<T>::fromArray(JSContext* cx, HandleObject other,
                                       HandleObject newTarget /* = nullptr */)
{
    // Allow nullptr newTarget for FriendAPI methods, which don't care about
    // subclassing.
    if (other->is<TypedArrayObject>())
        return fromTypedArray(cx, other, /* wrapped= */ false, newTarget);

    if (other->is<WrapperObject>() && UncheckedUnwrap(other)->is<TypedArrayObject>())
        return fromTypedArray(cx, other, /* wrapped= */ true, newTarget);

    return fromObject(cx, other, newTarget);
}

// ES 2017 draft rev 8633ffd9394b203b8876bb23cb79aff13eb07310 22.2.4.3.
template<typename T>
/* static */ JSObject*
TypedArrayObjectTemplate<T>::fromTypedArray(JSContext* cx, HandleObject other, bool isWrapped,
                                            HandleObject newTarget)
{
    // Step 1.
    MOZ_ASSERT_IF(!isWrapped, other->is<TypedArrayObject>());
    MOZ_ASSERT_IF(isWrapped,
                  other->is<WrapperObject>() &&
                  UncheckedUnwrap(other)->is<TypedArrayObject>());

    // Step 2 (done in caller).

    // Step 4 (partially).
    RootedObject proto(cx);
    if (!GetPrototypeForInstance(cx, newTarget, &proto))
        return nullptr;

    // Step 5.
    Rooted<TypedArrayObject*> srcArray(cx);
    if (!isWrapped) {
        srcArray = &other->as<TypedArrayObject>();
        if (!TypedArrayObject::ensureHasBuffer(cx, srcArray))
            return nullptr;
    } else {
        RootedObject unwrapped(cx, CheckedUnwrap(other));
        if (!unwrapped) {
            JS_ReportErrorASCII(cx, "Permission denied to access object");
            return nullptr;
        }

        JSAutoCompartment ac(cx, unwrapped);

        srcArray = &unwrapped->as<TypedArrayObject>();
        if (!TypedArrayObject::ensureHasBuffer(cx, srcArray))
            return nullptr;
    }

    // Step 6.
    Rooted<ArrayBufferObjectMaybeShared*> srcData(cx, srcArray->bufferEither());

    // Step 7.
    if (srcData->isDetached()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
        return nullptr;
    }

    // Steps 10.
    uint32_t elementLength = srcArray->length();

    // Steps 11-12.
    Scalar::Type srcType = srcArray->type();

    // Step 13 (skipped).

    // Step 14.
    uint32_t srcByteOffset = srcArray->byteOffset();

    // Step 17, modified for SharedArrayBuffer.
    bool isShared = srcArray->isSharedMemory();
    SpeciesConstructorOverride override = isShared ? SpeciesConstructorOverride::ArrayBuffer
                                                   : SpeciesConstructorOverride::None;

    // Steps 8-9, 17.
    Rooted<ArrayBufferObject*> buffer(cx);
    if (ArrayTypeID() == srcType) {
        // Step 17.a.
        uint32_t srcLength = srcArray->byteLength();

        // Step 17.b, modified for SharedArrayBuffer
        if (!CloneArrayBufferNoCopy(cx, srcData, isWrapped, srcByteOffset, srcLength, override,
                                    &buffer))
        {
            return nullptr;
        }
    } else {
        // Step 18.a, modified for SharedArrayBuffer
        RootedValue bufferCtor(cx);
        if (!GetSpeciesConstructor(cx, srcData, isWrapped, override, &bufferCtor))
            return nullptr;

        // Step 15-16, 18.b.
        if (!AllocateArrayBuffer(cx, bufferCtor, elementLength, BYTES_PER_ELEMENT, &buffer))
            return nullptr;

        // Step 18.c.
        if (srcArray->hasDetachedBuffer()) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
            return nullptr;
        }
    }

    // Steps 3, 4 (remaining part), 19-22.
    Rooted<TypedArrayObject*> obj(cx, makeInstance(cx, buffer, 0, elementLength, proto));
    if (!obj)
        return nullptr;

    // Step 18.d-g or 24.1.1.4 step 11.
    if (!TypedArrayMethods<TypedArrayObject>::setFromTypedArray(cx, obj, srcArray))
        return nullptr;

    // Step 23.
    return obj;
}

static MOZ_ALWAYS_INLINE bool
IsOptimizableInit(JSContext* cx, HandleObject iterable, bool* optimized)
{
    MOZ_ASSERT(!*optimized);

    if (!IsPackedArray(iterable))
        return true;

    ForOfPIC::Chain* stubChain = ForOfPIC::getOrCreate(cx);
    if (!stubChain)
        return false;

    return stubChain->tryOptimizeArray(cx, iterable.as<ArrayObject>(), optimized);
}

// ES2017 draft rev 6859bb9ccaea9c6ede81d71e5320e3833b92cb3e
// 22.2.4.4 TypedArray ( object )
template<typename T>
/* static */ JSObject*
TypedArrayObjectTemplate<T>::fromObject(JSContext* cx, HandleObject other, HandleObject newTarget)
{
    // Steps 1-2 (Already performed in caller).

    // Steps 3-4 (Allocation deferred until later).
    RootedObject proto(cx);
    if (!GetPrototypeForInstance(cx, newTarget, &proto))
        return nullptr;

    bool optimized = false;
    if (!IsOptimizableInit(cx, other, &optimized))
        return nullptr;

    // Fast path when iterable is a packed array using the default iterator.
    if (optimized) {
        // Step 6.a (We don't need to call IterableToList for the fast path).
        RootedArrayObject array(cx, &other->as<ArrayObject>());

        // Step 6.b.
        uint32_t len = array->getDenseInitializedLength();

        // Step 6.c.
        Rooted<ArrayBufferObject*> buffer(cx);
        if (!maybeCreateArrayBuffer(cx, len, BYTES_PER_ELEMENT, nullptr, &buffer))
            return nullptr;

        Rooted<TypedArrayObject*> obj(cx, makeInstance(cx, buffer, 0, len, proto));
        if (!obj)
            return nullptr;

        // Steps 6.d-e.
        if (!TypedArrayMethods<TypedArrayObject>::initFromIterablePackedArray(cx, obj, array))
            return nullptr;

        // Step 6.f (The assertion isn't applicable for the fast path).

        // Step 6.g.
        return obj;
    }

    // Step 5.
    RootedValue callee(cx);
    RootedId iteratorId(cx, SYMBOL_TO_JSID(cx->wellKnownSymbols().iterator));
    if (!GetProperty(cx, other, other, iteratorId, &callee))
        return nullptr;

    // Steps 6-8.
    RootedObject arrayLike(cx);
    if (!callee.isNullOrUndefined()) {
        // Throw if other[Symbol.iterator] isn't callable.
        if (!callee.isObject() || !callee.toObject().isCallable()) {
            RootedValue otherVal(cx, ObjectValue(*other));
            UniqueChars bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, otherVal, nullptr);
            if (!bytes)
                return nullptr;
            JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr, JSMSG_NOT_ITERABLE,
                                       bytes.get());
            return nullptr;
        }

        FixedInvokeArgs<2> args2(cx);
        args2[0].setObject(*other);
        args2[1].set(callee);

        // Step 6.a.
        RootedValue rval(cx);
        if (!CallSelfHostedFunction(cx, cx->names().IterableToList, UndefinedHandleValue, args2,
                                    &rval))
        {
            return nullptr;
        }

        // Steps 6.b-g (Implemented in steps 9-13 below).
        arrayLike = &rval.toObject();
    } else {
        // Step 7 is an assertion: object is not an Iterator. Testing this is
        // literally the very last thing we did, so we don't assert here.

        // Step 8.
        arrayLike = other;
    }

    // Step 9.
    uint32_t len;
    if (!GetLengthProperty(cx, arrayLike, &len))
        return nullptr;

    // Step 10.
    Rooted<ArrayBufferObject*> buffer(cx);
    if (!maybeCreateArrayBuffer(cx, len, BYTES_PER_ELEMENT, nullptr, &buffer))
        return nullptr;

    Rooted<TypedArrayObject*> obj(cx, makeInstance(cx, buffer, 0, len, proto));
    if (!obj)
        return nullptr;

    // Steps 11-12.
    if (!TypedArrayMethods<TypedArrayObject>::setFromNonTypedArray(cx, obj, arrayLike, len))
        return nullptr;

    // Step 13.
    return obj;
}

bool
TypedArrayConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_CALL_OR_CONSTRUCT,
                              args.isConstructing() ? "construct" : "call");
    return false;
}

/* static */ bool
TypedArrayObject::GetTemplateObjectForNative(JSContext* cx, Native native, uint32_t len,
                                             MutableHandleObject res)
{
#define CHECK_TYPED_ARRAY_CONSTRUCTOR(T, N) \
    if (native == &TypedArrayObjectTemplate<T>::class_constructor) { \
        size_t nbytes; \
        if (!js::CalculateAllocSize<T>(len, &nbytes)) \
            return true; \
        \
        if (nbytes < TypedArrayObject::SINGLETON_BYTE_LENGTH) { \
            res.set(TypedArrayObjectTemplate<T>::makeTemplateObject(cx, len)); \
            return !!res; \
        } \
    }
JS_FOR_EACH_TYPED_ARRAY(CHECK_TYPED_ARRAY_CONSTRUCTOR)
#undef CHECK_TYPED_ARRAY_CONSTRUCTOR
    return true;
}

/*
 * These next 3 functions are brought to you by the buggy GCC we use to build
 * B2G ICS. Older GCC versions have a bug in which they fail to compile
 * reinterpret_casts of templated functions with the message: "insufficient
 * contextual information to determine type". JS_PSG needs to
 * reinterpret_cast<JSGetterOp>, so this causes problems for us here.
 *
 * We could restructure all this code to make this nicer, but since ICS isn't
 * going to be around forever (and since this bug is fixed with the newer GCC
 * versions we use on JB and KK), the workaround here is designed for ease of
 * removal. When you stop seeing ICS Emulator builds on TBPL, remove these 3
 * JSNatives and insert the templated callee directly into the JS_PSG below.
 */
static bool
TypedArray_lengthGetter(JSContext* cx, unsigned argc, Value* vp)
{
    return TypedArrayObject::Getter<TypedArrayObject::lengthValue>(cx, argc, vp); \
}

static bool
TypedArray_byteLengthGetter(JSContext* cx, unsigned argc, Value* vp)
{
    return TypedArrayObject::Getter<TypedArrayObject::byteLengthValue>(cx, argc, vp);
}

static bool
TypedArray_byteOffsetGetter(JSContext* cx, unsigned argc, Value* vp)
{
    return TypedArrayObject::Getter<TypedArrayObject::byteOffsetValue>(cx, argc, vp);
}

bool
BufferGetterImpl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(TypedArrayObject::is(args.thisv()));
    Rooted<TypedArrayObject*> tarray(cx, &args.thisv().toObject().as<TypedArrayObject>());
    if (!TypedArrayObject::ensureHasBuffer(cx, tarray))
        return false;
    args.rval().set(TypedArrayObject::bufferValue(tarray));
    return true;
}

/*static*/ bool
js::TypedArray_bufferGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<TypedArrayObject::is, BufferGetterImpl>(cx, args);
}

/* static */ const JSPropertySpec
TypedArrayObject::protoAccessors[] = {
    JS_PSG("length", TypedArray_lengthGetter, 0),
    JS_PSG("buffer", TypedArray_bufferGetter, 0),
    JS_PSG("byteLength", TypedArray_byteLengthGetter, 0),
    JS_PSG("byteOffset", TypedArray_byteOffsetGetter, 0),
    JS_SELF_HOSTED_SYM_GET(toStringTag, "TypedArrayToStringTag", 0),
    JS_PS_END
};

/* static */ bool
TypedArrayObject::set(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<TypedArrayObject::is,
                                TypedArrayMethods<TypedArrayObject>::set>(cx, args);
}

/* static */ const JSFunctionSpec
TypedArrayObject::protoFunctions[] = {
    JS_SELF_HOSTED_FN("subarray", "TypedArraySubarray", 2, 0),
#if 0 /* disabled until perf-testing is completed */
    JS_SELF_HOSTED_FN("set", "TypedArraySet", 2, 0),
#else
    JS_FN("set", TypedArrayObject::set, 1, 0),
#endif
    JS_SELF_HOSTED_FN("copyWithin", "TypedArrayCopyWithin", 3, 0),
    JS_SELF_HOSTED_FN("every", "TypedArrayEvery", 1, 0),
    JS_SELF_HOSTED_FN("fill", "TypedArrayFill", 3, 0),
    JS_SELF_HOSTED_FN("filter", "TypedArrayFilter", 1, 0),
    JS_SELF_HOSTED_FN("find", "TypedArrayFind", 1, 0),
    JS_SELF_HOSTED_FN("findIndex", "TypedArrayFindIndex", 1, 0),
    JS_SELF_HOSTED_FN("forEach", "TypedArrayForEach", 1, 0),
    JS_SELF_HOSTED_FN("indexOf", "TypedArrayIndexOf", 2, 0),
    JS_SELF_HOSTED_FN("join", "TypedArrayJoin", 1, 0),
    JS_SELF_HOSTED_FN("lastIndexOf", "TypedArrayLastIndexOf", 2, 0),
    JS_SELF_HOSTED_FN("map", "TypedArrayMap", 1, 0),
    JS_SELF_HOSTED_FN("reduce", "TypedArrayReduce", 1, 0),
    JS_SELF_HOSTED_FN("reduceRight", "TypedArrayReduceRight", 1, 0),
    JS_SELF_HOSTED_FN("reverse", "TypedArrayReverse", 0, 0),
    JS_SELF_HOSTED_FN("slice", "TypedArraySlice", 2, 0),
    JS_SELF_HOSTED_FN("some", "TypedArraySome", 1, 0),
    JS_SELF_HOSTED_FN("sort", "TypedArraySort", 1, 0),
    JS_SELF_HOSTED_FN("entries", "TypedArrayEntries", 0, 0),
    JS_SELF_HOSTED_FN("keys", "TypedArrayKeys", 0, 0),
    JS_SELF_HOSTED_FN("values", "TypedArrayValues", 0, 0),
    JS_SELF_HOSTED_SYM_FN(iterator, "TypedArrayValues", 0, 0),
    JS_SELF_HOSTED_FN("includes", "TypedArrayIncludes", 2, 0),
    JS_SELF_HOSTED_FN("toString", "ArrayToString", 0, 0),
    JS_SELF_HOSTED_FN("toLocaleString", "TypedArrayToLocaleString", 2, 0),
    JS_FS_END
};

/* static */ const JSFunctionSpec
TypedArrayObject::staticFunctions[] = {
    JS_SELF_HOSTED_FN("from", "TypedArrayStaticFrom", 3, 0),
    JS_SELF_HOSTED_FN("of", "TypedArrayStaticOf", 0, 0),
    JS_FS_END
};

/* static */ const JSPropertySpec
TypedArrayObject::staticProperties[] = {
    JS_SELF_HOSTED_SYM_GET(species, "TypedArraySpecies", 0),
    JS_PS_END
};

static const ClassSpec
TypedArrayObjectSharedTypedArrayPrototypeClassSpec = {
    GenericCreateConstructor<TypedArrayConstructor, 3, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype,
    TypedArrayObject::staticFunctions,
    TypedArrayObject::staticProperties,
    TypedArrayObject::protoFunctions,
    TypedArrayObject::protoAccessors,
    nullptr,
    ClassSpec::DontDefineConstructor
};

/* static */ const Class
TypedArrayObject::sharedTypedArrayPrototypeClass = {
    // Actually ({}).toString.call(%TypedArray%.prototype) should throw,
    // because %TypedArray%.prototype lacks the the typed array internal
    // slots.  (It's not clear this is desirable -- particularly applied to
    // the actual typed array prototypes, see below -- but it's what ES6
    // draft 20140824 requires.)  But this is about as much as we can do
    // until we implement @@toStringTag.
    "???",
    JSCLASS_HAS_CACHED_PROTO(JSProto_TypedArray),
    JS_NULL_CLASS_OPS,
    &TypedArrayObjectSharedTypedArrayPrototypeClassSpec
};

template<typename T>
bool
ArrayBufferObject::createTypedArrayFromBufferImpl(JSContext* cx, const CallArgs& args)
{
    typedef TypedArrayObjectTemplate<T> ArrayType;
    MOZ_ASSERT(IsAnyArrayBuffer(args.thisv()));
    MOZ_ASSERT(args.length() == 3);

    Rooted<JSObject*> buffer(cx, &args.thisv().toObject());
    Rooted<JSObject*> proto(cx, &args[2].toObject());

    Rooted<JSObject*> obj(cx);
    double byteOffset = args[0].toNumber();
    MOZ_ASSERT(0 <= byteOffset);
    MOZ_ASSERT(byteOffset <= UINT32_MAX);
    MOZ_ASSERT(byteOffset == uint32_t(byteOffset));
    obj = ArrayType::fromBufferWithProto(cx, buffer, uint32_t(byteOffset), args[1].toInt32(),
                                         proto);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

template<typename T>
bool
ArrayBufferObject::createTypedArrayFromBuffer(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsAnyArrayBuffer, createTypedArrayFromBufferImpl<T> >(cx, args);
}

// this default implementation is only valid for integer types
// less than 32-bits in size.
template<typename NativeType>
Value
TypedArrayObjectTemplate<NativeType>::getIndexValue(JSObject* tarray, uint32_t index)
{
    static_assert(sizeof(NativeType) < 4,
                  "this method must only handle NativeType values that are "
                  "always exact int32_t values");

    return Int32Value(getIndex(tarray, index));
}

namespace {

// and we need to specialize for 32-bit integers and floats
template<>
Value
TypedArrayObjectTemplate<int32_t>::getIndexValue(JSObject* tarray, uint32_t index)
{
    return Int32Value(getIndex(tarray, index));
}

template<>
Value
TypedArrayObjectTemplate<uint32_t>::getIndexValue(JSObject* tarray, uint32_t index)
{
    uint32_t val = getIndex(tarray, index);
    return NumberValue(val);
}

template<>
Value
TypedArrayObjectTemplate<float>::getIndexValue(JSObject* tarray, uint32_t index)
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
    return DoubleValue(CanonicalizeNaN(dval));
}

template<>
Value
TypedArrayObjectTemplate<double>::getIndexValue(JSObject* tarray, uint32_t index)
{
    double val = getIndex(tarray, index);

    /*
     * Doubles in typed arrays could be typed-punned arrays of integers. This
     * could allow user code to break the engine-wide invariant that only
     * canonical nans are stored into jsvals, which means user code could
     * confuse the engine into interpreting a double-typed jsval as an
     * object-typed jsval.
     */
    return DoubleValue(CanonicalizeNaN(val));
}

} /* anonymous namespace */

static NewObjectKind
DataViewNewObjectKind(JSContext* cx, uint32_t byteLength, JSObject* proto)
{
    if (!proto && byteLength >= TypedArrayObject::SINGLETON_BYTE_LENGTH)
        return SingletonObject;
    jsbytecode* pc;
    JSScript* script = cx->currentScript(&pc);
    if (script && ObjectGroup::useSingletonForAllocationSite(script, pc, &DataViewObject::class_))
        return SingletonObject;
    return GenericObject;
}

DataViewObject*
DataViewObject::create(JSContext* cx, uint32_t byteOffset, uint32_t byteLength,
                       Handle<ArrayBufferObject*> arrayBuffer, JSObject* protoArg)
{
    MOZ_ASSERT(byteOffset <= INT32_MAX);
    MOZ_ASSERT(byteLength <= INT32_MAX);
    MOZ_ASSERT(byteOffset + byteLength < UINT32_MAX);
    MOZ_ASSERT(!arrayBuffer || !arrayBuffer->is<SharedArrayBufferObject>());

    RootedObject proto(cx, protoArg);
    RootedObject obj(cx);

    NewObjectKind newKind = DataViewNewObjectKind(cx, byteLength, proto);
    obj = NewObjectWithClassProto(cx, &class_, proto, newKind);
    if (!obj)
        return nullptr;

    if (!proto) {
        if (byteLength >= TypedArrayObject::SINGLETON_BYTE_LENGTH) {
            MOZ_ASSERT(obj->isSingleton());
        } else {
            jsbytecode* pc;
            RootedScript script(cx, cx->currentScript(&pc));
            if (script && !ObjectGroup::setAllocationSiteObjectGroup(cx, script, pc, obj,
                                                                     newKind == SingletonObject))
            {
                return nullptr;
            }
        }
    }

    // Caller should have established these preconditions, and no
    // (non-self-hosted) JS code has had an opportunity to run so nothing can
    // have invalidated them.
    MOZ_ASSERT(byteOffset <= arrayBuffer->byteLength());
    MOZ_ASSERT(byteOffset + byteLength <= arrayBuffer->byteLength());

    DataViewObject& dvobj = obj->as<DataViewObject>();
    dvobj.setFixedSlot(TypedArrayObject::BYTEOFFSET_SLOT, Int32Value(byteOffset));
    dvobj.setFixedSlot(TypedArrayObject::LENGTH_SLOT, Int32Value(byteLength));
    dvobj.setFixedSlot(TypedArrayObject::BUFFER_SLOT, ObjectValue(*arrayBuffer));
    dvobj.initPrivate(arrayBuffer->dataPointer() + byteOffset);

    // Include a barrier if the data view's data pointer is in the nursery, as
    // is done for typed arrays.
    if (!IsInsideNursery(obj) && cx->runtime()->gc.nursery.isInside(arrayBuffer->dataPointer()))
        cx->runtime()->gc.storeBuffer.putWholeCell(obj);

    // Verify that the private slot is at the expected place
    MOZ_ASSERT(dvobj.numFixedSlots() == TypedArrayObject::DATA_SLOT);

    if (!arrayBuffer->addView(cx, &dvobj))
        return nullptr;

    return &dvobj;
}

bool
DataViewObject::getAndCheckConstructorArgs(JSContext* cx, JSObject* bufobj, const CallArgs& args,
                                           uint32_t* byteOffsetPtr, uint32_t* byteLengthPtr)
{
    if (!IsArrayBuffer(bufobj)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_EXPECTED_TYPE,
                                  "DataView", "ArrayBuffer", bufobj->getClass()->name);
        return false;
    }

    Rooted<ArrayBufferObject*> buffer(cx, &AsArrayBuffer(bufobj));
    uint32_t byteOffset = 0;
    uint32_t byteLength = buffer->byteLength();

    if (args.length() > 1) {
        if (!ToUint32(cx, args[1], &byteOffset))
            return false;
        if (byteOffset > INT32_MAX) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_ARG_INDEX_OUT_OF_RANGE,
                                      "1");
            return false;
        }
    }

    if (buffer->isDetached()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
        return false;
    }

    if (args.length() > 1) {
        if (byteOffset > byteLength) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_ARG_INDEX_OUT_OF_RANGE,
                                      "1");
            return false;
        }

        if (args.get(2).isUndefined()) {
            byteLength -= byteOffset;
        } else {
            if (!ToUint32(cx, args[2], &byteLength))
                return false;
            if (byteLength > INT32_MAX) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                          JSMSG_ARG_INDEX_OUT_OF_RANGE, "2");
                return false;
            }

            MOZ_ASSERT(byteOffset + byteLength >= byteOffset,
                       "can't overflow: both numbers are less than INT32_MAX");
            if (byteOffset + byteLength > buffer->byteLength()) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                          JSMSG_ARG_INDEX_OUT_OF_RANGE, "1");
                return false;
            }
        }
    }

    /* The sum of these cannot overflow a uint32_t */
    MOZ_ASSERT(byteOffset <= INT32_MAX);
    MOZ_ASSERT(byteLength <= INT32_MAX);


    *byteOffsetPtr = byteOffset;
    *byteLengthPtr = byteLength;

    return true;
}

bool
DataViewObject::constructSameCompartment(JSContext* cx, HandleObject bufobj, const CallArgs& args)
{
    MOZ_ASSERT(args.isConstructing());
    assertSameCompartment(cx, bufobj);

    uint32_t byteOffset, byteLength;
    if (!getAndCheckConstructorArgs(cx, bufobj, args, &byteOffset, &byteLength))
        return false;

    RootedObject proto(cx);
    RootedObject newTarget(cx, &args.newTarget().toObject());
    if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
        return false;

    Rooted<ArrayBufferObject*> buffer(cx, &AsArrayBuffer(bufobj));
    JSObject* obj = DataViewObject::create(cx, byteOffset, byteLength, buffer, proto);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

// Create a DataView object in another compartment.
//
// ES6 supports creating a DataView in global A (using global A's DataView
// constructor) backed by an ArrayBuffer created in global B.
//
// Our DataViewObject implementation doesn't support a DataView in
// compartment A backed by an ArrayBuffer in compartment B. So in this case,
// we create the DataView in B (!) and return a cross-compartment wrapper.
//
// Extra twist: the spec says the new DataView's [[Prototype]] must be
// A's DataView.prototype. So even though we're creating the DataView in B,
// its [[Prototype]] must be (a cross-compartment wrapper for) the
// DataView.prototype in A.
//
// As if this were not confusing enough, the way we actually do this is also
// tricky. We call compartment A's createDataViewForThis method, passing it
// bufobj as `this`. That calls ArrayBufferObject::createDataViewForThis(),
// which uses CallNonGenericMethod to switch to compartment B so that
// the new DataView is created there.
bool
DataViewObject::constructWrapped(JSContext* cx, HandleObject bufobj, const CallArgs& args)
{
    MOZ_ASSERT(args.isConstructing());
    MOZ_ASSERT(bufobj->is<WrapperObject>());

    JSObject* unwrapped = CheckedUnwrap(bufobj);
    if (!unwrapped) {
        JS_ReportErrorASCII(cx, "Permission denied to access object");
        return false;
    }

    // NB: This entails the IsArrayBuffer check
    uint32_t byteOffset, byteLength;
    if (!getAndCheckConstructorArgs(cx, unwrapped, args, &byteOffset, &byteLength))
        return false;

    // Make sure to get the [[Prototype]] for the created view from this
    // compartment.
    RootedObject proto(cx);
    RootedObject newTarget(cx, &args.newTarget().toObject());
    if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
        return false;

    Rooted<GlobalObject*> global(cx, cx->compartment()->maybeGlobal());
    if (!proto) {
        proto = global->getOrCreateDataViewPrototype(cx);
        if (!proto)
            return false;
    }

    FixedInvokeArgs<3> args2(cx);

    args2[0].set(PrivateUint32Value(byteOffset));
    args2[1].set(PrivateUint32Value(byteLength));
    args2[2].setObject(*proto);

    RootedValue fval(cx, global->createDataViewForThis());
    RootedValue thisv(cx, ObjectValue(*bufobj));
    return js::Call(cx, fval, thisv, args2, args.rval());
}

bool
DataViewObject::class_constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "DataView"))
        return false;

    RootedObject bufobj(cx);
    if (!GetFirstArgumentAsObject(cx, args, "DataView constructor", &bufobj))
        return false;

    if (bufobj->is<WrapperObject>())
        return constructWrapped(cx, bufobj, args);
    return constructSameCompartment(cx, bufobj, args);
}

template <typename NativeType>
/* static */ uint8_t*
DataViewObject::getDataPointer(JSContext* cx, Handle<DataViewObject*> obj, double offset)
{
    MOZ_ASSERT(offset >= 0);

    const size_t TypeSize = sizeof(NativeType);
    if (offset > UINT32_MAX - TypeSize || offset + TypeSize > obj->byteLength()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_ARG_INDEX_OUT_OF_RANGE,
                                  "1");
        return nullptr;
    }

    MOZ_ASSERT(offset < UINT32_MAX);
    return static_cast<uint8_t*>(obj->dataPointer()) + uint32_t(offset);
}

static inline bool
needToSwapBytes(bool littleEndian)
{
#if MOZ_LITTLE_ENDIAN
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

    static void fromBuffer(DataType* dest, const uint8_t* unalignedBuffer, bool wantSwap)
    {
        MOZ_ASSERT((reinterpret_cast<uintptr_t>(dest) & (Min<size_t>(MOZ_ALIGNOF(void*), sizeof(DataType)) - 1)) == 0);
        memcpy((void*) dest, unalignedBuffer, sizeof(ReadWriteType));
        if (wantSwap) {
            ReadWriteType* rwDest = reinterpret_cast<ReadWriteType*>(dest);
            *rwDest = swapBytes(*rwDest);
        }
    }

    static void toBuffer(uint8_t* unalignedBuffer, const DataType* src, bool wantSwap)
    {
        MOZ_ASSERT((reinterpret_cast<uintptr_t>(src) & (Min<size_t>(MOZ_ALIGNOF(void*), sizeof(DataType)) - 1)) == 0);
        ReadWriteType temp = *reinterpret_cast<const ReadWriteType*>(src);
        if (wantSwap)
            temp = swapBytes(temp);
        memcpy(unalignedBuffer, (void*) &temp, sizeof(ReadWriteType));
    }
};

static bool
ToIndex(JSContext* cx, HandleValue v, double* index)
{
    if (v.isUndefined()) {
        *index = 0.0;
        return true;
    }

    double integerIndex;
    if (!ToInteger(cx, v, &integerIndex))
        return false;

    // Inlined version of ToLength.
    // 1. Already an integer
    // 2. Step eliminates < 0, +0 == -0 with SameValueZero
    // 3/4. Limit to <= 2^53-1, so everything above should fail.
    if (integerIndex < 0 || integerIndex > 9007199254740991) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_INDEX);
        return false;
    }

    *index = integerIndex;
    return true;
}

template<typename NativeType>
/* static */ bool
DataViewObject::read(JSContext* cx, Handle<DataViewObject*> obj,
                     const CallArgs& args, NativeType* val, const char* method)
{
    // Steps 1-2. done by the caller
    // Step 3. unnecessary assert

    // Step 4.
    double getIndex;
    if (!ToIndex(cx, args.get(0), &getIndex))
        return false;

    // Step 5.
    bool isLittleEndian = args.length() >= 2 && ToBoolean(args[1]);

    // Steps 6-7.
    if (obj->arrayBuffer().isDetached()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
        return false;
    }

    // Steps 8-12.
    uint8_t* data = DataViewObject::getDataPointer<NativeType>(cx, obj, getIndex);
    if (!data)
        return false;

    // Step 13.
    DataViewIO<NativeType>::fromBuffer(val, data, needToSwapBytes(isLittleEndian));
    return true;
}

template <typename NativeType>
static inline bool
WebIDLCast(JSContext* cx, HandleValue value, NativeType* out)
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
WebIDLCast<float>(JSContext* cx, HandleValue value, float* out)
{
    double temp;
    if (!ToNumber(cx, value, &temp))
        return false;
    *out = static_cast<float>(temp);
    return true;
}

template <>
inline bool
WebIDLCast<double>(JSContext* cx, HandleValue value, double* out)
{
    return ToNumber(cx, value, out);
}

template<typename NativeType>
/* static */ bool
DataViewObject::write(JSContext* cx, Handle<DataViewObject*> obj,
                      const CallArgs& args, const char* method)
{
    // Steps 1-2. done by the caller
    // Step 3. unnecessary assert

    // Step 4.
    double getIndex;
    if (!ToIndex(cx, args.get(0), &getIndex))
        return false;

    // Step 5. Should just call ToNumber (unobservable)
    NativeType value;
    if (!WebIDLCast(cx, args.get(1), &value))
        return false;

#ifdef JS_MORE_DETERMINISTIC
    // See the comment in ElementSpecific::doubleToNative.
    if (TypeIsFloatingPoint<NativeType>())
        value = JS::CanonicalizeNaN(value);
#endif

    // Step 6.
    bool isLittleEndian = args.length() >= 3 && ToBoolean(args[2]);

    // Steps 7-8.
    if (obj->arrayBuffer().isDetached()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
        return false;
    }

    // Steps 9-13.
    uint8_t* data = DataViewObject::getDataPointer<NativeType>(cx, obj, getIndex);
    if (!data)
        return false;

    // Step 14.
    DataViewIO<NativeType>::toBuffer(data, &value, needToSwapBytes(isLittleEndian));
    return true;
}

bool
DataViewObject::getInt8Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    int8_t val;
    if (!read(cx, thisView, args, &val, "getInt8"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getInt8(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getInt8Impl>(cx, args);
}

bool
DataViewObject::getUint8Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    uint8_t val;
    if (!read(cx, thisView, args, &val, "getUint8"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getUint8(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getUint8Impl>(cx, args);
}

bool
DataViewObject::getInt16Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    int16_t val;
    if (!read(cx, thisView, args, &val, "getInt16"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getInt16(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getInt16Impl>(cx, args);
}

bool
DataViewObject::getUint16Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    uint16_t val;
    if (!read(cx, thisView, args, &val, "getUint16"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getUint16(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getUint16Impl>(cx, args);
}

bool
DataViewObject::getInt32Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    int32_t val;
    if (!read(cx, thisView, args, &val, "getInt32"))
        return false;
    args.rval().setInt32(val);
    return true;
}

bool
DataViewObject::fun_getInt32(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getInt32Impl>(cx, args);
}

bool
DataViewObject::getUint32Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    uint32_t val;
    if (!read(cx, thisView, args, &val, "getUint32"))
        return false;
    args.rval().setNumber(val);
    return true;
}

bool
DataViewObject::fun_getUint32(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getUint32Impl>(cx, args);
}

bool
DataViewObject::getFloat32Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    float val;
    if (!read(cx, thisView, args, &val, "getFloat32"))
        return false;

    args.rval().setDouble(CanonicalizeNaN(val));
    return true;
}

bool
DataViewObject::fun_getFloat32(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getFloat32Impl>(cx, args);
}

bool
DataViewObject::getFloat64Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    double val;
    if (!read(cx, thisView, args, &val, "getFloat64"))
        return false;

    args.rval().setDouble(CanonicalizeNaN(val));
    return true;
}

bool
DataViewObject::fun_getFloat64(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getFloat64Impl>(cx, args);
}

bool
DataViewObject::setInt8Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<int8_t>(cx, thisView, args, "setInt8"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setInt8(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setInt8Impl>(cx, args);
}

bool
DataViewObject::setUint8Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<uint8_t>(cx, thisView, args, "setUint8"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setUint8(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setUint8Impl>(cx, args);
}

bool
DataViewObject::setInt16Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<int16_t>(cx, thisView, args, "setInt16"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setInt16(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setInt16Impl>(cx, args);
}

bool
DataViewObject::setUint16Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<uint16_t>(cx, thisView, args, "setUint16"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setUint16(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setUint16Impl>(cx, args);
}

bool
DataViewObject::setInt32Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<int32_t>(cx, thisView, args, "setInt32"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setInt32(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setInt32Impl>(cx, args);
}

bool
DataViewObject::setUint32Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<uint32_t>(cx, thisView, args, "setUint32"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setUint32(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setUint32Impl>(cx, args);
}

bool
DataViewObject::setFloat32Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<float>(cx, thisView, args, "setFloat32"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setFloat32(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setFloat32Impl>(cx, args);
}

bool
DataViewObject::setFloat64Impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(is(args.thisv()));

    Rooted<DataViewObject*> thisView(cx, &args.thisv().toObject().as<DataViewObject>());

    if (!write<double>(cx, thisView, args, "setFloat64"))
        return false;
    args.rval().setUndefined();
    return true;
}

bool
DataViewObject::fun_setFloat64(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setFloat64Impl>(cx, args);
}

Value
TypedArrayObject::getElement(uint32_t index)
{
    switch (type()) {
      case Scalar::Int8:
        return Int8Array::getIndexValue(this, index);
      case Scalar::Uint8:
        return Uint8Array::getIndexValue(this, index);
      case Scalar::Int16:
        return Int16Array::getIndexValue(this, index);
      case Scalar::Uint16:
        return Uint16Array::getIndexValue(this, index);
      case Scalar::Int32:
        return Int32Array::getIndexValue(this, index);
      case Scalar::Uint32:
        return Uint32Array::getIndexValue(this, index);
      case Scalar::Float32:
        return Float32Array::getIndexValue(this, index);
      case Scalar::Float64:
        return Float64Array::getIndexValue(this, index);
      case Scalar::Uint8Clamped:
        return Uint8ClampedArray::getIndexValue(this, index);
      case Scalar::Int64:
      case Scalar::Float32x4:
      case Scalar::Int8x16:
      case Scalar::Int16x8:
      case Scalar::Int32x4:
      case Scalar::MaxTypedArrayViewType:
        break;
    }

    MOZ_CRASH("Unknown TypedArray type");
}

void
TypedArrayObject::setElement(TypedArrayObject& obj, uint32_t index, double d)
{
    MOZ_ASSERT(index < obj.length());

#ifdef JS_MORE_DETERMINISTIC
    // See the comment in ElementSpecific::doubleToNative.
    d = JS::CanonicalizeNaN(d);
#endif

    switch (obj.type()) {
      case Scalar::Int8:
        Int8Array::setIndexValue(obj, index, d);
        return;
      case Scalar::Uint8:
        Uint8Array::setIndexValue(obj, index, d);
        return;
      case Scalar::Uint8Clamped:
        Uint8ClampedArray::setIndexValue(obj, index, d);
        return;
      case Scalar::Int16:
        Int16Array::setIndexValue(obj, index, d);
        return;
      case Scalar::Uint16:
        Uint16Array::setIndexValue(obj, index, d);
        return;
      case Scalar::Int32:
        Int32Array::setIndexValue(obj, index, d);
        return;
      case Scalar::Uint32:
        Uint32Array::setIndexValue(obj, index, d);
        return;
      case Scalar::Float32:
        Float32Array::setIndexValue(obj, index, d);
        return;
      case Scalar::Float64:
        Float64Array::setIndexValue(obj, index, d);
        return;
      case Scalar::Int64:
      case Scalar::Float32x4:
      case Scalar::Int8x16:
      case Scalar::Int16x8:
      case Scalar::Int32x4:
      case Scalar::MaxTypedArrayViewType:
        break;
    }

    MOZ_CRASH("Unknown TypedArray type");
}

/***
 *** JS impl
 ***/

/*
 * TypedArrayObject boilerplate
 */

#define IMPL_TYPED_ARRAY_JSAPI_CONSTRUCTORS(Name,NativeType)                                    \
  JS_FRIEND_API(JSObject*) JS_New ## Name ## Array(JSContext* cx, uint32_t nelements)           \
  {                                                                                             \
      return TypedArrayObjectTemplate<NativeType>::fromLength(cx, nelements);                   \
  }                                                                                             \
  JS_FRIEND_API(JSObject*) JS_New ## Name ## ArrayFromArray(JSContext* cx, HandleObject other)  \
  {                                                                                             \
      return TypedArrayObjectTemplate<NativeType>::fromArray(cx, other);                        \
  }                                                                                             \
  JS_FRIEND_API(JSObject*) JS_New ## Name ## ArrayWithBuffer(JSContext* cx,                     \
                               HandleObject arrayBuffer, uint32_t byteOffset, int32_t length)   \
  {                                                                                             \
      return TypedArrayObjectTemplate<NativeType>::fromBuffer(cx, arrayBuffer, byteOffset,      \
                                                              length);                          \
  }                                                                                             \
  JS_FRIEND_API(bool) JS_Is ## Name ## Array(JSObject* obj)                                     \
  {                                                                                             \
      if (!(obj = CheckedUnwrap(obj)))                                                          \
          return false;                                                                         \
      const Class* clasp = obj->getClass();                                                     \
      return clasp == TypedArrayObjectTemplate<NativeType>::instanceClass();                    \
  }                                                                                             \
  JS_FRIEND_API(JSObject*) js::Unwrap ## Name ## Array(JSObject* obj)                           \
  {                                                                                             \
      obj = CheckedUnwrap(obj);                                                                 \
      if (!obj)                                                                                 \
          return nullptr;                                                                       \
      const Class* clasp = obj->getClass();                                                     \
      if (clasp == TypedArrayObjectTemplate<NativeType>::instanceClass())                       \
          return obj;                                                                           \
      return nullptr;                                                                           \
  }                                                                                             \
  const js::Class* const js::detail::Name ## ArrayClassPtr =                                    \
      &js::TypedArrayObject::classes[TypedArrayObjectTemplate<NativeType>::ArrayTypeID()];

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
  JS_FRIEND_API(JSObject*) JS_GetObjectAs ## Name ## Array(JSObject* obj,                  \
                                                            uint32_t* length,               \
                                                            bool* isShared,                 \
                                                            ExternalType** data)            \
  {                                                                                         \
      if (!(obj = CheckedUnwrap(obj)))                                                      \
          return nullptr;                                                                   \
                                                                                            \
      const Class* clasp = obj->getClass();                                                 \
      if (clasp != TypedArrayObjectTemplate<InternalType>::instanceClass())                 \
          return nullptr;                                                                   \
                                                                                            \
      TypedArrayObject* tarr = &obj->as<TypedArrayObject>();                                \
      *length = tarr->length();                                                             \
      *isShared = tarr->isSharedMemory();                                                         \
      *data = static_cast<ExternalType*>(tarr->viewDataEither().unwrap(/*safe - caller sees isShared flag*/)); \
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

static const ClassOps TypedArrayClassOps = {
    nullptr,                 /* addProperty */
    nullptr,                 /* delProperty */
    nullptr,                 /* getProperty */
    nullptr,                 /* setProperty */
    nullptr,                 /* enumerate   */
    nullptr,                 /* resolve     */
    nullptr,                 /* mayResolve  */
    TypedArrayObject::finalize, /* finalize    */
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    TypedArrayObject::trace, /* trace  */
};

static const ClassExtension TypedArrayClassExtension = {
    nullptr,
    TypedArrayObject::objectMoved,
};

#define IMPL_TYPED_ARRAY_PROPERTIES(_type)                                     \
{                                                                              \
JS_INT32_PS("BYTES_PER_ELEMENT", _type##Array::BYTES_PER_ELEMENT,              \
            JSPROP_READONLY | JSPROP_PERMANENT),                               \
JS_PS_END                                                                      \
}

static const JSPropertySpec static_prototype_properties[Scalar::MaxTypedArrayViewType][2] = {
    IMPL_TYPED_ARRAY_PROPERTIES(Int8),
    IMPL_TYPED_ARRAY_PROPERTIES(Uint8),
    IMPL_TYPED_ARRAY_PROPERTIES(Int16),
    IMPL_TYPED_ARRAY_PROPERTIES(Uint16),
    IMPL_TYPED_ARRAY_PROPERTIES(Int32),
    IMPL_TYPED_ARRAY_PROPERTIES(Uint32),
    IMPL_TYPED_ARRAY_PROPERTIES(Float32),
    IMPL_TYPED_ARRAY_PROPERTIES(Float64),
    IMPL_TYPED_ARRAY_PROPERTIES(Uint8Clamped)
};

#define IMPL_TYPED_ARRAY_CLASS_SPEC(_type)                                     \
{                                                                              \
    _type##Array::createConstructor,                                           \
    _type##Array::createPrototype,                                             \
    nullptr,                                                                   \
    static_prototype_properties[Scalar::Type::_type],                          \
    nullptr,                                                                   \
    static_prototype_properties[Scalar::Type::_type],                          \
    nullptr,                                                                   \
    JSProto_TypedArray                                                         \
}

static const ClassSpec TypedArrayObjectClassSpecs[Scalar::MaxTypedArrayViewType] = {
    IMPL_TYPED_ARRAY_CLASS_SPEC(Int8),
    IMPL_TYPED_ARRAY_CLASS_SPEC(Uint8),
    IMPL_TYPED_ARRAY_CLASS_SPEC(Int16),
    IMPL_TYPED_ARRAY_CLASS_SPEC(Uint16),
    IMPL_TYPED_ARRAY_CLASS_SPEC(Int32),
    IMPL_TYPED_ARRAY_CLASS_SPEC(Uint32),
    IMPL_TYPED_ARRAY_CLASS_SPEC(Float32),
    IMPL_TYPED_ARRAY_CLASS_SPEC(Float64),
    IMPL_TYPED_ARRAY_CLASS_SPEC(Uint8Clamped)
};

#define IMPL_TYPED_ARRAY_CLASS(_type)                                          \
{                                                                              \
    #_type "Array",                                                            \
    JSCLASS_HAS_RESERVED_SLOTS(TypedArrayObject::RESERVED_SLOTS) |             \
    JSCLASS_HAS_PRIVATE |                                                      \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##_type##Array) |                         \
    JSCLASS_DELAY_METADATA_BUILDER |                                           \
    JSCLASS_SKIP_NURSERY_FINALIZE |                                            \
    JSCLASS_BACKGROUND_FINALIZE,                                               \
    &TypedArrayClassOps,                                                       \
    &TypedArrayObjectClassSpecs[Scalar::Type::_type],                          \
    &TypedArrayClassExtension                                                  \
}

const Class TypedArrayObject::classes[Scalar::MaxTypedArrayViewType] = {
    IMPL_TYPED_ARRAY_CLASS(Int8),
    IMPL_TYPED_ARRAY_CLASS(Uint8),
    IMPL_TYPED_ARRAY_CLASS(Int16),
    IMPL_TYPED_ARRAY_CLASS(Uint16),
    IMPL_TYPED_ARRAY_CLASS(Int32),
    IMPL_TYPED_ARRAY_CLASS(Uint32),
    IMPL_TYPED_ARRAY_CLASS(Float32),
    IMPL_TYPED_ARRAY_CLASS(Float64),
    IMPL_TYPED_ARRAY_CLASS(Uint8Clamped)
};

#define IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(_type) \
{ \
    DELEGATED_CLASSSPEC(TypedArrayObject::classes[Scalar::Type::_type].spec), \
    nullptr, \
    nullptr, \
    nullptr, \
    nullptr, \
    nullptr, \
    nullptr, \
    JSProto_TypedArray | ClassSpec::IsDelegated \
}

static const ClassSpec TypedArrayObjectProtoClassSpecs[Scalar::MaxTypedArrayViewType] = {
    IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(Int8),
    IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(Uint8),
    IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(Int16),
    IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(Uint16),
    IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(Int32),
    IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(Uint32),
    IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(Float32),
    IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(Float64),
    IMPL_TYPED_ARRAY_PROTO_CLASS_SPEC(Uint8Clamped)
};

// The various typed array prototypes are supposed to 1) be normal objects,
// 2) stringify to "[object <name of constructor>]", and 3) (Gecko-specific)
// be xrayable.  The first and second requirements mandate (in the absence of
// @@toStringTag) a custom class.  The third requirement mandates that each
// prototype's class have the relevant typed array's cached JSProtoKey in them.
// Thus we need one class with cached prototype per kind of typed array, with a
// delegated ClassSpec.
#define IMPL_TYPED_ARRAY_PROTO_CLASS(_type) \
{ \
    /*
     * Actually ({}).toString.call(Uint8Array.prototype) should throw, because
     * Uint8Array.prototype lacks the the typed array internal slots.  (Same as
     * with %TypedArray%.prototype.)  It's not clear this is desirable (see
     * above), but it's what we've always done, so keep doing it till we
     * implement @@toStringTag or ES6 changes.
     */ \
    #_type "ArrayPrototype", \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##_type##Array), \
    JS_NULL_CLASS_OPS, \
    &TypedArrayObjectProtoClassSpecs[Scalar::Type::_type] \
}

const Class TypedArrayObject::protoClasses[Scalar::MaxTypedArrayViewType] = {
    IMPL_TYPED_ARRAY_PROTO_CLASS(Int8),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint8),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Int16),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint16),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Int32),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint32),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Float32),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Float64),
    IMPL_TYPED_ARRAY_PROTO_CLASS(Uint8Clamped)
};

/* static */ bool
TypedArrayObject::isOriginalLengthGetter(Native native)
{
    return native == TypedArray_lengthGetter;
}

const Class DataViewObject::protoClass = {
    "DataViewPrototype",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(TypedArrayObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_DataView)
};

static const ClassOps DataViewObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    nullptr, /* finalize */
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    ArrayBufferViewObject::trace
};

const Class DataViewObject::class_ = {
    "DataView",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(TypedArrayObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_DataView),
    &DataViewObjectClassOps
};

const JSFunctionSpec DataViewObject::jsfuncs[] = {
    JS_FN("getInt8",    DataViewObject::fun_getInt8,      1,0),
    JS_FN("getUint8",   DataViewObject::fun_getUint8,     1,0),
    JS_FN("getInt16",   DataViewObject::fun_getInt16,     1,0),
    JS_FN("getUint16",  DataViewObject::fun_getUint16,    1,0),
    JS_FN("getInt32",   DataViewObject::fun_getInt32,     1,0),
    JS_FN("getUint32",  DataViewObject::fun_getUint32,    1,0),
    JS_FN("getFloat32", DataViewObject::fun_getFloat32,   1,0),
    JS_FN("getFloat64", DataViewObject::fun_getFloat64,   1,0),
    JS_FN("setInt8",    DataViewObject::fun_setInt8,      2,0),
    JS_FN("setUint8",   DataViewObject::fun_setUint8,     2,0),
    JS_FN("setInt16",   DataViewObject::fun_setInt16,     2,0),
    JS_FN("setUint16",  DataViewObject::fun_setUint16,    2,0),
    JS_FN("setInt32",   DataViewObject::fun_setInt32,     2,0),
    JS_FN("setUint32",  DataViewObject::fun_setUint32,    2,0),
    JS_FN("setFloat32", DataViewObject::fun_setFloat32,   2,0),
    JS_FN("setFloat64", DataViewObject::fun_setFloat64,   2,0),
    JS_FS_END
};

template<Value ValueGetter(DataViewObject* view)>
bool
DataViewObject::getterImpl(JSContext* cx, const CallArgs& args)
{
    args.rval().set(ValueGetter(&args.thisv().toObject().as<DataViewObject>()));
    return true;
}

template<Value ValueGetter(DataViewObject* view)>
bool
DataViewObject::getter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getterImpl<ValueGetter> >(cx, args);
}

template<Value ValueGetter(DataViewObject* view)>
bool
DataViewObject::defineGetter(JSContext* cx, PropertyName* name, HandleNativeObject proto)
{
    RootedId id(cx, NameToId(name));
    RootedAtom atom(cx, IdToFunctionName(cx, id, "get"));
    if (!atom)
        return false;
    unsigned attrs = JSPROP_SHARED | JSPROP_GETTER;

    Rooted<GlobalObject*> global(cx, cx->compartment()->maybeGlobal());
    JSObject* getter =
        NewNativeFunction(cx, DataViewObject::getter<ValueGetter>, 0, atom);
    if (!getter)
        return false;

    return NativeDefineProperty(cx, proto, id, UndefinedHandleValue,
                                JS_DATA_TO_FUNC_PTR(GetterOp, getter), nullptr, attrs);
}

/* static */ bool
DataViewObject::initClass(JSContext* cx)
{
    Rooted<GlobalObject*> global(cx, cx->compartment()->maybeGlobal());
    if (global->isStandardClassResolved(JSProto_DataView))
        return true;

    RootedNativeObject proto(cx, global->createBlankPrototype(cx, &DataViewObject::protoClass));
    if (!proto)
        return false;

    RootedFunction ctor(cx, global->createConstructor(cx, DataViewObject::class_constructor,
                                                      cx->names().DataView, 3));
    if (!ctor)
        return false;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return false;

    if (!defineGetter<bufferValue>(cx, cx->names().buffer, proto))
        return false;

    if (!defineGetter<byteLengthValue>(cx, cx->names().byteLength, proto))
        return false;

    if (!defineGetter<byteOffsetValue>(cx, cx->names().byteOffset, proto))
        return false;

    if (!JS_DefineFunctions(cx, proto, DataViewObject::jsfuncs))
        return false;

    if (!DefineToStringTag(cx, proto, cx->names().DataView))
        return false;

    /*
     * Create a helper function to implement the craziness of
     * |new DataView(new otherWindow.ArrayBuffer())|, and install it in the
     * global for use by the DataViewObject constructor.
     */
    RootedFunction fun(cx, NewNativeFunction(cx, ArrayBufferObject::createDataViewForThis,
                                             0, nullptr));
    if (!fun)
        return false;

    if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_DataView, ctor, proto))
        return false;

    global->setCreateDataViewForThis(fun);

    return true;
}

void
DataViewObject::notifyBufferDetached(void* newData)
{
    setFixedSlot(TypedArrayObject::LENGTH_SLOT, Int32Value(0));
    setFixedSlot(TypedArrayObject::BYTEOFFSET_SLOT, Int32Value(0));
    setPrivate(newData);
}

JSObject*
js::InitDataViewClass(JSContext* cx, HandleObject obj)
{
    if (!DataViewObject::initClass(cx))
        return nullptr;
    return &cx->global()->getPrototype(JSProto_DataView).toObject();
}

bool
js::IsTypedArrayConstructor(HandleValue v, uint32_t type)
{
    switch (type) {
      case Scalar::Int8:
        return IsNativeFunction(v, Int8Array::class_constructor);
      case Scalar::Uint8:
        return IsNativeFunction(v, Uint8Array::class_constructor);
      case Scalar::Int16:
        return IsNativeFunction(v, Int16Array::class_constructor);
      case Scalar::Uint16:
        return IsNativeFunction(v, Uint16Array::class_constructor);
      case Scalar::Int32:
        return IsNativeFunction(v, Int32Array::class_constructor);
      case Scalar::Uint32:
        return IsNativeFunction(v, Uint32Array::class_constructor);
      case Scalar::Float32:
        return IsNativeFunction(v, Float32Array::class_constructor);
      case Scalar::Float64:
        return IsNativeFunction(v, Float64Array::class_constructor);
      case Scalar::Uint8Clamped:
        return IsNativeFunction(v, Uint8ClampedArray::class_constructor);
      case Scalar::MaxTypedArrayViewType:
        break;
    }
    MOZ_CRASH("unexpected typed array type");
}

template <typename CharT>
bool
js::StringIsTypedArrayIndex(const CharT* s, size_t length, uint64_t* indexp)
{
    const CharT* end = s + length;

    if (s == end)
        return false;

    bool negative = false;
    if (*s == '-') {
        negative = true;
        if (++s == end)
            return false;
    }

    if (!JS7_ISDEC(*s))
        return false;

    uint64_t index = 0;
    uint32_t digit = JS7_UNDEC(*s++);

    /* Don't allow leading zeros. */
    if (digit == 0 && s != end)
        return false;

    index = digit;

    for (; s < end; s++) {
        if (!JS7_ISDEC(*s))
            return false;

        digit = JS7_UNDEC(*s);

        /* Watch for overflows. */
        if ((UINT64_MAX - digit) / 10 < index)
            index = UINT64_MAX;
        else
            index = 10 * index + digit;
    }

    if (negative)
        *indexp = UINT64_MAX;
    else
        *indexp = index;
    return true;
}

template bool
js::StringIsTypedArrayIndex(const char16_t* s, size_t length, uint64_t* indexp);

template bool
js::StringIsTypedArrayIndex(const Latin1Char* s, size_t length, uint64_t* indexp);

/* ES6 draft rev 34 (2015 Feb 20) 9.4.5.3 [[DefineOwnProperty]] step 3.c. */
bool
js::DefineTypedArrayElement(JSContext* cx, HandleObject obj, uint64_t index,
                            Handle<PropertyDescriptor> desc, ObjectOpResult& result)
{
    MOZ_ASSERT(obj->is<TypedArrayObject>());

    // These are all substeps of 3.c.
    // Steps i-vi.
    // We (wrongly) ignore out of range defines with a value.
    if (index >= obj->as<TypedArrayObject>().length())
        return result.succeed();

    // Step vii.
    if (desc.isAccessorDescriptor())
        return result.fail(JSMSG_CANT_REDEFINE_PROP);

    // Step viii.
    if (desc.hasConfigurable() && desc.configurable())
        return result.fail(JSMSG_CANT_REDEFINE_PROP);

    // Step ix.
    if (desc.hasEnumerable() && !desc.enumerable())
        return result.fail(JSMSG_CANT_REDEFINE_PROP);

    // Step x.
    if (desc.hasWritable() && !desc.writable())
        return result.fail(JSMSG_CANT_REDEFINE_PROP);

    // Step xi.
    if (desc.hasValue()) {
        double d;
        if (!ToNumber(cx, desc.value(), &d))
            return false;

        if (obj->is<TypedArrayObject>())
            TypedArrayObject::setElement(obj->as<TypedArrayObject>(), index, d);
    }

    // Step xii.
    return result.succeed();
}

/* JS Friend API */

JS_FRIEND_API(bool)
JS_IsTypedArrayObject(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<TypedArrayObject>() : false;
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayLength(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<TypedArrayObject>().length();
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteOffset(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<TypedArrayObject>().byteOffset();
}

JS_FRIEND_API(uint32_t)
JS_GetTypedArrayByteLength(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<TypedArrayObject>().byteLength();
}

JS_FRIEND_API(bool)
JS_GetTypedArraySharedness(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return false;
    return obj->as<TypedArrayObject>().isSharedMemory();
}

JS_FRIEND_API(js::Scalar::Type)
JS_GetArrayBufferViewType(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return Scalar::MaxTypedArrayViewType;

    if (obj->is<TypedArrayObject>())
        return obj->as<TypedArrayObject>().type();
    if (obj->is<DataViewObject>())
        return Scalar::MaxTypedArrayViewType;
    MOZ_CRASH("invalid ArrayBufferView type");
}

JS_FRIEND_API(int8_t*)
JS_GetInt8ArrayData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    MOZ_ASSERT((int32_t) tarr->type() == Scalar::Int8);
    *isSharedMemory = tarr->isSharedMemory();
    return static_cast<int8_t*>(tarr->viewDataEither().unwrap(/*safe - caller sees isShared*/));
}

JS_FRIEND_API(uint8_t*)
JS_GetUint8ArrayData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    MOZ_ASSERT((int32_t) tarr->type() == Scalar::Uint8);
    *isSharedMemory = tarr->isSharedMemory();
    return static_cast<uint8_t*>(tarr->viewDataEither().unwrap(/*safe - caller sees isSharedMemory*/));
}

JS_FRIEND_API(uint8_t*)
JS_GetUint8ClampedArrayData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    MOZ_ASSERT((int32_t) tarr->type() == Scalar::Uint8Clamped);
    *isSharedMemory = tarr->isSharedMemory();
    return static_cast<uint8_t*>(tarr->viewDataEither().unwrap(/*safe - caller sees isSharedMemory*/));
}

JS_FRIEND_API(int16_t*)
JS_GetInt16ArrayData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    MOZ_ASSERT((int32_t) tarr->type() == Scalar::Int16);
    *isSharedMemory = tarr->isSharedMemory();
    return static_cast<int16_t*>(tarr->viewDataEither().unwrap(/*safe - caller sees isSharedMemory*/));
}

JS_FRIEND_API(uint16_t*)
JS_GetUint16ArrayData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    MOZ_ASSERT((int32_t) tarr->type() == Scalar::Uint16);
    *isSharedMemory = tarr->isSharedMemory();
    return static_cast<uint16_t*>(tarr->viewDataEither().unwrap(/*safe - caller sees isSharedMemory*/));
}

JS_FRIEND_API(int32_t*)
JS_GetInt32ArrayData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    MOZ_ASSERT((int32_t) tarr->type() == Scalar::Int32);
    *isSharedMemory = tarr->isSharedMemory();
    return static_cast<int32_t*>(tarr->viewDataEither().unwrap(/*safe - caller sees isSharedMemory*/));
}

JS_FRIEND_API(uint32_t*)
JS_GetUint32ArrayData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    MOZ_ASSERT((int32_t) tarr->type() == Scalar::Uint32);
    *isSharedMemory = tarr->isSharedMemory();
    return static_cast<uint32_t*>(tarr->viewDataEither().unwrap(/*safe - caller sees isSharedMemory*/));
}

JS_FRIEND_API(float*)
JS_GetFloat32ArrayData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    MOZ_ASSERT((int32_t) tarr->type() == Scalar::Float32);
    *isSharedMemory = tarr->isSharedMemory();
    return static_cast<float*>(tarr->viewDataEither().unwrap(/*safe - caller sees isSharedMemory*/));
}

JS_FRIEND_API(double*)
JS_GetFloat64ArrayData(JSObject* obj, bool* isSharedMemory, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    MOZ_ASSERT((int32_t) tarr->type() == Scalar::Float64);
    *isSharedMemory = tarr->isSharedMemory();
    return static_cast<double*>(tarr->viewDataEither().unwrap(/*safe - caller sees isSharedMemory*/));
}

JS_FRIEND_API(bool)
JS_IsDataViewObject(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    return obj ? obj->is<DataViewObject>() : false;
}

JS_FRIEND_API(uint32_t)
JS_GetDataViewByteOffset(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<DataViewObject>().byteOffset();
}

JS_FRIEND_API(void*)
JS_GetDataViewData(JSObject* obj, const JS::AutoCheckCannotGC&)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return nullptr;
    return obj->as<DataViewObject>().dataPointer();
}

JS_FRIEND_API(uint32_t)
JS_GetDataViewByteLength(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    if (!obj)
        return 0;
    return obj->as<DataViewObject>().byteLength();
}

JS_FRIEND_API(JSObject*)
JS_NewDataView(JSContext* cx, HandleObject arrayBuffer, uint32_t byteOffset, int32_t byteLength)
{
    RootedObject constructor(cx);
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(&DataViewObject::class_);
    if (!GetBuiltinConstructor(cx, key, &constructor))
        return nullptr;

    FixedConstructArgs<3> cargs(cx);

    cargs[0].setObject(*arrayBuffer);
    cargs[1].setNumber(byteOffset);
    cargs[2].setInt32(byteLength);

    RootedValue fun(cx, ObjectValue(*constructor));
    RootedObject obj(cx);
    if (!Construct(cx, fun, cargs, fun, &obj))
        return nullptr;
    return obj;
}
