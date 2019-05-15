/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineJIT_h
#define jit_BaselineJIT_h

#include "mozilla/MemoryReporting.h"

#include "ds/LifoAlloc.h"
#include "jit/Bailouts.h"
#include "jit/IonCode.h"
#include "jit/shared/Assembler-shared.h"
#include "vm/EnvironmentObject.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/TraceLogging.h"

namespace js {
namespace jit {

class ICEntry;
class ICStub;
class ControlFlowGraph;
class ReturnAddressEntry;

class PCMappingSlotInfo {
  uint8_t slotInfo_;

 public:
  // SlotInfo encoding:
  //  Bits 0 & 1: number of slots at top of stack which are unsynced.
  //  Bits 2 & 3: SlotLocation of top slot value (only relevant if
  //              numUnsynced > 0).
  //  Bits 3 & 4: SlotLocation of next slot value (only relevant if
  //              numUnsynced > 1).
  enum SlotLocation { SlotInR0 = 0, SlotInR1 = 1, SlotIgnore = 3 };

  PCMappingSlotInfo() : slotInfo_(0) {}

  explicit PCMappingSlotInfo(uint8_t slotInfo) : slotInfo_(slotInfo) {}

  static inline bool ValidSlotLocation(SlotLocation loc) {
    return (loc == SlotInR0) || (loc == SlotInR1) || (loc == SlotIgnore);
  }

  inline static PCMappingSlotInfo MakeSlotInfo() {
    return PCMappingSlotInfo(0);
  }

  inline static PCMappingSlotInfo MakeSlotInfo(SlotLocation topSlotLoc) {
    MOZ_ASSERT(ValidSlotLocation(topSlotLoc));
    return PCMappingSlotInfo(1 | (topSlotLoc << 2));
  }

  inline static PCMappingSlotInfo MakeSlotInfo(SlotLocation topSlotLoc,
                                               SlotLocation nextSlotLoc) {
    MOZ_ASSERT(ValidSlotLocation(topSlotLoc));
    MOZ_ASSERT(ValidSlotLocation(nextSlotLoc));
    return PCMappingSlotInfo(2 | (topSlotLoc << 2) | (nextSlotLoc) << 4);
  }

  inline bool isStackSynced() const { return numUnsynced() == 0; }
  inline unsigned numUnsynced() const { return slotInfo_ & 0x3; }
  inline SlotLocation topSlotLocation() const {
    return static_cast<SlotLocation>((slotInfo_ >> 2) & 0x3);
  }
  inline SlotLocation nextSlotLocation() const {
    return static_cast<SlotLocation>((slotInfo_ >> 4) & 0x3);
  }
  inline uint8_t toByte() const { return slotInfo_; }
};

// A CompactBuffer is used to store native code offsets (relative to the
// previous pc) and PCMappingSlotInfo bytes. To allow binary search into this
// table, we maintain a second table of "index" entries. Every X ops, the
// compiler will add an index entry, so that from the index entry to the
// actual native code offset, we have to iterate at most X times.
struct PCMappingIndexEntry {
  // jsbytecode offset.
  uint32_t pcOffset;

  // Native code offset.
  uint32_t nativeOffset;

  // Offset in the CompactBuffer where data for pcOffset starts.
  uint32_t bufferOffset;
};

// Describes a single wasm::ImportExit which jumps (via an import with
// the given index) directly to a BaselineScript or IonScript.
struct DependentWasmImport {
  wasm::Instance* instance;
  size_t importIndex;

