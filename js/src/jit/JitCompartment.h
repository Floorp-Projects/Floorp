/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitCompartment_h
#define jit_JitCompartment_h

#include "mozilla/Array.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "builtin/TypedObject.h"
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

enum EnterJitType {
    EnterJitBaseline = 0,
    EnterJitOptimized = 1
};

struct EnterJitData
{
    explicit EnterJitData(JSContext* cx)
      : envChain(cx),
        result(cx)
    {}

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

typedef void (*EnterJitCode)(void* code, unsigned argc, Value* argv, InterpreterFrame* fp,
                             CalleeToken calleeToken, JSObject* envChain,
                             size_t numStackValues, Value* vp);

class JitcodeGlobalTable;

// Information about a loop backedge in the runtime, which can be set to
// point to either the loop header or to an OOL interrupt checking stub,
// if signal handlers are being used to implement interrupts.
class PatchableBackedge : public InlineListNode<PatchableBackedge>
{
    friend class JitRuntime;

    CodeLocationJump backedge;
    CodeLocationLabel loopHeader;
    CodeLocationLabel interruptCheck;

  public:
    PatchableBackedge(CodeLocationJump backedge,
                      CodeLocationLabel loopHeader,
                      CodeLocationLabel interruptCheck)
      : backedge(backedge), loopHeader(loopHeader), interruptCheck(interruptCheck)
    {}
};

class JitRuntime
{
  public:
    enum BackedgeTarget {
        BackedgeLoopHeader,
        BackedgeInterruptCheck
    };

  private:
    friend class JitCompartment;

    // Executable allocator for all code except wasm code and Ion code with
    // patchable backedges (see below).
    ExecutableAllocator execAlloc_;

    // Executable allocator for Ion scripts with patchable backedges.
    ExecutableAllocator backedgeExecAlloc_;

    // Shared exception-handler tail.
    JitCode* exceptionTail_;

    // Shared post-bailout-handler tail.
    JitCode* bailoutTail_;

    // Shared profiler exit frame tail.
    JitCode* profilerExitFrameTail_;

    // Trampoline for entering JIT code. Contains OSR prologue.
    JitCode* enterJIT_;

    // Trampoline for entering baseline JIT code.
    JitCode* enterBaselineJIT_;

    // Vector mapping frame class sizes to bailout tables.
    Vector<JitCode*, 4, SystemAllocPolicy> bailoutTables_;

    // Generic bailout table; used if the bailout table overflows.
    JitCode* bailoutHandler_;

    // Argument-rectifying thunk, in the case of insufficient arguments passed
    // to a function call site.
    JitCode* argumentsRectifier_;
    void* argumentsRectifierReturnAddr_;

    // Thunk that invalides an (Ion compiled) caller on the Ion stack.
    JitCode* invalidator_;

    // Thunk that calls the GC pre barrier.
    JitCode* valuePreBarrier_;
    JitCode* stringPreBarrier_;
    JitCode* objectPreBarrier_;
    JitCode* shapePreBarrier_;
    JitCode* objectGroupPreBarrier_;

    // Thunk to call malloc/free.
    JitCode* mallocStub_;
    JitCode* freeStub_;

    // Thunk called to finish compilation of an IonScript.
    JitCode* lazyLinkStub_;

    // Thunk used by the debugger for breakpoint and step mode.
    JitCode* debugTrapHandler_;

    // Thunk used to fix up on-stack recompile of baseline scripts.
    JitCode* baselineDebugModeOSRHandler_;
    void* baselineDebugModeOSRHandlerNoFrameRegPopAddr_;

    // Map VMFunction addresses to the JitCode of the wrapper.
    using VMWrapperMap = HashMap<const VMFunction*, JitCode*>;
    VMWrapperMap* functionWrappers_;

    // Buffer for OSR from baseline to Ion. To avoid holding on to this for
    // too long, it's also freed in JitCompartment::mark and in EnterBaseline
    // (after returning from JIT code).
    uint8_t* osrTempData_;

