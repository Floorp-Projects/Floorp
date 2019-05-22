/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitScript_h
#define jit_JitScript_h

#include "js/UniquePtr.h"
#include "vm/TypeInference.h"

class JSScript;

namespace js {

namespace jit {

class ICScript;

}  // namespace jit

// JitScript stores type inference and JIT data for a script. Scripts with a
// JitScript can run in the Baseline Interpreter.
class JitScript {
  friend class ::JSScript;

  // The freeze constraints added to stack type sets will only directly
  // invalidate the script containing those stack type sets. This Vector
  // contains compilations that inlined this script, so we can invalidate
  // them as well.
  RecompileInfoVector inlinedCompilations_;

  // ICScript and JitScript have the same lifetimes, so we store a pointer to
  // ICScript here to not increase sizeof(JSScript).
  using ICScriptPtr = js::UniquePtr<js::jit::ICScript>;
  ICScriptPtr icScript_;

  // Number of TypeSets in typeArray_.
  uint32_t numTypeSets_;

  // This field is used to avoid binary searches for the sought entry when
  // bytecode map queries are in linear order.
  uint32_t bytecodeTypeMapHint_ = 0;

  struct Flags {
    // Flag set when discarding JIT code to indicate this script is on the stack
    // and type information and JIT code should not be discarded.
    bool active : 1;

    // Generation for type sweeping. If out of sync with the TypeZone's
    // generation, this JitScript needs to be swept.
    bool typesGeneration : 1;

    // Whether freeze constraints for stack type sets have been generated.
    bool hasFreezeConstraints : 1;
  };
  Flags flags_ = {};  // Zero-initialize flags.

  // Variable-size array. This is followed by the bytecode type map.
  StackTypeSet typeArray_[1];

  StackTypeSet* typeArrayDontCheckGeneration() {
    // Ensure typeArray_ is the last data member of JitScript.
    static_assert(sizeof(JitScript) ==
                      sizeof(typeArray_) + offsetof(JitScript, typeArray_),
                  "typeArray_ must be the last member of JitScript");
    return const_cast<StackTypeSet*>(typeArray_);
  }

  uint32_t typesGeneration() const { return uint32_t(flags_.typesGeneration); }
  void setTypesGeneration(uint32_t generation) {
    MOZ_ASSERT(generation <= 1);
    flags_.typesGeneration = generation;
  }

 public:
  JitScript(JSScript* script, ICScriptPtr&& icScript, uint32_t numTypeSets);

  bool hasFreezeConstraints(const js::AutoSweepJitScript& sweep) const {
    MOZ_ASSERT(sweep.jitScript() == this);
    return flags_.hasFreezeConstraints;
  }
  void setHasFreezeConstraints(const js::AutoSweepJitScript& sweep) {
    MOZ_ASSERT(sweep.jitScript() == this);
    flags_.hasFreezeConstraints = true;
  }

  inline bool typesNeedsSweep(Zone* zone) const;
  void sweepTypes(const js::AutoSweepJitScript& sweep, Zone* zone);

  RecompileInfoVector& inlinedCompilations(
      const js::AutoSweepJitScript& sweep) {
    MOZ_ASSERT(sweep.jitScript() == this);
    return inlinedCompilations_;
  }
  MOZ_MUST_USE bool addInlinedCompilation(const js::AutoSweepJitScript& sweep,
                                          RecompileInfo info) {
    MOZ_ASSERT(sweep.jitScript() == this);
    if (!inlinedCompilations_.empty() && inlinedCompilations_.back() == info) {
      return true;
    }
    return inlinedCompilations_.append(info);
  }

  uint32_t numTypeSets() const { return numTypeSets_; }

  uint32_t* bytecodeTypeMapHint() { return &bytecodeTypeMapHint_; }

  bool active() const { return flags_.active; }
  void setActive() { flags_.active = true; }
  void resetActive() { flags_.active = false; }

  jit::ICScript* icScript() const {
    MOZ_ASSERT(icScript_);
    return icScript_.get();
  }

  /* Array of type sets for variables and JOF_TYPESET ops. */
  StackTypeSet* typeArray(const js::AutoSweepJitScript& sweep) {
    MOZ_ASSERT(sweep.jitScript() == this);
    return typeArrayDontCheckGeneration();
  }

