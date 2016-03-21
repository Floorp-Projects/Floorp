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

#include "asmjs/WasmModule.h"

#include "mozilla/Atomics.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/PodOperations.h"

#include "jsprf.h"

#include "asmjs/WasmBinaryToText.h"
#include "asmjs/WasmSerialize.h"
#include "builtin/AtomicsObject.h"
#include "builtin/SIMD.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/BaselineJIT.h"
#include "jit/ExecutableAllocator.h"
#include "jit/JitCommon.h"
#include "js/MemoryMetrics.h"
#include "vm/StringBuffer.h"
#ifdef MOZ_VTUNE
# include "vtune/VTuneWrapper.h"
#endif

#include "jsobjinlines.h"

#include "jit/MacroAssembler-inl.h"
#include "vm/ArrayBufferObject-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::Atomic;
using mozilla::BinarySearch;
using mozilla::MakeEnumeratedRange;
using mozilla::PodCopy;
using mozilla::PodZero;
using mozilla::Swap;
using JS::GenericNaN;

// Limit the number of concurrent wasm code allocations per process. Note that
// on Linux, the real maximum is ~32k, as each module requires 2 maps (RW/RX),
// and the kernel's default max_map_count is ~65k.
static Atomic<uint32_t> wasmCodeAllocations(0);
static const uint32_t MaxWasmCodeAllocations = 16384;

UniqueCodePtr
wasm::AllocateCode(ExclusiveContext* cx, size_t bytes)
{
    // Allocate RW memory. DynamicallyLinkModule will reprotect the code as RX.
    unsigned permissions =
        ExecutableAllocator::initialProtectionFlags(ExecutableAllocator::Writable);

    void* p = nullptr;
    if (wasmCodeAllocations++ < MaxWasmCodeAllocations)
        p = AllocateExecutableMemory(nullptr, bytes, permissions, "asm-js-code", gc::SystemPageSize());
    if (!p) {
        wasmCodeAllocations--;
        ReportOutOfMemory(cx);
    }

    return UniqueCodePtr((uint8_t*)p, CodeDeleter(bytes));
}

void
CodeDeleter::operator()(uint8_t* p)
{
    MOZ_ASSERT(wasmCodeAllocations > 0);
    wasmCodeAllocations--;

    MOZ_ASSERT(bytes_ != 0);
    DeallocateExecutableMemory(p, bytes_, gc::SystemPageSize());
}

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
// On MIPS, CodeLabels are instruction immediates so InternalLinks only
// patch instruction immediates.
StaticLinkData::InternalLink::InternalLink(Kind kind)
{
    MOZ_ASSERT(kind == CodeLabel || kind == InstructionImmediate);
}

bool
StaticLinkData::InternalLink::isRawPointerPatch()
{
    return false;
}
#else
// On the rest, CodeLabels are raw pointers so InternalLinks only patch
// raw pointers.
StaticLinkData::InternalLink::InternalLink(Kind kind)
{
    MOZ_ASSERT(kind == CodeLabel || kind == RawPointer);
}

bool
StaticLinkData::InternalLink::isRawPointerPatch()
{
    return true;
}
#endif

size_t
StaticLinkData::SymbolicLinkArray::serializedSize() const
{
    size_t size = 0;
    for (const Uint32Vector& offsets : *this)
        size += SerializedPodVectorSize(offsets);
    return size;
}

uint8_t*
StaticLinkData::SymbolicLinkArray::serialize(uint8_t* cursor) const
{
    for (const Uint32Vector& offsets : *this)
        cursor = SerializePodVector(cursor, offsets);
    return cursor;
}

const uint8_t*
StaticLinkData::SymbolicLinkArray::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    for (Uint32Vector& offsets : *this) {
        cursor = DeserializePodVector(cx, cursor, &offsets);
        if (!cursor)
            return nullptr;
    }
    return cursor;
}

bool
StaticLinkData::SymbolicLinkArray::clone(JSContext* cx, SymbolicLinkArray* out) const
{
    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        if (!ClonePodVector(cx, (*this)[imm], &(*out)[imm]))
            return false;
    }
    return true;
}

size_t
StaticLinkData::SymbolicLinkArray::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    size_t size = 0;
    for (const Uint32Vector& offsets : *this)
        size += offsets.sizeOfExcludingThis(mallocSizeOf);
    return size;
}

size_t
StaticLinkData::FuncPtrTable::serializedSize() const
{
    return sizeof(globalDataOffset) +
           SerializedPodVectorSize(elemOffsets);
}

uint8_t*
StaticLinkData::FuncPtrTable::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &globalDataOffset, sizeof(globalDataOffset));
    cursor = SerializePodVector(cursor, elemOffsets);
    return cursor;
}

const uint8_t*
StaticLinkData::FuncPtrTable::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &globalDataOffset, sizeof(globalDataOffset))) &&
    (cursor = DeserializePodVector(cx, cursor, &elemOffsets));
    return cursor;
}

bool
StaticLinkData::FuncPtrTable::clone(JSContext* cx, FuncPtrTable* out) const
{
    out->globalDataOffset = globalDataOffset;
    return ClonePodVector(cx, elemOffsets, &out->elemOffsets);
}

size_t
StaticLinkData::FuncPtrTable::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return elemOffsets.sizeOfExcludingThis(mallocSizeOf);
}

size_t
StaticLinkData::serializedSize() const
{
    return sizeof(pod) +
           SerializedPodVectorSize(internalLinks) +
           symbolicLinks.serializedSize() +
           SerializedVectorSize(funcPtrTables);
}

uint8_t*
StaticLinkData::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    cursor = SerializePodVector(cursor, internalLinks);
    cursor = symbolicLinks.serialize(cursor);
    cursor = SerializeVector(cursor, funcPtrTables);
    return cursor;
}

const uint8_t*
StaticLinkData::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &pod, sizeof(pod))) &&
    (cursor = DeserializePodVector(cx, cursor, &internalLinks)) &&
    (cursor = symbolicLinks.deserialize(cx, cursor)) &&
    (cursor = DeserializeVector(cx, cursor, &funcPtrTables));
    return cursor;
}

bool
StaticLinkData::clone(JSContext* cx, StaticLinkData* out) const
{
    out->pod = pod;
    return ClonePodVector(cx, internalLinks, &out->internalLinks) &&
           symbolicLinks.clone(cx, &out->symbolicLinks) &&
           CloneVector(cx, funcPtrTables, &out->funcPtrTables);
}

size_t
StaticLinkData::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return internalLinks.sizeOfExcludingThis(mallocSizeOf) +
           symbolicLinks.sizeOfExcludingThis(mallocSizeOf) +
           SizeOfVectorExcludingThis(funcPtrTables, mallocSizeOf);
}

