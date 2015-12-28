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

#include "mozilla/BinarySearch.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/PodOperations.h"

#include "jsprf.h"

#include "asmjs/AsmJSValidate.h"
#include "asmjs/WasmSerialize.h"
#include "builtin/AtomicsObject.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/BaselineJIT.h"
#include "jit/ExecutableAllocator.h"
#include "js/MemoryMetrics.h"
#ifdef MOZ_VTUNE
# include "vtune/VTuneWrapper.h"
#endif

#include "jit/MacroAssembler-inl.h"
#include "vm/ArrayBufferObject-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::BinarySearch;
using mozilla::MakeEnumeratedRange;
using mozilla::PodZero;
using mozilla::Swap;
using JS::GenericNaN;

UniqueCodePtr
wasm::AllocateCode(ExclusiveContext* cx, size_t bytes)
{
    // On most platforms, this will allocate RWX memory. On iOS, or when
    // --non-writable-jitcode is used, this will allocate RW memory. In this
    // case, DynamicallyLinkModule will reprotect the code as RX.
    unsigned permissions =
        ExecutableAllocator::initialProtectionFlags(ExecutableAllocator::Writable);

    void* p = AllocateExecutableMemory(nullptr, bytes, permissions, "asm-js-code", AsmJSPageSize);
    if (!p)
        ReportOutOfMemory(cx);

    MOZ_ASSERT(uintptr_t(p) % AsmJSPageSize == 0);

    return UniqueCodePtr((uint8_t*)p, CodeDeleter(bytes));
}

void
CodeDeleter::operator()(uint8_t* p)
{
    DeallocateExecutableMemory(p, bytes_, AsmJSPageSize);
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
    for (const OffsetVector& offsets : *this)
        size += SerializedPodVectorSize(offsets);
    return size;
}

uint8_t*
StaticLinkData::SymbolicLinkArray::serialize(uint8_t* cursor) const
{
    for (const OffsetVector& offsets : *this)
        cursor = SerializePodVector(cursor, offsets);
    return cursor;
}

const uint8_t*
StaticLinkData::SymbolicLinkArray::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    for (OffsetVector& offsets : *this) {
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
    for (const OffsetVector& offsets : *this)
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
    size_t size = internalLinks.sizeOfExcludingThis(mallocSizeOf) +
                  symbolicLinks.sizeOfExcludingThis(mallocSizeOf) +
                  SizeOfVectorExcludingThis(funcPtrTables, mallocSizeOf);

    for (const OffsetVector& offsets : symbolicLinks)
        size += offsets.sizeOfExcludingThis(mallocSizeOf);

    return size;
}

static size_t
SerializedSigSize(const MallocSig& sig)
{
    return sizeof(ExprType) +
           SerializedPodVectorSize(sig.args());
}

static uint8_t*
SerializeSig(uint8_t* cursor, const MallocSig& sig)
{
    cursor = WriteScalar<ExprType>(cursor, sig.ret());
    cursor = SerializePodVector(cursor, sig.args());
    return cursor;
}

static const uint8_t*
DeserializeSig(ExclusiveContext* cx, const uint8_t* cursor, MallocSig* sig)
{
    ExprType ret;
    cursor = ReadScalar<ExprType>(cursor, &ret);

    MallocSig::ArgVector args;
    cursor = DeserializePodVector(cx, cursor, &args);
    if (!cursor)
        return nullptr;

    sig->init(Move(args), ret);
    return cursor;
}

static bool
CloneSig(JSContext* cx, const MallocSig& sig, MallocSig* out)
{
    MallocSig::ArgVector args;
    if (!ClonePodVector(cx, sig.args(), &args))
        return false;

    out->init(Move(args), sig.ret());
    return true;
}

static size_t
SizeOfSigExcludingThis(const MallocSig& sig, MallocSizeOf mallocSizeOf)
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
    return CloneSig(cx, sig_, &out->sig_);
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
    return CloneSig(cx, sig_, &out->sig_);
}

size_t
Import::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return SizeOfSigExcludingThis(sig_, mallocSizeOf);
}

