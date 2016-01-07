/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_module_h
#define wasm_module_h

#include "asmjs/WasmTypes.h"
#include "gc/Barrier.h"
#include "vm/MallocProvider.h"

namespace js {

class WasmActivation;
namespace jit { struct BaselineScript; }

namespace wasm {

// A wasm Module and everything it contains must support serialization,
// deserialization and cloning. Some data can be simply copied as raw bytes and,
// as a convention, is stored in an inline CacheablePod struct. Everything else
// should implement the below methods which are called recusively by the
// containing Module. The implementation of all these methods are grouped
// together in WasmSerialize.cpp.

#define WASM_DECLARE_SERIALIZABLE(Type)                                         \
    size_t serializedSize() const;                                              \
    uint8_t* serialize(uint8_t* cursor) const;                                  \
    const uint8_t* deserialize(ExclusiveContext* cx, const uint8_t* cursor);    \
    bool clone(JSContext* cx, Type* out) const;                                 \
    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

// The StaticLinkData contains all the metadata necessary to perform
// Module::staticallyLink but is not necessary afterwards.

struct StaticLinkData
{
    struct InternalLink {
        enum Kind {
            RawPointer,
            CodeLabel,
            InstructionImmediate
        };
        uint32_t patchAtOffset;
        uint32_t targetOffset;

        InternalLink() = default;
        explicit InternalLink(Kind kind);
        bool isRawPointerPatch();
    };
    typedef Vector<InternalLink, 0, SystemAllocPolicy> InternalLinkVector;

    typedef Vector<uint32_t, 0, SystemAllocPolicy> OffsetVector;
    struct SymbolicLinkArray : mozilla::EnumeratedArray<SymbolicAddress,
                                                        SymbolicAddress::Limit,
                                                        OffsetVector> {
        WASM_DECLARE_SERIALIZABLE(SymbolicLinkArray)
    };

    struct FuncPtrTable {
        uint32_t globalDataOffset;
        OffsetVector elemOffsets;
        explicit FuncPtrTable(uint32_t globalDataOffset) : globalDataOffset(globalDataOffset) {}
        FuncPtrTable() = default;
        FuncPtrTable(FuncPtrTable&& rhs)
          : globalDataOffset(rhs.globalDataOffset), elemOffsets(Move(rhs.elemOffsets))
        {}
        WASM_DECLARE_SERIALIZABLE(FuncPtrTable)
    };
    typedef Vector<FuncPtrTable, 0, SystemAllocPolicy> FuncPtrTableVector;

    struct CacheablePod {
        uint32_t        interruptOffset;
        uint32_t        outOfBoundsOffset;
    } pod;
    InternalLinkVector  internalLinks;
    SymbolicLinkArray   symbolicLinks;
    FuncPtrTableVector  funcPtrTables;

    WASM_DECLARE_SERIALIZABLE(StaticLinkData)
};

typedef UniquePtr<StaticLinkData, JS::DeletePolicy<StaticLinkData>> UniqueStaticLinkData;

// An Export describes an export from a wasm module. Currently only functions
// can be exported.

class Export
{
    MallocSig sig_;
    struct CacheablePod {
        uint32_t funcIndex_;
        uint32_t stubOffset_;
    } pod;

  public:
    Export() = default;
    Export(MallocSig&& sig, uint32_t funcIndex)
      : sig_(Move(sig))
    {
        pod.funcIndex_ = funcIndex;
        pod.stubOffset_ = UINT32_MAX;
    }
    Export(Export&& rhs)
      : sig_(Move(rhs.sig_)),
        pod(rhs.pod)
    {}

    void initStubOffset(uint32_t stubOffset) {
        MOZ_ASSERT(pod.stubOffset_ == UINT32_MAX);
        pod.stubOffset_ = stubOffset;
    }

    uint32_t funcIndex() const {
        return pod.funcIndex_;
    }
    uint32_t stubOffset() const {
        return pod.stubOffset_;
    }
    const MallocSig& sig() const {
        return sig_;
    }