  DependentWasmImport(wasm::Instance& instance, size_t importIndex)
      : instance(&instance), importIndex(importIndex) {}
};

// Largest script that the baseline compiler will attempt to compile.
#if defined(JS_CODEGEN_ARM)
// ARM branches can only reach 32MB, and the macroassembler doesn't mitigate
// that limitation. Use a stricter limit on the acceptable script size to
// avoid crashing when branches go out of range.
static constexpr uint32_t BaselineMaxScriptLength = 1000000u;
#else
static constexpr uint32_t BaselineMaxScriptLength = 0x0fffffffu;
#endif

// Limit the locals on a given script so that stack check on baseline frames
// doesn't overflow a uint32_t value.
// (MAX_JSSCRIPT_SLOTS * sizeof(Value)) must fit within a uint32_t.
static constexpr uint32_t BaselineMaxScriptSlots = 0xffffu;

// An entry in the BaselineScript return address table. These entries are used
// to determine the bytecode pc for a return address into Baseline code.
//
// There must be an entry for each location where we can end up calling into
// C++ (directly or via script/trampolines) and C++ can request the current
// bytecode pc (this includes anything that may throw an exception, GC, or walk
// the stack). We currently add entries for each:
//
// * callVM
// * IC
// * DebugTrap (trampoline call)
// * JSOP_RESUME (because this is like a scripted call)
//
// Note: see also BaselineFrame::HAS_OVERRIDE_PC.
class RetAddrEntry {
  // Offset from the start of the JIT code where call instruction is.
  uint32_t returnOffset_;

  // The offset of this bytecode op within the JSScript.
  uint32_t pcOffset_ : 28;

 public:
  enum class Kind : uint32_t {
    // An IC for a JOF_IC op.
    IC,

    // A prologue IC.
    PrologueIC,

    // A callVM for an op.
    CallVM,

    // A callVM not for an op (e.g., in the prologue).
    NonOpCallVM,

    // A callVM for the warmup counter.
    WarmupCounter,

    // A callVM for the over-recursion check on function entry.
    StackCheck,

    // DebugTrapHandler (for debugger breakpoints/stepping).
    DebugTrap,

    // A callVM for Debug{Prologue,AfterYield,Epilogue}.
    DebugPrologue,
    DebugAfterYield,
    DebugEpilogue,

    Invalid
  };

 private:
  // What this entry is for.
  uint32_t kind_ : 4;

 public:
  RetAddrEntry(uint32_t pcOffset, Kind kind, CodeOffset retOffset)
      : returnOffset_(uint32_t(retOffset.offset())), pcOffset_(pcOffset) {
    MOZ_ASSERT(returnOffset_ == retOffset.offset(),
               "retOffset must fit in returnOffset_");

    // The pc offset must fit in at least 28 bits, since we shave off 4 for
    // the Kind enum.
    MOZ_ASSERT(pcOffset_ == pcOffset);
    JS_STATIC_ASSERT(BaselineMaxScriptLength <= (1u << 28) - 1);
    MOZ_ASSERT(pcOffset <= BaselineMaxScriptLength);
    setKind(kind);
  }

  // Set the kind and asserts that it's sane.
  void setKind(Kind kind) {
    MOZ_ASSERT(kind < Kind::Invalid);
    kind_ = uint32_t(kind);
    MOZ_ASSERT(this->kind() == kind);
  }

  CodeOffset returnOffset() const { return CodeOffset(returnOffset_); }

  uint32_t pcOffset() const { return pcOffset_; }

  jsbytecode* pc(JSScript* script) const {
    return script->offsetToPC(pcOffset_);
  }

  Kind kind() const {
    MOZ_ASSERT(kind_ < uint32_t(Kind::Invalid));
    return Kind(kind_);
  }
};

struct BaselineScript final {
 private:
  // Code pointer containing the actual method.
  HeapPtr<JitCode*> method_ = nullptr;

  // For functions with a call object, template objects to use for the call
  // object and decl env object (linked via the call object's enclosing
  // scope).
  HeapPtr<EnvironmentObject*> templateEnv_ = nullptr;

  // If non-null, the list of wasm::Modules that contain an optimized call
  // directly to this script.
  Vector<DependentWasmImport>* dependentWasmImports_ = nullptr;

  // Early Ion bailouts will enter at this address. This is after frame
  // construction and before environment chain is initialized.
  uint32_t bailoutPrologueOffset_;

  // Baseline Interpreter can enter Baseline Compiler code at this address. This
  // is right after the warm-up counter check in the prologue.
  uint32_t warmUpCheckPrologueOffset_;

