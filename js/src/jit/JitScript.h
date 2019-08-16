/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitScript_h
#define jit_JitScript_h

#include "jit/BaselineIC.h"
#include "js/UniquePtr.h"
#include "vm/TypeInference.h"

class JSScript;

namespace js {
namespace jit {

class ControlFlowGraph;

// Describes a single wasm::ImportExit which jumps (via an import with
// the given index) directly to a JitScript.
struct DependentWasmImport {
  wasm::Instance* instance;
  size_t importIndex;

  DependentWasmImport(wasm::Instance& instance, size_t importIndex)
      : instance(&instance), importIndex(importIndex) {}
};

// Information about a script's bytecode, used by IonBuilder. This is cached
// in JitScript.
struct IonBytecodeInfo {
  bool usesEnvironmentChain = false;
  bool modifiesArguments = false;
};

// Magic BaselineScript value indicating Baseline compilation has been disabled.
static constexpr uintptr_t BaselineDisabledScript = 0x1;

static BaselineScript* const BaselineDisabledScriptPtr =
    reinterpret_cast<BaselineScript*>(BaselineDisabledScript);

// Magic IonScript values indicating Ion compilation has been disabled or the
// script is being Ion-compiled off-thread.
static constexpr uintptr_t IonDisabledScript = 0x1;
static constexpr uintptr_t IonCompilingScript = 0x2;

static IonScript* const IonDisabledScriptPtr =
    reinterpret_cast<IonScript*>(IonDisabledScript);
static IonScript* const IonCompilingScriptPtr =
    reinterpret_cast<IonScript*>(IonCompilingScript);

// [SMDOC] JitScript
//
// JitScript stores type inference data, Baseline ICs and other JIT-related data
// for a script. Scripts with a JitScript can run in the Baseline Interpreter.
//
// IC Data
// =======
// All IC data for Baseline (Interpreter and JIT) is stored in JitScript. Ion
// has its own IC chains stored in IonScript.
//
// For each IC we store an ICEntry, which points to the first ICStub in the
// chain. Note that multiple stubs in the same zone can share Baseline IC code.
// This works because the stub data is stored in the ICStub instead of baked in
// in the stub code.
//
// Storing this separate from BaselineScript allows us to use the same ICs in
// the Baseline Interpreter and Baseline JIT. It also simplifies debug mode OSR
// because the JitScript can be reused when we have to recompile the
// BaselineScript.
//
// JitScript contains the following IC data structures:
//
// * Fallback stub space: this stores all fallback stubs and the "can GC" stubs.
//   These stubs are never purged before destroying the JitScript. (Other stubs
//   are stored in the optimized stub space stored in JitZone and can be
//   discarded more eagerly. See JitScript::purgeOptimizedStubs.)
//
// * List of IC entries, in the following order:
//
//   - Type monitor IC for |this|.
//   - Type monitor IC for each formal argument.
//   - IC for each JOF_IC bytecode op.
//
// Type Inference Data
// ===================
// JitScript also contains Type Inference data, most importantly:
//
// * An array of StackTypeSets for type monitoring of |this|, formal arguments,
//   JOF_TYPESET ops. These TypeSets record the types we observed and have
//   constraints to trigger invalidation of Ion code when the TypeSets change.
//
// * The bytecode type map to map from StackTypeSet index to bytecode offset.
//
// * List of Ion compilations inlining this script, for invalidation.
//
// Memory Layout
// =============
// JitScript has various trailing (variable-length) arrays. The memory layout is
// as follows:
//
//  Item                    | Offset
//  ------------------------+------------------------
//  JitScript               | 0
//  ICEntry[]               | sizeof(JitScript)
//  StackTypeSet[]          | typeSetOffset_
//  uint32_t[]              | bytecodeTypeMapOffset_
//    (= bytecode type map) |
//
// These offsets are also used to compute numICEntries and numTypeSets.
class alignas(uintptr_t) JitScript final {
  friend class ::JSScript;

