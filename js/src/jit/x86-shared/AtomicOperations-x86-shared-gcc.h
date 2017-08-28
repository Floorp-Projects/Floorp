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

#if !defined(__clang__) && !defined(__GNUC__)
# error "This file only for gcc-compatible compilers"
#endif

// Lock-freedom and access-atomicity on x86 and x64.
//
// In general, aligned accesses are access-atomic up to 8 bytes since the
// Pentium; we require SSE2, which was introduced with the Pentium 4, so we may
// assume access-atomicity.
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

// The default implementation tactic for gcc/clang is to use the newer
// __atomic intrinsics added for use in C++11 <atomic>.  Where that
// isn't available, we use GCC's older __sync functions instead.
//
// ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS is kept as a backward
// compatible option for older compilers: enable this to use GCC's old
// __sync functions instead of the newer __atomic functions.  This
// will be required for GCC 4.6.x and earlier, and probably for Clang
// 3.1, should we need to use those versions.

// #define ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS

// Lock-free 8-byte atomics are assumed on x86 but must be disabled in
// corner cases, see comments below and in isLockfree8().

# define LOCKFREE8

// This pertains to Clang compiling with -m32, in this case the 64-bit
// __atomic builtins are not available (observed on various Mac OS X
// versions with Apple Clang and on Linux with Clang 3.5).
//
// For now just punt: disable lock-free 8-word data.  The JIT will
// call isLockfree8() to determine what to do and will stay in sync.
// (Bug 1146817 tracks the work to improve on this.)

# if defined(__clang__) && defined(__i386)
#  undef LOCKFREE8
# endif

inline bool
js::jit::AtomicOperations::isLockfree8()
{
# ifndef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int8_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int16_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int32_t), 0));
# endif
# ifdef LOCKFREE8
#  ifndef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int64_t), 0));
#  endif
    return true;
# else
    return false;
# endif
}

inline void
js::jit::AtomicOperations::fenceSeqCst()
{
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    __sync_synchronize();
# else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
# endif
}

template<typename T>
inline T
js::jit::AtomicOperations::loadSeqCst(T* addr)
{
    MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    // Inhibit compiler reordering with a volatile load.  The x86 does
    // not reorder loads with respect to subsequent loads or stores
    // and no ordering barrier is required here.  See more elaborate
    // comments in storeSeqCst.
    T v = *static_cast<T volatile*>(addr);
# else
    T v;
    __atomic_load(addr, &v, __ATOMIC_SEQ_CST);
# endif
    return v;
}

# ifndef LOCKFREE8
template<>
inline int64_t
js::jit::AtomicOperations::loadSeqCst(int64_t* addr)
{
    MOZ_CRASH();
}

template<>
inline uint64_t
js::jit::AtomicOperations::loadSeqCst(uint64_t* addr)
{
    MOZ_CRASH();
}
# endif // LOCKFREE8

template<typename T>
inline void
js::jit::AtomicOperations::storeSeqCst(T* addr, T val)
{
    MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    // Inhibit compiler reordering with a volatile store.  The x86 may
    // reorder a store with respect to a subsequent load from a
    // different location, hence there is an ordering barrier here to
    // prevent that.
    //
    // By way of background, look to eg
    // http://bartoszmilewski.com/2008/11/05/who-ordered-memory-fences-on-an-x86/
    //
    // Consider:
    //
    //   uint8_t x = 0, y = 0; // to start
    //
    // thread1:
    //   sx: AtomicOperations::store(&x, 1);
    //   gy: uint8_t obs1 = AtomicOperations::loadSeqCst(&y);
    //
    // thread2:
    //   sy: AtomicOperations::store(&y, 1);
    //   gx: uint8_t obs2 = AtomicOperations::loadSeqCst(&x);
    //
    // Sequential consistency requires a total global ordering of
    // operations: sx-gy-sy-gx, sx-sy-gx-gy, sx-sy-gy-gx, sy-gx-sx-gy,
    // sy-sx-gy-gx, or sy-sx-gx-gy.  In every ordering at least one of
    // sx-before-gx or sy-before-gy happens, so *at least one* of
    // obs1/obs2 is 1.
    //
    // If AtomicOperations::{load,store}SeqCst were just volatile
    // {load,store}, x86 could reorder gx/gy before each thread's
    // prior load.  That would permit gx-gy-sx-sy: both loads would be
    // 0!  Thus after a volatile store we must synchronize to ensure
    // the store happens before the load.
    *static_cast<T volatile*>(addr) = val;
    __sync_synchronize();
# else
    __atomic_store(addr, &val, __ATOMIC_SEQ_CST);
# endif
}

# ifndef LOCKFREE8
template<>
inline void
js::jit::AtomicOperations::storeSeqCst(int64_t* addr, int64_t val)
{
    MOZ_CRASH();
}

