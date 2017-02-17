/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_TypedArrayObject_inl_h
#define vm_TypedArrayObject_inl_h

/* Utilities and common inline code for TypedArray */

#include "vm/TypedArrayObject.h"

#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"

#include "jsarray.h"
#include "jscntxt.h"
#include "jsnum.h"

#include "jit/AtomicOperations.h"

#include "js/Conversions.h"
#include "js/Value.h"

#include "vm/NativeObject.h"

namespace js {

// ValueIsLength happens not to be according to ES6, which mandates
// the use of ToLength, which in turn includes ToNumber, ToInteger,
// and clamping.  ValueIsLength is used in the current TypedArray code
// but will disappear when that code is made spec-compliant.

inline bool
ValueIsLength(const Value& v, uint32_t* len)
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
        if (mozilla::IsNaN(d))
            return false;

        uint32_t length = uint32_t(d);
        if (d != double(length))
            return false;

        *len = length;
        return true;
    }

    return false;
}

template<typename To, typename From>
inline To
ConvertNumber(From src);

template<>
inline int8_t
ConvertNumber<int8_t, float>(float src)
{
    return JS::ToInt8(src);
}

template<>
inline uint8_t
ConvertNumber<uint8_t, float>(float src)
{
    return JS::ToUint8(src);
}

template<>
inline uint8_clamped
ConvertNumber<uint8_clamped, float>(float src)
{
    return uint8_clamped(src);
}

template<>
inline int16_t
ConvertNumber<int16_t, float>(float src)
{
    return JS::ToInt16(src);
}

template<>
inline uint16_t
ConvertNumber<uint16_t, float>(float src)
{
    return JS::ToUint16(src);
}

template<>
inline int32_t
ConvertNumber<int32_t, float>(float src)
{
    return JS::ToInt32(src);
}

template<>
inline uint32_t
ConvertNumber<uint32_t, float>(float src)
{
    return JS::ToUint32(src);
}

template<> inline int8_t
ConvertNumber<int8_t, double>(double src)
{
    return JS::ToInt8(src);
}

template<>
inline uint8_t
ConvertNumber<uint8_t, double>(double src)
{
    return JS::ToUint8(src);
}

template<>
inline uint8_clamped
ConvertNumber<uint8_clamped, double>(double src)
{
    return uint8_clamped(src);
}

template<>
inline int16_t
ConvertNumber<int16_t, double>(double src)
{
    return JS::ToInt16(src);
}

template<>
inline uint16_t
ConvertNumber<uint16_t, double>(double src)
{
    return JS::ToUint16(src);
}

template<>
inline int32_t
ConvertNumber<int32_t, double>(double src)
{
    return JS::ToInt32(src);
}

template<>
inline uint32_t
ConvertNumber<uint32_t, double>(double src)
{
    return JS::ToUint32(src);
}

template<typename To, typename From>
inline To
ConvertNumber(From src)
{
    static_assert(!mozilla::IsFloatingPoint<From>::value ||
                  (mozilla::IsFloatingPoint<From>::value && mozilla::IsFloatingPoint<To>::value),
                  "conversion from floating point to int should have been handled by "
                  "specializations above");
    return To(src);
}

template<typename NativeType> struct TypeIDOfType;
template<> struct TypeIDOfType<int8_t> { static const Scalar::Type id = Scalar::Int8; };
template<> struct TypeIDOfType<uint8_t> { static const Scalar::Type id = Scalar::Uint8; };
template<> struct TypeIDOfType<int16_t> { static const Scalar::Type id = Scalar::Int16; };
template<> struct TypeIDOfType<uint16_t> { static const Scalar::Type id = Scalar::Uint16; };
template<> struct TypeIDOfType<int32_t> { static const Scalar::Type id = Scalar::Int32; };
template<> struct TypeIDOfType<uint32_t> { static const Scalar::Type id = Scalar::Uint32; };
template<> struct TypeIDOfType<float> { static const Scalar::Type id = Scalar::Float32; };
template<> struct TypeIDOfType<double> { static const Scalar::Type id = Scalar::Float64; };
template<> struct TypeIDOfType<uint8_clamped> { static const Scalar::Type id = Scalar::Uint8Clamped; };

