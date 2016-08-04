/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2016 Mozilla Foundation
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

#ifndef wasm_code_h
#define wasm_code_h

#include "asmjs/WasmBinaryToExperimentalText.h"
#include "asmjs/WasmTypes.h"

namespace js {

struct AsmJSMetadata;

namespace wasm {

struct LinkData;
struct Metadata;

// A wasm CodeSegment owns the allocated executable code for a wasm module.
// This allocation also currently includes the global data segment, which allows
// RIP-relative access to global data on some architectures, but this will
// change in the future to give global data its own allocation.

class CodeSegment;
typedef UniquePtr<CodeSegment> UniqueCodeSegment;

class CodeSegment
{
    // bytes_ points to a single allocation with two contiguous ranges:
    // executable machine code in the range [0, codeLength) and global data in
    // the range [codeLength, codeLength + globalDataLength). The range
    // [0, functionCodeLength) is the subrange of [0, codeLength) which contains
    // function code.
    uint8_t* bytes_;
    uint32_t functionCodeLength_;
    uint32_t codeLength_;
    uint32_t globalDataLength_;

    // These are pointers into code for stubs used for asynchronous
    // signal-handler control-flow transfer.
    uint8_t* interruptCode_;
    uint8_t* badIndirectCallCode_;
    uint8_t* outOfBoundsCode_;
    uint8_t* unalignedAccessCode_;

    // The profiling mode may be changed dynamically.
    bool profilingEnabled_;

    CodeSegment() { PodZero(this); }
    template <class> friend struct js::MallocProvider;

    CodeSegment(const CodeSegment&) = delete;
    CodeSegment(CodeSegment&&) = delete;
    void operator=(const CodeSegment&) = delete;
    void operator=(CodeSegment&&) = delete;

  public:
    static UniqueCodeSegment create(JSContext* cx,
                                    const Bytes& code,
                                    const LinkData& linkData,
                                    const Metadata& metadata,
                                    HandleWasmMemoryObject memory);
    ~CodeSegment();

    uint8_t* base() const { return bytes_; }
    uint8_t* globalData() const { return bytes_ + codeLength_; }
    uint32_t codeLength() const { return codeLength_; }
    uint32_t globalDataLength() const { return globalDataLength_; }
    uint32_t totalLength() const { return codeLength_ + globalDataLength_; }

    uint8_t* interruptCode() const { return interruptCode_; }
    uint8_t* badIndirectCallCode() const { return badIndirectCallCode_; }
    uint8_t* outOfBoundsCode() const { return outOfBoundsCode_; }
    uint8_t* unalignedAccessCode() const { return unalignedAccessCode_; }

    // The range [0, functionBytes) is a subrange of [0, codeBytes) that
    // contains only function body code, not the stub code. This distinction is
    // used by the async interrupt handler to only interrupt when the pc is in
    // function code which, in turn, simplifies reasoning about how stubs
    // enter/exit.

    bool containsFunctionPC(const void* pc) const {
        return pc >= base() && pc < (base() + functionCodeLength_);
    }
    bool containsCodePC(const void* pc) const {
        return pc >= base() && pc < (base() + codeLength_);
    }
};

// ShareableBytes is a ref-counted vector of bytes which are incrementally built
// during compilation and then immutably shared.

struct ShareableBytes : ShareableBase<ShareableBytes>
{
    // Vector is 'final', so instead make Vector a member and add boilerplate.
    Bytes bytes;
    size_t sizeOfExcludingThis(MallocSizeOf m) const { return bytes.sizeOfExcludingThis(m); }
    const uint8_t* begin() const { return bytes.begin(); }
    const uint8_t* end() const { return bytes.end(); }
    bool append(const uint8_t *p, uint32_t ct) { return bytes.append(p, ct); }
};

typedef RefPtr<ShareableBytes> MutableBytes;
typedef RefPtr<const ShareableBytes> SharedBytes;

// A FuncExport represents a single function inside a wasm Module that has been
// exported one or more times. A FuncExport represents an internal entry point
// that can be called via function-index by Instance::callExport(). To allow
// O(log(n)) lookup of a FuncExport by function-index, the FuncExportVector
// is stored sorted by function index.

class FuncExport
{
    Sig sig_;
    struct CacheablePod {
        uint32_t funcIndex_;
        uint32_t entryOffset_;
        uint32_t tableEntryOffset_;
    } pod;

  public:
    FuncExport() = default;
    explicit FuncExport(Sig&& sig, uint32_t funcIndex, uint32_t tableEntryOffset)
      : sig_(Move(sig))
    {
        pod.funcIndex_ = funcIndex;
        pod.entryOffset_ = UINT32_MAX;
        pod.tableEntryOffset_ = tableEntryOffset;
    }
    void initEntryOffset(uint32_t entryOffset) {
        MOZ_ASSERT(pod.entryOffset_ == UINT32_MAX);
        pod.entryOffset_ = entryOffset;
    }

