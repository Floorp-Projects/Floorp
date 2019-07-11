/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that pause-lifetime grip clients are marked invalid after a resume.
 */

var gDebuggee;
var gClient;
var gThreadFront;

function run_test() {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack", function(
      response,
      targetFront,
      threadFront
    ) {
      gThreadFront = threadFront;
      test_pause_frame();
    });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadFront.once("paused", async function(packet) {
    const args = packet.frame.arguments;
    const objActor = args[0].actor;
    Assert.equal(args[0].class, "Object");
    Assert.ok(!!objActor);

    const objClient = gThreadFront.pauseGrip(args[0]);
    Assert.ok(objClient.valid);

    // Make a bogus request to the grip actor.  Should get
    // unrecognized-packet-type (and not no-such-actor).
    try {
      await gClient.request({ to: objActor, type: "bogusRequest" });
      ok(false, "bogusRequest should throw");
    } catch (e) {
      ok(true, "bogusRequest thrown");
      Assert.equal(e.error, "unrecognizedPacketType");
    }
    Assert.ok(objClient.valid);

    gThreadFront.resume().then(async function() {
      // Now that we've resumed, should get no-such-actor for the
      // same request.
      try {
        await gClient.request({ to: objActor, type: "bogusRequest" });
        ok(false, "bogusRequest should throw");
      } catch (e) {
        ok(true, "bogusRequest thrown");
        Assert.equal(e.error, "noSuchActor");
      }
      Assert.ok(!objClient.valid);
      finishClient(gClient);
    });
  });

  gDebuggee.eval(
    "(" +
      function() {
        function stopMe(obj) {
          debugger;
        }
        stopMe({ foo: "bar" });
      } +
      ")()"
  );
}
