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

  # Opacity

  A type representation is considered "opaque" if it contains
  references (strings, objects, any). In those cases we have to be
  more limited with respect to aliasing etc to preserve portability
  across engines (for example, we don't want to expose sizeof(Any))
  and memory safety.

  # Future plans:

  The owner object will eventually be exposed to self-hosted script
  and will offer methods for querying and manipulating the binary data
  it describes.

 */

#include "jsalloc.h"
#include "jscntxt.h"
#include "jspubtd.h"

#include "builtin/TypedObjectConstants.h"
#include "gc/Barrier.h"
#include "js/HashTable.h"

namespace js {

class TypeRepresentation;
class ScalarTypeRepresentation;
class ReferenceTypeRepresentation;
class X4TypeRepresentation;
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
    static HashNumber hashReference(ReferenceTypeRepresentation *key);
    static HashNumber hashX4(X4TypeRepresentation *key);
    static HashNumber hashStruct(StructTypeRepresentation *key);
    static HashNumber hashArray(ArrayTypeRepresentation *key);

    static bool matchScalars(ScalarTypeRepresentation *key1,
                             ScalarTypeRepresentation *key2);
    static bool matchReferences(ReferenceTypeRepresentation *key1,
                                ReferenceTypeRepresentation *key2);
    static bool matchX4s(X4TypeRepresentation *key1,
                         X4TypeRepresentation *key2);
    static bool matchStructs(StructTypeRepresentation *key1,
                             StructTypeRepresentation *key2);
    static bool matchArrays(ArrayTypeRepresentation *key1,
                            ArrayTypeRepresentation *key2);
};

typedef js::HashSet<TypeRepresentation *,
                    TypeRepresentationHasher,
                    RuntimeAllocPolicy> TypeRepresentationHash;

class TypeRepresentationHelper;

class TypeRepresentation {
  public:
    enum Kind {
        Scalar = JS_TYPEREPR_SCALAR_KIND,
        Reference = JS_TYPEREPR_REFERENCE_KIND,
        X4 = JS_TYPEREPR_X4_KIND,
        Struct = JS_TYPEREPR_STRUCT_KIND,
        Array = JS_TYPEREPR_ARRAY_KIND
    };

  protected:
    TypeRepresentation(Kind kind, size_t size, size_t align, bool opaque);

    // in order to call addToTableOrFree()
    friend class TypeRepresentationHelper;

    size_t size_;
    size_t alignment_;
    Kind kind_;
    bool opaque_;

    JSObject *addToTableOrFree(JSContext *cx, TypeRepresentationHash::AddPtr &p);

  private:
    static const Class class_;
    static void obj_trace(JSTracer *trace, JSObject *object);
    static void obj_finalize(js::FreeOp *fop, JSObject *object);

    HeapPtrObject ownerObject_;
    void traceFields(JSTracer *tracer);

  public:
    size_t size() const { return size_; }
    size_t alignment() const { return alignment_; }
    Kind kind() const { return kind_; }
    bool opaque() const { return opaque_; }
    bool transparent() const { return !opaque_; }
    JSObject *ownerObject() const { return ownerObject_.get(); }

    // Appends a stringified form of this type representation onto
    // buffer, for use in error messages and the like.
    bool appendString(JSContext *cx, StringBuffer &buffer);

    // Initializes memory that contains an instance of this type
    // with appropriate default values (typically 0).
    void initInstance(const JSRuntime *rt, uint8_t *mem);

    // Traces memory that contains an instance of this type.
    void traceInstance(JSTracer *trace, uint8_t *mem);

    static bool isOwnerObject(JSObject &obj);
    static TypeRepresentation *fromOwnerObject(JSObject &obj);

    bool isScalar() const {
        return kind() == Scalar;
    }

    ScalarTypeRepresentation *asScalar() {
        JS_ASSERT(isScalar());
        return (ScalarTypeRepresentation*) this;
    }

    bool isReference() const {
        return kind() == Reference;
    }

    ReferenceTypeRepresentation *asReference() {
        JS_ASSERT(isReference());
        return (ReferenceTypeRepresentation*) this;
    }

    bool isX4() const {
        return kind() == X4;
    }

    X4TypeRepresentation *asX4() {
        JS_ASSERT(isX4());
        return (X4TypeRepresentation*) this;
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
        TYPE_INT8 = JS_SCALARTYPEREPR_INT8,
        TYPE_UINT8 = JS_SCALARTYPEREPR_UINT8,
        TYPE_INT16 = JS_SCALARTYPEREPR_INT16,
        TYPE_UINT16 = JS_SCALARTYPEREPR_UINT16,
        TYPE_INT32 = JS_SCALARTYPEREPR_INT32,
        TYPE_UINT32 = JS_SCALARTYPEREPR_UINT32,
        TYPE_FLOAT32 = JS_SCALARTYPEREPR_FLOAT32,
        TYPE_FLOAT64 = JS_SCALARTYPEREPR_FLOAT64,

        /*
         * Special type that's a uint8_t, but assignments are clamped to 0 .. 255.
         * Treat the raw data type as a uint8_t.
         */
        TYPE_UINT8_CLAMPED = JS_SCALARTYPEREPR_UINT8_CLAMPED,
    };
    static const int32_t TYPE_MAX = TYPE_UINT8_CLAMPED + 1;

  private:
    // so TypeRepresentation can call appendStringScalar() etc
    friend class TypeRepresentation;

    // in order to call constructor
    friend class TypeRepresentationHelper;

    const Type type_;

    explicit ScalarTypeRepresentation(Type type);

    // See TypeRepresentation::appendString()
    bool appendStringScalar(JSContext *cx, StringBuffer &buffer);

  public:
    Type type() const {
        return type_;
    }

    const char *typeName() const {
        return typeName(type());
    }

    static const char *typeName(Type type);
    static JSObject *Create(JSContext *cx, Type type);
};

// Enumerates the cases of ScalarTypeRepresentation::Type which have
// unique C representation. In particular, omits Uint8Clamped since it
// is just a Uint8.
#define JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(macro_)                     \
    macro_(ScalarTypeRepresentation::TYPE_INT8,    int8_t,   int8)            \
    macro_(ScalarTypeRepresentation::TYPE_UINT8,   uint8_t,  uint8)           \
    macro_(ScalarTypeRepresentation::TYPE_INT16,   int16_t,  int16)           \
    macro_(ScalarTypeRepresentation::TYPE_UINT16,  uint16_t, uint16)          \
    macro_(ScalarTypeRepresentation::TYPE_INT32,   int32_t,  int32)           \
    macro_(ScalarTypeRepresentation::TYPE_UINT32,  uint32_t, uint32)          \
    macro_(ScalarTypeRepresentation::TYPE_FLOAT32, float,    float32)         \
    macro_(ScalarTypeRepresentation::TYPE_FLOAT64, double,   float64)

// Must be in same order as the enum ScalarTypeRepresentation::Type:
#define JS_FOR_EACH_SCALAR_TYPE_REPR(macro_)                                    \
    JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(macro_)                           \
    macro_(ScalarTypeRepresentation::TYPE_UINT8_CLAMPED, uint8_t, uint8Clamped)

class ReferenceTypeRepresentation : public TypeRepresentation {
  public:
    // Must match order of JS_FOR_EACH_REFERENCE_TYPE_REPR below
    enum Type {
        TYPE_ANY = JS_REFERENCETYPEREPR_ANY,
        TYPE_OBJECT = JS_REFERENCETYPEREPR_OBJECT,
        TYPE_STRING = JS_REFERENCETYPEREPR_STRING,
    };
    static const int32_t TYPE_MAX = TYPE_STRING + 1;

  private:
    // so TypeRepresentation can call appendStringScalar() etc
    friend class TypeRepresentation;

    Type type_;

    explicit ReferenceTypeRepresentation(Type type);

    // See TypeRepresentation::appendString()
    bool appendStringReference(JSContext *cx, StringBuffer &buffer);

  public:
    Type type() const {
        return type_;
    }

    const char *typeName() const {
        return typeName(type());
    }

    static const char *typeName(Type type);
    static JSObject *Create(JSContext *cx, Type type);
};

#define JS_FOR_EACH_REFERENCE_TYPE_REPR(macro_)                             \
    macro_(ReferenceTypeRepresentation::TYPE_ANY,    HeapValue, Any)        \
    macro_(ReferenceTypeRepresentation::TYPE_OBJECT, HeapPtrObject, Object) \
    macro_(ReferenceTypeRepresentation::TYPE_STRING, HeapPtrString, string)

class X4TypeRepresentation : public TypeRepresentation {
  public:
    enum Type {
        TYPE_INT32 = JS_X4TYPEREPR_INT32,
        TYPE_FLOAT32 = JS_X4TYPEREPR_FLOAT32,
    };

  private:
    // so TypeRepresentation can call appendStringScalar() etc
    friend class TypeRepresentation;

    // in order to call constructor
    friend class TypeRepresentationHelper;

    const Type type_;

    explicit X4TypeRepresentation(Type type);

    // See TypeRepresentation::appendString()
    bool appendStringX4(JSContext *cx, StringBuffer &buffer);

  public:
    Type type() const {
        return type_;
    }

    static JSObject *Create(JSContext *cx, Type type);
};

// Must be in same order as the enum ScalarTypeRepresentation::Type:
#define JS_FOR_EACH_X4_TYPE_REPR(macro_)                                      \
    macro_(X4TypeRepresentation::TYPE_INT32, int32_t, int32)                  \
    macro_(X4TypeRepresentation::TYPE_FLOAT32, float, float32)

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
                         size_t offset);
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

    const StructField *fieldNamed(jsid id) const;

    static JSObject *Create(JSContext *cx,
                            AutoIdVector &ids,
                            AutoObjectVector &typeReprOwners);
};

} // namespace js

#endif
