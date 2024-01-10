/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ABIFunctionType_h
#define jit_ABIFunctionType_h

#include <initializer_list>
#include <stdint.h>

#include "jit/ABIFunctionTypeGenerated.h"

namespace js {
namespace jit {

enum ABIArgType {
  // A pointer sized integer
  ArgType_General = 0x1,
  // A 32-bit integer
  ArgType_Int32 = 0x2,
  // A 64-bit integer
  ArgType_Int64 = 0x3,
  // A 32-bit floating point number
  ArgType_Float32 = 0x4,
  // A 64-bit floating point number
  ArgType_Float64 = 0x5,

  RetType_Shift = 0x0,
  ArgType_Shift = 0x3,
  ArgType_Mask = 0x7
};

namespace detail {

static constexpr uint64_t MakeABIFunctionType(
    ABIArgType ret, std::initializer_list<ABIArgType> args) {
  uint64_t abiType = 0;
  for (auto arg : args) {
    abiType <<= ArgType_Shift;
    abiType |= (uint64_t)arg;
  }
  abiType <<= ArgType_Shift;
  abiType |= (uint64_t)ret;
  return abiType;
}

}  // namespace detail

enum ABIFunctionType : uint64_t {
  // The enum must be explicitly typed to avoid UB: some validly constructed
  // members are larger than any explicitly declared members.
  ABI_FUNCTION_TYPE_ENUM
};

static constexpr ABIFunctionType MakeABIFunctionType(
    ABIArgType ret, std::initializer_list<ABIArgType> args) {
  return ABIFunctionType(detail::MakeABIFunctionType(ret, args));
}

}  // namespace jit
}  // namespace js

#endif /* jit_ABIFunctionType_h */
