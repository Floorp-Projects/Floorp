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

#include "mozilla/Atomics.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/EnumeratedRange.h"

#include "jsprf.h"

#include "jit/ExecutableAllocator.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "vtune/VTuneWrapper.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmSerialize.h"

#include "jsobjinlines.h"

#include "jit/MacroAssembler-inl.h"
#include "vm/ArrayBufferObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::Atomic;
using mozilla::BinarySearch;
using mozilla::MakeEnumeratedRange;
using JS::GenericNaN;

// Limit the number of concurrent wasm code allocations per process. Note that
// on Linux, the real maximum is ~32k, as each module requires 2 maps (RW/RX),
// and the kernel's default max_map_count is ~65k.
//
// Note: this can be removed once writable/non-executable global data stops
// being stored in the code segment.
static Atomic<uint32_t> wasmCodeAllocations(0);
static const uint32_t MaxWasmCodeAllocations = 16384;

static uint32_t
RoundupCodeLength(uint32_t codeLength)
{
    // codeLength is a multiple of the system's page size, but not necessarily
    // a multiple of ExecutableCodePageSize.
    MOZ_ASSERT(codeLength % gc::SystemPageSize() == 0);
    return JS_ROUNDUP(codeLength, ExecutableCodePageSize);
}

static uint8_t*
AllocateCodeBytes(uint32_t codeLength)
{
    codeLength = RoundupCodeLength(codeLength);

    if (wasmCodeAllocations >= MaxWasmCodeAllocations)
        return nullptr;

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

    wasmCodeAllocations++;
    return (uint8_t*)p;
}

static void
FreeCodeBytes(uint8_t* bytes, uint32_t codeLength)
{
    MOZ_ASSERT(wasmCodeAllocations > 0);
    wasmCodeAllocations--;

    codeLength = RoundupCodeLength(codeLength);
#ifdef MOZ_VTUNE
    vtune::UnmarkBytes(bytes, codeLength);
#endif
    DeallocateExecutableMemory(bytes, codeLength);
}

