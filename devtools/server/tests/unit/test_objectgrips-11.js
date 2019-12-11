/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we get the magic properties on Error objects.

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );
    const args = packet.frame.arguments;

    const objClient = threadFront.pauseGrip(args[0]);
    const response = await objClient.getOwnPropertyNames();
    const opn = response.ownPropertyNames;
    Assert.equal(opn.length, 4);
    opn.sort();
    Assert.equal(opn[0], "columnNumber");
    Assert.equal(opn[1], "fileName");
    Assert.equal(opn[2], "lineNumber");
    Assert.equal(opn[3], "message");

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    function stopMe(arg1) {
      debugger;
    }.toString()
  );

  debuggee.eval("stopMe(new TypeError('error message text'))");
}
