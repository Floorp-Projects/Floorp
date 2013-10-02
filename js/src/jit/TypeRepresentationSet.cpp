/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TypeRepresentationSet.h"

#include "mozilla/HashFunctions.h"

#include "jit/IonBuilder.h"

using namespace js;
using namespace jit;

///////////////////////////////////////////////////////////////////////////
// TypeRepresentationSet hasher

HashNumber
TypeRepresentationSetHasher::hash(TypeRepresentationSet key)
{
    HashNumber hn = mozilla::HashGeneric(key.length());
    for (size_t i = 0; i < key.length(); i++)
        hn = mozilla::AddToHash(hn, uintptr_t(key.get(i)));
    return hn;
}

bool
TypeRepresentationSetHasher::match(TypeRepresentationSet key1,
                                   TypeRepresentationSet key2)
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
// TypeRepresentationSetBuilder

TypeRepresentationSetBuilder::TypeRepresentationSetBuilder()
  : invalid_(false)
{}

bool
TypeRepresentationSetBuilder::insert(TypeRepresentation *typeRepr)
{
    if (invalid_)
        return true;

    if (entries_.length() == 0)
        return entries_.append(typeRepr);

    // Check that this new type repr is of the same basic kind as the
    // ones we have seen thus far. If not, for example if we have an
    // `int` and a `struct`, then convert this set to the invalid set.
    TypeRepresentation *entry0 = entries_[0];
    if (typeRepr->kind() != entry0->kind()) {
        invalid_ = true;
        entries_.clear();
        return true;
    }

    // Otherwise, use binary search to find the right place to insert
    // the type descriptor. We keep list sorted by the *address* of
    // the type representations within.
    uintptr_t typeReprAddr = (uintptr_t) typeRepr;
    size_t min = 0;
    size_t max = entries_.length();
    while (min != max) {
        size_t i = min + ((max - min) >> 1); // average w/o fear of overflow

        uintptr_t entryiaddr = (uintptr_t) entries_[i];
        if (entryiaddr == typeReprAddr)
            return true; // typeRepr already present in the set

        if (entryiaddr < typeReprAddr) {
            // typeRepr lies to the right of entry i
            min = i;
        } else {
            // typeRepr lies to the left of entry i
            max = i;
        }
    }

    // As a sanity check, give up if the TypeRepresentationSet grows too large.
    if (entries_.length() >= 512) {
        invalid_ = true;
        entries_.clear();
        return true;
    }

    // Not present. Insert at position `min`.
    if (min == entries_.length())
        return entries_.append(typeRepr);
    TypeRepresentation **insertLoc = &entries_[min];
    return entries_.insert(insertLoc, typeRepr) != nullptr;
}

bool
TypeRepresentationSetBuilder::build(IonBuilder &builder,
                                    TypeRepresentationSet *out)
{
    if (invalid_) {
        *out = TypeRepresentationSet();
        return true;
    }

    TypeRepresentationSetHash *table = builder.getOrCreateReprSetHash();
    if (!table)
        return false;

    // Check if there is already a copy in the hashtable.
    size_t length = entries_.length();
    TypeRepresentationSet tempSet(length, entries_.begin());
    TypeRepresentationSetHash::AddPtr p = table->lookupForAdd(tempSet);
    if (p) {
        *out = *p;
        return true;
    }

    // If not, allocate a permanent copy in Ion temp memory and add it.
    size_t space = sizeof(TypeRepresentation*) * length;
    TypeRepresentation **array = (TypeRepresentation**)
        GetIonContext()->temp->allocate(space);
    if (!array)
        return false;
    memcpy(array, entries_.begin(), space);
    TypeRepresentationSet permSet(length, array);
    if (!table->add(p, permSet))
        return false;

    *out = permSet;
    return true;
}

///////////////////////////////////////////////////////////////////////////
// TypeRepresentationSet

TypeRepresentationSet::TypeRepresentationSet(const TypeRepresentationSet &c)
  : length_(c.length_),
    entries_(c.entries_)
{}

TypeRepresentationSet::TypeRepresentationSet(size_t length,
                                             TypeRepresentation **entries)
  : length_(length),
    entries_(entries)
{}

TypeRepresentationSet::TypeRepresentationSet()
  : length_(0),
    entries_(nullptr)
{}

bool
TypeRepresentationSet::empty()
{
    return length() == 0;
}

size_t
TypeRepresentationSet::length()
{
    return length_;
}

TypeRepresentation *
TypeRepresentationSet::get(size_t i)
{
    JS_ASSERT(i < length());
    return entries_[i];
}

bool
TypeRepresentationSet::allOfKind(TypeRepresentation::Kind aKind)
{
    if (empty())
        return false;

    return kind() == aKind;
}

TypeRepresentation::Kind
TypeRepresentationSet::kind()
{
    JS_ASSERT(!empty());
    return get(0)->kind();
}

size_t
TypeRepresentationSet::arrayLength()
{
    JS_ASSERT(kind() == TypeRepresentation::Array);
    const size_t result = get(0)->asArray()->length();
    for (size_t i = 1; i < length(); i++) {
        if (get(i)->asArray()->length() != result)
            return SIZE_MAX;
    }
    return result;
}

bool
TypeRepresentationSet::arrayElementType(IonBuilder &builder,
                                        TypeRepresentationSet *out)
{
    JS_ASSERT(kind() == TypeRepresentation::Array);

    TypeRepresentationSetBuilder elementTypes;
    for (size_t i = 0; i < length(); i++) {
        if (!elementTypes.insert(get(i)->asArray()->element()))
            return false;
    }
    return elementTypes.build(builder, out);
}

bool
TypeRepresentationSet::fieldNamed(IonBuilder &builder,
                                  jsid id,
                                  size_t *offset,
                                  TypeRepresentationSet *out,
                                  size_t *index)
{
    JS_ASSERT(kind() == TypeRepresentation::Struct);

    // Initialize `*offset` and `*out` for the case where incompatible
    // or absent fields are found.
    *offset = SIZE_MAX;
    *index = SIZE_MAX;
    *out = TypeRepresentationSet();

    // Remember offset of the first field.
    size_t offset0;
    size_t index0;
    TypeRepresentationSetBuilder fieldTypes;
    {
        const StructField *field = get(0)->asStruct()->fieldNamed(id);
        if (!field)
            return true;

        offset0 = field->offset;
        index0 = field->index;
        if (!fieldTypes.insert(field->typeRepr))
            return false;
    }

    // Check that all subsequent fields are at the same offset
    // and compute the union of their types.
    for (size_t i = 1; i < length(); i++) {
        const StructField *field = get(i)->asStruct()->fieldNamed(id);
        if (!field)
            return true;

        if (field->offset != offset0)
            return true;

        if (field->index != index0)
            index0 = SIZE_MAX;

        if (!fieldTypes.insert(field->typeRepr))
            return false;
    }

    // All struct types had a field named `id` at the same offset
    // (though it's still possible that the types are incompatible and
    // that the indices disagree).
    *offset = offset0;
    *index = index0;
    return fieldTypes.build(builder, out);
}
