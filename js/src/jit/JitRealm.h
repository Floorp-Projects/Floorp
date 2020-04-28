/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitRealm_h
#define jit_JitRealm_h

#include "mozilla/Array.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/MemoryReporting.h"

#include <utility>

#include "builtin/TypedObject.h"
#include "jit/BaselineICList.h"
#include "jit/BaselineJIT.h"
#include "jit/CompileInfo.h"
#include "jit/ICStubSpace.h"
#include "jit/IonCode.h"
#include "jit/JitFrames.h"
#include "jit/shared/Assembler-shared.h"
#include "js/GCHashTable.h"
#include "js/Value.h"
#include "vm/Stack.h"

namespace js {
namespace jit {

class FrameSizeClass;
struct VMFunctionData;

enum class TailCallVMFunctionId;
enum class VMFunctionId;

struct EnterJitData {
  explicit EnterJitData(JSContext* cx)
      : jitcode(nullptr),
        osrFrame(nullptr),
        calleeToken(nullptr),
        maxArgv(nullptr),
        maxArgc(0),
        numActualArgs(0),
        osrNumStackValues(0),
        envChain(cx),
        result(cx),
        constructing(false) {}

  uint8_t* jitcode;
  InterpreterFrame* osrFrame;

  void* calleeToken;

  Value* maxArgv;
  unsigned maxArgc;
  unsigned numActualArgs;
  unsigned osrNumStackValues;

  RootedObject envChain;
  RootedValue result;

  bool constructing;
};

enum class BaselineICFallbackKind {
#define DEF_ENUM_KIND(kind) kind,
  IC_BASELINE_FALLBACK_CODE_KIND_LIST(DEF_ENUM_KIND)
#undef DEF_ENUM_KIND
      Count
};

enum class BailoutReturnKind {
  GetProp,
  GetPropSuper,
  SetProp,
  GetElem,
  GetElemSuper,
  Call,
  New,
  Count
};

// Class storing code and offsets for all Baseline IC fallback trampolines. This
// is stored in JitRuntime and generated when creating the JitRuntime.
class BaselineICFallbackCode {
  JitCode* code_ = nullptr;
  using OffsetArray =
      mozilla::EnumeratedArray<BaselineICFallbackKind,
                               BaselineICFallbackKind::Count, uint32_t>;
  OffsetArray offsets_ = {};

  // Keep track of offset into various baseline stubs' code at return
  // point from called script.
  using BailoutReturnArray =
      mozilla::EnumeratedArray<BailoutReturnKind, BailoutReturnKind::Count,
                               uint32_t>;
  BailoutReturnArray bailoutReturnOffsets_ = {};

 public:
  BaselineICFallbackCode() = default;
  BaselineICFallbackCode(const BaselineICFallbackCode&) = delete;
  void operator=(const BaselineICFallbackCode&) = delete;

  void initOffset(BaselineICFallbackKind kind, uint32_t offset) {
    offsets_[kind] = offset;
  }
  void initCode(JitCode* code) { code_ = code; }
  void initBailoutReturnOffset(BailoutReturnKind kind, uint32_t offset) {
    bailoutReturnOffsets_[kind] = offset;
  }
  TrampolinePtr addr(BaselineICFallbackKind kind) const {
    return TrampolinePtr(code_->raw() + offsets_[kind]);
  }
  uint8_t* bailoutReturnAddr(BailoutReturnKind kind) const {
    return code_->raw() + bailoutReturnOffsets_[kind];
  }
};

enum class DebugTrapHandlerKind { Interpreter, Compiler, Count };

using EnterJitCode = void (*)(void*, unsigned int, Value*, InterpreterFrame*,
                              CalleeToken, JSObject*, size_t, Value*);

class JitcodeGlobalTable;

class JitRuntime {
 private:
  friend class JitRealm;

  MainThreadData<uint64_t> nextCompilationId_{0};

  // Buffer for OSR from baseline to Ion. To avoid holding on to this for too
  // long it's also freed in EnterBaseline and EnterJit (after returning from
  // JIT code).
  MainThreadData<js::UniquePtr<uint8_t>> ionOsrTempData_{nullptr};

  // Shared exception-handler tail.
  WriteOnceData<uint32_t> exceptionTailOffset_{0};

  // Shared post-bailout-handler tail.
  WriteOnceData<uint32_t> bailoutTailOffset_{0};

