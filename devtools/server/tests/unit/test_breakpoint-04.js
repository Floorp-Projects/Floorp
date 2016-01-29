/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that setting a breakpoint in a line in a child script works.
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
      test_child_breakpoint();
    });
  });
}

function test_child_breakpoint()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let source = gThreadClient.source(aPacket.frame.where.source);
    let location = { line: gDebuggee.line0 + 3 };

    source.setBreakpoint(location, function (aResponse, bpClient) {
      // actualLocation is not returned when breakpoints don't skip forward.
      do_check_eq(aResponse.actualLocation, undefined);

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
    "function foo() {\n" + // line0 + 1
    "  this.a = 1;\n" +    // line0 + 2
    "  this.b = 2;\n" +    // line0 + 3
    "}\n" +                // line0 + 4
    "debugger;\n" +        // line0 + 5
    "foo();\n",            // line0 + 6
    gDebuggee
  );
}