template<>
inline void
js::jit::AtomicOperations::storeSeqCst(uint64_t* addr, uint64_t val)
{
    MOZ_CRASH();
}
# endif // LOCKFREE8

template<typename T>
inline T
js::jit::AtomicOperations::exchangeSeqCst(T* addr, T val)
{
    MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    T v;
    do {
        // Here I assume the compiler will not hoist the load.  It
        // shouldn't, because the CAS could affect* addr.
        v = *addr;
    } while (!__sync_bool_compare_and_swap(addr, v, val));
    return v;
# else
    T v;
    __atomic_exchange(addr, &val, &v, __ATOMIC_SEQ_CST);
    return v;
# endif
}

# ifndef LOCKFREE8
template<>
inline int64_t
js::jit::AtomicOperations::exchangeSeqCst(int64_t* addr, int64_t val)
{
    MOZ_CRASH();
}

template<>
inline uint64_t
js::jit::AtomicOperations::exchangeSeqCst(uint64_t* addr, uint64_t val)
{
    MOZ_CRASH();
}
# endif // LOCKFREE8

template<typename T>
inline T
js::jit::AtomicOperations::compareExchangeSeqCst(T* addr, T oldval, T newval)
{
    MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    return __sync_val_compare_and_swap(addr, oldval, newval);
# else
    __atomic_compare_exchange(addr, &oldval, &newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return oldval;
# endif
}

# ifndef LOCKFREE8
template<>
inline int64_t
js::jit::AtomicOperations::compareExchangeSeqCst(int64_t* addr, int64_t oldval, int64_t newval)
{
    MOZ_CRASH();
}

template<>
inline uint64_t
js::jit::AtomicOperations::compareExchangeSeqCst(uint64_t* addr, uint64_t oldval, uint64_t newval)
{
    MOZ_CRASH();
}
# endif // LOCKFREE8

template<typename T>
inline T
js::jit::AtomicOperations::fetchAddSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    return __sync_fetch_and_add(addr, val);
# else
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
# endif
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchSubSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    return __sync_fetch_and_sub(addr, val);
# else
    return __atomic_fetch_sub(addr, val, __ATOMIC_SEQ_CST);
# endif
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAndSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    return __sync_fetch_and_and(addr, val);
# else
    return __atomic_fetch_and(addr, val, __ATOMIC_SEQ_CST);
# endif
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchOrSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    return __sync_fetch_and_or(addr, val);
# else
    return __atomic_fetch_or(addr, val, __ATOMIC_SEQ_CST);
# endif
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchXorSeqCst(T* addr, T val)
{
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    return __sync_fetch_and_xor(addr, val);
# else
    return __atomic_fetch_xor(addr, val, __ATOMIC_SEQ_CST);
# endif
}

template<typename T>
inline T
js::jit::AtomicOperations::loadSafeWhenRacy(T* addr)
{
    return *addr;               // FIXME (1208663): not yet safe
}

template<typename T>
inline void
js::jit::AtomicOperations::storeSafeWhenRacy(T* addr, T val)
{
    *addr = val;                // FIXME (1208663): not yet safe
}

inline void
js::jit::AtomicOperations::memcpySafeWhenRacy(void* dest, const void* src, size_t nbytes)
{
    MOZ_ASSERT(!((char*)dest <= (char*)src && (char*)src < (char*)dest+nbytes));
    MOZ_ASSERT(!((char*)src <= (char*)dest && (char*)dest < (char*)src+nbytes));
    ::memcpy(dest, src, nbytes); // FIXME (1208663): not yet safe
}

inline void
js::jit::AtomicOperations::memmoveSafeWhenRacy(void* dest, const void* src, size_t nbytes)
{
    ::memmove(dest, src, nbytes); // FIXME (1208663): not yet safe
}

template<size_t nbytes>
inline void
js::jit::RegionLock::acquire(void* addr)
{
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    while (!__sync_bool_compare_and_swap(&spinlock, 0, 1))
        continue;
# else
    uint32_t zero = 0;
    uint32_t one = 1;
    while (!__atomic_compare_exchange(&spinlock, &zero, &one, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
        zero = 0;
    }
# endif
}

template<size_t nbytes>
inline void
js::jit::RegionLock::release(void* addr)
{
    MOZ_ASSERT(AtomicOperations::loadSeqCst(&spinlock) == 1, "releasing unlocked region lock");
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    __sync_sub_and_fetch(&spinlock, 1); // Should turn into LOCK XADD
# else
    uint32_t zero = 0;
    __atomic_store(&spinlock, &zero, __ATOMIC_SEQ_CST);
# endif
}

# undef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
# undef LOCKFREE8

#endif // jit_shared_AtomicOperations_x86_shared_gcc_h
