/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check basic getSources functionality.
 */

var gNumTimesSourcesSent = 0;

add_task(
  threadFrontTest(async ({ threadFront, debuggee, client }) => {
    client.request = (function (origRequest) {
      return function (request, onResponse) {
        if (request.type === "sources") {
          ++gNumTimesSourcesSent;
        }
        return origRequest.call(this, request, onResponse);
      };
    })(client.request);

    await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const response = await threadFront.getSources();

    Assert.ok(
      response.sources.some(function (s) {
        return s.url && s.url.match(/test_listsources-01.js/);
      })
    );

    Assert.ok(
      gNumTimesSourcesSent <= 1,
      "Should only send one sources request at most, even though we" +
        " might have had to send one to determine feature support."
    );

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  /* eslint-disable */
  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n" +
      "debugger;\n" + // line0 + 1
      "var a = 1;\n" + // line0 + 2
      "var b = 2;\n", // line0 + 3
    debuggee
  );
  /* eslint-enable */
}
