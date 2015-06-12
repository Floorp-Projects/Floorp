/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation, see jit/AtomicOperations.h */

#ifndef jit_arm64_AtomicOperations_arm64_h
#define jit_arm64_AtomicOperations_arm64_h

#include "jit/arm64/Architecture-arm64.h"
#include "jit/AtomicOperations.h"

inline bool
js::jit::AtomicOperations::isLockfree8()
{
    MOZ_CRASH("isLockfree8()");
}

inline void
js::jit::AtomicOperations::fenceSeqCst()
{
    MOZ_CRASH("fenceSeqCst()");
}

template<typename T>
inline T
js::jit::AtomicOperations::loadSeqCst(T* addr)
{
    MOZ_CRASH("loadSeqCst()");
}

template<typename T>
inline void
js::jit::AtomicOperations::storeSeqCst(T* addr, T val)
{
    MOZ_CRASH("storeSeqCst()");
}

template<typename T>
inline T
js::jit::AtomicOperations::exchangeSeqCst(T* addr, T val)
{
    MOZ_CRASH("exchangeSeqCst()");
}

template<typename T>
inline T
js::jit::AtomicOperations::compareExchangeSeqCst(T* addr, T oldval, T newval)
{
    MOZ_CRASH("compareExchangeSeqCst()");
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAddSeqCst(T* addr, T val)
{
    MOZ_CRASH("fetchAddSeqCst()");
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchSubSeqCst(T* addr, T val)
{
    MOZ_CRASH("fetchSubSeqCst()");
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAndSeqCst(T* addr, T val)
{
    MOZ_CRASH("fetchAndSeqCst()");
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchOrSeqCst(T* addr, T val)
{
    MOZ_CRASH("fetchOrSeqCst()");
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchXorSeqCst(T* addr, T val)
{
    MOZ_CRASH("fetchXorSeqCst()");
}

template<size_t nbytes>
inline void
js::jit::RegionLock::acquire(void* addr)
{
    MOZ_CRASH("acquire()");
}

template<size_t nbytes>
inline void
js::jit::RegionLock::release(void* addr)
{
    MOZ_CRASH("release()");
}

#endif // jit_arm64_AtomicOperations_arm64_h
