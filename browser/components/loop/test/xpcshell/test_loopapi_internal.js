/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { LoopAPI } = Cu.import("resource:///modules/loop/MozLoopAPI.jsm", {});
const [LoopAPIInternal] = LoopAPI.inspect();

add_test(function test_intialize() {
  let [, , pageListeners] = LoopAPI.inspect();
  Assert.equal(pageListeners, null, "Page listeners should not be initialized yet");

  LoopAPIInternal.initialize();
  let [, , pageListeners2] = LoopAPI.inspect();
  Assert.equal(pageListeners2.length, 2, "Two page listeners should be added");

  let pageListenersStub = {};
  LoopAPI.stub([pageListenersStub]);

  LoopAPIInternal.initialize();
  let [, , pageListeners3] = LoopAPI.inspect();
  Assert.equal(pageListeners3[0], pageListenersStub, "A second call should be ignored");

  // Restore the original pageListeners.
  LoopAPI.restore();
  run_next_test();
});

var gSeqCount = 0;

function createMessage(action, ...args) {
  return {
    name: "Loop:Message",
    data: [++gSeqCount, action, ...args]
  };
}

add_test(function test_handleMessage() {
  // First check if batch messages are recognized properly.
  let callCount = 0;
  LoopAPIInternal.handleMessage(createMessage("Batch:Foo", []), () => ++callCount);
  Assert.strictEqual(callCount, 0, "The reply handler should not be called");

  // Check for errors when a handler is called that does not exist.
  LoopAPIInternal.handleMessage(createMessage("Foo"), result => {
    Assert.ok(result.isError);
    Assert.equal(result.message, "Ouch, no message handler available for 'Foo'",
      "Error messages should match");
    ++callCount;
  });
  Assert.strictEqual(callCount, 1, "The reply handler should've been called");

  // Run an end-to-end API call with the simplest message handler available.
  LoopAPIInternal.handleMessage(createMessage("GetLoopPref", "enabled"), result => {
    Assert.strictEqual(result, true);
    callCount++;
  });
  Assert.strictEqual(callCount, 2, "The reply handler should've been called");

  run_next_test();
});

function run_test() {
  do_register_cleanup(function() {
    LoopAPI.destroy();
  });
  run_next_test();
}