  // Shared profiler exit frame tail.
  WriteOnceData<uint32_t> profilerExitFrameTailOffset_{0};

  // Trampoline for entering JIT code.
  WriteOnceData<uint32_t> enterJITOffset_{0};

  // Vector mapping frame class sizes to bailout tables.
  struct BailoutTable {
    uint32_t startOffset;
    uint32_t size;
    BailoutTable(uint32_t startOffset, uint32_t size)
        : startOffset(startOffset), size(size) {}
  };
  typedef Vector<BailoutTable, 4, SystemAllocPolicy> BailoutTableVector;
  WriteOnceData<BailoutTableVector> bailoutTables_;

  // Generic bailout table; used if the bailout table overflows.
  WriteOnceData<uint32_t> bailoutHandlerOffset_{0};

  // Argument-rectifying thunk, in the case of insufficient arguments passed
  // to a function call site.
  WriteOnceData<uint32_t> argumentsRectifierOffset_{0};
  WriteOnceData<uint32_t> argumentsRectifierReturnOffset_{0};

  // Thunk that invalides an (Ion compiled) caller on the Ion stack.
  WriteOnceData<uint32_t> invalidatorOffset_{0};

  // Thunk that calls the GC pre barrier.
  WriteOnceData<uint32_t> valuePreBarrierOffset_{0};
  WriteOnceData<uint32_t> stringPreBarrierOffset_{0};
  WriteOnceData<uint32_t> objectPreBarrierOffset_{0};
  WriteOnceData<uint32_t> shapePreBarrierOffset_{0};
  WriteOnceData<uint32_t> objectGroupPreBarrierOffset_{0};

  // Thunk to call malloc/free.
  WriteOnceData<uint32_t> freeStubOffset_{0};

  // Thunk called to finish compilation of an IonScript.
  WriteOnceData<uint32_t> lazyLinkStubOffset_{0};

  // Thunk to enter the interpreter from JIT code.
  WriteOnceData<uint32_t> interpreterStubOffset_{0};

  // Thunk to convert the value in R0 to int32 if it's a double.
  // Note: this stub treats -0 as +0 and may clobber R1.scratchReg().
  WriteOnceData<uint32_t> doubleToInt32ValueStubOffset_{0};

  // Thunk used by the debugger for breakpoint and step mode.
  mozilla::EnumeratedArray<DebugTrapHandlerKind, DebugTrapHandlerKind::Count,
                           WriteOnceData<JitCode*>>
      debugTrapHandlers_;

  // BaselineInterpreter state.
  BaselineInterpreter baselineInterpreter_;

  // Code for trampolines and VMFunction wrappers.
  WriteOnceData<JitCode*> trampolineCode_{nullptr};

  // Map VMFunction addresses to the offset of the wrapper in
  // trampolineCode_.
  using VMWrapperMap = HashMap<const VMFunction*, uint32_t, VMFunction>;
  WriteOnceData<VMWrapperMap*> functionWrappers_{nullptr};

  // Maps VMFunctionId to the offset of the wrapper code in trampolineCode_.
  using VMWrapperOffsets = Vector<uint32_t, 0, SystemAllocPolicy>;
  VMWrapperOffsets functionWrapperOffsets_;

  // Maps TailCallVMFunctionId to the offset of the wrapper code in
  // trampolineCode_.
  VMWrapperOffsets tailCallFunctionWrapperOffsets_;

  MainThreadData<BaselineICFallbackCode> baselineICFallbackCode_;

  // Global table of jitcode native address => bytecode address mappings.
  UnprotectedData<JitcodeGlobalTable*> jitcodeGlobalTable_{nullptr};

#ifdef DEBUG
  // The number of possible bailing places encountered before forcefully bailing
  // in that place if the counter reaches zero. Note that zero also means
  // inactive.
  MainThreadData<uint32_t> ionBailAfterCounter_{0};

  // Whether the bailAfter mechanism is enabled. Used to avoid generating the
  // Ion code instrumentation for ionBailAfterCounter_ if the testing function
  // isn't used.
  MainThreadData<bool> ionBailAfterEnabled_{false};
#endif

