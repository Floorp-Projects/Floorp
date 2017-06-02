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

#include "wasm/WasmCode.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/EnumeratedRange.h"

#include "jit/ExecutableAllocator.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "vtune/VTuneWrapper.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmSerialize.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::BinarySearch;
using mozilla::MakeEnumeratedRange;
using JS::GenericNaN;

static uint32_t
RoundupCodeLength(uint32_t codeLength)
{
    // codeLength is a multiple of the system's page size, but not necessarily
    // a multiple of ExecutableCodePageSize.
    MOZ_ASSERT(codeLength % gc::SystemPageSize() == 0);
    return JS_ROUNDUP(codeLength, ExecutableCodePageSize);
}

/* static */ CodeSegment::UniqueCodeBytes
CodeSegment::AllocateCodeBytes(uint32_t codeLength)
{
    codeLength = RoundupCodeLength(codeLength);

    void* p = AllocateExecutableMemory(codeLength, ProtectionSetting::Writable);

    // If the allocation failed and the embedding gives us a last-ditch attempt
    // to purge all memory (which, in gecko, does a purging GC/CC/GC), do that
    // then retry the allocation.
    if (!p) {
        if (OnLargeAllocationFailure) {
            OnLargeAllocationFailure();
            p = AllocateExecutableMemory(codeLength, ProtectionSetting::Writable);
        }
    }

    if (!p)
        return nullptr;

    // We account for the bytes allocated in WasmModuleObject::create, where we
    // have the necessary JSContext.

    return UniqueCodeBytes((uint8_t*)p, FreeCode(codeLength));
}

void
CodeSegment::FreeCode::operator()(uint8_t* bytes)
{
    MOZ_ASSERT(codeLength);
    MOZ_ASSERT(codeLength == RoundupCodeLength(codeLength));

#ifdef MOZ_VTUNE
    vtune::UnmarkBytes(bytes, codeLength);
#endif
    DeallocateExecutableMemory(bytes, codeLength);
}

static bool
StaticallyLink(const CodeSegment& cs, const LinkDataTier& linkData)
{
    for (LinkDataTier::InternalLink link : linkData.internalLinks) {
        uint8_t* patchAt = cs.base() + link.patchAtOffset;
        void* target = cs.base() + link.targetOffset;
        if (link.isRawPointerPatch())
            *(void**)(patchAt) = target;
        else
            Assembler::PatchInstructionImmediate(patchAt, PatchedImmPtr(target));
    }

    if (!EnsureBuiltinThunksInitialized())
        return false;

    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        const Uint32Vector& offsets = linkData.symbolicLinks[imm];
        if (offsets.empty())
            continue;

        void* target = SymbolicAddressTarget(imm);
        for (uint32_t offset : offsets) {
            uint8_t* patchAt = cs.base() + offset;
            Assembler::PatchDataWithValueCheck(CodeLocationLabel(patchAt),
                                               PatchedImmPtr(target),
                                               PatchedImmPtr((void*)-1));
        }
    }

    return true;
}

static void
StaticallyUnlink(uint8_t* base, const LinkDataTier& linkData)
{
    for (LinkDataTier::InternalLink link : linkData.internalLinks) {
        uint8_t* patchAt = base + link.patchAtOffset;
        void* target = 0;
        if (link.isRawPointerPatch())
            *(void**)(patchAt) = target;
        else
            Assembler::PatchInstructionImmediate(patchAt, PatchedImmPtr(target));
    }

    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        const Uint32Vector& offsets = linkData.symbolicLinks[imm];
        if (offsets.empty())
            continue;

        void* target = SymbolicAddressTarget(imm);
        for (uint32_t offset : offsets) {
            uint8_t* patchAt = base + offset;
            Assembler::PatchDataWithValueCheck(CodeLocationLabel(patchAt),
                                               PatchedImmPtr((void*)-1),
                                               PatchedImmPtr(target));
        }
    }
}