class SharedOps
{
  public:
    template<typename T>
    static T load(SharedMem<T*> addr) {
        return js::jit::AtomicOperations::loadSafeWhenRacy(addr);
    }

    template<typename T>
    static void store(SharedMem<T*> addr, T value) {
        js::jit::AtomicOperations::storeSafeWhenRacy(addr, value);
    }

    template<typename T>
    static void memcpy(SharedMem<T*> dest, SharedMem<T*> src, size_t size) {
        js::jit::AtomicOperations::memcpySafeWhenRacy(dest, src, size);
    }

    template<typename T>
    static void memmove(SharedMem<T*> dest, SharedMem<T*> src, size_t size) {
        js::jit::AtomicOperations::memmoveSafeWhenRacy(dest, src, size);
    }

    template<typename T>
    static void podCopy(SharedMem<T*> dest, SharedMem<T*> src, size_t nelem) {
        js::jit::AtomicOperations::podCopySafeWhenRacy(dest, src, nelem);
    }

    template<typename T>
    static void podMove(SharedMem<T*> dest, SharedMem<T*> src, size_t nelem) {
        js::jit::AtomicOperations::podMoveSafeWhenRacy(dest, src, nelem);
    }

    static SharedMem<void*> extract(TypedArrayObject* obj) {
        return obj->viewDataEither();
    }
};

class UnsharedOps
{
  public:
    template<typename T>
    static T load(SharedMem<T*> addr) {
        return *addr.unwrapUnshared();
    }

    template<typename T>
    static void store(SharedMem<T*> addr, T value) {
        *addr.unwrapUnshared() = value;
    }

    template<typename T>
    static void memcpy(SharedMem<T*> dest, SharedMem<T*> src, size_t size) {
        ::memcpy(dest.unwrapUnshared(), src.unwrapUnshared(), size);
    }

    template<typename T>
    static void memmove(SharedMem<T*> dest, SharedMem<T*> src, size_t size) {
        ::memmove(dest.unwrapUnshared(), src.unwrapUnshared(), size);
    }

    template<typename T>
    static void podCopy(SharedMem<T*> dest, SharedMem<T*> src, size_t nelem) {
        mozilla::PodCopy(dest.unwrapUnshared(), src.unwrapUnshared(), nelem);
    }

    template<typename T>
    static void podMove(SharedMem<T*> dest, SharedMem<T*> src, size_t nelem) {
        mozilla::PodMove(dest.unwrapUnshared(), src.unwrapUnshared(), nelem);
    }

    static SharedMem<void*> extract(TypedArrayObject* obj) {
        return SharedMem<void*>::unshared(obj->viewDataUnshared());
    }
};

