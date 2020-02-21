/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SharedStencil_h
#define vm_SharedStencil_h

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

#include "jstypes.h"

/*
 * Data shared between the vm and stencil structures.
 */

namespace js {
namespace frontend {

class ImmutableScriptFlags {
 public:
  enum ScriptFlags : uint32_t {
    // Input Flags
    //
    // Flags that come from CompileOptions.
    // ----

    // No need for result value of last expression statement.
    NoScriptRval = 1 << 0,

    // See Parser::selfHostingMode.
    SelfHosted = 1 << 1,

    // Script is a lambda to treat as running once or a global or eval script
    // that will only run once.  Which one it is can be disambiguated by
    // checking whether function() is null.
    TreatAsRunOnce = 1 << 2,
    // ----

    // Code is in strict mode.
    Strict = 1 << 3,

    // True if the script has a non-syntactic scope on its dynamic scope chain.
    // That is, there are objects about which we know nothing between the
    // outermost syntactic scope and the global.
    HasNonSyntacticScope = 1 << 4,

    // See FunctionBox.
    BindingsAccessedDynamically = 1 << 5,
    FunHasExtensibleScope = 1 << 6,

    // Bytecode contains JSOp::CallSiteObj
    // (We don't relazify functions with template strings, due to observability)
    HasCallSiteObj = 1 << 7,

    // Script is parsed with a top-level goal of Module. This may be a top-level
    // or an inner-function script.
    HasModuleGoal = 1 << 8,

    FunctionHasThisBinding = 1 << 9,
    FunctionHasExtraBodyVarScope = 1 << 10,

    // Whether the arguments object for this script, if it needs one, should be
    // mapped (alias formal parameters).
    HasMappedArgsObj = 1 << 11,

    // Script contains inner functions. Used to check if we can relazify the
    // script.
    HasInnerFunctions = 1 << 12,

    NeedsHomeObject = 1 << 13,

    IsDerivedClassConstructor = 1 << 14,
    IsDefaultClassConstructor = 1 << 15,

    // 'this', 'arguments' and f.apply() are used. This is likely to be a
    // wrapper.
    IsLikelyConstructorWrapper = 1 << 16,

    // Set if this function is a generator function or async generator.
    IsGenerator = 1 << 17,

    // Set if this function is an async function or async generator.
    IsAsync = 1 << 18,

    // Set if this function has a rest parameter.
    HasRest = 1 << 19,

    // See comments below.
    ArgumentsHasVarBinding = 1 << 20,

    // Script came from eval().
    IsForEval = 1 << 21,

    // Whether this is a top-level module script.
    IsModule = 1 << 22,

    // Whether this function needs a call object or named lambda environment.
    NeedsFunctionEnvironmentObjects = 1 << 23,

    // Whether the Parser declared 'arguments'.
    ShouldDeclareArguments = 1 << 24,

    // Script is for function.
    IsFunction = 1 << 25,

    // Whether this script contains a direct eval statement.
    HasDirectEval = 1 << 26,

    // Whether this BaseScript is a LazyScript. This flag will be removed after
    // LazyScript and JSScript are merged in Bug 1529456.
    IsLazyScript = 1 << 27,
  };

  // This is stored as uint32_t instead of bitfields to make it more
  // predictable to access from JIT code.
  uint32_t scriptFlags_ = 0;

 public:
  ImmutableScriptFlags() : scriptFlags_() {
    static_assert(sizeof(ImmutableScriptFlags) == sizeof(scriptFlags_),
                  "No extra members allowed");
    static_assert(offsetof(ImmutableScriptFlags, scriptFlags_) == 0,
                  "Required for JIT flag access");
  }

  MOZ_IMPLICIT ImmutableScriptFlags(ScriptFlags sf) : scriptFlags_(sf) {}
  MOZ_IMPLICIT ImmutableScriptFlags(int32_t flags)
      : scriptFlags_(ScriptFlags(flags)) {}

  operator uint32_t() const { return scriptFlags_; }

  void operator=(uint32_t flag) { scriptFlags_ = flag; }

  uint32_t toRaw() const { return scriptFlags_; }
};

}  // namespace frontend
}  // namespace js

#endif /* vm_SharedStencil_h */