static void
SendCodeRangesToProfiler(const CodeSegment& cs, const Bytes& bytecode, const Metadata& metadata)
{
    bool enabled = false;
#ifdef JS_ION_PERF
    enabled |= PerfFuncEnabled();
#endif
#ifdef MOZ_VTUNE
    enabled |= vtune::IsProfilingActive();
#endif
    if (!enabled)
        return;

    for (const CodeRange& codeRange : metadata.metadata(cs.tier()).codeRanges) {
        if (!codeRange.isFunction())
            continue;

        uintptr_t start = uintptr_t(cs.base() + codeRange.begin());
        uintptr_t end = uintptr_t(cs.base() + codeRange.end());
        uintptr_t size = end - start;

        UTF8Bytes name;
        if (!metadata.getFuncName(&bytecode, codeRange.funcIndex(), &name))
            return;
        if (!name.append('\0'))
            return;

        // Avoid "unused" warnings
        (void)start;
        (void)size;

#ifdef JS_ION_PERF
        if (PerfFuncEnabled()) {
            const char* file = metadata.filename.get();
            unsigned line = codeRange.funcLineOrBytecode();
            unsigned column = 0;
            writePerfSpewerWasmFunctionMap(start, size, file, line, column, name.begin());
        }
#endif
#ifdef MOZ_VTUNE
        if (vtune::IsProfilingActive())
            vtune::MarkWasm(vtune::GenerateUniqueMethodID(), name.begin(), (void*)start, size);
#endif
    }
}

/* static */ UniqueConstCodeSegment
CodeSegment::create(Tier tier,
                    MacroAssembler& masm,
                    const ShareableBytes& bytecode,
                    const LinkDataTier& linkData,
                    const Metadata& metadata)
{
    // Round up the code size to page size since this is eventually required by
    // the executable-code allocator and for setting memory protection.
    uint32_t bytesNeeded = masm.bytesNeeded();
    uint32_t padding = ComputeByteAlignment(bytesNeeded, gc::SystemPageSize());
    uint32_t codeLength = bytesNeeded + padding;

    MOZ_ASSERT(linkData.functionCodeLength < codeLength);

    UniqueCodeBytes codeBytes = AllocateCodeBytes(codeLength);
    if (!codeBytes)
        return nullptr;

    // We'll flush the icache after static linking, in initialize().
    masm.executableCopy(codeBytes.get(), /* flushICache = */ false);

    // Zero the padding.
    memset(codeBytes.get() + bytesNeeded, 0, padding);

    return create(tier, Move(codeBytes), codeLength, bytecode, linkData, metadata);
}

/* static */ UniqueConstCodeSegment
CodeSegment::create(Tier tier,
                    const Bytes& unlinkedBytes,
                    const ShareableBytes& bytecode,
                    const LinkDataTier& linkData,
                    const Metadata& metadata)
{
    // The unlinked bytes are a snapshot of the MacroAssembler's contents so
    // round up just like in the MacroAssembler overload above.
    uint32_t padding = ComputeByteAlignment(unlinkedBytes.length(), gc::SystemPageSize());
    uint32_t codeLength = unlinkedBytes.length() + padding;

    UniqueCodeBytes codeBytes = AllocateCodeBytes(codeLength);
    if (!codeBytes)
        return nullptr;

    memcpy(codeBytes.get(), unlinkedBytes.begin(), unlinkedBytes.length());
    memset(codeBytes.get() + unlinkedBytes.length(), 0, padding);

    return create(tier, Move(codeBytes), codeLength, bytecode, linkData, metadata);
}

/* static */ UniqueConstCodeSegment
CodeSegment::create(Tier tier,
                    UniqueCodeBytes codeBytes,
                    uint32_t codeLength,
                    const ShareableBytes& bytecode,
                    const LinkDataTier& linkData,
                    const Metadata& metadata)
{
    // These should always exist and should never be first in the code segment.
    MOZ_ASSERT(linkData.interruptOffset != 0);
    MOZ_ASSERT(linkData.outOfBoundsOffset != 0);
    MOZ_ASSERT(linkData.unalignedAccessOffset != 0);

    auto cs = js::MakeUnique<CodeSegment>();
    if (!cs)
        return nullptr;

    if (!cs->initialize(tier, Move(codeBytes), codeLength, bytecode, linkData, metadata))
        return nullptr;

    return UniqueConstCodeSegment(cs.release());
}

