/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can nest event loops when needed in
// ThreadActor.prototype.unsafeSynchronize.

var gClient;
var gThreadActor;

function run_test() {
  initTestDebuggerServer();
  let gDebuggee = addTestGlobal("test-nesting");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-nesting", function (aResponse, aTabClient, aThreadClient) {
      // Reach over the protocol connection and get a reference to the thread actor.
      gThreadActor = aThreadClient._transport._serverConnection.getActor(aThreadClient._actor);

      test_nesting();
    });
  });
  do_test_pending();
}

function test_nesting() {
  const thread = gThreadActor;
  const { resolve, reject, promise: p } = promise.defer();

  let currentStep = 0;

  executeSoon(function () {
    // Should be on the first step
    do_check_eq(++currentStep, 1);
    // We should have one nested event loop from unsfeSynchronize
    do_check_eq(thread._nestedEventLoops.size, 1);
    resolve(true);
  });

  do_check_eq(thread.unsafeSynchronize(p), true);

  // Should be on the second step
  do_check_eq(++currentStep, 2);
  // There shouldn't be any nested event loops anymore
  do_check_eq(thread._nestedEventLoops.size, 0);

  finishClient(gClient);
}
