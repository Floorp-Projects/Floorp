/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_UnboxedObject_inl_h
#define vm_UnboxedObject_inl_h

#include "vm/UnboxedObject.h"

#include "gc/StoreBuffer-inl.h"
#include "vm/ArrayObject-inl.h"
#include "vm/NativeObject-inl.h"

namespace js {

static inline Value
GetUnboxedValue(uint8_t* p, JSValueType type, bool maybeUninitialized)
{
    switch (type) {
      case JSVAL_TYPE_BOOLEAN:
        return BooleanValue(*p != 0);

      case JSVAL_TYPE_INT32:
        return Int32Value(*reinterpret_cast<int32_t*>(p));

      case JSVAL_TYPE_DOUBLE: {
        // During unboxed plain object creation, non-GC thing properties are
        // left uninitialized. This is normally fine, since the properties will
        // be filled in shortly, but if they are read before that happens we
        // need to make sure that doubles are canonical.
        double d = *reinterpret_cast<double*>(p);
        if (maybeUninitialized)
            return DoubleValue(JS::CanonicalizeNaN(d));
        return DoubleValue(d);
      }

      case JSVAL_TYPE_STRING:
        return StringValue(*reinterpret_cast<JSString**>(p));

      case JSVAL_TYPE_OBJECT:
        return ObjectOrNullValue(*reinterpret_cast<JSObject**>(p));

      default:
        MOZ_CRASH("Invalid type for unboxed value");
    }
}

static inline void
SetUnboxedValueNoTypeChange(JSObject* unboxedObject,
                            uint8_t* p, JSValueType type, const Value& v,
                            bool preBarrier)
{
    switch (type) {
      case JSVAL_TYPE_BOOLEAN:
        *p = v.toBoolean();
        return;

      case JSVAL_TYPE_INT32:
        *reinterpret_cast<int32_t*>(p) = v.toInt32();
        return;

      case JSVAL_TYPE_DOUBLE:
        *reinterpret_cast<double*>(p) = v.toNumber();
        return;

      case JSVAL_TYPE_STRING: {
        MOZ_ASSERT(!IsInsideNursery(v.toString()));
        JSString** np = reinterpret_cast<JSString**>(p);
        if (preBarrier)
            JSString::writeBarrierPre(*np);
        *np = v.toString();
        return;
      }

      case JSVAL_TYPE_OBJECT: {
        JSObject** np = reinterpret_cast<JSObject**>(p);

        // Manually trigger post barriers on the whole object. If we treat
        // the pointer as a HeapPtrObject we will get confused later if the
        // object is converted to its native representation.
        JSObject* obj = v.toObjectOrNull();
        if (IsInsideNursery(obj) && !IsInsideNursery(unboxedObject))
            unboxedObject->zone()->group()->storeBuffer().putWholeCell(unboxedObject);

        if (preBarrier)
            JSObject::writeBarrierPre(*np);
        *np = obj;
        return;
      }

      default:
        MOZ_CRASH("Invalid type for unboxed value");
    }
}

static inline bool
SetUnboxedValue(JSContext* cx, JSObject* unboxedObject, jsid id,
                uint8_t* p, JSValueType type, const Value& v, bool preBarrier)
{
    switch (type) {
      case JSVAL_TYPE_BOOLEAN:
        if (v.isBoolean()) {
            *p = v.toBoolean();
            return true;
        }
        return false;

      case JSVAL_TYPE_INT32:
        if (v.isInt32()) {
            *reinterpret_cast<int32_t*>(p) = v.toInt32();
            return true;
        }
        return false;

      case JSVAL_TYPE_DOUBLE:
        if (v.isNumber()) {
            *reinterpret_cast<double*>(p) = v.toNumber();
            return true;
        }
        return false;

      case JSVAL_TYPE_STRING:
        if (v.isString()) {
            MOZ_ASSERT(!IsInsideNursery(v.toString()));
            JSString** np = reinterpret_cast<JSString**>(p);
            if (preBarrier)
                JSString::writeBarrierPre(*np);
            *np = v.toString();
            return true;
        }
        return false;

      case JSVAL_TYPE_OBJECT:
        if (v.isObjectOrNull()) {
            JSObject** np = reinterpret_cast<JSObject**>(p);

            // Update property types when writing object properties. Types for
            // other properties were captured when the unboxed layout was
            // created.
            AddTypePropertyId(cx, unboxedObject, id, v);

            // As above, trigger post barriers on the whole object.
            JSObject* obj = v.toObjectOrNull();
            if (IsInsideNursery(v.toObjectOrNull()) && !IsInsideNursery(unboxedObject))
                unboxedObject->zone()->group()->storeBuffer().putWholeCell(unboxedObject);

            if (preBarrier)
                JSObject::writeBarrierPre(*np);
            *np = obj;
            return true;
        }
        return false;

      default:
        MOZ_CRASH("Invalid type for unboxed value");
    }
}

/////////////////////////////////////////////////////////////////////
// UnboxedPlainObject
/////////////////////////////////////////////////////////////////////

inline const UnboxedLayout&
UnboxedPlainObject::layout() const
{
    return group()->unboxedLayout();
}

/////////////////////////////////////////////////////////////////////
// Combined methods for NativeObject and UnboxedArrayObject accesses.
/////////////////////////////////////////////////////////////////////

static inline bool
HasAnyBoxedOrUnboxedDenseElements(JSObject* obj)
{
    return obj->isNative();
}

static inline size_t
GetAnyBoxedOrUnboxedInitializedLength(JSObject* obj)
{
    if (obj->isNative())
        return obj->as<NativeObject>().getDenseInitializedLength();
    return 0;
}

static inline size_t
GetAnyBoxedOrUnboxedCapacity(JSObject* obj)
{
    if (obj->isNative())
        return obj->as<NativeObject>().getDenseCapacity();
    return 0;
}

static inline Value
GetAnyBoxedOrUnboxedDenseElement(JSObject* obj, size_t index)
{
    return obj->as<NativeObject>().getDenseElement(index);
}

static inline size_t
GetAnyBoxedOrUnboxedArrayLength(JSObject* obj)
{
    return obj->as<ArrayObject>().length();
}

static inline void
SetAnyBoxedOrUnboxedArrayLength(JSContext* cx, JSObject* obj, size_t length)
{
    MOZ_ASSERT(length >= obj->as<ArrayObject>().length());
    obj->as<ArrayObject>().setLength(cx, length);
}

static inline bool
SetAnyBoxedOrUnboxedDenseElement(JSContext* cx, JSObject* obj, size_t index, const Value& value)
{
    obj->as<NativeObject>().setDenseElementWithType(cx, index, value);
    return true;
}

static inline bool
InitAnyBoxedOrUnboxedDenseElement(JSContext* cx, JSObject* obj, size_t index, const Value& value)
{
    obj->as<NativeObject>().initDenseElementWithType(cx, index, value);
    return true;
}

/////////////////////////////////////////////////////////////////////
// Template methods for NativeObject and UnboxedArrayObject accesses.
/////////////////////////////////////////////////////////////////////

static inline JSValueType
GetBoxedOrUnboxedType(JSObject* obj)
{
    return JSVAL_TYPE_MAGIC;
}

static inline bool
HasBoxedOrUnboxedDenseElements(JSObject* obj)
{
    return obj->isNative();
}

static inline size_t
GetBoxedOrUnboxedInitializedLength(JSObject* obj)
{
    return obj->as<NativeObject>().getDenseInitializedLength();
}

static inline DenseElementResult
SetBoxedOrUnboxedInitializedLength(JSContext* cx, JSObject* obj, size_t initlen)
{
    size_t oldInitlen = GetBoxedOrUnboxedInitializedLength(obj);
    obj->as<NativeObject>().setDenseInitializedLength(initlen);
    if (initlen < oldInitlen)
        obj->as<NativeObject>().shrinkElements(cx, initlen);
    return DenseElementResult::Success;
}

static inline size_t
GetBoxedOrUnboxedCapacity(JSObject* obj)
{
    return obj->as<NativeObject>().getDenseCapacity();
}

static inline Value
GetBoxedOrUnboxedDenseElement(JSObject* obj, size_t index)
{
    return obj->as<NativeObject>().getDenseElement(index);
}

static inline void
SetBoxedOrUnboxedDenseElementNoTypeChange(JSObject* obj, size_t index, const Value& value)
{
    obj->as<NativeObject>().setDenseElement(index, value);
}

static inline bool
SetBoxedOrUnboxedDenseElement(JSContext* cx, JSObject* obj, size_t index, const Value& value)
{
    obj->as<NativeObject>().setDenseElementWithType(cx, index, value);
    return true;
}

static inline DenseElementResult
EnsureBoxedOrUnboxedDenseElements(JSContext* cx, JSObject* obj, size_t count)
{
    if (!obj->as<ArrayObject>().ensureElements(cx, count))
        return DenseElementResult::Failure;
    return DenseElementResult::Success;
}

static inline DenseElementResult
SetOrExtendBoxedOrUnboxedDenseElements(JSContext* cx, JSObject* obj,
                                       uint32_t start, const Value* vp, uint32_t count,
                                       ShouldUpdateTypes updateTypes = ShouldUpdateTypes::Update)
{
    NativeObject* nobj = &obj->as<NativeObject>();

    if (nobj->denseElementsAreFrozen())
        return DenseElementResult::Incomplete;

    if (obj->is<ArrayObject>() &&
        !obj->as<ArrayObject>().lengthIsWritable() &&
        start + count >= obj->as<ArrayObject>().length())
    {
        return DenseElementResult::Incomplete;
    }

    DenseElementResult result = nobj->ensureDenseElements(cx, start, count);
    if (result != DenseElementResult::Success)
        return result;

    if (obj->is<ArrayObject>() && start + count >= obj->as<ArrayObject>().length())
        obj->as<ArrayObject>().setLengthInt32(start + count);

    if (updateTypes == ShouldUpdateTypes::DontUpdate && !nobj->shouldConvertDoubleElements()) {
        nobj->copyDenseElements(start, vp, count);
    } else {
        for (size_t i = 0; i < count; i++)
            nobj->setDenseElementWithType(cx, start + i, vp[i]);
    }

    return DenseElementResult::Success;
}

static inline DenseElementResult
MoveBoxedOrUnboxedDenseElements(JSContext* cx, JSObject* obj, uint32_t dstStart, uint32_t srcStart,
                                uint32_t length)
{
    MOZ_ASSERT(HasBoxedOrUnboxedDenseElements(obj));

    if (obj->as<NativeObject>().denseElementsAreFrozen())
        return DenseElementResult::Incomplete;

    if (!obj->as<NativeObject>().maybeCopyElementsForWrite(cx))
        return DenseElementResult::Failure;
    obj->as<NativeObject>().moveDenseElements(dstStart, srcStart, length);

    return DenseElementResult::Success;
}

static inline DenseElementResult
CopyBoxedOrUnboxedDenseElements(JSContext* cx, JSObject* dst, JSObject* src,
                                uint32_t dstStart, uint32_t srcStart, uint32_t length)
{
    MOZ_ASSERT(HasBoxedOrUnboxedDenseElements(src));
    MOZ_ASSERT(HasBoxedOrUnboxedDenseElements(dst));
    MOZ_ASSERT(GetBoxedOrUnboxedInitializedLength(dst) == dstStart);
    MOZ_ASSERT(GetBoxedOrUnboxedInitializedLength(src) >= srcStart + length);
    MOZ_ASSERT(GetBoxedOrUnboxedCapacity(dst) >= dstStart + length);

    SetBoxedOrUnboxedInitializedLength(cx, dst, dstStart + length);

    const Value* vp = src->as<NativeObject>().getDenseElements() + srcStart;
    dst->as<NativeObject>().initDenseElements(dstStart, vp, length);

    return DenseElementResult::Success;
}

/////////////////////////////////////////////////////////////////////
// Dispatch to specialized methods based on the type of an object.
/////////////////////////////////////////////////////////////////////

// Goop to fix MSVC. See DispatchTraceKindTyped in TraceKind.h.
// The clang-cl front end defines _MSC_VER, but still requires the explicit
// template declaration, so we must test for __clang__ here as well.
#if defined(_MSC_VER) && !defined(__clang__)
# define DEPENDENT_TEMPLATE_HINT
#else
# define DEPENDENT_TEMPLATE_HINT template
#endif

// Function to dispatch a method specialized to whatever boxed or unboxed dense
// elements which an input object has.
template <typename F>
DenseElementResult
CallBoxedOrUnboxedSpecialization(F f, JSObject* obj)
{
    if (!HasAnyBoxedOrUnboxedDenseElements(obj))
        return DenseElementResult::Incomplete;
    return f. DEPENDENT_TEMPLATE_HINT operator()<JSVAL_TYPE_MAGIC>();
}

// As above, except the specialization can reflect the unboxed type of two objects.
template <typename F>
DenseElementResult
CallBoxedOrUnboxedSpecialization(F f, JSObject* obj1, JSObject* obj2)
{
    if (!HasAnyBoxedOrUnboxedDenseElements(obj1) || !HasAnyBoxedOrUnboxedDenseElements(obj2))
        return DenseElementResult::Incomplete;

    return f. DEPENDENT_TEMPLATE_HINT operator()<JSVAL_TYPE_MAGIC, JSVAL_TYPE_MAGIC>();
}

#undef DEPENDENT_TEMPLATE_HINT

#define DefineBoxedOrUnboxedFunctor1(Signature, A)                      \
struct Signature ## Functor {                                           \
    A a;                                                                \
    explicit Signature ## Functor(A a)                                  \
      : a(a)                                                            \
    {}                                                                  \
    template <JSValueType Type>                                         \
    DenseElementResult operator()() {                                   \
        return Signature<Type>(a);                                      \
    }                                                                   \
}

