/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check basic breakpoint functionality.
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
}

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-stack", aServer);
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_simple_breakpoint();
    });
  });
}

function test_simple_breakpoint()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let source = gThreadClient.source(aPacket.frame.where.source);
    let location = {
      line: gDebuggee.line0 + 3
    };

    source.setBreakpoint(location, function (aResponse, bpClient) {
      gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
        // Check the return value.
        do_check_eq(aPacket.type, "paused");
        do_check_eq(aPacket.frame.where.source.actor, source.actor);
        do_check_eq(aPacket.frame.where.line, location.line);
        do_check_eq(aPacket.why.type, "breakpoint");
        do_check_eq(aPacket.why.actors[0], bpClient.actor);
        // Check that the breakpoint worked.
        do_check_eq(gDebuggee.a, 1);
        do_check_eq(gDebuggee.b, undefined);

        // Remove the breakpoint.
        bpClient.remove(function (aResponse) {
          gThreadClient.resume(function () {
            gClient.close(gCallback);
          });
        });

      });

      // Continue until the breakpoint is hit.
      gThreadClient.resume();
    });
  });

  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n" +
    "debugger;\n" +   // line0 + 1
    "var a = 1;\n" +  // line0 + 2
    "var b = 2;\n",   // line0 + 3
    gDebuggee
  );
}
