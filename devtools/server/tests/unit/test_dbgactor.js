/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(
  Ci.nsIJSInspector
);

add_task(
  threadFrontTest(async ({ threadFront, debuggee, targetFront }) => {
    Assert.equal(xpcInspector.eventLoopNestLevel, 0);

    await new Promise(resolve => {
      threadFront.on("paused", function(packet) {
        Assert.equal(false, "error" in packet);
        Assert.ok("actor" in packet);
        Assert.ok("why" in packet);
        Assert.equal(packet.why.type, "debuggerStatement");

        // Reach around the protocol to check that the debuggee is in the state
        // we expect.
        Assert.ok(debuggee.a);
        Assert.ok(!debuggee.b);

        Assert.equal(xpcInspector.eventLoopNestLevel, 1);

        // Let the debuggee continue execution.
        threadFront.resume().then(resolve);
      });

      // Now that we know we're resumed, we can make the debuggee do something.
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
