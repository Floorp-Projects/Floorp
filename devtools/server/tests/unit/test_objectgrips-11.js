/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we get the magic properties on Error objects.

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_object_grip();
    },
    { waitForFinish: true }
  )
);

function test_object_grip() {
  gThreadFront.once("paused", async function(packet) {
    const args = packet.frame.arguments;

    const objClient = gThreadFront.pauseGrip(args[0]);
    const response = await objClient.getOwnPropertyNames();
    const opn = response.ownPropertyNames;
    Assert.equal(opn.length, 4);
    opn.sort();
    Assert.equal(opn[0], "columnNumber");
    Assert.equal(opn[1], "fileName");
    Assert.equal(opn[2], "lineNumber");
    Assert.equal(opn[3], "message");

    await gThreadFront.resume();
    threadFrontTestFinished();
  });

  gDebuggee.eval(
    function stopMe(arg1) {
      debugger;
    }.toString()
  );

  gDebuggee.eval("stopMe(new TypeError('error message text'))");
}
