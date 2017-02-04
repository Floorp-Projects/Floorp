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

#include "ds/Sort.h"
#include "jit/ExecutableAllocator.h"
#include "jit/MacroAssembler.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "vm/Debugger.h"
#include "vm/StringBuffer.h"
#ifdef MOZ_VTUNE
# include "vtune/VTuneWrapper.h"
#endif
#include "wasm/WasmBinaryToText.h"
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
using mozilla::BinarySearchIf;
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

static uint8_t*
AllocateCodeSegment(JSContext* cx, uint32_t codeLength)
{
    if (wasmCodeAllocations >= MaxWasmCodeAllocations)
        return nullptr;

    // Allocate RW memory. DynamicallyLinkModule will reprotect the code as RX.
    unsigned permissions =
        ExecutableAllocator::initialProtectionFlags(ExecutableAllocator::Writable);

    void* p = AllocateExecutableMemory(codeLength, permissions, "wasm-code-segment",
                                       gc::SystemPageSize());
    if (!p) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    wasmCodeAllocations++;
    return (uint8_t*)p;
}

static void
StaticallyLink(CodeSegment& cs, const LinkData& linkData, JSContext* cx)
{
    for (LinkData::InternalLink link : linkData.internalLinks) {
        uint8_t* patchAt = cs.base() + link.patchAtOffset;
        void* target = cs.base() + link.targetOffset;
        if (link.isRawPointerPatch())
            *(void**)(patchAt) = target;
        else
            Assembler::PatchInstructionImmediate(patchAt, PatchedImmPtr(target));
    }

    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        const Uint32Vector& offsets = linkData.symbolicLinks[imm];
        for (size_t i = 0; i < offsets.length(); i++) {
            uint8_t* patchAt = cs.base() + offsets[i];
            void* target = AddressOf(imm, cx);
            Assembler::PatchDataWithValueCheck(CodeLocationLabel(patchAt),
                                               PatchedImmPtr(target),
                                               PatchedImmPtr((void*)-1));
        }
    }
}

static void
SpecializeToMemory(uint8_t* prevMemoryBase, CodeSegment& cs, const Metadata& metadata,
                   ArrayBufferObjectMaybeShared& buffer)
{
#ifdef WASM_HUGE_MEMORY
    MOZ_RELEASE_ASSERT(metadata.boundsChecks.empty());
#else
    uint32_t limit = buffer.wasmBoundsCheckLimit();
    MOZ_RELEASE_ASSERT(IsValidBoundsCheckImmediate(limit));

    for (const BoundsCheck& check : metadata.boundsChecks)
        MacroAssembler::wasmPatchBoundsCheck(check.patchAt(cs.base()), limit);
#endif

#if defined(JS_CODEGEN_X86)
    uint8_t* memoryBase = buffer.dataPointerEither().unwrap(/* code patching */);
    if (prevMemoryBase != memoryBase) {
        for (MemoryPatch patch : metadata.memoryPatches) {
            void* patchAt = cs.base() + patch.offset;

            uint8_t* prevImm = (uint8_t*)X86Encoding::GetPointer(patchAt);
            MOZ_ASSERT(prevImm >= prevMemoryBase);

            uint32_t offset = prevImm - prevMemoryBase;
            MOZ_ASSERT(offset <= INT32_MAX);

            X86Encoding::SetPointer(patchAt, memoryBase + offset);
        }
    }
#else
    MOZ_RELEASE_ASSERT(metadata.memoryPatches.empty());
#endif
}

static void
SendCodeRangesToProfiler(CodeSegment& cs, const Bytes& bytecode, const Metadata& metadata)
{
    bool enabled = false;
#ifdef JS_ION_PERF
    enabled |= PerfFuncEnabled();
#endif
#ifdef MOZ_VTUNE
    enabled |= IsVTuneProfilingActive();
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
        if (IsVTuneProfilingActive()) {
            unsigned method_id = iJIT_GetNewMethodID();
            if (method_id == 0)
                return;
            iJIT_Method_Load method;
            method.method_id = method_id;
            method.method_name = name.begin();
            method.method_load_address = (void*)start;
            method.method_size = size;
            method.line_number_size = 0;
            method.line_number_table = nullptr;
            method.class_id = 0;
            method.class_file_name = nullptr;
            method.source_file_name = nullptr;
            iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&method);
        }
