/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_ENUMSERIALIZER_H__
#define __IPC_GLUE_ENUMSERIALIZER_H__

#include "CrashAnnotations.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/Assertions.h"
#include "mozilla/IntegerTypeTraits.h"
#include "nsExceptionHandler.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsTLiteralString.h"

class PickleIterator;

namespace IPC {
class Message;
class MessageReader;
class MessageWriter;
}  // namespace IPC

#ifdef _MSC_VER
#  pragma warning(disable : 4800)
#endif

namespace IPC {

/**
 * Generic enum serializer.
 *
 * Consider using the specializations below, such as ContiguousEnumSerializer.
 *
 * This is a generic serializer for any enum type used in IPDL.
 * Programmers can define ParamTraits<E> for enum type E by deriving
 * EnumSerializer<E, MyEnumValidator> where MyEnumValidator is a struct
 * that has to define a static IsLegalValue function returning whether
 * a given value is a legal value of the enum type at hand.
 *
 * \sa https://developer.mozilla.org/en/IPDL/Type_Serialization
 */
template <typename E, typename EnumValidator>
struct EnumSerializer {
  typedef E paramType;

  // XXX(Bug 1690343) Should this be changed to
  // std::make_unsigned_t<std::underlying_type_t<paramType>>, to make this more
  // consistent with the type used for validating values?
  typedef typename mozilla::UnsignedStdintTypeForSize<sizeof(paramType)>::Type
      uintParamType;

  static void Write(MessageWriter* aWriter, const paramType& aValue) {
    // XXX This assertion is somewhat meaningless at least for E that don't have
    // a fixed underlying type: if aValue weren't a legal value, we would
    // already have UB where this function is called.
    MOZ_RELEASE_ASSERT(EnumValidator::IsLegalValue(
        static_cast<std::underlying_type_t<paramType>>(aValue)));
    WriteParam(aWriter, uintParamType(aValue));
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    uintParamType value;
    if (!ReadParam(aReader, &value)) {
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::IPCReadErrorReason, "Bad iter"_ns);
      return false;
    } else if (!EnumValidator::IsLegalValue(value)) {
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::IPCReadErrorReason, "Illegal value"_ns);
      return false;
    }
    *aResult = paramType(value);
    return true;
  }
};

template <typename E, E MinLegal, E HighBound>
class ContiguousEnumValidator {
  // Silence overzealous -Wtype-limits bug in GCC fixed in GCC 4.8:
  // "comparison of unsigned expression >= 0 is always true"
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11856
  template <typename T>
  static bool IsLessThanOrEqual(T a, T b) {
    return a <= b;
  }

 public:
  using IntegralType = std::underlying_type_t<E>;
  static constexpr auto kMinLegalIntegral = static_cast<IntegralType>(MinLegal);
  static constexpr auto kHighBoundIntegral =
      static_cast<IntegralType>(HighBound);

  static bool IsLegalValue(const IntegralType e) {
    return IsLessThanOrEqual(kMinLegalIntegral, e) && e < kHighBoundIntegral;
  }
};

template <typename E, E MinLegal, E MaxLegal>
class ContiguousEnumValidatorInclusive {
  // Silence overzealous -Wtype-limits bug in GCC fixed in GCC 4.8:
  // "comparison of unsigned expression >= 0 is always true"
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11856
  template <typename T>
  static bool IsLessThanOrEqual(T a, T b) {
    return a <= b;
  }

 public:
  using IntegralType = std::underlying_type_t<E>;
  static constexpr auto kMinLegalIntegral = static_cast<IntegralType>(MinLegal);
  static constexpr auto kMaxLegalIntegral = static_cast<IntegralType>(MaxLegal);

  static bool IsLegalValue(const IntegralType e) {
    return IsLessThanOrEqual(kMinLegalIntegral, e) && e <= kMaxLegalIntegral;
  }
};

template <typename E, E AllBits>
struct BitFlagsEnumValidator {
  static bool IsLegalValue(const std::underlying_type_t<E> e) {
    return (e & static_cast<std::underlying_type_t<E>>(AllBits)) == e;
  }
};

/**
 * Specialization of EnumSerializer for enums with contiguous enum values.
 *
 * Provide two values: MinLegal, HighBound. An enum value x will be
 * considered legal if MinLegal <= x < HighBound.
 *
 * For example, following is definition of serializer for enum type FOO.
 * \code
 * enum FOO { FOO_FIRST, FOO_SECOND, FOO_LAST, NUM_FOO };
 *
 * template <>
 * struct ParamTraits<FOO>:
 *     public ContiguousEnumSerializer<FOO, FOO_FIRST, NUM_FOO> {};
 * \endcode
 * FOO_FIRST, FOO_SECOND, and FOO_LAST are valid value.
 */
template <typename E, E MinLegal, E HighBound>
struct ContiguousEnumSerializer
    : EnumSerializer<E, ContiguousEnumValidator<E, MinLegal, HighBound>> {};

/**
 * This is similar to ContiguousEnumSerializer, but the last template
 * parameter is expected to be the highest legal value, rather than a
 * sentinel value. This is intended to support enumerations that don't
 * have sentinel values.
 */
template <typename E, E MinLegal, E MaxLegal>
struct ContiguousEnumSerializerInclusive
    : EnumSerializer<E,
                     ContiguousEnumValidatorInclusive<E, MinLegal, MaxLegal>> {
};

/**
 * Specialization of EnumSerializer for enums representing bit flags.
 *
 * Provide one value: AllBits. An enum value x will be
 * considered legal if (x & AllBits) == x;
 *
 * Example:
 * \code
 * enum FOO {
 *   FOO_FIRST =  1 << 0,
 *   FOO_SECOND = 1 << 1,
 *   FOO_LAST =   1 << 2,
 *   ALL_BITS =   (1 << 3) - 1
 * };
 *
 * template <>
 * struct ParamTraits<FOO>:
 *     public BitFlagsEnumSerializer<FOO, FOO::ALL_BITS> {};
 * \endcode
 */
template <typename E, E AllBits>
struct BitFlagsEnumSerializer
    : EnumSerializer<E, BitFlagsEnumValidator<E, AllBits>> {};

} /* namespace IPC */

#endif /* __IPC_GLUE_ENUMSERIALIZER_H__ */