CodeRange::CodeRange(Kind kind, Offsets offsets)
  : nameIndex_(0),
    lineNumber_(0),
    begin_(offsets.begin),
    profilingReturn_(0),
    end_(offsets.end)
{
    PodZero(&u);  // zero padding for Valgrind
    u.kind_ = kind;

    MOZ_ASSERT(begin_ <= end_);
    MOZ_ASSERT(u.kind_ == Entry || u.kind_ == Inline);
}

CodeRange::CodeRange(Kind kind, ProfilingOffsets offsets)
  : nameIndex_(0),
    lineNumber_(0),
    begin_(offsets.begin),
    profilingReturn_(offsets.profilingReturn),
    end_(offsets.end)
{
    PodZero(&u);  // zero padding for Valgrind
    u.kind_ = kind;

    MOZ_ASSERT(begin_ < profilingReturn_);
    MOZ_ASSERT(profilingReturn_ < end_);
    MOZ_ASSERT(u.kind_ == ImportJitExit || u.kind_ == ImportInterpExit || u.kind_ == Interrupt);
}

CodeRange::CodeRange(uint32_t nameIndex, uint32_t lineNumber, FuncOffsets offsets)
  : nameIndex_(nameIndex),
    lineNumber_(lineNumber)
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

size_t
CacheableChars::serializedSize() const
{
    return sizeof(uint32_t) + strlen(get());
}

uint8_t*
CacheableChars::serialize(uint8_t* cursor) const
{
    uint32_t length = strlen(get());
    cursor = WriteBytes(cursor, &length, sizeof(uint32_t));
    cursor = WriteBytes(cursor, get(), length);
    return cursor;
}

const uint8_t*
CacheableChars::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    uint32_t length;
    cursor = ReadBytes(cursor, &length, sizeof(uint32_t));

    reset(js_pod_calloc<char>(length + 1));
    if (!get())
        return nullptr;

    cursor = ReadBytes(cursor, get(), length);
    return cursor;
}

bool
CacheableChars::clone(JSContext* cx, CacheableChars* out) const
{
    *out = make_string_copy(get());
    return !!*out;
}

class Module::AutoMutateCode
{
    AutoWritableJitCode awjc_;
    AutoFlushICache afc_;

   public:
    AutoMutateCode(JSContext* cx, Module& module, const char* name)
      : awjc_(cx->runtime(), module.code(), module.pod.codeBytes_),
        afc_(name)
    {
        AutoFlushICache::setRange(uintptr_t(module.code()), module.pod.codeBytes_);
    }
};

