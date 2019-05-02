/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-disable no-shadow */

/**
 * Test that we don't hit breakpoints in black boxed sources, and that when we
 * unblack box the source again, the breakpoint hasn't disappeared and we will
 * hit it again.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-black-box");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-black-box",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_black_box();
                           });
  });
  do_test_pending();
}

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

function test_black_box() {
  gClient.addOneTimeListener("paused", async function(event, packet) {
    gThreadClient.setBreakpoint({ sourceUrl: BLACK_BOXED_URL, line: 2 }, {});
    gThreadClient.resume().then(test_black_box_breakpoint);
  });

  /* eslint-disable no-multi-spaces, no-undef */
  Cu.evalInSandbox(
    "" + function doStuff(k) { // line 1
      const arg = 15;            // line 2 - Break here
      k(arg);                  // line 3
    },                         // line 4
    gDebuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );

  Cu.evalInSandbox(
    "" + function runTest() { // line 1
      doStuff(                // line 2
        function(n) {        // line 3
          debugger;           // line 5
        }                     // line 6
      );                      // line 7
    }                         // line 8
    + "\n debugger;",         // line 9
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable no-multi-spaces, no-undef */
}

function test_black_box_breakpoint() {
  gThreadClient.getSources().then(async function({error, sources}) {
    Assert.ok(!error, "Should not get an error: " + error);
    const sourceFront = gThreadClient.source(
      sources.filter(s => s.url == BLACK_BOXED_URL)[0]
    );

    await blackBox(sourceFront);

    gClient.addOneTimeListener("paused", function(event, packet) {
      Assert.equal(
        packet.why.type, "debuggerStatement",
        "We should pass over the breakpoint since the source is black boxed.");
      gThreadClient.resume().then(test_unblack_box_breakpoint.bind(null, sourceFront));
    });
    gDebuggee.runTest();
  });
}

async function test_unblack_box_breakpoint(sourceFront) {
  await unBlackBox(sourceFront);
  gClient.addOneTimeListener("paused", function(event, packet) {
    Assert.equal(packet.why.type, "breakpoint",
                 "We should hit the breakpoint again");

    // We will hit the debugger statement on resume, so do this
    // nastiness to skip over it.
    gClient.addOneTimeListener(
      "paused",
      async () => {
        await gThreadClient.resume();
        finishClient(gClient);
      }
    );
    gThreadClient.resume();
  });
  gDebuggee.runTest();
}