bool
CodeSegment::initialize(Tier tier,
                        UniqueCodeBytes codeBytes,
                        uint32_t codeLength,
                        const ShareableBytes& bytecode,
                        const LinkDataTier& linkData,
                        const Metadata& metadata)
{
    MOZ_ASSERT(bytes_ == nullptr);

    tier_ = tier;
    bytes_ = Move(codeBytes);
    functionLength_ = linkData.functionCodeLength;
    length_ = codeLength;
    interruptCode_ = bytes_.get() + linkData.interruptOffset;
    outOfBoundsCode_ = bytes_.get() + linkData.outOfBoundsOffset;
    unalignedAccessCode_ = bytes_.get() + linkData.unalignedAccessOffset;

    if (!StaticallyLink(*this, linkData))
        return false;

    ExecutableAllocator::cacheFlush(bytes_.get(), RoundupCodeLength(codeLength));

    // Reprotect the whole region to avoid having separate RW and RX mappings.
    if (!ExecutableAllocator::makeExecutable(bytes_.get(), RoundupCodeLength(codeLength)))
        return false;

    SendCodeRangesToProfiler(*this, bytecode.bytes, metadata);

    return true;
}

size_t
CodeSegment::serializedSize() const
{
    return sizeof(uint32_t) + length_;
}

void
CodeSegment::addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t* code, size_t* data) const
{
    *data += mallocSizeOf(this);
    *code += RoundupCodeLength(length_);
}

uint8_t*
CodeSegment::serialize(uint8_t* cursor, const LinkDataTier& linkData) const
{
    MOZ_ASSERT(tier() == Tier::Serialized);

    cursor = WriteScalar<uint32_t>(cursor, length_);
    uint8_t* base = cursor;
    cursor = WriteBytes(cursor, bytes_.get(), length_);
    StaticallyUnlink(base, linkData);
    return cursor;
}

const uint8_t*
CodeSegment::deserialize(const uint8_t* cursor, const ShareableBytes& bytecode,
                         const LinkDataTier& linkData, const Metadata& metadata)
{
    uint32_t length;
    cursor = ReadScalar<uint32_t>(cursor, &length);
    if (!cursor)
        return nullptr;

    MOZ_ASSERT(length_ % gc::SystemPageSize() == 0);
    UniqueCodeBytes bytes = AllocateCodeBytes(length);
    if (!bytes)
        return nullptr;

    cursor = ReadBytes(cursor, bytes.get(), length);
    if (!cursor)
        return nullptr;

    if (!initialize(Tier::Serialized, Move(bytes), length, bytecode, linkData, metadata))
        return nullptr;

    return cursor;
}

size_t
FuncExport::serializedSize() const
{
    return sig_.serializedSize() +
           sizeof(pod);
}

uint8_t*
FuncExport::serialize(uint8_t* cursor) const
{
    cursor = sig_.serialize(cursor);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    return cursor;
}

const uint8_t*
FuncExport::deserialize(const uint8_t* cursor)
{
    (cursor = sig_.deserialize(cursor)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
    return cursor;
}

size_t
FuncExport::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return sig_.sizeOfExcludingThis(mallocSizeOf);
}

size_t
FuncImport::serializedSize() const
{
    return sig_.serializedSize() +
           sizeof(pod);
}

uint8_t*
FuncImport::serialize(uint8_t* cursor) const
{
    cursor = sig_.serialize(cursor);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    return cursor;
}

const uint8_t*
FuncImport::deserialize(const uint8_t* cursor)
{
    (cursor = sig_.deserialize(cursor)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
    return cursor;
}

size_t
FuncImport::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return sig_.sizeOfExcludingThis(mallocSizeOf);
}

static size_t
StringLengthWithNullChar(const char* chars)
{
    return chars ? strlen(chars) + 1 : 0;
}

size_t
CacheableChars::serializedSize() const
{
    return sizeof(uint32_t) + StringLengthWithNullChar(get());
}

uint8_t*
CacheableChars::serialize(uint8_t* cursor) const
{
    uint32_t lengthWithNullChar = StringLengthWithNullChar(get());
    cursor = WriteScalar<uint32_t>(cursor, lengthWithNullChar);
    cursor = WriteBytes(cursor, get(), lengthWithNullChar);
    return cursor;
}