#define DefineBoxedOrUnboxedFunctor3(Signature, A, B, C)                \
struct Signature ## Functor {                                           \
    A a; B b; C c;                                                      \
    Signature ## Functor(A a, B b, C c)                                 \
      : a(a), b(b), c(c)                                                \
    {}                                                                  \
    template <JSValueType Type>                                         \
    DenseElementResult operator()() {                                   \
        return Signature<Type>(a, b, c);                                \
    }                                                                   \
}

#define DefineBoxedOrUnboxedFunctor4(Signature, A, B, C, D)             \
struct Signature ## Functor {                                           \
    A a; B b; C c; D d;                                                 \
    Signature ## Functor(A a, B b, C c, D d)                            \
      : a(a), b(b), c(c), d(d)                                          \
    {}                                                                  \
    template <JSValueType Type>                                         \
    DenseElementResult operator()() {                                   \
        return Signature<Type>(a, b, c, d);                             \
    }                                                                   \
}

#define DefineBoxedOrUnboxedFunctorPair4(Signature, A, B, C, D)         \
struct Signature ## Functor {                                           \
    A a; B b; C c; D d;                                                 \
    Signature ## Functor(A a, B b, C c, D d)                            \
      : a(a), b(b), c(c), d(d)                                          \
    {}                                                                  \
    template <JSValueType TypeOne, JSValueType TypeTwo>                 \
    DenseElementResult operator()() {                                   \
        return Signature<TypeOne, TypeTwo>(a, b, c, d);                 \
    }                                                                   \
}

