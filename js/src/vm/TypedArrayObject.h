/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_TypedArrayObject_h
#define vm_TypedArrayObject_h

#include "mozilla/Attributes.h"
#include "mozilla/TextUtils.h"

#include "gc/Barrier.h"
#include "js/Class.h"
#include "vm/ArrayBufferObject.h"
#include "vm/ArrayBufferViewObject.h"
#include "vm/JSObject.h"
#include "vm/SharedArrayObject.h"

#define JS_FOR_EACH_TYPED_ARRAY(macro) \
    macro(int8_t, Int8) \
    macro(uint8_t, Uint8) \
    macro(int16_t, Int16) \
    macro(uint16_t, Uint16) \
    macro(int32_t, Int32) \
    macro(uint32_t, Uint32) \
    macro(float, Float32) \
    macro(double, Float64) \
    macro(uint8_clamped, Uint8Clamped)

namespace js {

/*
 * TypedArrayObject
 *
 * The non-templated base class for the specific typed implementations.
 * This class holds all the member variables that are used by
 * the subclasses.
 */

class TypedArrayObject : public ArrayBufferViewObject
{
  public:
    static int lengthOffset();
    static int dataOffset();

    static_assert(js::detail::TypedArrayLengthSlot == LENGTH_SLOT,
                  "bad inlined constant in jsfriendapi.h");

    static bool sameBuffer(Handle<TypedArrayObject*> a, Handle<TypedArrayObject*> b) {
        // Inline buffers.
        if (!a->hasBuffer() || !b->hasBuffer()) {
            return a.get() == b.get();
        }

        // Shared buffers.
        if (a->isSharedMemory() && b->isSharedMemory()) {
            return (a->bufferObject()->as<SharedArrayBufferObject>().globalID() ==
                    b->bufferObject()->as<SharedArrayBufferObject>().globalID());
        }

        return a->bufferObject() == b->bufferObject();
    }

    static const Class classes[Scalar::MaxTypedArrayViewType];
    static const Class protoClasses[Scalar::MaxTypedArrayViewType];
    static const Class sharedTypedArrayPrototypeClass;

    static const Class* classForType(Scalar::Type type) {
        MOZ_ASSERT(type < Scalar::MaxTypedArrayViewType);
        return &classes[type];
    }

    static const Class* protoClassForType(Scalar::Type type) {
        MOZ_ASSERT(type < Scalar::MaxTypedArrayViewType);
        return &protoClasses[type];
    }

    static const size_t FIXED_DATA_START = DATA_SLOT + 1;

    // For typed arrays which can store their data inline, the array buffer
    // object is created lazily.
    static const uint32_t INLINE_BUFFER_LIMIT =
        (NativeObject::MAX_FIXED_SLOTS - FIXED_DATA_START) * sizeof(Value);

    static inline gc::AllocKind AllocKindForLazyBuffer(size_t nbytes);

    inline Scalar::Type type() const;
    inline size_t bytesPerElement() const;

    static Value byteOffsetValue(const TypedArrayObject* tarr) {
        Value v = tarr->getFixedSlot(BYTEOFFSET_SLOT);
        MOZ_ASSERT(v.toInt32() >= 0);
        return v;
    }
    static Value byteLengthValue(const TypedArrayObject* tarr) {
        return Int32Value(tarr->getFixedSlot(LENGTH_SLOT).toInt32() * tarr->bytesPerElement());
    }
    static Value lengthValue(const TypedArrayObject* tarr) {
        return tarr->getFixedSlot(LENGTH_SLOT);
    }

    static bool
    ensureHasBuffer(JSContext* cx, Handle<TypedArrayObject*> tarray);

    uint32_t byteOffset() const {
        return byteOffsetValue(this).toInt32();
    }
    uint32_t byteLength() const {
        return byteLengthValue(this).toInt32();
    }
    uint32_t length() const {
        return lengthValue(this).toInt32();
    }

    bool hasInlineElements() const;
    void setInlineElements();
    uint8_t* elementsRaw() const {
        return *(uint8_t **)((((char *)this) + js::TypedArrayObject::dataOffset()));
    }
    uint8_t* elements() const {
        assertZeroLengthArrayData();
        return elementsRaw();
    }

#ifdef DEBUG
    void assertZeroLengthArrayData() const;
#else
    void assertZeroLengthArrayData() const {};
#endif

    Value getElement(uint32_t index);
    static void setElement(TypedArrayObject& obj, uint32_t index, double d);

    /*
     * Copy all elements from this typed array to vp. vp must point to rooted
     * memory.
     */
    void getElements(Value* vp);

    static bool
    GetTemplateObjectForNative(JSContext* cx, Native native, uint32_t len,
                               MutableHandleObject res);

    /*
     * Byte length above which created typed arrays will have singleton types
     * regardless of the context in which they are created. This only applies to
     * typed arrays created with an existing ArrayBuffer.
     */
    static const uint32_t SINGLETON_BYTE_LENGTH = 1024 * 1024 * 10;

    static bool isOriginalLengthGetter(Native native);

    static void finalize(FreeOp* fop, JSObject* obj);
    static size_t objectMoved(JSObject* obj, JSObject* old);

    /* Initialization bits */

