/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2022 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_serialize_h
#define wasm_serialize_h

#include "mozilla/MacroForEach.h"
#include "mozilla/Maybe.h"

#include <cstdint>
#include <type_traits>

namespace js {
namespace wasm {

// [SMDOC] "Cacheable POD"
//
// Module serialization relies on copying simple structs to and from the
// cache format. We need a way to ensure that we only do this on types that are
// "safe". We call this "cacheable POD". Note: this is not the same thing as
// "POD" as that may contain pointers, which are not cacheable.
//
// We define cacheable POD (C-POD) recursively upon types:
//   1. any integer type is C-POD
//   2. any floating point type is C-POD
//   3. any enum type is C-POD
//   4. any mozilla::Maybe<T> with T: C-POD is C-POD
//   5. any T[N] with T: C-POD is C-POD
//   6. any union where all fields are C-POD is C-POD
//   7. any struct with the following conditions must is C-POD
//      * every field's type must be C-POD
//      * the parent type, if it exists, must also be C-POD
//      * there must be no virtual methods
//
// There are no combination of C++ type traits at this time that can
// automatically meet these criteria, so we are rolling our own system.
//
// We define a "IsCacheablePod" type trait, with builtin rules for cases (1-5).
// The complex cases (6-7) are handled using manual declaration and checking
// macros that must be used upon structs and unions that are considered
// cacheable POD.
//
// See the following macros for details:
//   - WASM_DECLARE_CACHEABLE_POD
//   - WASM_CHECK_CACHEABLE_POD[_WITH_PARENT]

// The IsCacheablePod type trait primary template. Contains the rules for
// (cases 1-3).
template <typename T>
struct IsCacheablePod
    : public std::conditional_t<std::is_arithmetic_v<T> || std::is_enum_v<T>,
                                std::true_type, std::false_type> {};

// Partial specialization for (case 4).
template <typename T>
struct IsCacheablePod<mozilla::Maybe<T>>
    : public std::conditional_t<IsCacheablePod<T>::value, std::true_type,
                                std::false_type> {};

// Partial specialization for (case 5).
template <typename T, size_t N>
struct IsCacheablePod<T[N]>
    : public std::conditional_t<IsCacheablePod<T>::value, std::true_type,
                                std::false_type> {};

template <class T>
inline constexpr bool is_cacheable_pod = IsCacheablePod<T>::value;

// Declare the type 'Type' to be cacheable POD. The definition of the type must
// contain a WASM_CHECK_CACHEABLE_POD[_WITH_PARENT] to ensure all fields of the
// type are cacheable POD.
#define WASM_DECLARE_CACHEABLE_POD(Type)                                      \
  static_assert(!std::is_polymorphic_v<Type>,                                 \
                #Type "must not have virtual methods");                       \
  } /* namespace wasm */                                                      \
  } /* namespace js */                                                        \
  template <>                                                                 \
  struct js::wasm::IsCacheablePod<js::wasm::Type> : public std::true_type {}; \
  namespace js {                                                              \
  namespace wasm {

// Helper: check each field's type to be cacheable POD
#define WASM_CHECK_CACHEABLE_POD_FIELD_(Field)                    \
  static_assert(js::wasm::IsCacheablePod<decltype(Field)>::value, \
                #Field " must be cacheable pod");

// Check every field in a type definition to ensure they are cacheable POD.
#define WASM_CHECK_CACHEABLE_POD(Fields...) \
  MOZ_FOR_EACH(WASM_CHECK_CACHEABLE_POD_FIELD_, (), (Fields))

// Check every field in a type definition to ensure they are cacheable POD, and
// check that the parent class is also cacheable POD.
#define WASM_CHECK_CACHEABLE_POD_WITH_PARENT(Parent, Fields...) \
  static_assert(js::wasm::IsCacheablePod<Parent>::value,        \
                #Parent " must be cacheable pod");              \
  MOZ_FOR_EACH(WASM_CHECK_CACHEABLE_POD_FIELD_, (), (Fields))

// Allow fields that are not cacheable POD but are believed to be safe for
// serialization due to some justification.
#define WASM_ALLOW_NON_CACHEABLE_POD_FIELD(Field, Reason)          \
  static_assert(!js::wasm::IsCacheablePod<decltype(Field)>::value, \
                #Field " is not cacheable due to " Reason);

}  // namespace wasm
}  // namespace js

#endif  // wasm_serialize_h
