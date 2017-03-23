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

#include "js/HashTable.h"
#include "wasm/WasmTypes.h"

namespace js {

struct AsmJSMetadata;
class Debugger;
class WasmActivation;
class WasmBreakpoint;
class WasmBreakpointSite;
class WasmInstanceObject;

namespace wasm {

struct LinkData;
struct Metadata;
class FrameIterator;

// A wasm CodeSegment owns the allocated executable code for a wasm module.

class CodeSegment;
typedef UniquePtr<CodeSegment> UniqueCodeSegment;

class CodeSegment
{
    // bytes_ points to a single allocation of executable machine code in
    // the range [0, length_).  The range [0, functionLength_) is
    // the subrange of [0, length_) which contains function code.
    uint8_t* bytes_;
    uint32_t functionLength_;
    uint32_t length_;

    // These are pointers into code for stubs used for asynchronous
    // signal-handler control-flow transfer.
    uint8_t* interruptCode_;
    uint8_t* outOfBoundsCode_;
    uint8_t* unalignedAccessCode_;

  public:
#ifdef MOZ_VTUNE
    unsigned vtune_method_id_; // Zero if unset.
#endif

  protected:
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
    uint32_t length() const { return length_; }

    uint8_t* interruptCode() const { return interruptCode_; }
    uint8_t* outOfBoundsCode() const { return outOfBoundsCode_; }
    uint8_t* unalignedAccessCode() const { return unalignedAccessCode_; }

    // The range [0, functionBytes) is a subrange of [0, codeBytes) that
    // contains only function body code, not the stub code. This distinction is
    // used by the async interrupt handler to only interrupt when the pc is in
    // function code which, in turn, simplifies reasoning about how stubs
    // enter/exit.

    bool containsFunctionPC(const void* pc) const {
        return pc >= base() && pc < (base() + functionLength_);
    }
    bool containsCodePC(const void* pc) const {
        return pc >= base() && pc < (base() + length_);
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
    size_t length() const { return bytes.length(); }
    bool append(const uint8_t *p, uint32_t ct) { return bytes.append(p, ct); }
};

typedef RefPtr<ShareableBytes> MutableBytes;
typedef RefPtr<const ShareableBytes> SharedBytes;

// A FuncExport represents a single function definition inside a wasm Module
// that has been exported one or more times. A FuncExport represents an
// internal entry point that can be called via function definition index by
// Instance::callExport(). To allow O(log(n)) lookup of a FuncExport by
// function definition index, the FuncExportVector is stored sorted by
// function definition index.

class FuncExport
{
    Sig sig_;
    MOZ_INIT_OUTSIDE_CTOR struct CacheablePod {
        uint32_t funcIndex_;
        uint32_t codeRangeIndex_;
        uint32_t entryOffset_;
    } pod;

