/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm_AtomicOperations_arm_h
#define jit_arm_AtomicOperations_arm_h

#include "jit/arm/Architecture-arm.h"

// For documentation, see jit/AtomicOperations.h

// NOTE, this file is *not* used with the ARM simulator, only when compiling for
// actual ARM hardware.  The simulators get the files that are appropriate for
// the hardware the simulator is running on.  See the comments before the
// #include nest at the bottom of jit/AtomicOperations.h for more information.

// Firefox requires gcc > 4.8, so we will always have the __atomic intrinsics
// added for use in C++11 <atomic>.
//
// Note that using these intrinsics for most operations is not correct: the code
// has undefined behavior.  The gcc documentation states that the compiler
// assumes the code is race free.  This supposedly means C++ will allow some
// instruction reorderings (effectively those allowed by TSO) even for seq_cst
// ordered operations, but these reorderings are not allowed by JS.  To do
// better we will end up with inline assembler or JIT-generated code.

#if !defined(__clang__) && !defined(__GNUC__)
# error "This file only for gcc-compatible compilers"
#endif

inline bool
js::jit::AtomicOperations::hasAtomic8()
{
    // This guard is really only for tier-2 and tier-3 systems: LDREXD and
    // STREXD have been available since ARMv6K, and only ARMv7 and later are
    // tier-1.
    return HasLDSTREXBHD();
}

inline bool
js::jit::AtomicOperations::isLockfree8()
{
    // The JIT and the C++ compiler must agree on whether to use atomics
    // for 64-bit accesses.  There are two ways to do this: either the
    // JIT defers to the C++ compiler (so if the C++ code is compiled
    // for ARMv6, say, and __atomic_always_lock_free(8) is false, then the
    // JIT ignores the fact that the program is running on ARMv7 or newer);
    // or the C++ code in this file calls out to run-time generated code
    // to do whatever the JIT does.
    //
    // For now, make the JIT defer to the C++ compiler when we know what
    // the C++ compiler will do, otherwise assume a lock is needed.
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int8_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int16_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int32_t), 0));

    return hasAtomic8() && __atomic_always_lock_free(sizeof(int64_t), 0);
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
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    T v;
    __atomic_load(addr, &v, __ATOMIC_SEQ_CST);
    return v;
}

template<typename T>
inline void
js::jit::AtomicOperations::storeSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    __atomic_store(addr, &val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::exchangeSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    T v;
    __atomic_exchange(addr, &val, &v, __ATOMIC_SEQ_CST);
    return v;
}

template<typename T>
inline T
js::jit::AtomicOperations::compareExchangeSeqCst(T* addr, T oldval, T newval)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    __atomic_compare_exchange(addr, &oldval, &newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return oldval;
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAddSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchSubSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    return __atomic_fetch_sub(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAndSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    return __atomic_fetch_and(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchOrSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    return __atomic_fetch_or(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchXorSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    return __atomic_fetch_xor(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::loadSafeWhenRacy(T* addr)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    T v;
    __atomic_load(addr, &v, __ATOMIC_RELAXED);
    return v;
}

template<typename T>
inline void
js::jit::AtomicOperations::storeSafeWhenRacy(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    __atomic_store(addr, &val, __ATOMIC_RELAXED);
}

inline void
js::jit::AtomicOperations::memcpySafeWhenRacy(void* dest, const void* src, size_t nbytes)
{
    MOZ_ASSERT(!((char*)dest <= (char*)src && (char*)src < (char*)dest+nbytes));
    MOZ_ASSERT(!((char*)src <= (char*)dest && (char*)dest < (char*)src+nbytes));
    memcpy(dest, src, nbytes);
}

inline void
js::jit::AtomicOperations::memmoveSafeWhenRacy(void* dest, const void* src, size_t nbytes)
{
    memmove(dest, src, nbytes);
}

template<size_t nbytes>
inline void
js::jit::RegionLock::acquire(void* addr)
{
    uint32_t zero = 0;
    uint32_t one = 1;
    while (!__atomic_compare_exchange(&spinlock, &zero, &one, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
        zero = 0;
        continue;
    }
}

template<size_t nbytes>
inline void
js::jit::RegionLock::release(void* addr)
{
    MOZ_ASSERT(AtomicOperations::loadSeqCst(&spinlock) == 1, "releasing unlocked region lock");
    uint32_t zero = 0;
    __atomic_store(&spinlock, &zero, __ATOMIC_SEQ_CST);
}

#endif // jit_arm_AtomicOperations_arm_h
