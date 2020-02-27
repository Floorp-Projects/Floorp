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
#include "js/CompileOptions.h"

/*
 * Data shared between the vm and stencil structures.
 */

namespace js {
class BaseScript;

template <typename EnumType>
class ScriptFlagBase {
  // To allow cross-checking offsetof assert.
  friend class js::BaseScript;

 protected:
  // Stored as a uint32_t to make access more predictable from
  // JIT code.
  uint32_t flags_ = 0;

 public:
  MOZ_MUST_USE bool hasFlag(EnumType flag) const {
    return flags_ & static_cast<uint32_t>(flag);
  }
  void setFlag(EnumType flag) { flags_ |= static_cast<uint32_t>(flag); }
  void clearFlag(EnumType flag) { flags_ &= ~static_cast<uint32_t>(flag); }
  void setFlag(EnumType flag, bool b) {
    if (b) {
      setFlag(flag);
    } else {
      clearFlag(flag);
    }
  }

  operator uint32_t() const { return flags_; }
};

enum class ImmutableScriptFlagsEnum : uint32_t {
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

class ImmutableScriptFlags : public ScriptFlagBase<ImmutableScriptFlagsEnum> {
  // Immutable flags should not be modified after the JSScript that contains
  // them has been initialized. These flags should likely be preserved when
  // serializing (XDR) or copying (CopyScript) the script. This is only public
  // for the JITs.
  //
  // Specific accessors for flag values are defined with
  // IMMUTABLE_FLAG_* macros below.
 public:
  ImmutableScriptFlags() = default;

  void static_asserts() {
    static_assert(sizeof(ImmutableScriptFlags) == sizeof(flags_),
                  "No extra members allowed");
    static_assert(offsetof(ImmutableScriptFlags, flags_) == 0,
                  "Required for JIT flag access");
  }

  void operator=(uint32_t flag) { flags_ = flag; }

  static ImmutableScriptFlags fromCompileOptions(
      const JS::ReadOnlyCompileOptions& options) {
    ImmutableScriptFlags isf;
    isf.setFlag(ImmutableScriptFlagsEnum::NoScriptRval, options.noScriptRval);
    isf.setFlag(ImmutableScriptFlagsEnum::SelfHosted, options.selfHostingMode);
    isf.setFlag(ImmutableScriptFlagsEnum::TreatAsRunOnce, options.isRunOnce);
    return isf;
  };

  static ImmutableScriptFlags fromCompileOptions(
      const JS::TransitiveCompileOptions& options) {
    ImmutableScriptFlags isf;
    isf.setFlag(ImmutableScriptFlagsEnum::NoScriptRval,
                /* noScriptRval (non-transitive compile option) = */ false);
    isf.setFlag(ImmutableScriptFlagsEnum::SelfHosted, options.selfHostingMode);
    isf.setFlag(ImmutableScriptFlagsEnum::TreatAsRunOnce,
                /* isRunOnce (non-transitive compile option) = */ false);
    return isf;
  };
};

enum class MutableScriptFlagsEnum : uint32_t {
  // Number of times the |warmUpCount| was forcibly discarded. The counter is
  // reset when a script is successfully jit-compiled.
  WarmupResets_MASK = 0xFF,

  // Have warned about uses of undefined properties in this script.
  WarnedAboutUndefinedProp = 1 << 8,

  // If treatAsRunOnce, whether script has executed.
  HasRunOnce = 1 << 9,

  // Script has been reused for a clone.
  HasBeenCloned = 1 << 10,

  // Script has an entry in Realm::scriptCountsMap.
  HasScriptCounts = 1 << 12,

  // Script has an entry in Realm::debugScriptMap.
  HasDebugScript = 1 << 13,

  // Do not relazify this script. This is used by the relazify() testing
  // function for scripts that are on the stack and also by the AutoDelazify
  // RAII class. Usually we don't relazify functions in compartments with
  // scripts on the stack, but the relazify() testing function overrides that,
  // and sometimes we're working with a cross-compartment function and need to
  // keep it from relazifying.
  DoNotRelazify = 1 << 14,

  // IonMonkey compilation hints.

  // Script has had hoisted bounds checks fail.
  FailedBoundsCheck = 1 << 15,

  // Script has had hoisted shape guard fail.
  FailedShapeGuard = 1 << 16,

  HadFrequentBailouts = 1 << 17,
  HadOverflowBailout = 1 << 18,

  // Whether Baseline or Ion compilation has been disabled for this script.
  // IonDisabled is equivalent to |jitScript->canIonCompile() == false| but
  // JitScript can be discarded on GC and we don't want this to affect
  // observable behavior (see ArgumentsGetterImpl comment).
  BaselineDisabled = 1 << 19,
  IonDisabled = 1 << 20,

  // Explicitly marked as uninlineable.
  Uninlineable = 1 << 21,

  // Idempotent cache has triggered invalidation.
  InvalidatedIdempotentCache = 1 << 22,

  // Lexical check did fail and bail out.
  FailedLexicalCheck = 1 << 23,

  // See comments below.
  NeedsArgsAnalysis = 1 << 24,
  NeedsArgsObj = 1 << 25,

  // Set if the debugger's onNewScript hook has not yet been called.
  HideScriptFromDebugger = 1 << 26,

  // Set if the script has opted into spew
  SpewEnabled = 1 << 27,

  // Set for LazyScripts which have been wrapped by some Debugger.
  WrappedByDebugger = 1 << 28,
};

class MutableScriptFlags : public ScriptFlagBase<MutableScriptFlagsEnum> {
 public:
  MutableScriptFlags() = default;

  void static_asserts() {
    static_assert(sizeof(MutableScriptFlags) == sizeof(flags_),
                  "No extra members allowed");
    static_assert(offsetof(MutableScriptFlags, flags_) == 0,
                  "Required for JIT flag access");
  }

  MutableScriptFlags& operator&=(const uint32_t rhs) {
    flags_ &= rhs;
    return *this;
  }

  MutableScriptFlags& operator|=(const uint32_t rhs) {
    flags_ |= rhs;
    return *this;
  }
};

}  // namespace js

#endif /* vm_SharedStencil_h */
