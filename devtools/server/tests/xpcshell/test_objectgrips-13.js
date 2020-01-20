/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that ObjectFront.prototype.getDefinitionSite and the "definitionSite"
// request work properly.

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );
    const [funcGrip, objGrip] = packet.frame.arguments;
    const func = threadFront.pauseGrip(funcGrip);
    const obj = threadFront.pauseGrip(objGrip);

    // test definition site
    const response = await func.getDefinitionSite();
    Assert.ok(!response.error);
    Assert.equal(response.source.url, getFilePath("test_objectgrips-13.js"));
    Assert.equal(response.line, debuggee.line0 + 1);
    Assert.equal(response.column, 0);

    // test bad definition site
    try {
      obj._client.request("definitionSite", () => Assert.ok(false));
    } catch (e) {
      threadFront.resume();
    }
  })
);

function evalCode(debuggee) {
  Cu.evalInSandbox(
    function stopMe() {
      debugger;
    }.toString(),
    debuggee
  );

  Cu.evalInSandbox(
    [
      "this.line0 = Error().lineNumber;",
      "function f() {}",
      "stopMe(f, {});",
    ].join("\n"),
    debuggee
  );
}
