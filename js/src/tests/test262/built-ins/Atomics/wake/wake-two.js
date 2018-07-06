// |reftest| skip-if(!this.hasOwnProperty('Atomics')||!this.hasOwnProperty('SharedArrayBuffer')) -- Atomics,SharedArrayBuffer is not enabled unconditionally
// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test that Atomics.wake wakes two waiters if that's what the count is.
includes: [atomicsHelper.js]
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

const WAIT_INDEX = 0;             // Agents wait here
const RUNNING = 1;                // Accounting of live agents here
const WAKECOUNT = 2;
const NUMAGENT = 3;
const BUFFER_SIZE = 4;

const TIMEOUT = $262.agent.timeouts.long;

for (var i = 0; i < NUMAGENT; i++ ) {
  $262.agent.start(`
    $262.agent.receiveBroadcast(function(sab) {
      const i32a = new Int32Array(sab);
      Atomics.add(i32a, ${RUNNING}, 1);

      // Waiters that are not woken will time out eventually.
      $262.agent.report(Atomics.wait(i32a, ${WAIT_INDEX}, 0, ${TIMEOUT}));
      $262.agent.leaving();
    })
  `);
}

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * BUFFER_SIZE)
);

$262.agent.broadcast(i32a.buffer);

// Wait for agents to be running.
$262.agent.waitUntil(i32a, RUNNING, NUMAGENT);

// Try to yield control to ensure the agent actually started to wait.
$262.agent.tryYield();

// There's a slight risk we'll fail to wake the desired count, if the preceding
// tryYield() took much longer than anticipated and workers have started timing
// out.
assert.sameValue(
  Atomics.wake(i32a, 0, WAKECOUNT),
  WAKECOUNT,
  'Atomics.wake(i32a, 0, WAKECOUNT) returns the value of `WAKECOUNT`'
);

// Try to sleep past the timeout.
$262.agent.trySleep(TIMEOUT);

// Collect and check results
const reports = [];
for (var i = 0; i < NUMAGENT; i++) {
  reports.push($262.agent.getReport());
}
reports.sort();

for (var i = 0; i < WAKECOUNT; i++) {
  assert.sameValue(reports[i], 'ok', 'The value of reports[i] is "ok"');
}
for (var i = WAKECOUNT; i < NUMAGENT; i++) {
  assert.sameValue(reports[i], 'timed-out', 'The value of reports[i] is "timed-out"');
}

reportCompare(0, 0);