  public:
    FuncExport() = default;
    explicit FuncExport(Sig&& sig,
                        uint32_t funcIndex,
                        uint32_t codeRangeIndex)
      : sig_(Move(sig))
    {
        pod.funcIndex_ = funcIndex;
        pod.codeRangeIndex_ = codeRangeIndex;
        pod.entryOffset_ = UINT32_MAX;
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
    uint32_t codeRangeIndex() const {
        return pod.codeRangeIndex_;
    }
    uint32_t entryOffset() const {
        MOZ_ASSERT(pod.entryOffset_ != UINT32_MAX);
        return pod.entryOffset_;
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

// A CodeRange describes a single contiguous range of code within a wasm
// module's code segment. A CodeRange describes what the code does and, for
// function bodies, the name and source coordinates of the function.

class CodeRange
{
  public:
    enum Kind {
        Function,          // function definition
        Entry,             // calls into wasm from C++
        ImportJitExit,     // fast-path calling from wasm into JIT code
        ImportInterpExit,  // slow-path calling from wasm into C++ interp
        TrapExit,          // calls C++ to report and jumps to throw stub
        DebugTrap,         // calls C++ to handle debug event such as
                           // enter/leave frame or breakpoint
        FarJumpIsland,     // inserted to connect otherwise out-of-range insns
        Inline,            // stub that is jumped-to within prologue/epilogue
        Throw              // special stack-unwinding stub
    };

  private:
    // All fields are treated as cacheable POD:
    uint32_t begin_;
    uint32_t ret_;
    uint32_t end_;
    uint32_t funcIndex_;
    uint32_t funcLineOrBytecode_;
    uint8_t funcBeginToNormalEntry_;
    Kind kind_ : 8;

  public:
    CodeRange() = default;
    CodeRange(Kind kind, Offsets offsets);
    CodeRange(Kind kind, CallableOffsets offsets);
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
    bool isTrapExit() const {
        return kind() == TrapExit;
    }
    bool isInline() const {
        return kind() == Inline;
    }
    bool isThunk() const {
        return kind() == FarJumpIsland;
    }

    // Every CodeRange except entry and inline stubs are callable and have a
    // return statement. Asynchronous frame iteration needs to know the offset
    // of the return instruction to calculate the frame pointer.

    uint32_t ret() const {
        MOZ_ASSERT(isFunction() || isImportExit() || isTrapExit());
        return ret_;
    }

    // Function CodeRanges have two entry points: one for normal calls (with a
    // known signature) and one for table calls (which involves dynamic
    // signature checking).

    uint32_t funcTableEntry() const {
        MOZ_ASSERT(isFunction());
        return begin_;
    }
    uint32_t funcNormalEntry() const {
        MOZ_ASSERT(isFunction());
        return begin_ + funcBeginToNormalEntry_;
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
    NameInBytecode(uint32_t offset, uint32_t length)
      : offset(offset), length(length)
    {}
};

typedef Vector<NameInBytecode, 0, SystemAllocPolicy> NameInBytecodeVector;

// CustomSection represents a custom section in the bytecode which can be
// extracted via Module.customSections. The (offset, length) pair does not
// include the custom section name.

struct CustomSection
{
    NameInBytecode name;
    uint32_t offset;
    uint32_t length;

    CustomSection() = default;
    CustomSection(NameInBytecode name, uint32_t offset, uint32_t length)
      : name(name), offset(offset), length(length)
    {}
};

typedef Vector<CustomSection, 0, SystemAllocPolicy> CustomSectionVector;
typedef Vector<ValTypeVector, 0, SystemAllocPolicy> FuncArgTypesVector;
typedef Vector<ExprType, 0, SystemAllocPolicy> FuncReturnTypesVector;

// Metadata holds all the data that is needed to describe compiled wasm code
// at runtime (as opposed to data that is only used to statically link or
// instantiate a module).
//
// Metadata is built incrementally by ModuleGenerator and then shared immutably
// between modules.

struct MetadataCacheablePod
{
    ModuleKind            kind;
    MemoryUsage           memoryUsage;
    uint32_t              minMemoryLength;
    Maybe<uint32_t>       maxMemoryLength;
    Maybe<uint32_t>       startFuncIndex;

    explicit MetadataCacheablePod(ModuleKind kind)
      : kind(kind),
        memoryUsage(MemoryUsage::None),
        minMemoryLength(0)
    {}
};

struct Metadata : ShareableBase<Metadata>, MetadataCacheablePod
{
    explicit Metadata(ModuleKind kind = ModuleKind::Wasm) : MetadataCacheablePod(kind) {}
    virtual ~Metadata() {}

    MetadataCacheablePod& pod() { return *this; }
    const MetadataCacheablePod& pod() const { return *this; }

    FuncImportVector      funcImports;
    FuncExportVector      funcExports;
    SigWithIdVector       sigIds;
    GlobalDescVector      globals;
    TableDescVector       tables;
    MemoryAccessVector    memoryAccesses;
    CodeRangeVector       codeRanges;
    CallSiteVector        callSites;
    NameInBytecodeVector  funcNames;
    CustomSectionVector   customSections;
    CacheableChars        filename;

    // Debug-enabled code is not serialized.
    bool                  debugEnabled;
    Uint32Vector          debugTrapFarJumpOffsets;
    Uint32Vector          debugFuncToCodeRange;
    FuncArgTypesVector    debugFuncArgTypes;
    FuncReturnTypesVector debugFuncReturnTypes;

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
    virtual bool getFuncName(const Bytes* maybeBytecode, uint32_t funcIndex, UTF8Bytes* name) const;

    WASM_DECLARE_SERIALIZABLE_VIRTUAL(Metadata);
};

typedef RefPtr<Metadata> MutableMetadata;
typedef RefPtr<const Metadata> SharedMetadata;

// The generated source location for the AST node/expression. The offset field refers
// an offset in an binary format file.

struct ExprLoc
{
    uint32_t lineno;
    uint32_t column;
    uint32_t offset;
    ExprLoc() : lineno(0), column(0), offset(0) {}
    ExprLoc(uint32_t lineno_, uint32_t column_, uint32_t offset_)
      : lineno(lineno_), column(column_), offset(offset_)
    {}
};

typedef Vector<ExprLoc, 0, SystemAllocPolicy> ExprLocVector;
typedef Vector<uint32_t, 0, SystemAllocPolicy> ExprLocIndexVector;

// The generated source map for WebAssembly binary file. This map is generated during
// building the text buffer (see BinaryToExperimentalText).

class GeneratedSourceMap
{
    ExprLocVector exprlocs_;
    UniquePtr<ExprLocIndexVector> sortedByOffsetExprLocIndices_;
    uint32_t totalLines_;

  public:
    explicit GeneratedSourceMap() : totalLines_(0) {}
    ExprLocVector& exprlocs() { return exprlocs_; }

    uint32_t totalLines() { return totalLines_; }
    void setTotalLines(uint32_t val) { totalLines_ = val; }

    bool searchLineByOffset(JSContext* cx, uint32_t offset, size_t* exprlocIndex);
};

typedef UniquePtr<GeneratedSourceMap> UniqueGeneratedSourceMap;
typedef HashMap<uint32_t, uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy> StepModeCounters;
typedef HashMap<uint32_t, WasmBreakpointSite*, DefaultHasher<uint32_t>, SystemAllocPolicy> WasmBreakpointSiteMap;

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

    // Mutated at runtime:
    CacheableCharsVector     profilingLabels_;

    // State maintained when debugging is enabled:

    uint32_t                 enterAndLeaveFrameTrapsCounter_;
    WasmBreakpointSiteMap    breakpointSites_;
    StepModeCounters         stepModeCounters_;

    void toggleDebugTrap(uint32_t offset, bool enabled);
    bool ensureSourceMap(JSContext* cx);

  public:
    Code(UniqueCodeSegment segment,
         const Metadata& metadata,
         const ShareableBytes* maybeBytecode);

    CodeSegment& segment() { return *segment_; }
    const CodeSegment& segment() const { return *segment_; }
    const Metadata& metadata() const { return *metadata_; }

    // Frame iterator support:

    const CallSite* lookupCallSite(void* returnAddress) const;
    const CodeRange* lookupRange(void* pc) const;
    const MemoryAccess* lookupMemoryAccess(void* pc) const;

    // Return the name associated with a given function index, or generate one
    // if none was given by the module.

    bool getFuncName(uint32_t funcIndex, UTF8Bytes* name) const;
    JSAtom* getFuncAtom(JSContext* cx, uint32_t funcIndex) const;

    // If the source bytecode was saved when this Code was constructed, this
    // method will render the binary as text. Otherwise, a diagnostic string
    // will be returned.

    JSString* createText(JSContext* cx);
    bool getLineOffsets(JSContext* cx, size_t lineno, Vector<uint32_t>* offsets);
    bool getOffsetLocation(JSContext* cx, uint32_t offset, bool* found, size_t* lineno, size_t* column);
    bool totalSourceLines(JSContext* cx, uint32_t* count);

    // To save memory, profilingLabels_ are generated lazily when profiling mode
    // is enabled.

    void ensureProfilingLabels(bool profilingEnabled);
    const char* profilingLabel(uint32_t funcIndex) const;

    // The Code can track enter/leave frame events. Any such event triggers
    // debug trap. The enter/leave frame events enabled or disabled across
    // all functions.

    void adjustEnterAndLeaveFrameTrapsState(JSContext* cx, bool enabled);

    // When the Code is debugEnabled, individual breakpoints can be enabled or
    // disabled at instruction offsets.

    bool hasBreakpointTrapAtOffset(uint32_t offset);
    void toggleBreakpointTrap(JSRuntime* rt, uint32_t offset, bool enabled);
    WasmBreakpointSite* getOrCreateBreakpointSite(JSContext* cx, uint32_t offset);
    bool hasBreakpointSite(uint32_t offset);
    void destroyBreakpointSite(FreeOp* fop, uint32_t offset);
    bool clearBreakpointsIn(JSContext* cx, WasmInstanceObject* instance,
                            js::Debugger* dbg, JSObject* handler);

    // When the Code is debug-enabled, single-stepping mode can be toggled on
    // the granularity of individual functions.

    bool stepModeEnabled(uint32_t funcIndex) const;
    bool incrementStepModeCount(JSContext* cx, uint32_t funcIndex);
    bool decrementStepModeCount(JSContext* cx, uint32_t funcIndex);

    // Stack inspection helpers.

    bool debugGetLocalTypes(uint32_t funcIndex, ValTypeVector* locals, size_t* argsLength);
    ExprType debugGetResultType(uint32_t funcIndex);

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
