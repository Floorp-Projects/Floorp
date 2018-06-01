/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify the "frames" request on the thread.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_pause_frame();
                           });
  });
  do_test_pending();
}

var gFrames = [
  // Function calls...
  { type: "call", callee: { name: "depth3" } },
  { type: "call", callee: { name: "depth2" } },
  { type: "call", callee: { name: "depth1" } },

  // Anonymous function call in our eval...
  { type: "call", callee: { name: undefined } },

  // The eval itself.
  { type: "eval", callee: { name: undefined } },
];

var gSliceTests = [
  { start: 0, count: undefined, resetActors: true },
  { start: 0, count: 1 },
  { start: 2, count: 2 },
  { start: 1, count: 15 },
  { start: 15, count: undefined },
];

function test_frame_slice() {
  if (gSliceTests.length == 0) {
    gThreadClient.resume(() => finishClient(gClient));
    return;
  }

  const test = gSliceTests.shift();
  gThreadClient.getFrames(test.start, test.count, function(response) {
    const testFrames = gFrames.slice(test.start,
                                   test.count ? test.start + test.count : undefined);
    Assert.equal(testFrames.length, response.frames.length);
    for (let i = 0; i < testFrames.length; i++) {
      const expected = testFrames[i];
      const actual = response.frames[i];

      if (test.resetActors) {
        expected.actor = actual.actor;
      }

      for (const key of ["type", "callee-name"]) {
        Assert.equal(expected[key] || undefined, actual[key]);
      }
    }
    test_frame_slice();
  });
}

function test_pause_frame() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    test_frame_slice();
  });

  gDebuggee.eval("(" + function() {
    function depth3() {
      debugger;
    }
    function depth2() {
      depth3();
    }
    function depth1() {
      depth2();
    }
    depth1();
  } + ")()");
}
