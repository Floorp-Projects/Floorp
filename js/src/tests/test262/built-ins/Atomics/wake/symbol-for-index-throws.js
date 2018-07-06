// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Return abrupt when ToInteger throws for 'index' argument to Atomics.wake
info: |
  Atomics.wake( typedArray, index, value, timeout )

  2. Let i be ? ValidateAtomicAccess(typedArray, index).

  ValidateAtomicAccess( typedArray, requestIndex )

  2. Let accessIndex be ? ToIndex(requestIndex).

  ToIndex ( value )

  2. Else,
    a. Let integerIndex be ? ToInteger(value).

  ToInteger(value)

  1. Let number be ? ToNumber(argument).

    Symbol --> Throw a TypeError exception.

features: [Atomics, SharedArrayBuffer, Symbol, Symbol.toPrimitive, TypedArray]
---*/

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

const poisonedValueOf = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

const poisonedToPrimitive = {
  [Symbol.toPrimitive]: function() {
    throw new Test262Error("passing a poisoned object using @@ToPrimitive");
  }
};

assert.throws(Test262Error, function() {
  Atomics.wake(i32a, poisonedValueOf, poisonedValueOf);
}, '`Atomics.wake(i32a, poisonedValueOf, poisonedValueOf)` throws Test262Error');

assert.throws(Test262Error, function() {
  Atomics.wake(i32a, poisonedToPrimitive, poisonedToPrimitive);
}, '`Atomics.wake(i32a, poisonedToPrimitive, poisonedToPrimitive)` throws Test262Error');

assert.throws(TypeError, function() {
  Atomics.wake(i32a, Symbol("foo"), poisonedValueOf);
}, '`Atomics.wake(i32a, Symbol("foo"), poisonedValueOf)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(i32a, Symbol("foo"), poisonedToPrimitive);
}, '`Atomics.wake(i32a, Symbol("foo"), poisonedToPrimitive)` throws TypeError');

reportCompare(0, 0);