  // Number of Ion compilations which were finished off thread and are
  // waiting to be lazily linked. This is only set while holding the helper
  // thread state lock, but may be read from at other times.
  typedef mozilla::Atomic<size_t, mozilla::SequentiallyConsistent>
      NumFinishedOffThreadTasksType;
  NumFinishedOffThreadTasksType numFinishedOffThreadTasks_{0};

  // List of Ion compilation waiting to get linked.
  using IonCompileTaskList = mozilla::LinkedList<js::jit::IonCompileTask>;
  MainThreadData<IonCompileTaskList> ionLazyLinkList_;
  MainThreadData<size_t> ionLazyLinkListSize_{0};

  // Counter used to help dismbiguate stubs in CacheIR
  MainThreadData<uint64_t> disambiguationId_{0};

#ifdef DEBUG
  // Flag that can be set from JIT code to indicate it's invalid to call
  // arbitrary JS code in a particular region. This is checked in RunScript.
  MainThreadData<uint32_t> disallowArbitraryCode_{false};
#endif

  bool generateTrampolines(JSContext* cx);
  bool generateBaselineICFallbackCode(JSContext* cx);

  void generateLazyLinkStub(MacroAssembler& masm);
  void generateInterpreterStub(MacroAssembler& masm);
  void generateDoubleToInt32ValueStub(MacroAssembler& masm);
  void generateProfilerExitFrameTailStub(MacroAssembler& masm,
                                         Label* profilerExitTail);
  void generateExceptionTailStub(MacroAssembler& masm, void* handler,
                                 Label* profilerExitTail);
  void generateBailoutTailStub(MacroAssembler& masm, Label* bailoutTail);
  void generateEnterJIT(JSContext* cx, MacroAssembler& masm);
  void generateArgumentsRectifier(MacroAssembler& masm);
  BailoutTable generateBailoutTable(MacroAssembler& masm, Label* bailoutTail,
                                    uint32_t frameClass);
  void generateBailoutHandler(MacroAssembler& masm, Label* bailoutTail);
  void generateInvalidator(MacroAssembler& masm, Label* bailoutTail);
  uint32_t generatePreBarrier(JSContext* cx, MacroAssembler& masm,
                              MIRType type);
  void generateFreeStub(MacroAssembler& masm);
  JitCode* generateDebugTrapHandler(JSContext* cx, DebugTrapHandlerKind kind);

  bool generateVMWrapper(JSContext* cx, MacroAssembler& masm,
                         const VMFunctionData& f, void* nativeFun,
                         uint32_t* wrapperOffset);

  template <typename IdT>
  bool generateVMWrappers(JSContext* cx, MacroAssembler& masm,
                          VMWrapperOffsets& offsets);
  bool generateVMWrappers(JSContext* cx, MacroAssembler& masm);

  bool generateTLEventVM(MacroAssembler& masm, const VMFunctionData& f,
                         bool enter);

  inline bool generateTLEnterVM(MacroAssembler& masm, const VMFunctionData& f) {
    return generateTLEventVM(masm, f, /* enter = */ true);
  }
  inline bool generateTLExitVM(MacroAssembler& masm, const VMFunctionData& f) {
    return generateTLEventVM(masm, f, /* enter = */ false);
  }

  uint32_t startTrampolineCode(MacroAssembler& masm);

  TrampolinePtr trampolineCode(uint32_t offset) const {
    MOZ_ASSERT(offset > 0);
    MOZ_ASSERT(offset < trampolineCode_->instructionsSize());
    return TrampolinePtr(trampolineCode_->raw() + offset);
  }

 public:
  JitRuntime() = default;
  ~JitRuntime();
  MOZ_MUST_USE bool initialize(JSContext* cx);

  static void Trace(JSTracer* trc, const js::AutoAccessAtomsZone& access);
  static MOZ_MUST_USE bool MarkJitcodeGlobalTableIteratively(GCMarker* marker);
  static void TraceWeakJitcodeGlobalTable(JSRuntime* rt, JSTracer* trc);

  const BaselineICFallbackCode& baselineICFallbackCode() const {
    return baselineICFallbackCode_.ref();
  }

  IonCompilationId nextCompilationId() {
    return IonCompilationId(nextCompilationId_++);
  }

#ifdef DEBUG
  bool disallowArbitraryCode() const { return disallowArbitraryCode_; }
  void clearDisallowArbitraryCode() { disallowArbitraryCode_ = false; }
  const void* addressOfDisallowArbitraryCode() const {
    return &disallowArbitraryCode_.refNoCheck();
  }
#endif

