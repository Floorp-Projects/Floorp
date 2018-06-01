/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Tests the Poller class.

const { Poller } = require("devtools/client/shared/poller");

add_task(async function() {
  let count1 = 0, count2 = 0, count3 = 0;

  const poller1 = new Poller(function() {
    count1++;
  }, 1000000000, true);
  const poller2 = new Poller(function() {
    count2++;
  }, 10);
  const poller3 = new Poller(function() {
    count3++;
  }, 1000000000);

  poller2.on();

  ok(!poller1.isPolling(), "isPolling() returns false for an off poller");
  ok(poller2.isPolling(), "isPolling() returns true for an on poller");

  await waitUntil(() => count2 > 10);

  ok(count2 > 10, "poller that was turned on polled several times");
  ok(count1 === 0, "poller that was never turned on never polled");

  await poller2.off();
  const currentCount2 = count2;

  // Really high poll time!
  poller1.on();
  poller3.on();

  await waitUntil(() => count1 === 1);
  ok(true, "Poller calls fn immediately when `immediate` is true");
  ok(count3 === 0, "Poller does not call fn immediately when `immediate` is not set");

  ok(count2 === currentCount2, "a turned off poller does not continue to poll");
  await poller2.off();
  await poller2.off();
  await poller2.off();
  ok(true, "Poller.prototype.off() is idempotent");

  // This should still have not polled a second time
  is(count1, 1, "wait time works");

  ok(poller1.isPolling(), "isPolling() returns true for an on poller");
  ok(!poller2.isPolling(), "isPolling() returns false for an off poller");
});

add_task(async function() {
  let count = -1;
  // Create a poller that returns a promise.
  // The promise is resolved asynchronously after adding 9 to the count, ensuring
  // that on every poll, we have a multiple of 10.
  const asyncPoller = new Poller(function() {
    count++;
    ok(!(count % 10), `Async poller called with a multiple of 10: ${count}`);
    return new Promise(function(resolve, reject) {
      let add9 = 9;
      const interval = setInterval(() => {
        if (add9--) {
          count++;
        } else {
          clearInterval(interval);
          resolve();
        }
      }, 10);
    });
  });

  asyncPoller.on(1);
  await waitUntil(() => count > 50);
  await asyncPoller.off();
});

add_task(async function() {
  // Create a poller that returns a promise. This poll call
  // is called immediately, and then subsequently turned off.
  // The call to `off` should not resolve until the inflight call
  // finishes.
  let inflightFinished = null;
  let pollCalls = 0;
  const asyncPoller = new Poller(function() {
    pollCalls++;
    return new Promise(function(resolve, reject) {
      setTimeout(() => {
        inflightFinished = true;
        resolve();
      }, 1000);
    });
  }, 1, true);
  asyncPoller.on();

  await asyncPoller.off();
  ok(inflightFinished,
     "off() method does not resolve until remaining inflight poll calls finish");
  is(pollCalls, 1, "should only be one poll call to occur before turning off polling");
});

add_task(async function() {
  // Create a poller that returns a promise. This poll call
  // is called immediately, and then subsequently turned off.
  // The call to `off` should not resolve until the inflight call
  // finishes.
  let inflightFinished = null;
  let pollCalls = 0;
  const asyncPoller = new Poller(function() {
    pollCalls++;
    return new Promise(function(resolve, reject) {
      setTimeout(() => {
        inflightFinished = true;
        resolve();
      }, 1000);
    });
  }, 1, true);
  asyncPoller.on();

  await asyncPoller.destroy();
  ok(inflightFinished,
     "destroy() method does not resolve until remaining inflight poll calls finish");
  is(pollCalls, 1, "should only be one poll call to occur before destroying polling");

  try {
    asyncPoller.on();
    ok(false, "Calling on() after destruction should throw");
  } catch (e) {
    ok(true, "Calling on() after destruction should throw");
  }
});
