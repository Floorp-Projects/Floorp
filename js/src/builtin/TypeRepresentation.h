/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_TypeRepresentation_h
#define builtin_TypeRepresentation_h

/*

  Type Representations are the way that the compiler stores
  information about binary data types that the user defines.  They are
  always interned into a hashset in the compartment (typeReprs),
  meaning that you can compare two type representations for equality
  just using `==`.

  # Creation and canonicalization:

  Each kind of `TypeRepresentation` object includes a `New` method
  that will create a canonical instance of it. So, for example, you
  can do `ScalarTypeRepresentation::Create(Uint8)` to get the
  canonical representation of uint8. The object that is returned is
  designed to be immutable, and the API permits only read access.

  # Memory management:

  Each TypeRepresentations has a representative JSObject, called its
  owner object. When this object is finalized, the TypeRepresentation*
  will be freed (and removed from the interning table, see
  below). Therefore, if you reference a TypeRepresentation*, you must
  ensure this owner object is traced. In type objects, this is done by
  invoking TypeRepresentation::mark(); in binary data type descriptors,
  the owner object for the type is stored in `SLOT_TYPE_REPR`.

  The canonicalization table maintains *weak references* to the
  TypeRepresentation* pointers. That is, the table is not traced.
  Instead, whenever an object is created, it is paired with its owner
  object, and the finalizer of the owner object removes the pointer
  from the table and then frees the pointer.

  # Future plans:

  The owner object will eventually be exposed to self-hosted script
  and will offer methods for querying and manipulating the binary data
  it describes.

 */

#include "jsalloc.h"
#include "jscntxt.h"
#include "jspubtd.h"

#include "gc/Barrier.h"
#include "js/HashTable.h"

namespace js {

class TypeRepresentation;
class ScalarTypeRepresentation;
class ArrayTypeRepresentation;
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
    static HashNumber hashStruct(StructTypeRepresentation *key);
    static HashNumber hashArray(ArrayTypeRepresentation *key);

    static bool matchScalars(ScalarTypeRepresentation *key1,
                             ScalarTypeRepresentation *key2);
    static bool matchStructs(StructTypeRepresentation *key1,
                             StructTypeRepresentation *key2);
    static bool matchArrays(ArrayTypeRepresentation *key1,
                            ArrayTypeRepresentation *key2);
};

typedef js::HashSet<TypeRepresentation *,
                    TypeRepresentationHasher,
                    RuntimeAllocPolicy> TypeRepresentationSet;

class TypeRepresentation {
  public:
    enum Kind {
        Scalar,
        Struct,
        Array
    };

  protected:
    TypeRepresentation(Kind kind, size_t size, size_t align);

    size_t size_;
    size_t alignment_;
    Kind kind_;

    JSObject *addToTableOrFree(JSContext *cx, TypeRepresentationSet::AddPtr &p);

  private:
    static Class class_;
    static void obj_trace(JSTracer *trace, JSObject *object);
    static void obj_finalize(js::FreeOp *fop, JSObject *object);

    js::HeapPtrObject ownerObject_;
    void traceFields(JSTracer *tracer);

  public:
    size_t size() const { return size_; }
    size_t alignment() const { return alignment_; }
    Kind kind() const { return kind_; }
    JSObject *ownerObject() const { return ownerObject_.get(); }

    // Appends a stringified form of this type representation onto
    // buffer, for use in error messages and the like.
    bool appendString(JSContext *cx, StringBuffer &buffer);

    static bool isTypeRepresentationOwnerObject(JSObject *obj);
    static TypeRepresentation *fromOwnerObject(JSObject *obj);

    bool isScalar() const {
        return kind() == Scalar;
    }

    ScalarTypeRepresentation *asScalar() {
        JS_ASSERT(isScalar());
        return (ScalarTypeRepresentation*) this;
    }

    bool isArray() const {
        return kind() == Array;
    }

    ArrayTypeRepresentation *asArray() {
        JS_ASSERT(isArray());
        return (ArrayTypeRepresentation*) this;
    }

    bool isStruct() const {
        return kind() == Struct;
    }

    StructTypeRepresentation *asStruct() {
        JS_ASSERT(isStruct());
        return (StructTypeRepresentation*) this;
    }

    void mark(JSTracer *tracer);
};