    // If true, the signal handler to interrupt Ion code should not attempt to
    // patch backedges, as we're busy modifying data structures.
    mozilla::Atomic<bool> preventBackedgePatching_;

    // Whether patchable backedges currently jump to the loop header or the
    // interrupt check.
    BackedgeTarget backedgeTarget_;

    // List of all backedges in all Ion code. The backedge edge list is accessed
    // asynchronously when the main thread is paused and preventBackedgePatching_
    // is false. Thus, the list must only be mutated while preventBackedgePatching_
    // is true.
    InlineList<PatchableBackedge> backedgeList_;

    // In certain cases, we want to optimize certain opcodes to typed instructions,
    // to avoid carrying an extra register to feed into an unbox. Unfortunately,
    // that's not always possible. For example, a GetPropertyCacheT could return a
    // typed double, but if it takes its out-of-line path, it could return an
    // object, and trigger invalidation. The invalidation bailout will consider the
    // return value to be a double, and create a garbage Value.
    //
    // To allow the GetPropertyCacheT optimization, we allow the ability for
    // GetPropertyCache to override the return value at the top of the stack - the
    // value that will be temporarily corrupt. This special override value is set
    // only in callVM() targets that are about to return *and* have invalidated
    // their callee.
    js::Value ionReturnOverride_;

    // Global table of jitcode native address => bytecode address mappings.
    JitcodeGlobalTable* jitcodeGlobalTable_;

  private:
    JitCode* generateLazyLinkStub(JSContext* cx);
    JitCode* generateProfilerExitFrameTailStub(JSContext* cx);
    JitCode* generateExceptionTailStub(JSContext* cx, void* handler);
    JitCode* generateBailoutTailStub(JSContext* cx);
    JitCode* generateEnterJIT(JSContext* cx, EnterJitType type);
    JitCode* generateArgumentsRectifier(JSContext* cx, void** returnAddrOut);
    JitCode* generateBailoutTable(JSContext* cx, uint32_t frameClass);
    JitCode* generateBailoutHandler(JSContext* cx);
    JitCode* generateInvalidator(JSContext* cx);
    JitCode* generatePreBarrier(JSContext* cx, MIRType type);
    JitCode* generateMallocStub(JSContext* cx);
    JitCode* generateFreeStub(JSContext* cx);
    JitCode* generateDebugTrapHandler(JSContext* cx);
    JitCode* generateBaselineDebugModeOSRHandler(JSContext* cx, uint32_t* noFrameRegPopOffsetOut);
    JitCode* generateVMWrapper(JSContext* cx, const VMFunction& f);

    bool generateTLEventVM(JSContext* cx, MacroAssembler& masm, const VMFunction& f, bool enter);

    inline bool generateTLEnterVM(JSContext* cx, MacroAssembler& masm, const VMFunction& f) {
        return generateTLEventVM(cx, masm, f, /* enter = */ true);
    }
    inline bool generateTLExitVM(JSContext* cx, MacroAssembler& masm, const VMFunction& f) {
        return generateTLEventVM(cx, masm, f, /* enter = */ false);
    }

  public:
    explicit JitRuntime(JSRuntime* rt);
    ~JitRuntime();
    MOZ_MUST_USE bool initialize(JSContext* cx, js::AutoLockForExclusiveAccess& lock);

    uint8_t* allocateOsrTempData(size_t size);
    void freeOsrTempData();

    static void Mark(JSTracer* trc, js::AutoLockForExclusiveAccess& lock);
    static void MarkJitcodeGlobalTableUnconditionally(JSTracer* trc);
    static MOZ_MUST_USE bool MarkJitcodeGlobalTableIteratively(JSTracer* trc);
    static void SweepJitcodeGlobalTable(JSRuntime* rt);

    ExecutableAllocator& execAlloc() {
        return execAlloc_;
    }
    ExecutableAllocator& backedgeExecAlloc() {
        return backedgeExecAlloc_;
    }

