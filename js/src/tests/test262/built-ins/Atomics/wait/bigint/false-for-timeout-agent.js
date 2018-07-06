// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('BigInt')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,BigInt,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2018 Amal Hussein.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  False timeout arg should result in an +0 timeout
info: |
  Atomics.wait( typedArray, index, value, timeout )

  4. Let q be ? ToNumber(timeout).

    Boolean -> If argument is true, return 1. If argument is false, return +0.

includes: [atomicsHelper.js]
features: [Atomics, BigInt, SharedArrayBuffer, TypedArray]
---*/

const RUNNING = 1;

$262.agent.start(`
  const valueOf = {
    valueOf: function() {
      return false;
    }
  };

  const toPrimitive = {
    [Symbol.toPrimitive]: function() {
      return false;
    }
  };

  $262.agent.receiveBroadcast(function(sab) {
    const i64a = new BigInt64Array(sab);
    Atomics.add(i64a, ${RUNNING}, 1n);

    const before = $262.agent.monotonicNow();
    const status1 = Atomics.wait(i64a, 0, 0n, false);
    const status2 = Atomics.wait(i64a, 0, 0n, valueOf);
    const status3 = Atomics.wait(i64a, 0, 0n, toPrimitive);
    const duration = $262.agent.monotonicNow() - before;

    $262.agent.report(status1);
    $262.agent.report(status2);
    $262.agent.report(status3);
    $262.agent.report(duration);
    $262.agent.leaving();
  });
`);

const i64a = new BigInt64Array(
  new SharedArrayBuffer(BigInt64Array.BYTES_PER_ELEMENT * 4)
);

$262.agent.broadcast(i64a.buffer);
$262.agent.waitUntil(i64a, RUNNING, 1n);

// Try to yield control to ensure the agent actually started to wait.
$262.agent.tryYield();

assert.sameValue(
  $262.agent.getReport(),
  'timed-out',
  '$262.agent.getReport() returns "timed-out"'
);
assert.sameValue(
  $262.agent.getReport(),
  'timed-out',
  '$262.agent.getReport() returns "timed-out"'
);
assert.sameValue(
  $262.agent.getReport(),
  'timed-out',
  '$262.agent.getReport() returns "timed-out"'
);

const lapse = $262.agent.getReport();

assert(lapse >= 0, 'The result of `(lapse >= 0)` is true (timeout should be a min of 0ms)');

assert(lapse <= $262.agent.MAX_TIME_EPSILON, 'The result of `(lapse <= $262.agent.MAX_TIME_EPSILON)` is true');

assert.sameValue(Atomics.wake(i64a, 0), 0, 'Atomics.wake(i64a, 0) returns 0');

reportCompare(0, 0);
