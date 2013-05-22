/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_parallel_functions_h__
#define jsion_parallel_functions_h__

#include "vm/ThreadPool.h"
#include "vm/ForkJoin.h"
#include "gc/Heap.h"

namespace js {
namespace ion {

ForkJoinSlice *ParForkJoinSlice();
JSObject *ParNewGCThing(gc::AllocKind allocKind);
bool ParWriteGuard(ForkJoinSlice *context, JSObject *object);
void ParBailout(uint32_t id);
bool ParCheckOverRecursed(ForkJoinSlice *slice);
bool ParCheckInterrupt(ForkJoinSlice *context);

void ParDumpValue(Value *v);

// We pass the arguments to ParPush in a structure because, in code
// gen, it is convenient to store them on the stack to avoid
// constraining the reg alloc for the slow path.
struct ParPushArgs {
    JSObject *object;
    Value value;
};

// Extends the given object with the given value (like `Array.push`).
// Returns NULL on failure or else `args->object`, which is convenient
// during code generation.
JSObject* ParPush(ParPushArgs *args);

// Extends the given array with `length` new holes.  Returns NULL on
// failure or else `array`, which is convenient during code
// generation.
JSObject *ParExtendArray(ForkJoinSlice *slice, JSObject *array, uint32_t length);

// These parallel operations fail if they would be required to convert
// to a string etc etc.
ParallelResult ParStrictlyEqual(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, JSBool *);
ParallelResult ParStrictlyUnequal(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, JSBool *);
ParallelResult ParLooselyEqual(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, JSBool *);
ParallelResult ParLooselyUnequal(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, JSBool *);
ParallelResult ParLessThan(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, JSBool *);
ParallelResult ParLessThanOrEqual(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, JSBool *);
ParallelResult ParGreaterThan(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, JSBool *);
ParallelResult ParGreaterThanOrEqual(ForkJoinSlice *slice, MutableHandleValue v1, MutableHandleValue v2, JSBool *);

ParallelResult ParStringsEqual(ForkJoinSlice *slice, HandleString v1, HandleString v2, JSBool *);
ParallelResult ParStringsUnequal(ForkJoinSlice *slice, HandleString v1, HandleString v2, JSBool *);

ParallelResult InitRestParameter(ForkJoinSlice *slice, uint32_t length, Value *rest,
                                 HandleObject templateObj, HandleObject res,
                                 MutableHandleObject out);

void ParallelAbort(ParallelBailoutCause cause,
                   JSScript *outermostScript,
                   JSScript *currentScript,
                   jsbytecode *bytecode);

void PropagateParallelAbort(JSScript *outermostScript,
                            JSScript *currentScript);

void TraceLIR(uint32_t bblock, uint32_t lir, uint32_t execModeInt,
              const char *lirOpName, const char *mirOpName,
              JSScript *script, jsbytecode *pc);

void ParCallToUncompiledScript(JSFunction *func);

} // namespace ion
} // namespace js

#endif // jsion_parallel_functions_h__