template<typename T, typename Ops>
class ElementSpecific
{
  public:
    /*
     * Copy |source|'s elements into |target|, starting at |target[offset]|.
     * Act as if the assignments occurred from a fresh copy of |source|, in
     * case the two memory ranges overlap.
     */
    static bool
    setFromTypedArray(JSContext* cx,
                      Handle<TypedArrayObject*> target, Handle<TypedArrayObject*> source,
                      uint32_t offset)
    {
        MOZ_ASSERT(TypeIDOfType<T>::id == target->type(),
                   "calling wrong setFromTypedArray specialization");
        MOZ_ASSERT(!target->hasDetachedBuffer(), "target isn't detached");
        MOZ_ASSERT(!source->hasDetachedBuffer(), "source isn't detached");

        MOZ_ASSERT(offset <= target->length());
        MOZ_ASSERT(source->length() <= target->length() - offset);

        if (TypedArrayObject::sameBuffer(target, source))
            return setFromOverlappingTypedArray(cx, target, source, offset);

        SharedMem<T*> dest = target->viewDataEither().template cast<T*>() + offset;
        uint32_t count = source->length();

        if (source->type() == target->type()) {
            Ops::podCopy(dest, source->viewDataEither().template cast<T*>(), count);
            return true;
        }

        // Inhibit unaligned accesses on ARM (bug 1097253, a compiler bug).
#ifdef __arm__
#  define JS_VOLATILE_ARM volatile
#else
#  define JS_VOLATILE_ARM
#endif

        SharedMem<void*> data = Ops::extract(source);
        switch (source->type()) {
          case Scalar::Int8: {
            SharedMem<JS_VOLATILE_ARM int8_t*> src = data.cast<JS_VOLATILE_ARM int8_t*>();
            for (uint32_t i = 0; i < count; ++i)
                Ops::store(dest++, ConvertNumber<T>(Ops::load(src++)));
            break;
          }
          case Scalar::Uint8:
          case Scalar::Uint8Clamped: {
            SharedMem<JS_VOLATILE_ARM uint8_t*> src = data.cast<JS_VOLATILE_ARM uint8_t*>();
            for (uint32_t i = 0; i < count; ++i)
                Ops::store(dest++, ConvertNumber<T>(Ops::load(src++)));
            break;
          }
          case Scalar::Int16: {
            SharedMem<JS_VOLATILE_ARM int16_t*> src = data.cast<JS_VOLATILE_ARM int16_t*>();
            for (uint32_t i = 0; i < count; ++i)
                Ops::store(dest++, ConvertNumber<T>(Ops::load(src++)));
            break;
          }
          case Scalar::Uint16: {
            SharedMem<JS_VOLATILE_ARM uint16_t*> src = data.cast<JS_VOLATILE_ARM uint16_t*>();
            for (uint32_t i = 0; i < count; ++i)
                Ops::store(dest++, ConvertNumber<T>(Ops::load(src++)));
            break;
          }
          case Scalar::Int32: {
            SharedMem<JS_VOLATILE_ARM int32_t*> src = data.cast<JS_VOLATILE_ARM int32_t*>();
            for (uint32_t i = 0; i < count; ++i)
                Ops::store(dest++, ConvertNumber<T>(Ops::load(src++)));
            break;
          }
          case Scalar::Uint32: {
            SharedMem<JS_VOLATILE_ARM uint32_t*> src = data.cast<JS_VOLATILE_ARM uint32_t*>();
            for (uint32_t i = 0; i < count; ++i)
                Ops::store(dest++, ConvertNumber<T>(Ops::load(src++)));
            break;
          }
          case Scalar::Float32: {
            SharedMem<JS_VOLATILE_ARM float*> src = data.cast<JS_VOLATILE_ARM float*>();
            for (uint32_t i = 0; i < count; ++i)
                Ops::store(dest++, ConvertNumber<T>(Ops::load(src++)));
            break;
          }
          case Scalar::Float64: {
            SharedMem<JS_VOLATILE_ARM double*> src = data.cast<JS_VOLATILE_ARM double*>();
            for (uint32_t i = 0; i < count; ++i)
                Ops::store(dest++, ConvertNumber<T>(Ops::load(src++)));
            break;
          }
          default:
            MOZ_CRASH("setFromTypedArray with a typed array with bogus type");
        }

#undef JS_VOLATILE_ARM

        return true;
    }

    /*
     * Copy |source[0]| to |source[len]| (exclusive) elements into the typed
     * array |target|, starting at index |offset|.  |source| must not be a
     * typed array.
     */
    static bool
    setFromNonTypedArray(JSContext* cx, Handle<TypedArrayObject*> target, HandleObject source,
                         uint32_t len, uint32_t offset = 0)
    {
        MOZ_ASSERT(target->type() == TypeIDOfType<T>::id,
                   "target type and NativeType must match");
        MOZ_ASSERT(!target->hasDetachedBuffer(), "target isn't detached");
        MOZ_ASSERT(!source->is<TypedArrayObject>(),
                   "use setFromTypedArray instead of this method");

        uint32_t i = 0;
        if (source->isNative()) {
            // Attempt fast-path infallible conversion of dense elements up to
            // the first potentially side-effectful lookup or conversion.
            uint32_t bound = Min(source->as<NativeObject>().getDenseInitializedLength(), len);

            SharedMem<T*> dest = target->viewDataEither().template cast<T*>() + offset;

            MOZ_ASSERT(!canConvertInfallibly(MagicValue(JS_ELEMENTS_HOLE)),
                       "the following loop must abort on holes");

            const Value* srcValues = source->as<NativeObject>().getDenseElements();
            for (; i < bound; i++) {
                if (!canConvertInfallibly(srcValues[i]))
                    break;
                Ops::store(dest + i, infallibleValueToNative(srcValues[i]));
            }
            if (i == len)
                return true;
        }

        // Convert and copy any remaining elements generically.
        RootedValue v(cx);
        for (; i < len; i++) {
            if (!GetElement(cx, source, source, i, &v))
                return false;

            T n;
            if (!valueToNative(cx, v, &n))
                return false;

            len = Min(len, target->length());
            if (i >= len)
                break;

            // Compute every iteration in case getElement/valueToNative
            // detaches the underlying array buffer or GC moves the data.
            SharedMem<T*> dest = target->viewDataEither().template cast<T*>() + offset + i;
            Ops::store(dest, n);
        }

        return true;
    }