    const Sig& sig() const {
        return sig_;
    }
    uint32_t funcIndex() const {
        return pod.funcIndex_;
    }
    uint32_t entryOffset() const {
        MOZ_ASSERT(pod.entryOffset_ != UINT32_MAX);
        return pod.entryOffset_;
    }
    uint32_t tableEntryOffset() const {
        return pod.tableEntryOffset_;
    }

    WASM_DECLARE_SERIALIZABLE(FuncExport)
};

typedef Vector<FuncExport, 0, SystemAllocPolicy> FuncExportVector;

// An FuncImport contains the runtime metadata needed to implement a call to an
// imported function. Each function import has two call stubs: an optimized path
// into JIT code and a slow path into the generic C++ js::Invoke and these
// offsets of these stubs are stored so that function-import callsites can be
// dynamically patched at runtime.

class FuncImport
{
    Sig sig_;
    struct CacheablePod {
        uint32_t tlsDataOffset_;
        uint32_t interpExitCodeOffset_;
        uint32_t jitExitCodeOffset_;
    } pod;

  public:
    FuncImport() {
        memset(&pod, 0, sizeof(CacheablePod));
    }

    FuncImport(Sig&& sig, uint32_t tlsDataOffset)
      : sig_(Move(sig))
    {
        pod.tlsDataOffset_ = tlsDataOffset;
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

    const Sig& sig() const {
        return sig_;
    }
    uint32_t tlsDataOffset() const {
        return pod.tlsDataOffset_;
    }
    uint32_t interpExitCodeOffset() const {
        return pod.interpExitCodeOffset_;
    }
    uint32_t jitExitCodeOffset() const {
        return pod.jitExitCodeOffset_;
    }

    WASM_DECLARE_SERIALIZABLE(FuncImport)
};

typedef Vector<FuncImport, 0, SystemAllocPolicy> FuncImportVector;

// TableDesc contains the metadata describing a table as well as the
// module-specific offset of the table's base pointer in global memory.
// The element kind of this table. Currently, wasm only has "any function" and
// asm.js only "typed function".

enum class TableKind
{
    AnyFunction,
    TypedFunction
};

struct TableDesc
{
    TableKind kind;
    uint32_t globalDataOffset;
    uint32_t initial;
    uint32_t maximum;

    TableDesc() { PodZero(this); }
    explicit TableDesc(TableKind kind) : kind(kind), globalDataOffset(0), initial(0), maximum(0) {}
};

WASM_DECLARE_POD_VECTOR(TableDesc, TableDescVector)

// A CodeRange describes a single contiguous range of code within a wasm
// module's code segment. A CodeRange describes what the code does and, for
// function bodies, the name and source coordinates of the function.

class CodeRange
{
  public:
    enum Kind { Function, Entry, ImportJitExit, ImportInterpExit, Inline, CallThunk };

  private:
    // All fields are treated as cacheable POD:
    uint32_t begin_;
    uint32_t profilingReturn_;
    uint32_t end_;
    uint32_t funcIndex_;
    uint32_t funcLineOrBytecode_;
    uint8_t funcBeginToTableEntry_;
    uint8_t funcBeginToTableProfilingJump_;
    uint8_t funcBeginToNonProfilingEntry_;
    uint8_t funcProfilingJumpToProfilingReturn_;
    uint8_t funcProfilingEpilogueToProfilingReturn_;
    Kind kind_ : 8;

  public:
    CodeRange() = default;
    CodeRange(Kind kind, Offsets offsets);
    CodeRange(Kind kind, ProfilingOffsets offsets);
    CodeRange(uint32_t funcIndex, uint32_t lineOrBytecode, FuncOffsets offsets);

    // All CodeRanges have a begin and end.

    uint32_t begin() const {
        return begin_;
    }
    uint32_t end() const {
        return end_;
    }

    // Other fields are only available for certain CodeRange::Kinds.

    Kind kind() const {
        return kind_;
    }

    bool isFunction() const {
        return kind() == Function;
    }
    bool isImportExit() const {
        return kind() == ImportJitExit || kind() == ImportInterpExit;
    }
    bool isInline() const {
        return kind() == Inline;
    }

    // Every CodeRange except entry and inline stubs has a profiling return
    // which is used for asynchronous profiling to determine the frame pointer.

    uint32_t profilingReturn() const {
        MOZ_ASSERT(isFunction() || isImportExit());
        return profilingReturn_;
    }