  // Baseline Debug OSR during prologue will enter at this address. This is
  // right after where a debug prologue VM call would have returned.
  uint32_t debugOsrPrologueOffset_;

  // Baseline Debug OSR during epilogue will enter at this address. This is
  // right after where a debug epilogue VM call would have returned.
  uint32_t debugOsrEpilogueOffset_;

  // The offsets for the toggledJump instructions for profiler instrumentation.
  uint32_t profilerEnterToggleOffset_;
  uint32_t profilerExitToggleOffset_;

  // The offsets and event used for Tracelogger toggling.
#ifdef JS_TRACE_LOGGING
#  ifdef DEBUG
  bool traceLoggerScriptsEnabled_ = false;
  bool traceLoggerEngineEnabled_ = false;
#  endif
  TraceLoggerEvent traceLoggerScriptEvent_ = {};
#endif

 public:
  enum Flag {
    // (1 << 0) and (1 << 1) are unused.

    // Flag set when the script contains any writes to its on-stack
    // (rather than call object stored) arguments.
    MODIFIES_ARGUMENTS = 1 << 2,

    // Flag set when compiled for use with Debugger. Handles various
    // Debugger hooks and compiles toggled calls for traps.
    HAS_DEBUG_INSTRUMENTATION = 1 << 3,

    // Flag set if this script has ever been Ion compiled, either directly
    // or inlined into another script. This is cleared when the script's
    // type information or caches are cleared.
    ION_COMPILED_OR_INLINED = 1 << 4,

    // Flag is set if this script has profiling instrumentation turned on.
    PROFILER_INSTRUMENTATION_ON = 1 << 5,

    // Whether this script uses its environment chain. This is currently
    // determined by the BytecodeAnalysis and cached on the BaselineScript
    // for IonBuilder.
    USES_ENVIRONMENT_CHAIN = 1 << 6,
  };

 private:
  uint32_t flags_ = 0;

 private:
  void trace(JSTracer* trc);

  uint32_t retAddrEntriesOffset_ = 0;
  uint32_t retAddrEntries_ = 0;

  uint32_t pcMappingIndexOffset_ = 0;
  uint32_t pcMappingIndexEntries_ = 0;

  uint32_t pcMappingOffset_ = 0;
  uint32_t pcMappingSize_ = 0;

  // We store the native code address corresponding to each bytecode offset in
  // the script's resumeOffsets list.
  uint32_t resumeEntriesOffset_ = 0;

  // By default tracelogger is disabled. Therefore we disable the logging code
  // by default. We store the offsets we must patch to enable the logging.
  uint32_t traceLoggerToggleOffsetsOffset_ = 0;
  uint32_t numTraceLoggerToggleOffsets_ = 0;

  // The total bytecode length of all scripts we inlined when we Ion-compiled
  // this script. 0 if Ion did not compile this script or if we didn't inline
  // anything.
  uint16_t inlinedBytecodeLength_ = 0;

  // The max inlining depth where we can still inline all functions we inlined
  // when we Ion-compiled this script. This starts as UINT8_MAX, since we have
  // no data yet, and won't affect inlining heuristics in that case. The value
  // is updated when we Ion-compile this script. See makeInliningDecision for
  // more info.
  uint8_t maxInliningDepth_ = UINT8_MAX;

  // An ion compilation that is ready, but isn't linked yet.
  IonBuilder* pendingBuilder_ = nullptr;

  ControlFlowGraph* controlFlowGraph_ = nullptr;

  // Use BaselineScript::New to create new instances. It will properly
  // allocate trailing objects.
  BaselineScript(uint32_t bailoutPrologueOffset,
                 uint32_t warmUpCheckPrologueOffset,
                 uint32_t debugOsrPrologueOffset,
                 uint32_t debugOsrEpilogueOffset,
                 uint32_t profilerEnterToggleOffset,
                 uint32_t profilerExitToggleOffset)
      : bailoutPrologueOffset_(bailoutPrologueOffset),
        warmUpCheckPrologueOffset_(warmUpCheckPrologueOffset),
        debugOsrPrologueOffset_(debugOsrPrologueOffset),
        debugOsrEpilogueOffset_(debugOsrEpilogueOffset),
        profilerEnterToggleOffset_(profilerEnterToggleOffset),
        profilerExitToggleOffset_(profilerExitToggleOffset) {}