static size_t
SerializedSigSize(const Sig& sig)
{
    return sizeof(ExprType) +
           SerializedPodVectorSize(sig.args());
}

static uint8_t*
SerializeSig(uint8_t* cursor, const Sig& sig)
{
    cursor = WriteScalar<ExprType>(cursor, sig.ret());
    cursor = SerializePodVector(cursor, sig.args());
    return cursor;
}

static const uint8_t*
DeserializeSig(ExclusiveContext* cx, const uint8_t* cursor, Sig* sig)
{
    ExprType ret;
    cursor = ReadScalar<ExprType>(cursor, &ret);

    ValTypeVector args;
    cursor = DeserializePodVector(cx, cursor, &args);
    if (!cursor)
        return nullptr;

    *sig = Sig(Move(args), ret);
    return cursor;
}

static size_t
SizeOfSigExcludingThis(const Sig& sig, MallocSizeOf mallocSizeOf)
{
    return sig.args().sizeOfExcludingThis(mallocSizeOf);
}

size_t
Export::serializedSize() const
{
    return SerializedSigSize(sig_) +
           sizeof(pod);
}

uint8_t*
Export::serialize(uint8_t* cursor) const
{
    cursor = SerializeSig(cursor, sig_);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    return cursor;
}

const uint8_t*
Export::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = DeserializeSig(cx, cursor, &sig_)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
    return cursor;
}

bool
Export::clone(JSContext* cx, Export* out) const
{
    out->pod = pod;
    return out->sig_.clone(sig_);
}

size_t
Export::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return SizeOfSigExcludingThis(sig_, mallocSizeOf);
}

size_t
Import::serializedSize() const
{
    return SerializedSigSize(sig_) +
           sizeof(pod);
}

uint8_t*
Import::serialize(uint8_t* cursor) const
{
    cursor = SerializeSig(cursor, sig_);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    return cursor;
}

const uint8_t*
Import::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = DeserializeSig(cx, cursor, &sig_)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
    return cursor;
}

bool
Import::clone(JSContext* cx, Import* out) const
{
    out->pod = pod;
    return out->sig_.clone(sig_);
}

size_t
Import::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return SizeOfSigExcludingThis(sig_, mallocSizeOf);
}

CodeRange::CodeRange(Kind kind, Offsets offsets)
  : funcIndex_(0),
    funcLineOrBytecode_(0),
    begin_(offsets.begin),
    profilingReturn_(0),
    end_(offsets.end)
{
    PodZero(&u);  // zero padding for Valgrind
    u.kind_ = kind;

    MOZ_ASSERT(begin_ <= end_);
    MOZ_ASSERT(u.kind_ == Entry || u.kind_ == Inline || u.kind_ == CallThunk);
}

CodeRange::CodeRange(Kind kind, ProfilingOffsets offsets)
  : funcIndex_(0),
    funcLineOrBytecode_(0),
    begin_(offsets.begin),
    profilingReturn_(offsets.profilingReturn),
    end_(offsets.end)
{
    PodZero(&u);  // zero padding for Valgrind
    u.kind_ = kind;

    MOZ_ASSERT(begin_ < profilingReturn_);
    MOZ_ASSERT(profilingReturn_ < end_);
    MOZ_ASSERT(u.kind_ == ImportJitExit || u.kind_ == ImportInterpExit || u.kind_ == ErrorExit);
}

CodeRange::CodeRange(uint32_t funcIndex, uint32_t funcLineOrBytecode, FuncOffsets offsets)
  : funcIndex_(funcIndex),
    funcLineOrBytecode_(funcLineOrBytecode)
{
    PodZero(&u);  // zero padding for Valgrind
    u.kind_ = Function;

    MOZ_ASSERT(offsets.nonProfilingEntry - offsets.begin <= UINT8_MAX);
    begin_ = offsets.begin;
    u.func.beginToEntry_ = offsets.nonProfilingEntry - begin_;

    MOZ_ASSERT(offsets.nonProfilingEntry < offsets.profilingReturn);
    MOZ_ASSERT(offsets.profilingReturn - offsets.profilingJump <= UINT8_MAX);
    MOZ_ASSERT(offsets.profilingReturn - offsets.profilingEpilogue <= UINT8_MAX);
    profilingReturn_ = offsets.profilingReturn;
    u.func.profilingJumpToProfilingReturn_ = profilingReturn_ - offsets.profilingJump;
    u.func.profilingEpilogueToProfilingReturn_ = profilingReturn_ - offsets.profilingEpilogue;

    MOZ_ASSERT(offsets.nonProfilingEntry < offsets.end);
    end_ = offsets.end;
}

static size_t
NullableStringLength(const char* chars)
{
    return chars ? strlen(chars) : 0;
}

size_t
CacheableChars::serializedSize() const
{
    return sizeof(uint32_t) + NullableStringLength(get());
}

uint8_t*
CacheableChars::serialize(uint8_t* cursor) const
{
    uint32_t length = NullableStringLength(get());
    cursor = WriteBytes(cursor, &length, sizeof(uint32_t));
    cursor = WriteBytes(cursor, get(), length);
    return cursor;
}

const uint8_t*
CacheableChars::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    uint32_t length;
    cursor = ReadBytes(cursor, &length, sizeof(uint32_t));

    reset(cx->pod_calloc<char>(length + 1));
    if (!get())
        return nullptr;

    cursor = ReadBytes(cursor, get(), length);
    return cursor;
}

bool
CacheableChars::clone(JSContext* cx, CacheableChars* out) const
{
    uint32_t length = NullableStringLength(get());

    UniqueChars chars(cx->pod_calloc<char>(length + 1));
    if (!chars)
        return false;

    PodCopy(chars.get(), get(), length);

    *out = Move(chars);
    return true;
}

size_t
CacheableChars::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(get());
}

size_t
ExportMap::serializedSize() const
{
    return SerializedVectorSize(fieldNames) +
           SerializedPodVectorSize(fieldsToExports) +
           SerializedPodVectorSize(exportFuncIndices);
}

uint8_t*
ExportMap::serialize(uint8_t* cursor) const
{
    cursor = SerializeVector(cursor, fieldNames);
    cursor = SerializePodVector(cursor, fieldsToExports);
    cursor = SerializePodVector(cursor, exportFuncIndices);
    return cursor;
}

const uint8_t*
ExportMap::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = DeserializeVector(cx, cursor, &fieldNames)) &&
    (cursor = DeserializePodVector(cx, cursor, &fieldsToExports)) &&
    (cursor = DeserializePodVector(cx, cursor, &exportFuncIndices));
    return cursor;
}

