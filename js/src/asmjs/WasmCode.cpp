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

#include "asmjs/WasmCode.h"

#include "mozilla/Atomics.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/EnumeratedRange.h"

#include "jsprf.h"

#include "asmjs/WasmModule.h"
#include "asmjs/WasmSerialize.h"
#include "jit/ExecutableAllocator.h"
#include "jit/MacroAssembler.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#ifdef MOZ_VTUNE
# include "vtune/VTuneWrapper.h"
#endif

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

static uint8_t*
AllocateCodeSegment(ExclusiveContext* cx, uint32_t totalLength)
{
    if (wasmCodeAllocations >= MaxWasmCodeAllocations)
        return nullptr;

    // Allocate RW memory. DynamicallyLinkModule will reprotect the code as RX.
    unsigned permissions =
        ExecutableAllocator::initialProtectionFlags(ExecutableAllocator::Writable);

    void* p = AllocateExecutableMemory(nullptr, totalLength, permissions,
                                       "wasm-code-segment", gc::SystemPageSize());
    if (!p) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    wasmCodeAllocations++;
    return (uint8_t*)p;
}

static void
StaticallyLink(CodeSegment& cs, const LinkData& linkData, ExclusiveContext* cx)
{
    for (LinkData::InternalLink link : linkData.internalLinks) {
        uint8_t* patchAt = cs.code() + link.patchAtOffset;
        void* target = cs.code() + link.targetOffset;
        if (link.isRawPointerPatch())
            *(void**)(patchAt) = target;
        else
            Assembler::PatchInstructionImmediate(patchAt, PatchedImmPtr(target));
    }

    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        const Uint32Vector& offsets = linkData.symbolicLinks[imm];
        for (size_t i = 0; i < offsets.length(); i++) {
            uint8_t* patchAt = cs.code() + offsets[i];
            void* target = AddressOf(imm, cx);
            Assembler::PatchDataWithValueCheck(CodeLocationLabel(patchAt),
                                               PatchedImmPtr(target),
                                               PatchedImmPtr((void*)-1));
        }
    }

    // These constants are logically part of the code:

    *(double*)(cs.globalData() + NaN64GlobalDataOffset) = GenericNaN();
    *(float*)(cs.globalData() + NaN32GlobalDataOffset) = GenericNaN();
}

static void
SpecializeToMemory(CodeSegment& cs, const Metadata& metadata, HandleWasmMemoryObject memory)
{
    for (const BoundsCheck& check : metadata.boundsChecks)
        Assembler::UpdateBoundsCheck(check.patchAt(cs.code()), memory->buffer().byteLength());

#if defined(JS_CODEGEN_X86)
    uint8_t* base = memory->buffer().dataPointerEither().unwrap();
    for (const MemoryAccess& access : metadata.memoryAccesses) {
        // Patch memory pointer immediate.
        void* addr = access.patchMemoryPtrImmAt(cs.code());
        uint32_t disp = reinterpret_cast<uint32_t>(X86Encoding::GetPointer(addr));
        MOZ_ASSERT(disp <= INT32_MAX);
        X86Encoding::SetPointer(addr, (void*)(base + disp));
    }
#endif
}

