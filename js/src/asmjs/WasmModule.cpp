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

#include "asmjs/WasmBinaryToExperimentalText.h"
#include "asmjs/WasmBinaryToText.h"
#include "asmjs/WasmSerialize.h"
#include "builtin/AtomicsObject.h"
#include "builtin/SIMD.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/BaselineJIT.h"
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
using mozilla::BinarySearch;
using mozilla::MakeEnumeratedRange;
using mozilla::PodCopy;
using mozilla::PodZero;
using mozilla::Swap;
using JS::GenericNaN;

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

size_t
StaticLinkData::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return internalLinks.sizeOfExcludingThis(mallocSizeOf) +
           symbolicLinks.sizeOfExcludingThis(mallocSizeOf) +
           SizeOfVectorExcludingThis(funcPtrTables, mallocSizeOf);
}

size_t
ExportMap::serializedSize() const
{
    return SerializedVectorSize(fieldNames) +
           SerializedPodVectorSize(fieldsToExports);
}

uint8_t*
ExportMap::serialize(uint8_t* cursor) const
{
    cursor = SerializeVector(cursor, fieldNames);
    cursor = SerializePodVector(cursor, fieldsToExports);
    return cursor;
}

const uint8_t*
ExportMap::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = DeserializeVector(cx, cursor, &fieldNames)) &&
    (cursor = DeserializePodVector(cx, cursor, &fieldsToExports));
    return cursor;
}

