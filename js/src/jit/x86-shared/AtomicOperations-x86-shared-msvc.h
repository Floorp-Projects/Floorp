/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For overall documentation, see jit/AtomicOperations.h */

#ifndef jit_shared_AtomicOperations_x86_shared_msvc_h
#define jit_shared_AtomicOperations_x86_shared_msvc_h

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

#if !defined(_MSC_VER)
# error "This file only for Microsoft Visual C++"
#endif

// For general comments on lock-freedom, access-atomicity, and related matters
// on x86 and x64, see the comment block in AtomicOperations-x86-shared-gcc.h.

// Below, _ReadWriteBarrier is a compiler directive, preventing reordering of
// instructions and reuse of memory values across it.

inline bool
js::jit::AtomicOperations::isLockfree8()
{
    // See general comments at the start of this file.
    //
    // The MSDN docs suggest very strongly that if code is compiled for Pentium
    // or better the 64-bit primitives will be lock-free, see eg the "Remarks"
    // secion of the page for _InterlockedCompareExchange64, currently here:
    // https://msdn.microsoft.com/en-us/library/ttk2z1ws%28v=vs.85%29.aspx
    //
    // But I've found no way to assert that at compile time or run time, there
    // appears to be no WinAPI is_lock_free() test.

    return true;
}

inline void
js::jit::AtomicOperations::fenceSeqCst()
{
    _ReadWriteBarrier();
# ifdef _M_IX86
    // If configured for SSE2+ we can use the MFENCE instruction, available
    // through the _mm_mfence intrinsic.  But for non-SSE2 systems we have to do
    // something else.  Linux uses "lock add [esp], 0", so why not?
    //
    // TODO: I think SSE2+ is assumed now.
    __asm lock add [esp], 0;
# else
    _mm_mfence();
# endif
}

template<typename T>
inline T
js::jit::AtomicOperations::loadSeqCst(T* addr)
{
    MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());
    _ReadWriteBarrier();
    T v = *addr;
    _ReadWriteBarrier();
    return v;
}

template<typename T>
inline void
js::jit::AtomicOperations::storeSeqCst(T* addr, T val)
{
    MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());
    _ReadWriteBarrier();
    *addr = val;
    fenceSeqCst();
}

// On 32-bit CPUs there is no 64-bit XCHG instruction, one must instead use a
// loop with CMPXCHG8B.  Since MSVC provides _InterlockedExchange64 only if it
// maps directly to XCHG, the workaround must be manual.

# define MSC_EXCHANGEOP(T, U, xchgop)                           \
    template<> inline T                                         \
    js::jit::AtomicOperations::exchangeSeqCst(T* addr, T val) { \
        MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());        \
        return (T)xchgop((U volatile*)addr, (U)val);            \
    }

# define MSC_EXCHANGEOP_CAS(T, U, cmpxchg)                           \
    template<> inline T                                              \
    js::jit::AtomicOperations::exchangeSeqCst(T* addr, T newval) {   \
        MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());             \
        T oldval;                                                    \
        do {                                                         \
            _ReadWriteBarrier();                                     \
            oldval = *addr;                                          \
        } while (!cmpxchg((U volatile*)addr, (U)newval, (U)oldval)); \
        return oldval;                                               \
    }

MSC_EXCHANGEOP(int8_t, char, _InterlockedExchange8)
MSC_EXCHANGEOP(uint8_t, char, _InterlockedExchange8)
MSC_EXCHANGEOP(int16_t, short, _InterlockedExchange16)
MSC_EXCHANGEOP(uint16_t, short, _InterlockedExchange16)
MSC_EXCHANGEOP(int32_t, long, _InterlockedExchange)
MSC_EXCHANGEOP(uint32_t, long, _InterlockedExchange)
# ifdef _M_X64
MSC_EXCHANGEOP(int64_t, __int64, _InterlockedExchange64)
MSC_EXCHANGEOP(uint64_t, __int64, _InterlockedExchange64)
# else
MSC_EXCHANGEOP_CAS(int64_t, __int64, _InterlockedCompareExchange64)
MSC_EXCHANGEOP_CAS(uint64_t, __int64, _InterlockedCompareExchange64)
# endif

# undef MSC_EXCHANGEOP
# undef MSC_EXCHANGEOP_CAS

