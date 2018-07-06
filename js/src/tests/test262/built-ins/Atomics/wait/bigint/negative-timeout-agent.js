// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('BigInt')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,BigInt,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2018 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Test that Atomics.wait times out with a negative timeout
includes: [atomicsHelper.js]
features: [Atomics, BigInt, SharedArrayBuffer, TypedArray]
---*/

const RUNNING = 1;

$262.agent.start(`
  $262.agent.receiveBroadcast(function(sab, id) {
    const i64a = new BigInt64Array(sab);
    Atomics.add(i64a, ${RUNNING}, 1n);

    $262.agent.report(Atomics.wait(i64a, 0, 0n, -5)); // -5 => 0
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
assert.sameValue(Atomics.wake(i64a, 0), 0, 'Atomics.wake(i64a, 0) returns 0');

reportCompare(0, 0);