    WASM_DECLARE_SERIALIZABLE(Export)
};

typedef Vector<Export, 0, SystemAllocPolicy> ExportVector;

// An Import describes a wasm module import. Currently, only functions can be
// imported in wasm. A function import includes the signature used within the
// module to call it.

class Import
{
    MallocSig sig_;
    struct CacheablePod {
        uint32_t exitGlobalDataOffset_;
        uint32_t interpExitCodeOffset_;
        uint32_t jitExitCodeOffset_;
    } pod;

  public:
    Import() {}
    Import(Import&& rhs) : sig_(Move(rhs.sig_)), pod(rhs.pod) {}
    Import(MallocSig&& sig, uint32_t exitGlobalDataOffset)
      : sig_(Move(sig))
    {
        pod.exitGlobalDataOffset_ = exitGlobalDataOffset;
        pod.interpExitCodeOffset_ = 0;
        pod.jitExitCodeOffset_ = 0;
    }

    void initInterpExitOffset(uint32_t off) {
        MOZ_ASSERT(!pod.interpExitCodeOffset_);
        pod.interpExitCodeOffset_ = off;
    }
    void initJitExitOffset(uint32_t off) {
        MOZ_ASSERT(!pod.jitExitCodeOffset_);
        pod.jitExitCodeOffset_ = off;
    }

    const MallocSig& sig() const {
        return sig_;
    }
    uint32_t exitGlobalDataOffset() const {
        return pod.exitGlobalDataOffset_;
    }
    uint32_t interpExitCodeOffset() const {
        MOZ_ASSERT(pod.interpExitCodeOffset_);
        return pod.interpExitCodeOffset_;
    }
    uint32_t jitExitCodeOffset() const {
        MOZ_ASSERT(pod.jitExitCodeOffset_);
        return pod.jitExitCodeOffset_;
    }

    WASM_DECLARE_SERIALIZABLE(Import)
};

typedef Vector<Import, 0, SystemAllocPolicy> ImportVector;

// A CodeRange describes a single contiguous range of code within a wasm
// module's code segment. A CodeRange describes what the code does and, for
// function bodies, the name and source coordinates of the function.

class CodeRange
{
    uint32_t nameIndex_;
    uint32_t lineNumber_;
    uint32_t begin_;
    uint32_t profilingReturn_;
    uint32_t end_;
    union {
        struct {
            uint8_t kind_;
            uint8_t beginToEntry_;
            uint8_t profilingJumpToProfilingReturn_;
            uint8_t profilingEpilogueToProfilingReturn_;
        } func;
        uint8_t kind_;
    } u;

    void assertValid();

  public:
    enum Kind { Function, Entry, ImportJitExit, ImportInterpExit, Interrupt, Inline };

    CodeRange() = default;
    CodeRange(Kind kind, Offsets offsets);
    CodeRange(Kind kind, ProfilingOffsets offsets);
    CodeRange(uint32_t nameIndex, uint32_t lineNumber, FuncOffsets offsets);

    // All CodeRanges have a begin and end.

    uint32_t begin() const {
        return begin_;
    }
    uint32_t end() const {
        return end_;
    }

    // Other fields are only available for certain CodeRange::Kinds.

    Kind kind() const { return Kind(u.kind_); }

    // Every CodeRange except entry and inline stubs has a profiling return
    // which is used for asynchronous profiling to determine the frame pointer.

    uint32_t profilingReturn() const {
        MOZ_ASSERT(kind() != Entry && kind() != Inline);
        return profilingReturn_;
    }

    // Functions have offsets which allow patching to selectively execute
    // profiling prologues/epilogues.

    bool isFunction() const {
        return kind() == Function;
    }
    uint32_t funcProfilingEntry() const {
        MOZ_ASSERT(isFunction());
        return begin();
    }
    uint32_t funcNonProfilingEntry() const {
        MOZ_ASSERT(isFunction());
        return begin_ + u.func.beginToEntry_;
    }
    uint32_t functionProfilingJump() const {
        MOZ_ASSERT(isFunction());
        return profilingReturn_ - u.func.profilingJumpToProfilingReturn_;
    }
    uint32_t funcProfilingEpilogue() const {
        MOZ_ASSERT(isFunction());
        return profilingReturn_ - u.func.profilingEpilogueToProfilingReturn_;
    }
    uint32_t funcNameIndex() const {
        MOZ_ASSERT(isFunction());
        return nameIndex_;
    }
    uint32_t funcLineNumber() const {
        MOZ_ASSERT(isFunction());
        return lineNumber_;
    }

