/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that pause-lifetime grip clients are marked invalid after a resume.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_pause_frame();
                           });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let args = packet.frame.arguments;
    let objActor = args[0].actor;
    Assert.equal(args[0].class, "Object");
    Assert.ok(!!objActor);

    let objClient = gThreadClient.pauseGrip(args[0]);
    Assert.ok(objClient.valid);

    // Make a bogus request to the grip actor.  Should get
    // unrecognized-packet-type (and not no-such-actor).
    gClient.request({ to: objActor, type: "bogusRequest" }, function (response) {
      Assert.equal(response.error, "unrecognizedPacketType");
      Assert.ok(objClient.valid);

      gThreadClient.resume(function () {
        // Now that we've resumed, should get no-such-actor for the
        // same request.
        gClient.request({ to: objActor, type: "bogusRequest" }, function (response) {
          Assert.ok(!objClient.valid);
          Assert.equal(response.error, "noSuchActor");
          finishClient(gClient);
        });
      });
    });
  });

  gDebuggee.eval("(" + function () {
    function stopMe(obj) {
      debugger;
    }
    stopMe({ foo: "bar" });
  } + ")()");
}