  // Allocated space for fallback IC stubs.
  FallbackICStubSpace fallbackStubSpace_ = {};

  // Like JSScript::jitCodeRaw_ but when the script has an IonScript this can
  // point to a separate entry point that skips the argument type checks.
  uint8_t* jitCodeSkipArgCheck_ = nullptr;

  // If non-null, the list of wasm::Modules that contain an optimized call
  // directly to this script.
  js::UniquePtr<Vector<DependentWasmImport>> dependentWasmImports_;

  // Profile string used by the profiler for Baseline Interpreter frames.
  const char* profileString_ = nullptr;

  // Data allocated lazily the first time this script is compiled, inlined, or
  // analyzed by IonBuilder. This is done lazily to improve performance and
  // memory usage as most scripts are never Ion-compiled.
  struct CachedIonData {
    // The freeze constraints added to stack type sets will only directly
    // invalidate the script containing those stack type sets. This Vector
    // contains compilations that inlined this script, so we can invalidate
    // them as well.
    RecompileInfoVector inlinedCompilations_;

    // For functions with a call object, template objects to use for the call
    // object and decl env object (linked via the call object's enclosing
    // scope).
    HeapPtr<EnvironmentObject*> templateEnv = nullptr;

    // Cached control flow graph for IonBuilder. Owned by JitZone::cfgSpace and
    // can be purged by Zone::discardJitCode.
    ControlFlowGraph* controlFlowGraph = nullptr;

    // The total bytecode length of all scripts we inlined when we Ion-compiled
    // this script. 0 if Ion did not compile this script or if we didn't inline
    // anything.
    uint16_t inlinedBytecodeLength = 0;

    // The max inlining depth where we can still inline all functions we inlined
    // when we Ion-compiled this script. This starts as UINT8_MAX, since we have
    // no data yet, and won't affect inlining heuristics in that case. The value
    // is updated when we Ion-compile this script. See makeInliningDecision for
    // more info.
    uint8_t maxInliningDepth = UINT8_MAX;

    // Analysis information based on the script and its bytecode.
    IonBytecodeInfo bytecodeInfo = {};

    CachedIonData(EnvironmentObject* templateEnv, IonBytecodeInfo bytecodeInfo);

    CachedIonData(const CachedIonData&) = delete;
    void operator=(const CachedIonData&) = delete;

    void trace(JSTracer* trc);
  };
  js::UniquePtr<CachedIonData> cachedIonData_;

  // Baseline code for the script. Either nullptr, BaselineDisabledScriptPtr or
  // a valid BaselineScript*.
  BaselineScript* baselineScript_ = nullptr;

  // Ion code for this script. Either nullptr, IonDisabledScriptPtr,
  // IonCompilingScriptPtr or a valid IonScript*.
  IonScript* ionScript_ = nullptr;

  // Offset of the StackTypeSet array.
  uint32_t typeSetOffset_ = 0;

  // Offset of the bytecode type map.
  uint32_t bytecodeTypeMapOffset_ = 0;

  // This field is used to avoid binary searches for the sought entry when
  // bytecode map queries are in linear order.
  uint32_t bytecodeTypeMapHint_ = 0;

  // The size of this allocation.
  uint32_t allocBytes_ = 0;

  struct Flags {
    // Flag set when discarding JIT code to indicate this script is on the stack
    // and type information and JIT code should not be discarded.
    bool active : 1;

    // Generation for type sweeping. If out of sync with the TypeZone's
    // generation, this JitScript needs to be swept.
    bool typesGeneration : 1;

    // Whether freeze constraints for stack type sets have been generated.
    bool hasFreezeConstraints : 1;

    // Flag set if this script has ever been Ion compiled, either directly or
    // inlined into another script. This is cleared when the script's type
    // information or caches are cleared.
    bool ionCompiledOrInlined : 1;
  };
  Flags flags_ = {};  // Zero-initialize flags.

  ICEntry* icEntries() {
    uint8_t* base = reinterpret_cast<uint8_t*>(this);
    return reinterpret_cast<ICEntry*>(base + offsetOfICEntries());
  }

