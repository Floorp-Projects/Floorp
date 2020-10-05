/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ABIFunctionList_inl_h
#define jit_ABIFunctionList_inl_h

#include "jslibmath.h"  // js::NumberMod
#include "jsmath.h"     // js::ecmaPow, js::ecmaHypot, js::hypot3, js::hypot4,
                        // js::ecmaAtan2, js::UnaryMathFunctionType, js::powi
#include "jsnum.h"      // js::StringToNumberPure, js::Int32ToStringHelperPure,
                        // js::NumberToStringHelperPure

#include "builtin/Array.h"      // js::ArrayShiftMoveElements
#include "builtin/MapObject.h"  // js::MapIteratorObject::next,
                                // js::SetIteratorObject::next
#include "builtin/RegExp.h"     // js::RegExpPrototypeOptimizableRaw,
                                // js::RegExpInstanceOptimizableRaw

#include "irregexp/RegExpAPI.h"
// js::irregexp::CaseInsensitiveCompareNonUnicode,
// js::irregexp::CaseInsensitiveCompareUnicode,
// js::irregexp::GrowBacktrackStack

#include "jit/ABIFunctions.h"
#include "jit/Bailouts.h"  // js::jit::FinishBailoutToBaseline, js::jit::Bailout,
                           // js::jit::InvalidationBailout

#include "jit/JitFrames.h"    // HandleException
#include "jit/VMFunctions.h"  // Rest of js::jit::* functions.

#include "js/Conversions.h"  // JS::ToInt32
#include "vm/ArgumentsObject.h"  // js::ArgumentsObject::finishForIonPure
#include "vm/RegExpShared.h"     // js::ExecuteRegExpAtomRaw
#include "vm/TraceLogging.h"     // js::TraceLogStartEventPrivate,
                                 // js::TraceLogStartEvent,
                                 // js::TraceLogStopEventPrivate

#include "wasm/WasmBuiltins.h"  // js::wasm::*

namespace js {
namespace jit {

// List of all ABI functions to be used with callWithABI. Each entry stores
// the fully qualified name of the C++ function. This list must be sorted.
#ifdef WASM_CODEGEN_DEBUG
#  define ABIFUNCTION_WASM_CODEGEN_DEBUG_LIST(_) \
    _(js::wasm::PrintF32)                        \
    _(js::wasm::PrintF64)                        \
    _(js::wasm::PrintI32)                        \
    _(js::wasm::PrintPtr)                        \
    _(js::wasm::PrintText)
#else
#  define ABIFUNCTION_WASM_CODEGEN_DEBUG_LIST(_)
#endif

#define ABIFUNCTION_LIST(_)                                 \
  ABIFUNCTION_WASM_CODEGEN_DEBUG_LIST(_)                    \
  _(js::ArgumentsObject::finishForIonPure)                  \
  _(js::ArrayShiftMoveElements)                             \
  _(js::ecmaAtan2)                                          \
  _(js::ecmaHypot)                                          \
  _(js::ecmaPow)                                            \
  _(js::ExecuteRegExpAtomRaw)                               \
  _(js::hypot3)                                             \
  _(js::hypot4)                                             \
  _(js::Int32ToStringHelperPure)                            \
  _(js::irregexp::CaseInsensitiveCompareNonUnicode)         \
  _(js::irregexp::CaseInsensitiveCompareUnicode)            \
  _(js::irregexp::GrowBacktrackStack)                       \
  _(js::jit::AssertValidBigIntPtr)                          \
  _(js::jit::AssertValidObjectOrNullPtr)                    \
  _(js::jit::AssertValidObjectPtr)                          \
  _(js::jit::AssertValidStringPtr)                          \
  _(js::jit::AssertValidSymbolPtr)                          \
  _(js::jit::AssertValidValue)                              \
  _(js::jit::AssumeUnreachable)                             \
  _(js::jit::Bailout)                                       \
  _(js::jit::FinishBailoutToBaseline)                       \
  _(js::jit::HandleException)                               \
  _(js::jit::InitBaselineFrameForOsr)                       \
  _(js::jit::InvalidationBailout)                           \
  _(js::jit::Printf0)                                       \
  _(js::jit::Printf1)                                       \
  _(js::MapIteratorObject::next)                            \
  _(js::NumberMod)                                          \
  _(js::NumberToStringHelperPure)                           \
  _(js::powi)                                               \
  _(js::RegExpInstanceOptimizableRaw)                       \
  _(js::RegExpPrototypeOptimizableRaw)                      \
  _(js::SetIteratorObject::next)                            \
  _(js::StringToNumberPure)                                 \
  _(js::TraceLogStartEventPrivate)                          \
  _(js::TraceLogStopEventPrivate)

// List of all ABI functions to be used with callWithABI, which are
// overloaded. Each entry stores the fully qualified name of the C++ function,
// followed by the signature of the function to be called. When the function
// is not overloaded, you should prefer adding the function to
// ABIFUNCTION_LIST instead. This list must be sorted with the name of the C++
// function.
#define ABIFUNCTION_AND_TYPE_LIST(_)                       \
  _(js::TraceLogStartEvent,                                \
    void (*)(TraceLoggerThread*, const TraceLoggerEvent&)) \
  _(JS::ToInt32, int32_t (*)(double))

// List of all ABI function signature which are using a computed function
// pointer instead of a statically known function pointer.
#define ABIFUNCTIONSIG_LIST(_)                       \
  _(float (*)(float))                                \
  _(js::UnaryMathFunctionType)                       \
  _(void (*)(JSRuntime * rt, JSObject * *objp))      \
  _(void (*)(JSRuntime * rt, JSString * *stringp))   \
  _(void (*)(JSRuntime * rt, ObjectGroup * *groupp)) \
  _(void (*)(JSRuntime * rt, Shape * *shapep))       \
  _(void (*)(JSRuntime * rt, Value * vp))

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

// Define a known list of function signatures.
#define DEF_TEMPLATE(...)                        \
  template <>                                    \
  struct ABIFunctionSignatureData<__VA_ARGS__> { \
    static constexpr bool registered = true;     \
  };
ABIFUNCTIONSIG_LIST(DEF_TEMPLATE)
#undef DEF_TEMPLATE

#if MOZ_IS_GCC
#  pragma GCC diagnostic pop
#endif

}  // namespace jit
}  // namespace js

#endif  // jit_VMFunctionList_inl_h