bool
ExportMap::clone(JSContext* cx, ExportMap* map) const
{
    return CloneVector(cx, fieldNames, &map->fieldNames) &&
           ClonePodVector(cx, fieldsToExports, &map->fieldsToExports) &&
           ClonePodVector(cx, exportFuncIndices, &map->exportFuncIndices);
}

size_t
ExportMap::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return SizeOfVectorExcludingThis(fieldNames, mallocSizeOf) &&
           fieldsToExports.sizeOfExcludingThis(mallocSizeOf) &&
           exportFuncIndices.sizeOfExcludingThis(mallocSizeOf);
}

size_t
ModuleData::serializedSize() const
{
    return sizeof(pod()) +
           codeBytes +
           SerializedVectorSize(imports) +
           SerializedVectorSize(exports) +
           SerializedPodVectorSize(heapAccesses) +
           SerializedPodVectorSize(codeRanges) +
           SerializedPodVectorSize(callSites) +
           SerializedPodVectorSize(callThunks) +
           SerializedVectorSize(prettyFuncNames) +
           filename.serializedSize();
}

uint8_t*
ModuleData::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &pod(), sizeof(pod()));
    cursor = WriteBytes(cursor, code.get(), codeBytes);
    cursor = SerializeVector(cursor, imports);
    cursor = SerializeVector(cursor, exports);
    cursor = SerializePodVector(cursor, heapAccesses);
    cursor = SerializePodVector(cursor, codeRanges);
    cursor = SerializePodVector(cursor, callSites);
    cursor = SerializePodVector(cursor, callThunks);
    cursor = SerializeVector(cursor, prettyFuncNames);
    cursor = filename.serialize(cursor);
    return cursor;
}

/* static */ const uint8_t*
ModuleData::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    cursor = ReadBytes(cursor, &pod(), sizeof(pod()));

    code = AllocateCode(cx, totalBytes());
    if (!code)
        return nullptr;
    cursor = ReadBytes(cursor, code.get(), codeBytes);

    (cursor = DeserializeVector(cx, cursor, &imports)) &&
    (cursor = DeserializeVector(cx, cursor, &exports)) &&
    (cursor = DeserializePodVector(cx, cursor, &heapAccesses)) &&
    (cursor = DeserializePodVector(cx, cursor, &codeRanges)) &&
    (cursor = DeserializePodVector(cx, cursor, &callSites)) &&
    (cursor = DeserializePodVector(cx, cursor, &callThunks)) &&
    (cursor = DeserializeVector(cx, cursor, &prettyFuncNames)) &&
    (cursor = filename.deserialize(cx, cursor));
    return cursor;
}

bool
ModuleData::clone(JSContext* cx, ModuleData* out) const
{
    out->pod() = pod();

    out->code = AllocateCode(cx, totalBytes());
    if (!out->code)
        return false;
    memcpy(out->code.get(), code.get(), codeBytes);

    return CloneVector(cx, imports, &out->imports) &&
           CloneVector(cx, exports, &out->exports) &&
           ClonePodVector(cx, heapAccesses, &out->heapAccesses) &&
           ClonePodVector(cx, codeRanges, &out->codeRanges) &&
           ClonePodVector(cx, callSites, &out->callSites) &&
           ClonePodVector(cx, callThunks, &out->callThunks) &&
           CloneVector(cx, prettyFuncNames, &out->prettyFuncNames) &&
           filename.clone(cx, &out->filename);
}

size_t
ModuleData::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    // Module::addSizeOfMisc takes care of code and global memory.
    return SizeOfVectorExcludingThis(imports, mallocSizeOf) +
           SizeOfVectorExcludingThis(exports, mallocSizeOf) +
           heapAccesses.sizeOfExcludingThis(mallocSizeOf) +
           codeRanges.sizeOfExcludingThis(mallocSizeOf) +
           callSites.sizeOfExcludingThis(mallocSizeOf) +
           callThunks.sizeOfExcludingThis(mallocSizeOf) +
           prettyFuncNames.sizeOfExcludingThis(mallocSizeOf) +
           filename.sizeOfExcludingThis(mallocSizeOf);
}

uint8_t*
Module::rawHeapPtr() const
{
    return const_cast<Module*>(this)->rawHeapPtr();
}

uint8_t*&
Module::rawHeapPtr()
{
    return *(uint8_t**)(globalData() + HeapGlobalDataOffset);
}

WasmActivation*&
Module::activation()
{
    MOZ_ASSERT(dynamicallyLinked_);
    return *reinterpret_cast<WasmActivation**>(globalData() + ActivationGlobalDataOffset);
}

void
Module::specializeToHeap(ArrayBufferObjectMaybeShared* heap)
{
    MOZ_ASSERT(usesHeap());
    MOZ_ASSERT_IF(heap->is<ArrayBufferObject>(), heap->as<ArrayBufferObject>().isWasm());
    MOZ_ASSERT(!heap_);
    MOZ_ASSERT(!rawHeapPtr());

    uint8_t* ptrBase = heap->dataPointerEither().unwrap(/*safe - protected by Module methods*/);
    uint32_t heapLength = heap->byteLength();
#if defined(JS_CODEGEN_X86)
    // An access is out-of-bounds iff
    //      ptr + offset + data-type-byte-size > heapLength
    // i.e. ptr > heapLength - data-type-byte-size - offset. data-type-byte-size
    // and offset are already included in the addend so we
    // just have to add the heap length here.
    for (const HeapAccess& access : module_->heapAccesses) {
        if (access.hasLengthCheck())
            X86Encoding::AddInt32(access.patchLengthAt(code()), heapLength);
        void* addr = access.patchHeapPtrImmAt(code());
        uint32_t disp = reinterpret_cast<uint32_t>(X86Encoding::GetPointer(addr));
        MOZ_ASSERT(disp <= INT32_MAX);
        X86Encoding::SetPointer(addr, (void*)(ptrBase + disp));
    }
#elif defined(JS_CODEGEN_X64)
    // Even with signal handling being used for most bounds checks, there may be
    // atomic operations that depend on explicit checks.
    //
    // If we have any explicit bounds checks, we need to patch the heap length
    // checks at the right places. All accesses that have been recorded are the
    // only ones that need bound checks (see also
    // CodeGeneratorX64::visitAsmJS{Load,Store,CompareExchange,Exchange,AtomicBinop}Heap)
    for (const HeapAccess& access : module_->heapAccesses) {
        // See comment above for x86 codegen.
        if (access.hasLengthCheck())
            X86Encoding::AddInt32(access.patchLengthAt(code()), heapLength);
    }
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
      defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    for (const HeapAccess& access : module_->heapAccesses)
        Assembler::UpdateBoundsCheck(heapLength, (Instruction*)(access.insnOffset() + code()));
#endif

    heap_ = heap;
    rawHeapPtr() = ptrBase;
}

