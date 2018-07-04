// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('BigInt')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,BigInt,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2018 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Get the correct WaiterList
info: |
  Atomics.wait( typedArray, index, value, timeout )

  ...
  11. Let WL be GetWaiterList(block, indexedPosition).
  ...


  GetWaiterList( block, i )

  ...
  4. Return the WaiterList that is referenced by the pair (block, i).

includes: [atomicsHelper.js]
features: [Atomics, BigInt, SharedArrayBuffer, TypedArray]
---*/

var NUMAGENT = 2;
var RUNNING = 4;

$262.agent.start(`
  $262.agent.receiveBroadcast(function(sab) {
    const i64a = new BigInt64Array(sab);
    Atomics.add(i64a, ${RUNNING}, 1n);

    // Wait on index 0
    $262.agent.report(Atomics.wait(i64a, 0, 0n, Infinity));
    $262.agent.leaving();
  });
`);

$262.agent.start(`
  $262.agent.receiveBroadcast(function(sab) {
    const i64a = new BigInt64Array(sab);
    Atomics.add(i64a, ${RUNNING}, 1n);

    // Wait on index 2
    $262.agent.report(Atomics.wait(i64a, 2, 0n, Infinity));
    $262.agent.leaving();
  });
`);

const i64a = new BigInt64Array(
  new SharedArrayBuffer(BigInt64Array.BYTES_PER_ELEMENT * 5)
);

$262.agent.broadcast(i64a.buffer);

// Wait until all agents started.
$262.agent.waitUntil(i64a, RUNNING, BigInt(NUMAGENT));

// Wake index 1, wakes nothing
assert.sameValue(Atomics.wake(i64a, 1), 0, 'Atomics.wake(i64a, 1) returns 0');

// Wake index 3, wakes nothing
assert.sameValue(Atomics.wake(i64a, 3), 0, 'Atomics.wake(i64a, 3) returns 0');

// Wake index 2, wakes 1
var woken = 0;
while ((woken = Atomics.wake(i64a, 2)) === 0) ;
assert.sameValue(woken, 1, 'Atomics.wake(i64a, 2) returns 1');
assert.sameValue($262.agent.getReport(), 'ok', '$262.agent.getReport() returns "ok"');

// Wake index 0, wakes 1
var woken = 0;
while ((woken = Atomics.wake(i64a, 0)) === 0) ;
assert.sameValue(woken, 1, 'Atomics.wake(i64a, 0) returns 1');
assert.sameValue($262.agent.getReport(), 'ok', '$262.agent.getReport() returns "ok"');

reportCompare(0, 0);
