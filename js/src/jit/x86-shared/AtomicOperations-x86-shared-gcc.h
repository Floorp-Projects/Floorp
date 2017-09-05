/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For overall documentation, see jit/AtomicOperations.h */

#ifndef jit_shared_AtomicOperations_x86_shared_gcc_h
#define jit_shared_AtomicOperations_x86_shared_gcc_h

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

#include "vm/ArrayBufferObject.h"

#if !defined(__clang__) && !defined(__GNUC__)
# error "This file only for gcc-compatible compilers"
#endif

// Lock-freedom and access-atomicity on x86 and x64.
//
// In general, aligned accesses are access-atomic up to 8 bytes ever since the
// Pentium; Firefox requires SSE2, which was introduced with the Pentium 4, so
// we may assume access-atomicity.
//
// Four-byte accesses and smaller are simple:
//  - Use MOV{B,W,L} to load and store.  Stores require a post-fence
//    for sequential consistency as defined by the JS spec.  The fence
//    can be MFENCE, or the store can be implemented using XCHG.
//  - For compareExchange use LOCK; CMPXCGH{B,W,L}
//  - For exchange, use XCHG{B,W,L}
//  - For add, etc use LOCK; ADD{B,W,L} etc
//
// Eight-byte accesses are easy on x64:
//  - Use MOVQ to load and store (again with a fence for the store)
//  - For compareExchange, we use CMPXCHGQ
//  - For exchange, we use XCHGQ
//  - For add, etc use LOCK; ADDQ etc
//
// Eight-byte accesses are harder on x86:
//  - For load, use a sequence of MOVL + CMPXCHG8B
//  - For store, use a sequence of MOVL + a CMPXCGH8B in a loop,
//    no additional fence required
//  - For exchange, do as for store
//  - For add, etc do as for store

// Firefox requires gcc > 4.8, so we will always have the __atomic intrinsics
// added for use in C++11 <atomic>.
//
// Note that using these intrinsics for most operations is not correct: the code
// has undefined behavior.  The gcc documentation states that the compiler
// assumes the code is race free.  This supposedly means C++ will allow some
// instruction reorderings (effectively those allowed by TSO) even for seq_cst
// ordered operations, but these reorderings are not allowed by JS.  To do
// better we will end up with inline assembler or JIT-generated code.

// For now, we require that the C++ compiler's atomics are lock free, even for
// 64-bit accesses.

// When compiling with Clang on 32-bit linux it will be necessary to link with
// -latomic to get the proper 64-bit intrinsics.

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
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int64_t), 0));
    return true;
}

inline void
js::jit::AtomicOperations::fenceSeqCst()
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

template<typename T>
static bool
IsAligned(const T* addr)
{
    return (uintptr_t(addr) & (sizeof(T) - 1)) == 0;
}

template<typename T>
inline T
js::jit::AtomicOperations::loadSeqCst(T* addr)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    MOZ_ASSERT(IsAligned(addr));
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
    MOZ_ASSERT(IsAligned(addr));
    __atomic_store(addr, &val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::exchangeSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    MOZ_ASSERT(IsAligned(addr));
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
    MOZ_ASSERT(IsAligned(addr));
    __atomic_compare_exchange(addr, &oldval, &newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return oldval;
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAddSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    MOZ_ASSERT(IsAligned(addr));
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchSubSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    MOZ_ASSERT(IsAligned(addr));
    return __atomic_fetch_sub(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAndSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    MOZ_ASSERT(IsAligned(addr));
    return __atomic_fetch_and(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchOrSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    MOZ_ASSERT(IsAligned(addr));
    return __atomic_fetch_or(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchXorSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    MOZ_ASSERT(IsAligned(addr));
    return __atomic_fetch_xor(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::loadSafeWhenRacy(T* addr)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    MOZ_ASSERT(IsAligned(addr));
    T v;
    __atomic_load(addr, &v, __ATOMIC_RELAXED);
    return v;
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

} }

template<typename T>
inline void
js::jit::AtomicOperations::storeSafeWhenRacy(T* addr, T val)
{
    static_assert(sizeof(T) <= 8, "atomics supported up to 8 bytes only");
    MOZ_ASSERT_IF(sizeof(T) == 8, isLockfree8());
    MOZ_ASSERT(IsAligned(addr));
    __atomic_store(addr, &val, __ATOMIC_RELAXED);
}

namespace js { namespace jit {

template<>
inline void
js::jit::AtomicOperations::storeSafeWhenRacy(uint8_clamped* addr, uint8_clamped val)
{
    __atomic_store(&addr->val, &val.val, __ATOMIC_RELAXED);
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

template<size_t nbytes>
inline void
js::jit::RegionLock::acquire(void* addr)
{
    uint32_t zero = 0;
    uint32_t one = 1;
    while (!__atomic_compare_exchange(&spinlock, &zero, &one, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
        zero = 0;
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

#endif // jit_shared_AtomicOperations_x86_shared_gcc_h
