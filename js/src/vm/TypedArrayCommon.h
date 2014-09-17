/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_TypedArrayCommon_h
#define vm_TypedArrayCommon_h

/* Utilities and common inline code for TypedArray and SharedTypedArray */

#include "mozilla/FloatingPoint.h"

#include "js/Value.h"

#include "vm/SharedTypedArrayObject.h"
#include "vm/TypedArrayObject.h"

namespace js {

// Definitions below are shared between TypedArrayObject and
// SharedTypedArrayObject.

// ValueIsLength happens not to be according to ES6, which mandates
// the use of ToLength, which in turn includes ToNumber, ToInteger,
// and clamping.  ValueIsLength is used in the current TypedArray code
// but will disappear when that code is made spec-compliant.

inline bool
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

template<typename NativeType> static inline Scalar::Type TypeIDOfType();
template<> inline Scalar::Type TypeIDOfType<int8_t>() { return Scalar::Int8; }
template<> inline Scalar::Type TypeIDOfType<uint8_t>() { return Scalar::Uint8; }
template<> inline Scalar::Type TypeIDOfType<int16_t>() { return Scalar::Int16; }
template<> inline Scalar::Type TypeIDOfType<uint16_t>() { return Scalar::Uint16; }
template<> inline Scalar::Type TypeIDOfType<int32_t>() { return Scalar::Int32; }
template<> inline Scalar::Type TypeIDOfType<uint32_t>() { return Scalar::Uint32; }
template<> inline Scalar::Type TypeIDOfType<float>() { return Scalar::Float32; }
template<> inline Scalar::Type TypeIDOfType<double>() { return Scalar::Float64; }
template<> inline Scalar::Type TypeIDOfType<uint8_clamped>() { return Scalar::Uint8Clamped; }

inline bool
IsAnyTypedArray(HandleObject obj)
{
    return obj->is<TypedArrayObject>() || obj->is<SharedTypedArrayObject>();
}

inline bool
IsAnyTypedArray(const JSObject *obj)
{
    return obj->is<TypedArrayObject>() || obj->is<SharedTypedArrayObject>();
}

inline uint32_t
AnyTypedArrayLength(HandleObject obj)
{
    if (obj->is<TypedArrayObject>())
	return obj->as<TypedArrayObject>().length();
    return obj->as<SharedTypedArrayObject>().length();
}

inline uint32_t
AnyTypedArrayLength(const JSObject *obj)
{
    if (obj->is<TypedArrayObject>())
	return obj->as<TypedArrayObject>().length();
    return obj->as<SharedTypedArrayObject>().length();
}

inline Scalar::Type
AnyTypedArrayType(HandleObject obj)
{
    if (obj->is<TypedArrayObject>())
	return obj->as<TypedArrayObject>().type();
    return obj->as<SharedTypedArrayObject>().type();
}

inline Scalar::Type
AnyTypedArrayType(const JSObject *obj)
{
    if (obj->is<TypedArrayObject>())
	return obj->as<TypedArrayObject>().type();
    return obj->as<SharedTypedArrayObject>().type();
}

inline Shape*
AnyTypedArrayShape(HandleObject obj)
{
    if (obj->is<TypedArrayObject>())
	return obj->as<TypedArrayObject>().lastProperty();
    return obj->as<SharedTypedArrayObject>().lastProperty();
}

inline Shape*
AnyTypedArrayShape(const JSObject *obj)
{
    if (obj->is<TypedArrayObject>())
	return obj->as<TypedArrayObject>().lastProperty();
    return obj->as<SharedTypedArrayObject>().lastProperty();
}

inline const TypedArrayLayout&
AnyTypedArrayLayout(const JSObject *obj)
{
    if (obj->is<TypedArrayObject>())
	return obj->as<TypedArrayObject>().layout();
    return obj->as<SharedTypedArrayObject>().layout();
}

inline void *
AnyTypedArrayViewData(const JSObject *obj)
{
    if (obj->is<TypedArrayObject>())
	return obj->as<TypedArrayObject>().viewData();
    return obj->as<SharedTypedArrayObject>().viewData();
}

inline uint32_t
AnyTypedArrayByteLength(const JSObject *obj)
{
    if (obj->is<TypedArrayObject>())
	return obj->as<TypedArrayObject>().byteLength();
    return obj->as<SharedTypedArrayObject>().byteLength();
}

inline bool
IsAnyTypedArrayClass(const Class *clasp)
{
    return IsTypedArrayClass(clasp) || IsSharedTypedArrayClass(clasp);
}

// The subarray() method on TypedArray and SharedTypedArray

template<typename Adapter>
class TypedArraySubarrayTemplate
{
    typedef typename Adapter::ElementType NativeType;
    typedef typename Adapter::TypedArrayObjectType ArrayType;
    typedef typename Adapter::ArrayBufferObjectType BufferType;