    // Functions have offsets which allow patching to selectively execute
    // profiling prologues/epilogues.

    uint32_t funcProfilingEntry() const {
        MOZ_ASSERT(isFunction());
        return begin();
    }
    uint32_t funcTableEntry() const {
        MOZ_ASSERT(isFunction());
        return begin_ + funcBeginToTableEntry_;
    }
    uint32_t funcTableProfilingJump() const {
        MOZ_ASSERT(isFunction());
        return begin_ + funcBeginToTableProfilingJump_;
    }
    uint32_t funcNonProfilingEntry() const {
        MOZ_ASSERT(isFunction());
        return begin_ + funcBeginToNonProfilingEntry_;
    }
    uint32_t funcProfilingJump() const {
        MOZ_ASSERT(isFunction());
        return profilingReturn_ - funcProfilingJumpToProfilingReturn_;
    }
    uint32_t funcProfilingEpilogue() const {
        MOZ_ASSERT(isFunction());
        return profilingReturn_ - funcProfilingEpilogueToProfilingReturn_;
    }
    uint32_t funcIndex() const {
        MOZ_ASSERT(isFunction());
        return funcIndex_;
    }
    uint32_t funcLineOrBytecode() const {
        MOZ_ASSERT(isFunction());
        return funcLineOrBytecode_;
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

WASM_DECLARE_POD_VECTOR(CodeRange, CodeRangeVector)

// A CallThunk describes the offset and target of thunks so that they may be
// patched at runtime when profiling is toggled. Thunks are emitted to connect
// callsites that are too far away from callees to fit in a single call
// instruction's relative offset.

struct CallThunk
{
    uint32_t offset;
    union {
        uint32_t funcIndex;
        uint32_t codeRangeIndex;
    } u;

    CallThunk(uint32_t offset, uint32_t funcIndex) : offset(offset) { u.funcIndex = funcIndex; }
    CallThunk() = default;
};

WASM_DECLARE_POD_VECTOR(CallThunk, CallThunkVector)

// CacheableChars is used to cacheably store UniqueChars.

struct CacheableChars : UniqueChars
{
    CacheableChars() = default;
    explicit CacheableChars(char* ptr) : UniqueChars(ptr) {}
    MOZ_IMPLICIT CacheableChars(UniqueChars&& rhs) : UniqueChars(Move(rhs)) {}
    WASM_DECLARE_SERIALIZABLE(CacheableChars)
};

typedef Vector<CacheableChars, 0, SystemAllocPolicy> CacheableCharsVector;

// A wasm module can either use no memory, a unshared memory (ArrayBuffer) or
// shared memory (SharedArrayBuffer).

enum class MemoryUsage
{
    None = false,
    Unshared = 1,
    Shared = 2
};

static inline bool
UsesMemory(MemoryUsage memoryUsage)
{
    return bool(memoryUsage);
}

// NameInBytecode represents a name that is embedded in the wasm bytecode.
// The presence of NameInBytecode implies that bytecode has been kept.

struct NameInBytecode
{
    uint32_t offset;
    uint32_t length;

    NameInBytecode() = default;
    NameInBytecode(uint32_t offset, uint32_t length) : offset(offset), length(length) {}
};

typedef Vector<NameInBytecode, 0, SystemAllocPolicy> NameInBytecodeVector;
typedef Vector<char16_t, 64> TwoByteName;

// Metadata holds all the data that is needed to describe compiled wasm code
// at runtime (as opposed to data that is only used to statically link or
// instantiate a module).
//
// Metadata is built incrementally by ModuleGenerator and then shared immutably
// between modules.

class MetadataCacheablePod
{
    static const uint32_t NO_START_FUNCTION = UINT32_MAX;
    static_assert(NO_START_FUNCTION > MaxFuncs, "sentinel value");

    uint32_t              startFuncIndex_;

  public:
    ModuleKind            kind;
    MemoryUsage           memoryUsage;
    uint32_t              minMemoryLength;
    uint32_t              maxMemoryLength;

    MetadataCacheablePod() {
        mozilla::PodZero(this);
        startFuncIndex_ = NO_START_FUNCTION;
    }

    bool hasStartFunction() const {
        return startFuncIndex_ != NO_START_FUNCTION;
    }
    void initStartFuncIndex(uint32_t i) {
        MOZ_ASSERT(!hasStartFunction());
        startFuncIndex_ = i;
        MOZ_ASSERT(hasStartFunction());
    }
    uint32_t startFuncIndex() const {
        MOZ_ASSERT(hasStartFunction());
        return startFuncIndex_;
    }
};

struct Metadata : ShareableBase<Metadata>, MetadataCacheablePod
{
    virtual ~Metadata() {}

    MetadataCacheablePod& pod() { return *this; }
    const MetadataCacheablePod& pod() const { return *this; }

    FuncImportVector      funcImports;
    FuncExportVector      funcExports;
    SigWithIdVector       sigIds;
    GlobalDescVector      globals;
    TableDescVector       tables;
    MemoryAccessVector    memoryAccesses;
    BoundsCheckVector     boundsChecks;
    CodeRangeVector       codeRanges;
    CallSiteVector        callSites;
    CallThunkVector       callThunks;
    NameInBytecodeVector  funcNames;
    CacheableChars        filename;
    Assumptions           assumptions;

    bool usesMemory() const { return UsesMemory(memoryUsage); }
    bool hasSharedMemory() const { return memoryUsage == MemoryUsage::Shared; }

    const FuncExport& lookupFuncExport(uint32_t funcIndex) const;

    // AsmJSMetadata derives Metadata iff isAsmJS(). Mostly this distinction is
    // encapsulated within AsmJS.cpp, but the additional virtual functions allow
    // asm.js to override wasm behavior in the handful of cases that can't be
    // easily encapsulated by AsmJS.cpp.

    bool isAsmJS() const {
        return kind == ModuleKind::AsmJS;
    }
    const AsmJSMetadata& asAsmJS() const {
        MOZ_ASSERT(isAsmJS());
        return *(const AsmJSMetadata*)this;
    }
    virtual bool mutedErrors() const {
        return false;
    }
    virtual const char16_t* displayURL() const {
        return nullptr;
    }
    virtual ScriptSource* maybeScriptSource() const {
        return nullptr;
    }
    virtual bool getFuncName(JSContext* cx, const Bytes* maybeBytecode, uint32_t funcIndex,
                             TwoByteName* name) const;

    WASM_DECLARE_SERIALIZABLE_VIRTUAL(Metadata);
};

typedef RefPtr<Metadata> MutableMetadata;
typedef RefPtr<const Metadata> SharedMetadata;

// Code objects own executable code and the metadata that describes it. At the
// moment, Code objects are owned uniquely by instances since CodeSegments are
// not shareable. However, once this restriction is removed, a single Code
// object will be shared between a module and all its instances.

class Code
{
    const UniqueCodeSegment  segment_;
    const SharedMetadata     metadata_;
    const SharedBytes        maybeBytecode_;
    UniqueGeneratedSourceMap maybeSourceMap_;
    CacheableCharsVector     funcLabels_;
    bool                     profilingEnabled_;

  public:
    Code(UniqueCodeSegment segment,
         const Metadata& metadata,
         const ShareableBytes* maybeBytecode);

    const CodeSegment& segment() const { return *segment_; }
    const Metadata& metadata() const { return *metadata_; }

    // Frame iterator support:

    const CallSite* lookupCallSite(void* returnAddress) const;
    const CodeRange* lookupRange(void* pc) const;
#ifdef ASMJS_MAY_USE_SIGNAL_HANDLERS
    const MemoryAccess* lookupMemoryAccess(void* pc) const;
#endif

    // Return the name associated with a given function index, or generate one
    // if none was given by the module.

    bool getFuncName(JSContext* cx, uint32_t funcIndex, TwoByteName* name) const;
    JSAtom* getFuncAtom(JSContext* cx, uint32_t funcIndex) const;

    // If the source bytecode was saved when this Code was constructed, this
    // method will render the binary as text. Otherwise, a diagnostic string
    // will be returned.

    JSString* createText(JSContext* cx);
    bool getLineOffsets(size_t lineno, Vector<uint32_t>& offsets) const;

    // Each Code has a profiling mode that is updated to match the runtime's
    // profiling mode when there are no other activations of the code live on
    // the stack. Once in profiling mode, ProfilingFrameIterator can be used to
    // asynchronously walk the stack. Otherwise, the ProfilingFrameIterator will
    // skip any activations of this code.

    MOZ_MUST_USE bool ensureProfilingState(JSContext* cx, bool enabled);
    bool profilingEnabled() const { return profilingEnabled_; }
    const char* profilingLabel(uint32_t funcIndex) const { return funcLabels_[funcIndex].get(); }

    // about:memory reporting:

    void addSizeOfMisc(MallocSizeOf mallocSizeOf,
                       Metadata::SeenSet* seenMetadata,
                       ShareableBytes::SeenSet* seenBytes,
                       size_t* code,
                       size_t* data) const;

    WASM_DECLARE_SERIALIZABLE(Code);
};

typedef UniquePtr<Code> UniqueCode;

} // namespace wasm
} // namespace js

#endif // wasm_code_h