static bool
SendCodeRangesToProfiler(JSContext* cx, CodeSegment& cs, const Bytes& bytecode,
                         const Metadata& metadata)
{
    bool enabled = false;
#ifdef JS_ION_PERF
    enabled |= PerfFuncEnabled();
#endif
#ifdef MOZ_VTUNE
    enabled |= IsVTuneProfilingActive();
#endif
    if (!enabled)
        return true;

    for (const CodeRange& codeRange : metadata.codeRanges) {
        if (!codeRange.isFunction())
            continue;

        uintptr_t start = uintptr_t(cs.code() + codeRange.begin());
        uintptr_t end = uintptr_t(cs.code() + codeRange.end());
        uintptr_t size = end - start;

        TwoByteName name(cx);
        if (!metadata.getFuncName(cx, &bytecode, codeRange.funcIndex(), &name))
            return false;

        UniqueChars chars(
            (char*)JS::LossyTwoByteCharsToNewLatin1CharsZ(cx, name.begin(), name.length()).get());
        if (!chars)
            return false;

        // Avoid "unused" warnings
        (void)start;
        (void)size;

#ifdef JS_ION_PERF
        if (PerfFuncEnabled()) {
            const char* file = metadata.filename.get();
            unsigned line = codeRange.funcLineOrBytecode();
            unsigned column = 0;
            writePerfSpewerAsmJSFunctionMap(start, size, file, line, column, chars.get());
        }
#endif
#ifdef MOZ_VTUNE
        if (IsVTuneProfilingActive()) {
            unsigned method_id = iJIT_GetNewMethodID();
            if (method_id == 0)
                return true;
            iJIT_Method_Load method;
            method.method_id = method_id;
            method.method_name = chars.get();
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

    return true;
}

/* static */ UniqueCodeSegment
CodeSegment::create(JSContext* cx,
                    const Bytes& bytecode,
                    const LinkData& linkData,
                    const Metadata& metadata,
                    HandleWasmMemoryObject memory)
{
    MOZ_ASSERT(bytecode.length() % gc::SystemPageSize() == 0);
    MOZ_ASSERT(linkData.globalDataLength % gc::SystemPageSize() == 0);
    MOZ_ASSERT(linkData.functionCodeLength < bytecode.length());

    auto cs = cx->make_unique<CodeSegment>();
    if (!cs)
        return nullptr;

    cs->bytes_ = AllocateCodeSegment(cx, bytecode.length() + linkData.globalDataLength);
    if (!cs->bytes_)
        return nullptr;

    cs->functionCodeLength_ = linkData.functionCodeLength;
    cs->codeLength_ = bytecode.length();
    cs->globalDataLength_ = linkData.globalDataLength;
    cs->interruptCode_ = cs->code() + linkData.interruptOffset;
    cs->outOfBoundsCode_ = cs->code() + linkData.outOfBoundsOffset;
    cs->unalignedAccessCode_ = cs->code() + linkData.unalignedAccessOffset;
    cs->badIndirectCallCode_ = cs->code() + linkData.badIndirectCallOffset;

    {
        JitContext jcx(CompileRuntime::get(cx->compartment()->runtimeFromAnyThread()));
        AutoFlushICache afc("CodeSegment::create");
        AutoFlushICache::setRange(uintptr_t(cs->code()), cs->codeLength());

        memcpy(cs->code(), bytecode.begin(), bytecode.length());
        StaticallyLink(*cs, linkData, cx);
        if (memory)
            SpecializeToMemory(*cs, metadata, memory);
    }

    if (!ExecutableAllocator::makeExecutable(cs->code(), cs->codeLength())) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    if (!SendCodeRangesToProfiler(cx, *cs, bytecode, metadata))
        return nullptr;

    return cs;
}

CodeSegment::~CodeSegment()
{
    if (!bytes_)
        return;

    MOZ_ASSERT(wasmCodeAllocations > 0);
    wasmCodeAllocations--;

    MOZ_ASSERT(totalLength() > 0);
    DeallocateExecutableMemory(bytes_, totalLength(), gc::SystemPageSize());
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
    MOZ_ASSERT(kind_ == Entry || kind_ == Inline || kind_ == CallThunk);
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
    MOZ_ASSERT(kind_ == ImportJitExit || kind_ == ImportInterpExit);
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
    MOZ_ASSERT(funcBeginToTableEntry_ == offsets.tableEntry - begin_);
    MOZ_ASSERT(funcBeginToTableProfilingJump_ == offsets.tableProfilingJump - begin_);
    MOZ_ASSERT(funcBeginToNonProfilingEntry_ == offsets.nonProfilingEntry - begin_);
    MOZ_ASSERT(funcProfilingJumpToProfilingReturn_ == profilingReturn_ - offsets.profilingJump);
    MOZ_ASSERT(funcProfilingEpilogueToProfilingReturn_ == profilingReturn_ - offsets.profilingEpilogue);
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
           SerializedPodVectorSize(boundsChecks) +
           SerializedPodVectorSize(codeRanges) +
           SerializedPodVectorSize(callSites) +
           SerializedPodVectorSize(callThunks) +
           SerializedPodVectorSize(funcNames) +
           filename.serializedSize() +
           assumptions.serializedSize();
}

uint8_t*
Metadata::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &pod(), sizeof(pod()));
    cursor = SerializeVector(cursor, funcImports);
    cursor = SerializeVector(cursor, funcExports);
    cursor = SerializeVector(cursor, sigIds);
    cursor = SerializePodVector(cursor, globals);
    cursor = SerializePodVector(cursor, tables);
    cursor = SerializePodVector(cursor, memoryAccesses);
    cursor = SerializePodVector(cursor, boundsChecks);
    cursor = SerializePodVector(cursor, codeRanges);
    cursor = SerializePodVector(cursor, callSites);
    cursor = SerializePodVector(cursor, callThunks);
    cursor = SerializePodVector(cursor, funcNames);
    cursor = filename.serialize(cursor);
    cursor = assumptions.serialize(cursor);
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
    (cursor = DeserializePodVector(cursor, &boundsChecks)) &&
    (cursor = DeserializePodVector(cursor, &codeRanges)) &&
    (cursor = DeserializePodVector(cursor, &callSites)) &&
    (cursor = DeserializePodVector(cursor, &callThunks)) &&
    (cursor = DeserializePodVector(cursor, &funcNames)) &&
    (cursor = filename.deserialize(cursor)) &&
    (cursor = assumptions.deserialize(cursor));
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
           boundsChecks.sizeOfExcludingThis(mallocSizeOf) +
           codeRanges.sizeOfExcludingThis(mallocSizeOf) +
           callSites.sizeOfExcludingThis(mallocSizeOf) +
           callThunks.sizeOfExcludingThis(mallocSizeOf) +
           funcNames.sizeOfExcludingThis(mallocSizeOf) +
           filename.sizeOfExcludingThis(mallocSizeOf) +
           assumptions.sizeOfExcludingThis(mallocSizeOf);
}