  public:
    static JSObject *
    createSubarray(JSContext *cx, HandleObject tarrayArg, uint32_t begin, uint32_t end)
    {
        Rooted<ArrayType*> tarray(cx, &tarrayArg->as<ArrayType>());

        if (begin > tarray->length() || end > tarray->length() || begin > end) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_INDEX);
            return nullptr;
        }

        if (!Adapter::ensureHasBuffer(cx, tarray))
            return nullptr;

        Rooted<BufferType *> bufobj(cx, tarray->buffer());
        JS_ASSERT(bufobj);

        uint32_t length = end - begin;

        JS_ASSERT(begin < UINT32_MAX / sizeof(NativeType));
        uint32_t arrayByteOffset = tarray->byteOffset();
        JS_ASSERT(UINT32_MAX - begin * sizeof(NativeType) >= arrayByteOffset);
        uint32_t byteOffset = arrayByteOffset + begin * sizeof(NativeType);

        return Adapter::ThisTypedArrayObjectType::makeInstance(cx, bufobj, byteOffset, length);
    }

    static bool
    fun_subarray_impl(JSContext *cx, CallArgs args)
    {
        JS_ASSERT(Adapter::ThisTypedArrayObjectType::IsThisClass(args.thisv()));
        Rooted<ArrayType *> tarray(cx, &args.thisv().toObject().as<ArrayType>());

        // these are the default values
        uint32_t length = tarray->length();
        uint32_t begin = 0, end = length;

        if (args.length() > 0 && !ToClampedIndex(cx, args[0], length, &begin))
            return false;
        if (args.length() > 1 && !ToClampedIndex(cx, args[1], length, &end))
                    return false;

        if (begin > end)
            begin = end;

        JSObject *nobj = createSubarray(cx, tarray, begin, end);
        if (!nobj)
            return false;
        args.rval().setObject(*nobj);
        return true;
    }
};

// The copyWithin() method on TypedArray and SharedTypedArray

template<typename Adapter>
class TypedArrayCopyWithinTemplate
{
    typedef typename Adapter::ElementType NativeType;
    typedef typename Adapter::TypedArrayObjectType ArrayType;

  public:
    /* copyWithin(target, start[, end]) */
    // ES6 draft rev 26, 22.2.3.5
    static bool
    fun_copyWithin_impl(JSContext *cx, CallArgs args)
    {
        MOZ_ASSERT(Adapter::ThisTypedArrayObjectType::IsThisClass(args.thisv()));

        // Steps 1-2.
        Rooted<ArrayType*> obj(cx, &args.thisv().toObject().as<ArrayType>());

        // Steps 3-4.
        uint32_t len = obj->length();

        // Steps 6-8.
        uint32_t to;
        if (!ToClampedIndex(cx, args.get(0), len, &to))
            return false;

        // Steps 9-11.
        uint32_t from;
        if (!ToClampedIndex(cx, args.get(1), len, &from))
            return false;

        // Steps 12-14.
        uint32_t final;
        if (args.get(2).isUndefined()) {
            final = len;
        } else {
            if (!ToClampedIndex(cx, args.get(2), len, &final))
                return false;
        }

        // Steps 15-18.

        // If |final - from < 0|, then |count| will be less than 0, so step 18
        // never loops.  Exit early so |count| can use a non-negative type.
        // Also exit early if elements are being moved to their pre-existing
        // location.
        if (final < from || to == from) {
            args.rval().setObject(*obj);
            return true;
        }

        uint32_t count = Min(final - from, len - to);
        uint32_t lengthDuringMove = obj->length(); // beware ToClampedIndex

        MOZ_ASSERT(to <= INT32_MAX, "size limited to 2**31");
        MOZ_ASSERT(from <= INT32_MAX, "size limited to 2**31");
        MOZ_ASSERT(count <= INT32_MAX, "size limited to 2**31");

        if (from + count > lengthDuringMove || to + count > lengthDuringMove) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        const size_t ElementSize = sizeof(NativeType);
        MOZ_ASSERT(to <= INT32_MAX / ElementSize, "overall byteLength capped at INT32_MAX");
        MOZ_ASSERT(from <= INT32_MAX / ElementSize, "overall byteLength capped at INT32_MAX");
        MOZ_ASSERT(count <= INT32_MAX / ElementSize, "overall byteLength capped at INT32_MAX");

        uint32_t byteDest = to * ElementSize;
        uint32_t byteSrc = from * ElementSize;
        uint32_t byteSize = count * ElementSize;

#ifdef DEBUG
        uint32_t viewByteLength = obj->byteLength();
        JS_ASSERT(byteDest <= viewByteLength);
        JS_ASSERT(byteSrc <= viewByteLength);
        JS_ASSERT(byteDest + byteSize <= viewByteLength);
        JS_ASSERT(byteSrc + byteSize <= viewByteLength);

        // Should not overflow because size is limited to 2^31
        JS_ASSERT(byteDest + byteSize >= byteDest);
        JS_ASSERT(byteSrc + byteSize >= byteSrc);
#endif

        uint8_t *data = static_cast<uint8_t*>(obj->viewData());
        mozilla::PodMove(&data[byteDest], &data[byteSrc], byteSize);

        // Step 19.
        args.rval().set(args.thisv());
        return true;
    }
};