 public:
  static BaselineScript* New(
      JSScript* jsscript, uint32_t bailoutPrologueOffset,
      uint32_t warmUpCheckPrologueOffset, uint32_t debugOsrPrologueOffset,
      uint32_t debugOsrEpilogueOffset, uint32_t profilerEnterToggleOffset,
      uint32_t profilerExitToggleOffset, size_t retAddrEntries,
      size_t pcMappingIndexEntries, size_t pcMappingSize, size_t resumeEntries,
      size_t traceLoggerToggleOffsetEntries);

  static void Trace(JSTracer* trc, BaselineScript* script);
  static void Destroy(FreeOp* fop, BaselineScript* script);

  static inline size_t offsetOfMethod() {
    return offsetof(BaselineScript, method_);
  }

  void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                              size_t* data) const {
    *data += mallocSizeOf(this);
  }

  void setModifiesArguments() { flags_ |= MODIFIES_ARGUMENTS; }
  bool modifiesArguments() { return flags_ & MODIFIES_ARGUMENTS; }

  void setHasDebugInstrumentation() { flags_ |= HAS_DEBUG_INSTRUMENTATION; }
  bool hasDebugInstrumentation() const {
    return flags_ & HAS_DEBUG_INSTRUMENTATION;
  }

  void setIonCompiledOrInlined() { flags_ |= ION_COMPILED_OR_INLINED; }
  void clearIonCompiledOrInlined() { flags_ &= ~ION_COMPILED_OR_INLINED; }
  bool ionCompiledOrInlined() const { return flags_ & ION_COMPILED_OR_INLINED; }

  void setUsesEnvironmentChain() { flags_ |= USES_ENVIRONMENT_CHAIN; }
  bool usesEnvironmentChain() const { return flags_ & USES_ENVIRONMENT_CHAIN; }

  uint8_t* bailoutPrologueEntryAddr() const {
    return method_->raw() + bailoutPrologueOffset_;
  }
  uint8_t* warmUpCheckPrologueAddr() const {
    return method_->raw() + warmUpCheckPrologueOffset_;
  }
  uint8_t* debugOsrPrologueEntryAddr() const {
    return method_->raw() + debugOsrPrologueOffset_;
  }
  uint8_t* debugOsrEpilogueEntryAddr() const {
    return method_->raw() + debugOsrEpilogueOffset_;
  }

  RetAddrEntry* retAddrEntryList() {
    return (RetAddrEntry*)(reinterpret_cast<uint8_t*>(this) +
                           retAddrEntriesOffset_);
  }
  uint8_t** resumeEntryList() {
    return (uint8_t**)(reinterpret_cast<uint8_t*>(this) + resumeEntriesOffset_);
  }
  PCMappingIndexEntry* pcMappingIndexEntryList() {
    return (PCMappingIndexEntry*)(reinterpret_cast<uint8_t*>(this) +
                                  pcMappingIndexOffset_);
  }
  uint8_t* pcMappingData() {
    return reinterpret_cast<uint8_t*>(this) + pcMappingOffset_;
  }

  JitCode* method() const { return method_; }
  void setMethod(JitCode* code) {
    MOZ_ASSERT(!method_);
    method_ = code;
  }

  EnvironmentObject* templateEnvironment() const { return templateEnv_; }
  void setTemplateEnvironment(EnvironmentObject* templateEnv) {
    MOZ_ASSERT(!templateEnv_);
    templateEnv_ = templateEnv;
  }

  bool containsCodeAddress(uint8_t* addr) const {
    return method()->raw() <= addr &&
           addr <= method()->raw() + method()->instructionsSize();
  }

  uint8_t* returnAddressForEntry(const RetAddrEntry& ent);