    class AutoPreventBackedgePatching
    {
        mozilla::DebugOnly<JSRuntime*> rt_;
        JitRuntime* jrt_;
        bool prev_;

      public:
        // This two-arg constructor is provided for JSRuntime::createJitRuntime,
        // where we have a JitRuntime but didn't set rt->jitRuntime_ yet.
        AutoPreventBackedgePatching(JSRuntime* rt, JitRuntime* jrt)
          : rt_(rt),
            jrt_(jrt),
            prev_(false)  // silence GCC warning
        {
            MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
            if (jrt_) {
                prev_ = jrt_->preventBackedgePatching_;
                jrt_->preventBackedgePatching_ = true;
            }
        }
        explicit AutoPreventBackedgePatching(JSRuntime* rt)
          : AutoPreventBackedgePatching(rt, rt->jitRuntime())
        {}
        ~AutoPreventBackedgePatching() {
            MOZ_ASSERT(jrt_ == rt_->jitRuntime());
            if (jrt_) {
                MOZ_ASSERT(jrt_->preventBackedgePatching_);
                jrt_->preventBackedgePatching_ = prev_;
            }
        }
    };

    bool preventBackedgePatching() const {
        return preventBackedgePatching_;
    }
    BackedgeTarget backedgeTarget() const {
        return backedgeTarget_;
    }
    void addPatchableBackedge(PatchableBackedge* backedge) {
        MOZ_ASSERT(preventBackedgePatching_);
        backedgeList_.pushFront(backedge);
    }
    void removePatchableBackedge(PatchableBackedge* backedge) {
        MOZ_ASSERT(preventBackedgePatching_);
        backedgeList_.remove(backedge);
    }

    void patchIonBackedges(JSRuntime* rt, BackedgeTarget target);

    JitCode* getVMWrapper(const VMFunction& f) const;
    JitCode* debugTrapHandler(JSContext* cx);
    JitCode* getBaselineDebugModeOSRHandler(JSContext* cx);
    void* getBaselineDebugModeOSRHandlerAddress(JSContext* cx, bool popFrameReg);

    JitCode* getGenericBailoutHandler() const {
        return bailoutHandler_;
    }

    JitCode* getExceptionTail() const {
        return exceptionTail_;
    }

    JitCode* getBailoutTail() const {
        return bailoutTail_;
    }

    JitCode* getProfilerExitFrameTail() const {
        return profilerExitFrameTail_;
    }

    JitCode* getBailoutTable(const FrameSizeClass& frameClass) const;

    JitCode* getArgumentsRectifier() const {
        return argumentsRectifier_;
    }

    void* getArgumentsRectifierReturnAddr() const {
        return argumentsRectifierReturnAddr_;
    }

    JitCode* getInvalidationThunk() const {
        return invalidator_;
    }

    EnterJitCode enterIon() const {
        return enterJIT_->as<EnterJitCode>();
    }

    EnterJitCode enterBaseline() const {
        return enterBaselineJIT_->as<EnterJitCode>();
    }

    JitCode* preBarrier(MIRType type) const {
        switch (type) {
          case MIRType::Value: return valuePreBarrier_;
          case MIRType::String: return stringPreBarrier_;
          case MIRType::Object: return objectPreBarrier_;
          case MIRType::Shape: return shapePreBarrier_;
          case MIRType::ObjectGroup: return objectGroupPreBarrier_;
          default: MOZ_CRASH();
        }
    }

    JitCode* mallocStub() const {
        return mallocStub_;
    }

    JitCode* freeStub() const {
        return freeStub_;
    }

    JitCode* lazyLinkStub() const {
        return lazyLinkStub_;
    }

    bool hasIonReturnOverride() const {
        return !ionReturnOverride_.isMagic(JS_ARG_POISON);
    }
    js::Value takeIonReturnOverride() {
        js::Value v = ionReturnOverride_;
        ionReturnOverride_ = js::MagicValue(JS_ARG_POISON);
        return v;
    }
    void setIonReturnOverride(const js::Value& v) {
        MOZ_ASSERT(!hasIonReturnOverride());
        MOZ_ASSERT(!v.isMagic());
        ionReturnOverride_ = v;
    }

