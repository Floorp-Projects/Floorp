/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ParallelFunctions_h
#define jit_ParallelFunctions_h

#include "gc/Heap.h"
#include "vm/ForkJoin.h"

namespace js {

class TypedDatum; // subclass of JSObject* defined in builtin/TypedObject.h

namespace jit {

ForkJoinSlice *ForkJoinSlicePar();
JSObject *NewGCThingPar(ForkJoinSlice *slice, gc::AllocKind allocKind);
bool ParallelWriteGuard(ForkJoinSlice *slice, JSObject *object);
bool IsInTargetRegion(ForkJoinSlice *slice, TypedDatum *object);
bool CheckOverRecursedPar(ForkJoinSlice *slice);
bool CheckInterruptPar(ForkJoinSlice *slice);

// Extends the given array with `length` new holes.  Returns nullptr on
// failure or else `array`, which is convenient during code
// generation.
JSObject *ExtendArrayPar(ForkJoinSlice *slice, JSObject *array, uint32_t length);

// Set properties and elements on thread local objects.
bool SetPropertyPar(ForkJoinSlice *slice, HandleObject obj, HandlePropertyName name,
                    HandleValue value, bool strict, jsbytecode *pc);
bool SetElementPar(ForkJoinSlice *slice, HandleObject obj, HandleValue index,
                   HandleValue value, bool strict);

// String related parallel functions. These tend to call existing VM functions
// that take a ThreadSafeContext.
JSString *ConcatStringsPar(ForkJoinSlice *slice, HandleString left, HandleString right);
JSFlatString *IntToStringPar(ForkJoinSlice *slice, int i);
JSString *DoubleToStringPar(ForkJoinSlice *slice, double d);
bool StringToNumberPar(ForkJoinSlice *slice, JSString *str, double *out);

// Binary and unary operator functions on values. These tend to return
// RETRY_SEQUENTIALLY if the values are objects.
bool StrictlyEqualPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool StrictlyUnequalPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool LooselyEqualPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool LooselyUnequalPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool LessThanPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool LessThanOrEqualPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool GreaterThanPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool GreaterThanOrEqualPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);

bool StringsEqualPar(ForkJoinSlice *slice, HandleString v1, HandleString v2, bool *);
bool StringsUnequalPar(ForkJoinSlice *slice, HandleString v1, HandleString v2, bool *);

bool BitNotPar(ForkJoinSlice *slice, HandleValue in, int32_t *out);
bool BitXorPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);
bool BitOrPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);
bool BitAndPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);
bool BitLshPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);
bool BitRshPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);

bool UrshValuesPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs,
                             Value *out);

// Make a new rest parameter in parallel.
JSObject *InitRestParameterPar(ForkJoinSlice *slice, uint32_t length, Value *rest,
                               HandleObject templateObj, HandleObject res);

// Abort and debug tracing functions.
void AbortPar(ParallelBailoutCause cause, JSScript *outermostScript, JSScript *currentScript,
              jsbytecode *bytecode);
void PropagateAbortPar(JSScript *outermostScript, JSScript *currentScript);

void TraceLIR(IonLIRTraceData *current);

void CallToUncompiledScriptPar(JSObject *obj);

} // namespace jit
} // namespace js

#endif /* jit_ParallelFunctions_h */
