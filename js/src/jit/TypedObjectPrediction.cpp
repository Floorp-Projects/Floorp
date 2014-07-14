/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TypedObjectPrediction.h"

using namespace js;
using namespace jit;

static const size_t ALL_FIELDS = SIZE_MAX;

// Sets the prediction to be the common prefix of descrA and descrB,
// considering at most the first max fields.
//
// In the case where the current prediction is a specific struct,
// and we are now seeing a second struct, then descrA and descrB will be
// the current and new struct and max will be ALL_FIELDS.
//
// In the case where the current prediction is already a prefix, and
// we are now seeing an additional struct, then descrA will be the
// current struct and max will be the current prefix length, and
// descrB will be the new struct.
//
// (Note that in general it is not important which struct is passed as
// descrA and which struct is passed as descrB, as the operation is
// symmetric.)
void
TypedObjectPrediction::markAsCommonPrefix(const StructTypeDescr &descrA,
                                          const StructTypeDescr &descrB,
                                          size_t max)
{
    // count is the number of fields in common. It begins as the min
    // of the number of fields from descrA, descrB, and max, and then
    // is decremented as we find uncommon fields.
    if (max > descrA.fieldCount())
        max = descrA.fieldCount();
    if (max > descrB.fieldCount())
        max = descrB.fieldCount();

    size_t i = 0;
    for (; i < max; i++) {
        if (&descrA.fieldName(i) != &descrB.fieldName(i))
            break;
        if (&descrA.fieldDescr(i) != &descrB.fieldDescr(i))
            break;
        JS_ASSERT(descrA.fieldOffset(i) == descrB.fieldOffset(i));
    }

    if (i == 0) {
        // empty prefix is not particularly useful.
        markInconsistent();
    } else {
        setPrefix(descrA, i);
    }
}

void
TypedObjectPrediction::addProto(const TypedProto &proto)
{
    switch (predictionKind()) {
      case Empty:
        return setProto(proto);

      case Inconsistent:
        return; // keep same state

      case Proto: {
        if (&proto == data_.proto)
            return; // keep same state

        if (proto.kind() != data_.proto->kind())
            return markInconsistent();

        if (proto.kind() != type::Struct)
            return markInconsistent();

        const StructTypeDescr &structDescr = proto.typeDescr().as<StructTypeDescr>();
        const StructTypeDescr &currentDescr = data_.proto->typeDescr().as<StructTypeDescr>();
        markAsCommonPrefix(structDescr, currentDescr, ALL_FIELDS);
        return;
      }

      case Descr:
        // First downgrade from descr to proto, which is less precise,
        // and then recurse.
        setProto(data_.descr->typedProto());
        return addProto(proto);

      case Prefix:
        if (proto.kind() != type::Struct)
            return markInconsistent();

        markAsCommonPrefix(*data_.prefix.descr,
                           proto.typeDescr().as<StructTypeDescr>(),
                           data_.prefix.fields);
        return;
    }

    MOZ_ASSUME_UNREACHABLE("Bad predictionKind");
}

type::Kind
TypedObjectPrediction::kind() const
{
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        return proto().kind();

      case TypedObjectPrediction::Descr:
        return descr().kind();

      case TypedObjectPrediction::Prefix:
        return prefix().descr->kind();
    }

    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

bool
TypedObjectPrediction::ofArrayKind() const
{
    switch (kind()) {
      case type::Scalar:
      case type::Reference:
      case type::X4:
      case type::Struct:
        return false;

      case type::SizedArray:
      case type::UnsizedArray:
        return true;
    }

    MOZ_ASSUME_UNREACHABLE("Bad kind");
}

static bool
DescrHasKnownSize(const TypeDescr &descr, int32_t *out)
{
    if (!descr.is<SizedTypeDescr>())
        return false;

    *out = descr.as<SizedTypeDescr>().size();
    return true;
}

bool
TypedObjectPrediction::hasKnownSize(int32_t *out) const
{
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        switch (kind()) {
          case type::Scalar:
          case type::Reference:
          case type::X4:
          case type::Struct:
            *out = proto().typeDescr().as<SizedTypeDescr>().size();
            return true;

          case type::SizedArray:
          case type::UnsizedArray:
            // The prototype does not track the precise dimensions of arrays.
            return false;
        }
        MOZ_ASSUME_UNREACHABLE("Unknown kind");

      case TypedObjectPrediction::Descr:
        return DescrHasKnownSize(descr(), out);

      case TypedObjectPrediction::Prefix:
        // We only know a prefix of the struct fields, hence we do not
        // know its complete size.
        return false;
    }

    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

