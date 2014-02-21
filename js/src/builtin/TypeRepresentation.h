/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_TypeRepresentation_h
#define builtin_TypeRepresentation_h

/*
 *  Type Representations are canonical versions of user-defined type
 *  descriptors. Using the typed objects API, users are free to define
 *  new type descriptors representing C-like type definitions. Each such
 *  type descriptor has a distinct prototype and identity, even if it
 *  describes the same logical type as another type
 *  descriptor. Generally speaking, though, the only thing the compiler
 *  cares about is the binary data layout implied by a type descriptor.
 *
 *  Therefore, we link each such type descriptor to one canonical *type
 *  representation*. The type representation is itself an object, but
 *  this object is never exposed to user code (it is used by self-hosted
 *  code). Type representations are interned into a hashset in the
 *  compartment (typeReprs), meaning that you can compare two type
 *  representations for equality just using `==`. If they are equal,
 *  it means that they represent the same binary layout.
 *
 *  # Creation and canonicalization:
 *
 *  Each kind of `TypeRepresentation` object includes a `New` method
 *  that will create a canonical instance of it. So, for example, you
 *  can do `ScalarTypeRepresentation::Create(Uint8)` to get the
 *  canonical representation of uint8. The object that is returned is
 *  designed to be immutable, and the API permits only read access.
 *
 *  # Integration with TI:
 *
 *  Each TypeRepresentation has an associated Type Object. This Type
 *  Object is used as the type object for all type descriptors with this
 *  representation. The type object has an associated addendum linking
 *  to the type representation, thus allowing the jit to deduce
 *  information about type descriptors that appear in expressions.
 *
 *  # Memory management:
 *
 *  Each TypeRepresentations has an associated JSObject, called its
 *  owned object. When this object is finalized, the TypeRepresentation*
 *  will be freed (and removed from the interning table, see
 *  below). Therefore, if you reference a TypeRepresentation*, you must
 *  ensure this owner object is traced. In type objects, this is done by
 *  invoking TypeRepresentation::mark(); in binary data type
 *  descriptors, the owner object for the type is stored in
 *  `SLOT_TYPE_REPR`.
 *
 *  The canonicalization table maintains *weak references* to the
 *  TypeRepresentation* pointers. That is, the table is not traced.
 *  Instead, whenever an object is created, it is paired with its owner
 *  object, and the finalizer of the owner object removes the pointer
 *  from the table and then frees the pointer.
 *
 *  # Opacity
 *
 *  A type representation is considered "opaque" if it contains
 *  references (strings, objects, any). In those cases we have to be
 *  more limited with respect to aliasing etc to preserve portability
 *  across engines (for example, we don't want to expose sizeof(Any))
 *  and memory safety.
 */

#include "jsalloc.h"
#include "jscntxt.h"
#include "jspubtd.h"

#include "builtin/TypedObject.h"
#include "builtin/TypedObjectConstants.h"
#include "gc/Barrier.h"
#include "js/HashTable.h"

namespace js {

class TypeRepresentation;
class SizedTypeRepresentation;
class ScalarTypeRepresentation;
class ReferenceTypeRepresentation;
class X4TypeRepresentation;
class SizedArrayTypeRepresentation;
class UnsizedArrayTypeRepresentation;
class StructTypeRepresentation;

struct Class;
class StringBuffer;

struct TypeRepresentationHasher
{
    typedef TypeRepresentation *Lookup;
    static HashNumber hash(TypeRepresentation *key);
    static bool match(TypeRepresentation *key1, TypeRepresentation *key2);

  private:
    static HashNumber hashScalar(ScalarTypeRepresentation *key);
    static HashNumber hashReference(ReferenceTypeRepresentation *key);
    static HashNumber hashX4(X4TypeRepresentation *key);
    static HashNumber hashStruct(StructTypeRepresentation *key);
    static HashNumber hashUnsizedArray(UnsizedArrayTypeRepresentation *key);
    static HashNumber hashSizedArray(SizedArrayTypeRepresentation *key);

