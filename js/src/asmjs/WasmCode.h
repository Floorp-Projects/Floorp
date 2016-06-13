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

#include "asmjs/WasmTypes.h"

namespace js {

struct AsmJSMetadata;

namespace wasm {

struct LinkData;
struct Metadata;

// A wasm CodeSegment owns the allocated executable code for a wasm module.

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

    // These are pointers into code for two stubs used for asynchronous
    // signal-handler control-flow transfer.
    uint8_t* interruptCode_;
    uint8_t* outOfBoundsCode_;

    // The profiling mode may be changed dynamically.
    bool profilingEnabled_;

    CodeSegment() { PodZero(this); }
    template <class> friend struct js::MallocProvider;

    CodeSegment(const CodeSegment&) = delete;
    CodeSegment(CodeSegment&&) = delete;
    void operator=(const CodeSegment&) = delete;
    void operator=(CodeSegment&&) = delete;

  public:
    static UniqueCodeSegment create(ExclusiveContext* cx,
                                    const Bytes& code,
                                    const LinkData& linkData,
                                    const Metadata& metadata,
                                    uint8_t* heapBase,
                                    uint32_t heapLength);
    ~CodeSegment();

    uint8_t* code() const { return bytes_; }
    uint8_t* globalData() const { return bytes_ + codeLength_; }
    uint32_t codeLength() const { return codeLength_; }
    uint32_t globalDataLength() const { return globalDataLength_; }
    uint32_t totalLength() const { return codeLength_ + globalDataLength_; }

    uint8_t* interruptCode() const { return interruptCode_; }
    uint8_t* outOfBoundsCode() const { return outOfBoundsCode_; }

    // The range [0, functionBytes) is a subrange of [0, codeBytes) that
    // contains only function body code, not the stub code. This distinction is
    // used by the async interrupt handler to only interrupt when the pc is in
    // function code which, in turn, simplifies reasoning about how stubs
    // enter/exit.

    bool containsFunctionPC(void* pc) const {
        return pc >= code() && pc < (code() + functionCodeLength_);
    }
    bool containsCodePC(void* pc) const {
        return pc >= code() && pc < (code() + codeLength_);
    }

    WASM_DECLARE_SERIALIZABLE(CodeSegment)
};

// This reusable base class factors out the logic for a resource that is shared
// by multiple instances/modules but should only be counted once when computing
// about:memory stats.

template <class T>
struct ShareableBase : RefCounted<T>
{
    using SeenSet = HashSet<const T*, DefaultHasher<const T*>, SystemAllocPolicy>;

    size_t sizeOfIncludingThisIfNotSeen(MallocSizeOf mallocSizeOf, SeenSet* seen) const {
        const T* self = static_cast<const T*>(this);
        typename SeenSet::AddPtr p = seen->lookupForAdd(self);
        if (p)
            return 0;
        bool ok = seen->add(p, self);
        (void)ok;  // oh well
        return mallocSizeOf(self) + self->sizeOfExcludingThis(mallocSizeOf);
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

// An Export represents a single function inside a wasm Module that has been
// exported one or more times.

class Export
{
    Sig sig_;
    struct CacheablePod {
        uint32_t funcIndex_;
        uint32_t stubOffset_;
    } pod;

  public:
    Export() = default;
    explicit Export(Sig&& sig, uint32_t funcIndex)
      : sig_(Move(sig))
    {
        pod.funcIndex_ = funcIndex;
        pod.stubOffset_ = UINT32_MAX;
    }
    void initStubOffset(uint32_t stubOffset) {
        MOZ_ASSERT(pod.stubOffset_ == UINT32_MAX);
        pod.stubOffset_ = stubOffset;
    }

    const Sig& sig() const {
        return sig_;
    }
    uint32_t funcIndex() const {
        return pod.funcIndex_;
    }
    uint32_t stubOffset() const {
        MOZ_ASSERT(pod.stubOffset_ != UINT32_MAX);
        return pod.stubOffset_;
    }

    WASM_DECLARE_SERIALIZABLE(Export)
};

typedef Vector<Export, 0, SystemAllocPolicy> ExportVector;

// An Import describes a wasm module import. Currently, only functions can be
// imported in wasm. A function import includes the signature used within the
// module to call it.

class Import
{
    Sig sig_;
    struct CacheablePod {
        uint32_t exitGlobalDataOffset_;
        uint32_t interpExitCodeOffset_;
        uint32_t jitExitCodeOffset_;
    } pod;

  public:
    Import() = default;
    Import(Sig&& sig, uint32_t exitGlobalDataOffset)
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

    const Sig& sig() const {
        return sig_;
    }
    uint32_t exitGlobalDataOffset() const {
        return pod.exitGlobalDataOffset_;
    }
    uint32_t interpExitCodeOffset() const {
        return pod.interpExitCodeOffset_;
    }
    uint32_t jitExitCodeOffset() const {
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

// Metadata holds all the data that is needed to describe compiled wasm code
// at runtime (as opposed to data that is only used to statically link or
// instantiate a module).
//
// Metadata is built incrementally by ModuleGenerator and then shared immutably
// between modules.

struct MetadataCacheablePod
{
    ModuleKind            kind;
    HeapUsage             heapUsage;
    CompileArgs           compileArgs;

    MetadataCacheablePod() { mozilla::PodZero(this); }
};

struct Metadata : ShareableBase<Metadata>, MetadataCacheablePod
{
    virtual ~Metadata() {}

    MetadataCacheablePod& pod() { return *this; }
    const MetadataCacheablePod& pod() const { return *this; }

    ImportVector          imports;
    ExportVector          exports;
    MemoryAccessVector    memoryAccesses;
    BoundsCheckVector     boundsChecks;
    CodeRangeVector       codeRanges;
    CallSiteVector        callSites;
    CallThunkVector       callThunks;
    CacheableCharsVector  funcNames;
    CacheableChars        filename;

    bool usesHeap() const { return UsesHeap(heapUsage); }
    bool hasSharedHeap() const { return heapUsage == HeapUsage::Shared; }

    const char* getFuncName(ExclusiveContext* cx, uint32_t funcIndex, UniqueChars* owner) const;
    JSAtom* getFuncAtom(JSContext* cx, uint32_t funcIndex) const;

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

    WASM_DECLARE_SERIALIZABLE_VIRTUAL(Metadata);
};

typedef RefPtr<Metadata> MutableMetadata;
typedef RefPtr<const Metadata> SharedMetadata;

} // namespace wasm
} // namespace js

#endif // wasm_code_h
