// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Throws a TypeError if index arg can not be converted to an Integer
info: |
  Atomics.wait( typedArray, index, value, timeout )

  4. Let q be ? ToNumber(timeout).

    Symbol --> Throw a TypeError exception.

includes: [atomicsHelper.js]
features: [Atomics, SharedArrayBuffer, Symbol, Symbol.toPrimitive, TypedArray]
---*/

const RUNNING = 1;

$262.agent.start(`
  $262.agent.receiveBroadcast(function(sab) {
    const i32a = new Int32Array(sab);
    Atomics.add(i32a, ${RUNNING}, 1);

    let status1 = "";
    let status2 = "";

    const start = $262.agent.monotonicNow();
    try {
      Atomics.wait(i32a, 0, 0, Symbol("1"));
    } catch (error) {
      status1 = 'Symbol("1")';
    }
    try {
      Atomics.wait(i32a, 0, 0, Symbol("2"));
    } catch (error) {
      status2 = 'Symbol("2")';
    }
    const duration = $262.agent.monotonicNow() - start;

    $262.agent.report(status1);
    $262.agent.report(status2);
    $262.agent.report(duration);
    $262.agent.leaving();
  });
`);

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

$262.agent.broadcast(i32a.buffer);
$262.agent.waitUntil(i32a, RUNNING, 1);

// Try to yield control to ensure the agent actually started to wait.
$262.agent.tryYield();

assert.sameValue(
  $262.agent.getReport(),
  'Symbol("1")',
  '$262.agent.getReport() returns "Symbol("1")"'
);
assert.sameValue(
  $262.agent.getReport(),
  'Symbol("2")',
  '$262.agent.getReport() returns "Symbol("2")"'
);

const lapse = $262.agent.getReport();

assert(lapse >= 0, 'The result of `(lapse >= 0)` is true (timeout should be a min of 0ms)');
assert(lapse <= $262.agent.MAX_TIME_EPSILON, 'The result of `(lapse <= $262.agent.MAX_TIME_EPSILON)` is true (timeout should be a max of $$262.agent.MAX_TIME_EPSILON)');

assert.sameValue(Atomics.wake(i32a, 0), 0, 'Atomics.wake(i32a, 0) returns 0');

reportCompare(0, 0);
