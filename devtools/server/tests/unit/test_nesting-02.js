/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can nest event loops and then automatically exit nested event
// loops when requested.

var gClient;
var gThreadActor;

function run_test() {
  initTestDebuggerServer();
  let gDebuggee = addTestGlobal("test-nesting");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-nesting", function (aResponse, aTabClient, aThreadClient) {
      // Reach over the protocol connection and get a reference to the thread
      // actor.
      gThreadActor = aThreadClient._transport._serverConnection.getActor(aThreadClient._actor);

      test_nesting();
    });
  });
  do_test_pending();
}

function test_nesting() {
  const thread = gThreadActor;
  const { resolve, reject, promise: p } = promise.defer();

  // The following things should happen (in order):
  // 1. In the new event loop (created by unsafeSynchronize)
  // 2. Resolve the promise (shouldn't exit any event loops)
  // 3. Exit the event loop (should also then exit unsafeSynchronize's event loop)
  // 4. Be after the unsafeSynchronize call
  let currentStep = 0;

  executeSoon(function () {
    let eventLoop;

    executeSoon(function () {
      // Should be at step 2
      do_check_eq(++currentStep, 2);
      // Before resolving, should have the unsafeSynchronize event loop and the
      // one just created.
      do_check_eq(thread._nestedEventLoops.size, 2);

      executeSoon(function () {
        // Should be at step 3
        do_check_eq(++currentStep, 3);
        // Before exiting the manually created event loop, should have the
        // unsafeSynchronize event loop and the manual event loop.
        do_check_eq(thread._nestedEventLoops.size, 2);
        // Should have the event loop
        do_check_true(!!eventLoop);
        eventLoop.resolve();
      });

      resolve(true);
      // Shouldn't exit any event loops because a new one started since the call
      // to unsafeSynchronize
      do_check_eq(thread._nestedEventLoops.size, 2);
    });

    // Should be at step 1
    do_check_eq(++currentStep, 1);
    // Should have only the unsafeSynchronize event loop
    do_check_eq(thread._nestedEventLoops.size, 1);
    eventLoop = thread._nestedEventLoops.push();
    eventLoop.enter();
  });

  do_check_eq(thread.unsafeSynchronize(p), true);

  // Should be on the fourth step
  do_check_eq(++currentStep, 4);
  // There shouldn't be any nested event loops anymore
  do_check_eq(thread._nestedEventLoops.size, 0);

  finishClient(gClient);
}
