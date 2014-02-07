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

ForkJoinContext *ForkJoinContextPar();
JSObject *NewGCThingPar(ForkJoinContext *cx, gc::AllocKind allocKind);
bool ParallelWriteGuard(ForkJoinContext *cx, JSObject *object);
bool IsInTargetRegion(ForkJoinContext *cx, TypedDatum *object);
bool CheckOverRecursedPar(ForkJoinContext *cx);
bool InterruptCheckPar(ForkJoinContext *cx);

// Extends the given array with `length` new holes.  Returns nullptr on
// failure or else `array`, which is convenient during code
// generation.
JSObject *ExtendArrayPar(ForkJoinContext *cx, JSObject *array, uint32_t length);

// Set properties and elements on thread local objects.
bool SetPropertyPar(ForkJoinContext *cx, HandleObject obj, HandlePropertyName name,
                    HandleValue value, bool strict, jsbytecode *pc);
bool SetElementPar(ForkJoinContext *cx, HandleObject obj, HandleValue index,
                   HandleValue value, bool strict);

// String related parallel functions. These tend to call existing VM functions
// that take a ThreadSafeContext.
JSString *ConcatStringsPar(ForkJoinContext *cx, HandleString left, HandleString right);
JSFlatString *IntToStringPar(ForkJoinContext *cx, int i);
JSString *DoubleToStringPar(ForkJoinContext *cx, double d);
JSString *PrimitiveToStringPar(ForkJoinContext *cx, HandleValue input);
bool StringToNumberPar(ForkJoinContext *cx, JSString *str, double *out);

// Binary and unary operator functions on values. These tend to return
// RETRY_SEQUENTIALLY if the values are objects.
bool StrictlyEqualPar(ForkJoinContext *cx, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool StrictlyUnequalPar(ForkJoinContext *cx, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool LooselyEqualPar(ForkJoinContext *cx, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool LooselyUnequalPar(ForkJoinContext *cx, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool LessThanPar(ForkJoinContext *cx, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool LessThanOrEqualPar(ForkJoinContext *cx, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool GreaterThanPar(ForkJoinContext *cx, MutableHandleValue v1, MutableHandleValue v2, bool *);
bool GreaterThanOrEqualPar(ForkJoinContext *cx, MutableHandleValue v1, MutableHandleValue v2, bool *);

bool StringsEqualPar(ForkJoinContext *cx, HandleString v1, HandleString v2, bool *);
bool StringsUnequalPar(ForkJoinContext *cx, HandleString v1, HandleString v2, bool *);

bool BitNotPar(ForkJoinContext *cx, HandleValue in, int32_t *out);
bool BitXorPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out);
bool BitOrPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out);
bool BitAndPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out);
bool BitLshPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out);
bool BitRshPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, int32_t *out);

bool UrshValuesPar(ForkJoinContext *cx, HandleValue lhs, HandleValue rhs, Value *out);

// Make a new rest parameter in parallel.
JSObject *InitRestParameterPar(ForkJoinContext *cx, uint32_t length, Value *rest,
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
