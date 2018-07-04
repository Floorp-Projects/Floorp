// file: atomicsHelper.js
// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Collection of functions used to interact with Atomics.* operations across agent boundaries.
---*/

/**
 * The amount of slack allowed for testing time-related Atomics methods (i.e. wait and wake).
 * The absolute value of the difference of the observed time and the expected time must
 * be epsilon-close.
 */
$262.agent.MAX_TIME_EPSILON = 100;

/**
 * @return {String} A report sent from an agent.
 */
{
  // This is only necessary because the original
  // $262.agent.getReport API was insufficient.
  //
  // All runtimes currently have their own
  // $262.agent.getReport which is wrong, so we
  // will pave over it with a corrected version.
  //
  // Binding $262.agent is necessary to prevent
  // breaking SpiderMonkey's $262.agent.getReport
  let getReport = $262.agent.getReport.bind($262.agent);

  $262.agent.getReport = function() {
    var r;
    while ((r = getReport()) == null) {
      $262.agent.sleep(1);
    }
    return r;
  };
}
/**
 * With a given Int32Array or BigInt64Array, wait until the expected number of agents have
 * reported themselves by calling:
 *
 *    Atomics.add(typedArray, index, 1);
 *
 * @param {(Int32Array|BigInt64Array)} typedArray An Int32Array or BigInt64Array with a SharedArrayBuffer
 * @param {number} index    The index of which all agents will report.
 * @param {number} expected The number of agents that are expected to report as active.
 */
$262.agent.waitUntil = function(typedArray, index, expected) {
  var agents = 0;
  while ((agents = Atomics.load(typedArray, index)) !== expected) {
    /* nothing */
  }
  assert.sameValue(agents, expected, "Reporting number of 'agents' equals the value of 'expected'");
};

/**
 * Timeout values used throughout the Atomics tests. All timeouts are specified in milliseconds.
 *
 * @property {number} yield Used for `$262.agent.tryYield`. Must not be used in other functions.
 * @property {number} small Used when agents will always timeout and `Atomics.wake` is not part
 *                          of the test semantics. Must be larger than `$262.agent.timeouts.yield`.
 * @property {number} long  Used when some agents may timeout and `Atomics.wake` is called on some
 *                          agents. The agents are required to wait and this needs to be observable
 *                          by the main thread.
 * @property {number} huge  Used when `Atomics.wake` is called on all waiting agents. The waiting
 *                          must not timeout. The agents are required to wait and this needs to be
 *                          observable by the main thread. All waiting agents must be woken by the
 *                          main thread.
 *
 * Usage for `$262.agent.timeouts.small`:
 *   const WAIT_INDEX = 0;
 *   const RUNNING = 1;
 *   const TIMEOUT = $262.agent.timeouts.small;
 *   const i32a = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2));
 *
 *   $262.agent.start(`
 *     $262.agent.receiveBroadcast(function(sab) {
 *       const i32a = new Int32Array(sab);
 *       Atomics.add(i32a, ${RUNNING}, 1);
 *
 *       $262.agent.report(Atomics.wait(i32a, ${WAIT_INDEX}, 0, ${TIMEOUT}));
 *
 *       $262.agent.leaving();
 *     });
 *   `);
 *   $262.agent.broadcast(i32a.buffer);
 *
 *   // Wait until the agent was started and then try to yield control to increase
 *   // the likelihood the agent has called `Atomics.wait` and is now waiting.
 *   $262.agent.waitUntil(i32a, RUNNING, 1);
 *   $262.agent.tryYield();
 *
 *   // The agent is expected to time out.
 *   assert.sameValue($262.agent.getReport(), "timed-out");
 *
 *
 * Usage for `$262.agent.timeouts.long`:
 *   const WAIT_INDEX = 0;
 *   const RUNNING = 1;
 *   const NUMAGENT = 2;
 *   const TIMEOUT = $262.agent.timeouts.long;
 *   const i32a = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2));
 *
 *   for (let i = 0; i < NUMAGENT; i++) {
 *     $262.agent.start(`
 *       $262.agent.receiveBroadcast(function(sab) {
 *         const i32a = new Int32Array(sab);
 *         Atomics.add(i32a, ${RUNNING}, 1);
 *
 *         $262.agent.report(Atomics.wait(i32a, ${WAIT_INDEX}, 0, ${TIMEOUT}));
 *
 *         $262.agent.leaving();
 *       });
 *     `);
 *   }
 *   $262.agent.broadcast(i32a.buffer);
 *
 *   // Wait until the agents were started and then try to yield control to increase
 *   // the likelihood the agents have called `Atomics.wait` and are now waiting.
 *   $262.agent.waitUntil(i32a, RUNNING, NUMAGENT);
 *   $262.agent.tryYield();
 *
 *   // Wake exactly one agent.
 *   assert.sameValue(Atomics.wake(i32a, WAIT_INDEX, 1), 1);
 *
 *   // When it doesn't matter how many agents were woken at once, a while loop
 *   // can be used to make the test more resilient against intermittent failures
 *   // in case even though `tryYield` was called, the agents haven't started to
 *   // wait.
 *   //
 *   // // Repeat until exactly one agent was woken.
 *   // var woken = 0;
 *   // while ((woken = Atomics.wake(i32a, WAIT_INDEX, 1)) !== 0) ;
 *   // assert.sameValue(woken, 1);
 *
 *   // One agent was woken and the other one timed out.
 *   const reports = [$262.agent.getReport(), $262.agent.getReport()];
 *   assert(reports.includes("ok"));
 *   assert(reports.includes("timed-out"));
 *
 *
 * Usage for `$262.agent.timeouts.huge`:
 *   const WAIT_INDEX = 0;
 *   const RUNNING = 1;
 *   const NUMAGENT = 2;
 *   const TIMEOUT = $262.agent.timeouts.huge;
 *   const i32a = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2));
 *
 *   for (let i = 0; i < NUMAGENT; i++) {
 *     $262.agent.start(`
 *       $262.agent.receiveBroadcast(function(sab) {
 *         const i32a = new Int32Array(sab);
 *         Atomics.add(i32a, ${RUNNING}, 1);
 *
 *         $262.agent.report(Atomics.wait(i32a, ${WAIT_INDEX}, 0, ${TIMEOUT}));
 *
 *         $262.agent.leaving();
 *       });
 *     `);
 *   }
 *   $262.agent.broadcast(i32a.buffer);
 *
 *   // Wait until the agents were started and then try to yield control to increase
 *   // the likelihood the agents have called `Atomics.wait` and are now waiting.
 *   $262.agent.waitUntil(i32a, RUNNING, NUMAGENT);
 *   $262.agent.tryYield();
 *
 *   // Wake all agents.
 *   assert.sameValue(Atomics.wake(i32a, WAIT_INDEX), NUMAGENT);
 *
 *   // When it doesn't matter how many agents were woken at once, a while loop
 *   // can be used to make the test more resilient against intermittent failures
 *   // in case even though `tryYield` was called, the agents haven't started to
 *   // wait.
 *   //
 *   // // Repeat until all agents were woken.
 *   // for (var wokenCount = 0; wokenCount < NUMAGENT; ) {
 *   //   var woken = 0;
 *   //   while ((woken = Atomics.wake(i32a, WAIT_INDEX)) !== 0) ;
 *   //   // Maybe perform an action on the woken agents here.
 *   //   wokenCount += woken;
 *   // }
 *
 *   // All agents were woken and none timeout.
 *   for (var i = 0; i < NUMAGENT; i++) {
 *     assert($262.agent.getReport(), "ok");
 *   }
 */
