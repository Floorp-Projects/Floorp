/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineIC.h"
#include "jit/JitRealm.h"
#include "jit/VMFunctions.h"
#include "vm/Interpreter.h"

#include "jit/BaselineFrame-inl.h"
#include "vm/Interpreter-inl.h"

namespace js {
namespace jit {

// List of all VM functions to be used with callVM. Each entry stores the name
// (must be unique, used for the VMFunctionId enum and profiling) and the C++
// function to be called. This list must be sorted on the name field.
#define VMFUNCTION_LIST(_)                                                   \
  _(BaselineDebugPrologue, js::jit::DebugPrologue)                           \
  _(BaselineGetFunctionThis, js::jit::BaselineGetFunctionThis)               \
  _(BaselineThrowInitializedThis, js::jit::BaselineThrowInitializedThis)     \
  _(BaselineThrowUninitializedThis, js::jit::BaselineThrowUninitializedThis) \
  _(BindVarOperation, js::BindVarOperation)                                  \
  _(CheckIsCallable, js::jit::CheckIsCallable)                               \
  _(CheckOverRecursedBaseline, js::jit::CheckOverRecursedBaseline)           \
  _(CloneRegExpObject, js::CloneRegExpObject)                                \
  _(DefFunOperation, js::DefFunOperation)                                    \
  _(DefLexicalOperation, js::DefLexicalOperation)                            \
  _(DefVarOperation, js::DefVarOperation)                                    \
  _(DeleteElementNonStrict, js::DeleteElementJit<false>)                     \
  _(DeleteElementStrict, js::DeleteElementJit<true>)                         \
  _(DeleteNameOperation, js::DeleteNameOperation)                            \
  _(DeletePropertyNonStrict, js::DeletePropertyJit<false>)                   \
  _(DeletePropertyStrict, js::DeletePropertyJit<true>)                       \
  _(GetImportOperation, js::GetImportOperation)                              \
  _(GetNonSyntacticGlobalThis, js::GetNonSyntacticGlobalThis)                \
  _(InitElemGetterSetterOperation, js::InitElemGetterSetterOperation)        \
  _(InitPropGetterSetterOperation, js::InitPropGetterSetterOperation)        \
  _(InterruptCheck, js::jit::InterruptCheck)                                 \
  _(IonCompileScriptForBaseline, js::jit::IonCompileScriptForBaseline)       \
  _(Lambda, js::Lambda)                                                      \
  _(LambdaArrow, js::LambdaArrow)                                            \
  _(MutatePrototype, js::jit::MutatePrototype)                               \
  _(NewArrayCopyOnWriteOperation, js::NewArrayCopyOnWriteOperation)          \
  _(NewDenseCopyOnWriteArray, js::NewDenseCopyOnWriteArray)                  \
  _(OptimizeSpreadCall, js::OptimizeSpreadCall)                              \
  _(ProcessCallSiteObjOperation, js::ProcessCallSiteObjOperation)            \
  _(SetFunctionName, js::SetFunctionName)                                    \
  _(SetIntrinsicOperation, js::SetIntrinsicOperation)                        \
  _(SetPropertySuper, js::SetPropertySuper)                                  \
  _(SingletonObjectLiteralOperation, js::SingletonObjectLiteralOperation)    \
  _(ThrowBadDerivedReturn, js::jit::ThrowBadDerivedReturn)                   \
  _(ThrowCheckIsObject, js::ThrowCheckIsObject)                              \
  _(ThrowRuntimeLexicalError, js::jit::ThrowRuntimeLexicalError)

enum class VMFunctionId {
#define DEF_ID(name, fp) name,
  VMFUNCTION_LIST(DEF_ID)
#undef DEF_ID
      Count
};

// Define the VMFunctionToId template to map from signature + function to
// the VMFunctionId. This lets us verify the consumer/codegen code matches
// the C++ signature.
template <typename Function, Function fun>
struct VMFunctionToId;  // Error on this line? Forgot to update VMFUNCTION_LIST?

// GCC warns when the signature does not have matching attributes (for example
// MOZ_MUST_USE). Squelch this warning to avoid a GCC-only footgun.
#if MOZ_IS_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

// Note: the use of ::fp instead of fp is intentional to enforce use of
// fully-qualified names in the list above.
#define DEF_TEMPLATE(name, fp)                             \
  template <>                                              \
  struct VMFunctionToId<decltype(&(::fp)), ::fp> {         \
    static constexpr VMFunctionId id = VMFunctionId::name; \
  };
VMFUNCTION_LIST(DEF_TEMPLATE)
#undef DEF_TEMPLATE

#if MOZ_IS_GCC
#  pragma GCC diagnostic pop
#endif

}  // namespace jit
}  // namespace js