const uint8_t*
CacheableChars::deserialize(const uint8_t* cursor)
{
    uint32_t lengthWithNullChar;
    cursor = ReadBytes(cursor, &lengthWithNullChar, sizeof(uint32_t));

    if (lengthWithNullChar) {
        reset(js_pod_malloc<char>(lengthWithNullChar));
        if (!get())
            return nullptr;

        cursor = ReadBytes(cursor, get(), lengthWithNullChar);
    } else {
        MOZ_ASSERT(!get());
    }

    return cursor;
}

size_t
CacheableChars::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(get());
}

size_t
MetadataTier::serializedSize() const
{
    return SerializedPodVectorSize(memoryAccesses) +
           SerializedPodVectorSize(codeRanges) +
           SerializedPodVectorSize(callSites) +
           SerializedVectorSize(funcImports) +
           SerializedVectorSize(funcExports);
}

size_t
MetadataTier::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return memoryAccesses.sizeOfExcludingThis(mallocSizeOf) +
           codeRanges.sizeOfExcludingThis(mallocSizeOf) +
           callSites.sizeOfExcludingThis(mallocSizeOf) +
           SizeOfVectorExcludingThis(funcImports, mallocSizeOf) +
           SizeOfVectorExcludingThis(funcExports, mallocSizeOf);
}

uint8_t*
MetadataTier::serialize(uint8_t* cursor) const
{
    MOZ_ASSERT(debugTrapFarJumpOffsets.empty() && debugFuncToCodeRange.empty());
    cursor = SerializePodVector(cursor, memoryAccesses);
    cursor = SerializePodVector(cursor, codeRanges);
    cursor = SerializePodVector(cursor, callSites);
    cursor = SerializeVector(cursor, funcImports);
    cursor = SerializeVector(cursor, funcExports);
    return cursor;
}

/* static */ const uint8_t*
MetadataTier::deserialize(const uint8_t* cursor)
{
    (cursor = DeserializePodVector(cursor, &memoryAccesses)) &&
    (cursor = DeserializePodVector(cursor, &codeRanges)) &&
    (cursor = DeserializePodVector(cursor, &callSites)) &&
    (cursor = DeserializeVector(cursor, &funcImports)) &&
    (cursor = DeserializeVector(cursor, &funcExports));
    debugTrapFarJumpOffsets.clear();
    debugFuncToCodeRange.clear();
    return cursor;
}

void
Metadata::commitTier2() const
{
    MOZ_RELEASE_ASSERT(metadata2_.get());
    MOZ_RELEASE_ASSERT(!hasTier2_);
    hasTier2_ = true;
}

void
Metadata::setTier2(UniqueMetadataTier metadata) const
{
    MOZ_RELEASE_ASSERT(metadata->tier == Tier::Ion && metadata1_->tier != Tier::Ion);
    MOZ_RELEASE_ASSERT(!metadata2_.get());
    metadata2_ = Move(metadata);
}

Tiers
Metadata::tiers() const
{
    if (hasTier2())
        return Tiers(metadata1_->tier, metadata2_->tier);
    return Tiers(metadata1_->tier);
}

const MetadataTier&
Metadata::metadata(Tier t) const
{
    switch (t) {
      case Tier::Baseline:
        if (metadata1_->tier == Tier::Baseline)
            return *metadata1_;
        MOZ_CRASH("No metadata at this tier");
      case Tier::Ion:
        if (metadata1_->tier == Tier::Ion)
            return *metadata1_;
        if (hasTier2())
            return *metadata2_;
        MOZ_CRASH("No metadata at this tier");
      default:
        MOZ_CRASH();
    }
}

MetadataTier&
Metadata::metadata(Tier t)
{
    switch (t) {
      case Tier::Baseline:
        if (metadata1_->tier == Tier::Baseline)
            return *metadata1_;
        MOZ_CRASH("No metadata at this tier");
      case Tier::Ion:
        if (metadata1_->tier == Tier::Ion)
            return *metadata1_;
        if (hasTier2())
            return *metadata2_;
        MOZ_CRASH("No metadata at this tier");
      default:
        MOZ_CRASH();
    }
}

size_t
Metadata::serializedSize() const
{
    return sizeof(pod()) +
           metadata(Tier::Serialized).serializedSize() +
           SerializedVectorSize(sigIds) +
           SerializedPodVectorSize(globals) +
           SerializedPodVectorSize(tables) +
           SerializedPodVectorSize(funcNames) +
           SerializedPodVectorSize(customSections) +
           filename.serializedSize() +
           sizeof(hash);
}

