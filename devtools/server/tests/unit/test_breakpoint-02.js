/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that setting breakpoints when the debuggee is running works.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test()
{
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
};

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-stack", aServer);
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_breakpoint_running();
    });
  });
}

function test_breakpoint_running()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let location = { line: gDebuggee.line0 + 3 };

    gThreadClient.resume();

    // Setting the breakpoint later should interrupt the debuggee.
    gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
      do_check_eq(aPacket.type, "paused");
      do_check_eq(aPacket.why.type, "interrupted");
    });

    let source = gThreadClient.source(aPacket.frame.where.source);
    source.setBreakpoint(location, function(aResponse) {
      // Eval scripts don't stick around long enough for the breakpoint to be set,
      // so just make sure we got the expected response from the actor.
      do_check_neq(aResponse.error, "noScript");

      do_execute_soon(function() {
        gClient.close(gCallback);
      });
    });
  });

  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n" +
    "debugger;\n" +
    "var a = 1;\n" +  // line0 + 2
    "var b = 2;\n",  // line0 + 3
    gDebuggee
  );
}
