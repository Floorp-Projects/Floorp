/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test basic black boxing.
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
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             testBlackBox();
                           });
  });
  do_test_pending();
}

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

const testBlackBox = async function() {
  const packet = await executeOnNextTickAndWaitForPause(evalCode, gClient);
  const source = gThreadClient.source(packet.frame.where.source);

  await setBreakpoint(source, {
    line: 2
  });
  await resume(gThreadClient);

  const { sources } = await getSources(gThreadClient);
  const sourceClient = gThreadClient.source(
    sources.filter(s => s.url == BLACK_BOXED_URL)[0]);
  Assert.ok(!sourceClient.isBlackBoxed,
            "By default the source is not black boxed.");

  // Test that we can step into `doStuff` when we are not black boxed.
  await runTest(
    function onSteppedLocation(location) {
      Assert.equal(location.source.url, BLACK_BOXED_URL);
      Assert.equal(location.line, 2);
    },
    function onDebuggerStatementFrames(frames) {
      Assert.ok(!frames.some(f => f.where.source.isBlackBoxed));
    }
  );

  await blackBox(sourceClient);
  Assert.ok(sourceClient.isBlackBoxed);

  // Test that we step through `doStuff` when we are black boxed and its frame
  // doesn't show up.
  await runTest(
    function onSteppedLocation(location) {
      Assert.equal(location.source.url, SOURCE_URL);
      Assert.equal(location.line, 4);
    },
    function onDebuggerStatementFrames(frames) {
      for (const f of frames) {
        if (f.where.source.url == BLACK_BOXED_URL) {
          Assert.ok(f.where.source.isBlackBoxed);
        } else {
          Assert.ok(!f.where.source.isBlackBoxed);
        }
      }
    }
  );

  await unBlackBox(sourceClient);
  Assert.ok(!sourceClient.isBlackBoxed);

  // Test that we can step into `doStuff` again.
  await runTest(
    function onSteppedLocation(location) {
      Assert.equal(location.source.url, BLACK_BOXED_URL);
      Assert.equal(location.line, 2);
    },
    function onDebuggerStatementFrames(frames) {
      Assert.ok(!frames.some(f => f.where.source.isBlackBoxed));
    }
  );

  finishClient(gClient);
};

function evalCode() {
  /* eslint-disable */
  Cu.evalInSandbox(
    "" + function doStuff(k) { // line 1
      var arg = 15;            // line 2 - Step in here
      k(arg);                  // line 3
    },                         // line 4
    gDebuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );

  Cu.evalInSandbox(
    "" + function runTest() { // line 1
      doStuff(                // line 2 - Break here
        function (n) {        // line 3 - Step through `doStuff` to here
          debugger;           // line 4
        }                     // line 5
      );                      // line 6
    } + "\n"                  // line 7
    + "debugger;",            // line 8
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable */
}

const runTest = async function(onSteppedLocation, onDebuggerStatementFrames) {
  let packet = await executeOnNextTickAndWaitForPause(gDebuggee.runTest,
                                                      gClient);
  Assert.equal(packet.why.type, "breakpoint");

  await stepIn(gClient, gThreadClient);

  const location = await getCurrentLocation();
  onSteppedLocation(location);

  packet = await resumeAndWaitForPause(gClient, gThreadClient);
  Assert.equal(packet.why.type, "debuggerStatement");

  const { frames } = await getFrames(gThreadClient, 0, 100);
  onDebuggerStatementFrames(frames);

  return resume(gThreadClient);
};

const getCurrentLocation = async function() {
  const response = await getFrames(gThreadClient, 0, 1);
  return response.frames[0].where;
};
