/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is an internal atomic implementation, use
// base/atomicops.h instead.
//
// This is a very slow fallback implementation of atomic operations
// that uses a mutex instead of atomic instructions.
//
// (NB: a small "optimization" here would be using a spinlock instead
// of a blocking mutex, but it's probably not worth the time.)

#ifndef base_atomicops_internals_mutex_h
#define base_atomicops_internals_mutex_h

#include "base/lock.h"

namespace base {
namespace subtle {

extern Lock gAtomicsMutex;

template<typename T>
T Locked_CAS(volatile T* ptr, T old_value, T new_value) {
  AutoLock _(gAtomicsMutex);

  T current_value = *ptr;
  if (current_value == old_value)
    *ptr = new_value;
  
  return current_value;
}

template<typename T>
T Locked_AtomicExchange(volatile T* ptr, T new_value) {
  AutoLock _(gAtomicsMutex);

  T current_value = *ptr;
  *ptr = new_value;
  return current_value;
}

template<typename T>
T Locked_AtomicIncrement(volatile T* ptr, T increment) {
  AutoLock _(gAtomicsMutex);
  return *ptr += increment;
}

template<typename T>
void Locked_Store(volatile T* ptr, T value) {
  AutoLock _(gAtomicsMutex);
  *ptr = value;
}

template<typename T>
T Locked_Load(volatile const T* ptr) {
  AutoLock _(gAtomicsMutex);
  return *ptr;
}

inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  return Locked_CAS(ptr, old_value, new_value);
}

inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  return Locked_AtomicExchange(ptr, new_value);
}

inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  return Locked_AtomicIncrement(ptr, increment);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return Locked_AtomicIncrement(ptr, increment);
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  return Locked_CAS(ptr, old_value, new_value);
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  return Locked_CAS(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  return Locked_Store(ptr, value);
}

inline void MemoryBarrier() {
  AutoLock _(gAtomicsMutex);
  // lock/unlock work as a barrier here
}

inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  return Locked_Store(ptr, value);
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  return Locked_Store(ptr, value);
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return Locked_Load(ptr);
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  return NoBarrier_Load(ptr);
}

inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  return Locked_Load(ptr);
}

#ifdef ARCH_CPU_64_BITS

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  return Locked_CAS(ptr, old_value, new_value);
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  return Locked_AtomicExchange(ptr, new_value);
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  return Locked_AtomicIncrement(ptr, increment);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return Locked_AtomicIncrement(ptr, increment);
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  return Locked_Store(ptr, value);
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  return Locked_CAS(ptr, old_value, new_value);
}

inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
  return Locked_Store(ptr, value);
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  return Locked_Store(ptr, value);
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return Locked_Load(ptr);
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  return Locked_Load(ptr);
}

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  return Locked_Load(ptr);
}

#endif  // ARCH_CPU_64_BITS

#ifdef OS_MACOSX
// From atomicops_internals_x86_macosx.h:
//
//   MacOS uses long for intptr_t, AtomicWord and Atomic32 are always
//   different on the Mac, even when they are the same size.  We need
//   to explicitly cast from AtomicWord to Atomic32/64 to implement
//   the AtomicWord interface.

inline AtomicWord NoBarrier_CompareAndSwap(volatile AtomicWord* ptr,
                                           AtomicWord old_value,
                                           AtomicWord new_value) {
  return Locked_CAS(ptr, old_value, new_value);
}

inline AtomicWord NoBarrier_AtomicExchange(volatile AtomicWord* ptr,
                                           AtomicWord new_value) {
  return Locked_AtomicExchange(ptr, new_value);
}

inline AtomicWord NoBarrier_AtomicIncrement(volatile AtomicWord* ptr,
                                            AtomicWord increment) {
  return Locked_AtomicIncrement(ptr, increment);
}

inline AtomicWord Barrier_AtomicIncrement(volatile AtomicWord* ptr,
                                          AtomicWord increment) {
  return Locked_AtomicIncrement(ptr, increment);
}

inline AtomicWord Acquire_CompareAndSwap(volatile AtomicWord* ptr,
                                         AtomicWord old_value,
                                         AtomicWord new_value) {
  return Locked_CAS(ptr, old_value, new_value);
}

inline AtomicWord Release_CompareAndSwap(volatile AtomicWord* ptr,
                                         AtomicWord old_value,
                                         AtomicWord new_value) {
  return Locked_CAS(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile AtomicWord *ptr, AtomicWord value) {
  return Locked_Store(ptr, value);
}

inline void Acquire_Store(volatile AtomicWord* ptr, AtomicWord value) {
  return Locked_Store(ptr, value);
}

inline void Release_Store(volatile AtomicWord* ptr, AtomicWord value) {
  return Locked_Store(ptr, value);
}

inline AtomicWord NoBarrier_Load(volatile const AtomicWord *ptr) {
  return Locked_Load(ptr);
}

inline AtomicWord Acquire_Load(volatile const AtomicWord* ptr) {
  return Locked_Load(ptr);
}

inline AtomicWord Release_Load(volatile const AtomicWord* ptr) {
  return Locked_Load(ptr);
}

#endif  // OS_MACOSX

}  // namespace subtle
}  // namespace base

#endif  // base_atomicops_internals_mutex_h