  uint32_t* bytecodeTypeMap() {
    MOZ_ASSERT(numTypeSets_ > 0);
    return reinterpret_cast<uint32_t*>(typeArray_ + numTypeSets_);
  }

  inline StackTypeSet* thisTypes(const AutoSweepJitScript& sweep,
                                 JSScript* script);
  inline StackTypeSet* argTypes(const AutoSweepJitScript& sweep,
                                JSScript* script, unsigned i);

  /* Get the type set for values observed at an opcode. */
  inline StackTypeSet* bytecodeTypes(const AutoSweepJitScript& sweep,
                                     JSScript* script, jsbytecode* pc);

  template <typename TYPESET>
  static inline TYPESET* BytecodeTypes(JSScript* script, jsbytecode* pc,
                                       uint32_t* bytecodeMap, uint32_t* hint,
                                       TYPESET* typeArray);

  /*
   * Monitor a bytecode pushing any value. This must be called for any opcode
   * which is JOF_TYPESET, and where either the script has not been analyzed
   * by type inference or where the pc has type barriers. For simplicity, we
   * always monitor JOF_TYPESET opcodes in the interpreter and stub calls,
   * and only look at barriers when generating JIT code for the script.
   */
  static void MonitorBytecodeType(JSContext* cx, JSScript* script,
                                  jsbytecode* pc, const js::Value& val);
  static void MonitorBytecodeType(JSContext* cx, JSScript* script,
                                  jsbytecode* pc, TypeSet::Type type);

  static inline void MonitorBytecodeType(JSContext* cx, JSScript* script,
                                         jsbytecode* pc, StackTypeSet* types,
                                         const js::Value& val);

 private:
  static void MonitorBytecodeTypeSlow(JSContext* cx, JSScript* script,
                                      jsbytecode* pc, StackTypeSet* types,
                                      TypeSet::Type type);

 public:
  /* Monitor an assignment at a SETELEM on a non-integer identifier. */
  static inline void MonitorAssign(JSContext* cx, HandleObject obj, jsid id);

  /* Add a type for a variable in a script. */
  static inline void MonitorThisType(JSContext* cx, JSScript* script,
                                     TypeSet::Type type);
  static inline void MonitorThisType(JSContext* cx, JSScript* script,
                                     const js::Value& value);
  static inline void MonitorArgType(JSContext* cx, JSScript* script,
                                    unsigned arg, TypeSet::Type type);
  static inline void MonitorArgType(JSContext* cx, JSScript* script,
                                    unsigned arg, const js::Value& value);

  /*
   * Freeze all the stack type sets in a script, for a compilation. Returns
   * copies of the type sets which will be checked against the actual ones
   * under FinishCompilation, to detect any type changes.
   */
  static bool FreezeTypeSets(CompilerConstraintList* constraints,
                             JSScript* script, TemporaryTypeSet** pThisTypes,
                             TemporaryTypeSet** pArgTypes,
                             TemporaryTypeSet** pBytecodeTypes);

  void destroy(Zone* zone);

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    // Note: icScript_ size is reported in jit::AddSizeOfBaselineData.
    return mallocSizeOf(this);
  }

  static constexpr size_t offsetOfICScript() {
    // Note: icScript_ is a UniquePtr that stores the raw pointer. If that ever
    // changes and this assertion fails, we should stop using UniquePtr.
    static_assert(sizeof(icScript_) == sizeof(uintptr_t),
                  "JIT code assumes icScript_ is pointer-sized");
    return offsetof(JitScript, icScript_);
  }

#ifdef DEBUG
  void printTypes(JSContext* cx, HandleScript script);
#endif
};

// Ensures no JitScripts are purged in the current zone.
class MOZ_RAII AutoKeepJitScripts {
  TypeZone& zone_;
  bool prev_;

  AutoKeepJitScripts(const AutoKeepJitScripts&) = delete;
  void operator=(const AutoKeepJitScripts&) = delete;

 public:
  explicit inline AutoKeepJitScripts(JSContext* cx);
  inline ~AutoKeepJitScripts();
};

}  // namespace js

#endif /* jit_JitScript_h */
