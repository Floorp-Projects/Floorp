/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check getSources functionality when there are lots of sources.
 */

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_simple_listsources();
    },
    { waitForFinish: true }
  )
);

function test_simple_listsources() {
  gThreadFront.once("paused", function(packet) {
    gThreadFront.getSources().then(function(response) {
      Assert.ok(
        !response.error,
        "There shouldn't be an error fetching large amounts of sources."
      );

      Assert.ok(
        response.sources.some(function(s) {
          return s.url.match(/foo-999.js$/);
        })
      );

      gThreadFront.resume().then(function() {
        threadFrontTestFinished();
      });
    });
  });

  for (let i = 0; i < 1000; i++) {
    Cu.evalInSandbox(
      "function foo###() {return ###;}".replace(/###/g, i),
      gDebuggee,
      "1.8",
      "http://example.com/foo-" + i + ".js",
      1
    );
  }
  gDebuggee.eval("debugger;");
}