const TypedProto *
TypedObjectPrediction::getKnownPrototype() const
{
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        return nullptr;

      case TypedObjectPrediction::Proto:
        switch (proto().kind()) {
          case type::Scalar:
          case type::Reference:
            return nullptr;

          case type::X4:
          case type::Struct:
          case type::SizedArray:
          case type::UnsizedArray:
            return &proto();
        }
        MOZ_ASSUME_UNREACHABLE("Invalid proto().kind()");

      case TypedObjectPrediction::Descr:
        if (descr().is<ComplexTypeDescr>())
            return &descr().as<ComplexTypeDescr>().instancePrototype();
        return nullptr;

      case TypedObjectPrediction::Prefix:
        // We only know a prefix of the struct fields, hence we cannot
        // say for certain what its prototype will be.
        return nullptr;
    }

    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

template<typename T>
typename T::Type
TypedObjectPrediction::extractType() const
{
    JS_ASSERT(kind() == T::Kind);
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        return proto().typeDescr().as<T>().type();

      case TypedObjectPrediction::Descr:
        return descr().as<T>().type();

      case TypedObjectPrediction::Prefix:
        break; // Prefixes are always structs, never scalars etc
    }

    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

ScalarTypeDescr::Type
TypedObjectPrediction::scalarType() const
{
    return extractType<ScalarTypeDescr>();
}

ReferenceTypeDescr::Type
TypedObjectPrediction::referenceType() const
{
    return extractType<ReferenceTypeDescr>();
}

X4TypeDescr::Type
TypedObjectPrediction::x4Type() const
{
    return extractType<X4TypeDescr>();
}

bool
TypedObjectPrediction::hasKnownArrayLength(int32_t *length) const
{
    JS_ASSERT(ofArrayKind());
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        // The prototype does not track the lengths of arrays.
        return false;

      case TypedObjectPrediction::Descr:
        // In later patches, this condition will always be true
        // so long as this represents an array
        if (descr().is<SizedArrayTypeDescr>()) {
            *length = descr().as<SizedArrayTypeDescr>().length();
            return true;
        }
        return false;

      case TypedObjectPrediction::Prefix:
        break; // Prefixes are always structs, never arrays
    }
    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

static TypeDescr &
DescrArrayElementType(const TypeDescr &descr) {
    return (descr.is<SizedArrayTypeDescr>()
            ? descr.as<SizedArrayTypeDescr>().elementType()
            : descr.as<UnsizedArrayTypeDescr>().elementType());
}

TypedObjectPrediction
TypedObjectPrediction::arrayElementType() const
{
    JS_ASSERT(ofArrayKind());
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        return TypedObjectPrediction(DescrArrayElementType(proto().typeDescr()));

      case TypedObjectPrediction::Descr:
        return TypedObjectPrediction(DescrArrayElementType(descr()));

      case TypedObjectPrediction::Prefix:
        break; // Prefixes are always structs, never arrays
    }
    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

bool
TypedObjectPrediction::hasFieldNamedPrefix(const StructTypeDescr &descr,
                                           size_t fieldCount,
                                           jsid id,
                                           size_t *fieldOffset,
                                           TypedObjectPrediction *out,
                                           size_t *index) const
{
    // Find the index of the field |id| if any.
    if (!descr.fieldIndex(id, index))
        return false;

    // Check whether the index falls within our known safe prefix.
    if (*index >= fieldCount)
        return false;

    // Load the offset and type.
    *fieldOffset = descr.fieldOffset(*index);
    *out = TypedObjectPrediction(descr.fieldDescr(*index));
    return true;
}

bool
TypedObjectPrediction::hasFieldNamed(jsid id,
                                     size_t *fieldOffset,
                                     TypedObjectPrediction *fieldType,
                                     size_t *fieldIndex) const
{
    JS_ASSERT(kind() == type::Struct);

    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        return hasFieldNamedPrefix(
            proto().typeDescr().as<StructTypeDescr>(), ALL_FIELDS,
            id, fieldOffset, fieldType, fieldIndex);

      case TypedObjectPrediction::Descr:
        return hasFieldNamedPrefix(
            descr().as<StructTypeDescr>(), ALL_FIELDS,
            id, fieldOffset, fieldType, fieldIndex);

      case TypedObjectPrediction::Prefix:
        return hasFieldNamedPrefix(
            *prefix().descr, prefix().fields,
            id, fieldOffset, fieldType, fieldIndex);
    }
    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}