uint32_t
Module::totalBytes() const
{
    return pod.codeBytes_ + pod.globalBytes_;
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

void
Module::specializeToHeap(ArrayBufferObjectMaybeShared* heap)
{
    MOZ_ASSERT_IF(heap->is<ArrayBufferObject>(), heap->as<ArrayBufferObject>().isAsmJS());
    MOZ_ASSERT(!maybeHeap_);
    MOZ_ASSERT(!rawHeapPtr());

    uint8_t* ptrBase = heap->dataPointerEither().unwrap(/*safe - protected by Module methods*/);
    uint32_t heapLength = heap->byteLength();
#if defined(JS_CODEGEN_X86)
    // An access is out-of-bounds iff
    //      ptr + offset + data-type-byte-size > heapLength
    // i.e. ptr > heapLength - data-type-byte-size - offset. data-type-byte-size
    // and offset are already included in the addend so we
    // just have to add the heap length here.
    for (const HeapAccess& access : heapAccesses_) {
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
    for (const HeapAccess& access : heapAccesses_) {
        // See comment above for x86 codegen.
        if (access.hasLengthCheck())
            X86Encoding::AddInt32(access.patchLengthAt(code()), heapLength);
    }
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
      defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    for (const HeapAccess& access : heapAccesses_)
        Assembler::UpdateBoundsCheck(heapLength, (Instruction*)(access.insnOffset() + code()));
#endif

    maybeHeap_ = heap;
    rawHeapPtr() = ptrBase;
}

void
Module::despecializeFromHeap(ArrayBufferObjectMaybeShared* heap)
{
    MOZ_ASSERT_IF(maybeHeap_, maybeHeap_ == heap);
    MOZ_ASSERT_IF(rawHeapPtr(), rawHeapPtr() == heap->dataPointerEither().unwrap());

#if defined(JS_CODEGEN_X86)
    uint32_t heapLength = heap->byteLength();
    uint8_t* ptrBase = heap->dataPointerEither().unwrap(/*safe - used for value*/);
    for (unsigned i = 0; i < heapAccesses_.length(); i++) {
        const HeapAccess& access = heapAccesses_[i];
        if (access.hasLengthCheck())
            X86Encoding::AddInt32(access.patchLengthAt(code()), -heapLength);
        void* addr = access.patchHeapPtrImmAt(code());
        uint8_t* ptr = reinterpret_cast<uint8_t*>(X86Encoding::GetPointer(addr));
        MOZ_ASSERT(ptr >= ptrBase);
        X86Encoding::SetPointer(addr, reinterpret_cast<void*>(ptr - ptrBase));
    }
#elif defined(JS_CODEGEN_X64)
    uint32_t heapLength = heap->byteLength();
    for (unsigned i = 0; i < heapAccesses_.length(); i++) {
        const HeapAccess& access = heapAccesses_[i];
        if (access.hasLengthCheck())
            X86Encoding::AddInt32(access.patchLengthAt(code()), -heapLength);
    }
#endif

    maybeHeap_ = nullptr;
    rawHeapPtr() = nullptr;
}

void
Module::sendCodeRangesToProfiler(JSContext* cx)
{
#ifdef JS_ION_PERF
    if (PerfFuncEnabled()) {
        for (const CodeRange& codeRange : codeRanges_) {
            if (!codeRange.isFunction())
                continue;

            uintptr_t start = uintptr_t(code() + codeRange.begin());
            uintptr_t end = uintptr_t(code() + codeRange.end());
            uintptr_t size = end - start;
            const char* file = filename_.get();
            unsigned line = codeRange.funcLineNumber();
            unsigned column = 0;
            const char* name = funcNames_[codeRange.funcNameIndex()].get();

            writePerfSpewerAsmJSFunctionMap(start, size, file, line, column, name);
        }
    }
#endif
#ifdef MOZ_VTUNE
    if (IsVTuneProfilingActive()) {
        for (const CodeRange& codeRange : codeRanges_) {
            if (!codeRange.isFunction())
                continue;

            uintptr_t start = uintptr_t(code() + codeRange.begin());
            uintptr_t end = uintptr_t(code() + codeRange.end());
            uintptr_t size = end - start;
            const char* name = funcNames_[codeRange.funcNameIndex()].get();

            unsigned method_id = iJIT_GetNewMethodID();
            if (method_id == 0)
                return;
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
    }
#endif
}

Module::ImportExit&
Module::importToExit(const Import& import)
{
    return *reinterpret_cast<ImportExit*>(globalData() + import.exitGlobalDataOffset());
}

/* static */ Module::CacheablePod
Module::zeroPod()
{
    CacheablePod pod = {0, 0, 0, false, false, false, false};
    return pod;
}

void
Module::init()
{
   staticallyLinked_ = false;
   interrupt_ = nullptr;
   outOfBounds_ = nullptr;
   dynamicallyLinked_ = false;
   prev_ = nullptr;
   next_ = nullptr;
   interrupted_ = false;

    *(double*)(globalData() + NaN64GlobalDataOffset) = GenericNaN();
    *(float*)(globalData() + NaN32GlobalDataOffset) = GenericNaN();
}

// Private constructor used for deserialization and cloning.
Module::Module(const CacheablePod& pod,
               UniqueCodePtr code,
               ImportVector&& imports,
               ExportVector&& exports,
               HeapAccessVector&& heapAccesses,
               CodeRangeVector&& codeRanges,
               CallSiteVector&& callSites,
               CacheableCharsVector&& funcNames,
               CacheableChars filename,
               CacheBool loadedFromCache,
               ProfilingBool profilingEnabled,
               FuncLabelVector&& funcLabels)
  : pod(pod),
    code_(Move(code)),
    imports_(Move(imports)),
    exports_(Move(exports)),
    heapAccesses_(Move(heapAccesses)),
    codeRanges_(Move(codeRanges)),
    callSites_(Move(callSites)),
    funcNames_(Move(funcNames)),
    filename_(Move(filename)),
    loadedFromCache_(loadedFromCache),
    profilingEnabled_(profilingEnabled),
    funcLabels_(Move(funcLabels))
{
    MOZ_ASSERT_IF(!profilingEnabled, funcLabels_.empty());
    MOZ_ASSERT_IF(profilingEnabled, funcNames_.length() == funcLabels_.length());
    init();
}

// Public constructor for compilation.
Module::Module(CompileArgs args,
               uint32_t functionBytes,
               uint32_t codeBytes,
               uint32_t globalBytes,
               HeapBool usesHeap,
               SharedBool sharedHeap,
               UniqueCodePtr code,
               ImportVector&& imports,
               ExportVector&& exports,
               HeapAccessVector&& heapAccesses,
               CodeRangeVector&& codeRanges,
               CallSiteVector&& callSites,
               CacheableCharsVector&& funcNames,
               CacheableChars filename)
  : pod(zeroPod()),
    code_(Move(code)),
    imports_(Move(imports)),
    exports_(Move(exports)),
    heapAccesses_(Move(heapAccesses)),
    codeRanges_(Move(codeRanges)),
    callSites_(Move(callSites)),
    funcNames_(Move(funcNames)),
    filename_(Move(filename)),
    loadedFromCache_(false),
    profilingEnabled_(false)
{
    // Work around MSVC 2013 bug around {} member initialization.
    const_cast<uint32_t&>(pod.functionBytes_) = functionBytes;
    const_cast<uint32_t&>(pod.codeBytes_) = codeBytes;
    const_cast<uint32_t&>(pod.globalBytes_) = globalBytes;
    const_cast<bool&>(pod.usesHeap_) = bool(usesHeap);
    const_cast<bool&>(pod.sharedHeap_) = bool(sharedHeap);
    const_cast<bool&>(pod.usesSignalHandlersForOOB_) = args.useSignalHandlersForOOB;
    const_cast<bool&>(pod.usesSignalHandlersForInterrupt_) = args.useSignalHandlersForInterrupt;

    MOZ_ASSERT_IF(sharedHeap, usesHeap);
    init();
}

Module::~Module()
{
    MOZ_ASSERT(!interrupted_);

    if (code_) {
        for (unsigned i = 0; i < imports_.length(); i++) {
            ImportExit& exit = importToExit(imports_[i]);
            if (exit.baselineScript)
                exit.baselineScript->removeDependentWasmModule(*this, i);
        }
    }

    if (prev_)
        *prev_ = next_;
    if (next_)
        next_->prev_ = prev_;
}

void
Module::trace(JSTracer* trc)
{
    for (const Import& import : imports_) {
        if (importToExit(import).fun)
            TraceEdge(trc, &importToExit(import).fun, "wasm function import");
    }

    if (maybeHeap_)
        TraceEdge(trc, &maybeHeap_, "wasm buffer");
}

CompileArgs
Module::compileArgs() const
{
    CompileArgs args;
    args.useSignalHandlersForOOB = pod.usesSignalHandlersForOOB_;
    args.useSignalHandlersForInterrupt = pod.usesSignalHandlersForInterrupt_;
    return args;
}

bool
Module::containsFunctionPC(void* pc) const
{
    return pc >= code() && pc < (code() + pod.functionBytes_);
}

bool
Module::containsCodePC(void* pc) const
{
    return pc >= code() && pc < (code() + pod.codeBytes_);
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
    size_t upperBound = callSites_.length();

    size_t match;
    if (!BinarySearch(CallSiteRetAddrOffset(callSites_), lowerBound, upperBound, target, &match))
        return nullptr;

    return &callSites_[match];
}

const CodeRange*
Module::lookupCodeRange(void* pc) const
{
    CodeRange::PC target((uint8_t*)pc - code());
    size_t lowerBound = 0;
    size_t upperBound = codeRanges_.length();

    size_t match;
    if (!BinarySearch(codeRanges_, lowerBound, upperBound, target, &match))
        return nullptr;

    return &codeRanges_[match];
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
    size_t upperBound = heapAccesses_.length();

    size_t match;
    if (!BinarySearch(HeapAccessOffset(heapAccesses_), lowerBound, upperBound, target, &match))
        return nullptr;

    return &heapAccesses_[match];
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
    AutoFlushICache::setRange(uintptr_t(code()), pod.codeBytes_);

    interrupt_ = code() + linkData.pod.interruptOffset;
    outOfBounds_ = code() + linkData.pod.outOfBoundsOffset;

    for (StaticLinkData::InternalLink link : linkData.internalLinks) {
        uint8_t* patchAt = code() + link.patchAtOffset;
        void* target = code() + link.targetOffset;
        if (profilingEnabled_) {
            const CodeRange* codeRange = lookupCodeRange(target);
            if (codeRange && codeRange->isFunction())
                target = code() + codeRange->funcProfilingEntry();
        }
        if (link.isRawPointerPatch())
            *(void**)(patchAt) = target;
        else
            Assembler::PatchInstructionImmediate(patchAt, PatchedImmPtr(target));
    }

    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        const StaticLinkData::OffsetVector& offsets = linkData.symbolicLinks[imm];
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
            if (profilingEnabled_)
                elem = code() + lookupCodeRange(elem)->funcProfilingEntry();
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

bool
Module::dynamicallyLink(JSContext* cx, Handle<ArrayBufferObjectMaybeShared*> heap,
                        const AutoVectorRooter<JSFunction*>& imports)
{
    MOZ_ASSERT(staticallyLinked_);
    MOZ_ASSERT(!dynamicallyLinked_);
    dynamicallyLinked_ = true;

    // Add this module to the JSRuntime-wide list of dynamically-linked modules.
    next_ = cx->runtime()->linkedWasmModules;
    prev_ = &cx->runtime()->linkedWasmModules;
    cx->runtime()->linkedWasmModules = this;
    if (next_)
        next_->prev_ = &next_;

    // Push a JitContext for benefit of IsCompilingAsmJS and flush the ICache.
    // We've been inhibiting flushing up to this point so flush it all now.
    JitContext jcx(CompileRuntime::get(cx->compartment()->runtimeFromAnyThread()));
    MOZ_ASSERT(IsCompilingAsmJS());
    AutoFlushICache afc("Module::dynamicallyLink");
    AutoFlushICache::setRange(uintptr_t(code()), pod.codeBytes_);

    // Initialize imports with actual imported values.
    MOZ_ASSERT(imports.length() == imports_.length());
    for (size_t i = 0; i < imports_.length(); i++) {
        const Import& import = imports_[i];
        ImportExit& exit = importToExit(import);
        exit.code = code() + import.interpExitCodeOffset();
        exit.fun = imports[i];
        exit.baselineScript = nullptr;
    }

    // Specialize code to the actual heap.
    if (heap)
        specializeToHeap(heap);

    // See AllocateCode comment above.
    ExecutableAllocator::makeExecutable(code(), pod.codeBytes_);

    sendCodeRangesToProfiler(cx);
    return true;
}

Module*
Module::nextLinked() const
{
    MOZ_ASSERT(dynamicallyLinked_);
    return next_;
}

ArrayBufferObjectMaybeShared*
Module::maybeBuffer() const
{
    MOZ_ASSERT(dynamicallyLinked_);
    return maybeHeap_;
}

SharedMem<uint8_t*>
Module::maybeHeap() const
{
    MOZ_ASSERT(dynamicallyLinked_);
    MOZ_ASSERT_IF(!pod.usesHeap_, rawHeapPtr() == nullptr);
    return pod.sharedHeap_
           ? SharedMem<uint8_t*>::shared(rawHeapPtr())
           : SharedMem<uint8_t*>::unshared(rawHeapPtr());
}

size_t
Module::heapLength() const
{
    MOZ_ASSERT(dynamicallyLinked_);
    return maybeHeap_ ? maybeHeap_->byteLength() : 0;
}

void
Module::deoptimizeImportExit(uint32_t importIndex)
{
    MOZ_ASSERT(dynamicallyLinked_);
    const Import& import = imports_[importIndex];
    ImportExit& exit = importToExit(import);
    exit.code = code() + import.interpExitCodeOffset();
    exit.baselineScript = nullptr;
}

bool
Module::hasDetachedHeap() const
{
    MOZ_ASSERT(dynamicallyLinked_);
    return pod.usesHeap_ && !maybeHeap_;
}

bool
Module::changeHeap(Handle<ArrayBufferObject*> newHeap, JSContext* cx)
{
    MOZ_ASSERT(dynamicallyLinked_);
    MOZ_ASSERT(pod.usesHeap_);

    // Content JS should not be able to run (and change heap) from within an
    // interrupt callback, but in case it does, fail to change heap. Otherwise,
    // the heap can change at every single instruction which would prevent
    // future optimizations like heap-base hoisting.
    if (interrupted_)
        return false;

    AutoMutateCode amc(cx, *this, "Module::changeHeap");
    if (maybeHeap_)
        despecializeFromHeap(maybeHeap_);
    specializeToHeap(newHeap);
    return true;
}

bool
Module::detachHeap(JSContext* cx)
{
    MOZ_ASSERT(dynamicallyLinked_);
    MOZ_ASSERT(pod.usesHeap_);

    // Content JS should not be able to run (and detach heap) from within an
    // interrupt callback, but in case it does, fail. Otherwise, the heap can
    // change at an arbitrary instruction and break the assumption below.
    if (interrupted_) {
        JS_ReportError(cx, "attempt to detach from inside interrupt handler");
        return false;
    }

    // Even if this->active(), to reach here, the activation must have called
    // out via an import exit stub. FFI stubs check if heapDatum() is null on
    // reentry and throw an exception if so.
    MOZ_ASSERT_IF(active(), activation()->exitReason() == ExitReason::ImportJit ||
                            activation()->exitReason() == ExitReason::ImportInterp);

    AutoMutateCode amc(cx, *this, "Module::detachHeap");
    despecializeFromHeap(maybeHeap_);
    return true;
}

void
Module::setInterrupted(bool interrupted)
{
    MOZ_ASSERT(dynamicallyLinked_);
    interrupted_ = interrupted;
}

AsmJSActivation*&
Module::activation()
{
    MOZ_ASSERT(dynamicallyLinked_);
    return *reinterpret_cast<AsmJSActivation**>(globalData() + ActivationGlobalDataOffset);
}

Module::EntryFuncPtr
Module::entryTrampoline(const Export& func) const
{
    MOZ_ASSERT(dynamicallyLinked_);
    return JS_DATA_TO_FUNC_PTR(EntryFuncPtr, code() + func.stubOffset());
}

bool
Module::callImport(JSContext* cx, uint32_t importIndex, unsigned argc, const Value* argv,
                   MutableHandleValue rval)
{
    MOZ_ASSERT(dynamicallyLinked_);

    const Import& import = imports_[importIndex];

    RootedValue fval(cx, ObjectValue(*importToExit(import).fun));
    if (!Invoke(cx, UndefinedValue(), fval, argc, argv, rval))
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

void
Module::setProfilingEnabled(bool enabled, JSContext* cx)
{
    MOZ_ASSERT(dynamicallyLinked_);
    MOZ_ASSERT(!active());

    if (profilingEnabled_ == enabled)
        return;

    // When enabled, generate profiling labels for every name in funcNames_
    // that is the name of some Function CodeRange. This involves malloc() so
    // do it now since, once we start sampling, we'll be in a signal-handing
    // context where we cannot malloc.
    if (enabled) {
        funcLabels_.resize(funcNames_.length());
        for (const CodeRange& codeRange : codeRanges_) {
            if (!codeRange.isFunction())
                continue;
            unsigned lineno = codeRange.funcLineNumber();
            const char* name = funcNames_[codeRange.funcNameIndex()].get();
            funcLabels_[codeRange.funcNameIndex()] =
                UniqueChars(JS_smprintf("%s (%s:%u)", name, filename_.get(), lineno));
        }
    } else {
        funcLabels_.clear();
    }

    // Patch callsites and returns to execute profiling prologues/epililogues.
    {
        AutoMutateCode amc(cx, *this, "Module::setProfilingEnabled");

        for (const CallSite& callSite : callSites_)
            EnableProfilingPrologue(*this, callSite, enabled);

        for (const CodeRange& codeRange : codeRanges_)
            EnableProfilingEpilogue(*this, codeRange, enabled);
    }

    // Update the function-pointer tables to point to profiling prologues.
    for (FuncPtrTable& funcPtrTable : funcPtrTables_) {
        auto array = reinterpret_cast<void**>(globalData() + funcPtrTable.globalDataOffset);
        for (size_t i = 0; i < funcPtrTable.numElems; i++) {
            const CodeRange* codeRange = lookupCodeRange(array[i]);
            void* from = code() + codeRange->funcNonProfilingEntry();
            void* to = code() + codeRange->funcProfilingEntry();
            if (!enabled)
                Swap(from, to);
            MOZ_ASSERT(array[i] == from);
            array[i] = to;
        }
    }

    profilingEnabled_ = enabled;
}

const char*
Module::profilingLabel(uint32_t funcIndex) const
{
    MOZ_ASSERT(dynamicallyLinked_);
    MOZ_ASSERT(profilingEnabled_);
    return funcLabels_[funcIndex].get();
}

size_t
Module::serializedSize() const
{
    return sizeof(pod) +
           pod.codeBytes_ +
           SerializedVectorSize(imports_) +
           SerializedVectorSize(exports_) +
           SerializedPodVectorSize(heapAccesses_) +
           SerializedPodVectorSize(codeRanges_) +
           SerializedPodVectorSize(callSites_) +
           SerializedVectorSize(funcNames_) +
           filename_.serializedSize();
}

uint8_t*
Module::serialize(uint8_t* cursor) const
{
    MOZ_ASSERT(!profilingEnabled_, "assumed by Module::deserialize");

    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    cursor = WriteBytes(cursor, code(), pod.codeBytes_);
    cursor = SerializeVector(cursor, imports_);
    cursor = SerializeVector(cursor, exports_);
    cursor = SerializePodVector(cursor, heapAccesses_);
    cursor = SerializePodVector(cursor, codeRanges_);
    cursor = SerializePodVector(cursor, callSites_);
    cursor = SerializeVector(cursor, funcNames_);
    cursor = filename_.serialize(cursor);
    return cursor;
}

/* static */ const uint8_t*
Module::deserialize(ExclusiveContext* cx, const uint8_t* cursor, UniqueModule* out)
{
    CacheablePod pod = zeroPod();
    cursor = ReadBytes(cursor, &pod, sizeof(pod));
    if (!cursor)
        return nullptr;

    UniqueCodePtr code = AllocateCode(cx, pod.codeBytes_ + pod.globalBytes_);
    if (!code)
        return nullptr;

    cursor = ReadBytes(cursor, code.get(), pod.codeBytes_);

    ImportVector imports;
    cursor = DeserializeVector(cx, cursor, &imports);
    if (!cursor)
        return nullptr;

    ExportVector exports;
    cursor = DeserializeVector(cx, cursor, &exports);
    if (!cursor)
        return nullptr;

    HeapAccessVector heapAccesses;
    cursor = DeserializePodVector(cx, cursor, &heapAccesses);
    if (!cursor)
        return nullptr;

    CodeRangeVector codeRanges;
    cursor = DeserializePodVector(cx, cursor, &codeRanges);
    if (!cursor)
        return nullptr;

    CallSiteVector callSites;
    cursor = DeserializePodVector(cx, cursor, &callSites);
    if (!cursor)
        return nullptr;

    CacheableCharsVector funcNames;
    cursor = DeserializeVector(cx, cursor, &funcNames);
    if (!cursor)
        return nullptr;

    CacheableChars filename;
    cursor = filename.deserialize(cx, cursor);
    if (!cursor)
        return nullptr;

    *out = cx->make_unique<Module>(pod,
                                   Move(code),
                                   Move(imports),
                                   Move(exports),
                                   Move(heapAccesses),
                                   Move(codeRanges),
                                   Move(callSites),
                                   Move(funcNames),
                                   Move(filename),
                                   Module::LoadedFromCache,
                                   Module::ProfilingDisabled,
                                   FuncLabelVector());

    return cursor;
}

Module::UniqueModule
Module::clone(JSContext* cx, const StaticLinkData& linkData) const
{
    MOZ_ASSERT(dynamicallyLinked_);

    UniqueCodePtr code = AllocateCode(cx, totalBytes());
    if (!code)
        return nullptr;

    memcpy(code.get(), this->code(), pod.codeBytes_);

#ifdef DEBUG
    // Put the symbolic links back to -1 so PatchDataWithValueCheck assertions
    // in Module::staticallyLink are valid.
    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        void* callee = AddressOf(imm, cx);
        const StaticLinkData::OffsetVector& offsets = linkData.symbolicLinks[imm];
        for (uint32_t offset : offsets) {
            jit::Assembler::PatchDataWithValueCheck(jit::CodeLocationLabel(code.get() + offset),
                                                    jit::PatchedImmPtr((void*)-1),
                                                    jit::PatchedImmPtr(callee));
        }
    }
#endif

    ImportVector imports;
    if (!CloneVector(cx, imports_, &imports))
        return nullptr;

    ExportVector exports;
    if (!CloneVector(cx, exports_, &exports))
        return nullptr;

    HeapAccessVector heapAccesses;
    if (!ClonePodVector(cx, heapAccesses_, &heapAccesses))
        return nullptr;

    CodeRangeVector codeRanges;
    if (!ClonePodVector(cx, codeRanges_, &codeRanges))
        return nullptr;

    CallSiteVector callSites;
    if (!ClonePodVector(cx, callSites_, &callSites))
        return nullptr;

    CacheableCharsVector funcNames;
    if (!CloneVector(cx, funcNames_, &funcNames))
        return nullptr;

    CacheableChars filename;
    if (!filename_.clone(cx, &filename))
        return nullptr;

    FuncLabelVector funcLabels;
    if (!CloneVector(cx, funcLabels_, &funcLabels))
        return nullptr;

    // Must not GC between Module allocation and (successful) return.
    auto out = cx->make_unique<Module>(pod,
                                       Move(code),
                                       Move(imports),
                                       Move(exports),
                                       Move(heapAccesses),
                                       Move(codeRanges),
                                       Move(callSites),
                                       Move(funcNames),
                                       Move(filename),
                                       CacheBool::NotLoadedFromCache,
                                       ProfilingBool(profilingEnabled_),
                                       Move(funcLabels));
    if (!out)
        return nullptr;

    // If the copied machine code has been specialized to the heap, it must be
    // unspecialized in the copy.
    if (maybeHeap_)
        out->despecializeFromHeap(maybeHeap_);

    if (!out->staticallyLink(cx, linkData))
        return nullptr;

    return Move(out);
}

void
Module::addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* asmJSModuleCode, size_t* asmJSModuleData)
{
    *asmJSModuleCode += pod.codeBytes_;
    *asmJSModuleData += mallocSizeOf(this) +
                        pod.globalBytes_ +
                        SizeOfVectorExcludingThis(imports_, mallocSizeOf) +
                        SizeOfVectorExcludingThis(exports_, mallocSizeOf) +
                        heapAccesses_.sizeOfExcludingThis(mallocSizeOf) +
                        codeRanges_.sizeOfExcludingThis(mallocSizeOf) +
                        callSites_.sizeOfExcludingThis(mallocSizeOf) +
                        funcNames_.sizeOfExcludingThis(mallocSizeOf) +
                        funcPtrTables_.sizeOfExcludingThis(mallocSizeOf);
}