    static bool matchScalars(ScalarTypeRepresentation *key1,
                             ScalarTypeRepresentation *key2);
    static bool matchReferences(ReferenceTypeRepresentation *key1,
                                ReferenceTypeRepresentation *key2);
    static bool matchX4s(X4TypeRepresentation *key1,
                         X4TypeRepresentation *key2);
    static bool matchStructs(StructTypeRepresentation *key1,
                             StructTypeRepresentation *key2);
    static bool matchSizedArrays(SizedArrayTypeRepresentation *key1,
                                 SizedArrayTypeRepresentation *key2);
    static bool matchUnsizedArrays(UnsizedArrayTypeRepresentation *key1,
                                   UnsizedArrayTypeRepresentation *key2);
};

typedef js::HashSet<TypeRepresentation *,
                    TypeRepresentationHasher,
                    RuntimeAllocPolicy> TypeRepresentationHash;

class TypeRepresentationHelper;

class TypeRepresentation {
  protected:
    TypeRepresentation(TypeDescr::Kind kind, size_t alignment, bool opaque);

    // in order to call addToTableOrFree()
    friend class TypeRepresentationHelper;

    TypeDescr::Kind kind_;
    bool opaque_;
    size_t alignment_;

    JSObject *addToTableOrFree(JSContext *cx, TypeRepresentationHash::AddPtr &p);

  private:
    static const Class class_;
    static void obj_trace(JSTracer *trace, JSObject *object);
    static void obj_finalize(js::FreeOp *fop, JSObject *object);

    HeapPtrObject ownerObject_;
    void traceFields(JSTracer *tracer);

  public:
    TypeDescr::Kind kind() const { return kind_; }
    bool opaque() const { return opaque_; }
    bool transparent() const { return !opaque_; }
    JSObject *ownerObject() const { return ownerObject_.get(); }
    size_t alignment() const { return alignment_; }

    static bool isOwnerObject(JSObject &obj);
    static TypeRepresentation *fromOwnerObject(JSObject &obj);

    bool isSized() const {
        return TypeDescr::isSized(kind());
    }

    inline SizedTypeRepresentation *asSized();

    bool isScalar() const {
        return kind() == TypeDescr::Scalar;
    }

    inline ScalarTypeRepresentation *asScalar();

    bool isReference() const {
        return kind() == TypeDescr::Reference;
    }

    inline ReferenceTypeRepresentation *asReference();

    bool isX4() const {
        return kind() == TypeDescr::X4;
    }

    inline X4TypeRepresentation *asX4();

    bool isSizedArray() const {
        return kind() == TypeDescr::SizedArray;
    }

    inline SizedArrayTypeRepresentation *asSizedArray();

    bool isUnsizedArray() const {
        return kind() == TypeDescr::UnsizedArray;
    }

    inline UnsizedArrayTypeRepresentation *asUnsizedArray();

    bool isAnyArray() const {
        return isSizedArray() || isUnsizedArray();
    }

    bool isStruct() const {
        return kind() == TypeDescr::Struct;
    }

    inline StructTypeRepresentation *asStruct();

    void mark(JSTracer *tracer);
};

class SizedTypeRepresentation : public TypeRepresentation {
  protected:
    SizedTypeRepresentation(TypeDescr::Kind kind, bool opaque, size_t size, size_t align);

    size_t size_;

  public:
    size_t size() const { return size_; }

    // Initializes memory that contains `count` instances of this type.
    // `count` must be at least 1.
    void initInstance(const JSRuntime *rt, uint8_t *mem, size_t count);

    // Traces memory that contains `count` instances of this type.
    void traceInstance(JSTracer *trace, uint8_t *mem, size_t count);
};

class ScalarTypeRepresentation : public SizedTypeRepresentation {
  private:
    // in order to call constructor
    friend class TypeRepresentationHelper;

    const ScalarTypeDescr::Type type_;

    explicit ScalarTypeRepresentation(ScalarTypeDescr::Type type);

  public:
    ScalarTypeDescr::Type type() const {
        return type_;
    }

    const char *typeName() const {
        return ScalarTypeDescr::typeName(type());
    }

    static JSObject *Create(JSContext *cx, ScalarTypeDescr::Type type);
};

class ReferenceTypeRepresentation : public SizedTypeRepresentation {
  private:
    ReferenceTypeDescr::Type type_;

    explicit ReferenceTypeRepresentation(ReferenceTypeDescr::Type type);

  public:
    ReferenceTypeDescr::Type type() const {
        return type_;
    }

    const char *typeName() const {
        return ReferenceTypeDescr::typeName(type());
    }

    static JSObject *Create(JSContext *cx, ReferenceTypeDescr::Type type);
};

class X4TypeRepresentation : public SizedTypeRepresentation {
  private:
    // in order to call constructor
    friend class TypeRepresentationHelper;

    const X4TypeDescr::Type type_;

    explicit X4TypeRepresentation(X4TypeDescr::Type type);

