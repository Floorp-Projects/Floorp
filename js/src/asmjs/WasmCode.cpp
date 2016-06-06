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

#include "asmjs/WasmSerialize.h"
#include "jit/ExecutableAllocator.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;
using mozilla::Atomic;

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

/* static */ UniqueCodeSegment
CodeSegment::allocate(ExclusiveContext* cx, uint32_t codeLength, uint32_t globalDataLength)
{
    UniqueCodeSegment code = cx->make_unique<CodeSegment>();
    if (!code)
        return nullptr;

    uint8_t* bytes = AllocateCodeSegment(cx, codeLength + globalDataLength);
    if (!bytes)
        return nullptr;

    code->bytes_ = bytes;
    code->codeLength_ = codeLength;
    code->globalDataLength_ = globalDataLength;
    return code;
}

/* static */ UniqueCodeSegment
CodeSegment::clone(ExclusiveContext* cx, const CodeSegment& src)
{
    UniqueCodeSegment dst = allocate(cx, src.codeLength_, src.globalDataLength_);
    if (!dst)
        return nullptr;

    memcpy(dst->code(), src.code(), src.codeLength());
    return dst;
}

CodeSegment::~CodeSegment()
{
    if (!bytes_) {
        MOZ_ASSERT(!totalLength());
        return;
    }

    MOZ_ASSERT(wasmCodeAllocations > 0);
    wasmCodeAllocations--;

    MOZ_ASSERT(totalLength() > 0);
    DeallocateExecutableMemory(bytes_, totalLength(), gc::SystemPageSize());
}

size_t
CodeSegment::serializedSize() const
{
    return sizeof(uint32_t) +
           sizeof(uint32_t) +
           codeLength_;
}

uint8_t*
CodeSegment::serialize(uint8_t* cursor) const
{
    cursor = WriteScalar<uint32_t>(cursor, codeLength_);
    cursor = WriteScalar<uint32_t>(cursor, globalDataLength_);
    cursor = WriteBytes(cursor, bytes_, codeLength_);
    return cursor;
}

const uint8_t*
CodeSegment::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    cursor = ReadScalar<uint32_t>(cursor, &codeLength_);
    cursor = ReadScalar<uint32_t>(cursor, &globalDataLength_);

    bytes_ = AllocateCodeSegment(cx, codeLength_ + globalDataLength_);
    if (!bytes_)
        return nullptr;

    cursor = ReadBytes(cursor, bytes_, codeLength_);
    return cursor;
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

size_t
Import::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return SizeOfSigExcludingThis(sig_, mallocSizeOf);
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

size_t
CacheableChars::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(get());
}

size_t
Metadata::serializedSize() const
{
    return sizeof(pod()) +
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
Metadata::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &pod(), sizeof(pod()));
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
Metadata::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &pod(), sizeof(pod()))) &&
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

size_t
Metadata::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return SizeOfVectorExcludingThis(imports, mallocSizeOf) +
           SizeOfVectorExcludingThis(exports, mallocSizeOf) +
           heapAccesses.sizeOfExcludingThis(mallocSizeOf) +
           codeRanges.sizeOfExcludingThis(mallocSizeOf) +
           callSites.sizeOfExcludingThis(mallocSizeOf) +
           callThunks.sizeOfExcludingThis(mallocSizeOf) +
           SizeOfVectorExcludingThis(prettyFuncNames, mallocSizeOf) +
           filename.sizeOfExcludingThis(mallocSizeOf);
}
