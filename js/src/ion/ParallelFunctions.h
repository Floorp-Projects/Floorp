/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_ParallelFunctions_h
#define ion_ParallelFunctions_h

#include "gc/Heap.h"
#include "vm/ForkJoin.h"
#include "vm/ThreadPool.h"

namespace js {
namespace ion {

ForkJoinSlice *ForkJoinSlicePar();
JSObject *NewGCThingPar(gc::AllocKind allocKind);
bool IsThreadLocalObject(ForkJoinSlice *context, JSObject *object);
bool CheckOverRecursedPar(ForkJoinSlice *slice);
bool CheckInterruptPar(ForkJoinSlice *context);

// We pass the arguments to PushPar in a structure because, in code
// gen, it is convenient to store them on the stack to avoid
// constraining the reg alloc for the slow path.
struct PushParArgs {
    JSObject *object;
    Value value;
};

// Extends the given object with the given value (like `Array.push`).
// Returns NULL on failure or else `args->object`, which is convenient
// during code generation.
JSObject *PushPar(PushParArgs *args);

// Extends the given array with `length` new holes.  Returns NULL on
// failure or else `array`, which is convenient during code
// generation.
JSObject *ExtendArrayPar(ForkJoinSlice *slice, JSObject *array, uint32_t length);

// String related parallel functions. These tend to call existing VM functions
// that take a ThreadSafeContext.
ParallelResult ConcatStringsPar(ForkJoinSlice *slice, HandleString left, HandleString right,
                                MutableHandleString out);
ParallelResult IntToStringPar(ForkJoinSlice *slice, int i, MutableHandleString out);
ParallelResult DoubleToStringPar(ForkJoinSlice *slice, double d, MutableHandleString out);

// These parallel operations fail if they would be required to convert
// to a string etc etc.
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

ParallelResult InitRestParameterPar(ForkJoinSlice *slice, uint32_t length, Value *rest,
                                    HandleObject templateObj, HandleObject res,
                                    MutableHandleObject out);

void AbortPar(ParallelBailoutCause cause, JSScript *outermostScript, JSScript *currentScript,
              jsbytecode *bytecode);
void PropagateAbortPar(JSScript *outermostScript, JSScript *currentScript);

void TraceLIR(uint32_t bblock, uint32_t lir, uint32_t execModeInt,
              const char *lirOpName, const char *mirOpName,
              JSScript *script, jsbytecode *pc);

void CallToUncompiledScriptPar(JSFunction *func);

} // namespace ion
} // namespace js

#endif /* ion_ParallelFunctions_h */