// The set() method on TypedArray and SharedTypedArray

template<class Adapter>
class TypedArraySetTemplate
{
    typedef typename Adapter::ElementType NativeType;
    typedef typename Adapter::TypedArrayObjectType ArrayType;

  public:
    /* set(array[, offset]) */
    static bool
    fun_set_impl(JSContext *cx, CallArgs args)
    {
        MOZ_ASSERT(Adapter::ThisTypedArrayObjectType::IsThisClass(args.thisv()));

        Rooted<ArrayType*> tarray(cx, &args.thisv().toObject().as<ArrayType>());

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
        if (IsAnyTypedArray(arg0.get())) {
            if (AnyTypedArrayLength(arg0.get()) > tarray->length() - offset) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_ARRAY_LENGTH);
                return false;
            }

            if (!copyFromTypedArray(cx, tarray, arg0, offset))
                return false;
        } else {
            uint32_t len;
            if (!GetLengthProperty(cx, arg0, &len))
                return false;

            if (uint32_t(offset) > tarray->length() || len > tarray->length() - offset) {
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
    copyFromArray(JSContext *cx, HandleObject thisTypedArrayObj,
                  HandleObject source, uint32_t len, uint32_t offset = 0)
    {
        Rooted<ArrayType*> thisTypedArray(cx, &thisTypedArrayObj->as<ArrayType>());
        JS_ASSERT(offset <= thisTypedArray->length());
        JS_ASSERT(len <= thisTypedArray->length() - offset);
        if (source->is<TypedArrayObject>())
            return copyFromTypedArray(cx, thisTypedArray, source, offset);

        uint32_t i = 0;
        if (source->isNative()) {
            // Attempt fast-path infallible conversion of dense elements up to
            // the first potentially side-effectful lookup or conversion.
            uint32_t bound = Min(source->getDenseInitializedLength(), len);

            NativeType *dest = static_cast<NativeType*>(thisTypedArray->viewData()) + offset;

            const Value *srcValues = source->getDenseElements();
            for (; i < bound; i++) {
                // Note: holes don't convert infallibly.
                if (!canConvertInfallibly(srcValues[i]))
                    break;
                dest[i] = infallibleValueToNative(srcValues[i]);
            }
            if (i == len)
                return true;
        }

        // Convert and copy any remaining elements generically.
        RootedValue v(cx);
        for (; i < len; i++) {
            if (!JSObject::getElement(cx, source, source, i, &v))
                return false;

            NativeType n;
            if (!valueToNative(cx, v, &n))
                return false;

            len = Min(len, thisTypedArray->length());
            if (i >= len)
                break;

            // Compute every iteration in case getElement acts wacky.
            void *data = thisTypedArray->viewData();
            static_cast<NativeType*>(data)[offset + i] = n;
        }

        return true;
    }

    static bool
    copyFromTypedArray(JSContext *cx, JSObject *thisTypedArrayObj, JSObject *tarrayObj,
                       uint32_t offset)
    {
        Rooted<ArrayType *> thisTypedArray(cx, &thisTypedArrayObj->as<ArrayType>());
        Rooted<ArrayType *> tarray(cx, &tarrayObj->as<ArrayType>());
        JS_ASSERT(offset <= thisTypedArray->length());
        JS_ASSERT(tarray->length() <= thisTypedArray->length() - offset);

        // Object equality is not good enough for SharedArrayBufferObject.
        if (Adapter::sameBuffer(tarray->buffer(), thisTypedArray->buffer()))
            return copyFromWithOverlap(cx, thisTypedArray, tarray, offset);

        NativeType *dest = static_cast<NativeType*>(thisTypedArray->viewData()) + offset;

        if (tarray->type() == thisTypedArray->type()) {
            js_memcpy(dest, tarray->viewData(), tarray->byteLength());
            return true;
        }

        unsigned srclen = tarray->length();
        switch (tarray->type()) {
          case Scalar::Int8: {
            int8_t *src = static_cast<int8_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Uint8:
          case Scalar::Uint8Clamped: {
            uint8_t *src = static_cast<uint8_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Int16: {
            int16_t *src = static_cast<int16_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Uint16: {
            uint16_t *src = static_cast<uint16_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Int32: {
            int32_t *src = static_cast<int32_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Uint32: {
            uint32_t *src = static_cast<uint32_t*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Float32: {
            float *src = static_cast<float*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Float64: {
            double *src = static_cast<double*>(tarray->viewData());
            for (unsigned i = 0; i < srclen; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          default:
            MOZ_CRASH("copyFrom with a TypedArrayObject of unknown type");
        }

        return true;
    }

    static bool
    copyFromWithOverlap(JSContext *cx, JSObject *selfObj, JSObject *tarrayObj, uint32_t offset)
    {
        ArrayType *self = &selfObj->as<ArrayType>();
        ArrayType *tarray = &tarrayObj->as<ArrayType>();

        JS_ASSERT(offset <= self->length());

        NativeType *dest = static_cast<NativeType*>(self->viewData()) + offset;
        uint32_t byteLength = tarray->byteLength();

        if (tarray->type() == self->type()) {
            memmove(dest, tarray->viewData(), byteLength);
            return true;
        }

        // We have to make a copy of the source array here, since
        // there's overlap, and we have to convert types.
        uint8_t *srcbuf = self->zone()->template pod_malloc<uint8_t>(byteLength);
        if (!srcbuf)
            return false;
        js_memcpy(srcbuf, tarray->viewData(), byteLength);

        uint32_t len = tarray->length();
        switch (tarray->type()) {
          case Scalar::Int8: {
            int8_t *src = (int8_t*) srcbuf;
            for (unsigned i = 0; i < len; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Uint8:
          case Scalar::Uint8Clamped: {
            uint8_t *src = (uint8_t*) srcbuf;
            for (unsigned i = 0; i < len; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Int16: {
            int16_t *src = (int16_t*) srcbuf;
            for (unsigned i = 0; i < len; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Uint16: {
            uint16_t *src = (uint16_t*) srcbuf;
            for (unsigned i = 0; i < len; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Int32: {
            int32_t *src = (int32_t*) srcbuf;
            for (unsigned i = 0; i < len; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Uint32: {
            uint32_t *src = (uint32_t*) srcbuf;
            for (unsigned i = 0; i < len; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Float32: {
            float *src = (float*) srcbuf;
            for (unsigned i = 0; i < len; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          case Scalar::Float64: {
            double *src = (double*) srcbuf;
            for (unsigned i = 0; i < len; ++i)
                *dest++ = NativeType(*src++);
            break;
          }
          default:
            MOZ_CRASH("copyFromWithOverlap with a TypedArrayObject of unknown type");
        }

        js_free(srcbuf);
        return true;
    }

    static bool
    canConvertInfallibly(const Value &v)
    {
        return v.isNumber() || v.isBoolean() || v.isNull() || v.isUndefined();
    }

    static NativeType
    infallibleValueToNative(const Value &v)
    {
        if (v.isInt32())
            return NativeType(v.toInt32());
        if (v.isDouble())
            return doubleToNative(v.toDouble());
        if (v.isBoolean())
            return NativeType(v.toBoolean());
        if (v.isNull())
            return NativeType(0);

        MOZ_ASSERT(v.isUndefined());
        return TypeIsFloatingPoint<NativeType>() ? NativeType(JS::GenericNaN()) : NativeType(0);
    }

    static bool
    valueToNative(JSContext *cx, const Value &v, NativeType *result)
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

    static NativeType
    doubleToNative(double d)
    {
        if (TypeIsFloatingPoint<NativeType>()) {
#ifdef JS_MORE_DETERMINISTIC
            // The JS spec doesn't distinguish among different NaN values, and
            // it deliberately doesn't specify the bit pattern written to a
            // typed array when NaN is written into it.  This bit-pattern
            // inconsistency could confuse deterministic testing, so always
            // canonicalize NaN values in more-deterministic builds.
            d = JS::CanonicalizeNaN(d);
#endif
            return NativeType(d);
        }
        if (MOZ_UNLIKELY(mozilla::IsNaN(d)))
            return NativeType(0);
        if (TypeIsUnsigned<NativeType>())
            return NativeType(ToUint32(d));
        return NativeType(ToInt32(d));
    }
};

} // namespace js

#endif // vm_TypedArrayCommon_h
