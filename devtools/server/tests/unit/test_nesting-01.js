/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can nest event loops when needed in
// ThreadActor.prototype.unsafeSynchronize.

var gClient;
var gThreadActor;

function run_test() {
  initTestDebuggerServer();
  addTestGlobal("test-nesting");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(
      gClient, "test-nesting",
      function (response, tabClient, threadClient) {
        // Reach over the protocol connection and get a reference to the thread actor.
        gThreadActor =
          threadClient._transport._serverConnection.getActor(threadClient._actor);

        test_nesting();
      });
  });
  do_test_pending();
}

function test_nesting() {
  const thread = gThreadActor;
  const { resolve, promise: p } = defer();

  let currentStep = 0;

  executeSoon(function () {
    // Should be on the first step
    Assert.equal(++currentStep, 1);
    // We should have one nested event loop from unsfeSynchronize
    Assert.equal(thread._nestedEventLoops.size, 1);
    resolve(true);
  });

  Assert.equal(thread.unsafeSynchronize(p), true);

  // Should be on the second step
  Assert.equal(++currentStep, 2);
  // There shouldn't be any nested event loops anymore
  Assert.equal(thread._nestedEventLoops.size, 0);

  finishClient(gClient);
}