  RetAddrEntry& retAddrEntry(size_t index);
  RetAddrEntry& retAddrEntryFromPCOffset(uint32_t pcOffset,
                                         RetAddrEntry::Kind kind);
  RetAddrEntry& prologueRetAddrEntry(RetAddrEntry::Kind kind);
  RetAddrEntry& retAddrEntryFromReturnOffset(CodeOffset returnOffset);
  RetAddrEntry& retAddrEntryFromReturnAddress(uint8_t* returnAddr);

  size_t numRetAddrEntries() const { return retAddrEntries_; }

  void copyRetAddrEntries(JSScript* script, const RetAddrEntry* entries);

  // Copy resumeOffsets list from |script| and convert the pcOffsets
  // to native addresses in the Baseline code.
  void computeResumeNativeOffsets(JSScript* script);

  PCMappingIndexEntry& pcMappingIndexEntry(size_t index);
  CompactBufferReader pcMappingReader(size_t indexEntry);

  size_t numPCMappingIndexEntries() const { return pcMappingIndexEntries_; }

  void copyPCMappingIndexEntries(const PCMappingIndexEntry* entries);
  void copyPCMappingEntries(const CompactBufferWriter& entries);

  // Baseline JIT may not generate code for unreachable bytecode which
  // results in mapping returning nullptr.
  uint8_t* maybeNativeCodeForPC(JSScript* script, jsbytecode* pc,
                                PCMappingSlotInfo* slotInfo);
  uint8_t* nativeCodeForPC(JSScript* script, jsbytecode* pc,
                           PCMappingSlotInfo* slotInfo) {
    uint8_t* native = maybeNativeCodeForPC(script, pc, slotInfo);
    MOZ_ASSERT(native);
    return native;
  }

  // Return the bytecode offset for a given native code address. Be careful
  // when using this method: we don't emit code for some bytecode ops, so
  // the result may not be accurate.
  jsbytecode* approximatePcForNativeAddress(JSScript* script,
                                            uint8_t* nativeAddress);

  MOZ_MUST_USE bool addDependentWasmImport(JSContext* cx,
                                           wasm::Instance& instance,
                                           uint32_t idx);
  void removeDependentWasmImport(wasm::Instance& instance, uint32_t idx);
  void unlinkDependentWasmImports(FreeOp* fop);
  void clearDependentWasmImports();

  // Toggle debug traps (used for breakpoints and step mode) in the script.
  // If |pc| is nullptr, toggle traps for all ops in the script. Else, only
  // toggle traps at |pc|.
  void toggleDebugTraps(JSScript* script, jsbytecode* pc);

  void toggleProfilerInstrumentation(bool enable);
  bool isProfilerInstrumentationOn() const {
    return flags_ & PROFILER_INSTRUMENTATION_ON;
  }

#ifdef JS_TRACE_LOGGING
  void initTraceLogger(JSScript* script, const Vector<CodeOffset>& offsets);
  void toggleTraceLoggerScripts(JSScript* script, bool enable);
  void toggleTraceLoggerEngine(bool enable);

  static size_t offsetOfTraceLoggerScriptEvent() {
    return offsetof(BaselineScript, traceLoggerScriptEvent_);
  }

  uint32_t* traceLoggerToggleOffsets() {
    MOZ_ASSERT(traceLoggerToggleOffsetsOffset_);
    return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) +
                                       traceLoggerToggleOffsetsOffset_);
  }