void
Module::despecializeFromHeap(ArrayBufferObjectMaybeShared* heap)
{
    // heap_/rawHeapPtr can be null if this module holds cloned code from
    // another dynamically-linked module which we are despecializing from that
    // module's heap.
    MOZ_ASSERT_IF(heap_, heap_ == heap);
    MOZ_ASSERT_IF(rawHeapPtr(), rawHeapPtr() == heap->dataPointerEither().unwrap());

#if defined(JS_CODEGEN_X86)
    uint32_t heapLength = heap->byteLength();
    uint8_t* ptrBase = heap->dataPointerEither().unwrap(/*safe - used for value*/);
    for (unsigned i = 0; i < module_->heapAccesses.length(); i++) {
        const HeapAccess& access = module_->heapAccesses[i];
        if (access.hasLengthCheck())
            X86Encoding::AddInt32(access.patchLengthAt(code()), -heapLength);
        void* addr = access.patchHeapPtrImmAt(code());
        uint8_t* ptr = reinterpret_cast<uint8_t*>(X86Encoding::GetPointer(addr));
        MOZ_ASSERT(ptr >= ptrBase);
        X86Encoding::SetPointer(addr, reinterpret_cast<void*>(ptr - ptrBase));
    }
#elif defined(JS_CODEGEN_X64)
    uint32_t heapLength = heap->byteLength();
    for (unsigned i = 0; i < module_->heapAccesses.length(); i++) {
        const HeapAccess& access = module_->heapAccesses[i];
        if (access.hasLengthCheck())
            X86Encoding::AddInt32(access.patchLengthAt(code()), -heapLength);
    }
#endif

    heap_ = nullptr;
    rawHeapPtr() = nullptr;
}

bool
Module::sendCodeRangesToProfiler(JSContext* cx)
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

    for (const CodeRange& codeRange : module_->codeRanges) {
        if (!codeRange.isFunction())
            continue;

        uintptr_t start = uintptr_t(code() + codeRange.begin());
        uintptr_t end = uintptr_t(code() + codeRange.end());
        uintptr_t size = end - start;

        UniqueChars owner;
        const char* name = getFuncName(cx, codeRange.funcIndex(), &owner);
        if (!name)
            return false;

        // Avoid "unused" warnings
        (void)start;
        (void)size;
        (void)name;

#ifdef JS_ION_PERF
        if (PerfFuncEnabled()) {
            const char* file = module_->filename.get();
            unsigned line = codeRange.funcLineOrBytecode();
            unsigned column = 0;
            writePerfSpewerAsmJSFunctionMap(start, size, file, line, column, name);
        }
#endif
#ifdef MOZ_VTUNE
        if (IsVTuneProfilingActive()) {
            unsigned method_id = iJIT_GetNewMethodID();
            if (method_id == 0)
                return true;
            iJIT_Method_Load method;
            method.method_id = method_id;
            method.method_name = const_cast<char*>(name);
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

bool
Module::setProfilingEnabled(JSContext* cx, bool enabled)
{
    MOZ_ASSERT(dynamicallyLinked_);
    MOZ_ASSERT(!activation());

    if (profilingEnabled_ == enabled)
        return true;

    // When enabled, generate profiling labels for every name in funcNames_
    // that is the name of some Function CodeRange. This involves malloc() so
    // do it now since, once we start sampling, we'll be in a signal-handing
    // context where we cannot malloc.
    if (enabled) {
        if (!funcLabels_.resize(module_->numFuncs)) {
            ReportOutOfMemory(cx);
            return false;
        }
        for (const CodeRange& codeRange : module_->codeRanges) {
            if (!codeRange.isFunction())
                continue;

            UniqueChars owner;
            const char* funcName = getFuncName(cx, codeRange.funcIndex(), &owner);
            if (!funcName)
                return false;

            UniqueChars label(JS_smprintf("%s (%s:%u)",
                                          funcName,
                                          module_->filename.get(),
                                          codeRange.funcLineOrBytecode()));
            if (!label) {
                ReportOutOfMemory(cx);
                return false;
            }

            funcLabels_[codeRange.funcIndex()] = Move(label);
        }
    } else {
        funcLabels_.clear();
    }

    // Patch callsites and returns to execute profiling prologues/epilogues.
    {
        AutoWritableJitCode awjc(cx->runtime(), code(), codeBytes());
        AutoFlushICache afc("Module::setProfilingEnabled");
        AutoFlushICache::setRange(uintptr_t(code()), codeBytes());

        for (const CallSite& callSite : module_->callSites)
            EnableProfilingPrologue(*this, callSite, enabled);

        for (const CallThunk& callThunk : module_->callThunks)
            EnableProfilingThunk(*this, callThunk, enabled);

        for (const CodeRange& codeRange : module_->codeRanges)
            EnableProfilingEpilogue(*this, codeRange, enabled);
    }

    // Update the function-pointer tables to point to profiling prologues.
    for (FuncPtrTable& table : funcPtrTables_) {
        auto array = reinterpret_cast<void**>(globalData() + table.globalDataOffset);
        for (size_t i = 0; i < table.numElems; i++) {
            const CodeRange* codeRange = lookupCodeRange(array[i]);
            // Don't update entries for the BadIndirectCall exit.
            if (codeRange->isErrorExit())
                continue;
            void* from = code() + codeRange->funcNonProfilingEntry();
            void* to = code() + codeRange->funcProfilingEntry();
            if (!enabled)
                Swap(from, to);
            MOZ_ASSERT(array[i] == from);
            array[i] = to;
        }
    }

    profilingEnabled_ = enabled;
    return true;
}

Module::ImportExit&
Module::importToExit(const Import& import)
{
    return *reinterpret_cast<ImportExit*>(globalData() + import.exitGlobalDataOffset());
}

bool
Module::clone(JSContext* cx, const StaticLinkData& link, Module* out) const
{
    MOZ_ASSERT(dynamicallyLinked_);

    // The out->module_ field was already cloned and initialized when 'out' was
    // constructed. This function should clone the rest.
    MOZ_ASSERT(out->module_);

    out->profilingEnabled_ = profilingEnabled_;

    if (!CloneVector(cx, funcLabels_, &out->funcLabels_))
        return false;

#ifdef DEBUG
    // Put the symbolic links back to -1 so PatchDataWithValueCheck assertions
    // in Module::staticallyLink are valid.
    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        void* callee = AddressOf(imm, cx);
        const Uint32Vector& offsets = link.symbolicLinks[imm];
        for (uint32_t offset : offsets) {
            jit::Assembler::PatchDataWithValueCheck(jit::CodeLocationLabel(out->code() + offset),
                                                    jit::PatchedImmPtr((void*)-1),
                                                    jit::PatchedImmPtr(callee));
        }
    }
#endif

    // If the copied machine code has been specialized to the heap, it must be
    // unspecialized in the copy.
    if (usesHeap())
        out->despecializeFromHeap(heap_);

    return true;
}


Module::Module(UniqueModuleData module)
  : module_(Move(module)),
    staticallyLinked_(false),
    interrupt_(nullptr),
    outOfBounds_(nullptr),
    dynamicallyLinked_(false),
    profilingEnabled_(false)
{
    *(double*)(globalData() + NaN64GlobalDataOffset) = GenericNaN();
    *(float*)(globalData() + NaN32GlobalDataOffset) = GenericNaN();

#ifdef DEBUG
    uint32_t lastEnd = 0;
    for (const CodeRange& cr : module_->codeRanges) {
        MOZ_ASSERT(cr.begin() >= lastEnd);
        lastEnd = cr.end();
    }
#endif
}

Module::~Module()
{
    for (unsigned i = 0; i < imports().length(); i++) {
        ImportExit& exit = importToExit(imports()[i]);
        if (exit.baselineScript)
            exit.baselineScript->removeDependentWasmModule(*this, i);
    }
}

/* virtual */ void
Module::trace(JSTracer* trc)
{
    for (const Import& import : imports())
        TraceNullableEdge(trc, &importToExit(import).fun, "wasm function import");

    TraceNullableEdge(trc, &heap_, "wasm buffer");

    MOZ_ASSERT(ownerObject_);
    TraceEdge(trc, &ownerObject_, "wasm owner object");
}

/* virtual */ void
Module::readBarrier()
{
    InternalBarrierMethods<JSObject*>::readBarrier(owner());
}

/* virtual */ void
Module::addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code, size_t* data)
{
    *code += codeBytes();
    *data += mallocSizeOf(this) +
             globalBytes() +
             mallocSizeOf(module_.get()) +
             module_->sizeOfExcludingThis(mallocSizeOf) +
             source_.sizeOfExcludingThis(mallocSizeOf) +
             funcPtrTables_.sizeOfExcludingThis(mallocSizeOf) +
             SizeOfVectorExcludingThis(funcLabels_, mallocSizeOf);
}

