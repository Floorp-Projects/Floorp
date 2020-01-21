/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gThreadFront;

// This test ensures that we can create SourceActors and SourceFronts properly,
// and that they can communicate over the protocol to fetch the source text for
// a given script.

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_source();
    },
    { waitForFinish: true }
  )
);

const SOURCE_URL = "http://example.com/foobar.js";
const SOURCE_CONTENT = `
  stopMe();
  for(var i = 0; i < 2; i++) {
    debugger;
  }
`;

function test_source() {
  DebuggerServer.LONG_STRING_LENGTH = 200;

  gThreadFront.once("paused", function(packet) {
    gThreadFront.getSources().then(async function(response) {
      Assert.ok(!!response);
      Assert.ok(!!response.sources);

      const source = response.sources.filter(function(s) {
        return s.url === SOURCE_URL;
      })[0];

      Assert.ok(!!source);

      const sourceFront = gThreadFront.source(source);
      response = await sourceFront.getBreakpointPositions();
      Assert.ok(!!response);
      Assert.deepEqual(response, [
        {
          line: 2,
          column: 2,
        },
        {
          line: 3,
          column: 14,
        },
        {
          line: 3,
          column: 17,
        },
        {
          line: 3,
          column: 24,
        },
        {
          line: 4,
          column: 4,
        },
        {
          line: 6,
          column: 0,
        },
      ]);

      response = await sourceFront.getBreakpointPositionsCompressed();
      Assert.ok(!!response);

      Assert.deepEqual(response, {
        2: [2],
        3: [14, 17, 24],
        4: [4],
        6: [0],
      });

      await gThreadFront.resume();
      threadFrontTestFinished();
    });
  });

  Cu.evalInSandbox(
    "" +
      function stopMe(arg1) {
        debugger;
      },
    gDebuggee,
    "1.8",
    getFileUrl("test_source-02.js")
  );

  Cu.evalInSandbox(SOURCE_CONTENT, gDebuggee, "1.8", SOURCE_URL);
}