  StackTypeSet* typeArrayDontCheckGeneration() {
    uint8_t* base = reinterpret_cast<uint8_t*>(this);
    return reinterpret_cast<StackTypeSet*>(base + typeSetOffset_);
  }

  uint32_t typesGeneration() const { return uint32_t(flags_.typesGeneration); }
  void setTypesGeneration(uint32_t generation) {
    MOZ_ASSERT(generation <= 1);
    flags_.typesGeneration = generation;
  }

  bool hasCachedIonData() const { return !!cachedIonData_; }

  CachedIonData& cachedIonData() {
    MOZ_ASSERT(hasCachedIonData());
    return *cachedIonData_.get();
  }
  const CachedIonData& cachedIonData() const {
    MOZ_ASSERT(hasCachedIonData());
    return *cachedIonData_.get();
  }

 public:
  JitScript(JSScript* script, uint32_t typeSetOffset,
            uint32_t bytecodeTypeMapOffset, uint32_t allocBytes,
            const char* profileString);

#ifdef DEBUG
  ~JitScript() {
    // The contents of the fallback stub space are removed and freed
    // separately after the next minor GC. See prepareForDestruction.
    MOZ_ASSERT(fallbackStubSpace_.isEmpty());

    // BaselineScript and IonScript must have been destroyed at this point.
    MOZ_ASSERT(!hasBaselineScript());
    MOZ_ASSERT(!hasIonScript());
  }
#endif

  MOZ_MUST_USE bool initICEntriesAndBytecodeTypeMap(JSContext* cx,
                                                    JSScript* script);

  MOZ_MUST_USE bool ensureHasCachedIonData(JSContext* cx, HandleScript script);

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

  void setIonCompiledOrInlined() { flags_.ionCompiledOrInlined = true; }
  void clearIonCompiledOrInlined() { flags_.ionCompiledOrInlined = false; }
  bool ionCompiledOrInlined() const { return flags_.ionCompiledOrInlined; }

  RecompileInfoVector* maybeInlinedCompilations(
      const js::AutoSweepJitScript& sweep) {
    MOZ_ASSERT(sweep.jitScript() == this);
    if (!hasCachedIonData()) {
      return nullptr;
    }
    return &cachedIonData().inlinedCompilations_;
  }
  MOZ_MUST_USE bool addInlinedCompilation(const js::AutoSweepJitScript& sweep,
                                          RecompileInfo info) {
    MOZ_ASSERT(sweep.jitScript() == this);
    auto& inlinedCompilations = cachedIonData().inlinedCompilations_;
    if (!inlinedCompilations.empty() && inlinedCompilations.back() == info) {
      return true;
    }
    return inlinedCompilations.append(info);
  }

  uint32_t numICEntries() const {
    return (typeSetOffset_ - offsetOfICEntries()) / sizeof(ICEntry);
  }
  uint32_t numTypeSets() const {
    return (bytecodeTypeMapOffset_ - typeSetOffset_) / sizeof(StackTypeSet);
  }

  uint32_t* bytecodeTypeMapHint() { return &bytecodeTypeMapHint_; }

  bool active() const { return flags_.active; }
  void setActive() { flags_.active = true; }
  void resetActive() { flags_.active = false; }

  void ensureProfileString(JSContext* cx, JSScript* script);

  const char* profileString() const {
    MOZ_ASSERT(profileString_);
    return profileString_;
  }

  /* Array of type sets for variables and JOF_TYPESET ops. */
  StackTypeSet* typeArray(const js::AutoSweepJitScript& sweep) {
    MOZ_ASSERT(sweep.jitScript() == this);
    return typeArrayDontCheckGeneration();
  }

  uint32_t* bytecodeTypeMap() {
    uint8_t* base = reinterpret_cast<uint8_t*>(this);
    return reinterpret_cast<uint32_t*>(base + bytecodeTypeMapOffset_);
  }