class ScalarTypeRepresentation : public TypeRepresentation {
  public:
    // Must match order of JS_FOR_EACH_SCALAR_TYPE_REPR below
    enum Type {
        TYPE_INT8 = 0,
        TYPE_UINT8,
        TYPE_INT16,
        TYPE_UINT16,
        TYPE_INT32,
        TYPE_UINT32,
        TYPE_FLOAT32,
        TYPE_FLOAT64,

        /*
         * Special type that's a uint8_t, but assignments are clamped to 0 .. 255.
         * Treat the raw data type as a uint8_t.
         */
        TYPE_UINT8_CLAMPED,
    };
    static const int32_t TYPE_MAX = TYPE_UINT8_CLAMPED + 1;

  private:
    // so TypeRepresentation can call appendStringScalar() etc
    friend class TypeRepresentation;

    Type type_;

    explicit ScalarTypeRepresentation(Type type);

    // See TypeRepresentation::appendString()
    bool appendStringScalar(JSContext *cx, StringBuffer &buffer);

  public:
    Type type() const {
        return type_;
    }

    bool convertValue(JSContext *cx, HandleValue value, MutableHandleValue vp);

    static const char *typeName(Type type);
    static JSObject *Create(JSContext *cx, Type type);
};

// Must be in same order as the enum:
#define JS_FOR_EACH_SCALAR_TYPE_REPR(macro_)                                    \
    macro_(ScalarTypeRepresentation::TYPE_INT8,    int8_t,   int8)              \
    macro_(ScalarTypeRepresentation::TYPE_UINT8,   uint8_t,  uint8)             \
    macro_(ScalarTypeRepresentation::TYPE_INT16,   int16_t,  int16)             \
    macro_(ScalarTypeRepresentation::TYPE_UINT16,  uint16_t, uint16)            \
    macro_(ScalarTypeRepresentation::TYPE_INT32,   int32_t,  int32)             \
    macro_(ScalarTypeRepresentation::TYPE_UINT32,  uint32_t, uint32)            \
    macro_(ScalarTypeRepresentation::TYPE_FLOAT32, float,    float32)           \
    macro_(ScalarTypeRepresentation::TYPE_FLOAT64, double,   float64)           \
    macro_(ScalarTypeRepresentation::TYPE_UINT8_CLAMPED, uint8_t, uint8Clamped)

class ArrayTypeRepresentation : public TypeRepresentation {
  private:
    // so TypeRepresentation can call appendStringArray() etc
    friend class TypeRepresentation;

    TypeRepresentation *element_;
    size_t length_;

    ArrayTypeRepresentation(TypeRepresentation *element,
                            size_t length);

    // See TypeRepresentation::traceFields()
    void traceArrayFields(JSTracer *trace);

    // See TypeRepresentation::appendString()
    bool appendStringArray(JSContext *cx, StringBuffer &buffer);

  public:
    TypeRepresentation *element() {
        return element_;
    }

    size_t length() {
        return length_;
    }

    static JSObject *Create(JSContext *cx,
                            TypeRepresentation *elementTypeRepr,
                            size_t length);
};

struct StructField {
    size_t index;
    HeapId id;
    TypeRepresentation *typeRepr;
    size_t offset;

    explicit StructField(size_t index,
                         jsid &id,
                         TypeRepresentation *typeRepr,
                         size_t offset)
      : index(index),
        id(id),
        typeRepr(typeRepr),
        offset(offset)
    {}
};

class StructTypeRepresentation : public TypeRepresentation {
  private:
    // so TypeRepresentation can call appendStringStruct() etc
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
              AutoIdVector &ids,
              AutoObjectVector &typeReprOwners);

    // See TypeRepresentation::traceFields()
    void traceStructFields(JSTracer *trace);

    // See TypeRepresentation::appendString()
    bool appendStringStruct(JSContext *cx, StringBuffer &buffer);

  public:
    size_t fieldCount() const {
        return fieldCount_;
    }

    const StructField &field(size_t i) const {
        JS_ASSERT(i < fieldCount());
        return fields()[i];
    }

    const StructField *fieldNamed(HandleId id) const;

    static JSObject *Create(JSContext *cx,
                            AutoIdVector &ids,
                            AutoObjectVector &typeReprOwners);
};

} // namespace js

#endif
