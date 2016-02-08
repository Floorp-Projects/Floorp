/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that adding a breakpoint in the same place returns the same actor.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
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

  let [firstResponse, firstBpClient] = yield setBreakpoint(source, wholeLineLocation);
  let [secondResponse, secondBpClient] = yield setBreakpoint(source, wholeLineLocation);

  do_check_eq(firstBpClient.actor, secondBpClient.actor, "Should get the same actor w/ whole line breakpoints");

  // Specific column

  let columnLocation = {
    line: 2,
    column: 6
  };

  [firstResponse, firstBpClient] = yield setBreakpoint(source, columnLocation);
  [secondResponse, secondBpClient] = yield setBreakpoint(source, columnLocation);

  do_check_eq(secondBpClient.actor, secondBpClient.actor, "Should get the same actor column breakpoints");

  finishClient(gClient);
});

function evalCode() {
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
}