  inline StackTypeSet* thisTypes(const AutoSweepJitScript& sweep,
                                 JSScript* script);
  inline StackTypeSet* argTypes(const AutoSweepJitScript& sweep,
                                JSScript* script, unsigned i);

  static size_t NumTypeSets(JSScript* script);

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

  static void Destroy(Zone* zone, JitScript* script);

  static constexpr size_t offsetOfICEntries() { return sizeof(JitScript); }

  static constexpr size_t offsetOfJitCodeSkipArgCheck() {
    return offsetof(JitScript, jitCodeSkipArgCheck_);
  }
  static size_t offsetOfBaselineScript() {
    return offsetof(JitScript, baselineScript_);
  }
  static size_t offsetOfIonScript() { return offsetof(JitScript, ionScript_); }

#ifdef DEBUG
  void printTypes(JSContext* cx, HandleScript script);
#endif

  void prepareForDestruction(Zone* zone) {
    // When the script contains pointers to nursery things, the store buffer can
    // contain entries that point into the fallback stub space. Since we can
    // destroy scripts outside the context of a GC, this situation could result
    // in us trying to mark invalid store buffer entries.
    //
    // Defer freeing any allocated blocks until after the next minor GC.
    fallbackStubSpace_.freeAllAfterMinorGC(zone);
  }

  FallbackICStubSpace* fallbackStubSpace() { return &fallbackStubSpace_; }

  void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf, size_t* data,
                              size_t* fallbackStubs) const {
    *data += mallocSizeOf(this);

    // |data| already includes the ICStubSpace itself, so use
    // sizeOfExcludingThis.
    *fallbackStubs += fallbackStubSpace_.sizeOfExcludingThis(mallocSizeOf);
  }

  ICEntry& icEntry(size_t index) {
    MOZ_ASSERT(index < numICEntries());
    return icEntries()[index];
  }

  void noteAccessedGetter(uint32_t pcOffset);
  void noteHasDenseAdd(uint32_t pcOffset);

  void trace(JSTracer* trc);
  void purgeOptimizedStubs(JSScript* script);

  ICEntry* interpreterICEntryFromPCOffset(uint32_t pcOffset);

  ICEntry* maybeICEntryFromPCOffset(uint32_t pcOffset);
  ICEntry* maybeICEntryFromPCOffset(uint32_t pcOffset,
                                    ICEntry* prevLookedUpEntry);

  ICEntry& icEntryFromPCOffset(uint32_t pcOffset);
  ICEntry& icEntryFromPCOffset(uint32_t pcOffset, ICEntry* prevLookedUpEntry);

  MOZ_MUST_USE bool addDependentWasmImport(JSContext* cx,
                                           wasm::Instance& instance,
                                           uint32_t idx);
  void removeDependentWasmImport(wasm::Instance& instance, uint32_t idx);
  void unlinkDependentWasmImports();

  size_t allocBytes() const { return allocBytes_; }

  EnvironmentObject* templateEnvironment() const {
    return cachedIonData().templateEnv;
  }

  const ControlFlowGraph* controlFlowGraph() const {
    return cachedIonData().controlFlowGraph;
  }
  void setControlFlowGraph(ControlFlowGraph* controlFlowGraph) {
    MOZ_ASSERT(controlFlowGraph);
    cachedIonData().controlFlowGraph = controlFlowGraph;
  }
  void clearControlFlowGraph() {
    if (hasCachedIonData()) {
      cachedIonData().controlFlowGraph = nullptr;
    }
  }

  bool modifiesArguments() const {
    return cachedIonData().bytecodeInfo.modifiesArguments;
  }
  bool usesEnvironmentChain() const {
    return cachedIonData().bytecodeInfo.usesEnvironmentChain;
  }

  uint8_t maxInliningDepth() const {
    return hasCachedIonData() ? cachedIonData().maxInliningDepth : UINT8_MAX;
  }
  void resetMaxInliningDepth() { cachedIonData().maxInliningDepth = UINT8_MAX; }