#endif
    }

    return;
}

/* static */ UniqueCodeSegment
CodeSegment::create(JSContext* cx,
                    const Bytes& bytecode,
                    const LinkData& linkData,
                    const Metadata& metadata,
                    HandleWasmMemoryObject memory)
{
    MOZ_ASSERT(bytecode.length() % gc::SystemPageSize() == 0);
    MOZ_ASSERT(linkData.functionCodeLength < bytecode.length());

    // These should always exist and should never be first in the code segment.
    MOZ_ASSERT(linkData.interruptOffset != 0);
    MOZ_ASSERT(linkData.outOfBoundsOffset != 0);
    MOZ_ASSERT(linkData.unalignedAccessOffset != 0);

    auto cs = cx->make_unique<CodeSegment>();
    if (!cs)
        return nullptr;

    cs->bytes_ = AllocateCodeSegment(cx, bytecode.length());
    if (!cs->bytes_)
        return nullptr;

    uint8_t* codeBase = cs->base();

    cs->functionLength_ = linkData.functionCodeLength;
    cs->length_ = bytecode.length();
    cs->interruptCode_ = codeBase + linkData.interruptOffset;
    cs->outOfBoundsCode_ = codeBase + linkData.outOfBoundsOffset;
    cs->unalignedAccessCode_ = codeBase + linkData.unalignedAccessOffset;

    {
        JitContext jcx(CompileRuntime::get(cx->compartment()->runtimeFromAnyThread()));
        AutoFlushICache afc("CodeSegment::create");
        AutoFlushICache::setRange(uintptr_t(codeBase), cs->length());

        memcpy(codeBase, bytecode.begin(), bytecode.length());
        StaticallyLink(*cs, linkData, cx);
        if (memory)
            SpecializeToMemory(nullptr, *cs, metadata, memory->buffer());
    }

    if (!ExecutableAllocator::makeExecutable(codeBase, cs->length())) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    SendCodeRangesToProfiler(*cs, bytecode, metadata);

    return cs;
}

CodeSegment::~CodeSegment()
{
    if (!bytes_)
        return;

    MOZ_ASSERT(wasmCodeAllocations > 0);
    wasmCodeAllocations--;

    MOZ_ASSERT(length() > 0);
    DeallocateExecutableMemory(bytes_, length(), gc::SystemPageSize());
}

