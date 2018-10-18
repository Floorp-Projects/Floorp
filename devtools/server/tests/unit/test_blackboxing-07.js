/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that sources whose URL ends with ".min.js" automatically get black
 * boxed.
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
                             testBlackBox();
                           });
  });
  do_test_pending();
}

const BLACK_BOXED_URL = "http://example.com/black-boxed.min.js";
const SOURCE_URL = "http://example.com/source.js";

const testBlackBox = async function() {
  await executeOnNextTickAndWaitForPause(evalCode, gClient);

  const { sources } = await getSources(gThreadClient);
  equal(sources.length, 2);

  const blackBoxedSource = sources.filter(s => s.url === BLACK_BOXED_URL)[0];
  equal(blackBoxedSource.isBlackBoxed, true);

  const regularSource = sources.filter(s => s.url === SOURCE_URL)[0];
  equal(regularSource.isBlackBoxed, false);

  finishClient(gClient);
};

function evalCode() {
  Cu.evalInSandbox(
    "" + function blackBoxed() {},
    gDebuggee,
    "1.8",
    BLACK_BOXED_URL,
    1
  );

  Cu.evalInSandbox(
    "" + function source() {}
      + "\ndebugger;",
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
}