  void setMaxInliningDepth(uint32_t depth) {
    MOZ_ASSERT(depth <= UINT8_MAX);
    cachedIonData().maxInliningDepth = depth;
  }

  uint16_t inlinedBytecodeLength() const {
    return hasCachedIonData() ? cachedIonData().inlinedBytecodeLength : 0;
  }
  void setInlinedBytecodeLength(uint32_t len) {
    if (len > UINT16_MAX) {
      len = UINT16_MAX;
    }
    cachedIonData().inlinedBytecodeLength = len;
  }

 private:
  // Methods to set baselineScript_ to a BaselineScript*, nullptr, or
  // BaselineDisabledScriptPtr.
  void setBaselineScriptImpl(JSScript* script, BaselineScript* baselineScript);
  void setBaselineScriptImpl(JSFreeOp* fop, JSScript* script,
                             BaselineScript* baselineScript);

 public:
  // Methods for getting/setting/clearing a BaselineScript*.
  bool hasBaselineScript() const {
    bool res = baselineScript_ && baselineScript_ != BaselineDisabledScriptPtr;
    MOZ_ASSERT_IF(!res, !hasIonScript());
    return res;
  }
  BaselineScript* baselineScript() const {
    MOZ_ASSERT(hasBaselineScript());
    return baselineScript_;
  }
  void setBaselineScript(JSScript* script, BaselineScript* baselineScript) {
    MOZ_ASSERT(!hasBaselineScript());
    setBaselineScriptImpl(script, baselineScript);
    MOZ_ASSERT(hasBaselineScript());
  }
  MOZ_MUST_USE BaselineScript* clearBaselineScript(JSFreeOp* fop,
                                                   JSScript* script) {
    BaselineScript* baseline = baselineScript();
    setBaselineScriptImpl(fop, script, nullptr);
    return baseline;
  }

 private:
  // Methods to set ionScript_ to an IonScript*, nullptr, or one of the special
  // Ion{Disabled,Compiling}ScriptPtr values.
  void setIonScriptImpl(JSFreeOp* fop, JSScript* script, IonScript* ionScript);
  void setIonScriptImpl(JSScript* script, IonScript* ionScript);

 public:
  // Methods for getting/setting/clearing an IonScript*.
  bool hasIonScript() const {
    bool res = ionScript_ && ionScript_ != IonDisabledScriptPtr &&
               ionScript_ != IonCompilingScriptPtr;
    MOZ_ASSERT_IF(res, baselineScript_);
    return res;
  }
  IonScript* ionScript() const {
    MOZ_ASSERT(hasIonScript());
    return ionScript_;
  }
  void setIonScript(JSScript* script, IonScript* ionScript) {
    MOZ_ASSERT(!hasIonScript());
    setIonScriptImpl(script, ionScript);
    MOZ_ASSERT(hasIonScript());
  }
  MOZ_MUST_USE IonScript* clearIonScript(JSFreeOp* fop, JSScript* script) {
    IonScript* ion = ionScript();
    setIonScriptImpl(fop, script, nullptr);
    return ion;
  }

  // Methods for off-thread compilation.
  bool isIonCompilingOffThread() const {
    return ionScript_ == IonCompilingScriptPtr;
  }
  void setIsIonCompilingOffThread(JSScript* script) {
    MOZ_ASSERT(ionScript_ == nullptr);
    setIonScriptImpl(script, IonCompilingScriptPtr);
  }
  void clearIsIonCompilingOffThread(JSScript* script) {
    MOZ_ASSERT(isIonCompilingOffThread());
    setIonScriptImpl(script, nullptr);
  }
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

// Mark JitScripts on the stack as active, so that they are not discarded
// during GC.
void MarkActiveJitScripts(Zone* zone);

#ifdef JS_STRUCTURED_SPEW
void JitSpewBaselineICStats(JSScript* script, const char* dumpReason);
#endif

}  // namespace jit
}  // namespace js

#endif /* jit_JitScript_h */