static bool
StaticallyLink(const CodeSegment& cs, const LinkData& linkData)
{
    for (LinkData::InternalLink link : linkData.internalLinks) {
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
StaticallyUnlink(uint8_t* base, const LinkData& linkData)
{
    for (LinkData::InternalLink link : linkData.internalLinks) {
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

    for (const CodeRange& codeRange : metadata.codeRanges) {
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

    return;
}

/* static */ UniqueConstCodeSegment
CodeSegment::create(jit::MacroAssembler& masm,
                    const ShareableBytes& bytecode,
                    const LinkData& linkData,
                    const Metadata& metadata)
{
    // Round up the code size to page size since this is eventually required by
    // the executable-code allocator and for setting memory protection.
    uint32_t bytesNeeded = masm.bytesNeeded();
    uint32_t padding = ComputeByteAlignment(bytesNeeded, gc::SystemPageSize());
    uint32_t codeLength = bytesNeeded + padding;

    MOZ_ASSERT(linkData.functionCodeLength < codeLength);

    uint8_t* codeBase = AllocateCodeBytes(codeLength);
    if (!codeBase)
        return nullptr;

    // We'll flush the icache after static linking, in initialize().
    masm.executableCopy(codeBase, /* flushICache = */ false);

    // Zero the padding.
    memset(codeBase + bytesNeeded, 0, padding);

    return create(codeBase, codeLength, bytecode, linkData, metadata);
}

/* static */ UniqueConstCodeSegment
CodeSegment::create(const Bytes& unlinkedBytes, const ShareableBytes& bytecode,
                    const LinkData& linkData, const Metadata& metadata)
{
    uint32_t codeLength = unlinkedBytes.length();
    MOZ_ASSERT(codeLength % gc::SystemPageSize() == 0);

    uint8_t* codeBytes = AllocateCodeBytes(codeLength);
    if (!codeBytes)
        return nullptr;
    memcpy(codeBytes, unlinkedBytes.begin(), codeLength);

    return create(codeBytes, codeLength, bytecode, linkData, metadata);
}

/* static */ UniqueConstCodeSegment
CodeSegment::create(uint8_t* codeBase, uint32_t codeLength,
                    const ShareableBytes& bytecode,
                    const LinkData& linkData,
                    const Metadata& metadata)
{
    // These should always exist and should never be first in the code segment.
    MOZ_ASSERT(linkData.interruptOffset != 0);
    MOZ_ASSERT(linkData.outOfBoundsOffset != 0);
    MOZ_ASSERT(linkData.unalignedAccessOffset != 0);

    auto cs = js::MakeUnique<CodeSegment>();
    if (!cs) {
        FreeCodeBytes(codeBase, codeLength);
        return nullptr;
    }

    if (!cs->initialize(codeBase, codeLength, bytecode, linkData, metadata))
        return nullptr;

    return UniqueConstCodeSegment(cs.release());
}

bool
CodeSegment::initialize(uint8_t* codeBase, uint32_t codeLength,
                        const ShareableBytes& bytecode,
                        const LinkData& linkData,
                        const Metadata& metadata)
{
    MOZ_ASSERT(bytes_ == nullptr);

    bytes_ = codeBase;
    // This CodeSegment instance now owns the code bytes, and the CodeSegment's
    // destructor will take care of freeing those bytes in the case of error.
    functionLength_ = linkData.functionCodeLength;
    length_ = codeLength;
    interruptCode_ = codeBase + linkData.interruptOffset;
    outOfBoundsCode_ = codeBase + linkData.outOfBoundsOffset;
    unalignedAccessCode_ = codeBase + linkData.unalignedAccessOffset;

    if (!StaticallyLink(*this, linkData))
        return false;

    ExecutableAllocator::cacheFlush(codeBase, RoundupCodeLength(codeLength));

    // Reprotect the whole region to avoid having separate RW and RX mappings.
    if (!ExecutableAllocator::makeExecutable(codeBase, RoundupCodeLength(codeLength)))
        return false;

    SendCodeRangesToProfiler(*this, bytecode.bytes, metadata);

    return true;
}

CodeSegment::~CodeSegment()
{
    if (!bytes_)
        return;

    MOZ_ASSERT(length() > 0);
    FreeCodeBytes(bytes_, length());
}

UniqueConstBytes
CodeSegment::unlinkedBytesForDebugging(const LinkData& linkData) const
{
    UniqueBytes unlinkedBytes = js::MakeUnique<Bytes>();
    if (!unlinkedBytes)
        return nullptr;
    if (!unlinkedBytes->append(base(), length()))
        return nullptr;
    StaticallyUnlink(unlinkedBytes->begin(), linkData);
    return UniqueConstBytes(unlinkedBytes.release());
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
CodeSegment::serialize(uint8_t* cursor, const LinkData& linkData) const
{
    cursor = WriteScalar<uint32_t>(cursor, length_);
    uint8_t* base = cursor;
    cursor = WriteBytes(cursor, bytes_, length_);
    StaticallyUnlink(base, linkData);
    return cursor;
}

const uint8_t*
CodeSegment::deserialize(const uint8_t* cursor, const ShareableBytes& bytecode,
                         const LinkData& linkData, const Metadata& metadata)
{
    uint32_t length;
    cursor = ReadScalar<uint32_t>(cursor, &length);
    if (!cursor)
        return nullptr;

    MOZ_ASSERT(length_ % gc::SystemPageSize() == 0);
    uint8_t* bytes = AllocateCodeBytes(length);
    if (!bytes)
        return nullptr;

    cursor = ReadBytes(cursor, bytes, length);
    if (!cursor) {
        FreeCodeBytes(bytes, length);
        return nullptr;
    }

    if (!initialize(bytes, length, bytecode, linkData, metadata))
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
Metadata::serializedSize() const
{
    return sizeof(pod()) +
           SerializedVectorSize(funcImports) +
           SerializedVectorSize(funcExports) +
           SerializedVectorSize(sigIds) +
           SerializedPodVectorSize(globals) +
           SerializedPodVectorSize(tables) +
           SerializedPodVectorSize(memoryAccesses) +
           SerializedPodVectorSize(codeRanges) +
           SerializedPodVectorSize(callSites) +
           SerializedPodVectorSize(funcNames) +
           SerializedPodVectorSize(customSections) +
           filename.serializedSize() +
           sizeof(hash);
}

uint8_t*
Metadata::serialize(uint8_t* cursor) const
{
    MOZ_ASSERT(!debugEnabled && debugTrapFarJumpOffsets.empty() &&
               debugFuncArgTypes.empty() && debugFuncReturnTypes.empty() &&
               debugFuncToCodeRange.empty());
    cursor = WriteBytes(cursor, &pod(), sizeof(pod()));
    cursor = SerializeVector(cursor, funcImports);
    cursor = SerializeVector(cursor, funcExports);
    cursor = SerializeVector(cursor, sigIds);
    cursor = SerializePodVector(cursor, globals);
    cursor = SerializePodVector(cursor, tables);
    cursor = SerializePodVector(cursor, memoryAccesses);
    cursor = SerializePodVector(cursor, codeRanges);
    cursor = SerializePodVector(cursor, callSites);
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
    (cursor = DeserializeVector(cursor, &funcImports)) &&
    (cursor = DeserializeVector(cursor, &funcExports)) &&
    (cursor = DeserializeVector(cursor, &sigIds)) &&
    (cursor = DeserializePodVector(cursor, &globals)) &&
    (cursor = DeserializePodVector(cursor, &tables)) &&
    (cursor = DeserializePodVector(cursor, &memoryAccesses)) &&
    (cursor = DeserializePodVector(cursor, &codeRanges)) &&
    (cursor = DeserializePodVector(cursor, &callSites)) &&
    (cursor = DeserializePodVector(cursor, &funcNames)) &&
    (cursor = DeserializePodVector(cursor, &customSections)) &&
    (cursor = filename.deserialize(cursor)) &&
    (cursor = ReadBytes(cursor, hash, sizeof(hash)));
    debugEnabled = false;
    debugTrapFarJumpOffsets.clear();
    debugFuncToCodeRange.clear();
    debugFuncArgTypes.clear();
    debugFuncReturnTypes.clear();
    return cursor;
}

size_t
Metadata::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return SizeOfVectorExcludingThis(funcImports, mallocSizeOf) +
           SizeOfVectorExcludingThis(funcExports, mallocSizeOf) +
           SizeOfVectorExcludingThis(sigIds, mallocSizeOf) +
           globals.sizeOfExcludingThis(mallocSizeOf) +
           tables.sizeOfExcludingThis(mallocSizeOf) +
           memoryAccesses.sizeOfExcludingThis(mallocSizeOf) +
           codeRanges.sizeOfExcludingThis(mallocSizeOf) +
           callSites.sizeOfExcludingThis(mallocSizeOf) +
           funcNames.sizeOfExcludingThis(mallocSizeOf) +
           customSections.sizeOfExcludingThis(mallocSizeOf) +
           filename.sizeOfExcludingThis(mallocSizeOf);
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
Metadata::lookupFuncExport(uint32_t funcIndex) const
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
        MOZ_ASSERT(n.offset + n.length < maybeBytecode->length());

        if (n.length != 0)
            return name->append((const char*)maybeBytecode->begin() + n.offset, n.length);
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

Code::Code(UniqueConstCodeSegment segment,
           const Metadata& metadata,
           const ShareableBytes* maybeBytecode)
  : segment_(Move(segment)),
    metadata_(&metadata),
    maybeBytecode_(maybeBytecode),
    profilingLabels_(mutexid::WasmCodeProfilingLabels, CacheableCharsVector())
{
    MOZ_ASSERT_IF(metadata_->debugEnabled, maybeBytecode);
}

Code::Code()
  : profilingLabels_(mutexid::WasmCodeProfilingLabels, CacheableCharsVector())
{
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
           segment().serializedSize();
}

uint8_t*
Code::serialize(uint8_t* cursor, const LinkData& linkData) const
{
    MOZ_RELEASE_ASSERT(!metadata().debugEnabled);

    cursor = metadata().serialize(cursor);
    cursor = segment().serialize(cursor, linkData);
    return cursor;
}

const uint8_t*
Code::deserialize(const uint8_t* cursor, const SharedBytes& bytecode, const LinkData& linkData,
                  Metadata* maybeMetadata)
{
    MutableMetadata metadata;
    if (maybeMetadata) {
        metadata = maybeMetadata;
    } else {
        metadata = js_new<Metadata>();
        if (!metadata)
            return nullptr;
    }
    cursor = metadata->deserialize(cursor);
    if (!cursor)
        return nullptr;

    UniqueCodeSegment codeSegment = js::MakeUnique<CodeSegment>();
    if (!codeSegment)
        return nullptr;

    cursor = codeSegment->deserialize(cursor, *bytecode, linkData, *metadata);
    if (!cursor)
        return nullptr;

    segment_ = UniqueConstCodeSegment(codeSegment.release());
    metadata_ = metadata;
    maybeBytecode_ = bytecode;

    return cursor;
}

const CallSite*
Code::lookupCallSite(void* returnAddress) const
{
    uint32_t target = ((uint8_t*)returnAddress) - segment_->base();
    size_t lowerBound = 0;
    size_t upperBound = metadata().callSites.length();

    size_t match;
    if (!BinarySearch(CallSiteRetAddrOffset(metadata().callSites), lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata().callSites[match];
}

const CodeRange*
Code::lookupRange(void* pc) const
{
    CodeRange::OffsetInCode target((uint8_t*)pc - segment_->base());
    return LookupInSorted(metadata().codeRanges, target);
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
Code::lookupMemoryAccess(void* pc) const
{
    MOZ_ASSERT(segment_->containsFunctionPC(pc));

    uint32_t target = ((uint8_t*)pc) - segment_->base();
    size_t lowerBound = 0;
    size_t upperBound = metadata().memoryAccesses.length();

    size_t match;
    if (!BinarySearch(MemoryAccessOffset(metadata().memoryAccesses), lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata().memoryAccesses[match];
}

bool
Code::getFuncName(uint32_t funcIndex, UTF8Bytes* name) const
{
    const Bytes* maybeBytecode = maybeBytecode_ ? &maybeBytecode_.get()->bytes : nullptr;
    return metadata().getFuncName(maybeBytecode, funcIndex, name);
}

JSAtom*
Code::getFuncAtom(JSContext* cx, uint32_t funcIndex) const
{
    UTF8Bytes name;
    if (!getFuncName(funcIndex, &name))
        return nullptr;

    return AtomizeUTF8Chars(cx, name.begin(), name.length());
}

// When enabled, generate profiling labels for every name in funcNames_ that is
// the name of some Function CodeRange. This involves malloc() so do it now
// since, once we start sampling, we'll be in a signal-handing context where we
// cannot malloc.
void
Code::ensureProfilingLabels(bool profilingEnabled) const
{
    auto labels = profilingLabels_.lock();

    if (!profilingEnabled) {
        labels->clear();
        return;
    }

    if (!labels->empty())
        return;

    for (const CodeRange& codeRange : metadata().codeRanges) {
        if (!codeRange.isFunction())
            continue;

        ToCStringBuf cbuf;
        const char* bytecodeStr = NumberToCString(nullptr, &cbuf, codeRange.funcLineOrBytecode());
        MOZ_ASSERT(bytecodeStr);

        UTF8Bytes name;
        if (!getFuncName(codeRange.funcIndex(), &name) || !name.append(" (", 2))
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
                             ShareableBytes::SeenSet* seenBytes,
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

    segment_->addSizeOfMisc(mallocSizeOf, code, data);

    if (maybeBytecode_)
        *data += maybeBytecode_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenBytes);
}
