/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation, see jit/AtomicOperations.h, both the comment block at the
 * beginning and the #ifdef nest near the end.
 *
 * This is a common file for tier-3 platforms that are not providing
 * hardware-specific implementations of the atomic operations.  Please keep it
 * reasonably platform-independent by adding #ifdefs at the beginning as much as
 * possible, not throughout the file.
 *
 *
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!!!                              NOTE                                 !!!!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 * The implementations in this file are NOT SAFE and cannot be safe even in
 * principle because they rely on C++ undefined behavior.  However, they are
 * frequently good enough for tier-3 platforms.
 */

#ifndef jit_none_AtomicOperations_feeling_lucky_h
#define jit_none_AtomicOperations_feeling_lucky_h

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

// 64-bit JS atomics are not required by SpiderMonkey or the JS spec, but define
// these names if you want to provide them anyway.

#if defined(__ppc__) || defined(__PPC__)
#  define GNUC_COMPATIBLE
#endif

#if defined(__ppc64__) ||  defined (__PPC64__) || defined(__ppc64le__) || defined (__PPC64LE__)
#  define HAS_64BIT_LOCKFREE
#  define GNUC_COMPATIBLE
#endif

#ifdef __sparc__
#  define GNUC_COMPATIBLE
#  ifdef  __LP64__
#    define HAS_64BIT_ATOMICS
#    define HAS_64BIT_LOCKFREE
#  endif
#endif

#ifdef __alpha__
#  define GNUC_COMPATIBLE
#endif

#ifdef __hppa__
#  define GNUC_COMPATIBLE
#endif

#ifdef __sh__
#  define GNUC_COMPATIBLE
#endif

// The default implementation tactic for gcc/clang is to use the newer
// __atomic intrinsics added for use in C++11 <atomic>.  Where that
// isn't available, we use GCC's older __sync functions instead.
//
// ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS is kept as a backward
// compatible option for older compilers: enable this to use GCC's old
// __sync functions instead of the newer __atomic functions.  This
// will be required for GCC 4.6.x and earlier, and probably for Clang
// 3.1, should we need to use those versions.

//#define ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS


// Try to avoid platform #ifdefs below this point.

#ifdef GNUC_COMPATIBLE

inline bool
js::jit::AtomicOperations::isLockfree8()
{
# ifndef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int8_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int16_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int32_t), 0));
#  ifdef HAS_64BIT_LOCKFREE
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
    __sync_synchronize();
    T v = *addr;
    __sync_synchronize();
# else
    T v;
    __atomic_load(addr, &v, __ATOMIC_SEQ_CST);
# endif
    return v;
}

template<typename T>
inline void
js::jit::AtomicOperations::storeSeqCst(T* addr, T val)
{
    MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    __sync_synchronize();
    *addr = val;
    __sync_synchronize();
# else
    __atomic_store(addr, &val, __ATOMIC_SEQ_CST);
# endif
}

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

template<typename T>
inline T
js::jit::AtomicOperations::fetchAddSeqCst(T* addr, T val)
{
#ifndef HAS_64BIT_ATOMICS
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
#endif
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
#ifndef HAS_64BIT_ATOMICS
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
#endif
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
#ifndef HAS_64BIT_ATOMICS
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
#endif
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
#ifndef HAS_64BIT_ATOMICS
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
#endif
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
#ifndef HAS_64BIT_ATOMICS
    static_assert(sizeof(T) <= 4, "not available for 8-byte values yet");
#endif
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

template<typename T>
inline T
js::jit::AtomicOperations::exchangeSeqCst(T* addr, T val)
{
    MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    T v;
    __sync_synchronize();
    do {
	v = *addr;
    } while (__sync_val_compare_and_swap(addr, v, val) != v);
    return v;
# else
    T v;
    __atomic_exchange(addr, &val, &v, __ATOMIC_SEQ_CST);
    return v;
# endif
}

template<size_t nbytes>
inline void
js::jit::RegionLock::acquire(void* addr)
{
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    while (!__sync_bool_compare_and_swap(&spinlock, 0, 1))
        ;
# else
    uint32_t zero = 0;
    uint32_t one = 1;
    while (!__atomic_compare_exchange(&spinlock, &zero, &one, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
        zero = 0;
        continue;
    }
# endif
}

template<size_t nbytes>
inline void
js::jit::RegionLock::release(void* addr)
{
    MOZ_ASSERT(AtomicOperations::loadSeqCst(&spinlock) == 1, "releasing unlocked region lock");
# ifdef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
    __sync_sub_and_fetch(&spinlock, 1);
# else
    uint32_t zero = 0;
    __atomic_store(&spinlock, &zero, __ATOMIC_SEQ_CST);
# endif
}

#elif defined(ENABLE_SHARED_ARRAY_BUFFER)

# error "Either disable JS shared memory, use GCC or Clang, or add code here"

#endif

#undef ATOMICS_IMPLEMENTED_WITH_SYNC_INTRINSICS
#undef GNUC_COMPATIBLE
#undef HAS_64BIT_ATOMICS
#undef HAS_64BIT_LOCKFREE

#endif // jit_none_AtomicOperations_feeling_lucky_h
