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
#include "threading/ExclusiveData.h"
#include "wasm/WasmTypes.h"

namespace js {

struct AsmJSMetadata;
class WasmInstanceObject;

namespace wasm {

struct LinkDataTier;
struct MetadataTier;
class LinkData;
class Metadata;

// ShareableBytes is a reference-counted Vector of bytes.

struct ShareableBytes : ShareableBase<ShareableBytes>
{
    // Vector is 'final', so instead make Vector a member and add boilerplate.
    Bytes bytes;
    ShareableBytes() = default;
    explicit ShareableBytes(Bytes&& bytes) : bytes(Move(bytes)) {}
    size_t sizeOfExcludingThis(MallocSizeOf m) const { return bytes.sizeOfExcludingThis(m); }
    const uint8_t* begin() const { return bytes.begin(); }
    const uint8_t* end() const { return bytes.end(); }
    size_t length() const { return bytes.length(); }
    bool append(const uint8_t *p, uint32_t ct) { return bytes.append(p, ct); }
};

typedef RefPtr<ShareableBytes> MutableBytes;
typedef RefPtr<const ShareableBytes> SharedBytes;

// A wasm CodeSegment owns the allocated executable code for a wasm module.

class CodeSegment;
typedef UniquePtr<CodeSegment> UniqueCodeSegment;
typedef UniquePtr<const CodeSegment> UniqueConstCodeSegment;

class CodeSegment
{
    // Executable code must be deallocated specially.
    struct FreeCode {
        uint32_t codeLength;
        FreeCode() : codeLength(0) {}
        explicit FreeCode(uint32_t codeLength) : codeLength(codeLength) {}
        void operator()(uint8_t* codeBytes);
    };
    typedef UniquePtr<uint8_t, FreeCode> UniqueCodeBytes;
    static UniqueCodeBytes AllocateCodeBytes(uint32_t codeLength);

    const Code*     code_;
    Tier            tier_;
    UniqueCodeBytes bytes_;
    uint32_t        length_;

    // These are pointers into code for stubs used for asynchronous
    // signal-handler control-flow transfer.
    uint8_t*        interruptCode_;
    uint8_t*        outOfBoundsCode_;
    uint8_t*        unalignedAccessCode_;

    bool            registered_;

    bool initialize(Tier tier,
                    UniqueCodeBytes bytes,
                    uint32_t codeLength,
                    const ShareableBytes& bytecode,
                    const LinkDataTier& linkData,
                    const Metadata& metadata);

    static UniqueCodeSegment create(Tier tier,
                                    UniqueCodeBytes bytes,
                                    uint32_t codeLength,
                                    const ShareableBytes& bytecode,
                                    const LinkDataTier& linkData,
                                    const Metadata& metadata);
  public:
    CodeSegment(const CodeSegment&) = delete;
    void operator=(const CodeSegment&) = delete;

    CodeSegment()
      : code_(nullptr),
        tier_(Tier(-1)),
        length_(0),
        interruptCode_(nullptr),
        outOfBoundsCode_(nullptr),
        unalignedAccessCode_(nullptr),
        registered_(false)
    {}

    ~CodeSegment();

    static UniqueCodeSegment create(Tier tier,
                                    jit::MacroAssembler& masm,
                                    const ShareableBytes& bytecode,
                                    const LinkDataTier& linkData,
                                    const Metadata& metadata);

    static UniqueCodeSegment create(Tier tier,
                                    const Bytes& unlinkedBytes,
                                    const ShareableBytes& bytecode,
                                    const LinkDataTier& linkData,
                                    const Metadata& metadata);

    void initCode(const Code* code) {
        MOZ_ASSERT(!code_);
        code_ = code;
    }

    const Code& code() const { MOZ_ASSERT(code_); return *code_; }
    Tier tier() const { return tier_; }

    uint8_t* base() const { return bytes_.get(); }
    uint32_t length() const { return length_; }

    uint8_t* interruptCode() const { return interruptCode_; }
    uint8_t* outOfBoundsCode() const { return outOfBoundsCode_; }
    uint8_t* unalignedAccessCode() const { return unalignedAccessCode_; }

    bool containsCodePC(const void* pc) const {
        return pc >= base() && pc < (base() + length_);
    }

    // Structured clone support:

