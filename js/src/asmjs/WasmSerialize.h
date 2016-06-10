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

#ifndef wasm_serialize_h
#define wasm_serialize_h

#include "jit/MacroAssembler.h"

namespace js {
namespace wasm {

// Factor out common serialization, cloning and about:memory size-computation
// functions for reuse when serializing wasm and asm.js modules.

static inline uint8_t*
WriteBytes(uint8_t* dst, const void* src, size_t nbytes)
{
    memcpy(dst, src, nbytes);
    return dst + nbytes;
}

static inline const uint8_t*
ReadBytes(const uint8_t* src, void* dst, size_t nbytes)
{
    memcpy(dst, src, nbytes);
    return src + nbytes;
}

template <class T>
static inline uint8_t*
WriteScalar(uint8_t* dst, T t)
{
    memcpy(dst, &t, sizeof(t));
    return dst + sizeof(t);
}

template <class T>
static inline const uint8_t*
ReadScalar(const uint8_t* src, T* dst)
{
    memcpy(dst, src, sizeof(*dst));
    return src + sizeof(*dst);
}

template <class T, size_t N>
static inline size_t
SerializedVectorSize(const mozilla::Vector<T, N, SystemAllocPolicy>& vec)
{
    size_t size = sizeof(uint32_t);
    for (size_t i = 0; i < vec.length(); i++)
        size += vec[i].serializedSize();
    return size;
}

template <class T, size_t N>
static inline uint8_t*
SerializeVector(uint8_t* cursor, const mozilla::Vector<T, N, SystemAllocPolicy>& vec)
{
    cursor = WriteScalar<uint32_t>(cursor, vec.length());
    for (size_t i = 0; i < vec.length(); i++)
        cursor = vec[i].serialize(cursor);
    return cursor;
}

template <class T, size_t N>
static inline const uint8_t*
DeserializeVector(ExclusiveContext* cx, const uint8_t* cursor,
                  mozilla::Vector<T, N, SystemAllocPolicy>* vec)
{
    uint32_t length;
    cursor = ReadScalar<uint32_t>(cursor, &length);
    if (!vec->resize(length))
        return nullptr;
    for (size_t i = 0; i < vec->length(); i++) {
        if (!(cursor = (*vec)[i].deserialize(cx, cursor)))
            return nullptr;
    }
    return cursor;
}

template <class T, size_t N>
static inline size_t
SizeOfVectorExcludingThis(const mozilla::Vector<T, N, SystemAllocPolicy>& vec,
                          MallocSizeOf mallocSizeOf)
{
    size_t size = vec.sizeOfExcludingThis(mallocSizeOf);
    for (const T& t : vec)
        size += t.sizeOfExcludingThis(mallocSizeOf);
    return size;
}

template <class T, size_t N>
static inline size_t
SerializedPodVectorSize(const mozilla::Vector<T, N, SystemAllocPolicy>& vec)
{
    return sizeof(uint32_t) +
           vec.length() * sizeof(T);
}

template <class T, size_t N>
static inline uint8_t*
SerializePodVector(uint8_t* cursor, const mozilla::Vector<T, N, SystemAllocPolicy>& vec)
{
    cursor = WriteScalar<uint32_t>(cursor, vec.length());
    cursor = WriteBytes(cursor, vec.begin(), vec.length() * sizeof(T));
    return cursor;
}

template <class T, size_t N>
static inline const uint8_t*
DeserializePodVector(ExclusiveContext* cx, const uint8_t* cursor,
                     mozilla::Vector<T, N, SystemAllocPolicy>* vec)
{
    uint32_t length;
    cursor = ReadScalar<uint32_t>(cursor, &length);
    if (!vec->resize(length))
        return nullptr;
    cursor = ReadBytes(cursor, vec->begin(), length * sizeof(T));
    return cursor;
}

static inline MOZ_MUST_USE bool
GetCPUID(uint32_t* cpuId)
{
    enum Arch {
        X86 = 0x1,
        X64 = 0x2,
        ARM = 0x3,
        MIPS = 0x4,
        MIPS64 = 0x5,
        ARCH_BITS = 3
    };

#if defined(JS_CODEGEN_X86)
    MOZ_ASSERT(uint32_t(jit::CPUInfo::GetSSEVersion()) <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = X86 | (uint32_t(jit::CPUInfo::GetSSEVersion()) << ARCH_BITS);
    return true;
#elif defined(JS_CODEGEN_X64)
    MOZ_ASSERT(uint32_t(jit::CPUInfo::GetSSEVersion()) <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = X64 | (uint32_t(jit::CPUInfo::GetSSEVersion()) << ARCH_BITS);
    return true;
#elif defined(JS_CODEGEN_ARM)
    MOZ_ASSERT(jit::GetARMFlags() <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = ARM | (jit::GetARMFlags() << ARCH_BITS);
    return true;
#elif defined(JS_CODEGEN_MIPS32)
    MOZ_ASSERT(jit::GetMIPSFlags() <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = MIPS | (jit::GetMIPSFlags() << ARCH_BITS);
    return true;
#elif defined(JS_CODEGEN_MIPS64)
    MOZ_ASSERT(jit::GetMIPSFlags() <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = MIPS64 | (jit::GetMIPSFlags() << ARCH_BITS);
    return true;
#else
    return false;
#endif
}

class MachineId
{
    uint32_t cpuId_;
    JS::BuildIdCharVector buildId_;

  public:
    MOZ_MUST_USE bool extractCurrentState(ExclusiveContext* cx) {
        if (!cx->buildIdOp())
            return false;
        if (!cx->buildIdOp()(&buildId_))
            return false;
        if (!GetCPUID(&cpuId_))
            return false;
        return true;
    }

    size_t serializedSize() const {
        return sizeof(uint32_t) +
               SerializedPodVectorSize(buildId_);
    }

    uint8_t* serialize(uint8_t* cursor) const {
        cursor = WriteScalar<uint32_t>(cursor, cpuId_);
        cursor = SerializePodVector(cursor, buildId_);
        return cursor;
    }

    const uint8_t* deserialize(ExclusiveContext* cx, const uint8_t* cursor) {
        (cursor = ReadScalar<uint32_t>(cursor, &cpuId_)) &&
        (cursor = DeserializePodVector(cx, cursor, &buildId_));
        return cursor;
    }

    bool operator==(const MachineId& rhs) const {
        return cpuId_ == rhs.cpuId_ &&
               buildId_.length() == rhs.buildId_.length() &&
               mozilla::PodEqual(buildId_.begin(), rhs.buildId_.begin(), buildId_.length());
    }
    bool operator!=(const MachineId& rhs) const {
        return !(*this == rhs);
    }
};

} // namespace wasm
} // namespace js

#endif // wasm_serialize_h