    bool hasJitcodeGlobalTable() const {
        return jitcodeGlobalTable_ != nullptr;
    }

    JitcodeGlobalTable* getJitcodeGlobalTable() {
        MOZ_ASSERT(hasJitcodeGlobalTable());
        return jitcodeGlobalTable_;
    }

    bool isProfilerInstrumentationEnabled(JSRuntime* rt) {
        return rt->spsProfiler.enabled();
    }

    bool isOptimizationTrackingEnabled(JSRuntime* rt) {
        return isProfilerInstrumentationEnabled(rt);
    }
};

class JitZone
{
    // Allocated space for optimized baseline stubs.
    OptimizedICStubSpace optimizedStubSpace_;

  public:
    OptimizedICStubSpace* optimizedStubSpace() {
        return &optimizedStubSpace_;
    }
};

enum class CacheKind;
class CacheIRStubInfo;

struct CacheIRStubKey : public DefaultHasher<CacheIRStubKey> {
    struct Lookup {
        CacheKind kind;
        const uint8_t* code;
        uint32_t length;

        Lookup(CacheKind kind, const uint8_t* code, uint32_t length)
          : kind(kind), code(code), length(length)
        {}
    };

    static HashNumber hash(const Lookup& l);
    static bool match(const CacheIRStubKey& entry, const Lookup& l);

    UniquePtr<CacheIRStubInfo, JS::FreePolicy> stubInfo;

    explicit CacheIRStubKey(CacheIRStubInfo* info) : stubInfo(info) {}
    CacheIRStubKey(CacheIRStubKey&& other) : stubInfo(Move(other.stubInfo)) { }

    void operator=(CacheIRStubKey&& other) {
        stubInfo = Move(other.stubInfo);
    }
};

class JitCompartment
{
    friend class JitActivation;

    template<typename Key>
    struct IcStubCodeMapGCPolicy {
        static bool needsSweep(Key*, ReadBarrieredJitCode* value) {
            return IsAboutToBeFinalized(value);
        }
    };

    // Map ICStub keys to ICStub shared code objects.
    using ICStubCodeMap = GCHashMap<uint32_t,
                                    ReadBarrieredJitCode,
                                    DefaultHasher<uint32_t>,
                                    RuntimeAllocPolicy,
                                    IcStubCodeMapGCPolicy<uint32_t>>;
    ICStubCodeMap* stubCodes_;

    // Map ICStub keys to ICStub shared code objects.
    using CacheIRStubCodeMap = GCHashMap<CacheIRStubKey,
                                         ReadBarrieredJitCode,
                                         CacheIRStubKey,
                                         RuntimeAllocPolicy,
                                         IcStubCodeMapGCPolicy<CacheIRStubKey>>;
    CacheIRStubCodeMap* cacheIRStubCodes_;

    // Keep track of offset into various baseline stubs' code at return
    // point from called script.
    void* baselineCallReturnAddrs_[2];
    void* baselineGetPropReturnAddr_;
    void* baselineSetPropReturnAddr_;

    // Stubs to concatenate two strings inline, or perform RegExp calls inline.
    // These bake in zone and compartment specific pointers and can't be stored
    // in JitRuntime. These are weak pointers, but are not declared as
    // ReadBarriered since they are only read from during Ion compilation,
    // which may occur off thread and whose barriers are captured during
    // CodeGenerator::link.
    JitCode* stringConcatStub_;
    JitCode* regExpMatcherStub_;
    JitCode* regExpSearcherStub_;
    JitCode* regExpTesterStub_;

    mozilla::EnumeratedArray<SimdType, SimdType::Count, ReadBarrieredObject> simdTemplateObjects_;

    JitCode* generateStringConcatStub(JSContext* cx);
    JitCode* generateRegExpMatcherStub(JSContext* cx);
    JitCode* generateRegExpSearcherStub(JSContext* cx);
    JitCode* generateRegExpTesterStub(JSContext* cx);