    size_t serializedSize() const;
    uint8_t* serialize(uint8_t* cursor, const LinkDataTier& linkData) const;
    const uint8_t* deserialize(const uint8_t* cursor, const ShareableBytes& bytecode,
                               const LinkDataTier& linkData, const Metadata& metadata);

    void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t* code, size_t* data) const;
};

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
        uint32_t interpEntryOffset_; // Machine code offset
    } pod;

  public:
    FuncExport() = default;
    explicit FuncExport(Sig&& sig, uint32_t funcIndex)
      : sig_(Move(sig))
    {
        pod.funcIndex_ = funcIndex;
        pod.codeRangeIndex_ = UINT32_MAX;
        pod.interpEntryOffset_ = UINT32_MAX;
    }
    void initInterpEntryOffset(uint32_t entryOffset) {
        MOZ_ASSERT(pod.interpEntryOffset_ == UINT32_MAX);
        pod.interpEntryOffset_ = entryOffset;
    }
    void initCodeRangeIndex(uint32_t codeRangeIndex) {
        MOZ_ASSERT(pod.codeRangeIndex_ == UINT32_MAX);
        pod.codeRangeIndex_ = codeRangeIndex;
    }

    const Sig& sig() const {
        return sig_;
    }
    uint32_t funcIndex() const {
        return pod.funcIndex_;
    }
    uint32_t codeRangeIndex() const {
        MOZ_ASSERT(pod.codeRangeIndex_ != UINT32_MAX);
        return pod.codeRangeIndex_;
    }
    uint32_t interpEntryOffset() const {
        MOZ_ASSERT(pod.interpEntryOffset_ != UINT32_MAX);
        return pod.interpEntryOffset_;
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
        uint32_t interpExitCodeOffset_; // Machine code offset
        uint32_t jitExitCodeOffset_;    // Machine code offset
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

// A wasm module can either use no memory, a unshared memory (ArrayBuffer) or
// shared memory (SharedArrayBuffer).

enum class MemoryUsage
{
    None = false,
    Unshared = 1,
    Shared = 2
};

// NameInBytecode represents a name that is embedded in the wasm bytecode.
// The presence of NameInBytecode implies that bytecode has been kept.

struct NameInBytecode
{
    uint32_t offset;
    uint32_t length;

    NameInBytecode()
      : offset(UINT32_MAX), length(0)
    {}
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
//
// The Metadata structure is split into tier-invariant and tier-variant parts;
// the former points to instances of the latter.  Additionally, the asm.js
// subsystem subclasses the Metadata, adding more tier-invariant data, some of
// which is serialized.  See AsmJS.cpp.

struct MetadataCacheablePod
{
    ModuleKind            kind;
    MemoryUsage           memoryUsage;
    uint32_t              minMemoryLength;
    uint32_t              globalDataLength;
    Maybe<uint32_t>       maxMemoryLength;
    Maybe<uint32_t>       startFuncIndex;

    explicit MetadataCacheablePod(ModuleKind kind)
      : kind(kind),
        memoryUsage(MemoryUsage::None),
        minMemoryLength(0),
        globalDataLength(0)
    {}
};

typedef uint8_t ModuleHash[8];

struct MetadataTier
{
    explicit MetadataTier(Tier tier) : tier(tier) {}

    const Tier            tier;

    MemoryAccessVector    memoryAccesses;
    CodeRangeVector       codeRanges;
    CallSiteVector        callSites;
    FuncImportVector      funcImports;
    FuncExportVector      funcExports;

    // Debug information, not serialized.
    Uint32Vector          debugTrapFarJumpOffsets;
    Uint32Vector          debugFuncToCodeRange;

    FuncExport& lookupFuncExport(uint32_t funcIndex);
    const FuncExport& lookupFuncExport(uint32_t funcIndex) const;

    WASM_DECLARE_SERIALIZABLE(MetadataTier);
};

typedef UniquePtr<MetadataTier> UniqueMetadataTier;

class Metadata : public ShareableBase<Metadata>, public MetadataCacheablePod
{
  protected:
    UniqueMetadataTier         metadata1_;
    mutable UniqueMetadataTier metadata2_;  // Access only when hasTier2() is true
    mutable Atomic<bool>       hasTier2_;

  public:
    explicit Metadata(UniqueMetadataTier tier, ModuleKind kind = ModuleKind::Wasm)
      : MetadataCacheablePod(kind),
        metadata1_(Move(tier)),
        debugEnabled(false),
        debugHash()
    {}
    virtual ~Metadata() {}

    MetadataCacheablePod& pod() { return *this; }
    const MetadataCacheablePod& pod() const { return *this; }

    void commitTier2() const;
    bool hasTier2() const { return hasTier2_; }
    void setTier2(UniqueMetadataTier metadata) const;
    Tiers tiers() const;

    const MetadataTier& metadata(Tier t) const;
    MetadataTier& metadata(Tier t);

    UniquePtr<MetadataTier> takeMetadata(Tier tier) {
        MOZ_ASSERT(!hasTier2());
        MOZ_ASSERT(metadata1_->tier == tier);
        return Move(metadata1_);
    }

    SigWithIdVector       sigIds;
    GlobalDescVector      globals;
    TableDescVector       tables;
    NameInBytecodeVector  funcNames;
    CustomSectionVector   customSections;
    CacheableChars        filename;

    // Debug-enabled code is not serialized.
    bool                  debugEnabled;
    FuncArgTypesVector    debugFuncArgTypes;
    FuncReturnTypesVector debugFuncReturnTypes;
    ModuleHash            debugHash;

    bool usesMemory() const { return memoryUsage != MemoryUsage::None; }
    bool usesSharedMemory() const { return memoryUsage == MemoryUsage::Shared; }

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

typedef mozilla::UniquePtr<void*[], JS::FreePolicy> UniqueJumpTable;

// Code objects own executable code and the metadata that describe it. A single
// Code object is normally shared between a module and all its instances.
//
// profilingLabels_ is lazily initialized, but behind a lock.

class Code : public ShareableBase<Code>
{
    UniqueConstCodeSegment              segment1_;
    mutable UniqueConstCodeSegment      segment2_; // Access only when hasTier2() is true
    SharedMetadata                      metadata_;
    ExclusiveData<CacheableCharsVector> profilingLabels_;
    UniqueJumpTable                     jumpTable_;

    UniqueConstCodeSegment takeOwnership(UniqueCodeSegment segment) const {
        segment->initCode(this);
        return UniqueConstCodeSegment(segment.release());
    }

  public:
    Code();
    Code(UniqueCodeSegment tier, const Metadata& metadata, UniqueJumpTable maybeJumpTable);

    void** jumpTable() const { return jumpTable_.get(); }

    bool hasTier2() const { return metadata_->hasTier2(); }
    void setTier2(UniqueCodeSegment segment) const;
    Tiers tiers() const;
    bool hasTier(Tier t) const;

    Tier stableTier() const;    // This is stable during a run
    Tier bestTier() const;      // This may transition from Baseline -> Ion at any time

    const CodeSegment& segment(Tier tier) const;
    const MetadataTier& metadata(Tier tier) const { return metadata_->metadata(tier); }
    const Metadata& metadata() const { return *metadata_; }

    // Metadata lookup functions:

    const CallSite* lookupCallSite(void* returnAddress) const;
    const CodeRange* lookupRange(void* pc) const;
    const MemoryAccess* lookupMemoryAccess(void* pc) const;
    bool containsCodePC(const void* pc) const;

    // To save memory, profilingLabels_ are generated lazily when profiling mode
    // is enabled.

    void ensureProfilingLabels(const Bytes* maybeBytecode, bool profilingEnabled) const;
    const char* profilingLabel(uint32_t funcIndex) const;

    // about:memory reporting:

    void addSizeOfMiscIfNotSeen(MallocSizeOf mallocSizeOf,
                                Metadata::SeenSet* seenMetadata,
                                Code::SeenSet* seenCode,
                                size_t* code,
                                size_t* data) const;

    // A Code object is serialized as the length and bytes of the machine code
    // after statically unlinking it; the Code is then later recreated from the
    // machine code and other parts.

    size_t serializedSize() const;
    uint8_t* serialize(uint8_t* cursor, const LinkData& linkData) const;
    const uint8_t* deserialize(const uint8_t* cursor, const SharedBytes& bytecode,
                               const LinkData& linkData, Metadata& metadata);
};

typedef RefPtr<const Code> SharedCode;
typedef RefPtr<Code> MutableCode;

} // namespace wasm
} // namespace js

#endif // wasm_code_h