void
CodeSegment::onMovingGrow(uint8_t* prevMemoryBase, const Metadata& metadata, ArrayBufferObject& buffer)
{
    AutoWritableJitCode awjc(base(), length());
    AutoFlushICache afc("CodeSegment::onMovingGrow");
    AutoFlushICache::setRange(uintptr_t(base()), length());

    SpecializeToMemory(prevMemoryBase, *this, metadata, buffer);
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

CodeRange::CodeRange(Kind kind, Offsets offsets)
  : begin_(offsets.begin),
    profilingReturn_(0),
    end_(offsets.end),
    funcIndex_(0),
    funcLineOrBytecode_(0),
    funcBeginToTableEntry_(0),
    funcBeginToTableProfilingJump_(0),
    funcBeginToNonProfilingEntry_(0),
    funcProfilingJumpToProfilingReturn_(0),
    funcProfilingEpilogueToProfilingReturn_(0),
    kind_(kind)
{
    MOZ_ASSERT(begin_ <= end_);
    MOZ_ASSERT(kind_ == Entry || kind_ == Inline ||
               kind_ == FarJumpIsland || kind_ == DebugTrap);
}

CodeRange::CodeRange(Kind kind, ProfilingOffsets offsets)
  : begin_(offsets.begin),
    profilingReturn_(offsets.profilingReturn),
    end_(offsets.end),
    funcIndex_(0),
    funcLineOrBytecode_(0),
    funcBeginToTableEntry_(0),
    funcBeginToTableProfilingJump_(0),
    funcBeginToNonProfilingEntry_(0),
    funcProfilingJumpToProfilingReturn_(0),
    funcProfilingEpilogueToProfilingReturn_(0),
    kind_(kind)
{
    MOZ_ASSERT(begin_ < profilingReturn_);
    MOZ_ASSERT(profilingReturn_ < end_);
    MOZ_ASSERT(kind_ == ImportJitExit || kind_ == ImportInterpExit || kind_ == TrapExit);
}

CodeRange::CodeRange(uint32_t funcIndex, uint32_t funcLineOrBytecode, FuncOffsets offsets)
  : begin_(offsets.begin),
    profilingReturn_(offsets.profilingReturn),
    end_(offsets.end),
    funcIndex_(funcIndex),
    funcLineOrBytecode_(funcLineOrBytecode),
    funcBeginToTableEntry_(offsets.tableEntry - begin_),
    funcBeginToTableProfilingJump_(offsets.tableProfilingJump - begin_),
    funcBeginToNonProfilingEntry_(offsets.nonProfilingEntry - begin_),
    funcProfilingJumpToProfilingReturn_(profilingReturn_ - offsets.profilingJump),
    funcProfilingEpilogueToProfilingReturn_(profilingReturn_ - offsets.profilingEpilogue),
    kind_(Function)
{
    MOZ_ASSERT(begin_ < profilingReturn_);
    MOZ_ASSERT(profilingReturn_ < end_);
    MOZ_ASSERT(offsets.tableEntry - begin_ <= UINT8_MAX);
    MOZ_ASSERT(offsets.tableProfilingJump - begin_ <= UINT8_MAX);
    MOZ_ASSERT(offsets.nonProfilingEntry - begin_ <= UINT8_MAX);
    MOZ_ASSERT(profilingReturn_ - offsets.profilingJump <= UINT8_MAX);
    MOZ_ASSERT(profilingReturn_ - offsets.profilingEpilogue <= UINT8_MAX);
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
           SerializedPodVectorSize(memoryPatches) +
           SerializedPodVectorSize(boundsChecks) +
           SerializedPodVectorSize(codeRanges) +
           SerializedPodVectorSize(callSites) +
           SerializedPodVectorSize(callThunks) +
           SerializedPodVectorSize(funcNames) +
           SerializedPodVectorSize(customSections) +
           filename.serializedSize();
}

uint8_t*
Metadata::serialize(uint8_t* cursor) const
{
    MOZ_ASSERT(!debugEnabled && debugTrapFarJumpOffsets.empty());
    cursor = WriteBytes(cursor, &pod(), sizeof(pod()));
    cursor = SerializeVector(cursor, funcImports);
    cursor = SerializeVector(cursor, funcExports);
    cursor = SerializeVector(cursor, sigIds);
    cursor = SerializePodVector(cursor, globals);
    cursor = SerializePodVector(cursor, tables);
    cursor = SerializePodVector(cursor, memoryAccesses);
    cursor = SerializePodVector(cursor, memoryPatches);
    cursor = SerializePodVector(cursor, boundsChecks);
    cursor = SerializePodVector(cursor, codeRanges);
    cursor = SerializePodVector(cursor, callSites);
    cursor = SerializePodVector(cursor, callThunks);
    cursor = SerializePodVector(cursor, funcNames);
    cursor = SerializePodVector(cursor, customSections);
    cursor = filename.serialize(cursor);
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
    (cursor = DeserializePodVector(cursor, &memoryPatches)) &&
    (cursor = DeserializePodVector(cursor, &boundsChecks)) &&
    (cursor = DeserializePodVector(cursor, &codeRanges)) &&
    (cursor = DeserializePodVector(cursor, &callSites)) &&
    (cursor = DeserializePodVector(cursor, &callThunks)) &&
    (cursor = DeserializePodVector(cursor, &funcNames)) &&
    (cursor = DeserializePodVector(cursor, &customSections)) &&
    (cursor = filename.deserialize(cursor));
    debugEnabled = false;
    debugTrapFarJumpOffsets.clear();
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
           memoryPatches.sizeOfExcludingThis(mallocSizeOf) +
           boundsChecks.sizeOfExcludingThis(mallocSizeOf) +
           codeRanges.sizeOfExcludingThis(mallocSizeOf) +
           callSites.sizeOfExcludingThis(mallocSizeOf) +
           callThunks.sizeOfExcludingThis(mallocSizeOf) +
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

bool
GeneratedSourceMap::searchLineByOffset(JSContext* cx, uint32_t offset, size_t* exprlocIndex)
{
    MOZ_ASSERT(!exprlocs_.empty());
    size_t exprlocsLength = exprlocs_.length();

    // Lazily build sorted array for fast log(n) lookup.
    if (!sortedByOffsetExprLocIndices_) {
        ExprLocIndexVector scratch;
        auto indices = MakeUnique<ExprLocIndexVector>();
        if (!indices || !indices->resize(exprlocsLength) || !scratch.resize(exprlocsLength)) {
            ReportOutOfMemory(cx);
            return false;
        }
        sortedByOffsetExprLocIndices_ = Move(indices);

        for (size_t i = 0; i < exprlocsLength; i++)
            (*sortedByOffsetExprLocIndices_)[i] = i;

        auto compareExprLocViaIndex = [&](uint32_t i, uint32_t j, bool* lessOrEqualp) -> bool {
            *lessOrEqualp = exprlocs_[i].offset <= exprlocs_[j].offset;
            return true;
        };
        MOZ_ALWAYS_TRUE(MergeSort(sortedByOffsetExprLocIndices_->begin(), exprlocsLength,
                                  scratch.begin(), compareExprLocViaIndex));
    }

    // Allowing non-exact search and if BinarySearchIf returns out-of-bound
    // index, moving the index to the last index.
    auto lookupFn = [&](uint32_t i) -> int {
        const ExprLoc& loc = exprlocs_[i];
        return offset == loc.offset ? 0 : offset < loc.offset ? -1 : 1;
    };
    size_t match;
    Unused << BinarySearchIf(sortedByOffsetExprLocIndices_->begin(), 0, exprlocsLength, lookupFn, &match);
    if (match >= exprlocsLength)
        match = exprlocsLength - 1;
    *exprlocIndex = (*sortedByOffsetExprLocIndices_)[match];
    return true;
}

Code::Code(UniqueCodeSegment segment,
           const Metadata& metadata,
           const ShareableBytes* maybeBytecode)
  : segment_(Move(segment)),
    metadata_(&metadata),
    maybeBytecode_(maybeBytecode),
    profilingEnabled_(false),
    enterAndLeaveFrameTrapsCounter_(0)
{
    MOZ_ASSERT_IF(metadata_->debugEnabled, maybeBytecode);
}

struct CallSiteRetAddrOffset
{
    const CallSiteVector& callSites;
    explicit CallSiteRetAddrOffset(const CallSiteVector& callSites) : callSites(callSites) {}
    uint32_t operator[](size_t index) const {
        return callSites[index].returnAddressOffset();
    }
};

const CallSite*
Code::lookupCallSite(void* returnAddress) const
{
    uint32_t target = ((uint8_t*)returnAddress) - segment_->base();
    size_t lowerBound = 0;
    size_t upperBound = metadata_->callSites.length();

    size_t match;
    if (!BinarySearch(CallSiteRetAddrOffset(metadata_->callSites), lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->callSites[match];
}

const CodeRange*
Code::lookupRange(void* pc) const
{
    CodeRange::PC target((uint8_t*)pc - segment_->base());
    size_t lowerBound = 0;
    size_t upperBound = metadata_->codeRanges.length();

    size_t match;
    if (!BinarySearch(metadata_->codeRanges, lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->codeRanges[match];
}

const CodeRange*
Code::lookupRangeByFuncIndexSlow(uint32_t funcIndex) const
{
    for (const CodeRange& r : metadata_->codeRanges) {
        if (r.kind() == CodeRange::Function && r.funcIndex() == funcIndex)
            return &r;
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
Code::lookupMemoryAccess(void* pc) const
{
    MOZ_ASSERT(segment_->containsFunctionPC(pc));

    uint32_t target = ((uint8_t*)pc) - segment_->base();
    size_t lowerBound = 0;
    size_t upperBound = metadata_->memoryAccesses.length();

    size_t match;
    if (!BinarySearch(MemoryAccessOffset(metadata_->memoryAccesses), lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->memoryAccesses[match];
}

bool
Code::getFuncName(uint32_t funcIndex, UTF8Bytes* name) const
{
    const Bytes* maybeBytecode = maybeBytecode_ ? &maybeBytecode_.get()->bytes : nullptr;
    return metadata_->getFuncName(maybeBytecode, funcIndex, name);
}

JSAtom*
Code::getFuncAtom(JSContext* cx, uint32_t funcIndex) const
{
    UTF8Bytes name;
    if (!getFuncName(funcIndex, &name))
        return nullptr;

    return AtomizeUTF8Chars(cx, name.begin(), name.length());
}

const char experimentalWarning[] =
    "Temporary\n"
    ".--.      .--.   ____       .-'''-. ,---.    ,---.\n"
    "|  |_     |  | .'  __ `.   / _     \\|    \\  /    |\n"
    "| _( )_   |  |/   '  \\  \\ (`' )/`--'|  ,  \\/  ,  |\n"
    "|(_ o _)  |  ||___|  /  |(_ o _).   |  |\\_   /|  |\n"
    "| (_,_) \\ |  |   _.-`   | (_,_). '. |  _( )_/ |  |\n"
    "|  |/    \\|  |.'   _    |.---.  \\  :| (_ o _) |  |\n"
    "|  '  /\\  `  ||  _( )_  |\\    `-'  ||  (_,_)  |  |\n"
    "|    /  \\    |\\ (_ o _) / \\       / |  |      |  |\n"
    "`---'    `---` '.(_,_).'   `-...-'  '--'      '--'\n"
    "text support (Work In Progress):\n\n";

const size_t experimentalWarningLinesCount = 12;

const char enabledMessage[] =
    "Restart with developer tools open to view WebAssembly source";

struct LineComparator
{
    const uint32_t lineno;
    explicit LineComparator(uint32_t lineno) : lineno(lineno) {}

    int operator()(const ExprLoc& loc) const {
        return lineno == loc.lineno ? 0 : lineno < loc.lineno ? -1 : 1;
    }
};

JSString*
Code::createText(JSContext* cx)
{
    StringBuffer buffer(cx);
    if (maybeBytecode_) {
        const Bytes& bytes = maybeBytecode_->bytes;
        if (!buffer.append(experimentalWarning))
            return nullptr;

        auto sourceMap = MakeUnique<GeneratedSourceMap>();
        if (!sourceMap) {
            ReportOutOfMemory(cx);
            return nullptr;
        }
        maybeSourceMap_ = Move(sourceMap);

        if (!BinaryToText(cx, bytes.begin(), bytes.length(), buffer, maybeSourceMap_.get()))
            return nullptr;

#if DEBUG
        // Checking source map invariant: expression and function locations must be sorted
        // by line number.
        uint32_t lastLineno = 0;
        for (const ExprLoc& loc : maybeSourceMap_->exprlocs()) {
            MOZ_ASSERT(lastLineno <= loc.lineno);
            lastLineno = loc.lineno;
        }
        lastLineno = 0;
        for (const FunctionLoc& loc : maybeSourceMap_->functionlocs()) {
            MOZ_ASSERT(lastLineno <= loc.startLineno);
            MOZ_ASSERT(loc.startLineno <= loc.endLineno);
            lastLineno = loc.endLineno + 1;
        }
#endif
    } else {
        if (!buffer.append(enabledMessage))
            return nullptr;
    }
    return buffer.finishString();
}

bool
Code::ensureSourceMap(JSContext* cx)
{
    if (maybeSourceMap_ || !maybeBytecode_)
        return true;

    // We just need to cache maybeSourceMap_, ignoring the text result.
    return createText(cx);
}

bool
Code::getLineOffsets(JSContext* cx, size_t lineno, Vector<uint32_t>* offsets)
{
    if (!metadata_->debugEnabled)
        return true;

    if (!ensureSourceMap(cx))
        return false;

    if (!maybeSourceMap_)
        return true; // no source text available, keep offsets empty.

    if (lineno < experimentalWarningLinesCount)
        return true;

    lineno -= experimentalWarningLinesCount;

    ExprLocVector& exprlocs = maybeSourceMap_->exprlocs();

    // Binary search for the expression with the specified line number and
    // rewind to the first expression, if more than one expression on the same line.
    size_t match;
    if (!BinarySearchIf(exprlocs, 0, exprlocs.length(), LineComparator(lineno), &match))
        return true;

    while (match > 0 && exprlocs[match - 1].lineno == lineno)
        match--;

    // Return all expression offsets that were printed on the specified line.
    for (size_t i = match; i < exprlocs.length() && exprlocs[i].lineno == lineno; i++) {
        if (!offsets->append(exprlocs[i].offset))
            return false;
    }

    return true;
}

bool
Code::getOffsetLocation(JSContext* cx, uint32_t offset, bool* found, size_t* lineno, size_t* column)
{
    *found = false;
    if (!metadata_->debugEnabled)
        return true;

    if (!ensureSourceMap(cx))
        return false;

    if (!maybeSourceMap_ || maybeSourceMap_->exprlocs().empty())
        return true; // no source text available

    size_t foundAt;
    if (!maybeSourceMap_->searchLineByOffset(cx, offset, &foundAt))
        return false;

    const ExprLoc& loc = maybeSourceMap_->exprlocs()[foundAt];
    *found = true;
    *lineno = loc.lineno + experimentalWarningLinesCount;
    *column = loc.column;
    return true;
}

bool
Code::totalSourceLines(JSContext* cx, uint32_t* count)
{
    *count = 0;
    if (!metadata_->debugEnabled)
        return true;

    if (!ensureSourceMap(cx))
        return false;

    if (maybeSourceMap_)
        *count = maybeSourceMap_->totalLines() + experimentalWarningLinesCount;
    return true;
}

bool
Code::stepModeEnabled(uint32_t funcIndex) const
{
    return stepModeCounters_.initialized() && stepModeCounters_.lookup(funcIndex);
}

bool
Code::incrementStepModeCount(JSContext* cx, uint32_t funcIndex)
{
    MOZ_ASSERT(metadata_->debugEnabled);
    const CodeRange* codeRange = lookupRangeByFuncIndexSlow(funcIndex);
    MOZ_ASSERT(codeRange && codeRange->isFunction());

    if (!stepModeCounters_.initialized() && !stepModeCounters_.init()) {
        ReportOutOfMemory(cx);
        return false;
    }

    StepModeCounters::AddPtr p = stepModeCounters_.lookupForAdd(funcIndex);
    if (p) {
        MOZ_ASSERT(p->value() > 0);
        p->value()++;
        return true;
    }
    if (!stepModeCounters_.add(p, funcIndex, 1)) {
        ReportOutOfMemory(cx);
        return false;
    }

    AutoWritableJitCode awjc(cx->runtime(), segment_->base() + codeRange->begin(),
                             codeRange->end() - codeRange->begin());
    AutoFlushICache afc("Code::incrementStepModeCount");

    for (const CallSite& callSite : metadata_->callSites) {
        if (callSite.kind() != CallSite::Breakpoint)
            continue;
        uint32_t offset = callSite.returnAddressOffset();
        if (codeRange->begin() <= offset && offset <= codeRange->end())
            toggleDebugTrap(offset, true);
    }
    return true;
}

bool
Code::decrementStepModeCount(JSContext* cx, uint32_t funcIndex)
{
    MOZ_ASSERT(metadata_->debugEnabled);
    const CodeRange* codeRange = lookupRangeByFuncIndexSlow(funcIndex);
    MOZ_ASSERT(codeRange && codeRange->isFunction());

    MOZ_ASSERT(stepModeCounters_.initialized() && !stepModeCounters_.empty());
    StepModeCounters::Ptr p = stepModeCounters_.lookup(funcIndex);
    MOZ_ASSERT(p);
    if (--p->value())
        return true;

    stepModeCounters_.remove(p);

    AutoWritableJitCode awjc(cx->runtime(), segment_->base() + codeRange->begin(),
                             codeRange->end() - codeRange->begin());
    AutoFlushICache afc("Code::decrementStepModeCount");

    for (const CallSite& callSite : metadata_->callSites) {
        if (callSite.kind() != CallSite::Breakpoint)
            continue;
        uint32_t offset = callSite.returnAddressOffset();
        if (codeRange->begin() <= offset && offset <= codeRange->end()) {
            bool enabled = breakpointSites_.initialized() && breakpointSites_.has(offset);
            toggleDebugTrap(offset, enabled);
        }
    }
    return true;
}

static const CallSite*
SlowCallSiteSearchByOffset(const Metadata& metadata, uint32_t offset)
{
    for (const CallSite& callSite : metadata.callSites) {
        if (callSite.lineOrBytecode() == offset && callSite.kind() == CallSiteDesc::Breakpoint)
            return &callSite;
    }
    return nullptr;
}

bool
Code::hasBreakpointTrapAtOffset(uint32_t offset)
{
    if (!metadata_->debugEnabled)
        return false;
    return SlowCallSiteSearchByOffset(*metadata_, offset);
}

void
Code::toggleBreakpointTrap(JSRuntime* rt, uint32_t offset, bool enabled)
{
    MOZ_ASSERT(metadata_->debugEnabled);
    const CallSite* callSite = SlowCallSiteSearchByOffset(*metadata_, offset);
    if (!callSite)
        return;
    size_t debugTrapOffset = callSite->returnAddressOffset();

    const CodeRange* codeRange = lookupRange(segment_->base() + debugTrapOffset);
    MOZ_ASSERT(codeRange && codeRange->isFunction());

    if (stepModeCounters_.initialized() && stepModeCounters_.lookup(codeRange->funcIndex()))
        return; // no need to toggle when step mode is enabled

    AutoWritableJitCode awjc(rt, segment_->base(), segment_->length());
    AutoFlushICache afc("Code::toggleBreakpointTrap");
    AutoFlushICache::setRange(uintptr_t(segment_->base()), segment_->length());
    toggleDebugTrap(debugTrapOffset, enabled);
}

WasmBreakpointSite*
Code::getOrCreateBreakpointSite(JSContext* cx, uint32_t offset)
{
    WasmBreakpointSite* site;
    if (!breakpointSites_.initialized() && !breakpointSites_.init()) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    WasmBreakpointSiteMap::AddPtr p = breakpointSites_.lookupForAdd(offset);
    if (!p) {
        site = cx->runtime()->new_<WasmBreakpointSite>(this, offset);
        if (!site || !breakpointSites_.add(p, offset, site)) {
            js_delete(site);
            ReportOutOfMemory(cx);
            return nullptr;
        }
    } else {
        site = p->value();
    }
    return site;
}

bool
Code::hasBreakpointSite(uint32_t offset)
{
    return breakpointSites_.initialized() && breakpointSites_.has(offset);
}

void
Code::destroyBreakpointSite(FreeOp* fop, uint32_t offset)
{
    MOZ_ASSERT(breakpointSites_.initialized());
    WasmBreakpointSiteMap::Ptr p = breakpointSites_.lookup(offset);
    MOZ_ASSERT(p);
    fop->delete_(p->value());
    breakpointSites_.remove(p);
}

bool
Code::clearBreakpointsIn(JSContext* cx, WasmInstanceObject* instance, js::Debugger* dbg, JSObject* handler)
{
    MOZ_ASSERT(instance);
    if (!breakpointSites_.initialized())
        return true;

    // Make copy of all sites list, so breakpointSites_ can be modified by
    // destroyBreakpointSite calls.
    Vector<WasmBreakpointSite*> sites(cx);
    if (!sites.resize(breakpointSites_.count()))
        return false;
    size_t i = 0;
    for (WasmBreakpointSiteMap::Range r = breakpointSites_.all(); !r.empty(); r.popFront())
        sites[i++] = r.front().value();

    for (WasmBreakpointSite* site : sites) {
        Breakpoint* nextbp;
        for (Breakpoint* bp = site->firstBreakpoint(); bp; bp = nextbp) {
            nextbp = bp->nextInSite();
            if (bp->asWasm()->wasmInstance == instance &&
                (!dbg || bp->debugger == dbg) &&
                (!handler || bp->getHandler() == handler))
            {
                bp->destroy(cx->runtime()->defaultFreeOp());
            }
        }
    }
    return true;
}


bool
Code::ensureProfilingState(JSRuntime* rt, bool newProfilingEnabled)
{
    if (profilingEnabled_ == newProfilingEnabled)
        return true;

    // When enabled, generate profiling labels for every name in funcNames_
    // that is the name of some Function CodeRange. This involves malloc() so
    // do it now since, once we start sampling, we'll be in a signal-handing
    // context where we cannot malloc.
    if (newProfilingEnabled) {
        for (const CodeRange& codeRange : metadata_->codeRanges) {
            if (!codeRange.isFunction())
                continue;

            ToCStringBuf cbuf;
            const char* bytecodeStr = NumberToCString(nullptr, &cbuf, codeRange.funcLineOrBytecode());
            MOZ_ASSERT(bytecodeStr);

            UTF8Bytes name;
            if (!getFuncName(codeRange.funcIndex(), &name) || !name.append(" (", 2))
                return false;

            if (const char* filename = metadata_->filename.get()) {
                if (!name.append(filename, strlen(filename)))
                    return false;
            } else {
                if (!name.append('?'))
                    return false;
            }

            if (!name.append(':') ||
                !name.append(bytecodeStr, strlen(bytecodeStr)) ||
                !name.append(")\0", 2))
            {
                return false;
            }

            UniqueChars label(name.extractOrCopyRawBuffer());
            if (!label)
                return false;

            if (codeRange.funcIndex() >= funcLabels_.length()) {
                if (!funcLabels_.resize(codeRange.funcIndex() + 1))
                    return false;
            }

            funcLabels_[codeRange.funcIndex()] = Move(label);
        }
    } else {
        funcLabels_.clear();
    }

    // Only mutate the code after the fallible operations are complete to avoid
    // the need to rollback.
    profilingEnabled_ = newProfilingEnabled;

    {
        AutoWritableJitCode awjc(segment_->base(), segment_->length());
        AutoFlushICache afc("Code::ensureProfilingState");
        AutoFlushICache::setRange(uintptr_t(segment_->base()), segment_->length());

        for (const CallSite& callSite : metadata_->callSites)
            ToggleProfiling(*this, callSite, newProfilingEnabled);
        for (const CallThunk& callThunk : metadata_->callThunks)
            ToggleProfiling(*this, callThunk, newProfilingEnabled);
        for (const CodeRange& codeRange : metadata_->codeRanges)
            ToggleProfiling(*this, codeRange, newProfilingEnabled);
    }

    return true;
}

void
Code::toggleDebugTrap(uint32_t offset, bool enabled)
{
    MOZ_ASSERT(offset);
    uint8_t* trap = segment_->base() + offset;
    const Uint32Vector& farJumpOffsets = metadata_->debugTrapFarJumpOffsets;
    if (enabled) {
        MOZ_ASSERT(farJumpOffsets.length() > 0);
        size_t i = 0;
        while (i < farJumpOffsets.length() && offset < farJumpOffsets[i])
            i++;
        if (i >= farJumpOffsets.length() ||
            (i > 0 && offset - farJumpOffsets[i - 1] < farJumpOffsets[i] - offset))
            i--;
        uint8_t* farJump = segment_->base() + farJumpOffsets[i];
        MacroAssembler::patchNopToCall(trap, farJump);
    } else {
        MacroAssembler::patchCallToNop(trap);
    }
}

void
Code::adjustEnterAndLeaveFrameTrapsState(JSContext* cx, bool enabled)
{
    MOZ_ASSERT(metadata_->debugEnabled);
    MOZ_ASSERT_IF(!enabled, enterAndLeaveFrameTrapsCounter_ > 0);

    bool wasEnabled = enterAndLeaveFrameTrapsCounter_ > 0;
    if (enabled)
        ++enterAndLeaveFrameTrapsCounter_;
    else
        --enterAndLeaveFrameTrapsCounter_;
    bool stillEnabled = enterAndLeaveFrameTrapsCounter_ > 0;
    if (wasEnabled == stillEnabled)
        return;

    AutoWritableJitCode awjc(cx->runtime(), segment_->base(), segment_->length());
    AutoFlushICache afc("Code::adjustEnterAndLeaveFrameTrapsState");
    AutoFlushICache::setRange(uintptr_t(segment_->base()), segment_->length());
    for (const CallSite& callSite : metadata_->callSites) {
        if (callSite.kind() != CallSite::EnterFrame && callSite.kind() != CallSite::LeaveFrame)
            continue;
        toggleDebugTrap(callSite.returnAddressOffset(), stillEnabled);
    }
}

void
Code::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                    Metadata::SeenSet* seenMetadata,
                    ShareableBytes::SeenSet* seenBytes,
                    size_t* code,
                    size_t* data) const
{
    *code += segment_->length();
    *data += mallocSizeOf(this) +
             metadata_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenMetadata);

    if (maybeBytecode_)
        *data += maybeBytecode_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenBytes);
}
