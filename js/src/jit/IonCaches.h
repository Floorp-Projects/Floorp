/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonCaches_h
#define jit_IonCaches_h

#if defined(JS_CODEGEN_ARM)
# include "jit/arm/Assembler-arm.h"
#elif defined(JS_CODEGEN_ARM64)
# include "jit/arm64/Assembler-arm64.h"
#elif defined(JS_CODEGEN_MIPS32)
# include "jit/mips32/Assembler-mips32.h"
#elif defined(JS_CODEGEN_MIPS64)
# include "jit/mips64/Assembler-mips64.h"
#endif
#include "jit/JitCompartment.h"
#include "jit/Registers.h"
#include "jit/shared/Assembler-shared.h"
#include "js/TrackedOptimizationInfo.h"

#include "vm/TypedArrayObject.h"

namespace js {
namespace jit {

bool IsCacheableProtoChainForIonOrCacheIR(JSObject* obj, JSObject* holder);
bool IsCacheableGetPropReadSlotForIonOrCacheIR(JSObject* obj, JSObject* holder,
                                               PropertyResult prop);

bool IsCacheableGetPropCallScripted(JSObject* obj, JSObject* holder, Shape* shape,
                                    bool* isTemporarilyUnoptimizable = nullptr);
bool IsCacheableGetPropCallNative(JSObject* obj, JSObject* holder, Shape* shape);

bool IsCacheableSetPropCallScripted(JSObject* obj, JSObject* holder, Shape* shape,
                                    bool* isTemporarilyUnoptimizable = nullptr);
bool IsCacheableSetPropCallNative(JSObject* obj, JSObject* holder, Shape* shape);

bool ValueToNameOrSymbolId(JSContext* cx, HandleValue idval, MutableHandleId id,
                           bool* nameOrSymbol);

void* GetReturnAddressToIonCode(JSContext* cx);

void EmitIonStoreDenseElement(MacroAssembler& masm, const ConstantOrRegister& value,
                              Register elements, BaseObjectElementIndex target);

} // namespace jit
} // namespace js

#endif /* jit_IonCaches_h */
