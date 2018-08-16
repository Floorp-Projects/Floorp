// |reftest| skip-if(!this.hasOwnProperty('Atomics')) -- Atomics is not enabled unconditionally
// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.wake
description: >
  Throws a TypeError if typedArray arg is not an Object
info: |
  Atomics.wake( typedArray, index, count )

  1.Let buffer be ? ValidateSharedIntegerTypedArray(typedArray, true).
    ...
    2. if Type(typedArray) is not Object, throw a TypeError exception
features: [Atomics, Symbol]
---*/

const poisoned = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

assert.throws(TypeError, function() {
  Atomics.wake(null, poisoned, poisoned);
}, '`Atomics.wake(null, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(undefined, poisoned, poisoned);
}, '`Atomics.wake(undefined, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(true, poisoned, poisoned);
}, '`Atomics.wake(true, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(false, poisoned, poisoned);
}, '`Atomics.wake(false, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake('***string***', poisoned, poisoned);
}, '`Atomics.wake(\'***string***\', poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(Number.NEGATIVE_INFINITY, poisoned, poisoned);
}, '`Atomics.wake(Number.NEGATIVE_INFINITY, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  Atomics.wake(Symbol('***symbol***'), poisoned, poisoned);
}, '`Atomics.wake(Symbol(\'***symbol***\'), poisoned, poisoned)` throws TypeError');

reportCompare(0, 0);
