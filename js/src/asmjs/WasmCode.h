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
namespace wasm {

// A wasm CodeSegment owns the allocated executable code for a wasm module.
// CodeSegment passed to the Module constructor must be allocated via allocate.

class CodeSegment;
typedef UniquePtr<CodeSegment> UniqueCodeSegment;

class CodeSegment
{
    uint8_t* bytes_;
    uint32_t codeLength_;
    uint32_t globalDataLength_;

    CodeSegment(const CodeSegment&) = delete;
    void operator=(const CodeSegment&) = delete;

  public:
    static UniqueCodeSegment allocate(ExclusiveContext* cx, uint32_t codeLength, uint32_t dataLength);
    static UniqueCodeSegment clone(ExclusiveContext* cx, const CodeSegment& code);
    CodeSegment() : bytes_(nullptr), codeLength_(0), globalDataLength_(0) {}
    ~CodeSegment();

    uint8_t* code() const { return bytes_; }
    uint8_t* globalData() const { return bytes_ + codeLength_; }
    uint32_t codeLength() const { return codeLength_; }
    uint32_t globalDataLength() const { return globalDataLength_; }
    uint32_t totalLength() const { return codeLength_ + globalDataLength_; }

    WASM_DECLARE_SERIALIZABLE(CodeSegment)
};

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
    uint32_t              functionLength;
    ModuleKind            kind;
    HeapUsage             heapUsage;
    CompileArgs           compileArgs;

    MetadataCacheablePod() { mozilla::PodZero(this); }
};

struct Metadata : RefCounted<Metadata>, MetadataCacheablePod
{
    MetadataCacheablePod& pod() { return *this; }
    const MetadataCacheablePod& pod() const { return *this; }

    ImportVector          imports;
    ExportVector          exports;
    HeapAccessVector      heapAccesses;
    CodeRangeVector       codeRanges;
    CallSiteVector        callSites;
    CallThunkVector       callThunks;
    CacheableCharsVector  prettyFuncNames;
    CacheableChars        filename;

    WASM_DECLARE_SERIALIZABLE(Metadata);
};

typedef RefPtr<Metadata> MutableMetadata;
typedef RefPtr<const Metadata> SharedMetadata;

} // namespace wasm
} // namespace js

#endif // wasm_code_h