size_t
ExportMap::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return SizeOfVectorExcludingThis(fieldNames, mallocSizeOf) &&
           fieldsToExports.sizeOfExcludingThis(mallocSizeOf);
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
    for (const HeapAccess& access : metadata_->heapAccesses) {
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
    for (const HeapAccess& access : metadata_->heapAccesses) {
        // See comment above for x86 codegen.
        if (access.hasLengthCheck())
            X86Encoding::AddInt32(access.patchLengthAt(code()), heapLength);
    }
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
      defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    for (const HeapAccess& access : metadata_->heapAccesses)
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
    for (unsigned i = 0; i < metadata_->heapAccesses.length(); i++) {
        const HeapAccess& access = metadata_->heapAccesses[i];
        if (access.hasLengthCheck())
            X86Encoding::AddInt32(access.patchLengthAt(code()), -heapLength);
        void* addr = access.patchHeapPtrImmAt(code());
        uint8_t* ptr = reinterpret_cast<uint8_t*>(X86Encoding::GetPointer(addr));
        MOZ_ASSERT(ptr >= ptrBase);
        X86Encoding::SetPointer(addr, reinterpret_cast<void*>(ptr - ptrBase));
    }
#elif defined(JS_CODEGEN_X64)
    uint32_t heapLength = heap->byteLength();
    for (unsigned i = 0; i < metadata_->heapAccesses.length(); i++) {
        const HeapAccess& access = metadata_->heapAccesses[i];
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

    for (const CodeRange& codeRange : metadata_->codeRanges) {
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
            const char* file = metadata_->filename.get();
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
        for (const CodeRange& codeRange : metadata_->codeRanges) {
            if (!codeRange.isFunction())
                continue;

            UniqueChars owner;
            const char* funcName = getFuncName(cx, codeRange.funcIndex(), &owner);
            if (!funcName)
                return false;

            UniqueChars label(JS_smprintf("%s (%s:%u)",
                                          funcName,
                                          metadata_->filename.get(),
                                          codeRange.funcLineOrBytecode()));
            if (!label) {
                ReportOutOfMemory(cx);
                return false;
            }

            if (codeRange.funcIndex() >= funcLabels_.length()) {
                if (!funcLabels_.resize(codeRange.funcIndex() + 1))
                    return false;
            }
            funcLabels_[codeRange.funcIndex()] = Move(label);
        }
    } else {
        funcLabels_.clear();
    }

    // Patch callsites and returns to execute profiling prologues/epilogues.
    {
        AutoWritableJitCode awjc(cx->runtime(), code(), codeLength());
        AutoFlushICache afc("Module::setProfilingEnabled");
        AutoFlushICache::setRange(uintptr_t(code()), codeLength());

        for (const CallSite& callSite : metadata_->callSites)
            ToggleProfiling(*this, callSite, enabled);

        for (const CallThunk& callThunk : metadata_->callThunks)
            ToggleProfiling(*this, callThunk, enabled);

        for (const CodeRange& codeRange : metadata_->codeRanges)
            ToggleProfiling(*this, codeRange, enabled);
    }

    // In asm.js, table elements point directly to the prologue and must be
    // updated to reflect the profiling mode. In wasm, table elements point to
    // the (one) table entry which checks signature before jumping to the
    // appropriate prologue (which is patched by ToggleProfiling).
    if (isAsmJS()) {
        for (FuncPtrTable& table : funcPtrTables_) {
            auto array = reinterpret_cast<void**>(globalData() + table.globalDataOffset);
            for (size_t i = 0; i < table.numElems; i++) {
                const CodeRange* codeRange = lookupCodeRange(array[i]);
                void* from = code() + codeRange->funcNonProfilingEntry();
                void* to = code() + codeRange->funcProfilingEntry();
                if (!enabled)
                    Swap(from, to);
                MOZ_ASSERT(array[i] == from);
                array[i] = to;
            }
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

    // The out->metadata_ field was already cloned and initialized when 'out' was
    // constructed. This function should clone the rest.
    MOZ_ASSERT(out->metadata_);

    // Copy the profiling state over too since the cloned machine code
    // implicitly brings the profiling mode.
    out->profilingEnabled_ = profilingEnabled_;
    for (const CacheableChars& label : funcLabels_) {
        if (!out->funcLabels_.emplaceBack(DuplicateString(label.get())))
            return false;
    }

#ifdef DEBUG
    // Put the symbolic links back to -1 so PatchDataWithValueCheck assertions
    // in Module::staticallyLink are valid.
    for (auto imm : MakeEnumeratedRange(SymbolicAddress::Limit)) {
        void* callee = AddressOf(imm, cx);
        const Uint32Vector& offsets = link.symbolicLinks[imm];
        for (uint32_t offset : offsets) {
            Assembler::PatchDataWithValueCheck(CodeLocationLabel(out->code() + offset),
                                               PatchedImmPtr((void*)-1),
                                               PatchedImmPtr(callee));
        }
    }
#endif

    // If the copied machine code has been specialized to the heap, it must be
    // unspecialized in the copy.
    if (usesHeap())
        out->despecializeFromHeap(heap_);

    return true;
}

Module::Module(UniqueCodeSegment codeSegment, const Metadata& metadata)
  : codeSegment_(Move(codeSegment)),
    metadata_(&metadata),
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
    for (const CodeRange& cr : metadata_->codeRanges) {
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
    *code += codeSegment_->codeLength();
    *data += mallocSizeOf(this) +
             codeSegment_->globalDataLength() +
             mallocSizeOf(metadata_.get()) +
             metadata_->sizeOfExcludingThis(mallocSizeOf) +
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
    return pc >= code() && pc < (code() + metadata_->functionLength);
}

bool
Module::containsCodePC(void* pc) const
{
    return pc >= code() && pc < (code() + codeLength());
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
    size_t upperBound = metadata_->callSites.length();

    size_t match;
    if (!BinarySearch(CallSiteRetAddrOffset(metadata_->callSites), lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->callSites[match];
}

const CodeRange*
Module::lookupCodeRange(void* pc) const
{
    CodeRange::PC target((uint8_t*)pc - code());
    size_t lowerBound = 0;
    size_t upperBound = metadata_->codeRanges.length();

    size_t match;
    if (!BinarySearch(metadata_->codeRanges, lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->codeRanges[match];
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
    size_t upperBound = metadata_->heapAccesses.length();

    size_t match;
    if (!BinarySearch(HeapAccessOffset(metadata_->heapAccesses), lowerBound, upperBound, target, &match))
        return nullptr;

    return &metadata_->heapAccesses[match];
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
    AutoFlushICache::setRange(uintptr_t(code()), codeLength());

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
            if (profilingEnabled_ && !codeRange->isInline())
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
    const Export& exp = module.exports()[exportIndex];
    unsigned numArgs = exp.sig().args().length();

    RootedAtom name(cx, module.getFuncAtom(cx, exp.funcIndex()));
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
    AutoFlushICache::setRange(uintptr_t(code()), codeLength());

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

    // See CodeSegment::allocate comment above.
    if (!ExecutableAllocator::makeExecutable(code(), codeLength())) {
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

static bool
ReadI64Object(JSContext* cx, HandleValue v, int64_t* i64)
{
    if (!v.isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_FAIL,
                             "i64 JS value must be an object");
        return false;
    }

    RootedObject obj(cx, &v.toObject());

    int32_t* i32 = (int32_t*)i64;

    RootedValue val(cx);
    if (!JS_GetProperty(cx, obj, "low", &val))
        return false;
    if (!ToInt32(cx, val, &i32[0]))
        return false;

    if (!JS_GetProperty(cx, obj, "high", &val))
        return false;
    if (!ToInt32(cx, val, &i32[1]))
        return false;

    return true;
}

static JSObject*
CreateI64Object(JSContext* cx, int64_t i64)
{
    RootedObject result(cx, JS_NewPlainObject(cx));
    if (!result)
        return nullptr;

    RootedValue val(cx, Int32Value(uint32_t(i64)));
    if (!JS_DefineProperty(cx, result, "low", val, JSPROP_ENUMERATE))
        return nullptr;

    val = Int32Value(uint32_t(i64 >> 32));
    if (!JS_DefineProperty(cx, result, "high", val, JSPROP_ENUMERATE))
        return nullptr;

    return result;
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
            MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
            if (!ReadI64Object(cx, v, (int64_t*)&coercedArgs[i]))
                return false;
            break;
          case ValType::F32:
            if (!RoundFloat32(cx, v, (float*)&coercedArgs[i]))
                return false;
            break;
          case ValType::F64:
            if (!ToNumber(cx, v, (double*)&coercedArgs[i]))
                return false;
            break;
          case ValType::I8x16: {
            SimdConstant simd;
            if (!ToSimdConstant<Int8x16>(cx, v, &simd))
                return false;
            memcpy(&coercedArgs[i], simd.asInt8x16(), Simd128DataSize);
            break;
          }
          case ValType::I16x8: {
            SimdConstant simd;
            if (!ToSimdConstant<Int16x8>(cx, v, &simd))
                return false;
            memcpy(&coercedArgs[i], simd.asInt16x8(), Simd128DataSize);
            break;
          }
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
          case ValType::B8x16: {
            SimdConstant simd;
            if (!ToSimdConstant<Bool8x16>(cx, v, &simd))
                return false;
            // Bool8x16 uses the same representation as Int8x16.
            memcpy(&coercedArgs[i], simd.asInt8x16(), Simd128DataSize);
            break;
          }
          case ValType::B16x8: {
            SimdConstant simd;
            if (!ToSimdConstant<Bool16x8>(cx, v, &simd))
                return false;
            // Bool16x8 uses the same representation as Int16x8.
            memcpy(&coercedArgs[i], simd.asInt16x8(), Simd128DataSize);
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

    void* retAddr = &coercedArgs[0];
    JSObject* retObj = nullptr;
    switch (exp.sig().ret()) {
      case ExprType::Void:
        args.rval().set(UndefinedValue());
        break;
      case ExprType::I32:
        args.rval().set(Int32Value(*(int32_t*)retAddr));
        break;
      case ExprType::I64:
        MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
        retObj = CreateI64Object(cx, *(int64_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::F32:
        // The entry stub has converted the F32 into a double for us.
      case ExprType::F64:
        args.rval().set(NumberValue(*(double*)retAddr));
        break;
      case ExprType::I8x16:
        retObj = CreateSimd<Int8x16>(cx, (int8_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::I16x8:
        retObj = CreateSimd<Int16x8>(cx, (int16_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::I32x4:
        retObj = CreateSimd<Int32x4>(cx, (int32_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::F32x4:
        retObj = CreateSimd<Float32x4>(cx, (float*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::B8x16:
        retObj = CreateSimd<Bool8x16>(cx, (int8_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::B16x8:
        retObj = CreateSimd<Bool16x8>(cx, (int16_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::B32x4:
        retObj = CreateSimd<Bool32x4>(cx, (int32_t*)retAddr);
        if (!retObj)
            return false;
        break;
      case ExprType::Limit:
        MOZ_CRASH("Limit");
    }

    if (retObj)
        args.rval().set(ObjectValue(*retObj));

    return true;
}

bool
Module::callImport(JSContext* cx, uint32_t importIndex, unsigned argc, const uint64_t* argv,
                   MutableHandleValue rval)
{
    MOZ_ASSERT(dynamicallyLinked_);

    const Import& import = imports()[importIndex];

    InvokeArgs args(cx);
    if (!args.init(argc))
        return false;

    bool hasI64Arg = false;
    MOZ_ASSERT(import.sig().args().length() == argc);
    for (size_t i = 0; i < argc; i++) {
        switch (import.sig().args()[i]) {
          case ValType::I32:
            args[i].set(Int32Value(*(int32_t*)&argv[i]));
            break;
          case ValType::F32:
            args[i].set(JS::CanonicalizedDoubleValue(*(float*)&argv[i]));
            break;
          case ValType::F64:
            args[i].set(JS::CanonicalizedDoubleValue(*(double*)&argv[i]));
            break;
          case ValType::I64: {
            MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
            RootedObject obj(cx, CreateI64Object(cx, *(int64_t*)&argv[i]));
            if (!obj)
                return false;
            args[i].set(ObjectValue(*obj));
            hasI64Arg = true;
            break;
          }
          case ValType::I8x16:
          case ValType::I16x8:
          case ValType::I32x4:
          case ValType::F32x4:
          case ValType::B8x16:
          case ValType::B16x8:
          case ValType::B32x4:
          case ValType::Limit:
            MOZ_CRASH("unhandled type in callImport");
        }
    }

    RootedValue fval(cx, ObjectValue(*importToExit(import).fun));
    RootedValue thisv(cx, UndefinedValue());
    if (!Call(cx, fval, thisv, args, rval))
        return false;

    // Don't try to optimize if the function has at least one i64 arg or if
    // it returns an int64. GenerateJitExit relies on this, as does the
    // type inference code below in this function.
    if (hasI64Arg || import.sig().ret() == ExprType::I64)
        return true;

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
          case ValType::I64:   MOZ_CRASH("can't happen because of above guard");
          case ValType::F32:   type = TypeSet::DoubleType(); break;
          case ValType::F64:   type = TypeSet::DoubleType(); break;
          case ValType::I8x16: MOZ_CRASH("NYI");
          case ValType::I16x8: MOZ_CRASH("NYI");
          case ValType::I32x4: MOZ_CRASH("NYI");
          case ValType::F32x4: MOZ_CRASH("NYI");
          case ValType::B8x16: MOZ_CRASH("NYI");
          case ValType::B16x8: MOZ_CRASH("NYI");
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

/* static */ int32_t
Module::callImport_void(int32_t importIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    return activation->module().callImport(cx, importIndex, argc, argv, &rval);
}

/* static */ int32_t
Module::callImport_i32(int32_t importIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->module().callImport(cx, importIndex, argc, argv, &rval))
        return false;

    return ToInt32(cx, rval, (int32_t*)argv);
}

/* static */ int32_t
Module::callImport_i64(int32_t importIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->module().callImport(cx, importIndex, argc, argv, &rval))
        return false;

    return ReadI64Object(cx, rval, (int64_t*)argv);
}

/* static */ int32_t
Module::callImport_f64(int32_t importIndex, int32_t argc, uint64_t* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->module().callImport(cx, importIndex, argc, argv, &rval))
        return false;

    return ToNumber(cx, rval, (double*)argv);
}

const char*
Module::maybePrettyFuncName(uint32_t funcIndex) const
{
    if (funcIndex >= metadata_->prettyFuncNames.length())
        return nullptr;
    return metadata_->prettyFuncNames[funcIndex].get();
}

const char*
Module::getFuncName(JSContext* cx, uint32_t funcIndex, UniqueChars* owner) const
{
    if (const char* prettyName = maybePrettyFuncName(funcIndex))
        return prettyName;

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
    "Restart with developer tools open to view WebAssembly source";

JSString*
Module::createText(JSContext* cx)
{
    StringBuffer buffer(cx);
    if (!source_.empty()) {
        if (!buffer.append(experimentalWarning))
            return nullptr;
        if (!BinaryToExperimentalText(cx, source_.begin(), source_.length(), buffer, ExperimentalTextFormatting()))
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

void
WasmModuleObject::init(Module& module)
{
    MOZ_ASSERT(is<WasmModuleObject>());
    MOZ_ASSERT(!hasModule());
    module.setOwner(this);
    setReservedSlot(MODULE_SLOT, PrivateValue(&module));
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