size_t
Metadata::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    size_t sum = 0;

    for (auto t : tiers())
        sum += metadata(t).sizeOfExcludingThis(mallocSizeOf);

    return sum +
           SizeOfVectorExcludingThis(sigIds, mallocSizeOf) +
           globals.sizeOfExcludingThis(mallocSizeOf) +
           tables.sizeOfExcludingThis(mallocSizeOf) +
           funcNames.sizeOfExcludingThis(mallocSizeOf) +
           customSections.sizeOfExcludingThis(mallocSizeOf) +
           filename.sizeOfExcludingThis(mallocSizeOf);
}

uint8_t*
Metadata::serialize(uint8_t* cursor) const
{
    MOZ_ASSERT(!debugEnabled && debugFuncArgTypes.empty() && debugFuncReturnTypes.empty());
    cursor = WriteBytes(cursor, &pod(), sizeof(pod()));
    cursor = metadata(Tier::Serialized).serialize(cursor);
    cursor = SerializeVector(cursor, sigIds);
    cursor = SerializePodVector(cursor, globals);
    cursor = SerializePodVector(cursor, tables);
    cursor = SerializePodVector(cursor, funcNames);
    cursor = SerializePodVector(cursor, customSections);
    cursor = filename.serialize(cursor);
    cursor = WriteBytes(cursor, hash, sizeof(hash));
    return cursor;
}

/* static */ const uint8_t*
Metadata::deserialize(const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &pod(), sizeof(pod()))) &&
    (cursor = metadata(Tier::Serialized).deserialize(cursor)) &&
    (cursor = DeserializeVector(cursor, &sigIds)) &&
    (cursor = DeserializePodVector(cursor, &globals)) &&
    (cursor = DeserializePodVector(cursor, &tables)) &&
    (cursor = DeserializePodVector(cursor, &funcNames)) &&
    (cursor = DeserializePodVector(cursor, &customSections)) &&
    (cursor = filename.deserialize(cursor)) &&
    (cursor = ReadBytes(cursor, hash, sizeof(hash)));
    debugEnabled = false;
    debugFuncArgTypes.clear();
    debugFuncReturnTypes.clear();
    return cursor;
}

struct ProjectFuncIndex
{
    const FuncExportVector& funcExports;

    explicit ProjectFuncIndex(const FuncExportVector& funcExports)
      : funcExports(funcExports)
    {}
    uint32_t operator[](size_t index) const {
        return funcExports[index].funcIndex();
    }
};

const FuncExport&
MetadataTier::lookupFuncExport(uint32_t funcIndex) const
{
    size_t match;
    if (!BinarySearch(ProjectFuncIndex(funcExports), 0, funcExports.length(), funcIndex, &match))
        MOZ_CRASH("missing function export");

    return funcExports[match];
}

bool
Metadata::getFuncName(const Bytes* maybeBytecode, uint32_t funcIndex, UTF8Bytes* name) const
{
    if (funcIndex < funcNames.length()) {
        MOZ_ASSERT(maybeBytecode, "NameInBytecode requires preserved bytecode");

        const NameInBytecode& n = funcNames[funcIndex];
        if (n.length != 0) {
            MOZ_ASSERT(n.offset + n.length <= maybeBytecode->length());
            return name->append((const char*)maybeBytecode->begin() + n.offset, n.length);
        }
    }

    // For names that are out of range or invalid, synthesize a name.

    const char beforeFuncIndex[] = "wasm-function[";
    const char afterFuncIndex[] = "]";

    ToCStringBuf cbuf;
    const char* funcIndexStr = NumberToCString(nullptr, &cbuf, funcIndex);
    MOZ_ASSERT(funcIndexStr);

    return name->append(beforeFuncIndex, strlen(beforeFuncIndex)) &&
           name->append(funcIndexStr, strlen(funcIndexStr)) &&
           name->append(afterFuncIndex, strlen(afterFuncIndex));
}

Code::Code(UniqueConstCodeSegment tier, const Metadata& metadata)
  : segment1_(Move(tier)),
    metadata_(&metadata),
    profilingLabels_(mutexid::WasmCodeProfilingLabels, CacheableCharsVector())
{
}

