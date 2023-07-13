/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests UrlbarUtils.SkippableTimer
 */

"use strict";

let { SkippableTimer } = ChromeUtils.importESModule(
  "resource:///modules/UrlbarUtils.sys.mjs"
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

add_task(async function test_basic() {
  let invoked = 0;
  let deferred = PromiseUtils.defer();
  let timer = new SkippableTimer({
    name: "test 1",
    callback: () => {
      invoked++;
      deferred.resolve();
    },
    time: 50,
  });
  Assert.equal(timer.name, "test 1", "Timer should have the correct name");
  Assert.ok(!timer.done, "Should not be done");
  Assert.equal(invoked, 0, "Should not have invoked the callback yet");
  await deferred.promise;
  Assert.ok(timer.done, "Should be done");
  Assert.equal(invoked, 1, "Should have invoked the callback");
});

add_task(async function test_fire() {
  let longTimeMs = 1000;
  let invoked = 0;
  let deferred = PromiseUtils.defer();
  let timer = new SkippableTimer({
    name: "test 1",
    callback: () => {
      invoked++;
      deferred.resolve();
    },
    time: longTimeMs,
  });
  let start = Cu.now();
  Assert.equal(timer.name, "test 1", "Timer should have the correct name");
  Assert.ok(!timer.done, "Should not be done");
  Assert.equal(invoked, 0, "Should not have invoked the callback yet");
  // Call fire() many times to also verify the callback is invoked just once.
  timer.fire();
  timer.fire();
  timer.fire();
  Assert.ok(timer.done, "Should be done");
  await deferred.promise;
  Assert.greater(longTimeMs, Cu.now() - start, "Should have resolved earlier");
  Assert.equal(invoked, 1, "Should have invoked the callback");
});

add_task(async function test_cancel() {
  let timeMs = 50;
  let invoked = 0;
  let deferred = PromiseUtils.defer();
  let timer = new SkippableTimer({
    name: "test 1",
    callback: () => {
      invoked++;
      deferred.resolve();
    },
    time: timeMs,
  });
  let start = Cu.now();
  Assert.equal(timer.name, "test 1", "Timer should have the correct name");
  Assert.ok(!timer.done, "Should not be done");
  Assert.equal(invoked, 0, "Should not have invoked the callback yet");
  // Calling cancel many times shouldn't rise any error.
  timer.cancel();
  timer.cancel();
  Assert.ok(timer.done, "Should be done");
  await Promise.race([
    deferred.promise,
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    new Promise(r => setTimeout(r, timeMs * 4)),
  ]);
  Assert.greater(Cu.now() - start, timeMs, "Should not have resolved earlier");
  Assert.equal(invoked, 0, "Should not have invoked the callback");
});