    // A sorted array of CodeRanges can be looked up via BinarySearch and PC.

    struct PC {
        size_t offset;
        explicit PC(size_t offset) : offset(offset) {}
        bool operator==(const CodeRange& rhs) const {
            return offset >= rhs.begin() && offset < rhs.end();
        }
        bool operator<(const CodeRange& rhs) const {
            return offset < rhs.begin();
        }
    };
};

typedef Vector<CodeRange, 0, SystemAllocPolicy> CodeRangeVector;

// A CacheableUniquePtr is used to cacheably store strings in Module.

template <class CharT>
struct CacheableUniquePtr : public UniquePtr<CharT, JS::FreePolicy>
{
    typedef UniquePtr<CharT, JS::FreePolicy> UPtr;
    explicit CacheableUniquePtr(CharT* ptr) : UPtr(ptr) {}
    MOZ_IMPLICIT CacheableUniquePtr(UPtr&& rhs) : UPtr(Move(rhs)) {}
    CacheableUniquePtr() = default;
    CacheableUniquePtr(CacheableUniquePtr&& rhs) : UPtr(Move(rhs)) {}
    void operator=(CacheableUniquePtr&& rhs) { UPtr& base = *this; base = Move(rhs); }
    WASM_DECLARE_SERIALIZABLE(CacheableUniquePtr)
};

typedef CacheableUniquePtr<char> CacheableChars;
typedef CacheableUniquePtr<char16_t> CacheableTwoByteChars;
typedef Vector<CacheableChars, 0, SystemAllocPolicy> CacheableCharsVector;

// A UniqueCodePtr owns allocated executable code. Code passed to the Module
// constructor must be allocated via AllocateCode.

class CodeDeleter
{
    uint32_t bytes_;
  public:
    explicit CodeDeleter(uint32_t bytes) : bytes_(bytes) {}
    void operator()(uint8_t* p);
};
typedef JS::UniquePtr<uint8_t, CodeDeleter> UniqueCodePtr;

UniqueCodePtr
AllocateCode(ExclusiveContext* cx, size_t bytes);

// A wasm module can either use no heap, a unshared heap (ArrayBuffer) or shared
// heap (SharedArrayBuffer).

enum class HeapUsage
{
    None = false,
    Unshared = 1,
    Shared = 2
};

static inline bool
UsesHeap(HeapUsage heapUsage)
{
    return bool(heapUsage);
}

// Module represents a compiled WebAssembly module which lives until the last
// reference to any exported functions is dropped. Modules must be wrapped by a
// rooted JSObject immediately after creation so that Module::trace() is called
// during GC. Modules are created after compilation completes and start in a
// a fully unlinked state. After creation, a module must be first statically
// linked and then dynamically linked:
//
//  - Static linking patches code or global data that relies on absolute
//    addresses. Static linking should happen after a module is serialized into
//    a cache file so that the cached code is stored unlinked and ready to be
//    statically linked after deserialization.
//
//  - Dynamic linking patches code or global data that relies on the address of
//    the heap and imports of a module. A module may only be dynamically linked
//    once. However, a dynamically-linked module may be cloned so that the clone
//    can be independently dynamically linked.
//
// Once fully dynamically linked, a Module can have its exports invoked via
// callExport().

class Module
{
    struct ImportExit {
        void* code;
        jit::BaselineScript* baselineScript;
        HeapPtrFunction fun;
        static_assert(sizeof(HeapPtrFunction) == sizeof(void*), "for JIT access");
    };
    struct EntryArg {
        uint64_t lo;
        uint64_t hi;
    };
    typedef int32_t (*EntryFuncPtr)(EntryArg* args, uint8_t* global);
    struct FuncPtrTable {
        uint32_t globalDataOffset;
        uint32_t numElems;
        explicit FuncPtrTable(const StaticLinkData::FuncPtrTable& table)
          : globalDataOffset(table.globalDataOffset),
            numElems(table.elemOffsets.length())
        {}
    };
    typedef Vector<FuncPtrTable, 0, SystemAllocPolicy> FuncPtrTableVector;
    typedef Vector<CacheableChars, 0, SystemAllocPolicy> FuncLabelVector;
    typedef RelocatablePtrArrayBufferObjectMaybeShared BufferPtr;