struct ProjectFuncIndex
{
    const FuncExportVector& funcExports;
    explicit ProjectFuncIndex(const FuncExportVector& funcExports) : funcExports(funcExports) {}
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
Metadata::getFuncName(JSContext* cx, const Bytes* maybeBytecode, uint32_t funcIndex,
                      TwoByteName* name) const
{
    if (funcIndex < funcNames.length()) {
        MOZ_ASSERT(maybeBytecode, "NameInBytecode requires preserved bytecode");

        const NameInBytecode& n = funcNames[funcIndex];
        MOZ_ASSERT(n.offset + n.length < maybeBytecode->length());

        if (n.length == 0)
            goto invalid;

        UTF8Chars utf8((const char*)maybeBytecode->begin() + n.offset, n.length);

        // This code could be optimized by having JS::UTF8CharsToNewTwoByteCharsZ
        // return a Vector directly.
        size_t twoByteLength;
        UniqueTwoByteChars chars(JS::UTF8CharsToNewTwoByteCharsZ(cx, utf8, &twoByteLength).get());
        if (!chars)
            goto invalid;

        if (!name->growByUninitialized(twoByteLength))
            return false;

        PodCopy(name->begin(), chars.get(), twoByteLength);
        return true;
    }

  invalid:

    // For names that are out of range or invalid, synthesize a name.

    UniqueChars chars(JS_smprintf("wasm-function[%u]", funcIndex));
    if (!chars) {
        ReportOutOfMemory(cx);
        return false;
    }

    if (!name->growByUninitialized(strlen(chars.get())))
        return false;

    CopyAndInflateChars(name->begin(), chars.get(), name->length());
    return true;
}