/* virtual */ bool
Module::mutedErrors() const
{
    // WebAssembly code is always CORS-same-origin and so errors are never
    // muted. For asm.js, muting depends on the ScriptSource containing the
    // asm.js so this function is overridden by AsmJSModule.
    return false;
}

/* virtual */ const char16_t*
Module::displayURL() const
{
    // WebAssembly code does not have `//# sourceURL`.
    return nullptr;
}

bool
Module::containsFunctionPC(void* pc) const
{
    return pc >= code() && pc < (code() + module_->functionBytes);
}

bool
Module::containsCodePC(void* pc) const
{
    return pc >= code() && pc < (code() + codeBytes());
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
Module::lookupCallSite(void* returnAddress) const
{
    uint32_t target = ((uint8_t*)returnAddress) - code();
    size_t lowerBound = 0;
    size_t upperBound = module_->callSites.length();

    size_t match;
    if (!BinarySearch(CallSiteRetAddrOffset(module_->callSites), lowerBound, upperBound, target, &match))
        return nullptr;

    return &module_->callSites[match];
}

const CodeRange*
Module::lookupCodeRange(void* pc) const
{
    CodeRange::PC target((uint8_t*)pc - code());
    size_t lowerBound = 0;
    size_t upperBound = module_->codeRanges.length();

    size_t match;
    if (!BinarySearch(module_->codeRanges, lowerBound, upperBound, target, &match))
        return nullptr;

    return &module_->codeRanges[match];
}

struct HeapAccessOffset
{
    const HeapAccessVector& accesses;
    explicit HeapAccessOffset(const HeapAccessVector& accesses) : accesses(accesses) {}
    uintptr_t operator[](size_t index) const {
        return accesses[index].insnOffset();
    }
};

const HeapAccess*
Module::lookupHeapAccess(void* pc) const
{
    MOZ_ASSERT(containsFunctionPC(pc));

    uint32_t target = ((uint8_t*)pc) - code();
    size_t lowerBound = 0;
    size_t upperBound = module_->heapAccesses.length();

    size_t match;
    if (!BinarySearch(HeapAccessOffset(module_->heapAccesses), lowerBound, upperBound, target, &match))
        return nullptr;

    return &module_->heapAccesses[match];
}

bool
Module::staticallyLink(ExclusiveContext* cx, const StaticLinkData& linkData)
{
    MOZ_ASSERT(!dynamicallyLinked_);
    MOZ_ASSERT(!staticallyLinked_);
    staticallyLinked_ = true;

    // Push a JitContext for benefit of IsCompilingAsmJS and delay flushing
    // until Module::dynamicallyLink.
    JitContext jcx(CompileRuntime::get(cx->compartment()->runtimeFromAnyThread()));
    MOZ_ASSERT(IsCompilingAsmJS());
    AutoFlushICache afc("Module::staticallyLink", /* inhibit = */ true);
    AutoFlushICache::setRange(uintptr_t(code()), codeBytes());

    interrupt_ = code() + linkData.pod.interruptOffset;
    outOfBounds_ = code() + linkData.pod.outOfBoundsOffset;

    for (StaticLinkData::InternalLink link : linkData.internalLinks) {
        uint8_t* patchAt = code() + link.patchAtOffset;
        void* target = code() + link.targetOffset;

        // If the target of an InternalLink is the non-profiling entry of a
        // function, then we assume it is for a call that wants to call the
        // profiling entry when profiling is enabled. Note that the target may
        // be in the middle of a function (e.g., for a switch table) and in
        // these cases we should not modify the target.
        if (profilingEnabled_) {
            if (const CodeRange* cr = lookupCodeRange(target)) {
                if (cr->isFunction() && link.targetOffset == cr->funcNonProfilingEntry())
                    target = code() + cr->funcProfilingEntry();
            }
        }

        if (link.isRawPointerPatch())
            *(void**)(patchAt) = target;
        else
            Assembler::PatchInstructionImmediate(patchAt, PatchedImmPtr(target));
    }

    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        const Uint32Vector& offsets = linkData.symbolicLinks[imm];
        for (size_t i = 0; i < offsets.length(); i++) {
            uint8_t* patchAt = code() + offsets[i];
            void* target = AddressOf(imm, cx);
            Assembler::PatchDataWithValueCheck(CodeLocationLabel(patchAt),
                                               PatchedImmPtr(target),
                                               PatchedImmPtr((void*)-1));
        }
    }

    for (const StaticLinkData::FuncPtrTable& table : linkData.funcPtrTables) {
        auto array = reinterpret_cast<void**>(globalData() + table.globalDataOffset);
        for (size_t i = 0; i < table.elemOffsets.length(); i++) {
            uint8_t* elem = code() + table.elemOffsets[i];
            const CodeRange* codeRange = lookupCodeRange(elem);
            if (profilingEnabled_ && !codeRange->isErrorExit())
                elem = code() + codeRange->funcProfilingEntry();
            array[i] = elem;
        }
    }

    // CodeRangeVector, CallSiteVector and the code technically have all the
    // necessary info to do all the updates necessary in setProfilingEnabled.
    // However, to simplify the finding of function-pointer table sizes and
    // global-data offsets, save just that information here.

    if (!funcPtrTables_.appendAll(linkData.funcPtrTables)) {
        ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

static bool
WasmCall(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedFunction callee(cx, &args.callee().as<JSFunction>());

    Module& module = ExportedFunctionToModuleObject(callee)->module();
    uint32_t exportIndex = ExportedFunctionToIndex(callee);

    return module.callExport(cx, exportIndex, args);
}

static JSFunction*
NewExportedFunction(JSContext* cx, Handle<WasmModuleObject*> moduleObj, const ExportMap& exportMap,
                    uint32_t exportIndex)
{
    Module& module = moduleObj->module();
    unsigned numArgs = module.exports()[exportIndex].sig().args().length();

    RootedAtom name(cx, module.getFuncAtom(cx, exportMap.exportFuncIndices[exportIndex]));
    if (!name)
        return nullptr;

    JSFunction* fun = NewNativeConstructor(cx, WasmCall, numArgs, name,
                                           gc::AllocKind::FUNCTION_EXTENDED, GenericObject,
                                           JSFunction::ASMJS_CTOR);
    if (!fun)
        return nullptr;

    fun->setExtendedSlot(FunctionExtended::WASM_MODULE_SLOT, ObjectValue(*moduleObj));
    fun->setExtendedSlot(FunctionExtended::WASM_EXPORT_INDEX_SLOT, Int32Value(exportIndex));
    return fun;
}

static bool
CreateExportObject(JSContext* cx,
                   Handle<WasmModuleObject*> moduleObj,
                   Handle<ArrayBufferObjectMaybeShared*> heap,
                   const ExportMap& exportMap,
                   const ExportVector& exports,
                   MutableHandleObject exportObj)
{
    MOZ_ASSERT(exportMap.exportFuncIndices.length() == exports.length());
    MOZ_ASSERT(exportMap.fieldNames.length() == exportMap.fieldsToExports.length());

    for (size_t fieldIndex = 0; fieldIndex < exportMap.fieldNames.length(); fieldIndex++) {
        const char* fieldName = exportMap.fieldNames[fieldIndex].get();
        if (!*fieldName) {
            MOZ_ASSERT(!exportObj);
            uint32_t exportIndex = exportMap.fieldsToExports[fieldIndex];
            if (exportIndex == MemoryExport) {
                MOZ_ASSERT(heap);
                exportObj.set(heap);
            } else {
                exportObj.set(NewExportedFunction(cx, moduleObj, exportMap, exportIndex));
                if (!exportObj)
                    return false;
            }
            break;
        }
    }

    Rooted<ValueVector> vals(cx, ValueVector(cx));
    for (size_t exportIndex = 0; exportIndex < exports.length(); exportIndex++) {
        JSFunction* fun = NewExportedFunction(cx, moduleObj, exportMap, exportIndex);
        if (!fun || !vals.append(ObjectValue(*fun)))
            return false;
    }

    if (!exportObj) {
        exportObj.set(JS_NewPlainObject(cx));
        if (!exportObj)
            return false;
    }

    for (size_t fieldIndex = 0; fieldIndex < exportMap.fieldNames.length(); fieldIndex++) {
        const char* fieldName = exportMap.fieldNames[fieldIndex].get();
        if (!*fieldName)
            continue;

        JSAtom* atom = AtomizeUTF8Chars(cx, fieldName, strlen(fieldName));
        if (!atom)
            return false;

        RootedId id(cx, AtomToId(atom));
        RootedValue val(cx);
        uint32_t exportIndex = exportMap.fieldsToExports[fieldIndex];
        if (exportIndex == MemoryExport)
            val = ObjectValue(*heap);
        else
            val = vals[exportIndex];

        if (!JS_DefinePropertyById(cx, exportObj, id, val, JSPROP_ENUMERATE))
            return false;
    }

    return true;
}

bool
Module::dynamicallyLink(JSContext* cx,
                        Handle<WasmModuleObject*> moduleObj,
                        Handle<ArrayBufferObjectMaybeShared*> heap,
                        Handle<FunctionVector> importArgs,
                        const ExportMap& exportMap,
                        MutableHandleObject exportObj)
{
    MOZ_ASSERT(this == &moduleObj->module());
    MOZ_ASSERT(staticallyLinked_);
    MOZ_ASSERT(!dynamicallyLinked_);
    dynamicallyLinked_ = true;

    // Push a JitContext for benefit of IsCompilingAsmJS and flush the ICache.
    // We've been inhibiting flushing up to this point so flush it all now.
    JitContext jcx(CompileRuntime::get(cx->compartment()->runtimeFromAnyThread()));
    MOZ_ASSERT(IsCompilingAsmJS());
    AutoFlushICache afc("Module::dynamicallyLink");
    AutoFlushICache::setRange(uintptr_t(code()), codeBytes());

    // Initialize imports with actual imported values.
    MOZ_ASSERT(importArgs.length() == imports().length());
    for (size_t i = 0; i < imports().length(); i++) {
        const Import& import = imports()[i];
        ImportExit& exit = importToExit(import);
        exit.code = code() + import.interpExitCodeOffset();
        exit.fun = importArgs[i];
        exit.baselineScript = nullptr;
    }

    // Specialize code to the actual heap.
    if (usesHeap())
        specializeToHeap(heap);

    // See AllocateCode comment above.
    if (!ExecutableAllocator::makeExecutable(code(), codeBytes())) {
        ReportOutOfMemory(cx);
        return false;
    }

    if (!sendCodeRangesToProfiler(cx))
        return false;

    return CreateExportObject(cx, moduleObj, heap, exportMap, exports(), exportObj);
}

SharedMem<uint8_t*>
Module::heap() const
{
    MOZ_ASSERT(dynamicallyLinked_);
    MOZ_ASSERT(usesHeap());
    MOZ_ASSERT(rawHeapPtr());
    return hasSharedHeap()
           ? SharedMem<uint8_t*>::shared(rawHeapPtr())
           : SharedMem<uint8_t*>::unshared(rawHeapPtr());
}

size_t
Module::heapLength() const
{
    MOZ_ASSERT(dynamicallyLinked_);
    MOZ_ASSERT(usesHeap());
    return heap_->byteLength();
}

void
Module::deoptimizeImportExit(uint32_t importIndex)
{
    MOZ_ASSERT(dynamicallyLinked_);
    const Import& import = imports()[importIndex];
    ImportExit& exit = importToExit(import);
    exit.code = code() + import.interpExitCodeOffset();
    exit.baselineScript = nullptr;
}

bool
Module::callExport(JSContext* cx, uint32_t exportIndex, CallArgs args)
{
    MOZ_ASSERT(dynamicallyLinked_);

    const Export& exp = exports()[exportIndex];

    // Enable/disable profiling in the Module to match the current global
    // profiling state. Don't do this if the Module is already active on the
    // stack since this would leave the Module in a state where profiling is
    // enabled but the stack isn't unwindable.
    if (profilingEnabled() != cx->runtime()->spsProfiler.enabled() && !activation()) {
        if (!setProfilingEnabled(cx, cx->runtime()->spsProfiler.enabled()))
            return false;
    }

    // The calling convention for an external call into wasm is to pass an
    // array of 16-byte values where each value contains either a coerced int32
    // (in the low word), a double value (in the low dword) or a SIMD vector
    // value, with the coercions specified by the wasm signature. The external
    // entry point unpacks this array into the system-ABI-specified registers
    // and stack memory and then calls into the internal entry point. The return
    // value is stored in the first element of the array (which, therefore, must
    // have length >= 1).
    Vector<Module::EntryArg, 8> coercedArgs(cx);
    if (!coercedArgs.resize(Max<size_t>(1, exp.sig().args().length())))
        return false;

    RootedValue v(cx);
    for (unsigned i = 0; i < exp.sig().args().length(); ++i) {
        v = i < args.length() ? args[i] : UndefinedValue();
        switch (exp.sig().arg(i)) {
          case ValType::I32:
            if (!ToInt32(cx, v, (int32_t*)&coercedArgs[i]))
                return false;
            break;
          case ValType::I64:
            MOZ_CRASH("int64");
          case ValType::F32:
            if (!RoundFloat32(cx, v, (float*)&coercedArgs[i]))
                return false;
            break;
          case ValType::F64:
            if (!ToNumber(cx, v, (double*)&coercedArgs[i]))
                return false;
            break;
          case ValType::I32x4: {
            SimdConstant simd;
            if (!ToSimdConstant<Int32x4>(cx, v, &simd))
                return false;
            memcpy(&coercedArgs[i], simd.asInt32x4(), Simd128DataSize);
            break;
          }
          case ValType::F32x4: {
            SimdConstant simd;
            if (!ToSimdConstant<Float32x4>(cx, v, &simd))
                return false;
            memcpy(&coercedArgs[i], simd.asFloat32x4(), Simd128DataSize);
            break;
          }
          case ValType::B32x4: {
            SimdConstant simd;
            if (!ToSimdConstant<Bool32x4>(cx, v, &simd))
                return false;
            // Bool32x4 uses the same representation as Int32x4.
            memcpy(&coercedArgs[i], simd.asInt32x4(), Simd128DataSize);
            break;
          }
          case ValType::Limit:
            MOZ_CRASH("Limit");
        }
    }

    {
        // Push a WasmActivation to describe the wasm frames we're about to push
        // when running this module. Additionally, push a JitActivation so that
        // the optimized wasm-to-Ion FFI call path (which we want to be very
        // fast) can avoid doing so. The JitActivation is marked as inactive so
        // stack iteration will skip over it.
        WasmActivation activation(cx, *this);
        JitActivation jitActivation(cx, /* active */ false);

        // Call the per-exported-function trampoline created by GenerateEntry.
        auto entry = JS_DATA_TO_FUNC_PTR(EntryFuncPtr, code() + exp.stubOffset());
        if (!CALL_GENERATED_2(entry, coercedArgs.begin(), globalData()))
            return false;
    }

    if (args.isConstructing()) {
        // By spec, when a function is called as a constructor and this function
        // returns a primary type, which is the case for all wasm exported
        // functions, the returned value is discarded and an empty object is
        // returned instead.
        PlainObject* obj = NewBuiltinClassInstance<PlainObject>(cx);
        if (!obj)
            return false;
        args.rval().set(ObjectValue(*obj));
        return true;
    }

    JSObject* simdObj;
    switch (exp.sig().ret()) {
      case ExprType::Void:
        args.rval().set(UndefinedValue());
        break;
      case ExprType::I32:
        args.rval().set(Int32Value(*(int32_t*)&coercedArgs[0]));
        break;
      case ExprType::I64:
        MOZ_CRASH("int64");
      case ExprType::F32:
      case ExprType::F64:
        args.rval().set(NumberValue(*(double*)&coercedArgs[0]));
        break;
      case ExprType::I32x4:
        simdObj = CreateSimd<Int32x4>(cx, (int32_t*)&coercedArgs[0]);
        if (!simdObj)
            return false;
        args.rval().set(ObjectValue(*simdObj));
        break;
      case ExprType::F32x4:
        simdObj = CreateSimd<Float32x4>(cx, (float*)&coercedArgs[0]);
        if (!simdObj)
            return false;
        args.rval().set(ObjectValue(*simdObj));
        break;
      case ExprType::B32x4:
        simdObj = CreateSimd<Bool32x4>(cx, (int32_t*)&coercedArgs[0]);
        if (!simdObj)
            return false;
        args.rval().set(ObjectValue(*simdObj));
        break;
      case ExprType::Limit:
        MOZ_CRASH("Limit");
    }

    return true;
}

bool
Module::callImport(JSContext* cx, uint32_t importIndex, unsigned argc, const Value* argv,
                   MutableHandleValue rval)
{
    MOZ_ASSERT(dynamicallyLinked_);

    const Import& import = imports()[importIndex];

    InvokeArgs args(cx);
    if (!args.init(argc))
        return false;

    for (size_t i = 0; i < argc; i++)
        args[i].set(argv[i]);

    RootedValue fval(cx, ObjectValue(*importToExit(import).fun));
    RootedValue thisv(cx, UndefinedValue());
    if (!Call(cx, fval, thisv, args, rval))
        return false;

    ImportExit& exit = importToExit(import);

    // The exit may already have become optimized.
    void* jitExitCode = code() + import.jitExitCodeOffset();
    if (exit.code == jitExitCode)
        return true;

    // Test if the function is JIT compiled.
    if (!exit.fun->hasScript())
        return true;
    JSScript* script = exit.fun->nonLazyScript();
    if (!script->hasBaselineScript()) {
        MOZ_ASSERT(!script->hasIonScript());
        return true;
    }

    // Don't enable jit entry when we have a pending ion builder.
    // Take the interpreter path which will link it and enable
    // the fast path on the next call.
    if (script->baselineScript()->hasPendingIonBuilder())
        return true;

    // Currently we can't rectify arguments. Therefore disable if argc is too low.
    if (exit.fun->nargs() > import.sig().args().length())
        return true;

    // Ensure the argument types are included in the argument TypeSets stored in
    // the TypeScript. This is necessary for Ion, because the import exit will
    // use the skip-arg-checks entry point.
    //
    // Note that the TypeScript is never discarded while the script has a
    // BaselineScript, so if those checks hold now they must hold at least until
    // the BaselineScript is discarded and when that happens the import exit is
    // patched back.
    if (!TypeScript::ThisTypes(script)->hasType(TypeSet::UndefinedType()))
        return true;
    for (uint32_t i = 0; i < exit.fun->nargs(); i++) {
        TypeSet::Type type = TypeSet::UnknownType();
        switch (import.sig().args()[i]) {
          case ValType::I32:   type = TypeSet::Int32Type(); break;
          case ValType::I64:   MOZ_CRASH("NYI");
          case ValType::F32:   type = TypeSet::DoubleType(); break;
          case ValType::F64:   type = TypeSet::DoubleType(); break;
          case ValType::I32x4: MOZ_CRASH("NYI");
          case ValType::F32x4: MOZ_CRASH("NYI");
          case ValType::B32x4: MOZ_CRASH("NYI");
          case ValType::Limit: MOZ_CRASH("Limit");
        }
        if (!TypeScript::ArgTypes(script, i)->hasType(type))
            return true;
    }

    // Let's optimize it!
    if (!script->baselineScript()->addDependentWasmModule(cx, *this, importIndex))
        return false;

    exit.code = jitExitCode;
    exit.baselineScript = script->baselineScript();
    return true;
}

const char*
Module::prettyFuncName(uint32_t funcIndex) const
{
    return module_->prettyFuncNames[funcIndex].get();
}

const char*
Module::getFuncName(JSContext* cx, uint32_t funcIndex, UniqueChars* owner) const
{
    if (!module_->prettyFuncNames.empty())
        return prettyFuncName(funcIndex);

    char* chars = JS_smprintf("wasm-function[%u]", funcIndex);
    if (!chars) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    owner->reset(chars);
    return chars;
}

JSAtom*
Module::getFuncAtom(JSContext* cx, uint32_t funcIndex) const
{
    UniqueChars owner;
    const char* chars = getFuncName(cx, funcIndex, &owner);
    if (!chars)
        return nullptr;

    JSAtom* atom = AtomizeUTF8Chars(cx, chars, strlen(chars));
    if (!atom)
        return nullptr;

    return atom;
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

const char enabledMessage[] =
    "Restart with debugger open to view WebAssembly source";

JSString*
Module::createText(JSContext* cx)
{
    StringBuffer buffer(cx);
    if (!source_.empty()) {
        if (!buffer.append(experimentalWarning))
            return nullptr;
        if (!BinaryToText(cx, source_.begin(), source_.length(), buffer))
            return nullptr;
    } else {
        if (!buffer.append(enabledMessage))
            return nullptr;
    }
    return buffer.finishString();
}

const char*
Module::profilingLabel(uint32_t funcIndex) const
{
    MOZ_ASSERT(dynamicallyLinked_);
    MOZ_ASSERT(profilingEnabled_);
    return funcLabels_[funcIndex].get();
}

const ClassOps WasmModuleObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    WasmModuleObject::finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    WasmModuleObject::trace
};

const Class WasmModuleObject::class_ = {
    "WasmModuleObject",
    JSCLASS_IS_ANONYMOUS | JSCLASS_DELAY_METADATA_BUILDER |
    JSCLASS_HAS_RESERVED_SLOTS(WasmModuleObject::RESERVED_SLOTS),
    &WasmModuleObject::classOps_,
};

bool
WasmModuleObject::hasModule() const
{
    MOZ_ASSERT(is<WasmModuleObject>());
    return !getReservedSlot(MODULE_SLOT).isUndefined();
}

/* static */ void
WasmModuleObject::finalize(FreeOp* fop, JSObject* obj)
{
    WasmModuleObject& moduleObj = obj->as<WasmModuleObject>();
    if (moduleObj.hasModule())
        fop->delete_(&moduleObj.module());
}

/* static */ void
WasmModuleObject::trace(JSTracer* trc, JSObject* obj)
{
    WasmModuleObject& moduleObj = obj->as<WasmModuleObject>();
    if (moduleObj.hasModule())
        moduleObj.module().trace(trc);
}

/* static */ WasmModuleObject*
WasmModuleObject::create(ExclusiveContext* cx)
{
    AutoSetNewObjectMetadata metadata(cx);
    JSObject* obj = NewObjectWithGivenProto(cx, &WasmModuleObject::class_, nullptr);
    if (!obj)
        return nullptr;

    return &obj->as<WasmModuleObject>();
}

bool
WasmModuleObject::init(Module* module)
{
    MOZ_ASSERT(is<WasmModuleObject>());
    MOZ_ASSERT(!hasModule());
    if (!module)
        return false;
    module->setOwner(this);
    setReservedSlot(MODULE_SLOT, PrivateValue(module));
    return true;
}

Module&
WasmModuleObject::module() const
{
    MOZ_ASSERT(is<WasmModuleObject>());
    MOZ_ASSERT(hasModule());
    return *(Module*)getReservedSlot(MODULE_SLOT).toPrivate();
}

void
WasmModuleObject::addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code, size_t* data)
{
    if (hasModule())
        module().addSizeOfMisc(mallocSizeOf, code, data);
}

bool
wasm::IsExportedFunction(JSFunction* fun)
{
    return fun->maybeNative() == WasmCall;
}

WasmModuleObject*
wasm::ExportedFunctionToModuleObject(JSFunction* fun)
{
    MOZ_ASSERT(IsExportedFunction(fun));
    const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_MODULE_SLOT);
    return &v.toObject().as<WasmModuleObject>();
}

uint32_t
wasm::ExportedFunctionToIndex(JSFunction* fun)
{
    MOZ_ASSERT(IsExportedFunction(fun));
    const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_EXPORT_INDEX_SLOT);
    return v.toInt32();
}