    // Initialized when constructed:
    struct CacheablePod {
        const uint32_t           functionBytes_;
        const uint32_t           codeBytes_;
        const uint32_t           globalBytes_;
        const HeapUsage          heapUsage_;
        const bool               mutedErrors_;
        const bool               usesSignalHandlersForOOB_;
        const bool               usesSignalHandlersForInterrupt_;
    } pod;
    const UniqueCodePtr          code_;
    const ImportVector           imports_;
    const ExportVector           exports_;
    const HeapAccessVector       heapAccesses_;
    const CodeRangeVector        codeRanges_;
    const CallSiteVector         callSites_;
    const CacheableCharsVector   funcNames_;
    const CacheableChars         filename_;
    const CacheableTwoByteChars  displayURL_;
    const bool                   loadedFromCache_;

    // Initialized during staticallyLink:
    bool                         staticallyLinked_;
    uint8_t*                     interrupt_;
    uint8_t*                     outOfBounds_;
    FuncPtrTableVector           funcPtrTables_;

    // Initialized during dynamicallyLink:
    bool                         dynamicallyLinked_;
    BufferPtr                    heap_;

    // Mutated after dynamicallyLink:
    bool                         profilingEnabled_;
    FuncLabelVector              funcLabels_;

    class AutoMutateCode;

    uint32_t totalBytes() const;
    uint8_t* rawHeapPtr() const;
    uint8_t*& rawHeapPtr();
    WasmActivation*& activation();
    void specializeToHeap(ArrayBufferObjectMaybeShared* heap);
    void despecializeFromHeap(ArrayBufferObjectMaybeShared* heap);
    void sendCodeRangesToProfiler(JSContext* cx);
    MOZ_WARN_UNUSED_RESULT bool setProfilingEnabled(JSContext* cx, bool enabled);
    ImportExit& importToExit(const Import& import);

    enum CacheBool { NotLoadedFromCache = false, LoadedFromCache = true };
    enum ProfilingBool { ProfilingDisabled = false, ProfilingEnabled = true };

    static CacheablePod zeroPod();
    void init();
    Module(const CacheablePod& pod,
           UniqueCodePtr code,
           ImportVector&& imports,
           ExportVector&& exports,
           HeapAccessVector&& heapAccesses,
           CodeRangeVector&& codeRanges,
           CallSiteVector&& callSites,
           CacheableCharsVector&& funcNames,
           CacheableChars filename,
           CacheableTwoByteChars displayURL,
           CacheBool loadedFromCache,
           ProfilingBool profilingEnabled,
           FuncLabelVector&& funcLabels);

    template <class> friend struct js::MallocProvider;
    friend class js::WasmActivation;

  public:
    static const unsigned SizeOfImportExit = sizeof(ImportExit);
    static const unsigned OffsetOfImportExitFun = offsetof(ImportExit, fun);
    static const unsigned SizeOfEntryArg = sizeof(EntryArg);

    enum MutedBool { DontMuteErrors = false, MuteErrors = true };

    Module(CompileArgs args,
           uint32_t functionBytes,
           uint32_t codeBytes,
           uint32_t globalBytes,
           HeapUsage heapUsage,
           MutedBool mutedErrors,
           UniqueCodePtr code,
           ImportVector&& imports,
           ExportVector&& exports,
           HeapAccessVector&& heapAccesses,
           CodeRangeVector&& codeRanges,
           CallSiteVector&& callSites,
           CacheableCharsVector&& funcNames,
           CacheableChars filename,
           CacheableTwoByteChars displayURL);
    ~Module();
    void trace(JSTracer* trc);

