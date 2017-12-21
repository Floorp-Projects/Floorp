/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that adding a breakpoint in the same place returns the same actor.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             testSameBreakpoint();
                           });
  });
  do_test_pending();
}

const SOURCE_URL = "http://example.com/source.js";

const testSameBreakpoint = Task.async(function* () {
  let packet = yield executeOnNextTickAndWaitForPause(evalCode, gClient);
  let source = gThreadClient.source(packet.frame.where.source);

  // Whole line
  let wholeLineLocation = {
    line: 2
  };

  let [, firstBpClient] = yield setBreakpoint(source, wholeLineLocation);
  let [, secondBpClient] = yield setBreakpoint(source, wholeLineLocation);

  Assert.equal(firstBpClient.actor, secondBpClient.actor,
               "Should get the same actor w/ whole line breakpoints");

  // Specific column

  let columnLocation = {
    line: 2,
    column: 6
  };

  [, firstBpClient] = yield setBreakpoint(source, columnLocation);
  [, secondBpClient] = yield setBreakpoint(source, columnLocation);

  Assert.equal(secondBpClient.actor, secondBpClient.actor,
               "Should get the same actor column breakpoints");

  finishClient(gClient);
});

function evalCode() {
  /* eslint-disable */
  Components.utils.evalInSandbox(
    "" + function doStuff(k) { // line 1
      let arg = 15;            // line 2 - Step in here
      k(arg);                  // line 3
    } + "\n"                   // line 4
    + "debugger;",             // line 5
    gDebuggee,
    "1.8",
    SOURCE_URL,
    1
  );
  /* eslint-enable */
}