  uint8_t* allocateIonOsrTempData(size_t size);
  void freeIonOsrTempData();

  TrampolinePtr getVMWrapper(const VMFunction& f) const;

  TrampolinePtr getVMWrapper(VMFunctionId funId) const {
    MOZ_ASSERT(trampolineCode_);
    return trampolineCode(functionWrapperOffsets_[size_t(funId)]);
  }
  TrampolinePtr getVMWrapper(TailCallVMFunctionId funId) const {
    MOZ_ASSERT(trampolineCode_);
    return trampolineCode(tailCallFunctionWrapperOffsets_[size_t(funId)]);
  }

  JitCode* debugTrapHandler(JSContext* cx, DebugTrapHandlerKind kind);

  BaselineInterpreter& baselineInterpreter() { return baselineInterpreter_; }

  TrampolinePtr getGenericBailoutHandler() const {
    return trampolineCode(bailoutHandlerOffset_);
  }

  TrampolinePtr getExceptionTail() const {
    return trampolineCode(exceptionTailOffset_);
  }

  TrampolinePtr getBailoutTail() const {
    return trampolineCode(bailoutTailOffset_);
  }

  TrampolinePtr getProfilerExitFrameTail() const {
    return trampolineCode(profilerExitFrameTailOffset_);
  }

  TrampolinePtr getBailoutTable(const FrameSizeClass& frameClass) const;
  uint32_t getBailoutTableSize(const FrameSizeClass& frameClass) const;

  TrampolinePtr getArgumentsRectifier() const {
    return trampolineCode(argumentsRectifierOffset_);
  }

  TrampolinePtr getArgumentsRectifierReturnAddr() const {
    return trampolineCode(argumentsRectifierReturnOffset_);
  }

  TrampolinePtr getInvalidationThunk() const {
    return trampolineCode(invalidatorOffset_);
  }

  EnterJitCode enterJit() const {
    return JS_DATA_TO_FUNC_PTR(EnterJitCode,
                               trampolineCode(enterJITOffset_).value);
  }

  TrampolinePtr preBarrier(MIRType type) const {
    switch (type) {
      case MIRType::Value:
        return trampolineCode(valuePreBarrierOffset_);
      case MIRType::String:
        return trampolineCode(stringPreBarrierOffset_);
      case MIRType::Object:
        return trampolineCode(objectPreBarrierOffset_);
      case MIRType::Shape:
        return trampolineCode(shapePreBarrierOffset_);
      case MIRType::ObjectGroup:
        return trampolineCode(objectGroupPreBarrierOffset_);
      default:
        MOZ_CRASH();
    }
  }

  TrampolinePtr freeStub() const { return trampolineCode(freeStubOffset_); }

  TrampolinePtr lazyLinkStub() const {
    return trampolineCode(lazyLinkStubOffset_);
  }
  TrampolinePtr interpreterStub() const {
    return trampolineCode(interpreterStubOffset_);
  }

  TrampolinePtr getDoubleToInt32ValueStub() const {
    return trampolineCode(doubleToInt32ValueStubOffset_);
  }

  bool hasJitcodeGlobalTable() const { return jitcodeGlobalTable_ != nullptr; }

  JitcodeGlobalTable* getJitcodeGlobalTable() {
    MOZ_ASSERT(hasJitcodeGlobalTable());
    return jitcodeGlobalTable_;
  }

  bool isProfilerInstrumentationEnabled(JSRuntime* rt) {
    return rt->geckoProfiler().enabled();
  }

  bool isOptimizationTrackingEnabled(JSRuntime* rt) {
    return isProfilerInstrumentationEnabled(rt);
  }

#ifdef DEBUG
  void* addressOfIonBailAfterCounter() { return &ionBailAfterCounter_; }

  // Set after how many bailing places we should forcefully bail.
  // Zero disables this feature.
  void setIonBailAfterCounter(uint32_t after) { ionBailAfterCounter_ = after; }
  bool ionBailAfterEnabled() const { return ionBailAfterEnabled_; }
  void setIonBailAfterEnabled(bool enabled) { ionBailAfterEnabled_ = enabled; }
#endif

