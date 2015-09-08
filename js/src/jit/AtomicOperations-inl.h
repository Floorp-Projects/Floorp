/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AtomicOperations_inl_h
#define jit_AtomicOperations_inl_h

#if defined(JS_CODEGEN_ARM)
# include "jit/arm/AtomicOperations-arm.h"
#elif defined(JS_CODEGEN_ARM64)
# include "jit/arm64/AtomicOperations-arm64.h"
#elif defined(JS_CODEGEN_MIPS32)
# include "jit/mips-shared/AtomicOperations-mips-shared.h"
#elif defined(JS_CODEGEN_NONE)
# include "jit/none/AtomicOperations-none.h"
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
# include "jit/x86-shared/AtomicOperations-x86-shared.h"
#else
# error "Atomic operations must be defined for this platform"
#endif

inline bool
js::jit::AtomicOperations::isLockfree(int32_t size)
{
    // Keep this in sync with visitAtomicIsLockFree() in jit/CodeGenerator.cpp.

    switch (size) {
      case 1:
      case 2:
      case 4:
        return true;
      case 8:
        return AtomicOperations::isLockfree8();
      default:
        return false;
    }
}

#endif // jit_AtomicOperations_inl_h
