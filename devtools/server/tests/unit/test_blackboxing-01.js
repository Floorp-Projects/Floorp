/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test basic black boxing.
 */

var gDebuggee;
var gClient;
var gThreadFront;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-black-box");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-black-box", function(
      response,
      targetFront,
      threadFront
    ) {
      gThreadFront = threadFront;
      testBlackBox();
    });
  });
  do_test_pending();
}

const BLACK_BOXED_URL = "http://example.com/blackboxme.js";
const SOURCE_URL = "http://example.com/source.js";

const testBlackBox = async function() {
  const packet = await executeOnNextTickAndWaitForPause(evalCode, gThreadFront);

  const bpSource = await getSourceById(gThreadFront, packet.frame.where.actor);

  await setBreakpoint(gThreadFront, { sourceUrl: bpSource.url, line: 2 });
  await resume(gThreadFront);

  let sourceForm = await getSourceForm(gThreadFront, BLACK_BOXED_URL);

  Assert.ok(
    !sourceForm.isBlackBoxed,
    "By default the source is not black boxed."
  );

  // Test that we can step into `doStuff` when we are not black boxed.
  await runTest(
    async function onSteppedLocation(location) {
      const source = await getSourceFormById(gThreadFront, location.actor);
      Assert.equal(source.url, BLACK_BOXED_URL);
      Assert.equal(location.line, 2);
    },
    async function onDebuggerStatementFrames(frames) {
      for (const frame of frames) {
        const source = await getSourceFormById(gThreadFront, frame.where.actor);
        Assert.ok(!source.isBlackBoxed);
      }
    }
  );

  const blackboxedSource = await getSource(gThreadFront, BLACK_BOXED_URL);
  await blackBox(blackboxedSource);
  sourceForm = await getSourceForm(gThreadFront, BLACK_BOXED_URL);
  Assert.ok(sourceForm.isBlackBoxed);

  // Test that we step through `doStuff` when we are black boxed and its frame
  // doesn't show up.
  await runTest(
    async function onSteppedLocation(location) {
      const source = await getSourceFormById(gThreadFront, location.actor);
      Assert.equal(source.url, SOURCE_URL);
      Assert.equal(location.line, 4);
    },
    async function onDebuggerStatementFrames(frames) {
      for (const frame of frames) {
        const source = await getSourceFormById(gThreadFront, frame.where.actor);
        if (source.url == BLACK_BOXED_URL) {
          Assert.ok(source.isBlackBoxed);
        } else {
          Assert.ok(!source.isBlackBoxed);
        }
      }
    }
  );

  await unBlackBox(blackboxedSource);
  sourceForm = await getSourceForm(gThreadFront, BLACK_BOXED_URL);
  Assert.ok(!sourceForm.isBlackBoxed);

  // Test that we can step into `doStuff` again.
  await runTest(
    async function onSteppedLocation(location) {
      const source = await getSourceFormById(gThreadFront, location.actor);
      Assert.equal(source.url, BLACK_BOXED_URL);
      Assert.equal(location.line, 2);
    },
    async function onDebuggerStatementFrames(frames) {
      for (const frame of frames) {
        const source = await getSourceFormById(gThreadFront, frame.where.actor);
        Assert.ok(!source.isBlackBoxed);
      }
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
          (() => {})();       // line 4
          debugger;           // line 5
        }                     // line 6
      );                      // line 7
    } + "\n"                  // line 8
    + "debugger;",            // line 9
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable */
}

const runTest = async function(onSteppedLocation, onDebuggerStatementFrames) {
  let packet = await executeOnNextTickAndWaitForPause(
    gDebuggee.runTest,
    gThreadFront
  );
  Assert.equal(packet.why.type, "breakpoint");

  await stepIn(gThreadFront);

  const location = await getCurrentLocation();
  await onSteppedLocation(location);

  packet = await resumeAndWaitForPause(gThreadFront);
  Assert.equal(packet.why.type, "debuggerStatement");

  const { frames } = await getFrames(gThreadFront, 0, 100);
  await onDebuggerStatementFrames(frames);

  return resume(gThreadFront);
};

const getCurrentLocation = async function() {
  const response = await getFrames(gThreadFront, 0, 1);
  return response.frames[0].where;
};