#endif

  static size_t offsetOfFlags() { return offsetof(BaselineScript, flags_); }
  static size_t offsetOfResumeEntriesOffset() {
    return offsetof(BaselineScript, resumeEntriesOffset_);
  }

  static void writeBarrierPre(Zone* zone, BaselineScript* script);

  uint8_t maxInliningDepth() const { return maxInliningDepth_; }
  void setMaxInliningDepth(uint32_t depth) {
    MOZ_ASSERT(depth <= UINT8_MAX);
    maxInliningDepth_ = depth;
  }
  void resetMaxInliningDepth() { maxInliningDepth_ = UINT8_MAX; }

  uint16_t inlinedBytecodeLength() const { return inlinedBytecodeLength_; }
  void setInlinedBytecodeLength(uint32_t len) {
    if (len > UINT16_MAX) {
      len = UINT16_MAX;
    }
    inlinedBytecodeLength_ = len;
  }

  bool hasPendingIonBuilder() const { return !!pendingBuilder_; }

  js::jit::IonBuilder* pendingIonBuilder() {
    MOZ_ASSERT(hasPendingIonBuilder());
    return pendingBuilder_;
  }
  void setPendingIonBuilder(JSRuntime* rt, JSScript* script,
                            js::jit::IonBuilder* builder) {
    MOZ_ASSERT(script->baselineScript() == this);
    MOZ_ASSERT(!builder || !hasPendingIonBuilder());

    if (script->isIonCompilingOffThread()) {
      script->setIonScript(rt, ION_PENDING_SCRIPT);
    }

    pendingBuilder_ = builder;

    // lazy linking cannot happen during asmjs to ion.
    clearDependentWasmImports();

    script->updateJitCodeRaw(rt);
  }
  void removePendingIonBuilder(JSRuntime* rt, JSScript* script) {
    setPendingIonBuilder(rt, script, nullptr);
    if (script->maybeIonScript() == ION_PENDING_SCRIPT) {
      script->setIonScript(rt, nullptr);
    }
  }

  const ControlFlowGraph* controlFlowGraph() const { return controlFlowGraph_; }

  void setControlFlowGraph(ControlFlowGraph* controlFlowGraph) {
    controlFlowGraph_ = controlFlowGraph;
  }
};
static_assert(
    sizeof(BaselineScript) % sizeof(uintptr_t) == 0,
    "The data attached to the script must be aligned for fast JIT access.");

inline bool IsBaselineEnabled(JSContext* cx) {
#ifdef JS_CODEGEN_NONE
  return false;
#else
  return cx->options().baseline() && cx->runtime()->jitSupportsFloatingPoint;
#endif
}

enum class BaselineTier { Interpreter, Compiler };

template <BaselineTier Tier>
MethodStatus CanEnterBaselineMethod(JSContext* cx, RunState& state);

template <BaselineTier Tier>
MethodStatus CanEnterBaselineAtBranch(JSContext* cx, InterpreterFrame* fp);

JitExecStatus EnterBaselineAtBranch(JSContext* cx, InterpreterFrame* fp,
                                    jsbytecode* pc);

// Called by the Baseline Interpreter to compile a script for the Baseline JIT.
// |res| is set to the native code address in the BaselineScript to jump to, or
// nullptr if we were unable to compile this script.
bool BaselineCompileFromBaselineInterpreter(JSContext* cx, BaselineFrame* frame,
                                            uint8_t** res);

void FinishDiscardBaselineScript(FreeOp* fop, JSScript* script);

void AddSizeOfBaselineData(JSScript* script, mozilla::MallocSizeOf mallocSizeOf,
                           size_t* data, size_t* fallbackStubs);

void ToggleBaselineProfiling(JSRuntime* runtime, bool enable);

void ToggleBaselineTraceLoggerScripts(JSRuntime* runtime, bool enable);
void ToggleBaselineTraceLoggerEngine(JSRuntime* runtime, bool enable);

struct BaselineBailoutInfo {
  // Pointer into the current C stack, where overwriting will start.
  uint8_t* incomingStack;

  // The top and bottom heapspace addresses of the reconstructed stack
  // which will be copied to the bottom.
  uint8_t* copyStackTop;
  uint8_t* copyStackBottom;

  // Fields to store the top-of-stack baseline values that are held
  // in registers.  The setR0 and setR1 fields are flags indicating
  // whether each one is initialized.
  uint32_t setR0;
  Value valueR0;
  uint32_t setR1;
  Value valueR1;

  // The value of the frame pointer register on resume.
  void* resumeFramePtr;

  // The native code address to resume into.
  void* resumeAddr;

  // The bytecode pc where we will resume.
  jsbytecode* resumePC;

  // The bytecode pc of try block and fault block.
  jsbytecode* tryPC;
  jsbytecode* faultPC;

