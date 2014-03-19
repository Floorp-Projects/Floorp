/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TypeRepresentationSet_h
#define jit_TypeRepresentationSet_h

#include "builtin/TypedObject.h"
#include "jit/IonAllocPolicy.h"
#include "js/HashTable.h"

// TypeRepresentationSet stores a set of TypeRepresentation* objects,
// representing the possible types of the binary data associated with
// a typed object value.  Often TypeRepresentationSets will be
// singleton sets, but it is also possible to have cases where many
// type representations flow into a single point. In such cases, the
// various type representations may differ in their details but often
// have a common prefix. We try to optimize this case as well.
//
// So, for example, consider some code like:
//
//     var Point2Type = new StructType({x: uint8, y: uint8});
//     var Point3Type = new StructType({x: uint8, y: uint8, z: uint8});
//
//     function distance2d(pnt) {
//         return Math.sqrt(pnt.x * pnt.x + pnt.y * pnt.y);
//     }
//
// Even if the function `distance2d()` were used with instances of
// both Point2Type and Point3Type, we can still generate optimal code,
// because both of those types contain fields named `x` and `y` with
// the same types at the same offset.

namespace js {
namespace jit {

class IonBuilder;
class TypeDescrSet;

class TypeDescrSetBuilder {
  private:
    Vector<TypeDescr *, 4, SystemAllocPolicy> entries_;
    bool invalid_;

  public:
    TypeDescrSetBuilder();

    bool insert(TypeDescr *typeRepr);
    bool build(IonBuilder &builder, TypeDescrSet *out);
};

class TypeDescrSet {
  private:
    friend struct TypeDescrSetHasher;
    friend class TypeDescrSetBuilder;

    size_t length_;
    TypeDescr **entries_; // Allocated using temp policy

    TypeDescrSet(size_t length, TypeDescr **entries);

    size_t length() const {
        return length_;
    }

    TypeDescr *get(uint32_t i) const {
        return entries_[i];
    }

    template<typename T>
    bool genericType(typename T::Type *out);

  public:
    //////////////////////////////////////////////////////////////////////
    // Constructors
    //
    // For more flexible constructors, see
    // TypeDescrSetBuilder above.

    TypeDescrSet(const TypeDescrSet &c);
    TypeDescrSet(); // empty set

    //////////////////////////////////////////////////////////////////////
    // Query the set

    bool empty() const;
    bool allOfKind(TypeDescr::Kind kind);

    // Returns true only when non-empty and `kind()` is
    // `TypeDescr::Array`
    bool allOfArrayKind();

    // Returns true only if (1) non-empty, (2) for all types t in this
    // set, t is sized, and (3) there is some size S such that for all
    // types t in this set, `t.size() == S`.  When the above holds,
    // then also sets `*out` to S; otherwise leaves `*out` unchanged
    // and returns false.
    //
    // At the moment condition (2) trivially holds.  When Bug 922115
    // lands, some array types will be unsized.
    bool allHaveSameSize(size_t *out);

    types::TemporaryTypeSet *suitableTypeSet(IonBuilder &builder,
                                             const Class *knownClass);

    //////////////////////////////////////////////////////////////////////
    // The following operations are only valid on a non-empty set:

    TypeDescr::Kind kind();

    // Returns the prototype that a typed object whose type is within
    // this TypeDescrSet would have. Returns `null` if this cannot be
    // predicted or instances of the type are not objects (e.g., uint8).
    JSObject *knownPrototype() const;

    //////////////////////////////////////////////////////////////////////
    // Scalar operations
    //
    // Only valid when `kind() == TypeDescr::Scalar`

    // If all type descrs in this set have a single type, returns true
    // and sets *out. Else returns false.
    bool scalarType(ScalarTypeDescr::Type *out);

    //////////////////////////////////////////////////////////////////////
    // Reference operations
    //
    // Only valid when `kind() == TypeDescr::Reference`

    // If all type descrs in this set have a single type, returns true
    // and sets *out. Else returns false.
    bool referenceType(ReferenceTypeDescr::Type *out);

    //////////////////////////////////////////////////////////////////////
    // Reference operations
    //
    // Only valid when `kind() == TypeDescr::X4`

    // If all type descrs in this set have a single type, returns true
    // and sets *out. Else returns false.
    bool x4Type(X4TypeDescr::Type *out);

    //////////////////////////////////////////////////////////////////////
    // SizedArray operations
    //
    // Only valid when `kind() == TypeDescr::SizedArray`

    // Determines whether all arrays in this set have the same,
    // statically known, array length and return that length
    // (via `*length`) if so. Otherwise returns false.
    bool hasKnownArrayLength(size_t *length);

    // Returns a `TypeDescrSet` representing the element
    // types of the various array types in this set. The returned set
    // may be the empty set.
    bool arrayElementType(IonBuilder &builder, TypeDescrSet *out);

    //////////////////////////////////////////////////////////////////////
    // Struct operations
    //
    // Only valid when `kind() == TypeDescr::Struct`

    // Searches the type in the set for a field named `id`. All
    // possible types must agree on the offset of the field within the
    // structure and the possible types of the field must be
    // compatible. If any pair of types disagree on the offset or have
    // incompatible types for the field, then `*out` will be set to
    // the empty set.
    //
    // Upon success, `out` will be set to the set of possible types of
    // the field and `offset` will be set to the field's offset within
    // the struct (measured in bytes).
    //
    // The parameter `*index` is special. If all types agree on the
    // index of the field, then `*index` is set to the field index.
    // Otherwise, it is set to SIZE_MAX. Note that two types may agree
    // on the type and offset of a field but disagree about its index,
    // e.g. the field `c` in `new StructType({a: uint8, b: uint8, c:
    // uint16})` and `new StructType({a: uint16, c: uint16})`.
    bool fieldNamed(IonBuilder &builder,
                    jsid id,
                    size_t *offset,
                    TypeDescrSet *out,
                    size_t *index);
};

struct TypeDescrSetHasher
{
    typedef TypeDescrSet Lookup;
    static HashNumber hash(TypeDescrSet key);
    static bool match(TypeDescrSet key1,
                      TypeDescrSet key2);
};

typedef js::HashSet<TypeDescrSet,
                    TypeDescrSetHasher,
                    IonAllocPolicy> TypeDescrSetHash;

} // namespace jit
} // namespace js

#endif
