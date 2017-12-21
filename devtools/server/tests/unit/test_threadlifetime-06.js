/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that promoting multiple pause-lifetime actors to thread-lifetime actors
 * works as expected.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-grips",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_thread_lifetime();
                           });
  });
  do_test_pending();
}

function test_thread_lifetime() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let actors = [];
    let last;
    for (let grip of packet.frame.arguments) {
      actors.push(grip.actor);
      last = grip.actor;
    }

    // Create thread-lifetime actors for these objects.
    gThreadClient.threadGrips(actors, function (response) {
      // Successful promotion won't return an error.
      Assert.equal(response.error, undefined);

      gThreadClient.addOneTimeListener("paused", function (event, packet) {
        // Verify that the promoted actors are returned again.
        actors.forEach(function (actor, i) {
          Assert.equal(actor, packet.frame.arguments[i].actor);
        });
        // Now that we've resumed, release the thread-lifetime grips.
        gThreadClient.releaseMany(actors, function (response) {
          // Successful release won't return an error.
          Assert.equal(response.error, undefined);

          gClient.request({ to: last, type: "bogusRequest" }, function (response) {
            Assert.equal(response.error, "noSuchActor");
            gThreadClient.resume(function (response) {
              finishClient(gClient);
            });
          });
        });
      });
      gThreadClient.resume();
    });
  });

  gDebuggee.eval("(" + function () {
    function stopMe(arg1, arg2, arg3) {
      debugger;
      debugger;
    }
    stopMe({obj: 1}, {obj: 2}, {obj: 3});
  } + ")()");
}