  // If resuming into a TypeMonitor IC chain, this field holds the
  // address of the first stub in that chain.  If this field is
  // set, then the actual jitcode resumed into is the jitcode for
  // the first stub, not the resumeAddr above.  The resumeAddr
  // above, in this case, is pushed onto the stack so that the
  // TypeMonitor chain can tail-return into the main jitcode when done.
  ICStub* monitorStub;

  // Number of baseline frames to push on the stack.
  uint32_t numFrames;

  // If Ion bailed out on a global script before it could perform the global
  // declaration conflicts check. In such cases the baseline script is
  // resumed at the first pc instead of the prologue, so an extra flag is
  // needed to perform the check.
  bool checkGlobalDeclarationConflicts;

  // The bailout kind.
  BailoutKind bailoutKind;
};

MOZ_MUST_USE bool BailoutIonToBaseline(
    JSContext* cx, JitActivation* activation, const JSJitFrameIter& iter,
    bool invalidate, BaselineBailoutInfo** bailoutInfo,
    const ExceptionBailoutInfo* exceptionInfo);

// Mark TypeScripts on the stack as active, so that they are not discarded
// during GC.
void MarkActiveTypeScripts(Zone* zone);

MethodStatus BaselineCompile(JSContext* cx, JSScript* script,
                             bool forceDebugInstrumentation = false);

#ifdef JS_STRUCTURED_SPEW
void JitSpewBaselineICStats(JSScript* script, const char* dumpReason);
#endif

static const unsigned BASELINE_MAX_ARGS_LENGTH = 20000;

// Class storing the generated Baseline Interpreter code for the runtime.
class BaselineInterpreter {
  // The interpreter code.
  JitCode* code_ = nullptr;

  // Offset of the code to start interpreting a bytecode op.
  uint32_t interpretOpOffset_ = 0;

  // The offsets for the toggledJump instructions for profiler instrumentation.
  uint32_t profilerEnterToggleOffset_ = 0;
  uint32_t profilerExitToggleOffset_ = 0;

  // The offset for the toggledJump instruction for the debugger's
  // IsDebuggeeCheck code in the prologue.
  uint32_t debuggeeCheckOffset_ = 0;

  // Offsets of toggled calls to the DebugTrapHandler trampoline (for
  // breakpoints and stepping).
  using CodeOffsetVector = Vector<uint32_t, 0, SystemAllocPolicy>;
  CodeOffsetVector debugTrapOffsets_;

  // Offsets of toggled jumps for code coverage.
  CodeOffsetVector codeCoverageOffsets_;

 public:
  BaselineInterpreter() = default;

  BaselineInterpreter(const BaselineInterpreter&) = delete;
  void operator=(const BaselineInterpreter&) = delete;

  void init(JitCode* code, uint32_t interpretOpOffset,
            uint32_t profilerEnterToggleOffset,
            uint32_t profilerExitToggleOffset, uint32_t debuggeeCheckOffset,
            CodeOffsetVector&& debugTrapOffsets,
            CodeOffsetVector&& codeCoverageOffsets);

  uint8_t* codeRaw() const { return code_->raw(); }

  TrampolinePtr interpretOpAddr() const {
    return TrampolinePtr(codeRaw() + interpretOpOffset_);
  }

  void toggleProfilerInstrumentation(bool enable);
  void toggleDebuggerInstrumentation(bool enable);

  void toggleCodeCoverageInstrumentationUnchecked(bool enable);
  void toggleCodeCoverageInstrumentation(bool enable);
};

MOZ_MUST_USE bool GenerateBaselineInterpreter(JSContext* cx,
                                              BaselineInterpreter& interpreter);

}  // namespace jit
}  // namespace js

namespace JS {

template <>
struct DeletePolicy<js::jit::BaselineScript> {
  explicit DeletePolicy(JSRuntime* rt) : rt_(rt) {}
  void operator()(const js::jit::BaselineScript* script);

 private:
  JSRuntime* rt_;
};

}  // namespace JS

#endif /* jit_BaselineJIT_h */
