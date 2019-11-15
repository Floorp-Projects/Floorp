/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check basic newSource packet sent from server.
 */

var gDebuggee;
var gThreadFront;

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      test_simple_new_source();
    },
    { waitForFinish: true }
  )
);

function test_simple_new_source() {
  gThreadFront.once("newSource", function(packet) {
    Assert.ok(!!packet.source);
    Assert.ok(!!packet.source.url.match(/test_new_source-01.js$/));

    threadFrontTestFinished();
  });

  Cu.evalInSandbox(
    function inc(n) {
      return n + 1;
    }.toString(),
    gDebuggee
  );
}
