/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ABIFunctionList_inl_h
#define jit_ABIFunctionList_inl_h

#include "jit/ABIFunctions.h"

namespace js {
namespace jit {

// List of all ABI functions to be used with callWithABI. Each entry stores
// the fully qualified name of the C++ function. This list must be sorted.
#define ABIFUNCTION_LIST(_)                                 \

// List of all ABI functions to be used with callWithABI, which are
// overloaded. Each entry stores the fully qualified name of the C++ function,
// followed by the signature of the function to be called. When the function
// is not overloaded, you should prefer adding the function to
// ABIFUNCTION_LIST instead. This list must be sorted with the name of the C++
// function.
#define ABIFUNCTION_AND_TYPE_LIST(_)                       \

// GCC warns when the signature does not have matching attributes (for example
// MOZ_MUST_USE). Squelch this warning to avoid a GCC-only footgun.
#if MOZ_IS_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

// Note: the use of ::fp instead of fp is intentional to enforce use of
// fully-qualified names in the list above.
#define DEF_TEMPLATE(fp)                            \
  template <>                                       \
  struct ABIFunctionData<decltype(&(::fp)), ::fp> { \
    static constexpr bool registered = true;        \
  };
ABIFUNCTION_LIST(DEF_TEMPLATE)
#undef DEF_TEMPLATE

#define DEF_TEMPLATE(fp, ...)                 \
  template <>                                 \
  struct ABIFunctionData<__VA_ARGS__, ::fp> { \
    static constexpr bool registered = true;  \
  };
ABIFUNCTION_AND_TYPE_LIST(DEF_TEMPLATE)
#undef DEF_TEMPLATE

#if MOZ_IS_GCC
#  pragma GCC diagnostic pop
#endif

}  // namespace jit
}  // namespace js

#endif  // jit_VMFunctionList_inl_h
