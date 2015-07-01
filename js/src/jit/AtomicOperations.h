/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AtomicOperations_h
#define jit_AtomicOperations_h

namespace js {
namespace jit {

/*
 * The atomic operations layer defines the following types and
 * functions.  The "SeqCst" suffix on operations means "sequentially
 * consistent" and means such a function's operation must have
 * "sequentially consistent" memory ordering.  See mfbt/Atomics.h for
 * an explanation of this memory ordering.
 *
 * To make use of the functions you generally have to include
 * AtomicOperations-inl.h.
 *
 * The fundamental constraint on all these primitives is that their
 * realization by the C++ compiler MUST be compatible with code the
 * JIT generates for its Atomics operations, so that an atomic access
 * from the interpreter really is atomic relative to a concurrent
 * access from jitted code.
 *
 * It's not a requirement that these methods be inlined; performance
 * is not a great concern.  On some platforms these methods may call
 * out to code that's generated at run time.
 */

class AtomicOperations
{
  public:

    // Test lock-freedom for any integer value.
    //
    // This implements a platform-independent pattern, as follows:
    //
    // 1, 2, and 4 bytes are always lock free, lock-freedom for 8
    // bytes is determined by the platform's isLockfree8(), and there
    // is no lock-freedom for any other values on any platform.
    static inline bool isLockfree(int32_t n);

    // If the return value is true then a call to the 64-bit (8-byte)
    // routines below will work, otherwise those functions will assert in
    // debug builds and may crash in release build.  (See the code in
    // ../arm for an example.)  The value of this call does not change
    // during a run.
    static inline bool isLockfree8();

    // Execute a full memory barrier (LoadLoad+LoadStore+StoreLoad+StoreStore).
    static inline void fenceSeqCst();

    // The following functions are defined for T = int8_t, uint8_t,
    // int16_t, uint16_t, int32_t, uint32_t, int64_t, and uint64_t

    // Atomically read* addr.
    template<typename T>
    static inline T loadSeqCst(T* addr);

    // Atomically store val in *addr.
    template<typename T>
    static inline void storeSeqCst(T* addr, T val);

    // Atomically store val in *addr and return the old value of *addr.
    template<typename T>
    static inline T exchangeSeqCst(T* addr, T val);

    // Atomically check that *addr contains oldval and if so replace it
    // with newval, in any case return the old contents of *addr
    template<typename T>
    static inline T compareExchangeSeqCst(T* addr, T oldval, T newval);

    // The following functions are defined for T = int8_t, uint8_t,
    // int16_t, uint16_t, int32_t, uint32_t only.

    // Atomically add, subtract, bitwise-AND, bitwise-OR, or bitwise-XOR
    // val into *addr and return the old value of *addr.
    template<typename T>
    static inline T fetchAddSeqCst(T* addr, T val);

    template<typename T>
    static inline T fetchSubSeqCst(T* addr, T val);

    template<typename T>
    static inline T fetchAndSeqCst(T* addr, T val);

    template<typename T>
    static inline T fetchOrSeqCst(T* addr, T val);

    template<typename T>
    static inline T fetchXorSeqCst(T* addr, T val);
};

/* A data type representing a lock on some region of a
 * SharedArrayRawBuffer's memory, to be used only when the hardware
 * does not provide necessary atomicity (eg, float64 access on ARMv6
 * and some ARMv7 systems).
 */
struct RegionLock
{
  public:
    RegionLock() : spinlock(0) {}

    /* Addr is the address to be locked, nbytes the number of bytes we
     * need to lock.  The lock that is taken may cover a larger range
     * of bytes.
     */
    template<size_t nbytes>
    void acquire(void* addr);

    /* Addr is the address to be unlocked, nbytes the number of bytes
     * we need to unlock.  The lock must be held by the calling thread,
     * at the given address and for the number of bytes.
     */
    template<size_t nbytes>
    void release(void* addr);

  private:
    /* For now, a simple spinlock that covers the entire buffer. */
    uint32_t spinlock;
};

} // namespace jit
} // namespace js

#endif // jit_AtomicOperations_h
