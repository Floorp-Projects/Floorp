/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_InlinableNatives_h
#define jit_InlinableNatives_h

#define INLINABLE_NATIVE_LIST(_)    \
    _(MathAbs)                      \
    _(MathFloor)                    \
    _(MathCeil)                     \
    _(MathRound)                    \
    _(MathClz32)                    \
    _(MathSqrt)                     \
    _(MathATan2)                    \
    _(MathHypot)                    \
    _(MathMax)                      \
    _(MathMin)                      \
    _(MathPow)                      \
    _(MathRandom)                   \
    _(MathImul)                     \
    _(MathFRound)                   \
    _(MathSin)                      \
    _(MathTan)                      \
    _(MathCos)                      \
    _(MathExp)                      \
    _(MathLog)                      \
    _(MathASin)                     \
    _(MathATan)                     \
    _(MathACos)                     \
    _(MathLog10)                    \
    _(MathLog2)                     \
    _(MathLog1P)                    \
    _(MathExpM1)                    \
    _(MathSinH)                     \
    _(MathTanH)                     \
    _(MathCosH)                     \
    _(MathASinH)                    \
    _(MathATanH)                    \
    _(MathACosH)                    \
    _(MathSign)                     \
    _(MathTrunc)                    \
    _(MathCbrt)

struct JSJitInfo;

namespace js {
namespace jit {

enum class InlinableNative : uint16_t {
#define ADD_NATIVE(native) native,
    INLINABLE_NATIVE_LIST(ADD_NATIVE)
#undef ADD_NATIVE
};

#define ADD_NATIVE(native) extern const JSJitInfo JitInfo_##native;
    INLINABLE_NATIVE_LIST(ADD_NATIVE)
#undef ADD_NATIVE

} // namespace jit
} // namespace js

#endif /* jit_InlinableNatives_h */
