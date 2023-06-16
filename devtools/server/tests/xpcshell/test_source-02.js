/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test ensures that we can create SourceActors and SourceFronts properly,
// and that they can communicate over the protocol to fetch the source text for
// a given script.

const SOURCE_URL = "http://example.com/foobar.js";
const SOURCE_CONTENT = `
  stopMe();
  for(var i = 0; i < 2; i++) {
    debugger;
  }
`;

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    DevToolsServer.LONG_STRING_LENGTH = 200;

    await executeOnNextTickAndWaitForPause(
      () => evaluateTestCode(debuggee),
      threadFront
    );

    let response = await threadFront.getSources();
    Assert.ok(!!response);
    Assert.ok(!!response.sources);

    const source = response.sources.filter(function (s) {
      return s.url === SOURCE_URL;
    })[0];

    Assert.ok(!!source);

    const sourceFront = threadFront.source(source);
    response = await sourceFront.getBreakpointPositionsCompressed();
    Assert.ok(!!response);

    Assert.deepEqual(response, {
      2: [2],
      3: [14, 17, 24],
      4: [4],
      6: [0],
    });

    await threadFront.resume();
  })
);

function evaluateTestCode(debuggee) {
  Cu.evalInSandbox(
    "" +
      function stopMe(arg1) {
        debugger;
      },
    debuggee,
    "1.8",
    getFileUrl("test_source-02.js")
  );

  Cu.evalInSandbox(SOURCE_CONTENT, debuggee, "1.8", SOURCE_URL);
}
