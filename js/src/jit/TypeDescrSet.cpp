/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TypeDescrSet.h"

#include "mozilla/HashFunctions.h"

#include "builtin/TypedObject.h"
#include "jit/IonBuilder.h"

using namespace js;
using namespace jit;

///////////////////////////////////////////////////////////////////////////
// TypeDescrSet hasher

HashNumber
TypeDescrSetHasher::hash(TypeDescrSet key)
{
    HashNumber hn = mozilla::HashGeneric(key.length());
    for (size_t i = 0; i < key.length(); i++)
        hn = mozilla::AddToHash(hn, uintptr_t(key.get(i)));
    return hn;
}

bool
TypeDescrSetHasher::match(TypeDescrSet key1, TypeDescrSet key2)
{
    if (key1.length() != key2.length())
        return false;

    // Note: entries are always sorted
    for (size_t i = 0; i < key1.length(); i++) {
        if (key1.get(i) != key2.get(i))
            return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////
// TypeDescrSetBuilder

TypeDescrSetBuilder::TypeDescrSetBuilder()
  : invalid_(false)
{}

bool
TypeDescrSetBuilder::insert(TypeDescr *descr)
{
    // All type descriptors should be tenured, so it is safe to assume
    // that the pointers do not change during compilation, since no
    // major GC can overlap with compilation.
    JS_ASSERT(!GetIonContext()->runtime->isInsideNursery(descr));

    if (invalid_)
        return true;

    if (entries_.empty())
        return entries_.append(descr);

    // Check that this new type repr is of the same basic kind as the
    // ones we have seen thus far. If not, for example if we have an
    // `int` and a `struct`, then convert this set to the invalid set.
    TypeDescr *entry0 = entries_[0];
    if (descr->kind() != entry0->kind()) {
        invalid_ = true;
        entries_.clear();
        return true;
    }

    // Otherwise, use binary search to find the right place to insert
    // the TypeDescr. We keep the list sorted by the *address* of
    // the TypeDescrs within.
    uintptr_t descrAddr = (uintptr_t) descr;
    size_t min = 0;
    size_t max = entries_.length();
    while (min != max) {
        size_t i = min + ((max - min) >> 1); // average w/o fear of overflow

        uintptr_t entryiaddr = (uintptr_t) entries_[i];
        if (entryiaddr == descrAddr)
            return true; // descr already present in the set

        if (entryiaddr < descrAddr) {
            // descr lies to the right of entry i
            min = i + 1;
        } else {
            // descr lies to the left of entry i
            max = i;
        }
    }

    // As a sanity check, give up if the TypeDescrSet grows too large.
    if (entries_.length() >= 512) {
        invalid_ = true;
        entries_.clear();
        return true;
    }

    // Not present. Insert at position `min`.
    if (min == entries_.length())
        return entries_.append(descr);
    TypeDescr **insertLoc = &entries_[min];
    return entries_.insert(insertLoc, descr) != nullptr;
}

bool
TypeDescrSetBuilder::build(IonBuilder &builder, TypeDescrSet *out)
{
    if (invalid_) {
        *out = TypeDescrSet();
        return true;
    }

    TypeDescrSetHash *table = builder.getOrCreateDescrSetHash();
    if (!table)
        return false;

    // Check if there is already a copy in the hashtable.
    size_t length = entries_.length();
    TypeDescrSet tempSet(length, entries_.begin());
    TypeDescrSetHash::AddPtr p = table->lookupForAdd(tempSet);
    if (p) {
        *out = *p;
        return true;
    }

    // If not, allocate a permanent copy in Ion temp memory and add it.
    size_t space = sizeof(TypeDescr*) * length;
    TypeDescr **array = (TypeDescr**)
        GetIonContext()->temp->allocate(space);
    if (!array)
        return false;
    memcpy(array, entries_.begin(), space);
    TypeDescrSet permSet(length, array);
    if (!table->add(p, permSet))
        return false;

    *out = permSet;
    return true;
}

///////////////////////////////////////////////////////////////////////////
// TypeDescrSet

TypeDescrSet::TypeDescrSet(const TypeDescrSet &c)
  : length_(c.length_),
    entries_(c.entries_)
{}

TypeDescrSet::TypeDescrSet(size_t length,
                           TypeDescr **entries)
  : length_(length),
    entries_(entries)
{}

TypeDescrSet::TypeDescrSet()
  : length_(0),
    entries_(nullptr)
{}

bool
TypeDescrSet::empty() const
{
    return length_ == 0;
}

bool
TypeDescrSet::allOfArrayKind()
{
    if (empty())
        return false;

    switch (kind()) {
      case TypeDescr::SizedArray:
      case TypeDescr::UnsizedArray:
        return true;

      case TypeDescr::X4:
      case TypeDescr::Reference:
      case TypeDescr::Scalar:
      case TypeDescr::Struct:
        return false;
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind() in TypeDescrSet");
}

bool
TypeDescrSet::allOfKind(TypeDescr::Kind aKind)
{
    if (empty())
        return false;

    return kind() == aKind;
}

bool
TypeDescrSet::allHaveSameSize(int32_t *out)
{
    if (empty())
        return false;

    JS_ASSERT(TypeDescr::isSized(kind()));

    int32_t size = get(0)->as<SizedTypeDescr>().size();
    for (size_t i = 1; i < length(); i++) {
        if (get(i)->as<SizedTypeDescr>().size() != size)
            return false;
    }

    *out = size;
    return true;
}

JSObject *
TypeDescrSet::knownPrototype() const
{
    JS_ASSERT(!empty());
    if (length() > 1 || !get(0)->is<ComplexTypeDescr>())
        return nullptr;
    return &get(0)->as<ComplexTypeDescr>().instancePrototype();
}

TypeDescr::Kind
TypeDescrSet::kind()
{
    JS_ASSERT(!empty());
    return get(0)->kind();
}

template<typename T>
bool
TypeDescrSet::genericType(typename T::Type *out)
{
    JS_ASSERT(allOfKind(TypeDescr::Scalar));

    typename T::Type type = get(0)->as<T>().type();
    for (size_t i = 1; i < length(); i++) {
        if (get(i)->as<T>().type() != type)
            return false;
    }

    *out = type;
    return true;
}

bool
TypeDescrSet::scalarType(ScalarTypeDescr::Type *out)
{
    return genericType<ScalarTypeDescr>(out);
}

bool
TypeDescrSet::referenceType(ReferenceTypeDescr::Type *out)
{
    return genericType<ReferenceTypeDescr>(out);
}

bool
TypeDescrSet::x4Type(X4TypeDescr::Type *out)
{
    return genericType<X4TypeDescr>(out);
}

bool
TypeDescrSet::hasKnownArrayLength(int32_t *l)
{
    switch (kind()) {
      case TypeDescr::UnsizedArray:
        return false;

      case TypeDescr::SizedArray:
      {
        const size_t result = get(0)->as<SizedArrayTypeDescr>().length();
        for (size_t i = 1; i < length(); i++) {
            size_t l = get(i)->as<SizedArrayTypeDescr>().length();
            if (l != result)
                return false;
        }
        *l = result;
        return true;
      }

      default:
        MOZ_ASSUME_UNREACHABLE("Invalid array size for call to arrayLength()");
    }
}

bool
TypeDescrSet::arrayElementType(IonBuilder &builder, TypeDescrSet *out)
{
    TypeDescrSetBuilder elementTypes;
    for (size_t i = 0; i < length(); i++) {
        switch (kind()) {
          case TypeDescr::UnsizedArray:
            if (!elementTypes.insert(&get(i)->as<UnsizedArrayTypeDescr>().elementType()))
                return false;
            break;

          case TypeDescr::SizedArray:
            if (!elementTypes.insert(&get(i)->as<SizedArrayTypeDescr>().elementType()))
                return false;
            break;

          default:
            MOZ_ASSUME_UNREACHABLE("Invalid kind for arrayElementType()");
        }
    }
    return elementTypes.build(builder, out);
}

bool
TypeDescrSet::fieldNamed(IonBuilder &builder,
                         jsid id,
                         int32_t *offset,
                         TypeDescrSet *out,
                         size_t *index)
{
    JS_ASSERT(kind() == TypeDescr::Struct);

    // Initialize `*offset` and `*out` for the case where incompatible
    // or absent fields are found.
    *offset = -1;
    *index = SIZE_MAX;
    *out = TypeDescrSet();

    // Remember offset of the first field.
    int32_t offset0;
    size_t index0;
    TypeDescrSetBuilder fieldTypes;
    {
        StructTypeDescr &descr0 = get(0)->as<StructTypeDescr>();
        if (!descr0.fieldIndex(id, &index0))
            return true;

        offset0 = descr0.fieldOffset(index0);
        if (!fieldTypes.insert(&descr0.fieldDescr(index0)))
            return false;
    }

    // Check that all subsequent fields are at the same offset
    // and compute the union of their types.
    for (size_t i = 1; i < length(); i++) {
        StructTypeDescr &descri = get(i)->as<StructTypeDescr>();

        size_t indexi;
        if (!descri.fieldIndex(id, &indexi))
            return true;

        // Track whether all indices agree, but do not require it to be true
        if (indexi != index0)
            index0 = SIZE_MAX;

        // Require that all offsets agree
        if (descri.fieldOffset(indexi) != offset0)
            return true;

        if (!fieldTypes.insert(&descri.fieldDescr(indexi)))
            return false;
    }

    // All struct types had a field named `id` at the same offset
    // (though it's still possible that the types are incompatible and
    // that the indices disagree).
    *offset = offset0;
    *index = index0;
    return fieldTypes.build(builder, out);
}