$262.agent.timeouts = {
  yield: 100,
  small: 200,
  long: 1000,
  huge: 10000,
};

/**
 * Try to yield control to the agent threads.
 *
 * Usage:
 *   const VALUE = 0;
 *   const RUNNING = 1;
 *   const i32a = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2));
 *
 *   $262.agent.start(`
 *     $262.agent.receiveBroadcast(function(sab) {
 *       const i32a = new Int32Array(sab);
 *       Atomics.add(i32a, ${RUNNING}, 1);
 *
 *       Atomics.store(i32a, ${VALUE}, 1);
 *
 *       $262.agent.leaving();
 *     });
 *   `);
 *   $262.agent.broadcast(i32a.buffer);
 *
 *   // Wait until agent was started and then try to yield control.
 *   $262.agent.waitUntil(i32a, RUNNING, 1);
 *   $262.agent.tryYield();
 *
 *   // Note: This result is not guaranteed, but should hold in practice most of the time.
 *   assert.sameValue(Atomics.load(i32a, VALUE), 1);
 *
 * The default implementation simply waits for `$262.agent.timeouts.yield` milliseconds.
 */
$262.agent.tryYield = function() {
  $262.agent.sleep($262.agent.timeouts.yield);
};

/**
 * Try to sleep the current agent for the given amount of milliseconds. It is acceptable,
 * but not encouraged, to ignore this sleep request and directly continue execution.
 *
 * The default implementation calls `$262.agent.sleep(ms)`.
 *
 * @param {number} ms Time to sleep in milliseconds.
 */
$262.agent.trySleep = function(ms) {
  $262.agent.sleep(ms);
};

// file: detachArrayBuffer.js
// Copyright (C) 2016 the V8 project authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    A function used in the process of asserting correctness of TypedArray objects.

    $262.detachArrayBuffer is defined by a host.

---*/

function $DETACHBUFFER(buffer) {
  if (!$262 || typeof $262.detachArrayBuffer !== "function") {
    throw new Test262Error("No method available to detach an ArrayBuffer");
  }
  $262.detachArrayBuffer(buffer);
}
