/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(
  Ci.nsIJSInspector
);

add_task(
  threadClientTest(async ({ threadClient, debuggee, client, targetFront }) => {
    await new Promise(resolve => {
      threadClient.on("paused", function(packet) {
        Assert.equal(threadClient.state, "paused");
        // Reach around the protocol to check that the debuggee is in the state
        // we expect.
        Assert.ok(debuggee.a);
        Assert.ok(!debuggee.b);

        Assert.equal(xpcInspector.eventLoopNestLevel, 1);

        threadClient.resume().then(resolve);
      });

      Cu.evalInSandbox(
        "var a = true; var b = false; debugger; var b = true;",
        debuggee
      );

      // Now make sure that we've run the code after the debugger statement...
      Assert.ok(debuggee.b);
    });

    Assert.equal(xpcInspector.eventLoopNestLevel, 0);
  })
);