Code::Code()
  : profilingLabels_(mutexid::WasmCodeProfilingLabels, CacheableCharsVector())
{
}

void
Code::setTier2(UniqueConstCodeSegment segment) const
{
    MOZ_RELEASE_ASSERT(segment->tier() == Tier::Ion && segment1_->tier() != Tier::Ion);
    MOZ_RELEASE_ASSERT(!segment2_.get());
    segment2_ = Move(segment);
}

Tiers
Code::tiers() const
{
    if (hasTier2())
        return Tiers(segment1_->tier(), segment2_->tier());
    return Tiers(segment1_->tier());
}

bool
Code::hasTier(Tier t) const
{
    if (hasTier2() && segment2_->tier() == t)
        return true;
    return segment1_->tier() == t;
}

Tier
Code::stableTier() const
{
    return segment1_->tier();
}

Tier
Code::bestTier() const
{
    if (hasTier2())
        return segment2_->tier();
    return segment1_->tier();
}

const CodeSegment&
Code::segment(Tier tier) const
{
    switch (tier) {
      case Tier::Baseline:
        if (segment1_->tier() == Tier::Baseline)
            return *segment1_;
        MOZ_CRASH("No code segment at this tier");
      case Tier::Ion:
        if (segment1_->tier() == Tier::Ion)
            return *segment1_;
        if (hasTier2())
            return *segment2_;
        MOZ_CRASH("No code segment at this tier");
      default:
        MOZ_CRASH();
    }
}

bool
Code::containsFunctionPC(const void* pc, const CodeSegment** segmentp) const
{
    for (auto t : tiers()) {
        const CodeSegment& cs = segment(t);
        if (cs.containsFunctionPC(pc)) {
            if (segmentp)
                *segmentp = &cs;
            return true;
        }
    }
    return false;
}

bool
Code::containsCodePC(const void* pc, const CodeSegment** segmentp) const
{
    for (auto t : tiers()) {
        const CodeSegment& cs = segment(t);
        if (cs.containsCodePC(pc)) {
            if (segmentp)
                *segmentp = &cs;
            return true;
        }
    }
    return false;
}

struct CallSiteRetAddrOffset
{
    const CallSiteVector& callSites;
    explicit CallSiteRetAddrOffset(const CallSiteVector& callSites) : callSites(callSites) {}
    uint32_t operator[](size_t index) const {
        return callSites[index].returnAddressOffset();
    }
};

size_t
Code::serializedSize() const
{
    return metadata().serializedSize() +
           segment(Tier::Serialized).serializedSize();
}

uint8_t*
Code::serialize(uint8_t* cursor, const LinkData& linkData) const
{
    MOZ_RELEASE_ASSERT(!metadata().debugEnabled);

    cursor = metadata().serialize(cursor);
    cursor = segment(Tier::Serialized).serialize(cursor, linkData.linkData(Tier::Serialized));
    return cursor;
}

const uint8_t*
Code::deserialize(const uint8_t* cursor, const SharedBytes& bytecode, const LinkData& linkData,
                  Metadata& metadata)
{
    cursor = metadata.deserialize(cursor);
    if (!cursor)
        return nullptr;

    UniqueCodeSegment codeSegment = js::MakeUnique<CodeSegment>();
    if (!codeSegment)
        return nullptr;

    cursor = codeSegment->deserialize(cursor, *bytecode, linkData.linkData(Tier::Serialized), metadata);
    if (!cursor)
        return nullptr;

    segment1_ = UniqueConstCodeSegment(codeSegment.release());
    metadata_ = &metadata;

    return cursor;
}

const CallSite*
Code::lookupCallSite(void* returnAddress, const CodeSegment** segmentp) const
{
    for (auto t : tiers()) {
        uint32_t target = ((uint8_t*)returnAddress) - segment(t).base();
        size_t lowerBound = 0;
        size_t upperBound = metadata(t).callSites.length();

        size_t match;
        if (BinarySearch(CallSiteRetAddrOffset(metadata(t).callSites), lowerBound, upperBound,
                         target, &match))
        {
            if (segmentp)
                *segmentp = &segment(t);
            return &metadata(t).callSites[match];
        }
    }

    return nullptr;
}