#define DefineBoxedOrUnboxedFunctor5(Signature, A, B, C, D, E)          \
struct Signature ## Functor {                                           \
    A a; B b; C c; D d; E e;                                            \
    Signature ## Functor(A a, B b, C c, D d, E e)                       \
      : a(a), b(b), c(c), d(d), e(e)                                    \
    {}                                                                  \
    template <JSValueType Type>                                         \
    DenseElementResult operator()() {                                   \
        return Signature<Type>(a, b, c, d, e);                          \
    }                                                                   \
}

#define DefineBoxedOrUnboxedFunctor6(Signature, A, B, C, D, E, F)       \
struct Signature ## Functor {                                           \
    A a; B b; C c; D d; E e; F f;                                       \
    Signature ## Functor(A a, B b, C c, D d, E e, F f)                  \
      : a(a), b(b), c(c), d(d), e(e), f(f)                              \
    {}                                                                  \
    template <JSValueType Type>                                         \
    DenseElementResult operator()() {                                   \
        return Signature<Type>(a, b, c, d, e, f);                       \
    }                                                                   \
}

#define DefineBoxedOrUnboxedFunctorPair6(Signature, A, B, C, D, E, F)   \
struct Signature ## Functor {                                           \
    A a; B b; C c; D d; E e; F f;                                       \
    Signature ## Functor(A a, B b, C c, D d, E e, F f)                  \
      : a(a), b(b), c(c), d(d), e(e), f(f)                              \
    {}                                                                  \
    template <JSValueType TypeOne, JSValueType TypeTwo>                 \
    DenseElementResult operator()() {                                   \
        return Signature<TypeOne, TypeTwo>(a, b, c, d, e, f);           \
    }                                                                   \
}

} // namespace js

#endif // vm_UnboxedObject_inl_h
