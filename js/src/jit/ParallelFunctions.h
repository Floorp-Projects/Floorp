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
namespace jit {

ForkJoinSlice *ForkJoinSlicePar();
JSObject *NewGCThingPar(ForkJoinSlice *slice, gc::AllocKind allocKind);
bool IsThreadLocalObject(ForkJoinSlice *slice, JSObject *object);
bool CheckOverRecursedPar(ForkJoinSlice *slice);
bool CheckInterruptPar(ForkJoinSlice *slice);

// Extends the given array with `length` new holes.  Returns nullptr on
// failure or else `array`, which is convenient during code
// generation.
JSObject *ExtendArrayPar(ForkJoinSlice *slice, JSObject *array, uint32_t length);

// Set properties and elements on thread local objects.
ParallelResult SetElementPar(ForkJoinSlice *slice, HandleObject obj, HandleValue index,
                             HandleValue value, bool strict);

// String related parallel functions. These tend to call existing VM functions
// that take a ThreadSafeContext.
ParallelResult ConcatStringsPar(ForkJoinSlice *slice, HandleString left, HandleString right,
                                MutableHandleString out);
ParallelResult IntToStringPar(ForkJoinSlice *slice, int i, MutableHandleString out);
ParallelResult DoubleToStringPar(ForkJoinSlice *slice, double d, MutableHandleString out);
ParallelResult StringToNumberPar(ForkJoinSlice *slice, JSString *str, double *out);

// Binary and unary operator functions on values. These tend to return
// RETRY_SEQUENTIALLY if the values are objects.
ParallelResult StrictlyEqualPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
ParallelResult StrictlyUnequalPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
ParallelResult LooselyEqualPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
ParallelResult LooselyUnequalPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
ParallelResult LessThanPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
ParallelResult LessThanOrEqualPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
ParallelResult GreaterThanPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);
ParallelResult GreaterThanOrEqualPar(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, bool *);

ParallelResult StringsEqualPar(ForkJoinSlice *slice, HandleString v1, HandleString v2, bool *);
ParallelResult StringsUnequalPar(ForkJoinSlice *slice, HandleString v1, HandleString v2, bool *);

ParallelResult BitNotPar(ForkJoinSlice *slice, HandleValue in, int32_t *out);
ParallelResult BitXorPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);
ParallelResult BitOrPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);
ParallelResult BitAndPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);
ParallelResult BitLshPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);
ParallelResult BitRshPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs, int32_t *out);

ParallelResult UrshValuesPar(ForkJoinSlice *slice, HandleValue lhs, HandleValue rhs,
                             Value *out);

// Make a new rest parameter in parallel.
ParallelResult InitRestParameterPar(ForkJoinSlice *slice, uint32_t length, Value *rest,
                                    HandleObject templateObj, HandleObject res,
                                    MutableHandleObject out);

// Abort and debug tracing functions.
void AbortPar(ParallelBailoutCause cause, JSScript *outermostScript, JSScript *currentScript,
              jsbytecode *bytecode);
void PropagateAbortPar(JSScript *outermostScript, JSScript *currentScript);

void TraceLIR(IonLIRTraceData *current);

void CallToUncompiledScriptPar(JSObject *obj);

} // namespace jit
} // namespace js

#endif /* jit_ParallelFunctions_h */