    /*
     * Copy |source| into the typed array |target|.
     */
    static bool
    initFromIterablePackedArray(JSContext* cx, Handle<TypedArrayObject*> target,
                                HandleArrayObject source)
    {
        MOZ_ASSERT(target->type() == TypeIDOfType<T>::id,
                   "target type and NativeType must match");
        MOZ_ASSERT(!target->hasDetachedBuffer(), "target isn't detached");
        MOZ_ASSERT(IsPackedArray(source), "source array must be packed");
        MOZ_ASSERT(source->getDenseInitializedLength() <= target->length());

        uint32_t len = source->getDenseInitializedLength();
        uint32_t i = 0;

        // Attempt fast-path infallible conversion of dense elements up to the
        // first potentially side-effectful conversion.

        SharedMem<T*> dest = target->viewDataEither().template cast<T*>();

        const Value* srcValues = source->getDenseElements();
        for (; i < len; i++) {
            if (!canConvertInfallibly(srcValues[i]))
                break;
            Ops::store(dest + i, infallibleValueToNative(srcValues[i]));
        }
        if (i == len)
            return true;

        // Convert any remaining elements by first collecting them into a
        // temporary list, and then copying them into the typed array.
        AutoValueVector values(cx);
        if (!values.append(srcValues + i, len - i))
            return false;

        RootedValue v(cx);
        for (uint32_t j = 0; j < values.length(); i++, j++) {
            v = values[j];

            T n;
            if (!valueToNative(cx, v, &n))
                return false;

            // |target| is a newly allocated typed array and not yet visible to
            // content script, so valueToNative can't detach the underlying
            // buffer.
            MOZ_ASSERT(i < target->length());

            // Compute every iteration in case GC moves the data.
            SharedMem<T*> newDest = target->viewDataEither().template cast<T*>();
            Ops::store(newDest + i, n);
        }

        return true;
    }