  size_t numFinishedOffThreadTasks() const {
    return numFinishedOffThreadTasks_;
  }
  NumFinishedOffThreadTasksType& numFinishedOffThreadTasksRef(
      const AutoLockHelperThreadState& locked) {
    return numFinishedOffThreadTasks_;
  }

  IonCompileTaskList& ionLazyLinkList(JSRuntime* rt);

  size_t ionLazyLinkListSize() const { return ionLazyLinkListSize_; }

  void ionLazyLinkListRemove(JSRuntime* rt, js::jit::IonCompileTask* task);
  void ionLazyLinkListAdd(JSRuntime* rt, js::jit::IonCompileTask* task);

  uint64_t nextDisambiguationId() { return disambiguationId_++; }
};

enum class CacheKind : uint8_t;
class CacheIRStubInfo;

enum class ICStubEngine : uint8_t {
  // Baseline IC, see BaselineIC.h.
  Baseline = 0,

  // Ion IC, see IonIC.h.
  IonIC
};

struct CacheIRStubKey : public DefaultHasher<CacheIRStubKey> {
  struct Lookup {
    CacheKind kind;
    ICStubEngine engine;
    const uint8_t* code;
    uint32_t length;

    Lookup(CacheKind kind, ICStubEngine engine, const uint8_t* code,
           uint32_t length)
        : kind(kind), engine(engine), code(code), length(length) {}
  };

  static HashNumber hash(const Lookup& l);
  static bool match(const CacheIRStubKey& entry, const Lookup& l);

  UniquePtr<CacheIRStubInfo, JS::FreePolicy> stubInfo;

  explicit CacheIRStubKey(CacheIRStubInfo* info) : stubInfo(info) {}
  CacheIRStubKey(CacheIRStubKey&& other)
      : stubInfo(std::move(other.stubInfo)) {}

  void operator=(CacheIRStubKey&& other) {
    stubInfo = std::move(other.stubInfo);
  }
};

template <typename Key>
struct IcStubCodeMapGCPolicy {
  static bool traceWeak(JSTracer* trc, Key*, WeakHeapPtrJitCode* value) {
    return TraceWeakEdge(trc, value, "traceWeak");
  }
};

class JitZone {
  // Allocated space for optimized baseline stubs.
  OptimizedICStubSpace optimizedStubSpace_;

  // Set of CacheIRStubInfo instances used by Ion stubs in this Zone.
  using IonCacheIRStubInfoSet =
      HashSet<CacheIRStubKey, CacheIRStubKey, SystemAllocPolicy>;
  IonCacheIRStubInfoSet ionCacheIRStubInfoSet_;

  // Map CacheIRStubKey to shared JitCode objects.
  using BaselineCacheIRStubCodeMap =
      GCHashMap<CacheIRStubKey, WeakHeapPtrJitCode, CacheIRStubKey,
                SystemAllocPolicy, IcStubCodeMapGCPolicy<CacheIRStubKey>>;
  BaselineCacheIRStubCodeMap baselineCacheIRStubCodes_;

  // Executable allocator for all code except wasm code.
  MainThreadData<ExecutableAllocator> execAlloc_;

 public:
  void traceWeak(JSTracer* trc);

  void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                              JS::CodeSizes* code, size_t* jitZone,
                              size_t* baselineStubsOptimized) const;

  OptimizedICStubSpace* optimizedStubSpace() { return &optimizedStubSpace_; }

  JitCode* getBaselineCacheIRStubCode(const CacheIRStubKey::Lookup& key,
                                      CacheIRStubInfo** stubInfo) {
    auto p = baselineCacheIRStubCodes_.lookup(key);
    if (p) {
      *stubInfo = p->key().stubInfo.get();
      return p->value();
    }
    *stubInfo = nullptr;
    return nullptr;
  }
  MOZ_MUST_USE bool putBaselineCacheIRStubCode(
      const CacheIRStubKey::Lookup& lookup, CacheIRStubKey& key,
      JitCode* stubCode) {
    auto p = baselineCacheIRStubCodes_.lookupForAdd(lookup);
    MOZ_ASSERT(!p);
    return baselineCacheIRStubCodes_.add(p, std::move(key), stubCode);
  }