    template<Value ValueGetter(const TypedArrayObject* tarr)>
    static bool
    GetterImpl(JSContext* cx, const CallArgs& args)
    {
        MOZ_ASSERT(is(args.thisv()));
        args.rval().set(ValueGetter(&args.thisv().toObject().as<TypedArrayObject>()));
        return true;
    }

    // ValueGetter is a function that takes an unwrapped typed array object and
    // returns a Value. Given such a function, Getter<> is a native that
    // retrieves a given Value, probably from a slot on the object.
    template<Value ValueGetter(const TypedArrayObject* tarr)>
    static bool
    Getter(JSContext* cx, unsigned argc, Value* vp)
    {
        CallArgs args = CallArgsFromVp(argc, vp);
        return CallNonGenericMethod<is, GetterImpl<ValueGetter>>(cx, args);
    }

    static const JSFunctionSpec protoFunctions[];
    static const JSPropertySpec protoAccessors[];
    static const JSFunctionSpec staticFunctions[];
    static const JSPropertySpec staticProperties[];

    /* Accessors and functions */

    static bool is(HandleValue v);

    static bool set(JSContext* cx, unsigned argc, Value* vp);

  private:
    static bool set_impl(JSContext* cx, const CallArgs& args);
};

MOZ_MUST_USE bool TypedArray_bufferGetter(JSContext* cx, unsigned argc, Value* vp);

extern TypedArrayObject*
TypedArrayCreateWithTemplate(JSContext* cx, HandleObject templateObj, int32_t len);

inline bool
IsTypedArrayClass(const Class* clasp)
{
    return &TypedArrayObject::classes[0] <= clasp &&
           clasp < &TypedArrayObject::classes[Scalar::MaxTypedArrayViewType];
}

inline Scalar::Type
GetTypedArrayClassType(const Class* clasp)
{
    MOZ_ASSERT(IsTypedArrayClass(clasp));
    return static_cast<Scalar::Type>(clasp - &TypedArrayObject::classes[0]);
}

bool
IsTypedArrayConstructor(HandleValue v, uint32_t type);

// In WebIDL terminology, a BufferSource is either an ArrayBuffer or a typed
// array view. In either case, extract the dataPointer/byteLength.
bool
IsBufferSource(JSObject* object, SharedMem<uint8_t*>* dataPointer, size_t* byteLength);

inline Scalar::Type
TypedArrayObject::type() const
{
    return GetTypedArrayClassType(getClass());
}

inline size_t
TypedArrayObject::bytesPerElement() const
{
    return Scalar::byteSize(type());
}

// Return value is whether the string is some integer. If the string is an
// integer which is not representable as a uint64_t, the return value is true
// and the resulting index is UINT64_MAX.
template <typename CharT>
bool
StringIsTypedArrayIndex(const CharT* s, size_t length, uint64_t* indexp);

inline bool
IsTypedArrayIndex(jsid id, uint64_t* indexp)
{
    if (JSID_IS_INT(id)) {
        int32_t i = JSID_TO_INT(id);
        MOZ_ASSERT(i >= 0);
        *indexp = static_cast<uint64_t>(i);
        return true;
    }

    if (MOZ_UNLIKELY(!JSID_IS_STRING(id))) {
        return false;
    }

    JS::AutoCheckCannotGC nogc;
    JSAtom* atom = JSID_TO_ATOM(id);
    size_t length = atom->length();

    if (atom->hasLatin1Chars()) {
        const Latin1Char* s = atom->latin1Chars(nogc);
        if (!mozilla::IsAsciiDigit(*s) && *s != '-') {
            return false;
        }
        return StringIsTypedArrayIndex(s, length, indexp);
    }

    const char16_t* s = atom->twoByteChars(nogc);
    if (!mozilla::IsAsciiDigit(*s) && *s != '-') {
        return false;
    }
    return StringIsTypedArrayIndex(s, length, indexp);
}

/*
 * Implements [[DefineOwnProperty]] for TypedArrays when the property
 * key is a TypedArray index.
 */
bool
DefineTypedArrayElement(JSContext* cx, HandleObject arr, uint64_t index,
                        Handle<PropertyDescriptor> desc, ObjectOpResult& result);

static inline unsigned
TypedArrayShift(Scalar::Type viewType)
{
    switch (viewType) {
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Uint8Clamped:
        return 0;
      case Scalar::Int16:
      case Scalar::Uint16:
        return 1;
      case Scalar::Int32:
      case Scalar::Uint32:
      case Scalar::Float32:
        return 2;
      case Scalar::Int64:
      case Scalar::Float64:
        return 3;
      default:;
    }
    MOZ_CRASH("Unexpected array type");
}

static inline unsigned
TypedArrayElemSize(Scalar::Type viewType)
{
    return 1u << TypedArrayShift(viewType);
}

// Assign
//
//   target[targetOffset] = unsafeSrcCrossCompartment[0]
//   ...
//   target[targetOffset + unsafeSrcCrossCompartment.length - 1] =
//       unsafeSrcCrossCompartment[unsafeSrcCrossCompartment.length - 1]
//
// where the source element range doesn't overlap the target element range in
// memory.
extern void
SetDisjointTypedElements(TypedArrayObject* target, uint32_t targetOffset,
                         TypedArrayObject* unsafeSrcCrossCompartment);

} // namespace js

template <>
inline bool
JSObject::is<js::TypedArrayObject>() const
{
    return js::IsTypedArrayClass(getClass());
}

#endif /* vm_TypedArrayObject_h */