  public:
    JSObject* getSimdTemplateObjectFor(JSContext* cx, Handle<SimdTypeDescr*> descr) {
        ReadBarrieredObject& tpl = simdTemplateObjects_[descr->type()];
        if (!tpl)
            tpl.set(TypedObject::createZeroed(cx, descr, 0, gc::TenuredHeap));
        return tpl.get();
    }

    JSObject* maybeGetSimdTemplateObjectFor(SimdType type) const {
        const ReadBarrieredObject& tpl = simdTemplateObjects_[type];

        // This function is used by Eager Simd Unbox phase, so we cannot use the
        // read barrier. For more information, see the comment above
        // CodeGenerator::simdRefreshTemplatesDuringLink_ .
        return tpl.unbarrieredGet();
    }

    // This function is used to call the read barrier, to mark the SIMD template
    // type as used. This function can only be called from the main thread.
    void registerSimdTemplateObjectFor(SimdType type) {
        ReadBarrieredObject& tpl = simdTemplateObjects_[type];
        MOZ_ASSERT(tpl.unbarrieredGet());
        tpl.get();
    }

    JitCode* getStubCode(uint32_t key) {
        ICStubCodeMap::AddPtr p = stubCodes_->lookupForAdd(key);
        if (p)
            return p->value();
        return nullptr;
    }
    MOZ_MUST_USE bool putStubCode(JSContext* cx, uint32_t key, Handle<JitCode*> stubCode) {
        MOZ_ASSERT(stubCode);
        if (!stubCodes_->putNew(key, stubCode.get())) {
            ReportOutOfMemory(cx);
            return false;
        }
        return true;
    }
    JitCode* getCacheIRStubCode(const CacheIRStubKey::Lookup& key, CacheIRStubInfo** stubInfo) {
        CacheIRStubCodeMap::Ptr p = cacheIRStubCodes_->lookup(key);
        if (p) {
            *stubInfo = p->key().stubInfo.get();
            return p->value();
        }
        *stubInfo = nullptr;
        return nullptr;
    }
    MOZ_MUST_USE bool putCacheIRStubCode(const CacheIRStubKey::Lookup& lookup, CacheIRStubKey& key,
                                         JitCode* stubCode)
    {
        CacheIRStubCodeMap::AddPtr p = cacheIRStubCodes_->lookupForAdd(lookup);
        MOZ_ASSERT(!p);
        return cacheIRStubCodes_->add(p, Move(key), stubCode);
    }
    void initBaselineCallReturnAddr(void* addr, bool constructing) {
        MOZ_ASSERT(baselineCallReturnAddrs_[constructing] == nullptr);
        baselineCallReturnAddrs_[constructing] = addr;
    }
    void* baselineCallReturnAddr(bool constructing) {
        MOZ_ASSERT(baselineCallReturnAddrs_[constructing] != nullptr);
        return baselineCallReturnAddrs_[constructing];
    }
    void initBaselineGetPropReturnAddr(void* addr) {
        MOZ_ASSERT(baselineGetPropReturnAddr_ == nullptr);
        baselineGetPropReturnAddr_ = addr;
    }
    void* baselineGetPropReturnAddr() {
        MOZ_ASSERT(baselineGetPropReturnAddr_ != nullptr);
        return baselineGetPropReturnAddr_;
    }
    void initBaselineSetPropReturnAddr(void* addr) {
        MOZ_ASSERT(baselineSetPropReturnAddr_ == nullptr);
        baselineSetPropReturnAddr_ = addr;
    }
    void* baselineSetPropReturnAddr() {
        MOZ_ASSERT(baselineSetPropReturnAddr_ != nullptr);
        return baselineSetPropReturnAddr_;
    }

    void toggleBarriers(bool enabled);

  public:
    JitCompartment();
    ~JitCompartment();

    MOZ_MUST_USE bool initialize(JSContext* cx);