  CacheIRStubInfo* getIonCacheIRStubInfo(const CacheIRStubKey::Lookup& key) {
    IonCacheIRStubInfoSet::Ptr p = ionCacheIRStubInfoSet_.lookup(key);
    return p ? p->stubInfo.get() : nullptr;
  }
  MOZ_MUST_USE bool putIonCacheIRStubInfo(const CacheIRStubKey::Lookup& lookup,
                                          CacheIRStubKey& key) {
    IonCacheIRStubInfoSet::AddPtr p =
        ionCacheIRStubInfoSet_.lookupForAdd(lookup);
    MOZ_ASSERT(!p);
    return ionCacheIRStubInfoSet_.add(p, std::move(key));
  }
  void purgeIonCacheIRStubInfo() { ionCacheIRStubInfoSet_.clearAndCompact(); }

  ExecutableAllocator& execAlloc() { return execAlloc_.ref(); }
  const ExecutableAllocator& execAlloc() const { return execAlloc_.ref(); }
};

class JitRealm {
  friend class JitActivation;

  // Map ICStub keys to ICStub shared code objects.
  using ICStubCodeMap =
      GCHashMap<uint32_t, WeakHeapPtrJitCode, DefaultHasher<uint32_t>,
                ZoneAllocPolicy, IcStubCodeMapGCPolicy<uint32_t>>;
  ICStubCodeMap* stubCodes_;

  // The JitRealm stores stubs to concatenate strings inline and perform RegExp
  // calls inline. These bake in zone and realm specific pointers and can't be
  // stored in JitRuntime. They also are dependent on the value of
  // 'stringsCanBeInNursery' and must be flushed when its value changes.
  //
  // These are weak pointers, but they can by accessed during off-thread Ion
  // compilation and therefore can't use the usual read barrier. Instead, we
  // record which stubs have been read and perform the appropriate barriers in
  // CodeGenerator::link().

  enum StubIndex : uint32_t {
    StringConcat = 0,
    RegExpMatcher,
    RegExpSearcher,
    RegExpTester,
    Count
  };

  mozilla::EnumeratedArray<StubIndex, StubIndex::Count, WeakHeapPtrJitCode>
      stubs_;

  bool stringsCanBeInNursery;

  JitCode* generateStringConcatStub(JSContext* cx);
  JitCode* generateRegExpMatcherStub(JSContext* cx);
  JitCode* generateRegExpSearcherStub(JSContext* cx);
  JitCode* generateRegExpTesterStub(JSContext* cx);

  JitCode* getStubNoBarrier(StubIndex stub,
                            uint32_t* requiredBarriersOut) const {
    MOZ_ASSERT(CurrentThreadIsIonCompiling());
    *requiredBarriersOut |= 1 << uint32_t(stub);
    return stubs_[stub].unbarrieredGet();
  }

 public:
  JitCode* getStubCode(uint32_t key) {
    ICStubCodeMap::Ptr p = stubCodes_->lookup(key);
    if (p) {
      return p->value();
    }
    return nullptr;
  }
  MOZ_MUST_USE bool putStubCode(JSContext* cx, uint32_t key,
                                Handle<JitCode*> stubCode) {
    MOZ_ASSERT(stubCode);
    if (!stubCodes_->putNew(key, stubCode.get())) {
      ReportOutOfMemory(cx);
      return false;
    }
    return true;
  }

  JitRealm();
  ~JitRealm();

  MOZ_MUST_USE bool initialize(JSContext* cx, bool zoneHasNurseryStrings);

  // Initialize code stubs only used by Ion, not Baseline.
  MOZ_MUST_USE bool ensureIonStubsExist(JSContext* cx) {
    if (stubs_[StringConcat]) {
      return true;
    }
    stubs_[StringConcat] = generateStringConcatStub(cx);
    return stubs_[StringConcat];
  }

  void traceWeak(JSTracer* trc, JS::Realm* realm);

  void discardStubs() {
    for (WeakHeapPtrJitCode& stubRef : stubs_) {
      stubRef = nullptr;
    }
  }

  bool hasStubs() const {
    for (const WeakHeapPtrJitCode& stubRef : stubs_) {
      if (stubRef) {
        return true;
      }
    }
    return false;
  }

  void setStringsCanBeInNursery(bool allow) {
    MOZ_ASSERT(!hasStubs());
    stringsCanBeInNursery = allow;
  }

  JitCode* stringConcatStubNoBarrier(uint32_t* requiredBarriersOut) const {
    return getStubNoBarrier(StringConcat, requiredBarriersOut);
  }

