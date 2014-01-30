/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitCommon_h
#define jit_JitCommon_h

// Various macros used by all JITs, including YARR.

#ifdef JS_ARM_SIMULATOR
#include "jit/arm/Simulator-arm.h"

// Call into cross-jitted code by following the ABI of the simulated architecture.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4, p5, p6, p7)     \
    (js::jit::Simulator::Current()->call(                              \
        JS_FUNC_TO_DATA_PTR(uint8_t *, entry), 8, p0, p1, p2, p3, p4, p5, p6, p7) & 0xffffffff)

#define CALL_GENERATED_YARR_CODE3(entry, p0, p1, p2)     \
    js::jit::Simulator::Current()->call(JS_FUNC_TO_DATA_PTR(uint8_t *, entry), 3, p0, p1, p2)

#define CALL_GENERATED_YARR_CODE4(entry, p0, p1, p2, p3) \
    js::jit::Simulator::Current()->call(JS_FUNC_TO_DATA_PTR(uint8_t *, entry), 4, p0, p1, p2, p3)

#define CALL_GENERATED_ASMJS(entry, p0, p1)              \
    (Simulator::Current()->call(JS_FUNC_TO_DATA_PTR(uint8_t *, entry), 2, p0, p1) & 0xffffffff)

#else

// Call into jitted code by following the ABI of the native architecture.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4, p5, p6, p7)   \
  entry(p0, p1, p2, p3, p4, p5, p6, p7)

#define CALL_GENERATED_YARR_CODE3(entry, p0, p1, p2)                 \
  entry(p0, p1, p2)

#define CALL_GENERATED_YARR_CODE4(entry, p0, p1, p2, p3)             \
  entry(p0, p1, p2, p3)

#define CALL_GENERATED_ASMJS(entry, p0, p1)                          \
  entry(p0, p1)

#endif

#endif // jit_JitCommon_h