# define MSC_CAS(T, U, cmpxchg)                                                     \
    template<> inline T                                                             \
    js::jit::AtomicOperations::compareExchangeSeqCst(T* addr, T oldval, T newval) { \
        MOZ_ASSERT(sizeof(T) < 8 || isLockfree8());                            \
        return (T)cmpxchg((U volatile*)addr, (U)newval, (U)oldval);                 \
    }

MSC_CAS(int8_t, char, _InterlockedCompareExchange8)
MSC_CAS(uint8_t, char, _InterlockedCompareExchange8)
MSC_CAS(int16_t, short, _InterlockedCompareExchange16)
MSC_CAS(uint16_t, short, _InterlockedCompareExchange16)
MSC_CAS(int32_t, long, _InterlockedCompareExchange)
MSC_CAS(uint32_t, long, _InterlockedCompareExchange)
MSC_CAS(int64_t, __int64, _InterlockedCompareExchange64)
MSC_CAS(uint64_t, __int64, _InterlockedCompareExchange64)

# undef MSC_CAS

# define MSC_FETCHADDOP(T, U, xadd)                                           \
    template<> inline T                                                       \
    js::jit::AtomicOperations::fetchAddSeqCst(T* addr, T val) {               \
        static_assert(sizeof(T) <= 4, "not available for 8-byte values yet"); \
        return (T)xadd((U volatile*)addr, (U)val);                            \
    }                                                                         \
    template<> inline T                                                       \
    js::jit::AtomicOperations::fetchSubSeqCst(T* addr, T val) {               \
        static_assert(sizeof(T) <= 4, "not available for 8-byte values yet"); \
        return (T)xadd((U volatile*)addr, -(U)val);                           \
    }

MSC_FETCHADDOP(int8_t, char, _InterlockedExchangeAdd8)
MSC_FETCHADDOP(uint8_t, char, _InterlockedExchangeAdd8)
MSC_FETCHADDOP(int16_t, short, _InterlockedExchangeAdd16)
MSC_FETCHADDOP(uint16_t, short, _InterlockedExchangeAdd16)
MSC_FETCHADDOP(int32_t, long, _InterlockedExchangeAdd)
MSC_FETCHADDOP(uint32_t, long, _InterlockedExchangeAdd)

# undef MSC_FETCHADDOP

# define MSC_FETCHBITOP(T, U, andop, orop, xorop)                             \
    template<> inline T                                                       \
    js::jit::AtomicOperations::fetchAndSeqCst(T* addr, T val) {               \
        static_assert(sizeof(T) <= 4, "not available for 8-byte values yet"); \
        return (T)andop((U volatile*)addr, (U)val);                           \
    }                                                                         \
    template<> inline T                                                       \
    js::jit::AtomicOperations::fetchOrSeqCst(T* addr, T val) {                \
        static_assert(sizeof(T) <= 4, "not available for 8-byte values yet"); \
        return (T)orop((U volatile*)addr, (U)val);                            \
    }                                                                         \
    template<> inline T                                                       \
    js::jit::AtomicOperations::fetchXorSeqCst(T* addr, T val) {               \
        static_assert(sizeof(T) <= 4, "not available for 8-byte values yet"); \
        return (T)xorop((U volatile*)addr, (U)val);                           \
    }

MSC_FETCHBITOP(int8_t, char, _InterlockedAnd8, _InterlockedOr8, _InterlockedXor8)
MSC_FETCHBITOP(uint8_t, char, _InterlockedAnd8, _InterlockedOr8, _InterlockedXor8)
MSC_FETCHBITOP(int16_t, short, _InterlockedAnd16, _InterlockedOr16, _InterlockedXor16)
MSC_FETCHBITOP(uint16_t, short, _InterlockedAnd16, _InterlockedOr16, _InterlockedXor16)
MSC_FETCHBITOP(int32_t, long,  _InterlockedAnd, _InterlockedOr, _InterlockedXor)
MSC_FETCHBITOP(uint32_t, long, _InterlockedAnd, _InterlockedOr, _InterlockedXor)

# undef MSC_FETCHBITOP

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
    while (_InterlockedCompareExchange((long*)&spinlock, /*newval=*/1, /*oldval=*/0) == 1)
        continue;
}

template<size_t nbytes>
inline void
js::jit::RegionLock::release(void* addr)
{
    MOZ_ASSERT(AtomicOperations::loadSeqCst(&spinlock) == 1, "releasing unlocked region lock");
    _InterlockedExchange((long*)&spinlock, 0);
}

#endif // jit_shared_AtomicOperations_x86_shared_msvc_h