const CodeRange*
Code::lookupRange(void* pc, const CodeSegment** segmentp) const
{
    for (auto t : tiers()) {
        CodeRange::OffsetInCode target((uint8_t*)pc - segment(t).base());
        const CodeRange* result = LookupInSorted(metadata(t).codeRanges, target);
        if (result) {
            if (segmentp)
                *segmentp = &segment(t);
            return result;
        }
    }

    return nullptr;
}

struct MemoryAccessOffset
{
    const MemoryAccessVector& accesses;
    explicit MemoryAccessOffset(const MemoryAccessVector& accesses) : accesses(accesses) {}
    uintptr_t operator[](size_t index) const {
        return accesses[index].insnOffset();
    }
};

const MemoryAccess*
Code::lookupMemoryAccess(void* pc, const CodeSegment** segmentp) const
{
    for (auto t : tiers()) {
        const MemoryAccessVector& memoryAccesses = metadata(t).memoryAccesses;

        uint32_t target = ((uint8_t*)pc) - segment(t).base();
        size_t lowerBound = 0;
        size_t upperBound = memoryAccesses.length();

        size_t match;
        if (BinarySearch(MemoryAccessOffset(memoryAccesses), lowerBound, upperBound, target,
                         &match))
        {
            MOZ_ASSERT(segment(t).containsFunctionPC(pc));
            if (segmentp)
                *segmentp = &segment(t);
            return &memoryAccesses[match];
        }
    }
    return nullptr;
}

// When enabled, generate profiling labels for every name in funcNames_ that is
// the name of some Function CodeRange. This involves malloc() so do it now
// since, once we start sampling, we'll be in a signal-handing context where we
// cannot malloc.
void
Code::ensureProfilingLabels(const Bytes* maybeBytecode, bool profilingEnabled) const
{
    auto labels = profilingLabels_.lock();

    if (!profilingEnabled) {
        labels->clear();
        return;
    }

    if (!labels->empty())
        return;

    // Any tier will do, we only need tier-invariant data that are incidentally
    // stored with the code ranges.

    for (const CodeRange& codeRange : metadata(stableTier()).codeRanges) {
        if (!codeRange.isFunction())
            continue;

        ToCStringBuf cbuf;
        const char* bytecodeStr = NumberToCString(nullptr, &cbuf, codeRange.funcLineOrBytecode());
        MOZ_ASSERT(bytecodeStr);

        UTF8Bytes name;
        if (!metadata().getFuncName(maybeBytecode, codeRange.funcIndex(), &name))
            return;
        if (!name.append(" (", 2))
            return;

        if (const char* filename = metadata().filename.get()) {
            if (!name.append(filename, strlen(filename)))
                return;
        } else {
            if (!name.append('?'))
                return;
        }

        if (!name.append(':') ||
            !name.append(bytecodeStr, strlen(bytecodeStr)) ||
            !name.append(")\0", 2))
        {
            return;
        }

        UniqueChars label(name.extractOrCopyRawBuffer());
        if (!label)
            return;

        if (codeRange.funcIndex() >= labels->length()) {
            if (!labels->resize(codeRange.funcIndex() + 1))
                return;
        }

        ((CacheableCharsVector&)labels)[codeRange.funcIndex()] = Move(label);
    }
}

const char*
Code::profilingLabel(uint32_t funcIndex) const
{
    auto labels = profilingLabels_.lock();

    if (funcIndex >= labels->length() || !((CacheableCharsVector&)labels)[funcIndex])
        return "?";
    return ((CacheableCharsVector&)labels)[funcIndex].get();
}

void
Code::addSizeOfMiscIfNotSeen(MallocSizeOf mallocSizeOf,
                             Metadata::SeenSet* seenMetadata,
                             Code::SeenSet* seenCode,
                             size_t* code,
                             size_t* data) const
{
    auto p = seenCode->lookupForAdd(this);
    if (p)
        return;
    bool ok = seenCode->add(p, this);
    (void)ok;  // oh well

    *data += mallocSizeOf(this) +
             metadata().sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenMetadata) +
             profilingLabels_.lock()->sizeOfExcludingThis(mallocSizeOf);

    for (auto t : tiers())
        segment(t).addSizeOfMisc(mallocSizeOf, code, data);
}
