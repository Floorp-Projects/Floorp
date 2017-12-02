/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation, see jit/AtomicOperations.h */

// NOTE, this file is *not* used with the MIPS simulator, only when compiling
// for actual MIPS hardware.  The simulators get the files that are appropriate
// for the hardware the simulator is running on.  See the comments before the
// #include nest at the bottom of jit/AtomicOperations.h for more information.

// NOTE, MIPS32 unlike MIPS64 doesn't provide hardware support for lock-free
// 64-bit atomics. We lie down below about 8-byte atomics being always lock-
// free in order to support wasm jit. It is necessary to link with -latomic to
// get the 64-bit atomic intrinsics on MIPS32.

#ifndef jit_mips_shared_AtomicOperations_mips_shared_h
#define jit_mips_shared_AtomicOperations_mips_shared_h

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

#include "vm/ArrayBufferObject.h"

#if !defined(__clang__) && !defined(__GNUC__)
# error "This file only for gcc-compatible compilers"
#endif

inline bool
js::jit::AtomicOperations::hasAtomic8()
{
    return true;
}

inline bool
js::jit::AtomicOperations::isLockfree8()
{
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int8_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int16_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int32_t), 0));
# if _MIPS_SIM == _ABI64
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int64_t), 0));
# endif
    return true;
}

inline void
js::jit::AtomicOperations::fenceSeqCst()
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::loadSeqCst(T* addr)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    T v;
    __atomic_load(addr, &v, __ATOMIC_SEQ_CST);
    return v;
}

template<typename T>
inline void
js::jit::AtomicOperations::storeSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    __atomic_store(addr, &val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::compareExchangeSeqCst(T* addr, T oldval, T newval)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    __atomic_compare_exchange(addr, &oldval, &newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return oldval;
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAddSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchSubSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    return __atomic_fetch_sub(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAndSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    return __atomic_fetch_and(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchOrSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    return __atomic_fetch_or(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchXorSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    return __atomic_fetch_xor(addr, val, __ATOMIC_SEQ_CST);

}

template<typename T>
inline T
js::jit::AtomicOperations::loadSafeWhenRacy(T* addr)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    if (__atomic_always_lock_free(sizeof(T), 0)) {
        T v;
        __atomic_load(addr, &v, __ATOMIC_RELAXED);
        return v;
    } else {
        return *addr;
    }
}

namespace js { namespace jit {

template<>
inline uint8_clamped
js::jit::AtomicOperations::loadSafeWhenRacy(uint8_clamped* addr)
{
    uint8_t v;
    __atomic_load(&addr->val, &v, __ATOMIC_RELAXED);
    return uint8_clamped(v);
}

template<>
inline float
js::jit::AtomicOperations::loadSafeWhenRacy(float* addr)
{
    return *addr;
}

template<>
inline double
js::jit::AtomicOperations::loadSafeWhenRacy(double* addr)
{
    return *addr;
}

} }

template<typename T>
inline void
js::jit::AtomicOperations::storeSafeWhenRacy(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    if (__atomic_always_lock_free(sizeof(T), 0)) {
        __atomic_store(addr, &val, __ATOMIC_RELAXED);
    } else {
        *addr = val;
    }
}

namespace js { namespace jit {

template<>
inline void
js::jit::AtomicOperations::storeSafeWhenRacy(uint8_clamped* addr, uint8_clamped val)
{
    __atomic_store(&addr->val, &val.val, __ATOMIC_RELAXED);
}

template<>
inline void
js::jit::AtomicOperations::storeSafeWhenRacy(float* addr, float val)
{
    *addr = val;
}

template<>
inline void
js::jit::AtomicOperations::storeSafeWhenRacy(double* addr, double val)
{
    *addr = val;
}

} }

inline void
js::jit::AtomicOperations::memcpySafeWhenRacy(void* dest, const void* src, size_t nbytes)
{
    MOZ_ASSERT(!((char*)dest <= (char*)src && (char*)src < (char*)dest+nbytes));
    MOZ_ASSERT(!((char*)src <= (char*)dest && (char*)dest < (char*)src+nbytes));
    ::memcpy(dest, src, nbytes);
}

inline void
js::jit::AtomicOperations::memmoveSafeWhenRacy(void* dest, const void* src, size_t nbytes)
{
    ::memmove(dest, src, nbytes);
}

template<typename T>
inline T
js::jit::AtomicOperations::exchangeSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    T v;
    __atomic_exchange(addr, &val, &v, __ATOMIC_SEQ_CST);
    return v;
}

#endif // jit_mips_shared_AtomicOperations_mips_shared_h