  private:
    static bool
    setFromOverlappingTypedArray(JSContext* cx,
                                 Handle<TypedArrayObject*> target,
                                 Handle<TypedArrayObject*> source,
                                 uint32_t offset)
    {
        MOZ_ASSERT(TypeIDOfType<T>::id == target->type(),
                   "calling wrong setFromTypedArray specialization");
        MOZ_ASSERT(!target->hasDetachedBuffer(), "target isn't detached");
        MOZ_ASSERT(!source->hasDetachedBuffer(), "source isn't detached");
        MOZ_ASSERT(TypedArrayObject::sameBuffer(target, source),
                   "the provided arrays don't actually overlap, so it's "
                   "undesirable to use this method");

        MOZ_ASSERT(offset <= target->length());
        MOZ_ASSERT(source->length() <= target->length() - offset);

        SharedMem<T*> dest = target->viewDataEither().template cast<T*>() + offset;
        uint32_t len = source->length();

        if (source->type() == target->type()) {
            SharedMem<T*> src = source->viewDataEither().template cast<T*>();
            Ops::podMove(dest, src, len);
            return true;
        }

        // Copy |source| in case it overlaps the target elements being set.
        size_t sourceByteLen = len * source->bytesPerElement();
        void* data = target->zone()->template pod_malloc<uint8_t>(sourceByteLen);
        if (!data)
            return false;
        Ops::memcpy(SharedMem<void*>::unshared(data),
                    source->viewDataEither(),
                    sourceByteLen);

        switch (source->type()) {
          case Scalar::Int8: {
            int8_t* src = static_cast<int8_t*>(data);
            for (uint32_t i = 0; i < len; ++i)
                Ops::store(dest++, ConvertNumber<T>(*src++));
            break;
          }
          case Scalar::Uint8:
          case Scalar::Uint8Clamped: {
            uint8_t* src = static_cast<uint8_t*>(data);
            for (uint32_t i = 0; i < len; ++i)
                Ops::store(dest++, ConvertNumber<T>(*src++));
            break;
          }
          case Scalar::Int16: {
            int16_t* src = static_cast<int16_t*>(data);
            for (uint32_t i = 0; i < len; ++i)
                Ops::store(dest++, ConvertNumber<T>(*src++));
            break;
          }
          case Scalar::Uint16: {
            uint16_t* src = static_cast<uint16_t*>(data);
            for (uint32_t i = 0; i < len; ++i)
                Ops::store(dest++, ConvertNumber<T>(*src++));
            break;
          }
          case Scalar::Int32: {
            int32_t* src = static_cast<int32_t*>(data);
            for (uint32_t i = 0; i < len; ++i)
                Ops::store(dest++, ConvertNumber<T>(*src++));
            break;
          }
          case Scalar::Uint32: {
            uint32_t* src = static_cast<uint32_t*>(data);
            for (uint32_t i = 0; i < len; ++i)
                Ops::store(dest++, ConvertNumber<T>(*src++));
            break;
          }
          case Scalar::Float32: {
            float* src = static_cast<float*>(data);
            for (uint32_t i = 0; i < len; ++i)
                Ops::store(dest++, ConvertNumber<T>(*src++));
            break;
          }
          case Scalar::Float64: {
            double* src = static_cast<double*>(data);
            for (uint32_t i = 0; i < len; ++i)
                Ops::store(dest++, ConvertNumber<T>(*src++));
            break;
          }
          default:
            MOZ_CRASH("setFromOverlappingTypedArray with a typed array with bogus type");
        }

        js_free(data);
        return true;
    }

    static bool
    canConvertInfallibly(const Value& v)
    {
        return v.isNumber() || v.isBoolean() || v.isNull() || v.isUndefined();
    }

    static T
    infallibleValueToNative(const Value& v)
    {
        if (v.isInt32())
            return T(v.toInt32());
        if (v.isDouble())
            return doubleToNative(v.toDouble());
        if (v.isBoolean())
            return T(v.toBoolean());
        if (v.isNull())
            return T(0);

        MOZ_ASSERT(v.isUndefined());
        return TypeIsFloatingPoint<T>() ? T(JS::GenericNaN()) : T(0);
    }

    static bool
    valueToNative(JSContext* cx, HandleValue v, T* result)
    {
        MOZ_ASSERT(!v.isMagic());

        if (MOZ_LIKELY(canConvertInfallibly(v))) {
            *result = infallibleValueToNative(v);
            return true;
        }

        double d;
        MOZ_ASSERT(v.isString() || v.isObject() || v.isSymbol());
        if (!(v.isString() ? StringToNumber(cx, v.toString(), &d) : ToNumber(cx, v, &d)))
            return false;

        *result = doubleToNative(d);
        return true;
    }

    static T
    doubleToNative(double d)
    {
        if (TypeIsFloatingPoint<T>()) {
#ifdef JS_MORE_DETERMINISTIC
            // The JS spec doesn't distinguish among different NaN values, and
            // it deliberately doesn't specify the bit pattern written to a
            // typed array when NaN is written into it.  This bit-pattern
            // inconsistency could confuse deterministic testing, so always
            // canonicalize NaN values in more-deterministic builds.
            d = JS::CanonicalizeNaN(d);
#endif
            return T(d);
        }
        if (MOZ_UNLIKELY(mozilla::IsNaN(d)))
            return T(0);
        if (TypeIDOfType<T>::id == Scalar::Uint8Clamped)
            return T(d);
        if (TypeIsUnsigned<T>())
            return T(JS::ToUint32(d));
        return T(JS::ToInt32(d));
    }
};

} // namespace js

#endif // vm_TypedArrayObject_inl_h
