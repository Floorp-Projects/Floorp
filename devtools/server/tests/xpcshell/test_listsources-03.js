/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check getSources functionality when there are lots of sources.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const response = await threadFront.getSources();

    Assert.ok(
      !response.error,
      "There shouldn't be an error fetching large amounts of sources."
    );

    Assert.ok(
      response.sources.some(function (s) {
        return s.url.match(/foo-999.js$/);
      })
    );

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  for (let i = 0; i < 1000; i++) {
    Cu.evalInSandbox(
      "function foo###() {return ###;}".replace(/###/g, i),
      debuggee,
      "1.8",
      "http://example.com/foo-" + i + ".js",
      1
    );
  }
  debuggee.eval("debugger;");
}