    uint8_t* code() const { return code_.get(); }
    uint8_t* globalData() const { return code() + pod.codeBytes_; }
    uint32_t globalBytes() const { return pod.globalBytes_; }
    HeapUsage heapUsage() const { return pod.heapUsage_; }
    bool usesHeap() const { return UsesHeap(pod.heapUsage_); }
    bool hasSharedHeap() const { return pod.heapUsage_ == HeapUsage::Shared; }
    bool mutedErrors() const { return pod.mutedErrors_; }
    CompileArgs compileArgs() const;
    const ImportVector& imports() const { return imports_; }
    const ExportVector& exports() const { return exports_; }
    const char* functionName(uint32_t i) const { return funcNames_[i].get(); }
    const char* filename() const { return filename_.get(); }
    const char16_t* displayURL() const { return displayURL_.get(); }
    bool loadedFromCache() const { return loadedFromCache_; }
    bool staticallyLinked() const { return staticallyLinked_; }
    bool dynamicallyLinked() const { return dynamicallyLinked_; }

    // The range [0, functionBytes) is a subrange of [0, codeBytes) that
    // contains only function body code, not the stub code. This distinction is
    // used by the async interrupt handler to only interrupt when the pc is in
    // function code which, in turn, simplifies reasoning about how stubs
    // enter/exit.

    bool containsFunctionPC(void* pc) const;
    bool containsCodePC(void* pc) const;
    const CallSite* lookupCallSite(void* returnAddress) const;
    const CodeRange* lookupCodeRange(void* pc) const;
    const HeapAccess* lookupHeapAccess(void* pc) const;

    // This function transitions the module from an unlinked state to a
    // statically-linked state. The given StaticLinkData must have come from the
    // compilation of this module.

    bool staticallyLink(ExclusiveContext* cx, const StaticLinkData& linkData);

    // This function transitions the module from a statically-linked state to a
    // dynamically-linked state. If this module usesHeap(), a non-null heap
    // buffer must be given. The given import vector must match the module's
    // ImportVector.

    bool dynamicallyLink(JSContext* cx, Handle<ArrayBufferObjectMaybeShared*> heap,
                         const AutoVectorRooter<JSFunction*>& imports);

    // The wasm heap, established by dynamicallyLink.

    SharedMem<uint8_t*> heap() const;
    size_t heapLength() const;

    // The exports of a wasm module are called by preparing an array of
    // arguments (coerced to the corresponding types of the Export signature)
    // and calling the export's entry trampoline.

    bool callExport(JSContext* cx, uint32_t exportIndex, CallArgs args);

    // Initially, calls to imports in wasm code call out through the generic
    // callImport method. If the imported callee gets JIT compiled and the types
    // match up, callImport will patch the code to instead call through a thunk
    // directly into the JIT code. If the JIT code is released, the Module must
    // be notified so it can go back to the generic callImport.

    bool callImport(JSContext* cx, uint32_t importIndex, unsigned argc, const Value* argv,
                    MutableHandleValue rval);
    void deoptimizeImportExit(uint32_t importIndex);

    // At runtime, when $pc is in wasm function code (containsFunctionPC($pc)),
    // $pc may be moved abruptly to interrupt() or outOfBounds() by a signal
    // handler or SetContext() from another thread.

    uint8_t* interrupt() const { MOZ_ASSERT(staticallyLinked_); return interrupt_; }
    uint8_t* outOfBounds() const { MOZ_ASSERT(staticallyLinked_); return outOfBounds_; }

    // Each Module has a profilingEnabled state which is updated to match
    // SPSProfiler::enabled() on the next Module::callExport when there are no
    // frames from the Module on the stack. The ProfilingFrameIterator only
    // shows frames for Module activations that have profilingEnabled.

    bool profilingEnabled() const { return profilingEnabled_; }
    const char* profilingLabel(uint32_t funcIndex) const;

    // See WASM_DECLARE_SERIALIZABLE.
    size_t serializedSize() const;
    uint8_t* serialize(uint8_t* cursor) const;
    typedef UniquePtr<Module, JS::DeletePolicy<Module>> UniqueModule;
    static const uint8_t* deserialize(ExclusiveContext* cx, const uint8_t* cursor,
                                      UniqueModule* out);
    UniqueModule clone(JSContext* cx, const StaticLinkData& linkData) const;
    void addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* asmJSModuleCode,
                       size_t* asmJSModuleData);
};

} // namespace js
} // namespace wasm

#endif // wasm_module_h