    // Initialize code stubs only used by Ion, not Baseline.
    MOZ_MUST_USE bool ensureIonStubsExist(JSContext* cx);

    void mark(JSTracer* trc, JSCompartment* compartment);
    void sweep(FreeOp* fop, JSCompartment* compartment);

    JitCode* stringConcatStubNoBarrier() const {
        return stringConcatStub_;
    }

    JitCode* regExpMatcherStubNoBarrier() const {
        return regExpMatcherStub_;
    }

    MOZ_MUST_USE bool ensureRegExpMatcherStubExists(JSContext* cx) {
        if (regExpMatcherStub_)
            return true;
        regExpMatcherStub_ = generateRegExpMatcherStub(cx);
        return regExpMatcherStub_ != nullptr;
    }

    JitCode* regExpSearcherStubNoBarrier() const {
        return regExpSearcherStub_;
    }

    MOZ_MUST_USE bool ensureRegExpSearcherStubExists(JSContext* cx) {
        if (regExpSearcherStub_)
            return true;
        regExpSearcherStub_ = generateRegExpSearcherStub(cx);
        return regExpSearcherStub_ != nullptr;
    }

    JitCode* regExpTesterStubNoBarrier() const {
        return regExpTesterStub_;
    }

    MOZ_MUST_USE bool ensureRegExpTesterStubExists(JSContext* cx) {
        if (regExpTesterStub_)
            return true;
        regExpTesterStub_ = generateRegExpTesterStub(cx);
        return regExpTesterStub_ != nullptr;
    }

    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

// Called from JSCompartment::discardJitCode().
void InvalidateAll(FreeOp* fop, JS::Zone* zone);
void FinishInvalidation(FreeOp* fop, JSScript* script);

// On windows systems, really large frames need to be incrementally touched.
// The following constant defines the minimum increment of the touch.
#ifdef XP_WIN
const unsigned WINDOWS_BIG_FRAME_TOUCH_INCREMENT = 4096 - 1;
#endif

// If NON_WRITABLE_JIT_CODE is enabled, this class will ensure
// JIT code is writable (has RW permissions) in its scope.
// Otherwise it's a no-op.
class MOZ_STACK_CLASS AutoWritableJitCode
{
    // Backedge patching from the signal handler will change memory protection
    // flags, so don't allow it in a AutoWritableJitCode scope.
    JitRuntime::AutoPreventBackedgePatching preventPatching_;
    JSRuntime* rt_;
    void* addr_;
    size_t size_;

  public:
    AutoWritableJitCode(JSRuntime* rt, void* addr, size_t size)
      : preventPatching_(rt), rt_(rt), addr_(addr), size_(size)
    {
        rt_->toggleAutoWritableJitCodeActive(true);
        if (!ExecutableAllocator::makeWritable(addr_, size_))
            MOZ_CRASH();
    }
    AutoWritableJitCode(void* addr, size_t size)
      : AutoWritableJitCode(TlsPerThreadData.get()->runtimeFromMainThread(), addr, size)
    {}
    explicit AutoWritableJitCode(JitCode* code)
      : AutoWritableJitCode(code->runtimeFromMainThread(), code->raw(), code->bufferSize())
    {}
    ~AutoWritableJitCode() {
        if (!ExecutableAllocator::makeExecutable(addr_, size_))
            MOZ_CRASH();
        rt_->toggleAutoWritableJitCodeActive(false);
    }
};

class MOZ_STACK_CLASS MaybeAutoWritableJitCode
{
    mozilla::Maybe<AutoWritableJitCode> awjc_;

  public:
    MaybeAutoWritableJitCode(void* addr, size_t size, ReprotectCode reprotect) {
        if (reprotect)
            awjc_.emplace(addr, size);
    }
    MaybeAutoWritableJitCode(JitCode* code, ReprotectCode reprotect) {
        if (reprotect)
            awjc_.emplace(code);
    }
};

} // namespace jit
} // namespace js

#endif /* jit_JitCompartment_h */