  JitCode* regExpMatcherStubNoBarrier(uint32_t* requiredBarriersOut) const {
    return getStubNoBarrier(RegExpMatcher, requiredBarriersOut);
  }

  MOZ_MUST_USE bool ensureRegExpMatcherStubExists(JSContext* cx) {
    if (stubs_[RegExpMatcher]) {
      return true;
    }
    stubs_[RegExpMatcher] = generateRegExpMatcherStub(cx);
    return stubs_[RegExpMatcher];
  }

  JitCode* regExpSearcherStubNoBarrier(uint32_t* requiredBarriersOut) const {
    return getStubNoBarrier(RegExpSearcher, requiredBarriersOut);
  }

  MOZ_MUST_USE bool ensureRegExpSearcherStubExists(JSContext* cx) {
    if (stubs_[RegExpSearcher]) {
      return true;
    }
    stubs_[RegExpSearcher] = generateRegExpSearcherStub(cx);
    return stubs_[RegExpSearcher];
  }

  JitCode* regExpTesterStubNoBarrier(uint32_t* requiredBarriersOut) const {
    return getStubNoBarrier(RegExpTester, requiredBarriersOut);
  }

  MOZ_MUST_USE bool ensureRegExpTesterStubExists(JSContext* cx) {
    if (stubs_[RegExpTester]) {
      return true;
    }
    stubs_[RegExpTester] = generateRegExpTesterStub(cx);
    return stubs_[RegExpTester];
  }

  // Perform the necessary read barriers on stubs described by the bitmasks
  // passed in. This function can only be called from the main thread.
  //
  // The stub pointers must still be valid by the time these methods are
  // called. This is arranged by cancelling off-thread Ion compilation at the
  // start of GC and at the start of sweeping.
  void performStubReadBarriers(uint32_t stubsToBarrier) const;

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

// Called from Zone::discardJitCode().
void InvalidateAll(JSFreeOp* fop, JS::Zone* zone);
void FinishInvalidation(JSFreeOp* fop, JSScript* script);

// This class ensures JIT code is executable on its destruction. Creators
// must call makeWritable(), and not attempt to write to the buffer if it fails.
//
// AutoWritableJitCodeFallible may only fail to make code writable; it cannot
// fail to make JIT code executable (because the creating code has no chance to
// recover from a failed destructor).
class MOZ_RAII AutoWritableJitCodeFallible {
  JSRuntime* rt_;
  void* addr_;
  size_t size_;

 public:
  AutoWritableJitCodeFallible(JSRuntime* rt, void* addr, size_t size)
      : rt_(rt), addr_(addr), size_(size) {
    rt_->toggleAutoWritableJitCodeActive(true);
  }

  AutoWritableJitCodeFallible(void* addr, size_t size)
      : AutoWritableJitCodeFallible(TlsContext.get()->runtime(), addr, size) {}

  explicit AutoWritableJitCodeFallible(JitCode* code)
      : AutoWritableJitCodeFallible(code->runtimeFromMainThread(), code->raw(),
                                    code->bufferSize()) {}

  MOZ_MUST_USE bool makeWritable() {
    return ExecutableAllocator::makeWritable(addr_, size_);
  }

  ~AutoWritableJitCodeFallible() {
    if (!ExecutableAllocator::makeExecutableAndFlushICache(addr_, size_)) {
      MOZ_CRASH();
    }
    rt_->toggleAutoWritableJitCodeActive(false);
  }
};

// Infallible variant of AutoWritableJitCodeFallible, ensures writable during
// construction
class MOZ_RAII AutoWritableJitCode : private AutoWritableJitCodeFallible {
 public:
  AutoWritableJitCode(JSRuntime* rt, void* addr, size_t size)
      : AutoWritableJitCodeFallible(rt, addr, size) {
    MOZ_RELEASE_ASSERT(makeWritable());
  }

  AutoWritableJitCode(void* addr, size_t size)
      : AutoWritableJitCode(TlsContext.get()->runtime(), addr, size) {}

  explicit AutoWritableJitCode(JitCode* code)
      : AutoWritableJitCode(code->runtimeFromMainThread(), code->raw(),
                            code->bufferSize()) {}
};

}  // namespace jit
}  // namespace js

#endif /* jit_JitRealm_h */