  public:
    X4TypeDescr::Type type() const {
        return type_;
    }

    static JSObject *Create(JSContext *cx, X4TypeDescr::Type type);
};

class UnsizedArrayTypeRepresentation : public TypeRepresentation {
  private:
    // so TypeRepresentation can call tracing routines
    friend class TypeRepresentation;

    SizedTypeRepresentation *element_;

    UnsizedArrayTypeRepresentation(SizedTypeRepresentation *element);

    // See TypeRepresentation::traceFields()
    void traceUnsizedArrayFields(JSTracer *trace);

  public:
    SizedTypeRepresentation *element() {
        return element_;
    }

    static JSObject *Create(JSContext *cx,
                            SizedTypeRepresentation *elementTypeRepr);
};

class SizedArrayTypeRepresentation : public SizedTypeRepresentation {
  private:
    // so TypeRepresentation can call traceSizedArrayFields()
    friend class TypeRepresentation;

    SizedTypeRepresentation *element_;
    size_t length_;

    SizedArrayTypeRepresentation(SizedTypeRepresentation *element,
                                 size_t length);

    // See TypeRepresentation::traceFields()
    void traceSizedArrayFields(JSTracer *trace);

  public:
    SizedTypeRepresentation *element() {
        return element_;
    }

    size_t length() {
        return length_;
    }

    static JSObject *Create(JSContext *cx,
                            SizedTypeRepresentation *elementTypeRepr,
                            size_t length);
};

struct StructField {
    size_t index;
    HeapPtrPropertyName propertyName;
    SizedTypeRepresentation *typeRepr;
    size_t offset;

    explicit StructField(size_t index,
                         PropertyName *propertyName,
                         SizedTypeRepresentation *typeRepr,
                         size_t offset);
};

class StructTypeRepresentation : public SizedTypeRepresentation {
  private:
    // so TypeRepresentation can call traceStructFields() etc
    friend class TypeRepresentation;

    size_t fieldCount_;

    // StructTypeRepresentations are allocated with extra space to
    // store the contents of the fields array.
    StructField* fields() {
        return (StructField*) (this+1);
    }
    const StructField* fields() const {
        return (StructField*) (this+1);
    }

    StructTypeRepresentation();
    bool init(JSContext *cx,
              AutoPropertyNameVector &names,
              AutoObjectVector &typeReprOwners);

    // See TypeRepresentation::traceFields()
    void traceStructFields(JSTracer *trace);

  public:
    size_t fieldCount() const {
        return fieldCount_;
    }

    const StructField &field(size_t i) const {
        JS_ASSERT(i < fieldCount());
        return fields()[i];
    }

    const StructField *fieldNamed(jsid id) const;

    // Creates a struct type from two parallel arrays:
    // - `names`: the names of the struct type fields.
    // - `typeReprOwners`: the types of each field, which are assumed
    //   to be the owner objects for sized type representations.
    static JSObject *Create(JSContext *cx,
                            AutoPropertyNameVector &names,
                            AutoObjectVector &typeReprOwners);
};

// Definitions of the casting methods. These are pulled out of the
// main class definition because both the super- and subtypes must be
// defined for C++ to permit the static_cast.

SizedTypeRepresentation *
TypeRepresentation::asSized() {
    JS_ASSERT(isSized());
    return static_cast<SizedTypeRepresentation*>(this);
}

ScalarTypeRepresentation *
TypeRepresentation::asScalar() {
    JS_ASSERT(isScalar());
    return static_cast<ScalarTypeRepresentation*>(this);
}

ReferenceTypeRepresentation *
TypeRepresentation::asReference() {
    JS_ASSERT(isReference());
    return static_cast<ReferenceTypeRepresentation*>(this);
}

X4TypeRepresentation *
TypeRepresentation::asX4() {
    JS_ASSERT(isX4());
    return static_cast<X4TypeRepresentation*>(this);
}

SizedArrayTypeRepresentation *
TypeRepresentation::asSizedArray() {
    JS_ASSERT(isSizedArray());
    return static_cast<SizedArrayTypeRepresentation*>(this);
}

UnsizedArrayTypeRepresentation *
TypeRepresentation::asUnsizedArray() {
    JS_ASSERT(isUnsizedArray());
    return static_cast<UnsizedArrayTypeRepresentation*>(this);
}

StructTypeRepresentation *
TypeRepresentation::asStruct() {
    JS_ASSERT(isStruct());
    return static_cast<StructTypeRepresentation*>(this);
}

} // namespace js

#endif
