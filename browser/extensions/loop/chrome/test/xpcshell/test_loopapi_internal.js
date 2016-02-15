/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported run_test */

const { LoopAPI } = Cu.import("chrome://loop/content/modules/MozLoopAPI.jsm", {});
const [LoopAPIInternal] = LoopAPI.inspect();

add_test(function test_intialize() {
  let [, , pageListeners] = LoopAPI.inspect();
  Assert.equal(pageListeners, null, "Page listeners should not be initialized yet");

  LoopAPIInternal.initialize();
  let [, , pageListeners2] = LoopAPI.inspect();
  Assert.equal(pageListeners2.length, 3, "Three page listeners should be added");

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

add_test(function test_sendMessageToHandler() {
  // Testing error branches.
  LoopAPI.sendMessageToHandler({
    name: "WellThisDoesNotExist"
  }, err => {
    Assert.ok(err.isError, "An error should be returned");
    Assert.strictEqual(err.message,
      "Ouch, no message handler available for 'WellThisDoesNotExist'",
      "Error messages should match");
  });

  // Testing correct flow branches.
  let hangupNowCalls = [];
  LoopAPI.stubMessageHandlers({
    HangupNow: function(message, reply) {
      hangupNowCalls.push(message);
      reply();
    }
  });

  let message = {
    name: "HangupNow",
    data: ["fakeToken", 42]
  };
  LoopAPI.sendMessageToHandler(message);

  Assert.strictEqual(hangupNowCalls.length, 1, "HangupNow handler should be called once");
  Assert.deepEqual(hangupNowCalls.pop(), message, "Messages should be the same");

  LoopAPI.restore();

  run_next_test();
});

function run_test() {
  do_register_cleanup(function() {
    LoopAPI.destroy();
  });
  run_next_test();
}
